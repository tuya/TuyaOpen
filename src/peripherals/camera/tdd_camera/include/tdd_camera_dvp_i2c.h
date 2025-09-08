/**
 * @file tdd_camera_dvp_i2c.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_CAMERA_DVP_I2C_H__
#define __TDD_CAMERA_DVP_I2C_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define DVP_I2C_READ_MAX_LEN         (8)
#define DVP_I2C_WRITE_MAX_LEN        (8)
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_I2C_NUM_E  port;
    TUYA_PIN_NAME_E clk;
    TUYA_PIN_NAME_E sda;
}DVP_I2C_CFG_T;

typedef struct {
    TUYA_I2C_NUM_E port;
    uint8_t        addr;
    uint16_t       reg;
    uint8_t        is_16_reg;
}DVP_I2C_REG_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tdd_dvp_i2c_init(DVP_I2C_CFG_T *cfg);

OPERATE_RET tdd_dvp_i2c_read(DVP_I2C_REG_CFG_T *cfg, uint16_t read_len, uint8_t *buf);

OPERATE_RET tdd_dvp_i2c_write(DVP_I2C_REG_CFG_T *cfg, uint16_t write_len, uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_DVP_I2C_H__ */
