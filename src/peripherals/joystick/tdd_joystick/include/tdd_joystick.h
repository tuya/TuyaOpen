/**
 * @file tdd_joystick_gpio.h
 * @brief tdd_joystick_gpio, irq
 * @version 1.0
 * @date 2022-03-20
 * @copyright Copyright (c) tuya.inc 2022
 * GPIO joystick adaptation
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
    TUYA_GPIO_NUM_E btn_pin;        // button cfg
    TUYA_GPIO_LEVEL_E level;
    TDD_GPIO_TYPE_U pin_type;
    TDL_JOYSTICK_MODE_E mode;

    TUYA_ADC_NUM_E adc_num;         // adc cfg
    TUYA_ADC_BASE_CFG_T adc_cfg;    
} JOYSTICK_GPIO_CFG_T;

/**
 * @brief gpio joystick register
 * @param[in] name  joystick name
 * @param[in] gpio_cfg  joystick hardware configuration
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdd_joystick_register(char *name, JOYSTICK_GPIO_CFG_T *gpio_cfg);

// /**
//  * @brief Update the effective level of joystick configuration
//  * @param[in] handle  joystick handle
//  * @param[in] level  level
//  * @return Function Operation Result  OPRT_OK is ok other is fail
//  */
OPERATE_RET tdd_joystick_update_level(TDL_JOYSTICK_DEV_HANDLE handle, TUYA_GPIO_LEVEL_E level);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_TDD_GPIO_BUTTON_H_*/