/**
 * @file tuya_svc_devos.h
 * @brief tuya_svc_devos module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_SVC_DEVOS_H__
#define __TUYA_SVC_DEVOS_H__

#include "tuya_cloud_types.h"

#include "tuya_ai_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define EVENT_DEVOS_STATE_CHANGE "devos.stat.chg" // devos state changed

/***********************************************************
***********************typedef define***********************
***********************************************************/
/* devos state machine */
typedef enum {
    DEVOS_STATE_INIT,         // device is inited
    DEVOS_STATE_UNREGISTERED, // device is not activated
    DEVOS_STATE_REGISTERING,  // token/meta is got, start to activate
    DEVOS_STATE_ACTIVATED,    // device is activated and full-functional
    DEVOS_STATE_UPGRADING,    // device is in OTA

    DEVOS_STATE_MAX
} DEVOS_STATE_E;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Get current devos state
 *
 * @return see DEVOS_STATE_E
 */
DEVOS_STATE_E tuya_svc_devos_get_state(VOID);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_SVC_DEVOS_H__ */
