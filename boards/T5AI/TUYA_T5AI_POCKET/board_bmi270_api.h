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
#if 0
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
#define BMI270_REG_PWR_CTRL          0x7D
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


/*! @name BMI2 register addresses */
#define BMI2_CHIP_ID_ADDR                         UINT8_C(0x00)
#define BMI2_STATUS_ADDR                          UINT8_C(0x03)
#define BMI2_AUX_X_LSB_ADDR                       UINT8_C(0x04)
#define BMI2_ACC_X_LSB_ADDR                       UINT8_C(0x0C)
#define BMI2_GYR_X_LSB_ADDR                       UINT8_C(0x12)
#define BMI2_SENSORTIME_ADDR                      UINT8_C(0x18)
#define BMI2_EVENT_ADDR                           UINT8_C(0x1B)
#define BMI2_INT_STATUS_0_ADDR                    UINT8_C(0x1C)
#define BMI2_INT_STATUS_1_ADDR                    UINT8_C(0x1D)
#define BMI2_SC_OUT_0_ADDR                        UINT8_C(0x1E)
#define BMI2_SYNC_COMMAND_ADDR                    UINT8_C(0x1E)
#define BMI2_GYR_CAS_GPIO0_ADDR                   UINT8_C(0x1E)
#define BMI2_INTERNAL_STATUS_ADDR                 UINT8_C(0x21)
#define BMI2_FIFO_LENGTH_0_ADDR                   UINT8_C(0x24)
#define BMI2_FIFO_DATA_ADDR                       UINT8_C(0x26)
#define BMI2_FEAT_PAGE_ADDR                       UINT8_C(0x2F)
#define BMI2_FEATURES_REG_ADDR                    UINT8_C(0x30)
#define BMI2_ACC_CONF_ADDR                        UINT8_C(0x40)
#define BMI2_GYR_CONF_ADDR                        UINT8_C(0x42)
#define BMI2_AUX_CONF_ADDR                        UINT8_C(0x44)
#define BMI2_FIFO_DOWNS_ADDR                      UINT8_C(0x45)
#define BMI2_FIFO_WTM_0_ADDR                      UINT8_C(0x46)
#define BMI2_FIFO_WTM_1_ADDR                      UINT8_C(0x47)
#define BMI2_FIFO_CONFIG_0_ADDR                   UINT8_C(0x48)
#define BMI2_FIFO_CONFIG_1_ADDR                   UINT8_C(0x49)
#define BMI2_AUX_DEV_ID_ADDR                      UINT8_C(0x4B)
#define BMI2_AUX_IF_CONF_ADDR                     UINT8_C(0x4C)
#define BMI2_AUX_RD_ADDR                          UINT8_C(0x4D)
#define BMI2_AUX_WR_ADDR                          UINT8_C(0x4E)
#define BMI2_AUX_WR_DATA_ADDR                     UINT8_C(0x4F)
#define BMI2_INT1_IO_CTRL_ADDR                    UINT8_C(0x53)
#define BMI2_INT2_IO_CTRL_ADDR                    UINT8_C(0x54)
#define BMI2_INT_LATCH_ADDR                       UINT8_C(0x55)
#define BMI2_INT1_MAP_FEAT_ADDR                   UINT8_C(0x56)
#define BMI2_INT2_MAP_FEAT_ADDR                   UINT8_C(0x57)
#define BMI2_INT_MAP_DATA_ADDR                    UINT8_C(0x58)
#define BMI2_INIT_CTRL_ADDR                       UINT8_C(0x59)
#define BMI2_INIT_ADDR_0                          UINT8_C(0x5B)
#define BMI2_INIT_ADDR_1                          UINT8_C(0x5C)
#define BMI2_INIT_DATA_ADDR                       UINT8_C(0x5E)
#define BMI2_AUX_IF_TRIM                          UINT8_C(0x68)
#define BMI2_GYR_CRT_CONF_ADDR                    UINT8_C(0x69)
#define BMI2_NVM_CONF_ADDR                        UINT8_C(0x6A)
#define BMI2_IF_CONF_ADDR                         UINT8_C(0x6B)
#define BMI2_ACC_SELF_TEST_ADDR                   UINT8_C(0x6D)
#define BMI2_GYR_SELF_TEST_AXES_ADDR              UINT8_C(0x6E)
#define BMI2_SELF_TEST_MEMS_ADDR                  UINT8_C(0x6F)
#define BMI2_NV_CONF_ADDR                         UINT8_C(0x70)
#define BMI2_ACC_OFF_COMP_0_ADDR                  UINT8_C(0x71)
#define BMI2_GYR_OFF_COMP_3_ADDR                  UINT8_C(0x74)
#define BMI2_GYR_OFF_COMP_6_ADDR                  UINT8_C(0x77)
#define BMI2_GYR_USR_GAIN_0_ADDR                  UINT8_C(0x78)
#define BMI2_PWR_CONF_ADDR                        UINT8_C(0x7C)
#define BMI2_PWR_CTRL_ADDR                        UINT8_C(0x7D)
#define BMI2_CMD_REG_ADDR                         UINT8_C(0x7E)

/*! @name BMI2 I2C address */
#define BMI2_I2C_PRIM_ADDR                        UINT8_C(0x68)
#define BMI2_I2C_SEC_ADDR                         UINT8_C(0x69)

/*! @name BMI2 Commands */
#define BMI2_G_TRIGGER_CMD                        UINT8_C(0x02)
#define BMI2_USR_GAIN_CMD                         UINT8_C(0x03)
#define BMI2_NVM_PROG_CMD                         UINT8_C(0xA0)
#define BMI2_SOFT_RESET_CMD                       UINT8_C(0xB6)
#define BMI2_FIFO_FLUSH_CMD                       UINT8_C(0xB0)
/***********************************************************
************************data structure**********************
***********************************************************/
#endif
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
 * @brief Register BMI270 driver
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_register(void);

#if 0
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
 * @brief Scan I2C bus for BMI270 device
 * @param port I2C port number
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_scan_i2c(TUYA_I2C_NUM_E port);
#endif
bmi270_dev_t *board_bmi270_get_handle();
#ifdef __cplusplus
}
#endif

#endif /* __BOARD_BMI270_API_H__ */

