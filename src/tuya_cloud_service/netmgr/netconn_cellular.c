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
#if (defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)) || 100 == OPERATING_SYSTEM
            .card_type = TAL_NET_TYPE_POSIX,
#else
            .card_type = TAL_NET_TYPE_PLATFORM,
#endif
            .status = NETMGR_LINK_DOWN,
            .open = netconn_cellular_open,
            .close = netconn_cellular_close,
            .get = netconn_cellular_get,
            .set = netconn_cellular_set,
        },
};

static TIMER_ID __attribute__((unused)) cellular_status_timer = NULL;

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

static void __attribute__((unused)) __cellular_status_timer_cb(TIMER_ID timer_id, void *arg)
{
    OPERATE_RET rt = OPRT_OK;
    CELLULAR_STAT_E net_status = NETMGR_LINK_DOWN;
    TAL_CELLULAR_STAT_E stat;
    rt = tal_cellular_get_status(&stat);
    if (OPRT_OK == rt) {
        net_status = (stat == TAL_CELLULAR_LINK_UP) ? NETMGR_LINK_UP : NETMGR_LINK_DOWN;
    } else {
        net_status = NETMGR_LINK_DOWN;
    }
    __netconn_cellular_event(net_status);

    return;
}

OPERATE_RET netconn_cellular_open(void *config)
{
    OPERATE_RET rt = OPRT_OK;

    netmgr_conn_cellular_t *netmgr_cellular = &s_netmgr_cellular;

    // init
    TAL_CELLULAR_BASE_CFG_T cfg;
    memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.apn, sizeof(cfg.apn), "%s", "");
    tal_cellular_init(&cfg);

    netmgr_cellular->base.status = NETMGR_LINK_DOWN;
#if 1
    TUYA_CALL_ERR_RETURN(tal_cellular_set_status_cb(__netconn_cellular_event));
#else
    // TODO: T5AI has not implemented 4G status change callback, use software timer to poll status
    tal_sw_timer_create(__cellular_status_timer_cb, NULL, &cellular_status_timer);
    tal_sw_timer_start(cellular_status_timer, 3000, TAL_TIMER_CYCLE);
#endif

    tuya_iot_token_get_port_register(tuya_iot_client_get(), mqtt_bind_token_get);

    return rt;
}

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
