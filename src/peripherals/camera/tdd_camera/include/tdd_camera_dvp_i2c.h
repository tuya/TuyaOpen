/**
 * @file tdd_camera_dvp_i2c.h
 * @brief DVP camera I2C interface header
 *
 * This header file defines the I2C interface for DVP (Digital Video Port)
 * camera sensors. It provides functions for I2C initialization, register
 * read and write operations used to configure camera sensors.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
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
/**
 * @brief Initialize I2C interface for DVP camera sensor
 * @param cfg Pointer to I2C configuration structure containing port, clock, and data pins
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET tdd_dvp_i2c_init(DVP_I2C_CFG_T *cfg);

/**
 * @brief Read data from camera sensor register via I2C
 * @param cfg Pointer to I2C register configuration structure containing port,
 *            device address, register address, and register width flag
 * @param read_len Number of bytes to read (maximum DVP_I2C_READ_MAX_LEN)
 * @param buf Pointer to buffer to store read data
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET tdd_dvp_i2c_read(DVP_I2C_REG_CFG_T *cfg, uint16_t read_len, uint8_t *buf);

/**
 * @brief Write data to camera sensor register via I2C
 * @param cfg Pointer to I2C register configuration structure containing port,
 *            device address, register address, and register width flag
 * @param write_len Number of bytes to write (maximum DVP_I2C_WRITE_MAX_LEN)
 * @param buf Pointer to buffer containing data to write
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET tdd_dvp_i2c_write(DVP_I2C_REG_CFG_T *cfg, uint16_t write_len, uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_CAMERA_DVP_I2C_H__ */
