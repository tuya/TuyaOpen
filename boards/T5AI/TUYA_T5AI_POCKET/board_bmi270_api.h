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
#include "tkl_i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/* BMI270 I2C Configuration */
#define BMI270_I2C_PORT              TUYA_I2C_NUM_0
#define BMI270_I2C_ADDR              0x68  /* BMI270 I2C address (ADDR pin = 0) */
#define BMI270_I2C_ADDR_ALT          0x69  /* BMI270 I2C address (ADDR pin = 1) */

/* BMI270 Chip ID */
#define BMI270_CHIP_ID               0x24

/* BMI270 Register Addresses (from datasheet) */
#define BMI270_REG_CHIP_ID           0x00
#define BMI270_REG_ERR_REG           0x02
#define BMI270_REG_STATUS            0x03
#define BMI270_REG_DATA_0            0x04
#define BMI270_REG_DATA_1            0x05
#define BMI270_REG_DATA_2            0x06
#define BMI270_REG_DATA_3            0x07
#define BMI270_REG_DATA_4            0x08
#define BMI270_REG_DATA_5            0x09
#define BMI270_REG_DATA_6            0x0A
#define BMI270_REG_DATA_7            0x0B
#define BMI270_REG_DATA_8            0x0C
#define BMI270_REG_DATA_9            0x0D
#define BMI270_REG_DATA_10           0x0E
#define BMI270_REG_DATA_11           0x0F

/* BMI270 Power and Configuration Registers */
#define BMI270_REG_PWR_CTRL          0x7C
#define BMI270_REG_PWR_CONF          0x7C
#define BMI270_REG_CMD               0x7E

/* BMI270 Feature Configuration Registers */
#define BMI270_REG_FEATURE_CFG       0x30
#define BMI270_REG_INIT_CTRL         0x31
#define BMI270_REG_INIT_DATA         0x32

/* BMI270 Sensor Configuration Registers */
#define BMI270_REG_ACCEL_CONFIG      0x40
#define BMI270_REG_GYRO_CONFIG       0x42
#define BMI270_REG_ACCEL_RANGE       0x41
#define BMI270_REG_GYRO_RANGE        0x43

/* BMI270 Commands */
#define BMI270_CMD_SOFT_RESET        0xB6
#define BMI270_CMD_FEATURE_CFG       0x02

/* BMI270 Power Modes */
#define BMI270_POWER_MODE_SUSPEND    0x00
#define BMI270_POWER_MODE_CONFIG     0x01
#define BMI270_POWER_MODE_LOW_POWER  0x02
#define BMI270_POWER_MODE_NORMAL     0x03

/* BMI270 Accelerometer Range */
#define BMI270_ACC_RANGE_2G          0x00
#define BMI270_ACC_RANGE_4G          0x01
#define BMI270_ACC_RANGE_8G          0x02
#define BMI270_ACC_RANGE_16G         0x03

/* BMI270 Gyroscope Range */
#define BMI270_GYR_RANGE_2000DPS     0x00
#define BMI270_GYR_RANGE_1000DPS     0x01
#define BMI270_GYR_RANGE_500DPS      0x02
#define BMI270_GYR_RANGE_250DPS      0x03
#define BMI270_GYR_RANGE_125DPS      0x04

/* BMI270 Output Data Rates */
#define BMI270_ODR_0_78HZ            0x01
#define BMI270_ODR_1_56HZ            0x02
#define BMI270_ODR_3_12HZ            0x03
#define BMI270_ODR_6_25HZ            0x04
#define BMI270_ODR_12_5HZ            0x05
#define BMI270_ODR_25HZ              0x06
#define BMI270_ODR_50HZ              0x07
#define BMI270_ODR_100HZ             0x08
#define BMI270_ODR_200HZ             0x09
#define BMI270_ODR_400HZ             0x0A
#define BMI270_ODR_800HZ             0x0B
#define BMI270_ODR_1600HZ            0x0C
#define BMI270_ODR_3200HZ            0x0D

/***********************************************************
************************data structure**********************
***********************************************************/

/**
 * @brief BMI270 sensor data structure
 */
typedef struct {
    int16_t acc_x;    /* Accelerometer X-axis data */
    int16_t acc_y;    /* Accelerometer Y-axis data */
    int16_t acc_z;    /* Accelerometer Z-axis data */
    int16_t gyr_x;    /* Gyroscope X-axis data */
    int16_t gyr_y;    /* Gyroscope Y-axis data */
    int16_t gyr_z;    /* Gyroscope Z-axis data */
    int16_t temp;     /* Temperature data */
} bmi270_sensor_data_t;

/**
 * @brief BMI270 configuration structure
 */
typedef struct {
    uint8_t acc_range;    /* Accelerometer range */
    uint8_t gyr_range;    /* Gyroscope range */
    uint8_t acc_odr;      /* Accelerometer output data rate */
    uint8_t gyr_odr;      /* Gyroscope output data rate */
    uint8_t power_mode;   /* Power mode */
} bmi270_config_t;

/**
 * @brief BMI270 device structure
 */
typedef struct {
    TUYA_I2C_NUM_E i2c_port;     /* I2C port number */
    uint8_t i2c_addr;             /* I2C device address */
    bmi270_config_t config;       /* Sensor configuration */
    bool initialized;              /* Initialization status */
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
 * @brief Deinitialize BMI270 sensor
 * @param dev Pointer to BMI270 device structure
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_deinit(bmi270_dev_t *dev);

/**
 * @brief Configure BMI270 sensor
 * @param dev Pointer to BMI270 device structure
 * @param config Pointer to configuration structure
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_config(bmi270_dev_t *dev, const bmi270_config_t *config);

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
OPERATE_RET board_bmi270_read_accel(bmi270_dev_t *dev, int16_t *acc_x, int16_t *acc_y, int16_t *acc_z);

/**
 * @brief Read gyroscope data from BMI270
 * @param dev Pointer to BMI270 device structure
 * @param gyr_x Pointer to store X-axis angular velocity
 * @param gyr_y Pointer to store Y-axis angular velocity
 * @param gyr_z Pointer to store Z-axis angular velocity
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_read_gyro(bmi270_dev_t *dev, int16_t *gyr_x, int16_t *gyr_y, int16_t *gyr_z);

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
OPERATE_RET board_bmi270_set_power_mode(bmi270_dev_t *dev, uint8_t power_mode);

/**
 * @brief Force reset BMI270 sensor to known state
 * @param dev Pointer to BMI270 device structure
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_force_reset(bmi270_dev_t *dev);

/**
 * @brief Check if BMI270 is ready for data reading
 * @param dev Pointer to BMI270 device structure
 * @return true if ready, false otherwise
 */
bool board_bmi270_is_ready(bmi270_dev_t *dev);

/**
 * @brief Register BMI270 driver
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_register(void);

/**
 * @brief Scan I2C bus for BMI270 device
 * @param port I2C port number
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_scan_i2c(TUYA_I2C_NUM_E port);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_BMI270_API_H__ */

