/**
 * @file board_bmi270_api.h
 * @brief BMI270 IMU sensor driver API for M5Stack StickS3 board.
 *
 * The BMI270 is powered by 3V3_L1 (PMIC LDO) and shares the internal I2C1 bus
 * (SCL = G48, SDA = G47) with the ES8311 codec and M5PM1 PMIC.
 *
 * @version 0.1
 * @date 2026-07-27
 * @copyright Copyright (c) Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_BMI270_API_H__
#define __BOARD_BMI270_API_H__

#include "tuya_cloud_types.h"
#include "bmi2_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Types
 * --------------------------------------------------------------------------- */
/**
 * @brief BMI270 sensor data (accelerometer + gyroscope + temperature).
 */
typedef struct {
    float acc_x;    /**< Accelerometer X-axis (m/s^2) */
    float acc_y;    /**< Accelerometer Y-axis (m/s^2) */
    float acc_z;    /**< Accelerometer Z-axis (m/s^2) */
    float gyr_x;    /**< Gyroscope X-axis (dps) */
    float gyr_y;    /**< Gyroscope Y-axis (dps) */
    float gyr_z;    /**< Gyroscope Z-axis (dps) */
    int16_t temp;   /**< Temperature (raw) */
} bmi270_sensor_data_t;

/**
 * @brief BMI270 runtime configuration.
 */
typedef struct {
    uint8_t acc_range;  /**< Accelerometer range */
    uint8_t gyr_range;  /**< Gyroscope range */
    uint8_t acc_odr;    /**< Accelerometer output data rate */
    uint8_t gyr_odr;    /**< Gyroscope output data rate */
    uint8_t power_mode; /**< Power mode */
} bmi270_config_t;

/**
 * @brief BMI270 device handle.
 */
typedef struct {
    TUYA_I2C_NUM_E i2c_port;    /**< I2C port number */
    uint8_t i2c_addr;           /**< I2C device address */
    bmi270_config_t config;     /**< Sensor configuration */
    bool initialized;           /**< Initialization status */
} bmi270_dev_t;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Initialize and register the BMI270 sensor.
 * @return OPRT_OK on success, error code on failure.
 * @note 3V3_L1 must be enabled before calling this (done in power init).
 */
OPERATE_RET board_bmi270_register(void);

/**
 * @brief De-initialize the BMI270 sensor.
 * @param[in] dev Pointer to BMI270 device handle.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_bmi270_deinit(bmi270_dev_t *dev);

/**
 * @brief Read all sensor data (accel + gyro + temp).
 * @param[in] dev Pointer to BMI270 device handle.
 * @param[out] data Pointer to store sensor data.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_bmi270_read_data(bmi270_dev_t *dev, bmi270_sensor_data_t *data);

/**
 * @brief Read accelerometer data.
 * @param[in] dev Pointer to BMI270 device handle.
 * @param[out] acc_x X-axis acceleration (m/s^2).
 * @param[out] acc_y Y-axis acceleration (m/s^2).
 * @param[out] acc_z Z-axis acceleration (m/s^2).
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_bmi270_read_accel(bmi270_dev_t *dev, float *acc_x, float *acc_y, float *acc_z);

/**
 * @brief Read gyroscope data.
 * @param[in] dev Pointer to BMI270 device handle.
 * @param[out] gyr_x X-axis angular velocity (dps).
 * @param[out] gyr_y Y-axis angular velocity (dps).
 * @param[out] gyr_z Z-axis angular velocity (dps).
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_bmi270_read_gyro(bmi270_dev_t *dev, float *gyr_x, float *gyr_y, float *gyr_z);

/**
 * @brief Read temperature data.
 * @param[in] dev Pointer to BMI270 device handle.
 * @param[out] temp Raw temperature value.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_bmi270_read_temp(bmi270_dev_t *dev, int16_t *temp);

/**
 * @brief Set BMI270 power mode.
 * @param[in] dev Pointer to BMI270 device handle.
 * @param[in] power_mode true for advanced power save, false for normal.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_bmi270_set_power_mode(bmi270_dev_t *dev, bool power_mode);

/**
 * @brief Force reset the BMI270 sensor.
 * @param[in] dev Pointer to BMI270 device handle.
 * @return OPRT_OK on success, error code on failure.
 */
OPERATE_RET board_bmi270_force_reset(bmi270_dev_t *dev);

/**
 * @brief Scan the I2C bus for the BMI270 device.
 * @param[in] port I2C port number.
 * @return OPRT_OK if found, error code otherwise.
 */
OPERATE_RET board_bmi270_scan_i2c(TUYA_I2C_NUM_E port);

/**
 * @brief Get handle to the global BMI270 device instance.
 * @return Pointer to BMI270 device handle.
 */
bmi270_dev_t *board_bmi270_get_handle(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_BMI270_API_H__ */
