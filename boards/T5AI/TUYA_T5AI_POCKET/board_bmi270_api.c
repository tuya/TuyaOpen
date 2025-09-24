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
#include "bmi270.h"
#include "bmi270_legacy.h"
#include "bmi270_common.h"
/***********************************************************
***********************variable define**********************
***********************************************************/

/* Global BMI270 device instance */
static bmi270_dev_t g_bmi270_dev = {0};

/* I2C configuration for BMI270 */
static TUYA_IIC_BASE_CFG_T g_bmi270_i2c_cfg = {
    .role = TUYA_IIC_MODE_MASTER,
    .speed = TUYA_IIC_BUS_SPEED_100K,
    .addr_width = TUYA_IIC_ADDRESS_7BIT
};

/***********************************************************
***********************function define**********************
***********************************************************/
/******************************************************************************/
/*!                Macro definition                                           */

/*! Earth's gravity in m/s^2 */
#define GRAVITY_EARTH  (9.80665f)

/*! Macros to select the sensors                   */
#define ACCEL          UINT8_C(0x00)
#define GYRO           UINT8_C(0x01)

/******************************************************************************/
/*!           Static Function Declaration                                     */

/*!
 *  @brief This internal API is used to set configurations for accel.
 *
 *  @param[in] dev       : Structure instance of bmi2_dev.
 *
 *  @return Status of execution.
 */
static int8_t set_accel_gyro_config(struct bmi2_dev *bmi2_dev);

/*!
 *  @brief This function converts lsb to meter per second squared for 16 bit accelerometer at
 *  range 2G, 4G, 8G or 16G.
 *
 *  @param[in] val       : LSB from each axis.
 *  @param[in] g_range   : Gravity range.
 *  @param[in] bit_width : Resolution for accel.
 *
 *  @return Gravity.
 */
static float lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width);

/*!
 *  @brief This function converts lsb to degree per second for 16 bit gyro at
 *  range 125, 250, 500, 1000 or 2000dps.
 *
 *  @param[in] val       : LSB from each axis.
 *  @param[in] dps       : Degree per second.
 *  @param[in] bit_width : Resolution for gyro.
 *
 *  @return Degree per second.
 */
static float lsb_to_dps(int16_t val, float dps, uint8_t bit_width);

#if 0
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
    ret = bmi270_write_reg(dev, BMI270_REG_PWR_CTRL, 0x0E);  /* Enable accel and gyro */
    if (ret != OPRT_OK) {
        PR_ERR("Failed to enable sensors in power control");
        return ret;
    }
    
    /* Store configuration */
    dev->config = *config;
    
    PR_DEBUG("BMI270 configured: Acc=%dG/%dHz, Gyr=%dDPS/%dHz, Power=%d", 
             config->acc_range, config->acc_odr, config->gyr_range, config->gyr_odr, config->power_mode);
    
    return OPRT_OK;
}
#endif
OPERATE_RET board_bmi270_init(bmi270_dev_t *dev)
{
    OPERATE_RET ret;
    
    if (!dev) {
        PR_ERR("Invalid device pointer");
        return OPRT_INVALID_PARM;
    }
    
    PR_DEBUG("Initializing BMI270 sensor...");
    
    /* Configure I2C pins */
    tkl_io_pinmux_config(TUYA_GPIO_NUM_20, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_21, TUYA_IIC0_SDA);

    /* Initialize I2C */
    ret = tkl_i2c_init(BMI270_I2C_PORT, &g_bmi270_i2c_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to initialize I2C: %d", ret);
        return ret;
    }
    
    /* Initialize device structure */
    dev->i2c_port = BMI270_I2C_PORT;
    dev->i2c_addr = BMI270_I2C_ADDR;
    dev->initialized = false;
#if 0
    /* Status of api are returned to this variable. */
    int8_t rslt;

    /* Variable to define limit to print accel data. */
    uint8_t limit = 10;

    /* Assign accel and gyro sensor to variable. */
    uint8_t sensor_list[2] = { BMI2_ACCEL, BMI2_GYRO };

    /* Sensor initialization configuration. */
    struct bmi2_dev bmi2_dev;

    /* Create an instance of sensor data structure. */
    struct bmi2_sens_data sensor_data = { { 0 } };

    /* Initialize the interrupt status of accel and gyro. */
    uint16_t int_status = 0;

    uint8_t indx = 1;

    float x = 0, y = 0, z = 0;

    /* Interface reference is given as a parameter
     * For I2C : BMI2_I2C_INTF
     * For SPI : BMI2_SPI_INTF
     */
    rslt = bmi2_interface_init(&bmi2_dev, BMI2_I2C_INTF);
    bmi2_error_codes_print_result(rslt);

    /* Initialize bmi270_legacy. */
    rslt = bmi270_legacy_init(&bmi2_dev);
    bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK)
    {
        /* Accel and gyro configuration settings. */
        rslt = set_accel_gyro_config(&bmi2_dev);
        bmi2_error_codes_print_result(rslt);

        if (rslt == BMI2_OK)
        {
            /* NOTE: Accel and Gyro enable must be done after setting configurations */
            rslt = bmi270_legacy_sensor_enable(sensor_list, 2, &bmi2_dev);
            bmi2_error_codes_print_result(rslt);

            /* Loop to print accel and gyro data when interrupt occurs. */
            while (indx <= limit)
            {
                /* To get the data ready interrupt status of accel and gyro. */
                rslt = bmi2_get_int_status(&int_status, &bmi2_dev);
                bmi2_error_codes_print_result(rslt);

                /* To check the data ready interrupt status and print the status for 10 samples. */
                if ((int_status & BMI2_ACC_DRDY_INT_MASK) && (int_status & BMI2_GYR_DRDY_INT_MASK))
                {
                    /* Get accel and gyro data for x, y and z axis. */
                    rslt = bmi2_get_sensor_data(&sensor_data, &bmi2_dev);
                    bmi2_error_codes_print_result(rslt);

                    printf("\n*******  Accel(Raw and m/s2) Gyro(Raw and dps) data : %d  *******\n", indx);

                    printf("\nAcc_x = %d\t", sensor_data.acc.x);
                    printf("Acc_y = %d\t", sensor_data.acc.y);
                    printf("Acc_z = %d", sensor_data.acc.z);

                    /* Converting lsb to meter per second squared for 16 bit accelerometer at 2G range. */
                    x = lsb_to_mps2(sensor_data.acc.x, 2, bmi2_dev.resolution);
                    y = lsb_to_mps2(sensor_data.acc.y, 2, bmi2_dev.resolution);
                    z = lsb_to_mps2(sensor_data.acc.z, 2, bmi2_dev.resolution);

                    /* Print the data in m/s2. */
                    printf("\nAcc_ms2_X = %4.2f, Acc_ms2_Y = %4.2f, Acc_ms2_Z = %4.2f\n", x, y, z);

                    printf("\nGyr_X = %d\t", sensor_data.gyr.x);
                    printf("Gyr_Y = %d\t", sensor_data.gyr.y);
                    printf("Gyr_Z= %d\n", sensor_data.gyr.z);

                    /* Converting lsb to degree per second for 16 bit gyro at 2000dps range. */
                    x = lsb_to_dps(sensor_data.gyr.x, 2000, bmi2_dev.resolution);
                    y = lsb_to_dps(sensor_data.gyr.y, 2000, bmi2_dev.resolution);
                    z = lsb_to_dps(sensor_data.gyr.z, 2000, bmi2_dev.resolution);

                    /* Print the data in dps. */
                    printf("Gyro_DPS_X = %4.2f, Gyro_DPS_Y = %4.2f, Gyro_DPS_Z = %4.2f\n", x, y, z);

                    indx++;
                }
            }
        }
    }

#else
    /* Status of api are returned to this variable. */
    int8_t rslt;

    /* Variable to define limit to print accel data. */
    uint8_t limit = 10;

    /* Assign accel and gyro sensor to variable. */
    uint8_t sensor_list[2] = { BMI2_ACCEL, BMI2_GYRO };

    /* Sensor initialization configuration. */
    struct bmi2_dev bmi2_dev;

    /* Create an instance of sensor data structure. */
    struct bmi2_sens_data sensor_data = { { 0 } };

    /* Initialize the interrupt status of accel and gyro. */
    uint16_t int_status = 0;

    uint8_t indx = 1;

    float x = 0, y = 0, z = 0;

    /* Interface reference is given as a parameter
     * For I2C : BMI2_I2C_INTF
     * For SPI : BMI2_SPI_INTF
     */
    rslt = bmi2_interface_init(&bmi2_dev, BMI2_I2C_INTF);
    bmi2_error_codes_print_result(rslt);

    /* Initialize bmi270. */
    rslt = bmi270_init(&bmi2_dev);
    bmi2_error_codes_print_result(rslt);
    PR_DEBUG("BMI270 initialized successfully %d", rslt);
    if (rslt == BMI2_OK)
    {
        /* Accel and gyro configuration settings. */
        rslt = set_accel_gyro_config(&bmi2_dev);
        bmi2_error_codes_print_result(rslt);

        if (rslt == BMI2_OK)
        {
            /* NOTE:
             * Accel and Gyro enable must be done after setting configurations
             */
            rslt = bmi270_sensor_enable(sensor_list, 2, &bmi2_dev);
            bmi2_error_codes_print_result(rslt);

            /* Loop to print accel and gyro data when interrupt occurs. */
            while (indx <= limit)
            {
                /* To get the data ready interrupt status of accel and gyro. */
                rslt = bmi2_get_int_status(&int_status, &bmi2_dev);
                bmi2_error_codes_print_result(rslt);

                /* To check the data ready interrupt status and print the status for 10 samples. */
                if ((int_status & BMI2_ACC_DRDY_INT_MASK) && (int_status & BMI2_GYR_DRDY_INT_MASK))
                {
                    /* Get accel and gyro data for x, y and z axis. */
                    rslt = bmi2_get_sensor_data(&sensor_data, &bmi2_dev);
                    bmi2_error_codes_print_result(rslt);

                    printf("\n*******  Accel(Raw and m/s2) Gyro(Raw and dps) data : %d  *******\n", indx);

                    printf("\nAcc_x = %d\t", sensor_data.acc.x);
                    printf("Acc_y = %d\t", sensor_data.acc.y);
                    printf("Acc_z = %d", sensor_data.acc.z);

                    /* Converting lsb to meter per second squared for 16 bit accelerometer at 2G range. */
                    x = lsb_to_mps2(sensor_data.acc.x, 2, bmi2_dev.resolution);
                    y = lsb_to_mps2(sensor_data.acc.y, 2, bmi2_dev.resolution);
                    z = lsb_to_mps2(sensor_data.acc.z, 2, bmi2_dev.resolution);

                    /* Print the data in m/s2. */
                    printf("\nAcc_ms2_X = %4.2f, Acc_ms2_Y = %4.2f, Acc_ms2_Z = %4.2f\n", x, y, z);

                    printf("\nGyr_X = %d\t", sensor_data.gyr.x);
                    printf("Gyr_Y = %d\t", sensor_data.gyr.y);
                    printf("Gyr_Z= %d\n", sensor_data.gyr.z);

                    /* Converting lsb to degree per second for 16 bit gyro at 2000dps range. */
                    x = lsb_to_dps(sensor_data.gyr.x, 2000, bmi2_dev.resolution);
                    y = lsb_to_dps(sensor_data.gyr.y, 2000, bmi2_dev.resolution);
                    z = lsb_to_dps(sensor_data.gyr.z, 2000, bmi2_dev.resolution);

                    /* Print the data in dps. */
                    printf("Gyro_DPS_X = %4.2f, Gyro_DPS_Y = %4.2f, Gyro_DPS_Z = %4.2f\n", x, y, z);

                    indx++;
                }
            }
        }
    }
#endif 
    return OPRT_OK;
}
#if 0
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
    ret = bmi270_read_regs(dev, BMI270_REG_DATA_8, buf, 12);
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
#endif
OPERATE_RET board_bmi270_register(void)
{
    OPERATE_RET ret = OPRT_OK;
    /* Register BMI270 driver with the system */
    g_bmi270_dev.i2c_port = BMI270_I2C_PORT;
    g_bmi270_dev.i2c_addr = BMI270_I2C_ADDR;
    g_bmi270_dev.initialized = false;

    ret = board_bmi270_init(&g_bmi270_dev);
    // board_bmi270_config(&g_bmi270_dev, &(g_bmi270_dev.config));
    return ret;
}
#if 0
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
#endif
bmi270_dev_t *board_bmi270_get_handle()
{
    return &g_bmi270_dev;
}
#if 0
/*!
 * @brief This internal API is used to set configurations for accel and gyro.
 */
static int8_t set_accel_gyro_config(struct bmi2_dev *bmi2_dev)
{
    /* Status of api are returned to this variable. */
    int8_t rslt;

    /* Structure to define accelerometer and gyro configuration. */
    struct bmi2_sens_config config[2];

    /* Configure the type of feature. */
    config[ACCEL].type = BMI2_ACCEL;
    config[GYRO].type = BMI2_GYRO;

    /* Get default configurations for the type of feature selected. */
    rslt = bmi270_legacy_get_sensor_config(config, 2, bmi2_dev);
    bmi2_error_codes_print_result(rslt);

    /* Map data ready interrupt to interrupt pin. */
    rslt = bmi2_map_data_int(BMI2_DRDY_INT, BMI2_INT1, bmi2_dev);
    bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK)
    {
        /* NOTE: The user can change the following configuration parameters according to their requirement. */
        /* Set Output Data Rate */
        config[ACCEL].cfg.acc.odr = BMI2_ACC_ODR_200HZ;

        /* Gravity range of the sensor (+/- 2G, 4G, 8G, 16G). */
        config[ACCEL].cfg.acc.range = BMI2_ACC_RANGE_2G;

        /* The bandwidth parameter is used to configure the number of sensor samples that are averaged
         * if it is set to 2, then 2^(bandwidth parameter) samples
         * are averaged, resulting in 4 averaged samples.
         * Note1 : For more information, refer the datasheet.
         * Note2 : A higher number of averaged samples will result in a lower noise level of the signal, but
         * this has an adverse effect on the power consumed.
         */
        config[ACCEL].cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;

        /* Enable the filter performance mode where averaging of samples
         * will be done based on above set bandwidth and ODR.
         * There are two modes
         *  0 -> Ultra low power mode
         *  1 -> High performance mode(Default)
         * For more info refer datasheet.
         */
        config[ACCEL].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;

        /* The user can change the following configuration parameters according to their requirement. */
        /* Set Output Data Rate */
        config[GYRO].cfg.gyr.odr = BMI2_GYR_ODR_200HZ;

        /* Gyroscope Angular Rate Measurement Range.By default the range is 2000dps. */
        config[GYRO].cfg.gyr.range = BMI2_GYR_RANGE_2000;

        /* Gyroscope bandwidth parameters. By default the gyro bandwidth is in normal mode. */
        config[GYRO].cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;

        /* Enable/Disable the noise performance mode for precision yaw rate sensing
         * There are two modes
         *  0 -> Ultra low power mode(Default)
         *  1 -> High performance mode
         */
        config[GYRO].cfg.gyr.noise_perf = BMI2_POWER_OPT_MODE;

        /* Enable/Disable the filter performance mode where averaging of samples
         * will be done based on above set bandwidth and ODR.
         * There are two modes
         *  0 -> Ultra low power mode
         *  1 -> High performance mode(Default)
         */
        config[GYRO].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;

        /* Set the accel and gyro configurations. */
        rslt = bmi270_legacy_set_sensor_config(config, 2, bmi2_dev);
        bmi2_error_codes_print_result(rslt);
    }

    return rslt;
}


#else
/*!
 * @brief This internal API is used to set configurations for accel and gyro.
 */
static int8_t set_accel_gyro_config(struct bmi2_dev *bmi2_dev)
{
    /* Status of api are returned to this variable. */
    int8_t rslt;

    /* Structure to define accelerometer and gyro configuration. */
    struct bmi2_sens_config config[2];

    /* Configure the type of feature. */
    config[ACCEL].type = BMI2_ACCEL;
    config[GYRO].type = BMI2_GYRO;

    /* Get default configurations for the type of feature selected. */
    rslt = bmi270_get_sensor_config(config, 2, bmi2_dev);
    bmi2_error_codes_print_result(rslt);

    /* Map data ready interrupt to interrupt pin. */
    rslt = bmi2_map_data_int(BMI2_DRDY_INT, BMI2_INT1, bmi2_dev);
    bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK)
    {
        /* NOTE: The user can change the following configuration parameters according to their requirement. */
        /* Set Output Data Rate */
        config[ACCEL].cfg.acc.odr = BMI2_ACC_ODR_200HZ;

        /* Gravity range of the sensor (+/- 2G, 4G, 8G, 16G). */
        config[ACCEL].cfg.acc.range = BMI2_ACC_RANGE_2G;

        /* The bandwidth parameter is used to configure the number of sensor samples that are averaged
         * if it is set to 2, then 2^(bandwidth parameter) samples
         * are averaged, resulting in 4 averaged samples.
         * Note1 : For more information, refer the datasheet.
         * Note2 : A higher number of averaged samples will result in a lower noise level of the signal, but
         * this has an adverse effect on the power consumed.
         */
        config[ACCEL].cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;

        /* Enable the filter performance mode where averaging of samples
         * will be done based on above set bandwidth and ODR.
         * There are two modes
         *  0 -> Ultra low power mode
         *  1 -> High performance mode(Default)
         * For more info refer datasheet.
         */
        config[ACCEL].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;

        /* The user can change the following configuration parameters according to their requirement. */
        /* Set Output Data Rate */
        config[GYRO].cfg.gyr.odr = BMI2_GYR_ODR_200HZ;

        /* Gyroscope Angular Rate Measurement Range.By default the range is 2000dps. */
        config[GYRO].cfg.gyr.range = BMI2_GYR_RANGE_2000;

        /* Gyroscope bandwidth parameters. By default the gyro bandwidth is in normal mode. */
        config[GYRO].cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;

        /* Enable/Disable the noise performance mode for precision yaw rate sensing
         * There are two modes
         *  0 -> Ultra low power mode(Default)
         *  1 -> High performance mode
         */
        config[GYRO].cfg.gyr.noise_perf = BMI2_POWER_OPT_MODE;

        /* Enable/Disable the filter performance mode where averaging of samples
         * will be done based on above set bandwidth and ODR.
         * There are two modes
         *  0 -> Ultra low power mode
         *  1 -> High performance mode(Default)
         */
        config[GYRO].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;

        /* Set the accel and gyro configurations. */
        rslt = bmi270_set_sensor_config(config, 2, bmi2_dev);
        bmi2_error_codes_print_result(rslt);
    }

    return rslt;
}
#endif
/*!
 * @brief This function converts lsb to meter per second squared for 16 bit accelerometer at
 * range 2G, 4G, 8G or 16G.
 */
static float lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width)
{
    float half_scale = ((float)(1 << bit_width) / 2.0f);

    return (GRAVITY_EARTH * val * g_range) / half_scale;
}

/*!
 * @brief This function converts lsb to degree per second for 16 bit gyro at
 * range 125, 250, 500, 1000 or 2000dps.
 */
static float lsb_to_dps(int16_t val, float dps, uint8_t bit_width)
{
    float half_scale = ((float)(1 << bit_width) / 2.0f);

    return (dps / ((half_scale))) * (val);
}