/**
 * @file xteink_x4_pro_frontlight.h
 * @brief Xteink X4 Pro dual warm/cold frontlight driver (tkl_pwm only).
 * @version 0.1
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __XTEINK_X4_PRO_FRONTLIGHT_H__
#define __XTEINK_X4_PRO_FRONTLIGHT_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the warm/cold frontlight PWM pair (10 kHz, active-HIGH).
 * @return OPRT_OK on success.
 */
OPERATE_RET xteink_x4_pro_frontlight_init(void);

/**
 * @brief Set overall frontlight brightness.
 * @param[in] percent 0..100 total brightness, split between the cool and
 *                    warm LEDs according to the current warmth.
 * @return OPRT_OK on success.
 */
OPERATE_RET xteink_x4_pro_frontlight_set_brightness(uint8_t percent);

/**
 * @brief Set color temperature mix.
 * @param[in] percent 0 = all cool/white, 100 = all warm.
 * @return OPRT_OK on success.
 */
OPERATE_RET xteink_x4_pro_frontlight_set_warmth(uint8_t percent);

/**
 * @brief Read back the current settings.
 * @param[out] brightness 0..100, may be NULL.
 * @param[out] warmth 0..100, may be NULL.
 * @return OPRT_OK on success.
 */
OPERATE_RET xteink_x4_pro_frontlight_get(uint8_t *brightness, uint8_t *warmth);

#ifdef __cplusplus
}
#endif

#endif /* __XTEINK_X4_PRO_FRONTLIGHT_H__ */
