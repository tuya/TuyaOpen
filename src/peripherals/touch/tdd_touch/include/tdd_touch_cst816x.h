/**
 * @file tdd_touch_cst816x.h
 * @brief CST816X series capacitive touch controller driver interface definitions
 *
 * This header file defines the interface for the CST816X series capacitive touch
 * controller drivers in the TDD layer. It includes register definitions, configuration
 * parameters, and function prototypes for CST816X family touch controllers (CST816S,
 * CST816D, CST816T, CST820, CST716) with single-point touch and gesture support.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_TOUCH_CST816X_H__
#define __TDD_TOUCH_CST816X_H__

#include "tuya_cloud_types.h"
#include "tdd_touch_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define CST816_ADDR 0x15

#define REG_STATUS        0x00
#define REG_TOUCH_NUM     0x02
#define REG_XPOS_HIGH     0x03
#define REG_XPOS_LOW      0x04
#define REG_YPOS_HIGH     0x05
#define REG_YPOS_LOW      0x06
#define REG_CHIP_ID       0xA7
#define REG_FW_VERSION    0xA9
#define REG_IRQ_CTL       0xFA
#define REG_DIS_AUTOSLEEP 0xFE

#define IRQ_EN_MOTION 0x70

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * Whether the graphic is filled
 **/
typedef enum {
    CST816S_POINT_MODE = 1,
    CST816S_GESTURE_MODE,
    CST816S_ALL_MODE,
} CST816X_MODE;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tdd_touch_i2c_cst816x_register(char *name, TDD_TOUCH_I2C_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_TOUCH_CST816X_H__ */
