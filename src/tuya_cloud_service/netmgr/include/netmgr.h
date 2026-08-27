/**
 * @file netmgr.h
 * @brief Header file for the network manager module in Tuya devices.
 *
 * This header file defines the interfaces and data structures for the network
 * manager module, which is responsible for managing the network connections of
 * Tuya devices. It includes definitions for network connection types (WiFi,
 * Wired, Auto), and network link events to handle network connectivity changes.
 *
 * The network manager plays a crucial role in ensuring stable and reliable
 * network connectivity for Tuya devices, facilitating seamless communication
 * with Tuya cloud services and supporting device control and data exchange.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * 2025-07-11   yangjie     Add types to string conversion macros
 *
 */

#ifndef __NETMGR_H___
#define __NETMGR_H___

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief network connection type
 *
 */
#define NETMGR_TYPE_TO_STR(type)                                                                                       \
    ((type) == NETCONN_WIFI       ? "wifi"                                                                             \
     : (type) == NETCONN_WIRED    ? "wired"                                                                            \
     : (type) == NETCONN_CELLULAR ? "cellular"                                                                         \
     : (type) == NETCONN_AUTO     ? "auto"                                                                             \
                                  : "unknown")

typedef enum {
    NETCONN_AUTO = 1 << 0,
    NETCONN_WIFI = 1 << 1,
    NETCONN_WIRED = 1 << 2,
    NETCONN_CELLULAR = 1 << 3,
} netmgr_type_e;

/**
 * @brief the network link event
 *
 */
#define NETMGR_STATUS_TO_STR(status)                                                                                   \
    ((status) == NETMGR_LINK_DOWN        ? "link_down"                                                                 \
     : (status) == NETMGR_LINK_UP        ? "link_up"                                                                   \
     : (status) == NETMGR_LINK_UP_SWITCH ? "link_up_switch"                                                            \
                                         : "unknown")

typedef enum {
    NETMGR_LINK_DOWN,      // network was disconnected
    NETMGR_LINK_UP,        // network was connected
    NETMGR_LINK_UP_SWITCH, // network was connected but connection changed
} netmgr_status_e;

typedef enum {
    NETCONN_CMD_PRI,           // int
    NETCONN_CMD_IP,            // NW_IP_S
    NETCONN_CMD_MAC,           // NW_MAC_S
    NETCONN_CMD_STATUS,        // netmgr_type_e
    NETCONN_CMD_SSID_PSWD,     // netconn_wifi_info_t
    NETCONN_CMD_COUNTRYCODE,   // "US"/"CN"/"EU"/"JP"
    NETCONN_CMD_NETCFG,        // netconn_wifi_netcfg_t
    NETCONN_CMD_SET_STATUS_CB, // user define status callback instead of the
                               // default
    NETCONN_CMD_CLOSE,         // close network connection
    NETCONN_CMD_RESET,         // close network connection
    NETCONN_CMD_RECONN_TABLE,  // netmgr_reconn_table_t: reconnect back-off table (seconds)
} netmgr_conn_config_type_e;

/**
 * @brief reconnect back-off table (seconds between retries), grows to the last
 * entry and then repeats it. Used to configure a long back-off for low-power
 * scenarios so a device stays out of RF for longer while the network is down.
 */
typedef struct {
    uint32_t *table; // array of intervals in seconds
    uint32_t size;   // number of valid entries in table
} netmgr_reconn_table_t;

/**
 * @brief the device network config
 *
 */
typedef struct netmgr_conn_base {
    uint8_t pri;
    netmgr_type_e type;
    netmgr_status_e status;
    /* Which tal_network socket backend this connection's traffic leaves through.
     * Holds one of the TAL_NET_PROVIDER_* values the tal_network provider registry
     * defines - use TAL_NET_PROVIDER_DEFAULT to fill it in. Typed uint8_t, the
     * very type tal_net_provider_id_t is a typedef of, so this control-plane
     * header needs no include from the data plane. */
    uint8_t provider;

    OPERATE_RET (*open)(void *config);
    OPERATE_RET (*close)(void);
    OPERATE_RET (*set)(netmgr_conn_config_type_e cmd, void *param);
    OPERATE_RET (*get)(netmgr_conn_config_type_e cmd, void *param);
    void (*event_cb)(netmgr_type_e type, netmgr_status_e event);

    struct netmgr_conn_base *next; // for linked list
} netmgr_conn_base_t;

/**
 * @brief network manage init
 *
 * Registers every link the @a type mask selects, opens each one, starts the
 * reachability backend and installs the initial route.
 *
 * Idempotent: calling it on an already-initialised netmgr logs a warning and
 * answers OPRT_OK without touching anything. To change the type mask, call
 * netmgr_deinit() first - a second netmgr_init() cannot widen it.
 *
 * STACK DEPTH. This is the half of the contract that netmgr_deinit()'s thread
 * restriction has and this call did not, and it bites in exactly the place the
 * thread rule does not cover.
 *
 * netmgr_init() runs the whole registration path ON THE CALLER'S STACK,
 * conn->open() for every link included. That reaches straight into the driver
 * stacks - tal_wifi_init() and the vendor WiFi bring-up, netcfg initialisation,
 * tal_cellular_init() and its modem AT exchange - and, on an ENABLE_BLUETOOTH
 * build, tuya_ble_init() and the BLE stack construction. Their usual caller is
 * the application's main task, which has a large stack; none of them is written
 * to be frugal.
 *
 * So call it from the app's start-up task, and not from a small worker thread.
 * The concrete counter-example, because a limit is worth naming: the serial CLI
 * thread's stack is SERIAL_CLI_STACK_SIZE, 3072 bytes by default
 * (src/tal_cli/Kconfig). netmgr_deinit() fits there; netmgr_init() does not on
 * T5AI with ENABLE_BLUETOOTH.
 *
 * `netmgr init` exists anyway, because an operator who has just run
 * `netmgr deinit` needs a way back, and it warns instead of refusing - refusing
 * would need a stack-headroom test this SDK has no way to make, and the stack it
 * would overflow is the one such a check would itself be running on. So note
 * what the limit really means: a caller can satisfy every DOCUMENTED rule - it
 * is not on the WORKQ_SYSTEM thread, netmgr is not already up - and still
 * overflow. There is no runtime check anywhere, and this comment plus that
 * warning are the only guards.
 *
 * @param type bitmask of netmgr_type_e link types to bring up
 *
 * @return OPRT_OK on success, including when netmgr was already initialised.
 *         OPRT_INVALID_PARM when no link at all could be registered.
 *         OPRT_NOT_SUPPORTED when this build contains no link driver. On every
 *         error path it rolls itself back through netmgr_deinit().
 */
OPERATE_RET netmgr_init(netmgr_type_e type);

/**
 * @brief network manage deinit, undoing netmgr_init()
 *
 * Closes and unregisters every link, stops the notify work item and the shared
 * deadline timer, stops the reachability backend, releases the manual link pin,
 * and leaves the static connection nodes as netmgr_init() found them so a later
 * init can reuse them. Idempotent, and safe when netmgr_init() never ran or
 * failed part way.
 *
 * (It said "the LAN timer" before M3. That timer was a 500 ms poll deciding
 * whether to start the LAN service; the LAN decision is event-driven now and the
 * one timer netmgr owns is the shared deadline.)
 *
 * Must NOT be called from the WORKQ_SYSTEM thread: it waits for the notify
 * handler to finish and would wait on itself. That rules out calling it from an
 * EVENT_LINK_* subscriber.
 *
 * @return OPRT_OK on success, including when there was nothing to tear down.
 *         OPRT_TIMEOUT when a running notify handler did not finish inside the
 *         drain timeout; everything that can safely be torn down still is.
 */
OPERATE_RET netmgr_deinit(void);

/**
 * @brief get network connection attribute
 *
 * @param type connection type
 * @param cmd attribute type
 * @param param output attribute
 * @return OPERATE_RET
 */
OPERATE_RET netmgr_conn_get(netmgr_type_e type, netmgr_conn_config_type_e cmd, void *param);

/**
 * @brief set network connection attribute
 *
 * @param type connection type
 * @param cmd attribute type
 * @param param input attribute
 * @return OPERATE_RET
 */
OPERATE_RET netmgr_conn_set(netmgr_type_e type, netmgr_conn_config_type_e cmd, void *param);

#ifdef __cplusplus
}
#endif

#endif