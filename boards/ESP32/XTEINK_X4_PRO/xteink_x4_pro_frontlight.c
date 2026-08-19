/**
 * @file xteink_x4_pro_frontlight.c
 * @brief Xteink X4 Pro dual warm/cold frontlight driver.
 * @version 0.1
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * @note Recovered from the OEM LEDC init (FreeInk BoardConfig frontlight
 *       profile): two channels — GPIO8 and GPIO9, 10 kHz, active-HIGH
 *       (driving each pin high lights that LED string, confirmed on hardware
 *       in freeink-sdk/docs/xteink-x4pro-support.md). Mixing matches the
 *       FreeInk FrontlightManager:
 *         warm_duty = total * warmth / 100
 *         cool_duty = total - warm_duty
 *
 * @note ESP32 tkl_pwm adapter quirks handled here:
 *       1. tkl_pwm_init() scales TAL duty (hundredths of a percent) into a
 *          fixed 12-bit LEDC window, but tkl_pwm_duty_set() passes its
 *          argument straight through to ledc_set_duty(). Duties above 4096
 *          (40.96 %) would therefore latch fully ON — e.g. a warm channel at
 *          55 % duty saturating made the light "always warm". We pre-scale
 *          into the same 12-bit window before every duty_set.
 *       2. tkl_pwm_duty_set() applies the duty but then returns
 *          OPRT_NOT_SUPPORTED; the -4 is tolerated instead of propagated.
 *       3. ESP-IDF logs "GPIO x is not usable, maybe conflict with others"
 *          once per channel at init: ledc_channel_config() already reserves
 *          the pin, then tkl_pwm_start() re-attaches it. Harmless — both
 *          channels still emit PWM.
 */
#include "xteink_x4_pro_frontlight.h"

#include "board_config.h"
#include "tal_log.h"
#include "tkl_pinmux.h"
#include "tkl_pwm.h"
#include <string.h>

/* TUYA_PWM_NUM_0/1 map onto LEDC channels 0/1 via the adapter pin mux. */
#define FRONTLIGHT_PWM_COOL TUYA_PWM_NUM_0
#define FRONTLIGHT_PWM_WARM TUYA_PWM_NUM_1

#define FRONTLIGHT_DUTY_FULL 10000U /* TAL duty unit: hundredths of a percent */

/* ESP32 adapter runs LEDC at a fixed 12-bit resolution regardless of cycle. */
#define FRONTLIGHT_LEDC_FULL 4096U

/* Bring-up defaults: half brightness, neutral-cold white (warmth 0 %). */
#define FRONTLIGHT_DEFAULT_BRIGHTNESS 50U
#define FRONTLIGHT_DEFAULT_WARMTH     0U

static BOOL_T  s_fl_inited    = FALSE;
static uint8_t s_fl_brightness = 0;
static uint8_t s_fl_warmth     = 0;

/**
 * @brief Initialize one PWM channel onto its LED pin.
 * @param[in] ch TAL PWM channel.
 * @param[in] pin GPIO of the LED.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __channel_init(TUYA_PWM_NUM_E ch, TUYA_GPIO_NUM_E pin)
{
    OPERATE_RET          rt = OPRT_OK;
    TUYA_PWM_BASE_CFG_T  cfg;

    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config(pin, (TUYA_PIN_FUNC_E)(TUYA_PWM0 + ch)));

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.polarity   = TUYA_PWM_POSITIVE;
    cfg.count_mode = TUYA_PWM_CNT_UP;
    cfg.duty       = 0;
    cfg.cycle      = FRONTLIGHT_DUTY_FULL;
    cfg.frequency  = X4PRO_FRONTLIGHT_FREQ_HZ;

    TUYA_CALL_ERR_RETURN(tkl_pwm_init(ch, &cfg));
    return tkl_pwm_start(ch);
}

/**
 * @brief Scale TAL duty (0..FRONTLIGHT_DUTY_FULL) into the adapter's 12-bit
 *        LEDC window, mirroring the conversion tkl_pwm_init() applies.
 * @param[in] duty TAL duty in hundredths of a percent.
 * @return Duty in 12-bit LEDC units (0..FRONTLIGHT_LEDC_FULL).
 */
static uint32_t __to_ledc_duty(uint32_t duty)
{
    return duty * FRONTLIGHT_LEDC_FULL / FRONTLIGHT_DUTY_FULL;
}

/**
 * @brief Push one LEDC duty, tolerating the adapter's OPRT_NOT_SUPPORTED
 *        stub return (the hardware write still happens).
 * @param[in] ch  TAL PWM channel.
 * @param[in] duty 12-bit LEDC duty.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __duty_set(TUYA_PWM_NUM_E ch, uint32_t duty)
{
    OPERATE_RET rt = tkl_pwm_duty_set(ch, duty);

    if ((OPRT_OK != rt) && (OPRT_NOT_SUPPORTED != rt)) {
        return rt;
    }
    return OPRT_OK;
}

/**
 * @brief Apply the current brightness/warmth mix to both channels.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __apply(void)
{
    OPERATE_RET rt        = OPRT_OK; /* used by TUYA_CALL_ERR_RETURN */
    uint32_t    total     = (uint32_t)s_fl_brightness * FRONTLIGHT_DUTY_FULL / 100U;
    uint32_t    warm_duty = total * s_fl_warmth / 100U;
    uint32_t    cool_duty = total - warm_duty;

    TUYA_CALL_ERR_RETURN(__duty_set(FRONTLIGHT_PWM_WARM, __to_ledc_duty(warm_duty)));
    return __duty_set(FRONTLIGHT_PWM_COOL, __to_ledc_duty(cool_duty));
}

OPERATE_RET xteink_x4_pro_frontlight_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_fl_inited) {
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(__channel_init(FRONTLIGHT_PWM_COOL, X4PRO_FRONTLIGHT_PIN_COOL));
    TUYA_CALL_ERR_RETURN(__channel_init(FRONTLIGHT_PWM_WARM, X4PRO_FRONTLIGHT_PIN_WARM));

    s_fl_inited      = TRUE;
    s_fl_brightness  = FRONTLIGHT_DEFAULT_BRIGHTNESS;
    s_fl_warmth      = FRONTLIGHT_DEFAULT_WARMTH;
    return __apply();
}

OPERATE_RET xteink_x4_pro_frontlight_set_brightness(uint8_t percent)
{
    if (!s_fl_inited) {
        return OPRT_COM_ERROR;
    }
    if (percent > 100U) {
        percent = 100U;
    }

    s_fl_brightness = percent;
    return __apply();
}

OPERATE_RET xteink_x4_pro_frontlight_set_warmth(uint8_t percent)
{
    if (!s_fl_inited) {
        return OPRT_COM_ERROR;
    }
    if (percent > 100U) {
        percent = 100U;
    }

    s_fl_warmth = percent;
    return __apply();
}

OPERATE_RET xteink_x4_pro_frontlight_get(uint8_t *brightness, uint8_t *warmth)
{
    if (!s_fl_inited) {
        return OPRT_COM_ERROR;
    }

    if (NULL != brightness) {
        *brightness = s_fl_brightness;
    }
    if (NULL != warmth) {
        *warmth = s_fl_warmth;
    }

    return OPRT_OK;
}
