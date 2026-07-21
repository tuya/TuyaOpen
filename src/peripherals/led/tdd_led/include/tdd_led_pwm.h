/**
 * @file tdd_led_pwm.h
 * @brief PWM-based LED driver interface header file
 *
 * This header file defines the PWM-based LED driver interface for the TDD (Tuya Device Driver) layer.
 * It registers a PWM-backed LED so it can be driven through the standard TDL LED framework
 * (on/off, flash, blink). The LED's "on" brightness is a static property of the device: when the
 * framework turns the LED on the PWM outputs the configured duty, and off drives 0% duty. This lets
 * a board expose a dimmed indicator (e.g. a 20% charge LED) without changing the TDL on/off model.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_LED_PWM_H__
#define __TDD_LED_PWM_H__

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
    TUYA_PWM_NUM_E    pwm_ch;           // PWM channel driving the LED
    uint32_t          frequency;        // PWM frequency in Hz (e.g. 1000)
    TUYA_GPIO_LEVEL_E active_level;      // pin level that lights the LED: TUYA_GPIO_LEVEL_HIGH for an
                                        // active-HIGH LED, TUYA_GPIO_LEVEL_LOW for an active-LOW LED.
                                        // The driver maps brightness to PWM duty accordingly (an
                                        // active-LOW LED is driven with inverted duty) so all the
                                        // percentages below are the LED's lit fraction, 0 = off.
    uint8_t           on_duty_pct;      // 0..100 (%), brightness applied when the LED is turned on
    // PWM duty range that dimming/breathing (tdl_led_breath) is scaled into: a logical level
    // 0..100 maps linearly to [duty_min_pct, duty_max_pct], so level 0 -> duty_min_pct and
    // level 100 -> duty_max_pct. For the full range set duty_max_pct = 100 explicitly. Leaving
    // both 0 (a zero-initialized cfg) is treated as passthrough (identity 0..100). The steady
    // on/off duty above is independent of this range.
    uint8_t             duty_min_pct;   // 0..100 (%), duty the level control maps to at level 0
    uint8_t             duty_max_pct;   // 0..100 (%), duty the level control maps to at level 100
} TDD_LED_PWM_CFG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Registers a PWM-based LED device
 *
 * @param dev_name The name of the LED device to register.
 * @param led_cfg A pointer to the TDD_LED_PWM_CFG_T structure containing PWM configuration.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdd_led_pwm_register(char *dev_name, TDD_LED_PWM_CFG_T *led_cfg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__TDD_LED_PWM_H__*/
