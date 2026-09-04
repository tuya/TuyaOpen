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
 * @brief Compile-time switch for KCP send pacing.
 *
 * On means ikcp_flush spreads the congestion window across the RTT instead of
 * handing it to the socket all at once. Off restores the burst behaviour, which
 * is only useful for reproducing the loss pattern it was introduced to fix.
 *
 * This was once off for good reason: the original implementation advanced its
 * next-send deadline off kcp->current, which does not move within a flush, so
 * it let exactly one segment through per flush whatever rate it had computed,
 * and the caller charged it the accumulated output buffer rather than the
 * segment. Both faults are gone. The rate is measured from what the peer
 * acknowledges - see ikcp_pacing.c for why that is preferred over cwnd/srtt.
 */
#ifndef IKCP_PACING_RATE_LIMIT
#define IKCP_PACING_RATE_LIMIT 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ikcp.h sits next to ikcp.c rather than on the include path, so it cannot be
 * pulled in from here - forward declare instead, as this header always has. */
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
 * @note Call wherever a segment leaves snd_buf because it was acked. This is
 *       the only input to the rate estimate; without it pacing falls back to
 *       cwnd/srtt forever.
 */
void pacing_on_acked(ikcpcb *kcp, uint32_t wire_bytes);

/**
 * @brief Account a round-trip sample
 * @param[in,out] kcp kcp control block
 * @param[in] rtt measured round trip, milliseconds
 * @return none
 * @note Feeds the windowed minimum the bandwidth-delay product is built from.
 *       Without it pacing sets no in-flight ceiling and cannot drain a queue.
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
 * @note Worth logging next to rx_srtt: the gap between the two is the standing
 *       queue, which is the one number that separates a slow link from a bloated
 *       one and is not otherwise visible from outside.
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
