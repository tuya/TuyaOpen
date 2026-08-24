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

/* For TAL_NET_PROVIDER_DEFAULT below. netmgr.h used to pull this in; it no longer
 * does, so that the control plane's public header stays off the data plane. */
#include "tal_network_register.h"

netmgr_conn_wired_t s_netmgr_wired = {
    .base = {.pri       = 2,
             .type      = NETCONN_WIRED,
             .status    = NETMGR_LINK_DOWN,
             .card_type = TAL_NET_PROVIDER_DEFAULT,
             .open      = netconn_wired_open,
             .close     = netconn_wired_close,
             .get       = netconn_wired_get,
             .set       = netconn_wired_set},
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
 * A no-op, and it can only be a no-op. This is what NETCONN_CTRL_OBSERVE means
 * for this driver, stated where the caller will look for it.
 *
 * Nothing to release. netconn_wired_open() subscribes to no event, creates no
 * timer and allocates nothing; it installs one status callback and registers the
 * global activation token port.
 *
 * Nothing to bring down either. tal_wired.h is exactly tal_wired_get_status(),
 * tal_wired_set_status_cb(), tal_wired_{get,set}_ip() and
 * tal_wired_{get,set}_mac() - there is no connect, no disconnect, no
 * enable/disable, no deinit. netmgr can prefer this link or avoid it when
 * routing, but it can never make it go down.
 *
 * The one teardown that looks available is not: clearing the callback with
 * tal_wired_set_status_cb(NULL). No TAL or TKL contract says NULL is accepted,
 * and the reference implementation in tools/porting/template/linux/tkl_wired.c
 * spawns a fresh detached polling thread on every call - so passing NULL would
 * not remove the callback path, it would add another thread to it. Leave the
 * callback installed; the driver's static state is safe to be called back into
 * at any time, including after close().
 *
 * Trivially idempotent.
 *
 * @return OPRT_OK, always.
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
