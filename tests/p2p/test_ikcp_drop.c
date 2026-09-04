/**
 * @file test_ikcp_drop.c
 * @brief Host test for discarding queued data the peer has not been told about
 * @version 1.0
 * @date 2026-08-26
 * @copyright Copyright (c) Tuya Inc.
 */
#include <stdio.h>
#include <string.h>
#include "ikcp.h"

static int g_fail;

static void check(const char *what, int ok, const char *detail)
{
    printf("%-44s %s   %s\n", what, ok ? "ok  " : "FAIL", detail ? detail : "");
    if (!ok) {
        g_fail++;
    }
}

/* A link with no loss and no delay: what is under test is the sender's own
 * bookkeeping, so anything the receiver fails to produce is the sender's doing.
 * Acks can be held back, which is what pins a message across the cut - without
 * that, snd_una advances between flushes and the split is a race. */
static ikcpcb *g_snd, *g_rcv;
static int     g_acks_on = 1;
static long    g_received;

static int out_fwd(const char *buf, int len, ikcpcb *kcp, void *user)
{
    (void)kcp;
    (void)user;
    ikcp_input(g_rcv, buf, len);
    return 0;
}

static int out_rev(const char *buf, int len, ikcpcb *kcp, void *user)
{
    (void)kcp;
    (void)user;
    if (g_acks_on) {
        ikcp_input(g_snd, buf, len);
    }
    return 0;
}

static void pump(IUINT32 *clock, int rounds)
{
    char sink[65536];
    int  i;

    for (i = 0; i < rounds; i++) {
        *clock += 10;
        ikcp_update(g_snd, *clock);
        ikcp_update(g_rcv, *clock);
        for (;;) {
            int n = ikcp_recv(g_rcv, sink, (int)sizeof(sink));
            if (n <= 0) {
                break;
            }
            g_received += n;
        }
    }
}

static void link_up(IUINT32 conv, int snd_wnd)
{
    g_snd = ikcp_create(conv, NULL);
    g_rcv = ikcp_create(conv, NULL);
    ikcp_setoutput(g_snd, out_fwd);
    ikcp_setoutput(g_rcv, out_rev);
    ikcp_wndsize(g_snd, snd_wnd, 128);
    ikcp_wndsize(g_rcv, 128, 128);
    /* The settings the media channels actually run with. */
    ikcp_nodelay(g_snd, 0, 10, 2, 0);
    ikcp_nodelay(g_rcv, 0, 10, 2, 0);
    g_acks_on  = 1;
    g_received = 0;
}

static void link_down(void)
{
    ikcp_release(g_snd);
    ikcp_release(g_rcv);
}

int main(void)
{
    char buf[192];

    /* --- what is dropped is lost, what is not is still delivered --- *
     *
     * A segment gets its sequence number in ikcp_flush, so dropping what is
     * still queued must be invisible to the peer. Were it not, the receiver
     * would sit on a hole and stop producing entirely, so the conserved total
     * is the assertion worth making: everything sent, less what was dropped. */
    {
        char    payload[1024];
        IUINT32 clock = 0;
        long    sent  = 60 * 1024;
        int     dropped;
        int     i;

        memset(payload, 'x', sizeof(payload));
        link_up(1, 128);
        for (i = 0; i < 60; i++) {
            ikcp_send(g_snd, payload, (int)sizeof(payload));
        }
        pump(&clock, 3); /* long enough to get some of it moving, not all */

        dropped = ikcp_drop_unsent(g_snd);
        snprintf(buf, sizeof(buf), "%d bytes freed, %u segments left queued", dropped, g_snd->nsnd_que);
        check("drops what the peer has not been told of", dropped > 0 && g_snd->nsnd_que == 0, buf);

        pump(&clock, 400);
        snprintf(buf, sizeof(buf), "read %ld, expected %ld - %d = %ld", g_received, sent, dropped, sent - dropped);
        check("receiver gets exactly the rest", g_received == sent - (long)dropped, buf);

        snprintf(buf, sizeof(buf), "sender still holds %d segments", ikcp_waitsnd(g_snd));
        check("sender drains to empty", ikcp_waitsnd(g_snd) == 0, buf);
        link_down();
    }

    /* --- a message split across the cut is never orphaned --- *
     *
     * This is the hazard the whole thing turns on. ikcp_peeksize withholds a
     * message until it sees frg == 0, so dropping the tail of one whose head is
     * already in snd_buf stalls the receiver for good: it waits on a fragment
     * that will never be sent, and every later message queues behind it.
     *
     * Hold the acks so snd_una cannot advance, and a window of two segments
     * leaves a twelve-fragment message straddling the cut by construction. */
    {
        char    big[16384];
        IUINT32 clock = 0;
        int     dropped;

        memset(big, 'y', sizeof(big));
        link_up(2, 2);
        g_acks_on = 0;

        ikcp_send(g_snd, big, (int)sizeof(big));
        pump(&clock, 3);

        snprintf(buf, sizeof(buf), "%u fragments sent, %u still queued", g_snd->nsnd_buf, g_snd->nsnd_que);
        check("message straddles the cut", g_snd->nsnd_buf > 0 && g_snd->nsnd_que > 0, buf);

        ikcp_send(g_snd, big, (int)sizeof(big)); /* queued behind, and expendable */
        dropped = ikcp_drop_unsent(g_snd);

        snprintf(buf, sizeof(buf), "%d bytes freed, expected the whole second message (%d)", dropped, (int)sizeof(big));
        check("drops the message behind, not the one in flight", dropped == (int)sizeof(big), buf);

        g_acks_on = 1;
        pump(&clock, 400);

        snprintf(buf, sizeof(buf), "read %ld of the %d that survived", g_received, (int)sizeof(big));
        check("straddled message survives whole", g_received == (long)sizeof(big), buf);

        snprintf(buf, sizeof(buf), "sender still holds %d segments", ikcp_waitsnd(g_snd));
        check("no fragment left orphaned", ikcp_waitsnd(g_snd) == 0, buf);
        link_down();
    }

    printf("\n%s\n", g_fail ? "RESULT: FAIL" : "RESULT: PASS");
    return g_fail ? 1 : 0;
}
