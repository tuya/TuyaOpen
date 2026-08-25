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
    .base = {.pri      = 2,
             .type     = NETCONN_WIRED,
             .status   = NETMGR_LINK_DOWN,
             .provider = TAL_NET_PROVIDER_DEFAULT,
             .open     = netconn_wired_open,
             .close    = netconn_wired_close,
             .get      = netconn_wired_get,
             .set      = netconn_wired_set},
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
 * The one teardown that looks available is not PORTABLE: clearing the callback
 * with tal_wired_set_status_cb(NULL). No TAL or TKL contract says NULL is
 * accepted, and the four implementations on disk do four different things.
 *
 *   platform/T5AI/.../driver/tkl_wired.c        assigns and returns OPRT_OK, no
 *   platform/T3/.../driver/tkl_wired.c          thread anywhere. NULL here IS a
 *                                               clean withdrawal.
 *   platform/LINUX/tuyaos_adapter/src/tkl_wired.c
 *                                               whole body under `if (cb)`, so
 *                                               NULL is ignored outright and does
 *                                               not even clear the stored pointer.
 *   tools/porting/template/linux/tkl_wired.c    no guard at all: NULL is stored
 *                                               AND a thread is spawned, which
 *                                               then calls the NULL pointer.
 *
 * So the callback stays installed, and the reason is portability rather than
 * impossibility. On T5AI and T3 withdrawing it would work and would be the right
 * thing; on LINUX it is a silent no-op; on anything derived from the template it
 * crashes. A driver in the common tree cannot tell which one it is linked
 * against, so it must assume the worst.
 *
 * That makes this a gap rather than a law - closing it needs a TKL contract that
 * says what NULL means, at which point two of the four platforms already comply.
 * Until then the driver's static state must stay safe to be called back into at
 * any time, including after close().
 *
 * (Twice-corrected note. The first version cited only the template and claimed a
 * per-init thread leak on LINUX; the second corrected that but said "neither
 * implementation in the tree honours NULL", having looked at the same two files.
 * Both times the error was reading a same-named file from the wrong directory.
 * `find . -name tkl_wired.c` answers it in one command.)
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
        /* Guarded like every other call to it in this file. Unreachable through
         * netmgr_conn_set(), which refuses before netmgr_init() has installed the
         * shim - but netconn_wired_set() is on the global public include path, so
         * "no caller does that today" is not the same as "nobody can". */
        if (netmgr_wired->base.event_cb) {
            netmgr_wired->base.event_cb(NETCONN_WIRED, netmgr_wired->base.status);
        }
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
    // No NETCONN_CMD_CLOSE arm: handling "close" in a getter is meaningless, and
    // no caller in the tree ever issued get(NETCONN_CMD_CLOSE). The empty arm it
    // replaces answered OPRT_OK for a command it did not perform. On the set side
    // the command never reaches this driver at all - NETCONN_WIRED_SET_MASK has no
    // CLOSE bit, because tal_wired.h has no way to take the link down, which is
    // what NETCONN_CTRL_OBSERVE records for this row.
    default:
        return OPRT_NOT_SUPPORTED;
    }

    return OPRT_OK;
}
