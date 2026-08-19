/**
 * @file xteink_x4_pro_buttons.h
 * @brief Xteink X4 Pro digital button driver (tkl_gpio only).
 * @version 0.1
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __XTEINK_X4_PRO_BUTTONS_H__
#define __XTEINK_X4_PRO_BUTTONS_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Logical button bits reported by xteink_x4_pro_buttons_get_state().
 *        A bit is set while the corresponding key is held (active-LOW keys).
 */
#define X4PRO_BTN_LEFT  (1U << 0)
#define X4PRO_BTN_RIGHT (1U << 1)
#define X4PRO_BTN_POWER (1U << 2)

/**
 * @brief Initialize the three digital buttons as pull-up inputs.
 *
 * Confirmed on hardware: plain active-low GPIO buttons (Left=GPIO0,
 * Right=GPIO7, Power=GPIO3), NOT the OEM ADC ladder. GPIO0 is a boot strap —
 * fine as a key as long as it is not held during reset.
 *
 * @return OPRT_OK on success.
 */
OPERATE_RET xteink_x4_pro_buttons_init(void);

/**
 * @brief Raw bitmask of pressed buttons.
 * @param[out] state non-NULL receives the bitmask (X4PRO_BTN_* bits).
 * @return OPRT_OK on success.
 */
OPERATE_RET xteink_x4_pro_buttons_get_state(uint8_t *state);

#ifdef __cplusplus
}
#endif

#endif /* __XTEINK_X4_PRO_BUTTONS_H__ */
