/**
 * @file xteink_x4_buttons.h
 * @brief X4 resistor-ladder button bitmask (community-sdk InputManager indices).
 * @version 0.1
 * @date 2026-04-30
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __XTEINK_X4_BUTTONS_H__
#define __XTEINK_X4_BUTTONS_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define X4_BTN_BACK    (1U << 0)
#define X4_BTN_CONFIRM (1U << 1)
#define X4_BTN_LEFT    (1U << 2)
#define X4_BTN_RIGHT   (1U << 3)
#define X4_BTN_UP      (1U << 4)
#define X4_BTN_DOWN    (1U << 5)
#define X4_BTN_POWER   (1U << 6)

#ifdef __cplusplus
}
#endif

#endif /* __XTEINK_X4_BUTTONS_H__ */
