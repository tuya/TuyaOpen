/**
 * @file tdd_led_pwm.c
 * @brief PWM-based LED driver implementation
 *
 * This file implements the PWM-based LED driver for the TDD layer. It registers a PWM-backed LED
 * into the TDL LED framework (device registration, PWM init, on/off + brightness control, resource
 * release). The TDL layer is on/off based, so the steady on-brightness is a static property of the
 * device; the optional level control (used by tdl_led_breath) dims/breathes it.
 *
 * All percentages in the config are the LED's lit fraction (0 = off, 100 = full brightness). The
 * PWM hardware duty is the HIGH-level time fraction regardless of polarity, so the driver maps
 * brightness to duty based on the LED's active level: an active-HIGH LED uses duty = brightness,
 * an active-LOW LED uses duty = 100 - brightness. The channel keeps running and only the duty is
 * switched, which avoids output glitches.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tdd_led_pwm.h"

#include "tkl_pwm.h"
#include "tal_api.h"

#include "tdl_led_driver.h"
/***********************************************************
*************************micro define***********************
***********************************************************/
// Fixed PWM period the duty is expressed against (tkl_pwm treats duty as duty/10000 of the period).
#define TDD_LED_PWM_CYCLE     10000
#define TDD_LED_PWM_PCT_MAX   100

/***********************************************************
***********************function define**********************
***********************************************************/
// Map an LED lit-fraction (0..100 %) to the raw PWM duty (0..CYCLE). PWM duty is HIGH-level time,
// so an active-LOW LED (lit on the low phase) is driven with the complementary duty.
static uint32_t __tdd_led_pwm_duty(const TDD_LED_PWM_CFG_T *led_cfg, uint8_t brightness)
{
    uint8_t high_pct = 0;

    if (brightness > TDD_LED_PWM_PCT_MAX) {
        brightness = TDD_LED_PWM_PCT_MAX;
    }

    high_pct = (TUYA_GPIO_LEVEL_HIGH == led_cfg->active_level) ? brightness
                                                              : (uint8_t)(TDD_LED_PWM_PCT_MAX - brightness);

    return (uint32_t)high_pct * TDD_LED_PWM_CYCLE / TDD_LED_PWM_PCT_MAX;
}

// Scale a logical level (0..100) into the board's brightness window [duty_min_pct, duty_max_pct].
// Both 0 (a zero-initialized cfg) is treated as passthrough (identity 0..100).
static uint8_t __tdd_led_pwm_scale(const TDD_LED_PWM_CFG_T *led_cfg, uint8_t level)
{
    uint8_t lo = led_cfg->duty_min_pct;
    uint8_t hi = led_cfg->duty_max_pct;

    if (level > TDD_LED_PWM_PCT_MAX) {
        level = TDD_LED_PWM_PCT_MAX;
    }
    if (0 == lo && 0 == hi) {
        hi = TDD_LED_PWM_PCT_MAX;
    }
    if (hi > TDD_LED_PWM_PCT_MAX) {
        hi = TDD_LED_PWM_PCT_MAX;
    }
    if (lo > hi) {
        lo = hi;
    }

    return (uint8_t)(lo + (uint32_t)(hi - lo) * level / TDD_LED_PWM_PCT_MAX);
}

static OPERATE_RET __tdd_led_pwm_set(TDD_LED_HANDLE_T handle, bool is_on)
{
    TDD_LED_PWM_CFG_T *led_cfg = (TDD_LED_PWM_CFG_T *)handle;
    uint8_t            bright  = 0;

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    // On -> configured on-brightness; off -> 0. The channel keeps running.
    bright = (true == is_on) ? led_cfg->on_duty_pct : 0;

    return tkl_pwm_duty_set(led_cfg->pwm_ch, __tdd_led_pwm_duty(led_cfg, bright));
}

static OPERATE_RET __tdd_led_pwm_set_level(TDD_LED_HANDLE_T handle, uint8_t level)
{
    TDD_LED_PWM_CFG_T *led_cfg = (TDD_LED_PWM_CFG_T *)handle;
    uint8_t            bright  = 0;

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    // Level (0..100) scaled into the brightness window, then mapped to PWM duty. Used by the TDL
    // breath/dimming effect.
    bright = __tdd_led_pwm_scale(led_cfg, level);

    return tkl_pwm_duty_set(led_cfg->pwm_ch, __tdd_led_pwm_duty(led_cfg, bright));
}

static OPERATE_RET __tdd_led_pwm_open(TDD_LED_HANDLE_T handle)
{
    TDD_LED_PWM_CFG_T  *led_cfg = (TDD_LED_PWM_CFG_T *)handle;
    TUYA_PWM_BASE_CFG_T pwm_cfg = {0};
    OPERATE_RET         rt      = OPRT_OK;

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    // Idle/rest level = the LED-off level, so the pin sits at "off" before the first set and at
    // duty 0/full: active-HIGH LED off = LOW (NEGATIVE idle), active-LOW LED off = HIGH (POSITIVE).
    pwm_cfg.polarity   = (TUYA_GPIO_LEVEL_HIGH == led_cfg->active_level) ? TUYA_PWM_NEGATIVE
                                                                        : TUYA_PWM_POSITIVE;
    pwm_cfg.count_mode = TUYA_PWM_CNT_UP;
    pwm_cfg.cycle      = TDD_LED_PWM_CYCLE;
    pwm_cfg.frequency  = led_cfg->frequency;
    pwm_cfg.duty       = __tdd_led_pwm_duty(led_cfg, 0); // start off

    TUYA_CALL_ERR_RETURN(tkl_pwm_init(led_cfg->pwm_ch, &pwm_cfg));

    return tkl_pwm_start(led_cfg->pwm_ch);
}

static OPERATE_RET __tdd_led_pwm_close(TDD_LED_HANDLE_T handle)
{
    TDD_LED_PWM_CFG_T *led_cfg = (TDD_LED_PWM_CFG_T *)handle;

    if (NULL == handle) {
        return OPRT_INVALID_PARM;
    }

    tkl_pwm_stop(led_cfg->pwm_ch);

    return tkl_pwm_deinit(led_cfg->pwm_ch);
}

/**
 * @brief Registers a PWM-based LED device
 *
 * @param dev_name The name of the LED device to register.
 * @param led_cfg A pointer to the TDD_LED_PWM_CFG_T structure containing PWM configuration.
 *
 * @return Returns OPERATE_RET_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdd_led_pwm_register(char *dev_name, TDD_LED_PWM_CFG_T *led_cfg)
{
    TDD_LED_PWM_CFG_T *tdd_led_cfg = NULL;
    TDD_LED_INTFS_T    intfs;

    if (NULL == dev_name || NULL == led_cfg) {
        return OPRT_INVALID_PARM;
    }

    tdd_led_cfg = (TDD_LED_PWM_CFG_T *)tal_malloc(sizeof(TDD_LED_PWM_CFG_T));
    TUYA_CHECK_NULL_RETURN(tdd_led_cfg, OPRT_MALLOC_FAILED);
    memcpy(tdd_led_cfg, led_cfg, sizeof(TDD_LED_PWM_CFG_T));

    memset(&intfs, 0x00, sizeof(TDD_LED_INTFS_T));
    intfs.led_open      = __tdd_led_pwm_open;
    intfs.led_set       = __tdd_led_pwm_set;
    intfs.led_close     = __tdd_led_pwm_close;
    intfs.led_set_level = __tdd_led_pwm_set_level; // enables tdl_led_breath()

    return tdl_led_driver_register(dev_name, (TDD_LED_HANDLE_T)tdd_led_cfg, &intfs);
}
