/**
 * @file tdd_touch_cst816x.h
 * @brief CST9217 series capacitive touch controller driver interface definitions
 *
 * This header file defines the interface for the CST9217 series capacitive touch
 * controller drivers in the TDD layer. It includes register definitions, configuration
 * parameters.
 *
 * @copyright Copyright (c) 2021-2025 Waveshare Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_TOUCH_CST9217_H__
#define __TDD_TOUCH_CST9217_H__

#include "tuya_cloud_types.h"
#include "tdd_touch_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define CST9217_ADDR 0x5A

// Register address
#define CST9217_CMD_KEY_TOUCH_ADDR 0xD005

// Bit masks
#define KEY_TOUCH_FLAG_MASK   (1 << 7)    // bit7
#define FINGER_NUM_MASK       (0x7F)      // bit6~bit0

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * Whether the graphic is filled
 **/
typedef enum {
    CST9217_POINT_MODE = 1,
    CST9217_GESTURE_MODE,
    CST9217_ALL_MODE,
} CST9217_MODE;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tdd_touch_i2c_cst9217_register(char *name, TDD_TOUCH_I2C_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_TOUCH_CST9217_H__ */
