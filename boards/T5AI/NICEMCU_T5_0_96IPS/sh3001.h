/**
 * @file sh3001.h
 * @brief SH3001 6-axis IMU driver API (NiceMCU-T5-0.96IPS board)
 * @version 0.1
 * @date 2026-08-10
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __SH3001_H__
#define __SH3001_H__

#include "tuya_cloud_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* 7-bit I2C address when SDO is pulled to GND */
#define SH3001_I2C_ADDR_LOW  0x36
/* 7-bit I2C address when SDO is pulled to VDDIO */
#define SH3001_I2C_ADDR_HIGH 0x37

#define SH3001_CHIP_ID_VAL   0x61

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    float acc_x; /* g */
    float acc_y;
    float acc_z;
    float gyr_x; /* dps */
    float gyr_y;
    float gyr_z;
} sh3001_data_t;

typedef struct {
    TUYA_I2C_NUM_E i2c_port;
    uint8_t i2c_addr;
    float acc_lsb_div; /* LSB/g */
    float gyr_lsb_div; /* LSB/dps */
    bool initialized;
} sh3001_dev_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize SH3001 on the given I2C port
 * @param[in,out] dev device handle
 * @param[in] i2c_port I2C controller index
 * @param[in] i2c_addr 7-bit I2C address
 * @return OPRT_OK on success
 */
OPERATE_RET sh3001_init(sh3001_dev_t *dev, TUYA_I2C_NUM_E i2c_port, uint8_t i2c_addr);

/**
 * @brief Read accelerometer data in g
 * @param[in] dev device handle
 * @param[out] x X-axis acceleration
 * @param[out] y Y-axis acceleration
 * @param[out] z Z-axis acceleration
 * @return OPRT_OK on success
 */
OPERATE_RET sh3001_read_accel(sh3001_dev_t *dev, float *x, float *y, float *z);

/**
 * @brief Read gyroscope data in dps
 * @param[in] dev device handle
 * @param[out] x X-axis angular rate
 * @param[out] y Y-axis angular rate
 * @param[out] z Z-axis angular rate
 * @return OPRT_OK on success
 */
OPERATE_RET sh3001_read_gyro(sh3001_dev_t *dev, float *x, float *y, float *z);

/**
 * @brief Read accel + gyro into one structure
 * @param[in] dev device handle
 * @param[out] data sensor sample
 * @return OPRT_OK on success
 */
OPERATE_RET sh3001_read_sensor_data(sh3001_dev_t *dev, sh3001_data_t *data);

/**
 * @brief Read CHIP_ID register
 * @param[in] dev device handle
 * @param[out] chip_id CHIP_ID value
 * @return OPRT_OK on success
 */
OPERATE_RET sh3001_get_chip_id(sh3001_dev_t *dev, uint8_t *chip_id);

#ifdef __cplusplus
}
#endif

#endif /* __SH3001_H__ */
