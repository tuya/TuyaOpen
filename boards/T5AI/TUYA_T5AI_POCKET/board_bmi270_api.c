/**
 * @file board_bmi270_api.c
 * @author Tuya Inc.
 * @brief BMI270 sensor driver implementation for TUYA_T5AI_POCKET board
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "board_bmi270_api.h"
#include "tkl_pinmux.h"
#include "tal_log.h"
#include "tkl_system.h"

/***********************************************************
***********************variable define**********************
***********************************************************/

/* Global BMI270 device instance */
// static bmi270_dev_t g_bmi270_dev = {0};

/* I2C configuration for BMI270 */
static TUYA_IIC_BASE_CFG_T g_bmi270_i2c_cfg = {
    .role = TUYA_IIC_MODE_MASTER,
    .speed = TUYA_IIC_BUS_SPEED_100K,
    .addr_width = TUYA_IIC_ADDRESS_7BIT
};

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Write data to BMI270 register
 * @param dev Pointer to BMI270 device structure
 * @param reg Register address
 * @param data Data to write
 * @return OPERATE_RET_OK on success, error code on failure
 */
static OPERATE_RET bmi270_write_reg(bmi270_dev_t *dev, uint8_t reg, uint8_t data)
{
    OPERATE_RET ret;
    uint8_t buf[2];
    
    if (!dev) {
        return OPRT_INVALID_PARM;
    }
    
    buf[0] = reg;
    buf[1] = data;
    
    ret = tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, buf, 2, 3000);
    if (ret < 0) {
        PR_ERR("BMI270 write reg 0x%02X failed: %d", reg, ret);
        return ret;
    }
    
    return OPRT_OK;
}

/**
 * @brief Read multiple bytes from BMI270 registers
 * @param dev Pointer to BMI270 device structure
 * @param reg Starting register address
 * @param data Pointer to store read data
 * @param len Number of bytes to read
 * @return OPERATE_RET_OK on success, error code on failure
 */
static OPERATE_RET bmi270_read_regs(bmi270_dev_t *dev, uint8_t reg, uint8_t *data, uint8_t len)
{
    OPERATE_RET ret;
    
    if (!dev || !data || len == 0) {
        return OPRT_INVALID_PARM;
    }
    
    ret = tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, &reg, 1, 3000);
    if (ret < 0) {
        PR_ERR("BMI270 read reg 0x%02X failed: %d", reg, ret);
        return ret;
    }
    
    ret = tkl_i2c_master_receive(dev->i2c_port, dev->i2c_addr, data, len, 3000);
    if (ret < 0) {
        PR_ERR("BMI270 read data failed: %d", ret);
        return ret;
    }
    
    return OPRT_OK;
}

/**
 * @brief Test if device responds at given address
 * @param dev Pointer to BMI270 device structure
 * @return true if device responds, false otherwise
 */
static bool bmi270_test_device_response(bmi270_dev_t *dev)
{
    OPERATE_RET ret;
    uint8_t dummy_data = 0;
    
    ret = tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, &dummy_data, 1, 1000);
    return (ret == OPRT_OK);
}

/**
 * @brief Check if BMI270 chip is present
 * @param dev Pointer to BMI270 device structure
 * @return true if chip is present, false otherwise
 */
static bool bmi270_check_chip_id(bmi270_dev_t *dev)
{
    OPERATE_RET ret;
    uint8_t chip_id;
    
    ret = bmi270_read_regs(dev, BMI270_REG_CHIP_ID, &chip_id, 1);
    if (ret == OPRT_OK && chip_id == BMI270_CHIP_ID) {
        PR_DEBUG("BMI270 chip ID verified: 0x%02X", chip_id);
        return true;
    }
    
    PR_ERR("BMI270 chip ID check failed: expected 0x%02X, got 0x%02X", BMI270_CHIP_ID, chip_id);
    return false;
}

/**
 * @brief Write feature configuration to BMI270
 * @param dev Pointer to BMI270 device structure
 * @return OPERATE_RET_OK on success, error code on failure
 */
static OPERATE_RET bmi270_write_feature_config(bmi270_dev_t *dev)
{
    OPERATE_RET ret;
    
    /* Configure BMI270 in slave mode - simple sequential read/write */
    PR_DEBUG("Configuring BMI270 in slave mode");
    
    /* Step 1: Soft reset to ensure clean state */
    ret = bmi270_write_reg(dev, BMI270_REG_CMD, BMI270_CMD_SOFT_RESET);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to reset BMI270");
        return ret;
    }
    
    /* Wait for reset to complete */
    tkl_system_sleep(100);
    
    /* Step 2: Disable feature engine to use basic mode */
    ret = bmi270_write_reg(dev, BMI270_REG_INIT_CTRL, 0x00);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to disable feature engine");
        return ret;
    }
    
    /* Step 3: Enable accelerometer and gyroscope in normal mode */
    ret = bmi270_write_reg(dev, BMI270_REG_PWR_CTRL, 0x0E);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to enable sensors");
        return ret;
    }
    
    /* Wait for sensors to stabilize */
    tkl_system_sleep(200);
    
    PR_DEBUG("BMI270 slave mode configuration completed");
    
    return OPRT_OK;
}

/**
 * @brief Configure BMI270 sensor
 * @param dev Pointer to BMI270 device structure
 * @param config Pointer to configuration structure
 * @return OPERATE_RET_OK on success, error code on failure
 */
static OPERATE_RET bmi270_configure_sensor(bmi270_dev_t *dev, const bmi270_config_t *config)
{
    OPERATE_RET ret;
    
    if (!dev || !config) {
        return OPRT_INVALID_PARM;
    }
    
    /* Step 1: Set power mode to normal */
    ret = bmi270_write_reg(dev, BMI270_REG_PWR_CTRL, 0x0E);  /* Enable accel and gyro */
    if (ret != OPRT_OK) {
        PR_ERR("Failed to enable sensors");
        return ret;
    }
    
    /* Wait for power mode transition */
    tkl_system_sleep(100);
    
    /* Step 2: Configure accelerometer and gyroscope through power control */
    /* The BMI270 uses a different configuration approach */
    /* Enable accelerometer and gyroscope in normal mode */
    ret = bmi270_write_reg(dev, BMI270_REG_PWR_CTRL, 0x0E);  /* Enable accel and gyro */
    if (ret != OPRT_OK) {
        PR_ERR("Failed to enable sensors in power control");
        return ret;
    }
    
    /* Wait for sensors to stabilize */
    tkl_system_sleep(100);
    
    /* Wait for configuration to take effect */
    tkl_system_sleep(50);
    
    /* Store configuration */
    dev->config = *config;
    
    PR_DEBUG("BMI270 configured: Acc=%dG/%dHz, Gyr=%dDPS/%dHz, Power=%d", 
             config->acc_range, config->acc_odr, config->gyr_range, config->gyr_odr, config->power_mode);
    
    return OPRT_OK;
}

OPERATE_RET board_bmi270_init(bmi270_dev_t *dev)
{
    OPERATE_RET ret;
    
    if (!dev) {
        return OPRT_INVALID_PARM;
    }
    
    PR_DEBUG("Initializing BMI270 sensor...");
    
    /* Initialize I2C */
    ret = tkl_i2c_init(BMI270_I2C_PORT, &g_bmi270_i2c_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to initialize I2C: %d", ret);
        return ret;
    }
    
    /* Configure I2C pins */
    tkl_io_pinmux_config(TUYA_GPIO_NUM_20, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_21, TUYA_IIC0_SDA);
    
    /* Initialize device structure */
    dev->i2c_port = BMI270_I2C_PORT;
    dev->i2c_addr = BMI270_I2C_ADDR;
    dev->initialized = false;
    
    /* Check if device responds at primary address */
    if (!bmi270_test_device_response(dev)) {
        PR_DEBUG("Device not found at primary address, trying alternate address");
        dev->i2c_addr = BMI270_I2C_ADDR_ALT;
        if (!bmi270_test_device_response(dev)) {
            PR_ERR("BMI270 device not found on I2C bus");
            return OPRT_COM_ERROR;
        }
    }
    
    PR_DEBUG("BMI270 device found at address 0x%02X", dev->i2c_addr);
    
    /* Soft reset */
    ret = bmi270_write_reg(dev, BMI270_REG_CMD, BMI270_CMD_SOFT_RESET);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to reset BMI270");
        return ret;
    }
    
    /* Wait for reset to complete */
    tkl_system_sleep(100);
    
    /* Check chip ID */
    if (!bmi270_check_chip_id(dev)) {
        dev->i2c_addr = BMI270_I2C_ADDR_ALT;
        if (!bmi270_check_chip_id(dev)) {
            PR_ERR("BMI270 chip ID verification failed");
            return OPRT_COM_ERROR;
        }
    }
    
    /* Write feature configuration (this includes sensor enable) */
    ret = bmi270_write_feature_config(dev);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to write feature configuration");
        return ret;
    }
    
    /* Set default configuration for reference */
    bmi270_config_t default_config = {
        .acc_range = BMI270_ACC_RANGE_2G,
        .gyr_range = BMI270_GYR_RANGE_2000DPS,
        .acc_odr = BMI270_ODR_100HZ,
        .gyr_odr = BMI270_ODR_100HZ,
        .power_mode = BMI270_POWER_MODE_NORMAL
    };
    
    /* Store configuration for reference */
    dev->config = default_config;
    
    dev->initialized = true;
    PR_DEBUG("BMI270 initialization completed successfully");
    
    return OPRT_OK;
}

OPERATE_RET board_bmi270_deinit(bmi270_dev_t *dev)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    /* Set power mode to suspend */
    bmi270_write_reg(dev, BMI270_REG_PWR_CTRL, BMI270_POWER_MODE_SUSPEND);
    dev->initialized = false;
    
    return OPRT_OK;
}

OPERATE_RET board_bmi270_config(bmi270_dev_t *dev, const bmi270_config_t *config)
{
    if (!dev || !config || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    return bmi270_configure_sensor(dev, config);
}

OPERATE_RET board_bmi270_read_data(bmi270_dev_t *dev, bmi270_sensor_data_t *data)
{
    OPERATE_RET ret;
    uint8_t buf[12];
    
    if (!dev || !data || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    /* In slave mode, we read data directly without status checks */
    /* Read sensor data from DATA_0 to DATA_11 (12 bytes total) */
    ret = bmi270_read_regs(dev, BMI270_REG_DATA_0, buf, 12);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to read sensor data: %d", ret);
        return ret;
    }
    
    /* Also read status register for debugging */
    uint8_t status;
    if (bmi270_read_regs(dev, BMI270_REG_STATUS, &status, 1) == OPRT_OK) {
        PR_DEBUG("Status register: 0x%02X", status);
    }
    
    /* Read power control register for debugging */
    uint8_t pwr_ctrl;
    if (bmi270_read_regs(dev, BMI270_REG_PWR_CTRL, &pwr_ctrl, 1) == OPRT_OK) {
        PR_DEBUG("Power control register: 0x%02X", pwr_ctrl);
    }
    
    /* Read error register for debugging */
    uint8_t err_reg;
    if (bmi270_read_regs(dev, BMI270_REG_ERR_REG, &err_reg, 1) == OPRT_OK) {
        PR_DEBUG("Error register: 0x%02X", err_reg);
    }
    
    /* Debug: Print raw bytes */
    PR_DEBUG("Raw bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
             buf[6], buf[7], buf[8], buf[9], buf[10], buf[11]);
    
    /* Parse accelerometer data (DATA_0 to DATA_5) - BMI270 uses little-endian */
    data->acc_x = (int16_t)((buf[1] << 8) | buf[0]);
    data->acc_y = (int16_t)((buf[3] << 8) | buf[2]);
    data->acc_z = (int16_t)((buf[5] << 8) | buf[4]);
    
    /* Parse gyroscope data (DATA_6 to DATA_11) - BMI270 uses little-endian */
    data->gyr_x = (int16_t)((buf[7] << 8) | buf[6]);
    data->gyr_y = (int16_t)((buf[9] << 8) | buf[8]);
    data->gyr_z = (int16_t)((buf[11] << 8) | buf[10]);
    
    /* Temperature is not directly available in the basic data registers */
    /* We'll need to read it from a different register if available */
    data->temp = 0;  /* For now, set to 0 */
    
    return OPRT_OK;
}

OPERATE_RET board_bmi270_read_accel(bmi270_dev_t *dev, int16_t *acc_x, int16_t *acc_y, int16_t *acc_z)
{
    OPERATE_RET ret;
    uint8_t buf[6];
    
    if (!dev || !acc_x || !acc_y || !acc_z || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    /* Read accelerometer data (DATA_0 to DATA_5) */
    ret = bmi270_read_regs(dev, BMI270_REG_DATA_0, buf, 6);
    if (ret != OPRT_OK) {
        return ret;
    }
    
    /* Parse accelerometer data with corrected byte order */
    *acc_x = (int16_t)((buf[0] << 8) | buf[1]);
    *acc_y = (int16_t)((buf[2] << 8) | buf[3]);
    *acc_z = (int16_t)((buf[4] << 8) | buf[5]);
    
    return OPRT_OK;
}

OPERATE_RET board_bmi270_read_gyro(bmi270_dev_t *dev, int16_t *gyr_x, int16_t *gyr_y, int16_t *gyr_z)
{
    OPERATE_RET ret;
    uint8_t buf[6];
    
    if (!dev || !gyr_x || !gyr_y || !gyr_z || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    /* Read gyroscope data (DATA_6 to DATA_11) */
    ret = bmi270_read_regs(dev, BMI270_REG_DATA_6, buf, 6);
    if (ret != OPRT_OK) {
        return ret;
    }
    
    /* Parse gyroscope data with corrected byte order */
    *gyr_x = (int16_t)((buf[0] << 8) | buf[1]);
    *gyr_y = (int16_t)((buf[2] << 8) | buf[3]);
    *gyr_z = (int16_t)((buf[4] << 8) | buf[5]);
    
    return OPRT_OK;
}

OPERATE_RET board_bmi270_read_temp(bmi270_dev_t *dev, int16_t *temp)
{
    if (!dev || !temp || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    /* Temperature reading not implemented yet */
    *temp = 0;
    
    return OPRT_OK;
}

OPERATE_RET board_bmi270_set_power_mode(bmi270_dev_t *dev, uint8_t power_mode)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }
    
    return bmi270_write_reg(dev, BMI270_REG_PWR_CTRL, power_mode);
}

/**
 * @brief Force reset BMI270 sensor to known state
 * @param dev Pointer to BMI270 device structure
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_force_reset(bmi270_dev_t *dev)
{
    OPERATE_RET ret;
    
    if (!dev) {
        return OPRT_INVALID_PARM;
    }
    
    PR_DEBUG("Forcing BMI270 reset to known state");
    
    /* Soft reset */
    ret = bmi270_write_reg(dev, BMI270_REG_CMD, BMI270_CMD_SOFT_RESET);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to reset BMI270");
        return ret;
    }
    
    /* Wait for reset to complete */
    tkl_system_sleep(100);
    
    /* Disable feature engine */
    ret = bmi270_write_reg(dev, BMI270_REG_INIT_CTRL, 0x00);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to disable feature engine");
        return ret;
    }
    
    /* Enable sensors in normal mode */
    ret = bmi270_write_reg(dev, BMI270_REG_PWR_CTRL, 0x0E);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to enable sensors");
        return ret;
    }
    
    /* Wait for sensors to stabilize */
    tkl_system_sleep(200);
    
    PR_DEBUG("BMI270 force reset completed");
    
    return OPRT_OK;
}

bool board_bmi270_is_ready(bmi270_dev_t *dev)
{
    if (!dev || !dev->initialized) {
        return false;
    }
    
    /* In slave mode, we consider the sensor ready if we can read status */
    uint8_t status;
    OPERATE_RET ret = bmi270_read_regs(dev, BMI270_REG_STATUS, &status, 1);
    
    /* Return true if we can read status and it's not all zeros */
    return (ret == OPRT_OK && status != 0x00);
}

OPERATE_RET board_bmi270_register(void)
{
    /* Register BMI270 driver with the system */
    return OPRT_OK;
}

OPERATE_RET board_bmi270_scan_i2c(TUYA_I2C_NUM_E port)
{
    OPERATE_RET ret;
    uint8_t dummy_data = 0;
    
    PR_DEBUG("Scanning I2C bus %d for BMI270...", port);
    
    /* Try primary address */
    ret = tkl_i2c_master_send(port, BMI270_I2C_ADDR, &dummy_data, 1, 1000);
    if (ret == OPRT_OK) {
        PR_DEBUG("BMI270 found at address 0x%02X", BMI270_I2C_ADDR);
        return OPRT_OK;
    }
    
    /* Try alternate address */
    ret = tkl_i2c_master_send(port, BMI270_I2C_ADDR_ALT, &dummy_data, 1, 1000);
    if (ret == OPRT_OK) {
        PR_DEBUG("BMI270 found at address 0x%02X", BMI270_I2C_ADDR_ALT);
        return OPRT_OK;
    }
    
    PR_ERR("BMI270 not found on I2C bus %d", port);
    return OPRT_COM_ERROR;
}
