/**
 * @file test_ikcp_pacing.c
 * @brief Host test for KCP send pacing against a simulated bottleneck
 * @version 1.0
 * @date 2026-08-26
 * @copyright Copyright (c) Tuya Inc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ikcp.h"
#include "ikcp_pacing.h"

static int g_fail;

static void check(const char *what, int ok, const char *detail)
{
    printf("%-36s %s   %s\n", what, ok ? "ok  " : "FAIL", detail ? detail : "");
    if (!ok) {
        g_fail++;
    }
}

/* ---------------------------------------------------------------------------
 * Bottleneck link
 * --------------------------------------------------------------------------- */
#define LINK_CAPACITY_PKTS 256

struct link_pkt {
    IUINT32 arrive; /* when the far end may read it */
    int     len;
    char    buf[2048];
};

struct link {
    struct link_pkt q[LINK_CAPACITY_PKTS];
    int             head, tail; /* ring; tail is one past the last */
    IUINT32         free_at;    /* when the wire finishes the packet before */
    IUINT32         bps;        /* capacity in bytes per second */
    IUINT32         delay;      /* one-way propagation, ms */
    int             depth_max;  /* deepest the queue ever got */
    int             drops;
    ikcpcb         *peer; /* who receives out of this link */
};

static int link_depth(const struct link *l)
{
    return (l->tail - l->head + LINK_CAPACITY_PKTS) % LINK_CAPACITY_PKTS;
}

/*
 * Queue one packet. Departure is serialised: a packet cannot start before the
 * one ahead of it has finished, which is what makes an over-fast sender build a
 * standing queue rather than simply going faster.
 */
static void link_send(struct link *l, const char *buf, int len, IUINT32 now)
{
    IUINT32 depart, serialise;
    int     next = (l->tail + 1) % LINK_CAPACITY_PKTS;

    if (next == l->head) { /* queue full - tail drop, as a real one does */
        l->drops++;
        return;
    }
    serialise = (IUINT32)(((IUINT64)len * 1000u) / l->bps);
    if (serialise == 0) {
        serialise = 1;
    }
    depart = ((IINT32)(l->free_at - now) > 0) ? l->free_at : now;
    depart += serialise;
    l->free_at = depart;

    l->q[l->tail].arrive = depart + l->delay;
    l->q[l->tail].len    = len;
    memcpy(l->q[l->tail].buf, buf, (size_t)len);
    l->tail = next;

    if (link_depth(l) > l->depth_max) {
        l->depth_max = link_depth(l);
    }
}

static void link_deliver(struct link *l, IUINT32 now)
{
    while (l->head != l->tail && (IINT32)(now - l->q[l->head].arrive) >= 0) {
        ikcp_input(l->peer, l->q[l->head].buf, l->q[l->head].len);
        l->head = (l->head + 1) % LINK_CAPACITY_PKTS;
    }
}

/* ---------------------------------------------------------------------------
 * Harness
 * --------------------------------------------------------------------------- */
static struct link g_fwd; /* sender -> receiver, the bottleneck */
static struct link g_rev; /* receiver -> sender, acks only */
static IUINT32     g_now;
static long        g_sent_wire; /* everything the sender put on the wire */

static int out_fwd(const char *buf, int len, ikcpcb *kcp, void *user)
{
    (void)kcp;
    (void)user;
    g_sent_wire += len;
    link_send(&g_fwd, buf, len, g_now);
    return 0;
}

static int out_rev(const char *buf, int len, ikcpcb *kcp, void *user)
{
    (void)kcp;
    (void)user;
    link_send(&g_rev, buf, len, g_now);
    return 0;
}

struct run_result {
    long    goodput_bps; /* bytes/sec the receiver actually got */
    int     queue_max;   /* deepest the bottleneck queue got */
    int     drops;
    IUINT32 srtt;
    IUINT32 min_rtt;
};

/*
 * Push a backlogged stream across the link for `ms` milliseconds.
 *
 * The sender is always kept with data to send, so what comes out the far end is
 * decided by congestion control and pacing rather than by the application.
 */
static struct run_result run_flow(IUINT32 link_bps, IUINT32 delay_ms, IUINT32 ms)
{
    ikcpcb           *snd, *rcv;
    struct run_result r;
    char              payload[1024];
    char              sink[4096];
    long              received = 0;
    IUINT32           t;

    memset(&g_fwd, 0, sizeof(g_fwd));
    memset(&g_rev, 0, sizeof(g_rev));
    memset(&r, 0, sizeof(r));
    memset(payload, 'x', sizeof(payload));
    g_sent_wire = 0;
    g_now       = 0;

    snd = ikcp_create(1, NULL);
    rcv = ikcp_create(1, NULL);
    ikcp_setoutput(snd, out_fwd);
    ikcp_setoutput(rcv, out_rev);
    ikcp_wndsize(snd, 512, 512);
    ikcp_wndsize(rcv, 512, 512);
    ikcp_setmtu(snd, 1400);
    ikcp_setmtu(rcv, 1400);
    ikcp_nodelay(snd, 1, 10, 2, 1);
    ikcp_nodelay(rcv, 1, 10, 2, 1);

    g_fwd.bps   = link_bps;
    g_fwd.delay = delay_ms;
    g_fwd.peer  = rcv;
    /* The return path is not under test: make it quick and roomy so acks are
     * never the thing being measured. */
    g_rev.bps   = 1000000;
    g_rev.delay = delay_ms;
    g_rev.peer  = snd;

    for (t = 0; t < ms; t++) {
        g_now = t;

        /* Keep roughly a second of data queued, never more, so the test does
         * not spend its memory on a backlog nobody will drain. */
        while (ikcp_waitsnd(snd) < 256) {
            ikcp_send(snd, payload, (int)sizeof(payload));
        }

        ikcp_update(snd, t);
        ikcp_update(rcv, t);
        link_deliver(&g_fwd, t);
        link_deliver(&g_rev, t);

        for (;;) {
            int n = ikcp_recv(rcv, sink, (int)sizeof(sink));
            if (n <= 0) {
                break;
            }
            received += n;
        }
    }

    r.goodput_bps = (long)(((IUINT64)received * 1000u) / ms);
    r.queue_max   = g_fwd.depth_max;
    r.drops       = g_fwd.drops;
    r.srtt        = (IUINT32)snd->rx_srtt;
    r.min_rtt     = pacing_min_rtt(snd);

    ikcp_release(snd);
    ikcp_release(rcv);
    return r;
}

int main(void)
{
    char buf[192];

    /* --- the rate must be expressible below one packet per flush period ---
     *
     * This is the defect this version fixes. The previous pacer opened a budget
     * of one flush period's worth of its rate and then rounded anything smaller
     * up to a whole mtu, so with a 10 ms period it could never pace below about
     * 1.1 Mbit/s however slow the link was. Given a 200 kbit/s bottleneck it
     * kept pushing five times the capacity into the queue.
     *
     * 200 kbit/s is 25000 B/s; a flow that respects it should land near that
     * and should not need the queue to be deep to do so. */
    {
        struct run_result r      = run_flow(25000, 20, 12000);
        long              expect = 25000;

        snprintf(buf, sizeof(buf), "%ld B/s on a %ld B/s link, queue peak %d, drops %d", r.goodput_bps, expect,
                 r.queue_max, r.drops);
        check("paces below one packet per period", r.goodput_bps > (expect * 6) / 10 && r.goodput_bps <= expect, buf);

        /* The point of pacing at the delivery rate rather than cwnd/srtt: the
         * bottleneck queue should stay shallow instead of being discovered. */
        snprintf(buf, sizeof(buf), "peak depth %d packets of %d", r.queue_max, LINK_CAPACITY_PKTS);
        check("keeps the bottleneck queue shallow", r.queue_max < LINK_CAPACITY_PKTS / 2, buf);
    }

    /* --- a faster link must actually be used ---
     *
     * The standing objection to measuring delivery is that an estimator sitting
     * on a rate it is itself limiting only measures its own output and can never
     * climb. The gain cycle is the answer, so prove the flow reaches a link an
     * order of magnitude faster than the one above. */
    {
        struct run_result r      = run_flow(250000, 20, 12000);
        long              expect = 250000;

        snprintf(buf, sizeof(buf), "%ld B/s on a %ld B/s link (%ld pct)", r.goodput_bps, expect,
                 (r.goodput_bps * 100) / expect);
        check("finds a link 10x faster", r.goodput_bps > (expect * 5) / 10, buf);
    }

    /* --- a deep buffer must not be filled just because it is there ---
     *
     * The board's Wi-Fi buffers over a second of traffic: 3 ms idle, 230 ms
     * average and 1.2 s peak under a 1.1 Mbit/s load. Nothing is dropped there,
     * so a loss-driven sender has no signal at all and settles at the bottom of
     * a full queue.
     *
     * The standing queue is exactly the gap between the round trip under load
     * and the smallest one seen, so that ratio is what to assert - not a fixed
     * latency, which on a slow link is dominated by serialising one packet and
     * says nothing about queueing. Two BDPs may be outstanding by design, so
     * about twice the unloaded path is expected and three times is not. */
    {
        struct run_result r = run_flow(25000, 5, 12000);

        snprintf(buf, sizeof(buf), "srtt %u ms against a measured floor of %u ms, queue peak %d", r.srtt, r.min_rtt,
                 r.queue_max);
        check("does not inflate rtt into the queue", r.min_rtt > 0 && r.srtt < r.min_rtt * 3, buf);
    }

    printf("\n%s\n", g_fail ? "RESULT: FAIL" : "RESULT: PASS");
    return g_fail ? 1 : 0;
}
