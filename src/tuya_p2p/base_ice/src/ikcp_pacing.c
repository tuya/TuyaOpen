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

#define PACING_INFLIGHT_BDP      2U
#define PACING_INFLIGHT_MIN_PKTS 4U

#define PACING_RTT_FOLLOW_MAX_MS 1000U

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

    if (pacing_tdiff(now, p->probe_at) >= 0) {
        if (p->probe_until == 0) {

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
