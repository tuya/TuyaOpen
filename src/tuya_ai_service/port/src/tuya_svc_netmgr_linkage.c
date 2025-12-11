/**
 * @file tuya_svc_netmgr_linkage.c
 * @brief tuya_svc_netmgr_linkage module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_svc_netmgr_linkage.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
extern netmgr_linkage_t *__ai_port_linkage_get(VOID);
/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

netmgr_linkage_t *tuya_svc_netmgr_linkage_get(LINKAGE_TYPE_E type)
{
    if (type == LINKAGE_TYPE_WIFI || type == LINKAGE_TYPE_DEFAULT) {
        // TODO: 4G not supported yet
        return __ai_port_linkage_get();
    }

    return NULL;
}