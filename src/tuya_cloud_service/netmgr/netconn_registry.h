/**
 * @file netconn_registry.h
 * @brief Link registration contract for the network manager.
 *
 * netmgr used to learn about its links by naming them: netmgr.c carried one
 * `#ifdef ENABLE_<TECH>` block per technology, each declaring `extern
 * netmgr_conn_<tech>_t s_netmgr_<tech>` and registering it by hand. Adding a
 * link type meant editing netmgr.c; every cross-layer question netmgr had about
 * a link ("can this one host a SoftAP?", "does LAN make sense here?") was
 * answered either by another `#ifdef` or by guessing from unrelated state.
 *
 * This header replaces the naming with a declaration. Each driver publishes one
 * netconn_desc_t - name, capability bits, control level, default priority,
 * socket provider, supported attributes - pointing at the netmgr_conn_base_t
 * instance it already owns. netmgr walks a table of those descriptors and knows
 * nothing about any particular technology.
 *
 * What this header deliberately does NOT do
 * -----------------------------------------
 * It does not touch netmgr_conn_base_t or netmgr_conn_{wifi,wired,cellular}_t.
 * netmgr.h is included by 44 files in this tree and netconn_*.h are on the
 * global public include path with app code using their types, so moving
 * pri/status/next/event_cb out of the base struct is a separate, wider change.
 * The descriptor sits BESIDE the existing structs and carries only the metadata
 * netmgr needs; the base struct stays byte-for-byte as it was.
 *
 * Ownership rule between the two: at registration time netmgr copies
 * desc->default_pri into conn->pri and desc->provider into conn->card_type. The
 * descriptor is the source of truth, those two base fields become caches of it.
 * That is what lets a board retune priorities and providers without editing any
 * driver.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __NETCONN_REGISTRY_H__
#define __NETCONN_REGISTRY_H__

#include "tuya_cloud_types.h"
#include "netmgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
********************* control level ************************
***********************************************************/

/**
 * @brief How much of a link's lifecycle netmgr is allowed to drive.
 *
 * Derived strictly from what the TAL layer of each technology actually exposes,
 * not from what would be convenient. The levels are cumulative: a level always
 * permits everything the level below it permits.
 *
 * M3's policy layer must consult this before issuing an action. Asking a link to
 * do something above its level is a programming error, not a runtime fallback.
 */
typedef enum {
    /**
     * Observe only. The driver reports link state and can be configured, but no
     * TAL entry point exists to bring the link up or down.
     *
     * Wired sits here: tal_wired.h provides exactly tal_wired_get_status(),
     * tal_wired_set_status_cb(), tal_wired_{get,set}_ip() and
     * tal_wired_{get,set}_mac(). There is no connect, no disconnect, no
     * enable/disable. A policy layer can prefer or avoid this link in routing,
     * but it can never make it come up or go down.
     */
    NETCONN_CTRL_OBSERVE = 0,

    /**
     * Driver-sustained. netmgr starts the subsystem once through open() and the
     * driver keeps the link alive by itself. There is no per-attempt connect or
     * disconnect, so netmgr cannot retry, cannot back off and cannot park the
     * link to save power.
     *
     * Cellular sits here: tal_cellular_init() brings the data context up and
     * tal_cellular.h exposes no connect/disconnect pair - and no deinit either,
     * which is why netconn_cellular_close() cannot actually drop the link today.
     * Read this level as "netmgr can start it"; treat close() as best-effort
     * until the TAL grows a teardown.
     */
    NETCONN_CTRL_SUSTAINED = 1,

    /**
     * Fully managed. netmgr may associate and dissociate at will, so a policy
     * layer can retry, back off, hand the radio over or take the link down.
     *
     * WiFi sits here: tal_wifi_station_connect(),
     * tal_wifi_station_disconnect(), tal_wifi_set_work_mode(),
     * tal_wifi_ap_start()/stop(), tal_wifi_lp_{enable,disable}().
     */
    NETCONN_CTRL_MANAGED = 2,
} netconn_ctrl_level_e;

#define NETCONN_CTRL_TO_STR(lvl)                                                                                       \
    ((lvl) == NETCONN_CTRL_OBSERVE     ? "observe"                                                                     \
     : (lvl) == NETCONN_CTRL_SUSTAINED ? "sustained"                                                                   \
     : (lvl) == NETCONN_CTRL_MANAGED   ? "managed"                                                                     \
                                       : "unknown")

/***********************************************************
******************** capability bits ***********************
***********************************************************/

/**
 * @brief What a link can do, declared by its own driver.
 *
 * Every bit here exists to replace a specific guess that is in the tree today.
 * A bit is a statement about the technology and its TAL support in this build,
 * so it is a compile-time constant in the descriptor, not runtime state.
 */
typedef uint32_t netconn_caps_t;

#define NETCONN_CAP_NONE (0u)

/**
 * LAN direct connection (tuya_lan) is meaningful on this link.
 *
 * Replaces `#if !defined(ENABLE_CELLULAR) || (ENABLE_CELLULAR == 0)` around the
 * LAN timer in netmgr_init(), which compiles LAN out of the whole image as soon
 * as cellular is enabled - so a wifi+4G device loses LAN on its wifi link too.
 * Also replaces the `type & NETCONN_WIRED || type & NETCONN_WIFI` test inside
 * __tuya_lan_init_tm_cb(), which reads the CONFIGURED type mask rather than the
 * link that is actually active.
 *
 * The M3 form of both tests is one question against the active link:
 * netconn_registry_find(active)->caps & NETCONN_CAP_LAN.
 */
#define NETCONN_CAP_LAN (1u << 0)

/**
 * This link can host a SoftAP for provisioning (NETCFG_TUYA_WIFI_AP).
 *
 * Replaces the cross-layer query in __netconn_activate_token_get():
 *
 *     TAL_NETWORK_CARD_TYPE_E active_type = tal_network_card_get_active_type();
 *     if (netcfg.type & TUYA_NETMGR_NETCFG_AP && active_type != TAL_NET_TYPE_AT_MODEM)
 *
 * That test never fires. tal_network_card_get_active_type() returns
 * route.provider, whose only writers are tal_net_route_set() (fed by netmgr from
 * conn->card_type) and tal_network_card_set_active() (which has zero callers in
 * the tree). Every driver initialises card_type to TAL_NET_PROVIDER_DEFAULT,
 * which expands to TAL_NET_TYPE_POSIX or TAL_NET_TYPE_PLATFORM and never to
 * TAL_NET_TYPE_AT_MODEM - the only other mention of that constant in the tree is
 * its own #define. So the condition is a tautology and the 4G branch it guards
 * has never been taken.
 *
 * This bit makes the intended meaning explicit and, unlike the original, true:
 * the driver states whether it can raise an AP, and netmgr never asks the data
 * plane a control-plane question.
 */
#define NETCONN_CAP_NETCFG_AP (1u << 1)

/**
 * This link can be provisioned over BLE (NETCFG_TUYA_BLE).
 *
 * Set only when the build actually links a BLE netcfg backend, so the union of
 * NETCONN_CAP_NETCFG_* across the registered links answers "does this image need
 * netcfg at all" - see the netcfg gating note in the M2 design.
 */
#define NETCONN_CAP_NETCFG_BLE (1u << 2)

/** Convenience: any provisioning capability at all. */
#define NETCONN_CAP_NETCFG_ANY (NETCONN_CAP_NETCFG_AP | NETCONN_CAP_NETCFG_BLE)

/**
 * Traffic on this link is billed by volume.
 *
 * M3's selection policy uses this to keep a metered link as a fallback rather
 * than a peer: it should lose to any unmetered link that is up, and chatty
 * optional traffic (LAN discovery, OTA polling) should be held back while it is
 * the active link. Today the same intent is expressed by hardcoding cellular's
 * default priority to 0 and by compiling LAN out, neither of which survives a
 * board that wants a different order.
 */
#define NETCONN_CAP_METERED (1u << 3)

/***********************************************************
****************** attribute support mask ******************
***********************************************************/

/**
 * @brief Which netmgr_conn_config_type_e commands a link answers.
 *
 * netmgr screens against these before dispatching, so an unsupported command
 * returns OPRT_NOT_SUPPORTED from one place instead of from a `default:` arm
 * repeated in every driver.
 *
 * A bitmask needs the command values to be dense and to stay under 32.
 * netmgr_conn_config_type_e today is NETCONN_CMD_PRI(0) through
 * NETCONN_CMD_RECONN_TABLE(10): no explicit values, no gaps, 11 entries. The
 * typedef below turns a future 32nd command into a compile error instead of a
 * silently truncated mask.
 */
typedef uint32_t netconn_attr_mask_t;

#define NETCONN_ATTR_BIT(cmd) ((netconn_attr_mask_t)1u << (cmd))

typedef char netconn_attr_mask_fits_in_u32_t[(NETCONN_CMD_RECONN_TABLE < 32) ? 1 : -1];

/***********************************************************
******************** driver descriptor *********************
***********************************************************/

/**
 * @brief Everything netmgr needs to know about one link.
 *
 * Instances are `const` and live in the registry table; netmgr only reads them.
 * Mutable per-link state stays where it always was, in the netmgr_conn_base_t
 * that @ref conn points at.
 */
typedef struct netconn_desc {
    /**
     * Short lowercase name for logs and CLI output, e.g. "wifi". Replaces
     * NETMGR_TYPE_TO_STR() on every netmgr-internal log line, which is what
     * keeps a new link type from having to touch that macro.
     */
    const char *name;

    /** Which netmgr_type_e bit this link is. Must be unique in the table. */
    netmgr_type_e type;

    /** OR of NETCONN_CAP_* for this technology in this build. */
    netconn_caps_t caps;

    /** How much of the lifecycle netmgr may drive. */
    netconn_ctrl_level_e ctrl;

    /**
     * Priority this link starts at, copied into conn->pri at registration.
     * Higher wins. Keeping it here rather than in the driver's static
     * initialiser is what lets a board reorder links without patching drivers -
     * e.g. a wifi+4G board dropping cellular below wifi.
     */
    uint8_t default_pri;

    /**
     * Socket ops backend this link's traffic leaves through, copied into
     * conn->card_type at registration. Holds a TAL_NET_TYPE_* value; use
     * TAL_NET_PROVIDER_DEFAULT unless the board really has a second backend.
     *
     * Typed uint8_t, which is what TAL_NETWORK_CARD_TYPE_E is a typedef of, so
     * this control-plane header needs no include from the data plane - the same
     * discipline netmgr_conn_base_t.card_type already follows.
     */
    uint8_t provider;

    /** Commands accepted by conn->set(). See NETCONN_ATTR_BIT(). */
    netconn_attr_mask_t set_mask;

    /** Commands accepted by conn->get(). See NETCONN_ATTR_BIT(). */
    netconn_attr_mask_t get_mask;

    /**
     * The driver's existing static netmgr_conn_base_t (the `base` member of
     * netmgr_conn_{wifi,wired,cellular}_t). Never NULL in a valid row; never
     * freed, so netmgr may hold it across an unlock.
     */
    netmgr_conn_base_t *conn;
} netconn_desc_t;

/***********************************************************
*********************** the registry ***********************
***********************************************************/

/**
 * @brief Get the link table this build will register.
 *
 * Returns the board override when one was installed, otherwise the default
 * table. The default table lives in netconn_table.c - the one file in the module
 * that is allowed to carry `#ifdef ENABLE_<TECH>` - so that netmgr.c has none.
 *
 * @param[out] count number of rows; set to 0 when the return value is NULL
 *
 * @return the table, or NULL when this build registered no links at all
 */
const netconn_desc_t *netconn_registry_get_table(uint32_t *count);

/**
 * @brief Install a board-specific link table, replacing the default one.
 *
 * The board owns the storage and it must outlive netmgr - a `static const`
 * array in the board's own translation unit. Nothing is copied.
 *
 * Call it from board_register_hardware(), which every app runs before
 * netmgr_init(). Calling it after netmgr_init() has already registered the links
 * is rejected with OPRT_COM_ERROR rather than silently ignored.
 *
 * Chosen over a weak-symbol hook on purpose: the override would live in a board
 * static library, and a weak default only loses to a strong definition in an
 * archive member the linker already had a reason to pull in - which is exactly
 * the failure that silently reverts a board to defaults. An explicit call cannot
 * fail that way. Chosen over a linker section for the reason recorded in the M2
 * design: the section would have to be added to five vendor link scripts.
 *
 * @param[in] table rows to register, in the order they should be registered
 * @param[in] count number of rows
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET netconn_registry_set_table(const netconn_desc_t *table, uint32_t count);

/**
 * @brief Find the descriptor for one link type in the active table.
 *
 * The lookup netmgr, the CLI and M3's policy layer use to answer capability
 * questions about a link.
 *
 * @param[in] type a single netmgr_type_e bit; NETCONN_AUTO is not a link and
 *                 yields NULL
 *
 * @return the descriptor, or NULL when this build has no such link
 */
const netconn_desc_t *netconn_registry_find(netmgr_type_e type);

/***********************************************************
******************** reporting channel *********************
***********************************************************/

/**
 * @brief Report a link state change to netmgr, from any context.
 *
 * This replaces the netmgr_conn_base_t.event_cb function pointer as the way a
 * driver tells netmgr something happened. The difference is where the work runs.
 *
 * event_cb was a synchronous call, so the whole netmgr state machine - reselect
 * the active link, push the route down to the data plane, publish
 * EVENT_LINK_TYPE_CHG / EVENT_LINK_STATUS_CHG - executed on whatever context
 * raised the event: a vendor WiFi task, a modem status callback, the caller of
 * netmgr_conn_set(NETCONN_CMD_PRI), or netmgr_init() itself by way of the LINUX
 * tkl_wired_set_status_cb() firing before it returns. Every one of those was a
 * re-entry into netmgr, and re-entrancy is the single reason the locking
 * contract at the top of netmgr.c is as delicate as it is.
 *
 * netmgr_notify_link() only records the report and posts one work item to
 * WORKQ_SYSTEM. The state machine then runs in exactly one context, the
 * WORKQ_SYSTEM thread, serialised with itself by construction:
 *
 *   - it never re-enters from a driver, so nothing can self-deadlock on
 *     s_netmgr.lock;
 *   - it may block, which the state machine needs - pushing the route reads
 *     conn->get(NETCONN_CMD_IP), a blocking AT exchange on cellular. That is
 *     also why this cannot move to WORKQ_HIGHTPRI, which is documented as "block
 *     operations are not allowed";
 *   - it costs no extra stack: WORKQ_SYSTEM already exists (every app calls
 *     tal_workq_init() before netmgr_init(), and that call is idempotent), which
 *     is why no new thread is created.
 *
 * Reports coalesce. At most one work item is queued at a time; a second report
 * arriving before the handler runs is absorbed into the same pass. That is
 * lossless, because the handler never trusted the reported value in the first
 * place: __netmgr_event_cb() opens with `(void)status` and re-reads every link
 * through conn->get(NETCONN_CMD_STATUS). One consequence is worth knowing: a
 * link that flaps down-then-up faster than the handler runs is observed once, at
 * its settled state, and subscribers do not see the transient.
 *
 * @a status is advisory - it is logged, and it is what makes a trace readable -
 * but selection is always recomputed from the drivers.
 *
 * Safe to call at any time, including from inside netmgr_init(). A report is
 * DROPPED only when there is nowhere to record it: before netmgr_init() has
 * seeded the state, once netmgr_deinit() has started, or for a link that is not
 * registered.
 *
 * A report raised while netmgr_init() is still running is NOT dropped - it is
 * recorded and acted on. That matters, and it is not a detail: the LINUX
 * tal_wired_set_status_cb() fires the status callback before it returns, i.e.
 * from inside netmgr_init(), and that first report is what puts the wired link's
 * address into the route. Dropping it would leave s_netmgr.status at
 * NETMGR_LINK_DOWN for the rest of init, so init's own route seeding would push
 * src_ip = 0 - worse than the synchronous behaviour this replaced. The handler
 * is written for it: it reads conn->get(NETCONN_CMD_IP) directly rather than
 * through netmgr_conn_get(), which would answer OPRT_RESOURCE_NOT_READY that
 * early.
 *
 * @param[in] type   the reporting link, a single netmgr_type_e bit
 * @param[in] status the state the driver just moved to, advisory
 *
 * @return OPRT_OK when the report was accepted or already pending. A failure
 *         means the work could not be queued; the pending mark stays set so the
 *         next report retries. Drivers should log and continue - there is no
 *         driver-side recovery for this and never was.
 */
OPERATE_RET netmgr_notify_link(netmgr_type_e type, netmgr_status_e status);

#ifdef __cplusplus
}
#endif

#endif /* __NETCONN_REGISTRY_H__ */
