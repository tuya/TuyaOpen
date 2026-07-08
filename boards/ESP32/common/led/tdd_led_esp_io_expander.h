/**
 * @file tdd_led_esp_io_expander.h
 * @brief TDD LED driver for LEDs connected via IO expander on ESP32 boards (e.g. XL9555, TCA9554).
 *
 * Uses function pointers for expander ops so any compatible expander can be used.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_LED_ESP_IO_EXPANDER_H__
#define __TDD_LED_ESP_IO_EXPANDER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

typedef int (*io_exp_led_set_dir_fn)(uint32_t pin_mask, int is_input);
typedef int (*io_exp_led_set_level_fn)(uint32_t pin_mask, int level);

typedef struct {
    uint32_t                pin_mask;     /*!< bit-mask of the expander pin used for this LED */
    TUYA_GPIO_LEVEL_E       active_level; /*!< TUYA_GPIO_LEVEL_HIGH or TUYA_GPIO_LEVEL_LOW */
    io_exp_led_set_dir_fn   set_dir;      /*!< set pin direction (required) */
    io_exp_led_set_level_fn set_level;    /*!< set output level (required) */
} TDD_LED_IO_EXP_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Register an LED that is connected to an IO expander.
 *
 * @param[in] name  unique LED device name
 * @param[in] cfg   hardware configuration, must not be NULL
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET tdd_led_esp_io_expander_register(char *name, TDD_LED_IO_EXP_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_LED_ESP_IO_EXPANDER_H__ */
