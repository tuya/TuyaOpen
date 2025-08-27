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

#include "tal_network_register.h"

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
    ((status) == NETMGR_LINK_DOWN       ? "link_down"                                                                  \
     : (status) == NETMGR_LINK_UP       ? "link_up"                                                                    \
     : (status) == NETMGR_LINK_UP_SWITH ? "link_up_switch"                                                             \
                                        : "unknown")

typedef enum {
    NETMGR_LINK_DOWN,     // network was disconnected
    NETMGR_LINK_UP,       // network was connected
    NETMGR_LINK_UP_SWITH, // network was connected but connection changed
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
} netmgr_conn_config_type_e;

/**
 * @brief the device network config
 *
 */
typedef struct netmgr_conn_base {
    uint8_t pri;
    netmgr_type_e type;
    netmgr_status_e status;
    TAL_NETWORK_CARD_TYPE_E card_type;

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
 * @param config all network connections
 * @return OPERATE_RET
 */
OPERATE_RET netmgr_init(netmgr_type_e type);

/**
 * @brief set network connection attribute
 *
 * @param type connection type
 * @param cmd attribute type
 * @param param input attribute
 * @return OPERATE_RET
 */
OPERATE_RET netmgr_conn_get(netmgr_type_e type, netmgr_conn_config_type_e cmd, void *param);

/**
 * @brief get network connection attribute
 *
 * @param type connection type
 * @param cmd attribute type
 * @param param output attribute
 * @return OPERATE_RET
 */
OPERATE_RET netmgr_conn_set(netmgr_type_e type, netmgr_conn_config_type_e cmd, void *param);

#ifdef __cplusplus
}
#endif

#endif