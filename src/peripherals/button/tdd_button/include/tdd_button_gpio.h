/**
 * @file tdd_button_gpio.h
 * @brief tdd_button_gpio, irq
 * @version 1.0
 * @date 2022-03-20
 * @copyright Copyright (c) tuya.inc 2022
 * GPIO button adaptation
 */

#ifndef _TDD_GPIO_BUTTON_H_
#define _TDD_GPIO_BUTTON_H_

#include "tuya_cloud_types.h"
#include "tdl_button_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef union {
    TUYA_GPIO_MODE_E gpio_pull; // for BUTTON_TIMER_SCAN_MODE
    TUYA_GPIO_IRQ_E irq_edge;   // for BUTTON_IRQ_MODE
} TDD_GPIO_TYPE_U;

typedef struct {
    TUYA_GPIO_NUM_E pin;
    TUYA_GPIO_LEVEL_E level;
    TDD_GPIO_TYPE_U pin_type;
    TDL_BUTTON_MODE_E mode;
} BUTTON_GPIO_CFG_T;

/**
 * @brief gpio button register
 * @param[in] name  button name
 * @param[in] gpio_cfg  button hardware configuration
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdd_gpio_button_register(char *name, BUTTON_GPIO_CFG_T *gpio_cfg);

/**
 * @brief Update the effective level of button configuration
 * @param[in] handle  button handle
 * @param[in] level  level
 * @return Function Operation Result  OPRT_OK is ok other is fail
 */
OPERATE_RET tdd_gpio_button_update_level(DEVICE_BUTTON_HANDLE handle, TUYA_GPIO_LEVEL_E level);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_TDD_GPIO_BUTTON_H_*/