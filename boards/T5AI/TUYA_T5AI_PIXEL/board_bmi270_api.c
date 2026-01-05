/**
 * @file board_bmi270_api.c
 * @author Tuya Inc.
 * @brief BMI270 sensor driver implementation for TUYA_T5AI_POCKET board
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "board_bmi270_api.h"
#include "tal_log.h"
#include "tkl_pinmux.h"
#include "tkl_i2c.h"
#include "tkl_system.h"
#include "bmi270.h"
#include "bmi270_common.h"

/***********************************************************
************************macro define************************
***********************************************************/
/*! Earth's gravity in m/s^2 */
#define GRAVITY_EARTH (9.80665f)

/*! Macros to select the sensors                   */
#define ACCEL UINT8_C(0x00)
#define GYRO  UINT8_C(0x01)

/***********************************************************
***********************variable define**********************
***********************************************************/
/* Global BMI270 device instance */
static bmi270_dev_t g_bmi270_dev = {0};
/* Sensor initialization configuration. */
static struct bmi2_dev bmi2_dev;
/* Assign accel and gyro sensor to variable. */
static uint8_t sensor_list[2] = {BMI2_ACCEL, BMI2_GYRO};

/* I2C configuration for BMI270 */
static TUYA_IIC_BASE_CFG_T g_bmi270_i2c_cfg = {
    .role = TUYA_IIC_MODE_MASTER, .speed = TUYA_IIC_BUS_SPEED_400K, .addr_width = TUYA_IIC_ADDRESS_7BIT};

/***********************************************************
***********************function define**********************
***********************************************************/
static int8_t set_accel_gyro_config(struct bmi2_dev *bmi2_dev);
static float lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width);
static float lsb_to_dps(int16_t val, float dps, uint8_t bit_width);

/**
 * @brief Write data to BMI270 register
 * @param dev Pointer to BMI270 device structure
 * @param reg Register address
 * @param data Data to write
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET bmi270_write_reg(bmi270_dev_t *dev, uint8_t reg, uint8_t data)
{
    OPERATE_RET ret;
    uint8_t buf[2];

    if (!dev) {
        return OPRT_INVALID_PARM;
    }

    buf[0] = reg;
    buf[1] = data;

    ret = tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, buf, 2, TRUE);
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
OPERATE_RET bmi270_read_regs(bmi270_dev_t *dev, uint8_t reg, uint8_t *data, uint8_t len)
{
    OPERATE_RET ret;

    if (!dev || !data || len == 0) {
        return OPRT_INVALID_PARM;
    }

    ret = tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, &reg, 1, FALSE);
    if (ret < 0) {
        PR_ERR("BMI270 read reg 0x%02X failed: %d", reg, ret);
        return ret;
    }

    ret = tkl_i2c_master_receive(dev->i2c_port, dev->i2c_addr, data, len, TRUE);
    if (ret < 0) {
        PR_ERR("BMI270 read data failed: %d", ret);
        return ret;
    }

    return OPRT_OK;
}

OPERATE_RET board_bmi270_init(bmi270_dev_t *dev)
{
    OPERATE_RET ret;

    if (!dev) {
        PR_ERR("Invalid device pointer");
        return OPRT_INVALID_PARM;
    }

    /* Skip initialization if already initialized */
    if (dev->initialized) {
        return OPRT_OK;
    }

    /* Configure I2C pins */
    tkl_io_pinmux_config(TUYA_GPIO_NUM_20, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_21, TUYA_IIC0_SDA);

    ret = tkl_i2c_init(BMI270_I2C_PORT, &g_bmi270_i2c_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to initialize I2C: %d", ret);
        return ret;
    }

    /* Initialize device structure */
    dev->i2c_port = BMI270_I2C_PORT;
    dev->i2c_addr = BMI270_I2C_ADDR;

    /* Interface reference is given as a parameter
     * For I2C : BMI2_I2C_INTF
     * For SPI : BMI2_SPI_INTF
     */
    bmi2_dev.intf_ptr = &(dev->i2c_port);
    ret = bmi2_interface_init(&bmi2_dev, BMI2_I2C_INTF);
    if (ret != BMI2_OK) {
        PR_ERR("BMI270 interface init failed: %d", ret);
        return OPRT_COM_ERROR;
    }

    /* Initialize bmi270. */
    ret = bmi270_init(&bmi2_dev);
    if (ret != BMI2_OK) {
        PR_ERR("BMI270 init failed: %d", ret);
        return OPRT_COM_ERROR;
    }

    /* Accel and gyro configuration settings. */
    ret = set_accel_gyro_config(&bmi2_dev);
    if (ret != BMI2_OK) {
        PR_ERR("BMI270 config failed: %d", ret);
        return OPRT_COM_ERROR;
    }

    /* NOTE:
     * Accel and Gyro enable must be done after setting configurations
     */
    ret = bmi270_sensor_enable(sensor_list, 2, &bmi2_dev);
    if (ret != BMI2_OK) {
        PR_ERR("BMI270 sensor enable failed: %d", ret);
        return OPRT_COM_ERROR;
    }

    /* Mark as initialized only after all steps succeed */
    dev->initialized = true;
    return OPRT_OK;
}

OPERATE_RET board_bmi270_deinit(bmi270_dev_t *dev)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    tkl_i2c_deinit(dev->i2c_port);
    dev->initialized = false;

    return OPRT_OK;
}

OPERATE_RET board_bmi270_read_data(bmi270_dev_t *dev, bmi270_sensor_data_t *data)
{
    OPERATE_RET ret;
    /* Create an instance of sensor data structure. */
    struct bmi2_sens_data sensor_data = {{0}};

    if (!dev || !data || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    /* Get accel and gyro data for x, y and z axis. */
    /* Note: bmi2_get_sensor_data can be called directly without checking interrupt status */
    ret = bmi2_get_sensor_data(&sensor_data, &bmi2_dev);
    if (ret != BMI2_OK) {
        return OPRT_COM_ERROR;
    }

    /* Converting lsb to meter per second squared for 16 bit accelerometer at 16G range. */
    data->acc_x = lsb_to_mps2(sensor_data.acc.x, 16, bmi2_dev.resolution);
    data->acc_y = lsb_to_mps2(sensor_data.acc.y, 16, bmi2_dev.resolution);
    data->acc_z = lsb_to_mps2(sensor_data.acc.z, 16, bmi2_dev.resolution);

    /* Converting lsb to degree per second for 16 bit gyro at 2000dps range. */
    data->gyr_x = lsb_to_dps(sensor_data.gyr.x, 2000, bmi2_dev.resolution);
    data->gyr_y = lsb_to_dps(sensor_data.gyr.y, 2000, bmi2_dev.resolution);
    data->gyr_z = lsb_to_dps(sensor_data.gyr.z, 2000, bmi2_dev.resolution);

    /* Temperature is not directly available in the basic data registers */
    /* We'll need to read it from a different register if available */
    data->temp = 0; /* For now, set to 0 */

    return OPRT_OK;
}

OPERATE_RET board_bmi270_read_accel(bmi270_dev_t *dev, float *acc_x, float *acc_y, float *acc_z)
{
    OPERATE_RET ret;

    if (!dev || !acc_x || !acc_y || !acc_z || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    /* Create an instance of sensor data structure. */
    struct bmi2_sens_data sensor_data = {{0}};

    /* Get accel and gyro data for x, y and z axis. */
    /* Note: bmi2_get_sensor_data can be called directly without checking interrupt status */
    ret = bmi2_get_sensor_data(&sensor_data, &bmi2_dev);
    if (ret != BMI2_OK) {
        return OPRT_COM_ERROR;
    }

    /* Converting lsb to meter per second squared for 16 bit accelerometer at 16G range. */
    *acc_x = lsb_to_mps2(sensor_data.acc.x, 16, bmi2_dev.resolution);
    *acc_y = lsb_to_mps2(sensor_data.acc.y, 16, bmi2_dev.resolution);
    *acc_z = lsb_to_mps2(sensor_data.acc.z, 16, bmi2_dev.resolution);

    return OPRT_OK;
}

OPERATE_RET board_bmi270_read_gyro(bmi270_dev_t *dev, float *gyr_x, float *gyr_y, float *gyr_z)
{
    OPERATE_RET ret;

    if (!dev || !gyr_x || !gyr_y || !gyr_z || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    /* Create an instance of sensor data structure. */
    struct bmi2_sens_data sensor_data = {{0}};

    /* Get accel and gyro data for x, y and z axis. */
    /* Note: bmi2_get_sensor_data can be called directly without checking interrupt status */
    ret = bmi2_get_sensor_data(&sensor_data, &bmi2_dev);
    if (ret != BMI2_OK) {
        return OPRT_COM_ERROR;
    }

    /* Converting lsb to degree per second for 16 bit gyro at 2000dps range. */
    *gyr_x = lsb_to_dps(sensor_data.gyr.x, 2000, bmi2_dev.resolution);
    *gyr_y = lsb_to_dps(sensor_data.gyr.y, 2000, bmi2_dev.resolution);
    *gyr_z = lsb_to_dps(sensor_data.gyr.z, 2000, bmi2_dev.resolution);

    return OPRT_OK;
}

OPERATE_RET board_bmi270_read_temp(bmi270_dev_t *dev, int16_t *temp)
{
    if (!dev || !temp || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    /* Temperature reading not implemented yet */
    *temp = 0;

    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET board_bmi270_set_power_mode(bmi270_dev_t *dev, bool power_mode)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    bmi2_set_adv_power_save(power_mode, &bmi2_dev);
    g_bmi270_dev.config.power_mode = power_mode; // Normal
    return OPRT_OK;
}

/**
 * @brief Force reset BMI270 sensor to known state
 * @param dev Pointer to BMI270 device structure
 * @return OPERATE_RET_OK on success, error code on failure
 */
OPERATE_RET board_bmi270_force_reset(bmi270_dev_t *dev)
{
    if (!dev) {
        return OPRT_INVALID_PARM;
    }

    bmi2_soft_reset(&bmi2_dev);
    return OPRT_OK;
}

OPERATE_RET board_bmi270_register(void)
{
    OPERATE_RET ret = OPRT_OK;

    ret = board_bmi270_init(&g_bmi270_dev);

    return ret;
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

bmi270_dev_t *board_bmi270_get_handle()
{
    return &g_bmi270_dev;
}

/**
 * @brief Check if BMI270 sensor is ready
 * @param dev Pointer to BMI270 device structure
 * @return true if sensor is initialized and ready, false otherwise
 */
bool board_bmi270_is_ready(bmi270_dev_t *dev)
{
    if (!dev) {
        return false;
    }

    return dev->initialized;
}

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
    if (rslt != BMI2_OK) {
        PR_ERR("BMI270 get sensor config failed: %d", rslt);
        return rslt;
    }

    /* Map data ready interrupt to interrupt pin. */
    rslt = bmi2_map_data_int(BMI2_DRDY_INT, BMI2_INT1, bmi2_dev);
    if (rslt != BMI2_OK) {
        PR_ERR("BMI270 map data int failed: %d", rslt);
        return rslt;
    }

    if (rslt == BMI2_OK) {
        /* NOTE: The user can change the following configuration parameters according to their requirement. */
        /* Set Output Data Rate */
        config[ACCEL].cfg.acc.odr = BMI2_ACC_ODR_200HZ;

        /* Gravity range of the sensor (+/- 2G, 4G, 8G, 16G). */
        config[ACCEL].cfg.acc.range = BMI2_ACC_RANGE_16G;

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
        if (rslt != BMI2_OK) {
            PR_ERR("BMI270 set sensor config failed: %d", rslt);
            return rslt;
        }
    }
    g_bmi270_dev.config.acc_odr = config[ACCEL].cfg.acc.odr;
    g_bmi270_dev.config.acc_range = config[ACCEL].cfg.acc.range;
    g_bmi270_dev.config.gyr_odr = config[GYRO].cfg.gyr.odr;
    g_bmi270_dev.config.gyr_range = config[GYRO].cfg.gyr.range;
    g_bmi270_dev.config.power_mode = 0; // Normal

    return rslt;
}

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