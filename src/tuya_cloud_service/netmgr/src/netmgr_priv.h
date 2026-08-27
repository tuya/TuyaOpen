/**
 * @file netmgr_priv.h
 * @brief Module-internal contract between netmgr.c and its satellites.
 *
 * netmgr_cli.c lives in a separate translation unit and cannot reach the
 * static `s_netmgr`. This header is the narrow, read-only window it gets
 * instead: snapshot accessors that take s_netmgr.lock, copy out, and release
 * it, so no caller ever holds the lock while formatting output.
 *
 * Not a public API - reachable from anywhere only because
 * src/tuya_cloud_service/netmgr is on LIB_PUBLIC_INC. Nothing outside this
 * module should include it.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __NETMGR_PRIV_H__
#define __NETMGR_PRIV_H__

#include "tuya_cloud_types.h"
#include "netmgr.h"
#include "netconn_registry.h"

/* For netmgr_change_reason_e, the argument of netmgr_reselect_request(); the
 * only reason netmgr_event.h is pulled in here. */
#include "netmgr_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
*********************** teardown ***************************
***********************************************************/

/* Design record for netmgr_deinit(), declared in netmgr.h next to
 * netmgr_init().
 *
 * Order matters: gate first (clear `inited`, set `stopping` under the lock,
 * so netmgr_conn_get/set() and netmgr_notify_link() start refusing work);
 * cancel the notify work item if only queued, then drain one already running
 * (bounded poll on a busy counter - a timeout logs OPRT_TIMEOUT and finishes
 * teardown anyway, so late work such as tal_net_route_set() can land after
 * deinit returned); close each registered link newest-first (the conn nodes
 * are static globals a later netmgr_init() reuses, so they are left as
 * found); stop the shared deadline timer WITHOUT deleting it
 * (__netmgr_settle() reads the handle and arms it outside the lock); zero
 * s_netmgr but restore the mutex handle and `stopping` (a straggler that saw
 * stopping == FALSE would push a route with src_ip 0).
 *
 * The mutex is RETAINED for the whole cycle: no TAL layer can withdraw a
 * callback it already installed (known_gaps.md §7, §8), so a driver may call
 * back after teardown on any platform, and a freed-then-relocked mutex is
 * worse than one kept forever. Same reasoning gives BLE its own ownership
 * flag: netmgr_init() calls tuya_ble_init(), and tuya_iot_destroy() already
 * calls tuya_ble_deinit(), so netmgr_deinit() must only call it when this
 * netmgr_init() was the one that brought BLE up.
 *
 * Contract: idempotent, safe on a partial netmgr_init() failure (so
 * netmgr_init() can call this on its own error paths); must NOT run on
 * WORKQ_SYSTEM - the drain step would wait on itself.
 */
/***********************************************************
******************* reselect request *************************
***********************************************************/

/**
 * @brief Ask netmgr to re-evaluate which link should be active.
 *
 * Lets netmgr_policy.c trigger a pass without owning netmgr's work queue:
 * netmgr_policy_set() needs a policy change to take effect at the next
 * reselect rather than the next unrelated link event, and
 * netmgr_policy_pin() needs `netmgr switch <link>` to not wait for one. Kept
 * out of netmgr_link_state_get() so that inspection API stays a pure read.
 *
 * Safe from any context, any time; only marks and posts, never runs the
 * state machine on the caller's thread. DROPPED before netmgr_init() has
 * seeded state or once netmgr_deinit() has started - netmgr_deinit() itself
 * calls netmgr_policy_pin(NETCONN_AUTO), so this must not queue work into a
 * netmgr being dismantled. Coalesces with any already-pending pass.
 *
 * @param[in] reason for netmgr_change_t.reason; only reported when the pass
 *                   actually changed something. NETMGR_CHG_REASON_NONE lets
 *                   the pass name its own cause, as the pin does.
 *
 * @return OPRT_OK when a pass is queued, already pending, or deliberately
 *         dropped. Others when the work queue refused it.
 */
OPERATE_RET netmgr_reselect_request(netmgr_change_reason_e reason);

/***********************************************************
******************* snapshot accessors *********************
***********************************************************/

/**
 * @brief Global netmgr state, copied out under the lock.
 */
typedef struct {
    /** The type mask netmgr_init() was called with. */
    netmgr_type_e configured;
    /** The link traffic currently leaves through, or NETCONN_AUTO for none. */
    netmgr_type_e active;
    /** Status of that active link. */
    netmgr_status_e status;
    /** FALSE before netmgr_init() finishes and after netmgr_deinit(). */
    BOOL_T inited;
    /** Number of links actually registered, i.e. rows netmgr_link_info_at() has. */
    uint32_t link_num;
} netmgr_state_t;

/**
 * @brief One registered link, descriptor metadata plus live state - everything
 *        the CLI needs to print a row without an `#ifdef ENABLE_<TECH>`.
 */
typedef struct {
    /** Descriptor name, e.g. "wifi". Never NULL. */
    const char   *name;
    netmgr_type_e type;
    /** Live priority from conn->pri, which a NETCONN_CMD_PRI set can have moved. */
    uint8_t pri;
    /** Live status from conn->status. */
    netmgr_status_e status;
    /** Provider from conn->provider. */
    uint8_t              provider;
    netconn_caps_t       caps;
    netconn_ctrl_level_e ctrl;
    netconn_attr_mask_t  set_mask;
    netconn_attr_mask_t  get_mask;
} netmgr_link_info_t;

/**
 * @brief Snapshot the global state.
 *
 * @param[out] state filled on OPRT_OK, untouched otherwise
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET netmgr_state_get(netmgr_state_t *state);

/**
 * @brief Snapshot one registered link by position, for iteration.
 *
 * Positions run 0 .. netmgr_state_t.link_num - 1, in REGISTRATION order -
 * index i is the link whose netmgr_link_view_t.reg_index is i - and are
 * stable for as long as nothing (un)registers, which between netmgr_init()
 * and netmgr_deinit() means always.
 *
 * @param[in]  index position in registration order
 * @param[out] info  filled on OPRT_OK, untouched otherwise
 *
 * @return OPRT_OK on success, OPRT_NOT_FOUND when @a index is past the end.
 *         Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET netmgr_link_info_at(uint32_t index, netmgr_link_info_t *info);

/**
 * @brief Snapshot one registered link by type.
 *
 * @param[in]  type a single netmgr_type_e bit
 * @param[out] info filled on OPRT_OK, untouched otherwise
 *
 * @return OPRT_OK on success, OPRT_NOT_FOUND when @a type is not registered.
 *         Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET netmgr_link_info_get(netmgr_type_e type, netmgr_link_info_t *info);

/***********************************************************
*************************** CLI ****************************
***********************************************************/

/**
 * @brief The `netmgr` console command, moved out of netmgr.c into netmgr_cli.c.
 * The symbol is unchanged, so the four `extern void netmgr_cmd(int, char **)`
 * declarations in tal_cli and the app cli_cmd.c files keep working untouched.
 *
 * @param[in] argc argument count
 * @param[in] argv argument vector
 */
void netmgr_cmd(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* __NETMGR_PRIV_H__ */
