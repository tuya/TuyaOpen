/**
 * @file netmgr.c
 * @brief Network manager implementation for managing network connections on
 * Tuya devices.
 *
 * This file contains the implementation of the network manager, which is
 * responsible for managing the network connections of Tuya devices. It supports
 * multiple network interfaces including WiFi, wired Ethernet, and Bluetooth.
 * The network manager initializes the network modules, manages network
 * connection states, and switches between different network types based on
 * availability and user configuration.
 *
 * The implementation utilizes conditional compilation to include support for
 * the different network types based on the device capabilities and
 * configuration. It defines a structure for managing the state of the network
 * connections and provides functions for initializing the network manager,
 * setting the active network type, and querying the current network status.
 *
 * The network manager plays a crucial role in ensuring that Tuya devices can
 * maintain a stable and reliable connection to the Tuya cloud services,
 * facilitating device control and data exchange.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "netmgr.h"
#include "tal_api.h"
#include "tuya_slist.h"
#include "tuya_cloud_com_defs.h"
#include "tuya_error_code.h"
#include "tuya_lan.h"

#ifdef ENABLE_WIFI
#include "netconn_wifi.h"
extern netmgr_conn_wifi_t s_netmgr_wifi;
#endif

#ifdef ENABLE_WIRED
#include "netconn_wired.h"
extern netmgr_conn_wired_t s_netmgr_wired;
#endif

#ifdef ENABLE_CELLULAR
#include "netconn_cellular.h"
extern netmgr_conn_cellular_t s_netmgr_cellular;
#endif

#ifdef ENABLE_BLUETOOTH
#include "ble_mgr.h"
#endif

typedef struct {
    MUTEX_HANDLE lock; // mutex
    BOOL_T inited;

    netmgr_type_e type;     // network manage type
    netmgr_type_e active;   // the connect now used
    netmgr_status_e status; // the network status

    netmgr_conn_base_t *conn[NETCONN_AUTO - 1]; // connections
} netmgr_t;

static netmgr_t s_netmgr = {0};

/**
 * @brief get active connection status and
 *
 * @return netconn_type_t: the connection should be used
 */
static netmgr_type_e __get_active_conn()
{
    netmgr_t *netmgr = &s_netmgr;
    if (netmgr->type & NETCONN_WIFI && netmgr->type & NETCONN_WIRED) {
        netmgr_status_e wifi_status = NETMGR_LINK_DOWN;
        netmgr_status_e wired_status = NETMGR_LINK_DOWN;
        netmgr->conn[NETCONN_WIFI]->get(NETCONN_CMD_STATUS, &wifi_status);
        netmgr->conn[NETCONN_WIRED]->get(NETCONN_CMD_STATUS, &wired_status);

        if ((NETMGR_LINK_UP == wifi_status && NETMGR_LINK_UP == wired_status) ||
            (NETMGR_LINK_UP != wifi_status && NETMGR_LINK_UP != wired_status)) {
            // all connection up or down, return the high priority connection,
            // if same pririty, use wired connection first
            return (netmgr->conn[NETCONN_WIFI]->pri > netmgr->conn[NETCONN_WIRED]->pri) ? NETCONN_WIFI : NETCONN_WIRED;
        } else {
            // otherwise, return the up connection
            return (NETMGR_LINK_UP == wifi_status) ? NETCONN_WIFI : NETCONN_WIRED;
        }
    } else {
        return netmgr->type;
    }
}

/**
 * @brief connection event callback, called when connection event happed
 *
 * @param event the connection event
 */
static void __netmgr_event_cb(netmgr_type_e type, netmgr_status_e status)
{
    // status unused
    (void)status;

    if (s_netmgr.type & type) {
        netmgr_status_e active_status = NETMGR_LINK_DOWN;
        netmgr_type_e active_conn = __get_active_conn();
        s_netmgr.conn[active_conn]->get(NETCONN_CMD_STATUS, &active_status);

        // both changed
        if (active_status != s_netmgr.status && active_conn != s_netmgr.active) {
            PR_DEBUG("netmgr changed active %d status %d, old active %d status %d", active_status, active_conn,
                     s_netmgr.active, s_netmgr.status);
            s_netmgr.status = active_status;
            s_netmgr.active = active_conn;
            tal_event_publish(EVENT_LINK_TYPE_CHG, (void *)s_netmgr.active);
            tal_event_publish(EVENT_LINK_STATUS_CHG, (void *)s_netmgr.status);
        } else if (active_conn == s_netmgr.active) {
            // active_status changed
            PR_DEBUG("netmgr status changed to %d, old %d, active %d", active_status, s_netmgr.status, s_netmgr.active);
            s_netmgr.status = active_status;
            tal_event_publish(EVENT_LINK_STATUS_CHG, (void *)s_netmgr.status);
        } else if (active_status == s_netmgr.status) {
            // active_conn changed
            PR_DEBUG("netmgr active changed to %d, old %d, status %d", active_conn, s_netmgr.active, s_netmgr.status);
            s_netmgr.active = active_conn;
            tal_event_publish(EVENT_LINK_TYPE_CHG, (void *)s_netmgr.active);
        }
    }

    return;
}

OPERATE_RET __netmgr_conn_register(netmgr_type_e type, netmgr_conn_base_t *conn)
{
    s_netmgr.conn[type] = conn;
    s_netmgr.conn[type]->event_cb = __netmgr_event_cb;
    return s_netmgr.conn[type]->open(NULL);
}

/**
 * @brief Initializes the network manager.
 *
 * This function initializes the network manager based on the specified type.
 *
 * @param type The type of network manager to initialize.
 * @return The result of the initialization operation.
 */
OPERATE_RET netmgr_init(netmgr_type_e type)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&s_netmgr.lock));
    s_netmgr.status = NETMGR_LINK_DOWN;
    s_netmgr.type = type;

#ifdef ENABLE_WIRED
    if (type & NETCONN_WIRED) {
        __netmgr_conn_register(NETCONN_WIRED, (netmgr_conn_base_t *)&s_netmgr_wired);
    }
#endif

#ifdef ENABLE_CELLULAR
    if (type & NETCONN_CELLULAR) {
        __netmgr_conn_register(NETCONN_CELLULAR, (netmgr_conn_base_t *)&s_netmgr_cellular);
    }
#endif

#ifdef ENABLE_WIFI
    if (type & NETCONN_WIFI) {
        __netmgr_conn_register(NETCONN_WIFI, (netmgr_conn_base_t *)&s_netmgr_wifi);
    }
#endif
    s_netmgr.active = __get_active_conn();
    s_netmgr.inited = TRUE;

    tuya_lan_init(tuya_iot_client_get());

#ifdef ENABLE_BLUETOOTH
    tuya_ble_init(&(tuya_ble_cfg_t){.client = tuya_iot_client_get(), .device_name = "TYBLE"});
#endif

    return rt;
}

/**
 * @brief Sets the connection configuration for the network manager.
 *
 * This function is used to set the connection configuration for the network
 * manager.
 *
 * @param type The type of network manager.
 * @param cmd The connection configuration type.
 * @param param A pointer to the connection configuration parameter.
 *
 * @return The result of the operation.
 */
OPERATE_RET netmgr_conn_set(netmgr_type_e type, netmgr_conn_config_type_e cmd, void *param)
{
    if (!s_netmgr.inited) {
        return OPRT_RESOURCE_NOT_READY;
    }

    PR_DEBUG("netmgr conn %d set %d para %p", type, cmd, param);
    OPERATE_RET rt = OPRT_OK;
    if (NETCONN_AUTO == type) {
        TUYA_CALL_ERR_RETURN(s_netmgr.conn[s_netmgr.active]->set(cmd, param));
    } else if (s_netmgr.type & type) {
        TUYA_CALL_ERR_RETURN(s_netmgr.conn[type]->set(cmd, param));
    }

    return rt;
}

/**
 * @brief Get the connection configuration for the specified network manager
 * type.
 *
 * This function retrieves the connection configuration for the specified
 * network manager type.
 * @param type The network manager type.
 * @param cmd The connection configuration type.
 * @param param A pointer to the parameter structure for the connection
 * configuration.
 *
 * @return The operation result status.
 */
OPERATE_RET netmgr_conn_get(netmgr_type_e type, netmgr_conn_config_type_e cmd, void *param)
{
    if (!s_netmgr.inited) {
        return OPRT_RESOURCE_NOT_READY;
    }

    PR_TRACE("netmgr conn %d get %d para %p", type, cmd, param);
    OPERATE_RET rt = OPRT_OK;
    if (NETCONN_AUTO == type) {
        // get the active connection
        type = s_netmgr.active;
    }

    while (cur_conn) {
        if (cur_conn->type == type) {
            TUYA_CHECK_NULL_RETURN(cur_conn->get, OPRT_INVALID_PARM);

            rt = cur_conn->get(cmd, param);
            if (OPRT_OK != rt) {
                PR_ERR("netmgr conn %s get failed, cmd %d, rt = %d", NETMGR_TYPE_TO_STR(type), cmd, rt);
                return rt;
            }
            break;
        }
        cur_conn = cur_conn->next;
    }

    return rt;
}

/**
 * @brief Executes a network manager command.
 *
 * This function is responsible for executing a network manager command.
 *
 * @param argc The number of command line arguments.
 * @param argv An array of command line arguments.
 */
void netmgr_cmd(int argc, char *argv[])
{
    if (!s_netmgr.inited) {
        PR_INFO("network not ready!");
        return;
    }

    if (argc > 5) {
        PR_INFO("usage: netmgr [wifi|wired|switch] [donw/up]");
        return;
    }

    if (argc == 1) {
        // dump network connection
        PR_NOTICE("netmgr active %d, status %d", s_netmgr.active, s_netmgr.status);
        PR_NOTICE("---------------------------------------");
        if (s_netmgr.type & NETCONN_WIFI) {
            PR_NOTICE("type %d pri %d status %d", s_netmgr.conn[NETCONN_WIFI]->type, s_netmgr.conn[NETCONN_WIFI]->pri,
                      s_netmgr.conn[NETCONN_WIFI]->status);
        }
        if (s_netmgr.type & NETCONN_WIRED) {
            PR_NOTICE("type %d pri %d status %d", s_netmgr.conn[NETCONN_WIRED]->type, s_netmgr.conn[NETCONN_WIRED]->pri,
                      s_netmgr.conn[NETCONN_WIRED]->status);
        }
    } else {
        if (0 == strcmp(argv[1], "wifi")) {
#ifdef ENABLE_WIFI
            if (!s_netmgr.type & NETCONN_WIFI) {
                PR_INFO("usage: netmgr [wifi] [donw/up/scan]");
            } else if (0 == strcmp(argv[2], "up")) {
                netconn_wifi_info_t wifi_info = {0};
                strcpy(wifi_info.ssid, argv[3]);
                strcpy(wifi_info.pswd, argv[4]);
                netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info);
            } else if (0 == strcmp(argv[2], "down")) {
                netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_CLOSE, NULL);
            } else if (0 == strcmp(argv[2], "scan")) {

                AP_IF_S *aplist;
                uint32_t num;
                tal_wifi_all_ap_scan(&aplist, &num);
            } else {
                PR_INFO("usage: netmgr [wifi] [donw/up/scan]");
            }
#else
            PR_INFO("wifi disabled");
#endif
        } else if (0 == strcmp(argv[1], "wired")) {
#ifdef ENABLE_WIRED
            if (!(s_netmgr.type & NETCONN_WIRED)) {
                PR_INFO("usage: netmgr [wired] [donw/up]");
            } else if (0 == strcmp(argv[2], "up")) {
                // TBD..
            } else if (0 == strcmp(argv[2], "down")) {
                // TBD
            } else {
                PR_INFO("usage: netmgr wire [donw/up]");
            }
#else
            PR_INFO("wired disabled");
#endif
        } else if (0 == strcmp(argv[1], "switch")) {

        } else {
            PR_INFO("usage: netmgr [wifi|wired|switch] [donw|up]");
        }
    }

    return;
}
