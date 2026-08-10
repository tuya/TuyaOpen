/**
 * @file ikcp_pacing.h
 * @brief KCP send pacing (TuyaOS mid_p2p ikcp_pacing)
 * @version 1.0
 * @date 2026-08-04
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef __IKCP_PACING_H__
#define __IKCP_PACING_H__

#include <stdint.h>

/**
 * @brief Compile-time switch for KCP send rate-limiting (pacing).
 *
 * TuyaOS libtuyaos.a ships pacing_init/fini but never calls pacing_update /
 * pacing_try_send / pacing_on_send_fail — they are dead code; the OS sends at
 * full rate and relies on KCP cwnd for congestion control. OpenSDK wired those
 * calls in, which throttles the ~1Mbps video stream whenever the bandwidth
 * estimate runs low and accumulates latency.
 *
 * Default 0 (OFF) matches the OS low-latency behavior. Define to 1 to re-enable
 * pacing (weak-network tuning / debugging).
 */
#ifndef IKCP_PACING_RATE_LIMIT
#define IKCP_PACING_RATE_LIMIT 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct IKCPCB;
typedef struct IKCPCB ikcpcb;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Allocate and attach pacing state to kcp
 * @param[in,out] kcp kcp control block
 * @return 0 on success, <0 on failure
 */
int pacing_init(ikcpcb *kcp);

/**
 * @brief Free pacing state
 * @param[in,out] kcp kcp control block
 * @return none
 */
void pacing_fini(ikcpcb *kcp);

/**
 * @brief Update bandwidth estimate from delivered segments (call on ACK)
 * @param[in,out] kcp kcp control block
 * @return none
 */
void pacing_update(ikcpcb *kcp);

/**
 * @brief Get current pacing rate in bytes per millisecond
 * @param[in] kcp kcp control block
 * @return rate (bytes/ms), minimum 100 when pacing is active
 */
uint32_t pacing_rate(ikcpcb *kcp);

/**
 * @brief Push next_send forward after UDP send failure (ENOBUFS backoff)
 * @param[in,out] kcp kcp control block
 * @param[in] backoff_ms delay to add from kcp->current
 * @return none
 */
void pacing_on_send_fail(ikcpcb *kcp, uint32_t backoff_ms);

/**
 * @brief Account a paced data packet and advance next_send
 * @param[in,out] kcp kcp control block
 * @param[in] pkt_len packet length in bytes
 * @return 1 if packet may be sent now, 0 if paced out (caller should stop data flush)
 */
int pacing_try_send(ikcpcb *kcp, uint32_t pkt_len);

#ifdef __cplusplus
}
#endif

#endif /* __IKCP_PACING_H__ */
