/**
 * @file netconn_wired.c
 * @brief Implementation of wired network connection management for Tuya
 * devices.
 *
 * This file provides the implementation for managing wired network connections
 * on Tuya devices, including opening and closing connections, getting and
 * setting network parameters, and handling network events. It utilizes the TAL
 * for wired network communication and integrates with the MQTT binding for
 * network event notifications.
 *
 * The wired network connection management is essential for devices that support
 * Ethernet connectivity, ensuring reliable and stable network communication for
 * Tuya IoT devices.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * 2025-07-11   yangjie     Adjust WiFi priority
 *
 */

#include "netconn_wired.h"
#include "tal_api.h"
#include "tal_wired.h"
#include "mqtt_bind.h"

netmgr_conn_wired_t s_netmgr_wired = {
    .base = {.pri = 2,
             .type = NETCONN_WIRED,
             .status = NETMGR_LINK_DOWN,
             .card_type = TAL_NET_TYPE_POSIX,
             .open = netconn_wired_open,
             .close = netconn_wired_close,
             .get = netconn_wired_get,
             .set = netconn_wired_set},
};

/**
 * @brief a callback used to process the lowlayer event
 *
 * @param event the tal wired event
 * @param arg
 * @return static
 */
static void __netconn_wired_event(WIRED_STAT_E event)
{
    netmgr_conn_wired_t *netmgr_wired = &s_netmgr_wired;

    PR_NOTICE("wired status changed to %d, old stat: %d", event, netmgr_wired->base.status);
    netmgr_wired->base.status = (event == TKL_WIRED_LINK_UP) ? NETMGR_LINK_UP : NETMGR_LINK_DOWN;

    // notify netmgr
    if (netmgr_wired->base.event_cb) {
        netmgr_wired->base.event_cb(NETCONN_WIRED, netmgr_wired->base.status);
    }

    return;
}

/**
 * @brief open wired connection
 *
 * @param config: wired connection config
 * @return OPERATE_RET
 */
OPERATE_RET netconn_wired_open(void *config)
{
    OPERATE_RET rt = OPRT_OK;
    netmgr_conn_wired_t *netmgr_wired = &s_netmgr_wired;

    // open wired connection, default disconnect
    // memcpy(&netmgr_wired->config, config, sizeof(netmgr_wired->config));
    netmgr_wired->base.status = NETMGR_LINK_DOWN;
    TUYA_CALL_ERR_RETURN(tal_wired_set_status_cb(__netconn_wired_event));

    tuya_iot_token_get_port_register(tuya_iot_client_get(), mqtt_bind_token_get);

    return rt;
}

/**
 * @brief close wired connection
 *
 * @param config: wired connection config
 * @return OPERATE_RET
 */
OPERATE_RET netconn_wired_close()
{
    return OPRT_OK;
}

/**
 * @brief update wired connection
 *
 * @param config: the new config
 * @return OPERATE_RET
 */
OPERATE_RET netconn_wired_set(netmgr_conn_config_type_e cmd, void *param)
{
    netmgr_conn_wired_t *netmgr_wired = &s_netmgr_wired;
    OPERATE_RET rt = OPRT_OK;

    switch (cmd) {
    case NETCONN_CMD_PRI:
        netmgr_wired->base.pri = *(int *)param;
        netmgr_wired->base.event_cb(NETCONN_WIRED, netmgr_wired->base.status);
        break;
    case NETCONN_CMD_IP:
        TUYA_CALL_ERR_RETURN(tal_wired_set_ip((NW_IP_S *)param));
        break;
    case NETCONN_CMD_MAC:
        TUYA_CALL_ERR_RETURN(tal_wired_set_mac((NW_MAC_S *)param));
        break;
    default:
        return OPRT_NOT_SUPPORTED;
    }

    return OPRT_OK;
}

/**
 * @brief get wired connection attribte
 *
 * @param type
 * @param cmd
 * @param param
 * @return OPERATE_RET
 */
OPERATE_RET netconn_wired_get(netmgr_conn_config_type_e cmd, void *param)
{
    netmgr_conn_wired_t *netmgr_wired = &s_netmgr_wired;
    OPERATE_RET rt = OPRT_OK;

    switch (cmd) {
    case NETCONN_CMD_PRI:
        *(int *)param = netmgr_wired->base.pri;
        break;
    case NETCONN_CMD_IP:
        TUYA_CALL_ERR_RETURN(tal_wired_get_ip((NW_IP_S *)param));
        break;
    case NETCONN_CMD_MAC:
        TUYA_CALL_ERR_RETURN(tal_wired_get_mac((NW_MAC_S *)param));
        break;
    case NETCONN_CMD_STATUS:
        *(netmgr_status_e *)param = netmgr_wired->base.status;
        break;
    case NETCONN_CMD_CLOSE:
        break;
    default:
        return OPRT_NOT_SUPPORTED;
    }

    return OPRT_OK;
}
