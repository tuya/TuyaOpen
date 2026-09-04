/**
 * @file ikcp_pacing.c
 * @brief KCP send pacing at the measured delivery rate
 * @version 3.0
 * @date 2026-08-26
 * @copyright Copyright (c) Tuya Inc.
 */
#include "ikcp_pacing.h"
#include "ikcp_minmax.h"
#include "ikcp.h"
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
/* BBR ProbeBW gain cycle, 1/256 units; same constants as the vendor stack. */
#define PACING_GAIN_UNIT   256U
#define PACING_GAIN_PHASES 8U

/* Round trips the windowed max remembers a delivery rate sample for. */
#define PACING_BW_WIN_RTTS 10U

/* Gain over the cwnd/srtt fallback used before the first delivery sample. */
#define PACING_STARTUP_NUM 2U
#define PACING_STARTUP_DEN 1U

/* Headroom left unused so pacing cannot itself build a queue. */
#define PACING_MARGIN_PCT 1U

/* Must outlast any queue the flow builds, or min_rtt is just the queue. */
#define PACING_RTT_WIN_MS 10000U

/* In-flight ceiling in BDPs. Rate alone holds a queue at its current depth;
 * this is what drains it, and the only brake left when nothing is ever dropped. */
#define PACING_INFLIGHT_BDP      2U
#define PACING_INFLIGHT_MIN_PKTS 4U

/* Longest smoothed round trip the ceiling will size itself for. Bounds the
 * srtt term against a stalled link, where srtt runs to seconds; past this the
 * data in flight is older than anything a live stream would still show. */
#define PACING_RTT_FOLLOW_MAX_MS 1000U

/* ProbeRTT: drain the queue periodically so min_rtt sees an unloaded path.
 * Without it the filter latches onto a queued sample and the BDP is meaningless. */
/* Opening in-flight allowance before anything is measured, as TCP's IW10. */
#define PACING_INIT_INFLIGHT_PKTS 10U

#define PACING_PROBERTT_FIRST_MS  1000U
#define PACING_PROBERTT_PERIOD_MS 10000U
#define PACING_PROBERTT_MS        200U

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
struct pacing {
    struct minmax bw;               /* windowed max delivery rate, bytes/sec */
    struct minmax rtt;              /* windowed min rtt, ms - the unloaded latency */
    IUINT32       delivered;        /* cumulative wire bytes the peer has acked */
    IUINT32       sample_delivered; /* delivered as of the last rate sample */
    IUINT32       sample_stamp;     /* when that sample was taken */
    IUINT32       cycle_stamp;      /* when the current gain phase started */
    IUINT32       cycle_idx;        /* position in the gain cycle */
    IUINT32       rate;             /* current pacing rate, bytes/sec */
    IUINT32       inflight;         /* wire bytes sent once and not yet acked */
    IUINT32       inflight_cap;     /* wire bytes allowed outstanding, 0 = no limit */
    IUINT32       probe_at;         /* when the next rtt probe is due */
    IUINT32       probe_until;      /* end of the probe in progress, 0 if none */
    IUINT32       tokens;           /* wire bytes that may be sent right now */
    IUINT32       token_stamp;      /* when tokens were last topped up */
    int           started;          /* a timestamp has been seen, so deltas are real */
};

static const IUINT32 pacing_gain[PACING_GAIN_PHASES] = {
    320, /* 1.25x - probe for headroom */
    192, /* 0.75x - drain what the probe built */
    256, 256, 256, 256, 256, 256,
};

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Signed distance between two millisecond stamps, wrap-safe
 * @param[in] later later stamp
 * @param[in] earlier earlier stamp
 * @return signed difference in milliseconds
 */
static IINT32 pacing_tdiff(IUINT32 later, IUINT32 earlier)
{
    return (IINT32)(later - earlier);
}

int pacing_init(ikcpcb *kcp)
{
    struct pacing *p;

    if (kcp == NULL) {
        return -1;
    }
    p = (struct pacing *)ikcp_malloc(sizeof(*p));
    if (p == NULL) {
        return -1;
    }
    memset(p, 0, sizeof(*p));
    /*
     * One packet of credit to open with. Without it the first flush of a flow
     * whose rate rounds to less than a packet per period would send nothing at
     * all, and nothing would ever be delivered to build an estimate from.
     */
    p->tokens   = kcp->mtu;
    kcp->pacing = p;
    return 0;
}

void pacing_fini(ikcpcb *kcp)
{
    if (kcp == NULL || kcp->pacing == NULL) {
        return;
    }
    ikcp_free(kcp->pacing);
    kcp->pacing = NULL;
}

void pacing_on_acked(ikcpcb *kcp, uint32_t wire_bytes)
{
    struct pacing *p;

    if (kcp == NULL || kcp->pacing == NULL) {
        return;
    }
    p = (struct pacing *)kcp->pacing;
    /* Wraps every 4 GB; every read below is a difference, so wrapping is safe. */
    p->delivered += wire_bytes;
    p->inflight = (wire_bytes >= p->inflight) ? 0 : (p->inflight - wire_bytes);
}

void pacing_on_rtt(ikcpcb *kcp, uint32_t rtt)
{
    struct pacing *p;

    if (kcp == NULL || kcp->pacing == NULL || rtt == 0) {
        return;
    }
    p = (struct pacing *)kcp->pacing;
    /*
     * The minimum, not the average: the smallest round trip seen recently is
     * the path without a queue in it, which is what the BDP has to be built
     * from. kcp->rx_srtt cannot serve here - on a bloated link it is mostly
     * queue, and a BDP computed from it would licence exactly the backlog it
     * was measuring.
     */
    minmax_running_min(&p->rtt, PACING_RTT_WIN_MS, kcp->current, rtt);
}

/**
 * @brief Smoothed RTT, floored at one flush period
 * @param[in] kcp kcp control block
 * @return smoothed round trip in milliseconds, never below kcp->interval
 */
static IUINT32 pacing_srtt(const ikcpcb *kcp)
{
    IUINT32 srtt = (kcp->rx_srtt > 0) ? (IUINT32)kcp->rx_srtt : (IUINT32)kcp->rx_rto;

    if (srtt < kcp->interval) {
        srtt = kcp->interval;
    }
    return srtt;
}

/**
 * @brief Sample the delivery rate over the last round trip into the max filter
 * @param[in,out] p pacing state
 * @param[in] kcp kcp control block
 * @param[in] now current millisecond stamp
 * @return none
 * @note One RTT minimum: a shorter window measures the ack clumping, not the link.
 */
static void pacing_sample(struct pacing *p, const ikcpcb *kcp, IUINT32 now)
{
    IUINT32 srtt = pacing_srtt(kcp);
    IUINT32 delta, acked, bw;

    if (!p->started) {
        p->started          = 1;
        p->sample_stamp     = now;
        p->sample_delivered = p->delivered;
        p->cycle_stamp      = now;
        p->token_stamp      = now;
        p->probe_at         = now + PACING_PROBERTT_FIRST_MS;
        return;
    }

    if (pacing_tdiff(now, p->sample_stamp) < (IINT32)srtt) {
        return;
    }
    delta = (IUINT32)pacing_tdiff(now, p->sample_stamp);
    acked = p->delivered - p->sample_delivered;

    /*
     * Only a window that actually delivered something says anything about the
     * link. An idle stretch - the encoder between key frames, a paused viewer -
     * would otherwise enter the filter as a low rate and hold the flow down
     * long after there is data to send again.
     */
    if (acked > 0) {
        bw = (IUINT32)(((IUINT64)acked * 1000u) / delta);
        minmax_running_max(&p->bw, srtt * PACING_BW_WIN_RTTS, now, bw);
    }
    p->sample_stamp     = now;
    p->sample_delivered = p->delivered;
}

/**
 * @brief Advance the gain cycle one phase per round trip
 * @param[in,out] p pacing state
 * @param[in] kcp kcp control block
 * @param[in] now current millisecond stamp
 * @return none
 */
static void pacing_advance_cycle(struct pacing *p, const ikcpcb *kcp, IUINT32 now)
{
    if (pacing_tdiff(now, p->cycle_stamp) >= (IINT32)pacing_srtt(kcp)) {
        p->cycle_stamp = now;
        p->cycle_idx   = (p->cycle_idx + 1u) % PACING_GAIN_PHASES;
    }
}

/**
 * @brief Rate to pace at, bytes per second
 * @param[in] p pacing state
 * @param[in] kcp kcp control block
 * @return pacing rate; falls back to cwnd/srtt before the first delivery sample
 */
static IUINT32 pacing_target_rate(const struct pacing *p, const ikcpcb *kcp)
{
    IUINT32 bw = minmax_get(&p->bw);
    IUINT32 wnd, srtt;
    IUINT64 rate;

    if (bw > 0) {
        rate = ((IUINT64)bw * pacing_gain[p->cycle_idx]) / PACING_GAIN_UNIT;
        rate = (rate * (100u - PACING_MARGIN_PCT)) / 100u;
    } else {
        wnd = (kcp->snd_wnd < kcp->rmt_wnd) ? kcp->snd_wnd : kcp->rmt_wnd;
        if (kcp->nocwnd == 0 && kcp->cwnd < wnd) {
            wnd = kcp->cwnd;
        }
        if (wnd < 1) {
            wnd = 1;
        }
        srtt = pacing_srtt(kcp);
        rate = ((IUINT64)wnd * kcp->mtu * 1000u * PACING_STARTUP_NUM) / ((IUINT64)srtt * PACING_STARTUP_DEN);
    }

    if (rate > 0xFFFFFFFFull) {
        rate = 0xFFFFFFFFull;
    }
    return (IUINT32)rate;
}

/**
 * @brief Bytes allowed outstanding, from the measured BDP
 * @param[in,out] p pacing state
 * @param[in] kcp kcp control block
 * @param[in] now current millisecond stamp
 * @return ceiling in wire bytes, 0 if nothing has been measured yet
 */
static IUINT32 pacing_inflight_cap(struct pacing *p, const ikcpcb *kcp, IUINT32 now)
{
    IUINT32 bw, min_rtt;
    IUINT64 cap;

    /*
     * Due for a probe: squeeze in flight down to a few packets so the queue
     * ahead of us empties and the next round trip measures the path instead of
     * the backlog. Held for a fixed spell rather than until a sample arrives,
     * because on a badly bloated link the sample that ends it is itself stuck
     * behind the queue being drained.
     */
    if (pacing_tdiff(now, p->probe_at) >= 0) {
        if (p->probe_until == 0) {
            /*
             * Long enough to drain what is actually queued, not a fixed spell.
             * srtt is a measure of that backlog on a bloated link, so it is the
             * right scale; a constant 200 ms was measured ending the probe with
             * five seconds of queue still ahead of it.
             */
            IUINT32 hold = pacing_srtt(kcp);
            if (hold < PACING_PROBERTT_MS) {
                hold = PACING_PROBERTT_MS;
            }
            p->probe_until = now + hold;
        }
        if (pacing_tdiff(now, p->probe_until) < 0) {
            return PACING_INFLIGHT_MIN_PKTS * kcp->mtu;
        }
        p->probe_at    = now + PACING_PROBERTT_PERIOD_MS;
        p->probe_until = 0;
    }

    bw      = minmax_get(&p->bw);
    min_rtt = minmax_get(&p->rtt);
    if (bw == 0 || min_rtt == 0) {
        return PACING_INIT_INFLIGHT_PKTS * kcp->mtu;
    }
    cap = (((IUINT64)bw * min_rtt) / 1000u) * PACING_INFLIGHT_BDP;

    /*
     * A second floor, from the round trip a packet actually experiences.
     *
     * The BDP above is built from the unloaded latency on purpose - that is
     * what stops the ceiling licencing a queue it is measuring. But it assumes
     * every queue on the path is one this flow put there and can therefore see.
     * Where the backlog sits somewhere else - a radio's own transmit queue, an
     * access point - min_rtt keeps reading the empty path while packets wait
     * far longer, and the ceiling shrinks to a size that cannot sustain the
     * bandwidth already measured. It then throttles the flow, which lowers the
     * delivery rate, which lowers the estimate: the better the path's unloaded
     * latency, the harder this holds it down.
     *
     * Measured on hardware, same firmware a minute apart: over a relay with
     * min_rtt 142 ms the ceiling was 45 packets and the flow ran at 1.0 Mbps;
     * on the LAN beside it, min_rtt 20 ms against a 141 ms smoothed round trip
     * put the ceiling on its four packet floor and the same flow managed
     * 318 kbps, with 22 pct of frames shed for want of anywhere to go.
     *
     * Little's law gives the fix its size: sustaining bw at a round trip of
     * srtt needs exactly bw * srtt outstanding, no more. Sending stays paced at
     * bw either way, so this cannot feed a queue - it only stops the ceiling
     * from being the thing in the way.
     */
    {
        IUINT32 srtt = pacing_srtt(kcp);
        IUINT64 sustain;

        if (srtt > PACING_RTT_FOLLOW_MAX_MS) {
            srtt = PACING_RTT_FOLLOW_MAX_MS;
        }
        sustain = ((IUINT64)bw * srtt) / 1000u;
        if (cap < sustain) {
            cap = sustain;
        }
    }

    if (cap < (IUINT64)PACING_INFLIGHT_MIN_PKTS * kcp->mtu) {
        cap = (IUINT64)PACING_INFLIGHT_MIN_PKTS * kcp->mtu;
    }
    if (cap > 0xFFFFFFFFull) {
        cap = 0xFFFFFFFFull;
    }
    return (IUINT32)cap;
}

void pacing_flush_begin(ikcpcb *kcp)
{
    struct pacing *p;
    IUINT32        now, cap;
    IINT32         elapsed;
    IUINT64        topup;

    if (kcp == NULL || kcp->pacing == NULL) {
        return;
    }
    p = (struct pacing *)kcp->pacing;
    now = kcp->current;

    pacing_sample(p, kcp, now);
    pacing_advance_cycle(p, kcp, now);
    p->rate = pacing_target_rate(p, kcp);

    /*
     * Credit accrues with time rather than being reset each flush. A rate below
     * one packet per flush period is common on a poor link - version 2 rounded
     * that up to a whole packet every period, which put a floor of about
     * 1.1 Mbit/s under the pacer and made it unable to slow down at all on the
     * link that needed it most. Carrying the remainder lets the flow send one
     * packet every few periods instead.
     */
    elapsed = pacing_tdiff(now, p->token_stamp);
    if (elapsed > 0) {
        topup = ((IUINT64)p->rate * (IUINT32)elapsed) / 1000u;
        if (topup > 0xFFFFFFFFull) {
            topup = 0xFFFFFFFFull;
        }
        if (p->tokens > 0xFFFFFFFFu - (IUINT32)topup) {
            p->tokens = 0xFFFFFFFFu;
        } else {
            p->tokens += (IUINT32)topup;
        }
        p->token_stamp = now;
    }

    /*
     * Cap the credit at one flush period's worth plus a packet. Any more and an
     * idle flow would bank enough to burst on its first frame back, which is
     * the behaviour pacing exists to prevent; any less and a fast link could
     * not spend a period's allowance within the period.
     */
    cap = (IUINT32)(((IUINT64)p->rate * kcp->interval) / 1000u);
    if (cap > 0xFFFFFFFFu - kcp->mtu) {
        cap = 0xFFFFFFFFu - kcp->mtu;
    }
    cap += kcp->mtu;
    if (cap < 2u * kcp->mtu) {
        cap = 2u * kcp->mtu;
    }
    if (p->tokens > cap) {
        p->tokens = cap;
    }

    /*
     * Ceiling on what may be outstanding, from the measured BDP. Left at zero
     * until both terms have been measured, so a flow that has not yet learned
     * anything is governed by cwnd alone rather than by a guess.
     */
    p->inflight_cap = pacing_inflight_cap(p, kcp, now);
}

uint32_t pacing_min_rtt(const ikcpcb *kcp)
{
    if (kcp == NULL || kcp->pacing == NULL) {
        return 0;
    }
    return minmax_get(&((const struct pacing *)kcp->pacing)->rtt);
}

uint32_t pacing_bw(const ikcpcb *kcp)
{
    if (kcp == NULL || kcp->pacing == NULL) {
        return 0;
    }
    return minmax_get(&((const struct pacing *)kcp->pacing)->bw);
}

int pacing_try_send(ikcpcb *kcp, uint32_t pkt_len, int is_new)
{
    struct pacing *p;

    if (kcp == NULL || kcp->pacing == NULL) {
        return 1;
    }
    p = (struct pacing *)kcp->pacing;

    /*
     * The ceiling applies to new data only. A retransmission replaces something
     * already counted as outstanding, so charging it again would let a lossy
     * spell starve the very repairs that end it - and kcp->nsnd_buf cannot serve
     * as the measure here either, since it counts segments still waiting for
     * their first transmission alongside those actually in flight.
     */
    if (is_new && p->inflight_cap > 0 && p->inflight >= p->inflight_cap) {
        return 0;
    }

    /* Retransmissions are charged against the rate: they occupy the same queue. */
    if (p->tokens < pkt_len) {
        return 0;
    }
    p->tokens -= pkt_len;
    if (is_new) {
        p->inflight += pkt_len;
    }
    return 1;
}
