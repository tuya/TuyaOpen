/**
 * @file netmgr_priv.h
 * @brief Module-internal contract between netmgr.c and its satellites.
 *
 * netmgr.c is the state machine. netmgr_cli.c (new in M2) is the CLI front end
 * and lives in a separate translation unit, so it cannot reach `s_netmgr`, which
 * is static and stays static. This header is the narrow, read-only window it
 * gets instead: two snapshot accessors that take s_netmgr.lock, copy out, and
 * release it, so no caller ever holds the lock while formatting output.
 *
 * Not a public API. It is reachable from anywhere only because
 * src/tuya_cloud_service/netmgr is on LIB_PUBLIC_INC; nothing outside this
 * module should include it.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __NETMGR_PRIV_H__
#define __NETMGR_PRIV_H__

#include "tuya_cloud_types.h"
#include "netmgr.h"
#include "netconn_registry.h"

/* For netmgr_change_reason_e, the argument of netmgr_reselect_request(). This is
 * the only reason netmgr_event.h is pulled in here; nothing else in this header
 * needs it. */
#include "netmgr_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
*********************** teardown ***************************
***********************************************************/

/* Design record for netmgr_deinit(), which is DECLARED IN netmgr.h
 * ================================================================
 * Not a doc comment: it documents no declaration in this file, so it is opened as
 * a plain block rather than a Doxygen one, which would bind it to the next typedef.
 *
 * The declaration lives in netmgr.h next to netmgr_init(), where a public
 * teardown API belongs; it is deliberately not repeated here. What stays here is
 * the reasoning, which was written as one reviewable block before the code
 * existed and is worth more next to the module internals than inlined in a
 * header 44 files include.
 *
 * Symmetry with netmgr_init(), item by item:
 *
 *   init                                   deinit
 *   ----------------------------------     ----------------------------------
 *   tal_network_card_init()                nothing. The route lock has no
 *                                          teardown entry point and the data
 *                                          plane outlives netmgr. Documented
 *                                          gap, not fixed by M2.
 *   tal_mutex_create_init(&lock)           nothing - the mutex is RETAINED and
 *                                          reused by the next netmgr_init().
 *                                          Releasing it cannot be made safe:
 *                                          drivers read base.event_cb with no
 *                                          lock and no TAL layer can withdraw an
 *                                          installed callback, so every guard is
 *                                          "test a flag, then take the lock" and
 *                                          only narrows the window. One retained
 *                                          mutex per process beats a freed one
 *                                          being locked. See netmgr.c.
 *   register each table row, each          conn->close() then unlink, in
 *     ending in conn->open()                 reverse registration order
 *   tal_sw_timer_create/start (LAN)        stop + tal_sw_timer_delete, handle
 *                                            set back to NULL
 *   tuya_ble_init()                        see the BLE note below
 *   (new in M2) the notify work item       tal_workq_cancel() then drain
 *
 * Order matters, and this is the order:
 *
 *   1. under the lock: clear `inited` and set a `stopping` flag, so
 *      netmgr_conn_get/set() and netmgr_notify_link() start refusing work;
 *   2. tal_workq_cancel(WORKQ_SYSTEM, <notify handler>, NULL) to drop an item
 *      that is queued but has not started;
 *   3. drain: wait for a handler that is already running to finish, so it never
 *      observes a half-dismantled s_netmgr. The handler raises a busy counter
 *      under the lock on entry and lowers it on exit; deinit polls that counter
 *      with tal_system_sleep() under a bounded timeout. Because the mutex is
 *      retained, a timeout is no longer a use-after-free: log an error, return
 *      OPRT_TIMEOUT and finish the teardown. The consequence to state in that
 *      log is that work the late handler had already started - a
 *      tal_net_route_set() in particular - can land after deinit returned;
 *   4. for each registered link, newest first: conn->close(), then
 *      conn->event_cb = NULL, unlink it, conn->next = NULL,
 *      conn->status = NETMGR_LINK_DOWN - the conn nodes are static globals that
 *      a later netmgr_init() will reuse, so they must be left as they were
 *      found;
 *   5. stop and delete the LAN timer;
 *   6. zero s_netmgr, restoring the two fields that must survive it: the mutex
 *      handle, and `stopping` - a straggling handler that found stopping ==
 *      FALSE would walk an empty connection list and push a route with src_ip 0.
 *
 * Per-driver duties inside conn->close(), which today are all missing - every
 * one of these is a leak across an init/deinit cycle:
 *   - wifi:     tal_event_unsubscribe(EVENT_RESET, "wifi", ...) and
 *               (EVENT_LINK_ACTIVATE, "wifi", ...), tal_sw_timer_delete() on
 *               conn.timer, netcfg_stop(NETCFG_STOP_ALL_CFG_MODULE),
 *               tal_wifi_station_disconnect().
 *   - wired:    nothing to unsubscribe, and nothing portable to clear. Do NOT
 *               "clear the status callback with NULL": no TAL or TKL contract
 *               says what NULL means, and the four tkl_wired.c on disk disagree.
 *               T5AI and T3 assign and return OPRT_OK with no thread involved,
 *               so NULL is a clean withdrawal there. LINUX puts the body under
 *               `if (cb)`, so NULL is ignored without even clearing the pointer.
 *               The porting template has no guard, so NULL is stored and a
 *               thread is spawned that then calls it. See netconn_wired_close()
 *               for the full table. A common-tree driver cannot tell which it is
 *               linked against, so it assumes the worst.
 *   - cellular: nothing available - tal_cellular.h has no deinit. close() stays
 *               a documented no-op, which is exactly what
 *               NETCONN_CTRL_SUSTAINED is telling the caller.
 *
 * KNOWN LIMITATION, deinit then init again: netmgr cannot retract a callback it
 * installed, so after netmgr_deinit() a platform may still invoke whatever
 * base.event_cb holds.
 *
 * Which platform, precisely, because the previous version of this note named the
 * wrong one. It said "the wired poller outlives netmgr" and called that "the
 * concrete use-after-free this design note exists to answer". On T5AI - the
 * primary target - there IS no wired poller: tkl_wired_set_status_cb() assigns
 * and returns, and passing NULL would withdraw the callback cleanly if the driver
 * were allowed to try. The wired poller exists only on the LINUX adapter, where
 * it also cannot be stopped.
 *
 * The DECISION to retain the mutex survives that correction, on an example that
 * holds everywhere: tal_wifi.h has no uninit at all (see netmgr.c, where the
 * close duties are listed), so a vendor WiFi task can call back after teardown on
 * every platform. Anyone re-deriving this from the old wired example would have
 * concluded the retention was unnecessary, which is why the correction is
 * recorded rather than swapped in silently.
 *
 * An earlier version of this note said something stronger and wrong: that "on
 * LINUX every netmgr_init() leaks one polling thread", so an init/deinit/init
 * cycle ends with two pollers. It does not, on the platform that ships -
 * platform/LINUX/tuyaos_adapter/src/tkl_wired.c guards the create with
 * `if (!wired_event_thread)`, so there is exactly one for the life of the
 * process however many times netmgr is re-initialised. The claim came from
 * reading the porting template instead of the adapter; the correction is kept
 * rather than dropped because the wrong version is the more alarming one and
 * would send someone hunting a leak that is not there.
 *
 * Either way it cannot be fixed in the driver - tal_wired.h is six functions,
 * all status/config, no uninit - it needs a TKL entry point to withdraw a
 * callback.
 *
 * This used to end "nothing in the tree calls netmgr_deinit() today, so nothing
 * is blocked by it". That stopped being true in the same series that wrote it:
 * netmgr_cli.c offers `netmgr deinit`, and netmgr_init() calls deinit on its own
 * error paths. So the window is reachable from the serial console on any build
 * with the CLI, which is every debug build. It is still not FATAL - that is what
 * the retained mutex and the gate are for - but it is exercised, not theoretical.
 *
 * A related consequence for the report path: because no TAL layer can withdraw
 * a callback it installed (tal_wifi.h has no uninit for the WIFI_EVENT_CB
 * either), the report shim must stay safe to enter at any time after
 * netmgr_deinit(). Setting conn->event_cb = NULL is not sufficient - drivers
 * read that pointer with no lock - so netmgr_notify_link() gates on a flag that
 * netmgr_deinit() sets before it touches anything and only netmgr_init() clears.
 *
 * BLE: netmgr_init() calls tuya_ble_init() and tuya_iot_destroy() already calls
 * tuya_ble_deinit(). netmgr_deinit() must therefore call tuya_ble_deinit() only
 * when its own netmgr_init() was the one that brought BLE up, tracked with an
 * ownership flag, and M2 must not remove tuya_iot's call. That netmgr owns the
 * BLE stack at all is a layering problem; recording it here, fixing it in a
 * separate PR.
 *
 * Contract for callers:
 *   - idempotent, and safe when netmgr_init() never ran or failed part way,
 *     which is what lets netmgr_init() call it on its own error paths. Before
 *     M2 those paths returned with the mutex created and links registered when
 *     no link came up; they call netmgr_deinit() now;
 *   - must NOT be called from the WORKQ_SYSTEM thread: step 3 would wait on
 *     itself. That rules out calling it from an EVENT_LINK_* subscriber.
 *
 * Returns OPRT_OK on success, including when there was nothing to tear down, and
 * OPRT_TIMEOUT when the drain did not complete - in which case it still tears
 * down everything it safely can. See netmgr.h for the caller-facing contract.
 */
/***********************************************************
********************* reselect request *********************
***********************************************************/

/**
 * @brief Ask netmgr to re-evaluate which link should be active.
 *
 * The seam that lets a satellite module trigger a pass without owning netmgr's
 * work queue. netmgr_policy.c is the only caller, and it needs it twice:
 *
 *   - netmgr_policy_set(). netmgr_policy.h promises "a change takes effect at the
 *     next reselect, which netmgr schedules immediately", and netmgr_policy.c has
 *     no access to the work queue, so without this the promise was simply not
 *     implemented - a new policy took effect at the next unrelated link event;
 *   - netmgr_policy_pin(). A pin that only took effect at the next link event
 *     makes `netmgr switch <link>` read as a broken command.
 *
 * WHY THIS EXISTS RATHER THAN A POST INSIDE netmgr_link_state_get(). That was the
 * first shape and it was wrong: it made a read - an inspection API, documented
 * "for the CLI and for diagnostics" - carry a side effect, so every CLI dump
 * queued a pass and a caller had no way to ask the question without also asking
 * for the action. Splitting them means netmgr_link_state_get() is a pure read
 * again and the request is explicit at the two call sites that actually want it.
 *
 * Declared here rather than in netmgr_policy.h or netmgr.h: it is a
 * module-internal contract between netmgr.c and its satellites, exactly what this
 * header is for, and it has no business in a header 44 files include.
 *
 * Safe from any context and at any time. Like netmgr_notify_link() it only marks
 * and posts - it never runs the state machine on the caller's thread - and it is
 * DROPPED, returning OPRT_OK, before netmgr_init() has seeded the state or once
 * netmgr_deinit() has started. That last case matters concretely: netmgr_deinit()
 * calls netmgr_policy_pin(NETCONN_AUTO) to release the pin, and the gate closed in
 * step 0 of teardown is what keeps that from queueing work into a netmgr that is
 * being dismantled.
 *
 * The pass coalesces with any already-pending one, so N requests in a row cost at
 * most one pass.
 *
 * @param[in] reason why, for netmgr_change_t.reason. Folded by the same rank
 *                   order as everything the pass observes for itself, so a
 *                   request cannot mask a real link event that lands in the same
 *                   pass, and it is only reported when the pass actually changed
 *                   something. Pass NETMGR_CHG_REASON_NONE for "just
 *                   re-evaluate, the pass will name its own cause" - which is
 *                   what the pin does, because the pass detects a moved pin by
 *                   comparison and names PINNED or UNPINNED with the right
 *                   subject itself.
 *
 * @return OPRT_OK when a pass is queued, when one already was, or when the
 *         request was deliberately dropped. Others when the work queue refused
 *         it.
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
 * @brief One registered link, descriptor metadata plus live state.
 *
 * Everything the CLI needs to print a row, so netmgr_cli.c needs no `#ifdef
 * ENABLE_<TECH>` for the dump path. That also fixes the current dump, which
 * hand-codes a wifi block and a wired block and therefore never prints cellular
 * at all.
 */
typedef struct {
    /** Descriptor name, e.g. "wifi". Never NULL. */
    const char   *name;
    netmgr_type_e type;
    /** Live priority from conn->pri, which a NETCONN_CMD_PRI set can have moved. */
    uint8_t pri;
    /** Live status from conn->status. */
    netmgr_status_e status;
    /** Provider from conn->card_type. */
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
 * Positions run 0 .. netmgr_state_t.link_num - 1 and follow REGISTRATION order:
 * index 0 is the first link registered, i.e. the first row of netconn_table.c
 * that this build's type mask selected, and index i is the link whose
 * netmgr_link_view_t.reg_index is i.
 *
 * It used to say "selection order: index 0 is the link __get_active_conn()
 * considers first", and both halves of that are now wrong. That function is gone,
 * and selection is recomputed from a snapshot on every pass, so there is no fixed
 * selection order left to expose - which is the entire point of M3: an order
 * baked into the container is what stopped NETCONN_CMD_PRI from working.
 *
 * Positions are still stable for as long as nothing registers or unregisters,
 * which between netmgr_init() and netmgr_deinit() means always.
 *
 * @param[in]  index position in selection order
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
 *
 * The symbol does not change, so the four `extern void netmgr_cmd(int, char **)`
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
