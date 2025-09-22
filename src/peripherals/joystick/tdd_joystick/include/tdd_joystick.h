/**
 * @file tdd_joystick.h
 * @brief Joystick driver module
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * @date 2025-07-08     maidang      Initial version
 */

#ifndef _TDD_GPIO_JOYSTICK_H_
#define _TDD_GPIO_JOYSTICK_H_

#include "tuya_cloud_types.h"
#include "tdl_joystick_driver.h"
#include "tdd_button_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    TUYA_GPIO_NUM_E btn_pin;        /* button gpio pin */
    TUYA_GPIO_LEVEL_E level;        /* button gpio level */
    TDD_GPIO_TYPE_U pin_type;       /* button gpio type */
    TDL_JOYSTICK_MODE_E mode;       /* joystick mode */

    TUYA_ADC_NUM_E adc_num;         /* adc num */
    uint8_t        adc_ch_x;
    uint8_t        adc_ch_y;
    TUYA_ADC_BASE_CFG_T adc_cfg;    /* adc cfg */
} JOYSTICK_GPIO_CFG_T;

/**
 * @brief gpio joystick register
 * @param[in] name  joystick name
 * @param[in] gpio_cfg  joystick hardware configuration
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdd_joystick_register(char *name, JOYSTICK_GPIO_CFG_T *gpio_cfg);

/**
 * @brief Update the effective level of joystick configuration
 * @param[in] handle  joystick handle
 * @param[in] level  level
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdd_joystick_update_level(TDL_JOYSTICK_DEV_HANDLE handle, TUYA_GPIO_LEVEL_E level);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_TDD_GPIO_BUTTON_H_*/