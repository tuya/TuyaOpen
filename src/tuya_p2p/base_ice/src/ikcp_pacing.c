/**
 * @file ikcp_pacing.c
 * @brief KCP send pacing aligned with TuyaOS tuya_p2p_lite_ikcp_pacing
 * @version 1.0
 * @date 2026-08-04
 * @copyright Copyright (c) Tuya Inc.
 *
 * @note TuyaOS 3.13.8 ships pacing_init/fini from ikcp_create/release, but
 *       pacing_update/rate are not referenced by that archive. OpenSDK wires
 *       them into ACK + flush so UDP is rate-limited under weak networks.
 */
#include "ikcp_pacing.h"
#include "ikcp_minmax.h"
#include "ikcp.h"
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define PACING_GAIN_UNIT       256U
#define PACING_RATE_INIT       800U  /* bytes/ms ≈ 6.4 Mbps */
#define PACING_RATE_MIN        100U  /* bytes/ms ≈ 0.8 Mbps */
#define PACING_BW_WIN_MS       48U
#define PACING_CYCLE_LEN       8U

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
struct pacing {
    struct minmax bw;
    uint32_t cycle_mstamp;
    uint32_t cycle_idx;
    uint32_t lt_last_delivered;
    uint32_t lt_last_stamp;
    uint32_t pacing_gain;
    unsigned long rate;
    uint32_t sampling_count;
};

/* OS .rodata.pacing_gain: 0x140, 0xc0, then 0x100 x6 */
static const uint32_t s_pacing_gain[PACING_CYCLE_LEN] = {
    0x140, 0xc0, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100,
};

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Convert measured bw (pkt*256/ms) and gain into bytes/ms rate
 * @param[in] kcp kcp control block
 * @param[in] bw_fp bandwidth fixed-point packets*256 per ms
 * @param[in] gain pacing gain (1.0 == 256)
 * @return rate in bytes/ms
 */
static uint32_t __bw_to_rate(const ikcpcb *kcp, uint32_t bw_fp, uint32_t gain)
{
    uint64_t rate;

    /* bytes/ms = (bw_fp * mss * gain) / (256 * 256) */
    rate = (uint64_t)bw_fp * (uint64_t)kcp->mss * (uint64_t)gain;
    rate >>= 16;
    if (rate < PACING_RATE_MIN) {
        rate = PACING_RATE_MIN;
    }
    if (rate > 0xffffffffUL) {
        rate = 0xffffffffUL;
    }
    return (uint32_t)rate;
}

/**
 * @brief Apply gain to current bw estimate and store rate
 * @param[in,out] kcp kcp control block
 * @param[in] p pacing state
 * @return none
 */
static void __set_pacing_rate(ikcpcb *kcp, struct pacing *p)
{
    uint32_t bw_fp;

    bw_fp = minmax_get(&p->bw);
    p->rate = __bw_to_rate(kcp, bw_fp, p->pacing_gain);
}

/**
 * @brief Allocate and attach pacing state to kcp
 * @param[in,out] kcp kcp control block
 * @return 0 on success, <0 on failure
 */
int pacing_init(ikcpcb *kcp)
{
    struct pacing *p;

    if (kcp == NULL) {
        return -1;
    }
    p = (struct pacing *)calloc(1, sizeof(*p));
    if (p == NULL) {
        return -1;
    }
    minmax_reset(&p->bw, kcp->current, 0);
    p->rate = PACING_RATE_INIT;
    p->pacing_gain = s_pacing_gain[0];
    p->cycle_idx = (uint32_t)(rand() & 7);
    p->pacing_gain = s_pacing_gain[p->cycle_idx];
    p->lt_last_stamp = kcp->current;
    p->lt_last_delivered = kcp->snd_una;
    p->cycle_mstamp = 0;
    p->sampling_count = 0;
    kcp->pacing = p;
    kcp->next_send = kcp->current;
    return 0;
}

/**
 * @brief Free pacing state
 * @param[in,out] kcp kcp control block
 * @return none
 */
void pacing_fini(ikcpcb *kcp)
{
    if (kcp == NULL || kcp->pacing == NULL) {
        return;
    }
    free(kcp->pacing);
    kcp->pacing = NULL;
}

/**
 * @brief Update bandwidth estimate from delivered segments (call on ACK)
 * @param[in,out] kcp kcp control block
 * @return none
 * @note Matches OS lt_bw sampling: drop if idle or interval too long; sample
 *       when delta > 4*RTO; rotate gain when idle > 2*RTO.
 */
void pacing_update(ikcpcb *kcp)
{
    struct pacing *p;
    int32_t delivered;
    int32_t delta;
    uint32_t bw_fp;
    uint32_t rto;
    uint32_t abs_diff;

    if (kcp == NULL || kcp->pacing == NULL) {
        return;
    }
    p = (struct pacing *)kcp->pacing;
    if (p->lt_last_stamp == 0) {
        p->lt_last_stamp = kcp->current;
        p->lt_last_delivered = kcp->snd_una;
        return;
    }

    delivered = (int32_t)(kcp->snd_una - p->lt_last_delivered);
    delta = (int32_t)(kcp->current - p->lt_last_stamp);
    rto = (kcp->rx_rto > 0) ? (uint32_t)kcp->rx_rto : 100U;

    if (delivered <= 0) {
        /* No progress: maybe rotate gain after idle */
        if ((uint32_t)(kcp->current - p->cycle_mstamp) > (rto << 1)) {
            p->cycle_idx = (p->cycle_idx + 1U) & 7U;
            p->cycle_mstamp = kcp->current;
            p->pacing_gain = s_pacing_gain[p->cycle_idx];
        }
        if (p->sampling_count > 4U) {
            __set_pacing_rate(kcp, p);
        }
        return;
    }

    if (delta > (int32_t)(rto << 4)) {
        /* Interval too long — reset sampling window */
        p->lt_last_stamp = kcp->current;
        p->lt_last_delivered = kcp->snd_una;
        return;
    }

    if (delta <= (int32_t)(rto << 2)) {
        return;
    }

    /* bw_fp = delivered * 256 / delta_ms */
    bw_fp = (uint32_t)(((uint64_t)(uint32_t)delivered << 8) / (uint32_t)delta);
    p->sampling_count++;
    abs_diff = (bw_fp > minmax_get(&p->bw)) ? (bw_fp - minmax_get(&p->bw))
                                            : (minmax_get(&p->bw) - bw_fp);
    if (p->sampling_count > 1U && abs_diff > (minmax_get(&p->bw) >> 1) && minmax_get(&p->bw) != 0) {
        minmax_running_max(&p->bw, PACING_BW_WIN_MS, (uint32_t)delivered, bw_fp);
        p->pacing_gain = PACING_GAIN_UNIT;
    } else {
        minmax_running_max(&p->bw, PACING_BW_WIN_MS, (uint32_t)delivered, bw_fp);
    }
    p->lt_last_stamp = kcp->current;
    p->lt_last_delivered = kcp->snd_una;
    if (p->sampling_count > 4U) {
        __set_pacing_rate(kcp, p);
    }
}

/**
 * @brief Get current pacing rate in bytes per millisecond
 * @param[in] kcp kcp control block
 * @return rate (bytes/ms)
 */
uint32_t pacing_rate(ikcpcb *kcp)
{
    struct pacing *p;

    if (kcp == NULL || kcp->pacing == NULL) {
        return 0;
    }
    p = (struct pacing *)kcp->pacing;
    return (uint32_t)p->rate;
}

/**
 * @brief Push next_send forward after UDP send failure
 * @param[in,out] kcp kcp control block
 * @param[in] backoff_ms delay to add from kcp->current
 * @return none
 */
void pacing_on_send_fail(ikcpcb *kcp, uint32_t backoff_ms)
{
    if (kcp == NULL) {
        return;
    }
    if (backoff_ms < 5U) {
        backoff_ms = 5U;
    }
    kcp->next_send = kcp->current + backoff_ms;
}

/**
 * @brief Account a paced data packet and advance next_send
 * @param[in,out] kcp kcp control block
 * @param[in] pkt_len packet length in bytes
 * @return 1 if packet may be sent now, 0 if paced out
 */
int pacing_try_send(ikcpcb *kcp, uint32_t pkt_len)
{
    uint32_t rate;
    uint32_t delay_ms;

    if (kcp == NULL || kcp->pacing == NULL) {
        return 1;
    }
    rate = pacing_rate(kcp);
    if (rate < PACING_RATE_MIN) {
        rate = PACING_RATE_MIN;
    }
    if ((IINT32)(kcp->current - kcp->next_send) < 0) {
        return 0;
    }
    delay_ms = (pkt_len + rate - 1U) / rate;
    if (delay_ms < 1U) {
        delay_ms = 1U;
    }
    kcp->next_send = kcp->current + delay_ms;
    return 1;
}
