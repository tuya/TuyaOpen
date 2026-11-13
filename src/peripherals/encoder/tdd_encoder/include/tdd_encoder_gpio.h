/**
 * @file tdd_encoder_gpio.h
 * @brief GPIO-based rotary encoder hardware driver.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef _TDD_ENCODER_GPIO_H_
#define _TDD_ENCODER_GPIO_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    TUYA_GPIO_NUM_E pin_a;
    TUYA_GPIO_NUM_E pin_b;
    TUYA_GPIO_NUM_E pin_button;
    TUYA_GPIO_MODE_E pin_mode;
    TUYA_GPIO_MODE_E button_pin_mode;
    TUYA_GPIO_LEVEL_E button_active_level;
    bool button_enabled;
    /* Hardware and mechanical traits forwarded to TDL at registration. */
    uint16_t debounce_ms;
    uint8_t steps_per_detent;
    bool reverse;
    /* Capture quadrature transitions in GPIO interrupts instead of polling. */
    bool irq_enabled;
} TDD_ENCODER_GPIO_CFG_T;

/**
 * @brief Register a polled GPIO rotary encoder hardware device.
 *
 * @param[in] name Encoder device name.
 * @param[in] cfg GPIO pin and electrical configuration.
 *
 * @return OPRT_OK on success, otherwise an error code.
 */
OPERATE_RET tdd_encoder_gpio_register(char *name, TDD_ENCODER_GPIO_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* _TDD_ENCODER_GPIO_H_ */
