/**
 * @file tdd_motion_i2c.h
 * @brief Shared I2C helpers for motion sensor TDD drivers.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef _TDD_MOTION_I2C_H_
#define _TDD_MOTION_I2C_H_

#include "tdl_motion_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Apply the board-selected I2C pins before the peripheral is opened.
 */
OPERATE_RET tdd_motion_i2c_pinmux_config(const TDD_MOTION_I2C_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* _TDD_MOTION_I2C_H_ */
