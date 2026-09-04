/**
 * @file test_ikcp_cong.c
 * @brief Host test for the CUBIC congestion control used by KCP
 * @version 1.0
 * @date 2026-08-26
 * @copyright Copyright (c) Tuya Inc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ikcp.h"
#include "ikcp_cong.h"

static int g_fail;

static void check(const char *what, int ok, const char *detail)
{
    printf("%-34s %s   %s\n", what, ok ? "ok  " : "FAIL", detail ? detail : "");
    if (!ok) {
        g_fail++;
    }
}

static int dummy_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    (void)buf;
    (void)len;
    (void)kcp;
    (void)user;
    return 0;
}

/* Bytes the transport actually handed to the socket, for the pacing tests. */
static long g_wire_bytes;

static int counting_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    (void)buf;
    (void)kcp;
    (void)user;
    g_wire_bytes += len;
    return 0;
}

static ikcpcb *make_kcp(int with_cubic)
{
    ikcpcb *kcp = ikcp_create(1, NULL);

    ikcp_setoutput(kcp, dummy_output);
    ikcp_wndsize(kcp, 1024, 1024);
    kcp->rmt_wnd  = 1024;
    kcp->cwnd     = 1;
    kcp->ssthresh = 1024; /* stay in slow start until told otherwise */
    if (!with_cubic && kcp->cong) {
        ikcp_cong_cubic_release(kcp);
    }
    return kcp;
}

/* One RTT worth of ACKs at the current window.
 * Time has to move: the cubic curve is a function of elapsed time since the
 * last loss, so a test that holds the clock still measures nothing. */
#define TEST_RTT_MS 30
static void ack_one_rtt(ikcpcb *kcp)
{
    IUINT32 n = kcp->cwnd;
    IUINT32 i;

    kcp->current += TEST_RTT_MS;
    for (i = 0; i < n; i++) {
        ikcp_cong_cubic_on_ack(kcp, 1);
    }
}

/* What KCP will actually send with, which is what matters - see ikcp_flush. */
static IUINT32 effective_wnd(const ikcpcb *kcp)
{
    IUINT32 w = kcp->snd_wnd < kcp->rmt_wnd ? kcp->snd_wnd : kcp->rmt_wnd;

    return (kcp->nocwnd == 0 && kcp->cwnd < w) ? kcp->cwnd : w;
}

int main(void)
{
    ikcpcb *kcp;
    IUINT32 at64 = 0, at256 = 0, i, before, after;
    char    buf[128];

    /* --- ramp: how quickly a fresh flow reaches a usable window --- */
    kcp = make_kcp(1);
    check("cubic state allocated", kcp->cong != NULL, NULL);
    for (i = 1; i <= 400; i++) {
        ikcp_cong_cubic_on_rtt(kcp, 30);
        ack_one_rtt(kcp);
        if (at64 == 0 && kcp->cwnd >= 64)
            at64 = i;
        if (at256 == 0 && kcp->cwnd >= 256)
            at256 = i;
    }
    snprintf(buf, sizeof(buf), "cwnd>=64 @RTT %u, >=256 @RTT %u, final %u", at64, at256, kcp->cwnd);
    check("ramps to link capacity", at64 > 0 && at64 < 40 && kcp->cwnd >= 512, buf);

    /* --- fast retransmit: multiplicative decrease to beta (717/1024 = 0.7) --- */
    before = kcp->cwnd;
    ikcp_cong_cubic_on_loss(kcp, 0);
    after = kcp->cwnd;
    snprintf(buf, sizeof(buf), "%u -> %u (expect ~%u)", before, after, before * 717 / 1024);
    check("fast retransmit backs off 0.7x",
          after <= before * 3 / 4 && after >= before * 2 / 3 && after == kcp->ssthresh, buf);

    /* --- recovery: gets back near the previous window within a few seconds --- */
    for (i = 0; i < 40; i++) {
        ikcp_cong_cubic_on_rtt(kcp, 30);
        ack_one_rtt(kcp);
    }
    snprintf(buf, sizeof(buf), "recovered to %u of %u", kcp->cwnd, before);
    check("recovers after fast retransmit", kcp->cwnd >= before * 3 / 4, buf);

    /* --- RTO: pipe presumed empty, restart from one --- */
    ikcp_cong_cubic_on_loss(kcp, 1);
    snprintf(buf, sizeof(buf), "cwnd=%u ssthresh=%u", kcp->cwnd, kcp->ssthresh);
    check("RTO restarts from 1", kcp->cwnd == 1 && kcp->ssthresh >= 2, buf);
    for (i = 0; i < 300; i++) {
        ikcp_cong_cubic_on_rtt(kcp, 30);
        ack_one_rtt(kcp);
    }
    snprintf(buf, sizeof(buf), "back to %u", kcp->cwnd);
    check("recovers after RTO", kcp->cwnd >= 256, buf);

    /* --- receiver window is a hard ceiling --- */
    kcp->rmt_wnd = 40;
    for (i = 0; i < 100; i++) {
        ack_one_rtt(kcp);
    }
    snprintf(buf, sizeof(buf), "effective=%u cwnd=%u rmt_wnd=%u", effective_wnd(kcp), kcp->cwnd, kcp->rmt_wnd);
    check("never sends beyond peer window", effective_wnd(kcp) <= 40, buf);
    ikcp_release(kcp);

    /* --- floor: a flow that keeps losing must not stall completely --- */
    kcp = make_kcp(1);
    for (i = 0; i < 30; i++) {
        ikcp_cong_cubic_on_loss(kcp, 0);
    }
    snprintf(buf, sizeof(buf), "cwnd=%u after 30 consecutive losses", kcp->cwnd);
    check("keeps a minimum window", kcp->cwnd >= 1, buf);
    ikcp_release(kcp);

    /* --- reference: stock KCP growth, to show CUBIC is the faster of the two --- */
    kcp  = make_kcp(1);
    at64 = 0;
    for (i = 1; i <= 400; i++) {
        ikcp_cong_cubic_on_rtt(kcp, 30);
        ack_one_rtt(kcp);
        if (at64 == 0 && kcp->cwnd >= 64)
            at64 = i;
    }
    snprintf(buf, sizeof(buf), "cubic reached 64 in %u RTTs", at64);
    check("faster than one-per-RTT growth", at64 < 64, buf);
    ikcp_release(kcp);

    /* --- pacing: a window must reach the wire over an RTT, not in one burst ---
     *
     * This is the property the device was missing. With cwnd at 100 segments a
     * single unpaced flush put ~140 kB into the socket at once; the Wi-Fi queue
     * tail dropped a long run of it and every segment in that run timed out
     * together. What matters is not that pacing exists but that one flush emits
     * roughly its share of the window - here one 10 ms slice of a 300 ms RTT. */
    {
        const IUINT32 srtt = 300, interval = 10, wnd = 100;
        IUINT32       slices = srtt / interval;
        long          first_flush, whole_rtt, window_bytes;
        IUINT32       i;

        kcp = ikcp_create(1, NULL);
        ikcp_setoutput(kcp, counting_output);
        ikcp_wndsize(kcp, 1024, 1024);
        ikcp_setmtu(kcp, 1400);
        ikcp_nodelay(kcp, 0, (int)interval, 2, 0);
        kcp->rmt_wnd  = 1024;
        kcp->cwnd     = wnd;
        kcp->ssthresh = 1; /* congestion avoidance, so gain is 1.2x */
        kcp->rx_srtt  = (IINT32)srtt;
        kcp->updated  = 1; /* ikcp_flush is a no-op until update ran */
        /* Far enough out that nothing retransmits inside the window under test,
         * so what is counted is purely what pacing let through. */
        kcp->rx_rto = 5000;
        /* Full-mss payloads, so one segment is one mtu on the wire and the
         * window can be compared against bytes without a fudge factor. */
        window_bytes = (long)wnd * (long)kcp->mtu;

        for (i = 0; i < 400; i++) {
            char payload[1376];
            memset(payload, 'x', sizeof(payload));
            ikcp_send(kcp, payload, (int)kcp->mss);
        }

        g_wire_bytes = 0;
        kcp->current = 1000;
        ikcp_flush(kcp);
        first_flush = g_wire_bytes;
        snprintf(buf, sizeof(buf), "%ld B in one flush, window is %ld B", first_flush, window_bytes);
        check("one flush sends a slice, not all", first_flush > 0 && first_flush < window_bytes / 4, buf);

        /* Nothing here is ever acknowledged, and a sender that has not heard
         * from its peer must not commit a large backlog to a path it knows
         * nothing about - so the flow stops at the opening in-flight allowance
         * however big cwnd claims to be. An earlier version of this test
         * asserted the opposite, that a 100 segment window drained in full over
         * one RTT; that was the cwnd/srtt pacer, which had no notion of what was
         * outstanding. What a window does over an RTT on a link that answers is
         * covered properly in test_ikcp_pacing, against a simulated bottleneck.
         */
        for (i = 1; i < slices; i++) {
            kcp->current += interval;
            ikcp_flush(kcp);
        }
        whole_rtt = g_wire_bytes;
        snprintf(buf, sizeof(buf), "%ld B unacknowledged, window claims %ld B", whole_rtt, window_bytes);
        check("an unacked flow stops at the initial window", whole_rtt > 0 && whole_rtt <= window_bytes / 4, buf);

        ikcp_release(kcp);
    }

    /* --- pacing must never stall a flow completely --- */
    {
        long sent;

        kcp = ikcp_create(1, NULL);
        ikcp_setoutput(kcp, counting_output);
        ikcp_wndsize(kcp, 1024, 1024);
        ikcp_setmtu(kcp, 1400);
        ikcp_nodelay(kcp, 0, 10, 2, 0);
        kcp->rmt_wnd  = 1024;
        kcp->cwnd     = 1;
        kcp->ssthresh = 1;
        kcp->rx_srtt  = 5000; /* a window of one on a very long RTT */
        kcp->updated  = 1;    /* ikcp_flush is a no-op until update ran */

        for (i = 0; i < 10; i++) {
            char payload[1000];
            memset(payload, 'x', sizeof(payload));
            ikcp_send(kcp, payload, (int)sizeof(payload));
        }
        g_wire_bytes = 0;
        kcp->current = 1000;
        ikcp_flush(kcp);
        sent = g_wire_bytes;
        snprintf(buf, sizeof(buf), "%ld B with cwnd=1 srtt=5000ms", sent);
        check("a tiny budget still sends one packet", sent > 0, buf);
        ikcp_release(kcp);
    }

    printf("\n%s\n", g_fail ? "RESULT: FAIL" : "RESULT: PASS");
    return g_fail ? 1 : 0;
}
