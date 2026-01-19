/**
 * @file tuya_devos_utils.c
 * @brief tuya_devos_utils module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_devos_utils.h"

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
static char sg_devid[MAX_LENGTH_DEVICE_ID + 1] = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
CONST char *get_gw_dev_id(VOID)
{
    tuya_iot_client_t *client = tuya_iot_client_get();
    if (NULL == client) {
        return NULL;
    }

    memset(sg_devid, 0, MAX_LENGTH_DEVICE_ID + 1);
    const char *dev_id = tuya_iot_devid_get(client);
    if (dev_id != NULL) {
        strncpy(sg_devid, dev_id, MAX_LENGTH_DEVICE_ID);
    }
    
    return sg_devid;
}