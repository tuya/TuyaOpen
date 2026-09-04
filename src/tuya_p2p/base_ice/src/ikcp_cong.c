/**
 * @file ikcp_cong.c
 * @brief CUBIC congestion control for KCP
 * @version 1.0
 * @date 2026-08-26
 * @copyright Copyright (c) Tuya Inc.
 * @note Ported from RFC 8312 / Linux net/ipv4/tcp_cubic.c. Derived constants
 *       match the compiled mid_p2p in TuyaOS 3.13.8. HZ is 1000: KCP counts ms.
 */
#include "ikcp.h"
#include "ikcp_cong.h"

#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define BICTCP_BETA_SCALE 1024 /* beta is a fraction of this */
#define BICTCP_HZ         10   /* cwnd fixed-point shift for the cubic term */
#define CUBIC_HZ          1000 /* KCP timestamps are milliseconds */

#define CUBIC_BETA      717 /* 0.7 in BICTCP_BETA_SCALE units */
#define CUBIC_BIC_SCALE 41
#define CUBIC_MIN_CWND  2

/* Do not recompute the curve more than once per this interval (kernel HZ/32). */
#define CUBIC_MIN_RECALC_MS 30

/* Derived, verified against the shipped mid_p2p private state (see file note). */
#define CUBIC_CUBE_RTT_SCALE (CUBIC_BIC_SCALE * 10)                                                        /* 410 */
#define CUBIC_BETA_SCALE_C   (8 * (BICTCP_BETA_SCALE + CUBIC_BETA) / 3 / (BICTCP_BETA_SCALE - CUBIC_BETA)) /* 15 */
#define CUBIC_CUBE_FACTOR    (((IUINT64)1 << (10 + 3 * BICTCP_HZ)) / (CUBIC_BIC_SCALE * 10))

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
struct ikcp_cubic {
    IUINT32 cnt;              /* packets needed to raise cwnd by one */
    IUINT32 last_max_cwnd;    /* cwnd at the previous loss */
    IUINT32 last_cwnd;        /* cwnd when the curve was last recomputed */
    IUINT32 last_time;        /* when the curve was last recomputed (ms) */
    IUINT32 bic_origin_point; /* cwnd at the plateau of the cubic curve (W_max) */
    IUINT32 bic_K;            /* time to reach the plateau, BICTCP_HZ fixed point */
    IUINT32 delay_min;        /* smallest RTT seen (ms) */
    IUINT32 epoch_start;      /* start of the current congestion epoch (ms) */
    /*
     * Two counters, deliberately. ack_cnt feeds the TCP-friendliness estimate,
     * which consumes from it; cwnd_cnt paces the cubic increase itself. Sharing
     * one - as this first did - lets the friendliness loop drain the ACKs the
     * cubic step is waiting on, and congestion avoidance then barely grows at
     * all: the window sits where a loss left it. The kernel keeps these apart
     * for the same reason (ca->ack_cnt versus tp->snd_cwnd_cnt).
     */
    IUINT32 ack_cnt;  /* ACKs counted toward the Reno comparison */
    IUINT32 cwnd_cnt; /* ACKs counted toward the next cubic increment */
    IUINT32 tcp_cwnd; /* cwnd a Reno flow would have (TCP friendliness) */
};

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Integer cube root, as used by the kernel's cubic curve
 * @param[in] a value to take the cube root of
 * @return floor-ish cube root of a
 * @note Newton-Raphson from a power-of-two seed; matches tcp_cubic's cubic_root
 *       closely enough that the curve is indistinguishable in practice.
 */
static IUINT32 __cubic_root(IUINT64 a)
{
    IUINT64 x;
    IUINT64 pow2;
    int     b;

    if (a == 0) {
        return 0;
    }

    /* Seed with 2^ceil(bits(a)/3) so the iteration starts within one octave. */
    b    = 0;
    pow2 = a;
    while (pow2 > 0) {
        pow2 >>= 1;
        b++;
    }
    x = (IUINT64)1 << ((b + 2) / 3);

    /* Three iterations are enough for the 32-bit range cwnd lives in. */
    x = (2 * x + a / (x * x)) / 3;
    if (x == 0) {
        return 1;
    }
    x = (2 * x + a / (x * x)) / 3;
    if (x == 0) {
        return 1;
    }
    x = (2 * x + a / (x * x)) / 3;
    if (x == 0) {
        return 1;
    }
    return (IUINT32)x;
}

/**
 * @brief Reset per-epoch curve state
 * @param[in,out] ca cubic state
 * @return none
 */
static void __cubic_reset(struct ikcp_cubic *ca)
{
    ca->cnt              = 0;
    ca->last_max_cwnd    = 0;
    ca->last_cwnd        = 0;
    ca->last_time        = 0;
    ca->bic_origin_point = 0;
    ca->bic_K            = 0;
    ca->delay_min        = 0;
    ca->epoch_start      = 0;
    ca->ack_cnt          = 0;
    ca->cwnd_cnt         = 0;
    ca->tcp_cwnd         = 0;
}

/**
 * @brief Recompute how many ACKs are needed before cwnd may grow again
 * @param[in,out] kcp kcp control block
 * @param[in,out] ca cubic state
 * @param[in] cwnd current congestion window
 * @param[in] acked packets acknowledged by this ACK
 * @return none
 */
static void __cubic_update(ikcpcb *kcp, struct ikcp_cubic *ca, IUINT32 cwnd, IUINT32 acked)
{
    IUINT64 offs;
    IUINT64 delta;
    IUINT64 t;
    IUINT32 bic_target;
    IUINT32 now = kcp->current;

    ca->ack_cnt += acked;

    /* Throttle recomputation: the curve cannot move meaningfully inside 30ms. */
    if (ca->last_cwnd == cwnd && (IINT32)(now - ca->last_time) <= (IINT32)CUBIC_MIN_RECALC_MS) {
        return;
    }

    /*
     * Once the epoch has started and this is still the same millisecond, the
     * previously computed cnt is still valid - recomputing would only add
     * rounding noise.
     */
    if (ca->epoch_start != 0 && now == ca->last_time) {
        return;
    }

    ca->last_cwnd = cwnd;
    ca->last_time = now;

    if (ca->epoch_start == 0) {
        ca->epoch_start = now;
        ca->ack_cnt     = acked;
        ca->tcp_cwnd    = cwnd;

        if (ca->last_max_cwnd <= cwnd) {
            /* Already above the last loss point: plateau is here, climb now. */
            ca->bic_K            = 0;
            ca->bic_origin_point = cwnd;
        } else {
            /*
             * K = cbrt((W_max - cwnd) * cube_factor), the time still needed to
             * climb back to the window that last caused a loss.
             */
            ca->bic_K            = __cubic_root(CUBIC_CUBE_FACTOR * (IUINT64)(ca->last_max_cwnd - cwnd));
            ca->bic_origin_point = ca->last_max_cwnd;
        }
    }

    /* t = elapsed time in this epoch, in BICTCP_HZ fixed point. */
    t = (IUINT64)((IUINT32)(now - ca->epoch_start));
    t <<= BICTCP_HZ;
    t /= CUBIC_HZ;

    /* delta = cube_rtt_scale * |t - K|^3, scaled back down by 3*BICTCP_HZ. */
    if (t < (IUINT64)ca->bic_K) {
        offs = (IUINT64)ca->bic_K - t;
    } else {
        offs = t - (IUINT64)ca->bic_K;
    }
    delta = (CUBIC_CUBE_RTT_SCALE * offs * offs * offs) >> (10 + 3 * BICTCP_HZ);

    if (t < (IUINT64)ca->bic_K) {
        bic_target = (IUINT32)(ca->bic_origin_point - delta); /* still below plateau */
    } else {
        bic_target = (IUINT32)(ca->bic_origin_point + delta); /* past it, probing up */
    }

    if (bic_target > cwnd) {
        ca->cnt = cwnd / (bic_target - cwnd);
    } else {
        /* Target already reached: creep, do not stall completely. */
        ca->cnt = 100 * cwnd;
    }

    /*
     * Very start of a connection with no loss history: be aggressive so the
     * first seconds of a stream are not spent ramping.
     */
    if (ca->last_max_cwnd == 0 && ca->cnt > 20) {
        ca->cnt = 20;
    }

    /*
     * TCP friendliness - if a Reno flow would have grown faster by now, use
     * its window instead so CUBIC never loses to plain Reno on short links.
     */
    if (ca->delay_min > 0) {
        /*
         * Reno's window after the same number of ACKs, tracked alongside the
         * cubic one; where it would have been ahead, grow at its pace instead.
         * The step is (cwnd * beta_scale) >> 3 ACKs per additional packet, and
         * the surplus is carried rather than discarded - dropping it, as an
         * earlier version here did by zeroing ack_cnt, loses part of every
         * increment and quietly makes the flow less aggressive than Reno,
         * which is the opposite of what this is for.
         */
        IUINT32 delta = (cwnd * CUBIC_BETA_SCALE_C) >> 3;

        while (delta > 0 && ca->ack_cnt > delta) {
            ca->ack_cnt -= delta;
            ca->tcp_cwnd++;
        }
        if (ca->tcp_cwnd > cwnd) {
            IUINT32 delta_cwnd = ca->tcp_cwnd - cwnd;
            IUINT32 max_cnt    = cwnd / delta_cwnd;

            if (ca->cnt > max_cnt) {
                ca->cnt = max_cnt;
            }
        }
    }

    /* cnt must stay >= 2 so cwnd grows at most one per two ACKs. */
    if (ca->cnt < 2) {
        ca->cnt = 2;
    }
}

int ikcp_cong_cubic_init(ikcpcb *kcp)
{
    struct ikcp_cubic *ca;

    if (kcp == NULL) {
        return -1;
    }
    if (kcp->cong != NULL) {
        return 0;
    }
    /* Through KCP's own hooks, not bare malloc: the SDK overrides them to put
     * transport allocations where it wants them (PSRAM on parts that have it)
     * and to account for them. */
    ca = (struct ikcp_cubic *)ikcp_malloc(sizeof(*ca));
    if (ca == NULL) {
        return -1;
    }
    __cubic_reset(ca);
    kcp->cong = ca;
    return 0;
}

void ikcp_cong_cubic_release(ikcpcb *kcp)
{
    if (kcp == NULL || kcp->cong == NULL) {
        return;
    }
    ikcp_free(kcp->cong);
    kcp->cong = NULL;
}

void ikcp_cong_cubic_on_ack(ikcpcb *kcp, IKCP_CONG_U32 acked)
{
    struct ikcp_cubic *ca;
    IUINT32            mss;

    if (kcp == NULL || kcp->cong == NULL || acked == 0) {
        return;
    }
    ca  = (struct ikcp_cubic *)kcp->cong;
    mss = kcp->mss;

    if (kcp->cwnd >= kcp->rmt_wnd) {
        return; /* peer's receive window is the binding limit, not congestion */
    }

    if (kcp->cwnd < kcp->ssthresh) {
        /* Slow start: one extra packet per ACK, same as stock KCP. */
        kcp->cwnd += acked;
        if (kcp->cwnd > kcp->ssthresh) {
            kcp->cwnd = kcp->ssthresh;
        }
        kcp->incr = kcp->cwnd * mss;
    } else {
        /* Congestion avoidance: follow the cubic curve. */
        __cubic_update(kcp, ca, kcp->cwnd, acked);
        ca->cwnd_cnt += acked;
        if (ca->cnt > 0 && ca->cwnd_cnt >= ca->cnt) {
            ca->cwnd_cnt -= ca->cnt; /* carry the remainder, do not discard it */
            kcp->cwnd++;
            kcp->incr = kcp->cwnd * mss;
        }
    }

    if (kcp->cwnd > kcp->rmt_wnd) {
        kcp->cwnd = kcp->rmt_wnd;
        kcp->incr = kcp->rmt_wnd * mss;
    }
    if (kcp->cwnd < CUBIC_MIN_CWND) {
        kcp->cwnd = CUBIC_MIN_CWND;
        kcp->incr = kcp->cwnd * mss;
    }
}

void ikcp_cong_cubic_on_loss(ikcpcb *kcp, int is_timeout)
{
    struct ikcp_cubic *ca;
    IUINT32            cwnd;

    if (kcp == NULL || kcp->cong == NULL) {
        return;
    }
    ca   = (struct ikcp_cubic *)kcp->cong;
    cwnd = kcp->cwnd;

    ca->epoch_start = 0; /* a new epoch begins after this loss */

    /*
     * Fast convergence: when we lose below the previous loss point, the
     * available bandwidth has probably dropped, so pull W_max down further to
     * let the flow settle instead of aiming at a window that no longer fits.
     */
    if (cwnd < ca->last_max_cwnd) {
        ca->last_max_cwnd = (cwnd * (BICTCP_BETA_SCALE + CUBIC_BETA)) / (2 * BICTCP_BETA_SCALE);
    } else {
        ca->last_max_cwnd = cwnd;
    }

    kcp->ssthresh = (cwnd * CUBIC_BETA) / BICTCP_BETA_SCALE;
    if (kcp->ssthresh < CUBIC_MIN_CWND) {
        kcp->ssthresh = CUBIC_MIN_CWND;
    }

    if (is_timeout) {
        /* RTO: the pipe is presumed empty, restart from a minimal window. */
        kcp->cwnd = 1;
    } else {
        /* Fast retransmit: multiplicative decrease only. */
        kcp->cwnd = kcp->ssthresh;
    }
    kcp->incr = kcp->cwnd * kcp->mss;
}

void ikcp_cong_cubic_on_rtt(ikcpcb *kcp, IKCP_CONG_S32 rtt_ms)
{
    struct ikcp_cubic *ca;

    if (kcp == NULL || kcp->cong == NULL || rtt_ms <= 0) {
        return;
    }
    ca = (struct ikcp_cubic *)kcp->cong;
    if (ca->delay_min == 0 || (IUINT32)rtt_ms < ca->delay_min) {
        ca->delay_min = (IUINT32)rtt_ms;
    }
}
