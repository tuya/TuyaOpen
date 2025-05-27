/**
 * @file tdd_led_gpio.h
 * @brief Header file defining the GPIO-based LED driver interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_LED_GPIO_H__
#define __TDD_LED_GPIO_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_GPIO_NUM_E pin;
    TUYA_GPIO_MODE_E mode;
    TUYA_GPIO_LEVEL_E level;
} TDD_LED_GPIO_CFG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Registers a GPIO-based LED device
 * 
 * @param dev_name The name of the LED device to register.
 * @param led_cfg A pointer to the TDD_LED_GPIO_CFG_T structure containing GPIO configuration.
 * 
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdd_led_gpio_register(char *dev_name, TDD_LED_GPIO_CFG_T *led_cfg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__TDD_LED_GPIO_H__*/