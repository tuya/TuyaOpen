/**
 * @file tdd_tp_cst816x.h
 * @brief CST816X series capacitive tp controller driver interface definitions
 *
 * This header file defines the interface for the CST816X series capacitive tp
 * controller drivers in the TDD layer. It includes register definitions, configuration
 * parameters, and function prototypes for CST816X family tp controllers (CST816S,
 * CST816D, CST816T, CST820, CST716) with single-point tp and gesture support.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_TP_CST816X_H__
#define __TDD_TP_CST816X_H__

#include "tuya_cloud_types.h"
#include "tdl_tp_driver.h"
#include "tdd_tp_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define CST816_ADDR                    0x15

#define CST816_GESTURE_NONE            0x00
#define CST816_GESTURE_MOVE_UP         0x01
#define CST816_GESTURE_MOVE_DN         0x02
#define CST816_GESTURE_MOVE_LT         0x03
#define CST816_GESTURE_MOVE_RT         0x04
#define CST816_GESTURE_CLICK           0x05
#define CST816_GESTURE_DBLCLICK        0x0B
#define CST816_GESTURE_LONGPRESS       0x0C

#define REG_STATUS                     0x00
#define REG_GESTURE_ID                 0x01
#define REG_FINGER_NUM                 0x02
#define REG_XPOS_HIGH                  0x03
#define REG_XPOS_LOW                   0x04
#define REG_YPOS_HIGH                  0x05
#define REG_YPOS_LOW                   0x06
#define REG_CHIP_ID                    0xA7
#define REG_FW_VERSION                 0xA9
#define REG_IRQ_CTL                    0xFA
#define REG_DIS_AUTOSLEEP              0xFE

#define IRQ_EN_MOTION                  0x70

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_GPIO_NUM_E  rst_pin;
    TUYA_GPIO_NUM_E  intr_pin;
    TDD_TP_I2C_CFG_T i2c_cfg;
    TDD_TP_CONFIG_T  tp_cfg;
} TDD_TP_CST816X_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tdd_tp_i2c_cst816x_register(char *name, TDD_TP_CST816X_INFO_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_TP_CST816X_H__ */
