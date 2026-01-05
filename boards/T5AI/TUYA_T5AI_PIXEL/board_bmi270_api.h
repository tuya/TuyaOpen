/**
 * @file board_bmi270_api.h
 * @author Tuya Inc.
 * @brief BMI270 sensor driver API for TUYA_T5AI_POCKET board
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_BMI270_API_H__
#define __BOARD_BMI270_API_H__

#include "tuya_cloud_types.h"
#include "bmi2_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/* BMI270 I2C Configuration */
#define BMI270_I2C_PORT     TUYA_I2C_NUM_0
#define BMI270_I2C_ADDR     BMI2_I2C_PRIM_ADDR /* BMI270 I2C address (ADDR pin = 0) */
#define BMI270_I2C_ADDR_ALT BMI2_I2C_SEC_ADDR  /* BMI270 I2C address (ADDR pin = 1) */

/**
 * @brief BMI270 sensor data structure
 */
typedef struct {
    float acc_x;  /* Accelerometer X-axis data */
    float acc_y;  /* Accelerometer Y-axis data */
    float acc_z;  /* Accelerometer Z-axis data */
    float gyr_x;  /* Gyroscope X-axis data */
    float gyr_y;  /* Gyroscope Y-axis data */
    float gyr_z;  /* Gyroscope Z-axis data */
    int16_t temp; /* Temperature data */
} bmi270_sensor_data_t;

/**
 * @brief BMI270 configuration structure
 */
typedef struct {
    uint8_t acc_range;  /* Accelerometer range */
    uint8_t gyr_range;  /* Gyroscope range */
    uint8_t acc_odr;    /* Accelerometer output data rate */
    uint8_t gyr_odr;    /* Gyroscope output data rate */
    uint8_t power_mode; /* Power mode */
} bmi270_config_t;

/**
 * @brief BMI270 device structure
 */
typedef struct {
    TUYA_I2C_NUM_E i2c_port; /* I2C port number */
    uint8_t i2c_addr;        /* I2C device address */
    bmi270_config_t config;  /* Sensor configuration */
    bool initialized;        /* Initialization status */
} bmi270_dev_t;

/***********************************************************
************************function define**********************
***********************************************************/

/**
 * @brief Initialize BMI270 sensor
 * @param dev Pointer to BMI270 device structure
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_init(bmi270_dev_t *dev);

/**
 * @brief Register BMI270 driver
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_register(void);

/**
 * @brief Deinitialize BMI270 sensor
 * @param dev Pointer to BMI270 device structure
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_deinit(bmi270_dev_t *dev);

/**
 * @brief Read sensor data from BMI270
 * @param dev Pointer to BMI270 device structure
 * @param data Pointer to store sensor data
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_read_data(bmi270_dev_t *dev, bmi270_sensor_data_t *data);

/**
 * @brief Read accelerometer data from BMI270
 * @param dev Pointer to BMI270 device structure
 * @param acc_x Pointer to store X-axis acceleration
 * @param acc_y Pointer to store Y-axis acceleration
 * @param acc_z Pointer to store Z-axis acceleration
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_read_accel(bmi270_dev_t *dev, float *acc_x, float *acc_y, float *acc_z);

/**
 * @brief Read gyroscope data from BMI270
 * @param dev Pointer to BMI270 device structure
 * @param gyr_x Pointer to store X-axis angular velocity
 * @param gyr_y Pointer to store Y-axis angular velocity
 * @param gyr_z Pointer to store Z-axis angular velocity
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_read_gyro(bmi270_dev_t *dev, float *gyr_x, float *gyr_y, float *gyr_z);

/**
 * @brief Read temperature data from BMI270
 * @param dev Pointer to BMI270 device structure
 * @param temp Pointer to store temperature data
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_read_temp(bmi270_dev_t *dev, int16_t *temp);

/**
 * @brief Set power mode of BMI270
 * @param dev Pointer to BMI270 device structure
 * @param power_mode Power mode to set
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_set_power_mode(bmi270_dev_t *dev, bool power_mode);

/**
 * @brief Force reset BMI270 sensor to known state
 * @param dev Pointer to BMI270 device structure
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_force_reset(bmi270_dev_t *dev);

/**
 * @brief Scan I2C bus for BMI270 device
 * @param port I2C port number
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_scan_i2c(TUYA_I2C_NUM_E port);

/**
 * @brief Get handle to global BMI270 device instance
 * @return Pointer to BMI270 device structure
 */
bmi270_dev_t *board_bmi270_get_handle();

/**
 * @brief Check if BMI270 sensor is ready
 * @param dev Pointer to BMI270 device structure
 * @return true if sensor is initialized and ready, false otherwise
 */
bool board_bmi270_is_ready(bmi270_dev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_BMI270_API_H__ */
