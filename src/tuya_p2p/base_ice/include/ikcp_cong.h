/**
 * @file ikcp_cong.h
 * @brief Pluggable congestion control for KCP, CUBIC implementation
 * @version 1.0
 * @date 2026-08-13
 * @copyright Copyright (c) Tuya Inc.
 *
 * @note CUBIC congestion ops (init/release/pkts_loss/fast_recovery/
 *       ssthresh/cong_avoid/pkts_acked/update_rtt). Stock KCP grows cwnd by
 *       one per RTT once past slow start, which needs many round trips to
 *       reach LAN capacity and stalls the first seconds of a live stream.
 *       CUBIC reaches it in a few RTTs.
 */
#ifndef __IKCP_CONG_H__
#define __IKCP_CONG_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ikcp.h lives under src/, so mirror ikcp_pacing.h and forward declare. */
struct IKCPCB;
typedef struct IKCPCB ikcpcb;

typedef uint32_t IKCP_CONG_U32;
typedef int32_t  IKCP_CONG_S32;

/**
 * @brief Attach CUBIC congestion control to a kcp control block
 * @param[in,out] kcp kcp control block
 * @return 0 on success, <0 on allocation failure (caller keeps stock KCP)
 */
int ikcp_cong_cubic_init(ikcpcb *kcp);

/**
 * @brief Detach and free CUBIC state
 * @param[in,out] kcp kcp control block
 * @return none
 */
void ikcp_cong_cubic_release(ikcpcb *kcp);

/**
 * @brief Grow cwnd after an ACK advanced snd_una (replaces stock reno growth)
 * @param[in,out] kcp kcp control block
 * @param[in] acked number of newly acknowledged packets
 * @return none
 */
void ikcp_cong_cubic_on_ack(ikcpcb *kcp, IKCP_CONG_U32 acked);

/**
 * @brief Recompute ssthresh/cwnd on loss
 * @param[in,out] kcp kcp control block
 * @param[in] is_timeout non-zero for RTO loss, zero for fast retransmit
 * @return none
 * @note Sets both ssthresh and cwnd; the caller must not also apply its own
 *       reduction or the window collapses twice for one loss event.
 */
void ikcp_cong_cubic_on_loss(ikcpcb *kcp, int is_timeout);

/**
 * @brief Feed an RTT sample so CUBIC can track the minimum delay
 * @param[in,out] kcp kcp control block
 * @param[in] rtt_ms measured round trip time in milliseconds
 * @return none
 */
void ikcp_cong_cubic_on_rtt(ikcpcb *kcp, IKCP_CONG_S32 rtt_ms);

#ifdef __cplusplus
}
#endif

#endif /* __IKCP_CONG_H__ */
