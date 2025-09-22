/**
 * @file netconn_wifi.h
 * @brief Header file for WiFi connection management in Tuya devices.
 *
 * This header file defines the interfaces and data structures for managing WiFi
 * connections on Tuya devices. It includes the definition of network
 * configuration modes, WiFi connection events, and functions for initializing
 * the WiFi module, connecting to a network, and handling connection events.
 *
 * The file is integral to the Tuya IoT SDK, facilitating the management of WiFi
 * connections, including the configuration of network parameters and handling
 * of dynamic network events to ensure stable and reliable device connectivity.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __NETCONN_WIFI_H___
#define __NETCONN_WIFI_H___

#include "tuya_cloud_types.h"
#include "netmgr.h"
#include "netcfg.h"
#include "tal_wifi.h"
#include "tal_semaphore.h"
#include "tuya_iot.h"
#include "tuya_cloud_com_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief wifi network configure type
 *
 */
typedef enum {
    TUYA_NETMGR_NETCFG_AP = 1 << 0,  // configure wifi according ble
    TUYA_NETMGR_NETCFG_BLE = 1 << 1, // configure wifi according wifi ap
} netmgr_netcfg_mode_t;

/**
 * @brief wifi connection event
 *
 */
typedef enum {
    NETCONN_NETCFG_START = 1,
    NETCONN_NETCFG_DONE,
    NETCONN_NETCFG_TIMEOUT,
    NETCONN_WIFI_CONN_START,
    NETCONN_WIFI_CONN,
    NETCONN_WIFI_CONN_FAILED,
    NETCONN_WIFI_DISCONN,
} netconn_wifi_event_e;

/**
 * @brief wifi reconnection status
 *
 */
typedef enum {
    NETCONN_WIFI_CONN_REDAY,
    NETCONN_WIFI_CONN_CHECK,
    NETCONN_WIFI_CONN_LINKUP,
    NETCONN_WIFI_CONN_WAIT,
    NETCONN_WIFI_CONN_STOP,
} netconn_wifi_conn_status_e;

/**
 * @brief wifi connection reconnect configure
 *
 */
#define NETCONN_WIFI_CONN_TABLE 6
#define WIFI_CONN_TIMEOUT_MAX   20
typedef struct {
    char ssid[WIFI_SSID_LEN + 1];   // wifi ap ssid
    char pswd[WIFI_PASSWD_LEN + 1]; // wifi passwd
} netconn_wifi_info_t;

/**
 * @brief wifi connect/reconnect configure
 *
 */
typedef struct {
    netconn_wifi_conn_status_e stat;
    uint32_t count;
    uint32_t table_size;
    uint32_t table[NETCONN_WIFI_CONN_TABLE];
    TIMER_ID timer;
    netconn_wifi_info_t wifi_conn_info; // the connected wifi info
} netconn_wifi_conn_t;

/**
 * @brief network connection wifi configure
 *
 */
typedef struct {
    netmgr_conn_base_t base; // connection base, keep first

    char ccode[COUNTRY_CODE_LEN + 1];

    netcfg_args_t netcfg;     // wifi netcfg configure
    netconn_wifi_conn_t conn; // wifi connect/reconnect configure
} netmgr_conn_wifi_t;

/**
 * @brief open a wifi connection
 *
 * @param config
 * @return OPERATE_RET
 */
OPERATE_RET netconn_wifi_open(void *config);

/**
 * @brief close a wifi connection
 *
 * @param config
 * @return netconn_wifi_config_t*
 */
OPERATE_RET netconn_wifi_close(void);

/**
 * @brief update wifi connection
 *
 * @param config: the new config
 * @return OPERATE_RET
 */
OPERATE_RET netconn_wifi_set(netmgr_conn_config_type_e cmd, void *param);

/**
 * @brief get wifi connection attribte
 *
 * @param type
 * @param cmd
 * @param param
 * @return OPERATE_RET
 */
OPERATE_RET netconn_wifi_get(netmgr_conn_config_type_e cmd, void *param);

#ifdef __cplusplus
}
#endif

#endif
