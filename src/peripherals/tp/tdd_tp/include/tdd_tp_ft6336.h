/**
 * @file tdd_tp_ft6336.h
 * @brief FT6336 capacitive touch panel controller driver interface definitions
 *
 * This header file defines the interface for the FT6336 capacitive touch panel
 * controller driver in the TDD layer. It includes register definitions, configuration
 * parameters, and function prototypes for FT6336 touch panel controller operations
 * with I2C communication interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_TP_FT6336_H__
#define __TDD_TP_FT6336_H__

#include "tuya_cloud_types.h"
#include "tdl_tp_driver.h"
#include "tdd_tp_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define FT6336_I2C_ADDR 0x38

/* FT6336 Register Addresses */
#define FT6336_REG_DEV_MODE            0x00
#define FT6336_REG_GEST_ID             0x01
#define FT6336_REG_TD_STATUS           0x02
#define FT6336_REG_P1_XH               0x03
#define FT6336_REG_P1_XL               0x04
#define FT6336_REG_P1_YH               0x05
#define FT6336_REG_P1_YL               0x06
#define FT6336_REG_P1_WEIGHT           0x07
#define FT6336_REG_P1_MISC             0x08
#define FT6336_REG_P2_XH               0x09
#define FT6336_REG_P2_XL               0x0A
#define FT6336_REG_P2_YH               0x0B
#define FT6336_REG_P2_YL               0x0C
#define FT6336_REG_P2_WEIGHT           0x0D
#define FT6336_REG_P2_MISC             0x0E
#define FT6336_REG_TH_GROUP            0x80
#define FT6336_REG_TH_DIFF             0x85
#define FT6336_REG_CTRL                0x86
#define FT6336_REG_TIMEENTERMONITOR    0x87
#define FT6336_REG_PERIODACTIVE        0x88
#define FT6336_REG_PERIODMONITOR       0x89
#define FT6336_REG_RADIAN_VALUE        0x91
#define FT6336_REG_OFFSET_LEFT_RIGHT   0x92
#define FT6336_REG_OFFSET_UP_DOWN      0x93
#define FT6336_REG_DISTANCE_LEFT_RIGHT 0x94
#define FT6336_REG_DISTANCE_UP_DOWN    0x95
#define FT6336_REG_DISTANCE_ZOOM       0x96
#define FT6336_REG_LIB_VER_H           0xA1
#define FT6336_REG_LIB_VER_L           0xA2
#define FT6336_REG_CIPHER              0xA3
#define FT6336_REG_G_MODE              0xA4
#define FT6336_REG_PWR_MODE            0xA5
#define FT6336_REG_FIRMID              0xA6
#define FT6336_REG_FOCALTECH_ID        0xA8
#define FT6336_REG_RELEASE_CODE_ID     0xAF
#define FT6336_REG_STATE               0xBC

/* FT6336 Gesture IDs */
#define FT6336_GESTURE_NONE       0x00
#define FT6336_GESTURE_MOVE_UP    0x10
#define FT6336_GESTURE_MOVE_LEFT  0x14
#define FT6336_GESTURE_MOVE_DOWN  0x18
#define FT6336_GESTURE_MOVE_RIGHT 0x1C
#define FT6336_GESTURE_ZOOM_IN    0x48
#define FT6336_GESTURE_ZOOM_OUT   0x49

/* Max detectable simultaneous touch points */
#define FT6336_MAX_TOUCH_POINTS 2

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_GPIO_NUM_E  rst_pin;
    TUYA_GPIO_NUM_E  intr_pin;
    TDD_TP_I2C_CFG_T i2c_cfg;
    TDD_TP_CONFIG_T  tp_cfg;
} TDD_TP_FT6336_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers an FT6336 touch panel device with the TDL layer
 *
 * This function configures and registers a touch panel device for the FT6336
 * capacitive touch controller using the I2C communication protocol.
 *
 * @param name Name of the touch panel device (used for identification).
 * @param cfg Pointer to the FT6336 device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_tp_i2c_ft6336_register(char *name, TDD_TP_FT6336_INFO_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_TP_FT6336_H__ */
