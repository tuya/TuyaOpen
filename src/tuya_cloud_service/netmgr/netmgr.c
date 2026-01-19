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
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * 2025-07-11   yangjie     Refactored network manager to support management of multiple network connection types
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

    netmgr_conn_base_t *conn; // connections
} netmgr_t;

static netmgr_t s_netmgr = {0};

static TIMER_ID sg_lan_init_timer = NULL;

/**
 * @brief get active connection status and
 *
 * @return netconn_type_t: the connection should be used
 */
static netmgr_type_e __get_active_conn()
{
    netmgr_type_e active_type = NETCONN_AUTO;
    netmgr_conn_base_t *cur_conn = s_netmgr.conn;

    if (NULL == cur_conn) {
        PR_ERR("no connection registered");
        return NETCONN_AUTO;
    }

    netmgr_status_e netmgr_status = NETMGR_LINK_DOWN;

    active_type = cur_conn->type;

    while (cur_conn) {
        netmgr_status = NETMGR_LINK_DOWN;
        cur_conn->get(NETCONN_CMD_STATUS, &netmgr_status);
        if (netmgr_status == NETMGR_LINK_UP) {
            // return the first connection which is up
            PR_TRACE("netmgr active connection [%s]", NETMGR_TYPE_TO_STR(cur_conn->type));
            active_type = cur_conn->type;
            break;
        }
        cur_conn = cur_conn->next;
    }

    return active_type;
}

void __tuya_lan_init_tm_cb(TIMER_ID timer_id, void *arg)
{
    if (s_netmgr.status != NETMGR_LINK_UP) {
        return;
    }

    netmgr_type_e type = (netmgr_type_e)s_netmgr.type;
    tuya_iot_client_t *client = tuya_iot_client_get();

    if ((type & NETCONN_WIRED || type & NETCONN_WIFI) && client->is_activated) {
        PR_DEBUG("Start LAN initialization");
        tuya_lan_init(client);
        tal_sw_timer_stop(sg_lan_init_timer);
    }

    return;
}

static netmgr_conn_base_t *__get_conn_by_type(netmgr_type_e type)
{
    netmgr_conn_base_t *cur_conn = s_netmgr.conn;

    if (NETCONN_AUTO == type) {
        PR_ERR("type is NETCONN_AUTO");
        return NULL;
    }

    while (cur_conn) {
        if (cur_conn->type == type) {
            return cur_conn;
        }
        cur_conn = cur_conn->next;
    }

    PR_ERR("[%s] not found", NETMGR_TYPE_TO_STR(type));
    return NULL;
}

static OPERATE_RET __get_netmgr_status(netmgr_type_e type, netmgr_status_e *status)
{
    OPERATE_RET rt = OPRT_OK;
    netmgr_conn_base_t *cur_conn = s_netmgr.conn;

    if (NULL == status) {
        PR_ERR("netmgr get status failed, status is NULL");
        return OPRT_INVALID_PARM;
    }

    if (NETCONN_AUTO == type) {
        PR_ERR("netmgr get status failed, type is NETCONN_AUTO");
        return OPRT_INVALID_PARM;
    }

    *status = NETMGR_LINK_DOWN;

    if (!(s_netmgr.type & type)) {
        PR_ERR("netmgr type [%s] not supported", NETMGR_TYPE_TO_STR(type));
        return OPRT_NOT_SUPPORTED;
    }

    while (cur_conn) {
        if (cur_conn->type == type) {
            // get the connection status
            if (NULL == cur_conn->get) {
                PR_ERR("netmgr conn [%s] get status failed", NETMGR_TYPE_TO_STR(type));
                return OPRT_INVALID_PARM;
            }

            cur_conn->get(NETCONN_CMD_STATUS, status);
            PR_TRACE("netmgr conn [%s] status [%s]", NETMGR_TYPE_TO_STR(type), NETMGR_STATUS_TO_STR(*status));
            break;
        }
        cur_conn = cur_conn->next;
    }

    return rt;
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
        __get_netmgr_status(active_conn, &active_status);

        // both changed
        if (active_status != s_netmgr.status && active_conn != s_netmgr.active) {
            PR_DEBUG("netmgr conn type changed [%s] --> [%s], status changed %d --> %d",
                     NETMGR_TYPE_TO_STR(s_netmgr.active), NETMGR_TYPE_TO_STR(active_conn), s_netmgr.status,
                     active_status);
            s_netmgr.status = active_status;
            s_netmgr.active = active_conn;
            netmgr_conn_base_t *p_conn = __get_conn_by_type(active_conn);
            tal_network_card_set_active(p_conn->card_type);
            tal_event_publish(EVENT_LINK_TYPE_CHG, (void *)&s_netmgr.active);
            tal_event_publish(EVENT_LINK_STATUS_CHG, (void *)&s_netmgr.status);
        } else if (active_status != s_netmgr.status) {
            // active_status changed
            PR_DEBUG("netmgr conn status changed [%s] --> [%s]", NETMGR_STATUS_TO_STR(s_netmgr.status),
                     NETMGR_STATUS_TO_STR(active_status));
            s_netmgr.status = active_status;
            tal_event_publish(EVENT_LINK_STATUS_CHG, (void *)&s_netmgr.status);
        } else if (active_conn != s_netmgr.active) {
            // active_conn changed
            PR_DEBUG("netmgr conn type changed [%s] --> [%s]", NETMGR_TYPE_TO_STR(s_netmgr.active),
                     NETMGR_TYPE_TO_STR(active_conn));
            s_netmgr.active = active_conn;
            netmgr_conn_base_t *p_conn = __get_conn_by_type(active_conn);
            tal_network_card_set_active(p_conn->card_type);
            tal_event_publish(EVENT_LINK_TYPE_CHG, (void *)&s_netmgr.active);
        }
    }

    return;
}

OPERATE_RET __netmgr_conn_register(netmgr_type_e type, netmgr_conn_base_t *conn)
{
    OPERATE_RET rt = OPRT_OK;

    netmgr_conn_base_t *cur_conn = s_netmgr.conn;
    netmgr_conn_base_t *prev_conn = NULL;

    if (NULL == conn) {
        PR_ERR("netmgr [%s] register failed, conn is NULL", NETMGR_TYPE_TO_STR(type));
        return OPRT_INVALID_PARM;
    }

    conn->event_cb = __netmgr_event_cb;

    // check if the connection already registered
    while (cur_conn) {
        if (type == cur_conn->type) {
            PR_DEBUG("netmgr [%s] already registered", NETMGR_TYPE_TO_STR(type));
            return OPRT_INVALID_PARM;
        }
        cur_conn = cur_conn->next;
    }
    PR_DEBUG("netmgr [%s] register start", NETMGR_TYPE_TO_STR(type));

    // First insert the new connection
    if (NULL == s_netmgr.conn) {
        s_netmgr.conn = conn;
        conn->next = NULL;
        PR_DEBUG("netmgr [%s] is the first connection", NETMGR_TYPE_TO_STR(type));
        goto __EXIT;
    }

    // Insert the new connection in the linked list based on priority
    cur_conn = s_netmgr.conn;
    while (cur_conn) {
        if (cur_conn->pri < conn->pri) {
            if (prev_conn == NULL) {
                // insert at the head
                s_netmgr.conn = conn;
                conn->next = cur_conn;
            } else {
                // insert in the middle
                prev_conn->next = conn;
                conn->next = cur_conn;
            }
            break;
        }

        prev_conn = cur_conn;
        cur_conn = cur_conn->next;
    }

    // If we reached the end of the list, insert at the tail
    if (cur_conn == NULL) {
        if (prev_conn == NULL) {
            // This should not happen as we already handled empty list case above
            s_netmgr.conn = conn;
            conn->next = NULL;
        } else {
            prev_conn->next = conn;
            conn->next = NULL;
        }
    }

__EXIT:
    if (NULL != conn->open) {
        rt = conn->open(NULL);
    }

    return rt;
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

    TUYA_CALL_ERR_RETURN(tal_network_card_init());

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
    if (s_netmgr.active == NETCONN_AUTO) {
        PR_ERR("No connection available, please check your configuration");
        return OPRT_INVALID_PARM;
    }

    s_netmgr.inited = TRUE;

    // Cellular not support LAN
#if !defined(ENABLE_CELLULAR) || (ENABLE_CELLULAR == 0)
    tal_sw_timer_create(__tuya_lan_init_tm_cb, NULL, &sg_lan_init_timer);
    tal_sw_timer_start(sg_lan_init_timer, 500, TAL_TIMER_CYCLE);
#endif

#ifdef ENABLE_BLUETOOTH
    tuya_ble_cfg_t ble_cfg = {0};
    ble_cfg.client = tuya_iot_client_get();
    snprintf(ble_cfg.device_name, sizeof(ble_cfg.device_name), "TYBLE");
    tuya_ble_init(&ble_cfg);
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
    OPERATE_RET rt = OPRT_OK;
    netmgr_conn_base_t *cur_conn = s_netmgr.conn;

    if (!s_netmgr.inited) {
        return OPRT_RESOURCE_NOT_READY;
    }

    PR_DEBUG("netmgr conn %s set %d", NETMGR_TYPE_TO_STR(type), cmd);

    if (NETCONN_AUTO == type) {
        // get the active connection
        type = s_netmgr.active;
    }

    while (cur_conn) {
        if (cur_conn->type == type) {
            TUYA_CHECK_NULL_RETURN(cur_conn->set, OPRT_INVALID_PARM);
            rt = cur_conn->set(cmd, param);
            break;
        }
        cur_conn = cur_conn->next;
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
    OPERATE_RET rt = OPRT_OK;
    netmgr_conn_base_t *cur_conn = s_netmgr.conn;

    if (!s_netmgr.inited) {
        return OPRT_RESOURCE_NOT_READY;
    }

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

    netmgr_conn_base_t *p_conn = NULL;

    if (argc == 1) {
        // dump network connection
        PR_NOTICE("netmgr active %d, status %d", s_netmgr.active, s_netmgr.status);
        PR_NOTICE("---------------------------------------");
        if (s_netmgr.type & NETCONN_WIFI) {
            p_conn = __get_conn_by_type(NETCONN_WIFI);
            if (p_conn) {
                PR_NOTICE("type wifi pri %d status %s", p_conn->pri, NETMGR_STATUS_TO_STR(p_conn->status));
            }
        }
        if (s_netmgr.type & NETCONN_WIRED) {
            p_conn = __get_conn_by_type(NETCONN_WIRED);
            if (p_conn) {
                PR_NOTICE("type wire pri %d status %s", p_conn->pri, NETMGR_STATUS_TO_STR(p_conn->status));
            }
        }
    } else {
        if (0 == strcmp(argv[1], "wifi")) {
#ifdef ENABLE_WIFI
            if (!(s_netmgr.type & NETCONN_WIFI)) {
                PR_INFO("usage: netmgr [wifi] [down/up/scan]");
            } else if (0 == strcmp(argv[2], "up")) {
                netconn_wifi_info_t wifi_info = {0};
                if (argc < 4) {
                    PR_INFO("usage: netmgr wifi up <ssid> <password>");
                    return;
                }
                if (strlen(argv[3]) > WIFI_SSID_LEN || strlen(argv[4]) > WIFI_PASSWD_LEN) {
                    PR_INFO("ssid or password too long");
                    return;
                }
                strncpy(wifi_info.ssid, argv[3], sizeof(wifi_info.ssid) - 1);
                wifi_info.ssid[sizeof(wifi_info.ssid) - 1] = '\0';
                strncpy(wifi_info.pswd, argv[4], sizeof(wifi_info.pswd) - 1);
                wifi_info.pswd[sizeof(wifi_info.pswd) - 1] = '\0';
                netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info);
            } else if (0 == strcmp(argv[2], "down")) {
                netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_CLOSE, NULL);
            } else if (0 == strcmp(argv[2], "scan")) {
                AP_IF_S *aplist;
                uint32_t num;
                tal_wifi_all_ap_scan(&aplist, &num);
            } else {
                PR_INFO("usage: netmgr [wifi] [down/up/scan]");
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
            return;
        } else if (0 == strcmp(argv[1], "switch")) {
            PR_DEBUG("netmgr switch not implemented yet");
        } else {
            PR_INFO("usage: netmgr [wifi|wired|switch] [down|up]");
        }
    }
}
