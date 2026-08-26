/**
 * @file netconn_registry.h
 * @brief Link registration contract for the network manager.
 *
 * Each driver publishes one netconn_desc_t - name, capability bits, control
 * level, default priority, socket provider, supported attributes - pointing
 * at the netmgr_conn_base_t instance it already owns. netmgr walks that
 * table knowing nothing about any particular technology, so a board adds a
 * link type by adding a row, not by editing netmgr.c; the descriptor sits
 * BESIDE netmgr_conn_base_t (on the public include path) rather than
 * replacing its fields.
 *
 * Ownership rule: at registration netmgr copies desc->default_pri into
 * conn->pri and desc->provider into conn->provider, so the descriptor is the
 * source of truth and a board retunes either without touching any driver.
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
 * Derived from what each technology's TAL layer exposes, not from what would
 * be convenient; levels are cumulative. The policy layer must consult this
 * before acting - asking a link to do something above its level is a
 * programming error, not a runtime fallback.
 */
typedef enum {
    /** Observe only: no TAL entry point brings the link up or down. Wired -
     *  tal_wired.h has status/config getters and setters, no
     *  connect/disconnect. */
    NETCONN_CTRL_OBSERVE = 0,

    /** Driver-sustained: netmgr starts it once via open() and the driver
     *  keeps it alive; no per-attempt connect/disconnect, so no retry, back
     *  off or power-park. Cellular - tal_cellular_init() has no disconnect
     *  or deinit, so close() is best-effort until the TAL grows one. */
    NETCONN_CTRL_SUSTAINED = 1,

    /** Fully managed: netmgr may associate/dissociate at will. WiFi -
     *  tal_wifi_station_{connect,disconnect}(), tal_wifi_set_work_mode(),
     *  tal_wifi_ap_{start,stop}(), tal_wifi_lp_{enable,disable}(). */
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
 * A bit is a statement about the technology and its TAL support in THIS
 * build - a compile-time constant in the descriptor, not runtime state.
 */
typedef uint32_t netconn_caps_t;

#define NETCONN_CAP_NONE (0u)

/** LAN direct connection (tuya_lan) is meaningful on this link - the only
 *  question netmgr asks is netconn_registry_find(active)->caps &
 *  NETCONN_CAP_LAN. */
#define NETCONN_CAP_LAN (1u << 0)

/**
 * This link can host a SoftAP for provisioning (NETCFG_TUYA_WIFI_AP).
 *
 * Replaces a control-plane-asks-data-plane-a-question bug:
 * __netconn_activate_token_get() used to gate SoftAP eligibility on
 * `tal_network_card_get_active_type() != TAL_NET_PROVIDER_AT_MODEM`, but
 * every driver initialises provider to POSIX or TKL and nothing in the tree
 * ever assigns AT_MODEM to route.provider - the condition was a tautology
 * and the 4G branch it guarded was never taken. This bit is the driver
 * stating directly whether it can raise an AP.
 */
#define NETCONN_CAP_NETCFG_AP (1u << 1)

/** This link can be provisioned over BLE (NETCFG_TUYA_BLE); set only when
 *  the build actually links a BLE netcfg backend. */
#define NETCONN_CAP_NETCFG_BLE (1u << 2)

/** Convenience: any provisioning capability at all. */
#define NETCONN_CAP_NETCFG_ANY (NETCONN_CAP_NETCFG_AP | NETCONN_CAP_NETCFG_BLE)

/**
 * Traffic on this link is billed by volume. DECLARED AND SURFACED, NOT
 * ACTED ON: the built-in ranking never reads `caps`; it reaches a product
 * ranking hook via netmgr_link_view_t.caps and the CLI. Intended use: rank
 * a metered link below any unmetered up link, and hold back chatty optional
 * traffic (LAN discovery, OTA polling) while it is active.
 */
#define NETCONN_CAP_METERED (1u << 3)

/***********************************************************
****************** attribute support mask ******************
***********************************************************/

/**
 * @brief Which netmgr_conn_config_type_e commands a link answers.
 *
 * netmgr screens against these before dispatching, so an unsupported command
 * returns OPRT_NOT_SUPPORTED from one place rather than a `default:` arm
 * repeated per driver. Command values must stay dense and under 32; the
 * typedef below turns a 32nd command into a compile error, not a silently
 * truncated mask.
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
 * Instances are `const` and live in the registry table; netmgr only reads
 * them. Mutable per-link state stays in the netmgr_conn_base_t @ref conn
 * points at.
 */
typedef struct netconn_desc {
    /** Short lowercase name for logs and CLI, e.g. "wifi" - names a
     *  REGISTERED link everywhere in the module. NETMGR_TYPE_TO_STR() still
     *  covers the two places that name a type NOT in the registry. */
    const char *name;

    /** Which netmgr_type_e bit this link is. Must be unique in the table. */
    netmgr_type_e type;

    /** OR of NETCONN_CAP_* for this technology in this build. */
    netconn_caps_t caps;

    /** How much of the lifecycle netmgr may drive. */
    netconn_ctrl_level_e ctrl;

    /**
     * Priority this link starts at, copied into conn->pri at registration.
     * Higher wins. Kept here rather than in the driver's static initialiser
     * so a board can reorder links (e.g. drop cellular below wifi) without
     * patching drivers.
     */
    uint8_t default_pri;

    /**
     * Socket ops backend this link's traffic leaves through, copied into
     * conn->provider at registration; a TAL_NET_PROVIDER_* value, typed
     * uint8_t (what tal_net_provider_id_t is a typedef of) so this
     * control-plane header needs no data-plane include.
     *
     * USE TAL_NET_PROVIDER_DEFAULT: the backend table has exactly one
     * non-NULL entry, so naming any other provider publishes a route
     * tal_net_route_set() now refuses rather than silently breaking sockets.
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
 * Returns the board override when installed, else the default table in
 * netconn_table.c - the one file allowed `#ifdef ENABLE_<TECH>`, so netmgr.c
 * has none.
 *
 * @param[out] count number of rows; set to 0 when the return value is NULL
 *
 * @return the table, or NULL when this build registered no links at all
 */
const netconn_desc_t *netconn_registry_get_table(uint32_t *count);

/**
 * @brief Install a board-specific link table, replacing the default one.
 *
 * The board owns the storage - a `static const` array that must outlive
 * netmgr. Call from board_register_hardware(), before netmgr_init(); called
 * later it is rejected with OPRT_COM_ERROR rather than silently ignored,
 * chosen over a weak-symbol hook for the same reason - a weak default can
 * silently lose to whichever strong definition the linker pulls in first.
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
 * What netmgr_conn_base_t.event_cb resolves to, via a one-line shim in
 * netmgr.c - a new driver should call this directly rather than copy the
 * event_cb idiom.
 *
 * Only records the report and posts one coalesced work item to WORKQ_SYSTEM,
 * so the state machine runs in exactly one, non-reentrant context that is
 * free to block (pushing the route reads conn->get(NETCONN_CMD_IP), a
 * blocking AT exchange on cellular). The handler re-reads every link rather
 * than trust the reported value, so a link flapping faster than it runs is
 * observed once, at its settled state; @a status is advisory only.
 *
 * Safe at any time, including inside netmgr_init() - not dropped there:
 * LINUX's tal_wired_set_status_cb() fires before it returns, and that first
 * report seeds the wired link's address into the route. DROPPED, returning
 * OPRT_OK, only when there is nowhere to record it: before netmgr_init() has
 * seeded state, once netmgr_deinit() has started, or for an unregistered
 * link.
 *
 * @param[in] type   the reporting link, a single netmgr_type_e bit
 * @param[in] status the state the driver just moved to, advisory
 *
 * @return OPRT_OK when the report was accepted or already pending. A
 *         failure means the work could not be queued; the pending mark
 *         stays set so the next report retries.
 */
OPERATE_RET netmgr_notify_link(netmgr_type_e type, netmgr_status_e status);

#ifdef __cplusplus
}
#endif

#endif /* __NETCONN_REGISTRY_H__ */
