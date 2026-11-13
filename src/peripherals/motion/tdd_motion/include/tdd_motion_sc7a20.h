/**
 * @file tdd_motion_sc7a20.h
 * @brief SC7A20 3-axis accelerometer TDD registration.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef _TDD_MOTION_SC7A20_H_
#define _TDD_MOTION_SC7A20_H_

#include "tdl_motion_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register an SC7A20 accelerometer.
 *
 * Supported capabilities: TDL_MOTION_CAP_ACCEL.
 */
OPERATE_RET tdd_motion_sc7a20_register(char *name,
                                       const TDD_MOTION_I2C_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* _TDD_MOTION_SC7A20_H_ */
