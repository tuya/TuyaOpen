/**
 * @file tal_net_route.h
 * @brief Single source of truth for the route the data plane currently uses.
 *
 * A route is the pair of decisions every outbound socket depends on: which
 * socket ops backend to call into, and which local address to bind as the
 * source. Both are owned by whoever owns link selection (netmgr) and pushed
 * down once per link event, rather than looked up on every connect.
 *
 * The two halves must move together. A link switch that publishes the new
 * backend and the new address as two independent stores leaves a window where
 * sockets are created on the new backend but bound to the address of the link
 * that just went away. tal_net_route_set() closes that window by publishing
 * both in one guarded update, and tal_net_route_get() always returns a
 * consistent snapshot rather than one half of each.
 *
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TAL_NET_ROUTE_H__
#define __TAL_NET_ROUTE_H__

#include "tuya_cloud_types.h"
#include "tal_network_register.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t provider;      /* which socket ops backend; values are the existing
                              TAL_NET_PROVIDER_* constants */
    TUYA_IP_ADDR_T src_ip; /* source address outbound sockets bind to, 0 = do not bind */
} tal_net_route_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Publish the route the data plane should use from now on.
 *
 * Both fields are applied as one update, so no reader can observe the new
 * provider paired with the previous source address or the other way round.
 *
 * @param[in] route the route to publish. src_ip may be 0, meaning "do not
 *                  bind" - readers then let the stack pick the source itself.
 *
 * @return OPRT_OK on success. OPRT_INVALID_PARM when @a route is NULL or its
 *         provider is not a valid TAL_NET_PROVIDER_* value. Others on error, please
 *         refer to tuya_error_code.h
 */
OPERATE_RET tal_net_route_set(const tal_net_route_t *route);

/**
 * @brief Read the route currently in force.
 *
 * @param[out] route receives a consistent snapshot of both fields.
 *
 * @return OPRT_OK on success. OPRT_INVALID_PARM when @a route is NULL. Others
 *         on error, please refer to tuya_error_code.h
 */
OPERATE_RET tal_net_route_get(tal_net_route_t *route);

/**
 * @brief Get just the source address outbound sockets should bind to.
 *
 * Reads one field without taking the route lock, which is why it exists at all:
 * it sits on the connect path and does not have to agree with the provider. A
 * caller that needs the two halves to agree must use tal_net_route_get().
 *
 * @return the active connection address, or 0 when unknown - callers must treat
 *         0 as "do not bind" and let the stack pick the source itself.
 */
TUYA_IP_ADDR_T tal_net_route_src_ip(void);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_NET_ROUTE_H__ */
