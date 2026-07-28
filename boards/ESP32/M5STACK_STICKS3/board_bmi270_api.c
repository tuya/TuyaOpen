/**
 * @file board_bmi270_api.c
 * @brief BMI270 IMU sensor driver implementation for M5Stack StickS3 board.
 *
 * The BMI270 shares the internal I2C1 bus (SCL=G48, SDA=G47) with the ES8311
 * codec and M5PM1 PMIC.  I2C and pinmux are already initialised by the PMIC
 * power-up sequence in m5stack_sticks3.c, so this driver only needs to probe
 * the sensor on the existing bus.  The sensor is powered by 3V3_L1 (LDO),
 * which is also enabled during power init.
 *
 * @version 0.1
 * @date 2026-07-27
 * @copyright Copyright (c) Tuya Inc. All Rights Reserved.
 */

#include "board_bmi270_api.h"
#include "board_config.h"
#include "tal_log.h"
#include "tkl_i2c.h"
#include "tkl_system.h"
#include "bmi270.h"
#include "bmi270_common.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Private macros
 * --------------------------------------------------------------------------- */
/*! Earth's gravity in m/s^2 */
#define GRAVITY_EARTH  (9.80665f)

/*! Sensor indices for configuration array. */
#define ACCEL  UINT8_C(0x00)
#define GYRO   UINT8_C(0x01)

/* ---------------------------------------------------------------------------
 * Private variables
 * --------------------------------------------------------------------------- */
/* Global BMI270 device instance */
static bmi270_dev_t g_bmi270_dev = {0};

/* Bosch BMI2 backend device */
static struct bmi2_dev bmi2_dev;

/* Sensor enable list (accel + gyro) */
static uint8_t sensor_list[2] = { BMI2_ACCEL, BMI2_GYRO };

/* I2C base configuration – master, 100 kHz, 7-bit addressing.
 * Note: the I2C peripheral is already initialised by the M5PM1 power init;
 * this structure is kept for reference / future re-init paths.
 * Suppress unused-variable warning as this is intentionally kept for future use. */
__attribute__((unused)) static TUYA_IIC_BASE_CFG_T g_bmi270_i2c_cfg = {
    .role = TUYA_IIC_MODE_MASTER,
    .speed = TUYA_IIC_BUS_SPEED_100K,
    .addr_width = TUYA_IIC_ADDRESS_7BIT
};

/* ---------------------------------------------------------------------------
 * Private function prototypes
 * --------------------------------------------------------------------------- */
static int8_t set_accel_gyro_config(struct bmi2_dev *dev);
static float lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width);
static float lsb_to_dps(int16_t val, float dps, uint8_t bit_width);

/* ---------------------------------------------------------------------------
 * Low-level register access (used by the Bosch BMI2 backend via callbacks)
 * --------------------------------------------------------------------------- */
/**
 * @brief Write a single byte to a BMI270 register.
 */
OPERATE_RET bmi270_write_reg(bmi270_dev_t *dev, uint8_t reg, uint8_t data)
{
    uint8_t buf[2];

    if (!dev) {
        return OPRT_INVALID_PARM;
    }

    buf[0] = reg;
    buf[1] = data;

    OPERATE_RET ret = tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, buf, 2, TRUE);
    if (ret < 0) {
        PR_ERR("BMI270 write reg 0x%02X failed: %d", reg, ret);
        return ret;
    }

    return OPRT_OK;
}

/**
 * @brief Read multiple bytes from BMI270 registers.
 */
OPERATE_RET bmi270_read_regs(bmi270_dev_t *dev, uint8_t reg, uint8_t *data, uint8_t len)
{
    if (!dev || !data || len == 0) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET ret = tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, &reg, 1, FALSE);
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

/* ---------------------------------------------------------------------------
 * Initialisation
 * --------------------------------------------------------------------------- */
/**
 * @brief Initialise the BMI270 sensor on the shared I2C bus.
 *
 * I2C pinmux and peripheral are already configured by the PMIC init path.
 * 3V3_L1 power rail is already enabled before this function is called.
 */
static OPERATE_RET board_bmi270_init(bmi270_dev_t *dev)
{
    OPERATE_RET ret;

    if (!dev) {
        PR_ERR("Invalid device pointer");
        return OPRT_INVALID_PARM;
    }

    PR_DEBUG("Initializing BMI270 sensor on I2C port %d addr 0x%02X...",
             BMI270_I2C_PORT, BMI270_I2C_ADDR);

    /* Populate device handle. */
    dev->i2c_port = BMI270_I2C_PORT;
    dev->i2c_addr = BMI270_I2C_ADDR;
    dev->initialized = true;

    /* Bind the Bosch BMI2 backend to our I2C interface. */
    bmi2_dev.intf_ptr = &(dev->i2c_port);
    ret = bmi2_interface_init(&bmi2_dev, BMI2_I2C_INTF);
    bmi2_error_codes_print_result(ret);
    if (ret != BMI2_OK) {
        dev->initialized = false;
        return OPRT_COM_ERROR;
    }

    /* Initialise BMI270 (upload config file, etc.). */
    ret = bmi270_init(&bmi2_dev);
    bmi2_error_codes_print_result(ret);
    if (ret != BMI2_OK) {
        dev->initialized = false;
        return OPRT_COM_ERROR;
    }

    /* Apply accel + gyro configuration. */
    ret = set_accel_gyro_config(&bmi2_dev);
    bmi2_error_codes_print_result(ret);

    /* Enable accel and gyro sensors. */
    ret = bmi270_sensor_enable(sensor_list, 2, &bmi2_dev);
    bmi2_error_codes_print_result(ret);

    PR_NOTICE("BMI270 initialized successfully on shared I2C1 (3V3_L1 powered)");
    return OPRT_OK;
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */
OPERATE_RET board_bmi270_register(void)
{
    return board_bmi270_init(&g_bmi270_dev);
}

OPERATE_RET board_bmi270_deinit(bmi270_dev_t *dev)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    /* Note: we do NOT deinit the I2C bus because it is shared with ES8311/M5PM1. */
    dev->initialized = false;
    return OPRT_OK;
}

OPERATE_RET board_bmi270_read_data(bmi270_dev_t *dev, bmi270_sensor_data_t *data)
{
    uint16_t int_status = 0;
    struct bmi2_sens_data sensor_data;
    memset(&sensor_data, 0, sizeof(sensor_data));

    if (!dev || !data || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET ret = bmi2_get_int_status(&int_status, &bmi2_dev);
    bmi2_error_codes_print_result(ret);

    if ((int_status & BMI2_ACC_DRDY_INT_MASK) && (int_status & BMI2_GYR_DRDY_INT_MASK)) {
        ret = bmi2_get_sensor_data(&sensor_data, &bmi2_dev);
        bmi2_error_codes_print_result(ret);

        data->acc_x = lsb_to_mps2(sensor_data.acc.x, 16, bmi2_dev.resolution);
        data->acc_y = lsb_to_mps2(sensor_data.acc.y, 16, bmi2_dev.resolution);
        data->acc_z = lsb_to_mps2(sensor_data.acc.z, 16, bmi2_dev.resolution);

        data->gyr_x = lsb_to_dps(sensor_data.gyr.x, 2000, bmi2_dev.resolution);
        data->gyr_y = lsb_to_dps(sensor_data.gyr.y, 2000, bmi2_dev.resolution);
        data->gyr_z = lsb_to_dps(sensor_data.gyr.z, 2000, bmi2_dev.resolution);
    }

    data->temp = 0;
    return OPRT_OK;
}

OPERATE_RET board_bmi270_read_accel(bmi270_dev_t *dev, float *acc_x, float *acc_y, float *acc_z)
{
    uint16_t int_status = 0;
    struct bmi2_sens_data sensor_data;
    memset(&sensor_data, 0, sizeof(sensor_data));

    if (!dev || !acc_x || !acc_y || !acc_z || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET ret = bmi2_get_int_status(&int_status, &bmi2_dev);
    bmi2_error_codes_print_result(ret);

    if ((int_status & BMI2_ACC_DRDY_INT_MASK) && (int_status & BMI2_GYR_DRDY_INT_MASK)) {
        ret = bmi2_get_sensor_data(&sensor_data, &bmi2_dev);
        bmi2_error_codes_print_result(ret);

        *acc_x = lsb_to_mps2(sensor_data.acc.x, 16, bmi2_dev.resolution);
        *acc_y = lsb_to_mps2(sensor_data.acc.y, 16, bmi2_dev.resolution);
        *acc_z = lsb_to_mps2(sensor_data.acc.z, 16, bmi2_dev.resolution);
    }

    return OPRT_OK;
}

OPERATE_RET board_bmi270_read_gyro(bmi270_dev_t *dev, float *gyr_x, float *gyr_y, float *gyr_z)
{
    uint16_t int_status = 0;
    struct bmi2_sens_data sensor_data;
    memset(&sensor_data, 0, sizeof(sensor_data));

    if (!dev || !gyr_x || !gyr_y || !gyr_z || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET ret = bmi2_get_int_status(&int_status, &bmi2_dev);
    bmi2_error_codes_print_result(ret);

    if ((int_status & BMI2_ACC_DRDY_INT_MASK) && (int_status & BMI2_GYR_DRDY_INT_MASK)) {
        ret = bmi2_get_sensor_data(&sensor_data, &bmi2_dev);
        bmi2_error_codes_print_result(ret);

        *gyr_x = lsb_to_dps(sensor_data.gyr.x, 2000, bmi2_dev.resolution);
        *gyr_y = lsb_to_dps(sensor_data.gyr.y, 2000, bmi2_dev.resolution);
        *gyr_z = lsb_to_dps(sensor_data.gyr.z, 2000, bmi2_dev.resolution);
    }

    return OPRT_OK;
}

OPERATE_RET board_bmi270_read_temp(bmi270_dev_t *dev, int16_t *temp)
{
    if (!dev || !temp || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    *temp = 0;
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET board_bmi270_set_power_mode(bmi270_dev_t *dev, bool power_mode)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    bmi2_set_adv_power_save(power_mode, &bmi2_dev);
    g_bmi270_dev.config.power_mode = power_mode;
    return OPRT_OK;
}

OPERATE_RET board_bmi270_force_reset(bmi270_dev_t *dev)
{
    if (!dev) {
        return OPRT_INVALID_PARM;
    }

    bmi2_soft_reset(&bmi2_dev);
    PR_DEBUG("BMI270 force reset completed");
    return OPRT_OK;
}

OPERATE_RET board_bmi270_scan_i2c(TUYA_I2C_NUM_E port)
{
    uint8_t dummy = 0;
    OPERATE_RET ret;

    PR_DEBUG("Scanning I2C bus %d for BMI270...", port);

    ret = tkl_i2c_master_send(port, BMI270_I2C_ADDR, &dummy, 1, 1000);
    if (ret == OPRT_OK) {
        PR_DEBUG("BMI270 found at address 0x%02X", BMI270_I2C_ADDR);
        return OPRT_OK;
    }

    ret = tkl_i2c_master_send(port, BMI270_I2C_ADDR_ALT, &dummy, 1, 1000);
    if (ret == OPRT_OK) {
        PR_DEBUG("BMI270 found at address 0x%02X", BMI270_I2C_ADDR_ALT);
        return OPRT_OK;
    }

    PR_ERR("BMI270 not found on I2C bus %d", port);
    return OPRT_COM_ERROR;
}

bmi270_dev_t *board_bmi270_get_handle(void)
{
    return &g_bmi270_dev;
}

/* ---------------------------------------------------------------------------
 * Private helpers
 * --------------------------------------------------------------------------- */
/**
 * @brief Configure accelerometer and gyroscope parameters.
 */
static int8_t set_accel_gyro_config(struct bmi2_dev *bmi2_dev)
{
    int8_t rslt;
    struct bmi2_sens_config config[2];

    config[ACCEL].type = BMI2_ACCEL;
    config[GYRO].type = BMI2_GYRO;

    rslt = bmi270_get_sensor_config(config, 2, bmi2_dev);
    bmi2_error_codes_print_result(rslt);

    /* Map data-ready interrupt to INT1 pin. */
    rslt = bmi2_map_data_int(BMI2_DRDY_INT, BMI2_INT1, bmi2_dev);
    bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK) {
        /* Accelerometer: 200 Hz, ±16 g, normal avg4, high-performance mode. */
        config[ACCEL].cfg.acc.odr = BMI2_ACC_ODR_200HZ;
        config[ACCEL].cfg.acc.range = BMI2_ACC_RANGE_16G;
        config[ACCEL].cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;
        config[ACCEL].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;

        /* Gyroscope: 200 Hz, ±2000 dps, normal mode. */
        config[GYRO].cfg.gyr.odr = BMI2_GYR_ODR_200HZ;
        config[GYRO].cfg.gyr.range = BMI2_GYR_RANGE_2000;
        config[GYRO].cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;
        config[GYRO].cfg.gyr.noise_perf = BMI2_POWER_OPT_MODE;
        config[GYRO].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;

        rslt = bmi270_set_sensor_config(config, 2, bmi2_dev);
        bmi2_error_codes_print_result(rslt);
    }

    /* Cache configuration in global device handle. */
    g_bmi270_dev.config.acc_odr = config[ACCEL].cfg.acc.odr;
    g_bmi270_dev.config.acc_range = config[ACCEL].cfg.acc.range;
    g_bmi270_dev.config.gyr_odr = config[GYRO].cfg.gyr.odr;
    g_bmi270_dev.config.gyr_range = config[GYRO].cfg.gyr.range;
    g_bmi270_dev.config.power_mode = 0; /* Normal */

    return rslt;
}

/**
 * @brief Convert raw accel LSB to m/s^2.
 */
static float lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width)
{
    float half_scale = ((float)(1 << bit_width) / 2.0f);
    return (GRAVITY_EARTH * val * g_range) / half_scale;
}

/**
 * @brief Convert raw gyro LSB to degrees/second.
 */
static float lsb_to_dps(int16_t val, float dps, uint8_t bit_width)
{
    float half_scale = ((float)(1 << bit_width) / 2.0f);
    return (dps / half_scale) * val;
}
