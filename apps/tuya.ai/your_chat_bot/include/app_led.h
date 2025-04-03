/**
 * @file app_led.h
 * @brief app_led module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#ifndef __APP_LED_H__
#define __APP_LED_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET app_led_init(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E active_level);

OPERATE_RET app_led_set(uint8_t is_on);

uint8_t app_led_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_LED_H__ */
