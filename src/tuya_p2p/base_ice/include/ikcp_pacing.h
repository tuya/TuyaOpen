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

/** @brief Compile-time switch for KCP send pacing. */
#ifndef IKCP_PACING_RATE_LIMIT
#define IKCP_PACING_RATE_LIMIT 1
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
 * @brief Account wire bytes the peer has acknowledged
 * @param[in,out] kcp kcp control block
 * @param[in] wire_bytes on-the-wire length of the segment being retired
 * @return none
 */
void pacing_on_acked(ikcpcb *kcp, uint32_t wire_bytes);

/**
 * @brief Account a round-trip sample
 * @param[in,out] kcp kcp control block
 * @param[in] rtt measured round trip, milliseconds
 * @return none
 */
void pacing_on_rtt(ikcpcb *kcp, uint32_t rtt);

/**
 * @brief Refresh the pacing rate and top up the send credit
 * @param[in,out] kcp kcp control block
 * @return none
 * @note Call once per ikcp_flush, before any data segment is considered.
 */
void pacing_flush_begin(ikcpcb *kcp);

/**
 * @brief Smallest round trip seen recently, milliseconds
 * @param[in] kcp kcp control block
 * @return the windowed minimum, 0 before anything has been measured
 */
uint32_t pacing_min_rtt(const ikcpcb *kcp);

/**
 * @brief Estimated bottleneck bandwidth, bytes per second
 * @param[in] kcp kcp control block
 * @return the windowed maximum delivery rate, 0 before anything has been measured
 */
uint32_t pacing_bw(const ikcpcb *kcp);

/**
 * @brief Charge a packet against this flush's budget
 * @param[in,out] kcp kcp control block
 * @param[in] pkt_len wire length of the packet, retransmissions included
 * @param[in] is_new non-zero if this is the segment's first transmission
 * @return 1 if it may be sent now, 0 if it must wait (stop the flush)
 */
int pacing_try_send(ikcpcb *kcp, uint32_t pkt_len, int is_new);

#ifdef __cplusplus
}
#endif

#endif /* __IKCP_PACING_H__ */
