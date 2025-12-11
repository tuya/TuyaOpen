/**
 * @file gw_intf.c
 * @brief gw_intf module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "gw_intf.h"

#include "tal_api.h"

#include "tuya_iot.h"

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
static GW_CNTL_S g_gw_cntl = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Get gateway cntl
 *
 * @return Gateway cntl, see GW_CNTL_S
 */
GW_CNTL_S *get_gw_cntl(VOID)
{
    tuya_iot_client_t *client = tuya_iot_client_get();
    if (client == NULL) {
        g_gw_cntl.is_init = 0;
        return &g_gw_cntl;
    } else {
        g_gw_cntl.is_init = 1;
    }

    // gw_actv
    bool is_activated = tuya_iot_activated(client);
    g_gw_cntl.gw_wsm.stat = is_activated ? ACTIVATED : UNREGISTERED;

    // local_key
    const char *local_key = tuya_iot_localkey_get(client);
    memset(g_gw_cntl.gw_actv.local_key, 0, LOCAL_KEY_LEN + 1);
    strncpy(g_gw_cntl.gw_actv.local_key, local_key, LOCAL_KEY_LEN);

    return &g_gw_cntl;
}