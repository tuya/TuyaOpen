/**
 * @file netconn_cellular.c
 * @brief netconn_cellular module is used to manage cellular network connections.
 *
 * This file provides the implementation of the netconn_cellular module,
 * which is responsible for managing cellular network connections.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * 2025-07-10   yangjie     Initial version.
 */

#include "netconn_cellular.h"

#if defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
#include "tal_api.h"
#include "netmgr.h"
#include "tal_cellular.h"
#include "mqtt_bind.h"

/* For TAL_NET_PROVIDER_DEFAULT below. netmgr.h used to pull this in; it no longer
 * does, so that the control plane's public header stays off the data plane. */
#include "tal_network_register.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
netmgr_conn_cellular_t s_netmgr_cellular = {
    .base =
        {
            .pri = 0,
            .type = NETCONN_CELLULAR,
            .card_type = TAL_NET_PROVIDER_DEFAULT,
            .status = NETMGR_LINK_DOWN,
            .open = netconn_cellular_open,
            .close = netconn_cellular_close,
            .get = netconn_cellular_get,
            .set = netconn_cellular_set,
        },
};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __netconn_cellular_event(CELLULAR_STAT_E event)
{
    netmgr_conn_cellular_t *netmgr_cellular = &s_netmgr_cellular;

    if ((event == TAL_CELLULAR_LINK_UP && netmgr_cellular->base.status == NETMGR_LINK_UP) ||
        (event == TAL_CELLULAR_LINK_DOWN && netmgr_cellular->base.status == NETMGR_LINK_DOWN)) {
        // no change
        return;
    }

    PR_NOTICE("cellular status changed to %d, old stat: %d", event, netmgr_cellular->base.status);
    netmgr_cellular->base.status = (event == TAL_CELLULAR_LINK_UP) ? NETMGR_LINK_UP : NETMGR_LINK_DOWN;

    // notify netmgr
    if (netmgr_cellular->base.event_cb) {
        netmgr_cellular->base.event_cb(NETCONN_CELLULAR, netmgr_cellular->base.status);
    }

    return;
}

OPERATE_RET netconn_cellular_open(void *config)
{
    OPERATE_RET rt = OPRT_OK;

    netmgr_conn_cellular_t *netmgr_cellular = &s_netmgr_cellular;

    // init
    TAL_CELLULAR_BASE_CFG_T cfg;
    memset(&cfg, 0, sizeof(cfg));
#if defined(CELLULAR_APN)
    snprintf(cfg.apn, sizeof(cfg.apn), "%s", CELLULAR_APN);
#endif
    PR_NOTICE("cellular open, apn [%s]", cfg.apn);
    tal_cellular_init(&cfg);

    netmgr_cellular->base.status = NETMGR_LINK_DOWN;
    TUYA_CALL_ERR_RETURN(tal_cellular_set_status_cb(__netconn_cellular_event));

    tuya_iot_token_get_port_register(tuya_iot_client_get(), mqtt_bind_token_get);

    return rt;
}

/**
 * @brief close the cellular connection
 *
 * A documented no-op, and it can only be a no-op. This is what
 * NETCONN_CTRL_SUSTAINED means for this driver: netmgr can start the subsystem
 * but cannot stop it, so the link is driver-sustained for the life of the
 * process.
 *
 * Nothing to release. netconn_cellular_open() subscribes to no event, creates no
 * timer and allocates nothing; it calls tal_cellular_init(), installs one status
 * callback and registers the global activation token port.
 *
 * Nothing to bring down either. tal_cellular.h has tal_cellular_init() but no
 * deinit, and no connect/disconnect pair - so the data context that init raised
 * stays up. There is likewise no documented way to withdraw the status callback,
 * so __netconn_cellular_event() must stay safe to enter after close(); it only
 * touches this file's static state and the NULL-checked base.event_cb, so it is.
 *
 * When the TKL layer grows a PPP/modem shutdown, this is the single place it
 * belongs, and the moment it exists NETCONN_CMD_CLOSE can be handed back to
 * netconn_cellular_set() and to NETCONN_CELLULAR_SET_MASK. Until then that
 * command is refused rather than answered with a hollow OPRT_OK. This function
 * stays, because netmgr_deinit() calls conn->close() on every link directly; it
 * simply has nothing to dismantle.
 *
 * Trivially idempotent.
 *
 * @return OPRT_OK, always.
 */
OPERATE_RET netconn_cellular_close(void)
{
    OPERATE_RET rt = OPRT_OK;

    return rt;
}

OPERATE_RET netconn_cellular_set(netmgr_conn_config_type_e cmd, void *param)
{
    OPERATE_RET rt = OPRT_OK;

    netmgr_conn_cellular_t *netmgr_cellular = &s_netmgr_cellular;

    switch (cmd) {
    case NETCONN_CMD_PRI: {
        netmgr_cellular->base.pri = *(int *)param;
        netmgr_cellular->base.event_cb(NETCONN_CELLULAR, netmgr_cellular->base.status);
    } break;
    // No NETCONN_CMD_CLOSE arm, on purpose. tal_cellular.h exposes
    // tal_cellular_init() but neither a deinit nor a connect/disconnect pair, so
    // this driver has no call that brings the bearer down - which is exactly what
    // NETCONN_CTRL_SUSTAINED means for it. The arm that used to be here returned
    // OPRT_OK after calling the no-op netconn_cellular_close(), telling
    // tuya_iot_destroy() the link was closed while it was still up. Falling into
    // default: reports OPRT_NOT_SUPPORTED, which is the truth and is what a
    // caller can act on. This is not "not supported yet by netmgr" - there is no
    // TKL entry point to support.
    default: {
        rt = OPRT_NOT_SUPPORTED;
    } break;
    }

    return rt;
}

OPERATE_RET netconn_cellular_get(netmgr_conn_config_type_e cmd, void *param)
{
    OPERATE_RET rt = OPRT_OK;

    netmgr_conn_cellular_t *netmgr_cellular = &s_netmgr_cellular;

    switch (cmd) {
    case NETCONN_CMD_PRI: {
        *(int *)param = netmgr_cellular->base.pri;
    } break;
    case NETCONN_CMD_STATUS: {
        *(netmgr_status_e *)param = netmgr_cellular->base.status;
    } break;
    case NETCONN_CMD_IP: {
        TUYA_CALL_ERR_RETURN(tal_cellular_get_ip((NW_IP_S *)param));
    } break;
    case NETCONN_CMD_MAC: {
        rt = OPRT_NOT_SUPPORTED;
    } break;
    default: {
        rt = OPRT_NOT_SUPPORTED;
    } break;
    }

    return rt;
}

#endif // defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
