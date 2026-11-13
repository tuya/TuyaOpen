/**
 * @file tdd_motion_bmi270.h
 * @brief BMI270 6-axis motion sensor TDD registration.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef _TDD_MOTION_BMI270_H_
#define _TDD_MOTION_BMI270_H_

#include "tdl_motion_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register a BMI270 accelerometer and gyroscope device.
 *
 * Supported capabilities: TDL_MOTION_CAP_ACCEL, TDL_MOTION_CAP_GYRO and
 * TDL_MOTION_CAP_TEMP.
 */
OPERATE_RET tdd_motion_bmi270_register(char *name,
                                       const TDD_MOTION_I2C_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* _TDD_MOTION_BMI270_H_ */
