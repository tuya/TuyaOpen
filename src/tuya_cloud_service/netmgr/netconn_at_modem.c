/**
 * @file netconn_at_modem.c
 * @brief netconn_at_modem module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "netconn_at_modem.h"

#include "tal_at_modem.h"

#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
// #define AT_TRANSPORT_NAME "AT_UART"

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
netmgr_conn_at_modem_t s_netmgr_at_modem = {
    .base =
        {
            .pri = 0,
            .type = NETCONN_AT_MODEM,
            .status = NETMGR_LINK_DOWN,
            .card_type = TAL_NET_TYPE_AT_MODEM,
            .open = netconn_at_modem_open,
            .close = netconn_at_modem_close,
            .get = netconn_at_modem_get,
            .set = netconn_at_modem_set,
        },
};

/***********************************************************
***********************function define**********************
***********************************************************/

static void __netconn_at_modem_event(AT_MODEM_EVENT_E event, void *arg)
{
    netmgr_conn_at_modem_t *netmgr_at = &s_netmgr_at_modem;

    PR_NOTICE("AT Modem status changed to %d, old stat: %d", event, netmgr_at->base.status);
    netmgr_at->base.status = (event == AT_CONNECTED) ? NETMGR_LINK_UP : NETMGR_LINK_DOWN;

    // notify netmgr
    if (netmgr_at->base.event_cb) {
        netmgr_at->base.event_cb(NETCONN_AT_MODEM, netmgr_at->base.status);
    }
}

/**
 * @brief open a at_modem connection
 *
 * @param config: at_modem connection configuration
 * @return OPERATE_RET: return OPERATE_OK on success, otherwise return error code
 */
OPERATE_RET netconn_at_modem_open(void *config)
{
    OPERATE_RET rt = OPRT_OK;

    // Create ML307R Client
    TUYA_CALL_ERR_RETURN(tal_at_modem_set_event_cb(__netconn_at_modem_event));
    TUYA_CALL_ERR_RETURN(tal_at_modem_init(AT_TRANSPORT_NAME, TAL_AT_MODEM_TYPE_ML307R));

    return rt;
}

/**
 * @brief update at_modem connection
 *
 * @param none
 * @return OPERATE_RET: return OPERATE_OK on success, otherwise return error code
 */
OPERATE_RET netconn_at_modem_close(void)
{
    // tal_at_modem_deinit

    return OPRT_OK;
}

/**
 * @brief update at_modem connection configuration
 *
 * @param cmd: command to update configuration
 * @param param: parameter for the command
 * @return OPERATE_RET: return OPERATE_OK on success, otherwise return error code
 */
OPERATE_RET netconn_at_modem_set(netmgr_conn_config_type_e cmd, void *param)
{
    OPERATE_RET rt = OPRT_OK;

    netmgr_conn_at_modem_t *netmgr_at = &s_netmgr_at_modem;

    switch (cmd) {
    case NETCONN_CMD_PRI: {
        netmgr_at->base.pri = *(int *)param;
        netmgr_at->base.event_cb(NETCONN_AT_MODEM, netmgr_at->base.status);
    } break;
    default: {
        rt = OPRT_NOT_SUPPORTED;
    } break;
    }

    return rt;
}

/**
 * @brief get at_modem connection attribute
 *
 * @param cmd: command to get attribute
 * @param param: parameter for the command
 * @return OPERATE_RET: return OPERATE_OK on success, otherwise return error code
 */
OPERATE_RET netconn_at_modem_get(netmgr_conn_config_type_e cmd, void *param)
{
    OPERATE_RET rt = OPRT_OK;

    netmgr_conn_at_modem_t *netmgr_at = &s_netmgr_at_modem;

    switch (cmd) {
    case NETCONN_CMD_PRI: {
        *(int *)param = netmgr_at->base.pri;
    } break;
    case NETCONN_CMD_STATUS: {
        *(netmgr_status_e *)param = netmgr_at->base.status;
    } break;
    case NETCONN_CMD_IP: {
        TUYA_CALL_ERR_RETURN(tal_at_modem_get_ip((NW_IP_S *)param));
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
