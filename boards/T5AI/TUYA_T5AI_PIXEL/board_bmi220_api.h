/**
 * @file board_bmi220_api.h
 * @author Tuya Inc.
 * @brief BMI220 (chip ID 0x26) sensor driver API for TUYA_T5AI_PIXEL board.
 *        Uses Bosch BMI2 library with patched chip ID acceptance.
 *        Compatible with BMI270 config file for initialization.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_BMI220_API_H__
#define __BOARD_BMI220_API_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/* BMI220 I2C Configuration (same bus as BMI270) */
#define BMI220_I2C_PORT     TUYA_I2C_NUM_0
#define BMI220_I2C_ADDR     0x68 /* Primary I2C address (SDO = GND) */
#define BMI220_I2C_ADDR_ALT 0x69 /* Secondary I2C address (SDO = VDDIO) */

/* BMI220 Chip ID (differs from BMI270's 0x24) */
#define BMI220_CHIP_ID 0x26

/**
 * @brief BMI220 sensor data structure (same layout as BMI270 for compatibility)
 */
typedef struct {
    float acc_x;  /* Accelerometer X-axis data (m/s^2) */
    float acc_y;  /* Accelerometer Y-axis data (m/s^2) */
    float acc_z;  /* Accelerometer Z-axis data (m/s^2) */
    float gyr_x;  /* Gyroscope X-axis data (deg/s) */
    float gyr_y;  /* Gyroscope Y-axis data (deg/s) */
    float gyr_z;  /* Gyroscope Z-axis data (deg/s) */
    int16_t temp; /* Temperature data */
} bmi220_sensor_data_t;

/**
 * @brief BMI220 device structure
 */
typedef struct {
    TUYA_I2C_NUM_E i2c_port; /* I2C port number */
    uint8_t i2c_addr;        /* I2C device address */
    uint8_t chip_id;         /* Read-back chip ID */
    bool initialized;        /* Initialization status */
} bmi220_dev_t;

/***********************************************************
************************function define**********************
***********************************************************/

OPERATE_RET board_bmi220_init(bmi220_dev_t *dev);
OPERATE_RET board_bmi220_register(void);
OPERATE_RET board_bmi220_deinit(bmi220_dev_t *dev);
OPERATE_RET board_bmi220_read_data(bmi220_dev_t *dev, bmi220_sensor_data_t *data);
OPERATE_RET board_bmi220_read_accel(bmi220_dev_t *dev, float *acc_x, float *acc_y, float *acc_z);
OPERATE_RET board_bmi220_read_gyro(bmi220_dev_t *dev, float *gyr_x, float *gyr_y, float *gyr_z);
bmi220_dev_t *board_bmi220_get_handle(void);
bool board_bmi220_is_ready(bmi220_dev_t *dev);
OPERATE_RET board_bmi220_scan_i2c(TUYA_I2C_NUM_E port);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_BMI220_API_H__ */
