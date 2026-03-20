/**
 * @file tuya_mqtt_dispatch.h
 * @brief tuya_mqtt_dispatch module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_MQTT_DISPATCH_H__
#define __TUYA_MQTT_DISPATCH_H__

#include "tuya_cloud_types.h"
#include "mqtt_service.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef int (*tuya_mqtt_dispatch_callback_t)(cJSON *json, void *user_data);

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tuya_mqtt_dispatch_init(void);

OPERATE_RET tuya_mqtt_dispatch_deinit(void);

OPERATE_RET tuya_mqtt_dispatch_register(uint16_t protocol_id, const char *type_name, const char *desc, tuya_mqtt_dispatch_callback_t callback, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_MQTT_DISPATCH_H__ */
