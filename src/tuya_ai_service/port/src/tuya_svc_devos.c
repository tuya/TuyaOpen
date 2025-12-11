/**
 * @file tuya_svc_devos.c
 * @brief tuya_svc_devos module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_svc_devos.h"

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

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Get current devos state
 *
 * @return see DEVOS_STATE_E
 */
DEVOS_STATE_E tuya_svc_devos_get_state(VOID)
{
    tuya_iot_client_t *client = tuya_iot_client_get();

    bool activated = tuya_iot_activated(client);

    return activated ? DEVOS_STATE_ACTIVATED : DEVOS_STATE_UNREGISTERED;
}