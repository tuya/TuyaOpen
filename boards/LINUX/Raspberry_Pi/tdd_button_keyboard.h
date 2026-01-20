/**
 * @file tdd_button_keyboard.h
 * @brief tdd_button_keyboard module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_BUTTON_KEYBOARD_H__
#define __TDD_BUTTON_KEYBOARD_H__

#include "tuya_cloud_types.h"
#include "tdl_button_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDL_BUTTON_MODE_E mode;
} BUTTON_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief gpio button register
 * @param[in] name  button name
 * @param[in] gpio_cfg  button hardware configuration
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdd_keyboard_button_register(char *name, BUTTON_CFG_T *gpio_cfg);

/**
 * @brief Update the effective level of button configuration
 * @param[in] handle  button handle
 * @param[in] level  level
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdd_gpio_button_update_level(DEVICE_BUTTON_HANDLE handle, TUYA_GPIO_LEVEL_E level);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_BUTTON_KEYBOARD_H__ */
