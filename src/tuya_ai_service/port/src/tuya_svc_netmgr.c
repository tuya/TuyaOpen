/**
 * @file tuya_svc_netmgr.c
 * @brief tuya_svc_netmgr module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_svc_netmgr.h"

#include "netmgr.h"

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


/***********************************************************
***********************function define**********************
***********************************************************/
NETWORK_STATUS_E tuya_svc_netmgr_get_status(VOID)
{
    netmgr_status_e net_status = NETMGR_LINK_DOWN;

    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &net_status);

    return ((net_status == NETMGR_LINK_UP) ? (NETWORK_STATUS_MQTT) : (NETWORK_STATUS_OFFLINE));
}