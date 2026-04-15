/**
 * @file board_bmi220_api.c
 * @author Tuya Inc.
 * @brief BMI220 (chip ID 0x26) sensor driver for TUYA_T5AI_PIXEL board.
 *        Uses the Bosch BMI2 library with BMI270 config file, but accepts
 *        chip ID 0x26 instead of 0x24.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "board_bmi220_api.h"
#include "tal_log.h"
#include "tkl_pinmux.h"
#include "tkl_i2c.h"
#include "tkl_system.h"
#include "bmi270.h"
#include "bmi270_common.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define GRAVITY_EARTH (9.80665f)

#define ACCEL UINT8_C(0x00)
#define GYRO  UINT8_C(0x01)

/***********************************************************
***********************variable define**********************
***********************************************************/
static bmi220_dev_t g_bmi220_dev = {0};

/* Bosch BMI2 device structure (same as BMI270 driver uses) */
static struct bmi2_dev bmi2_dev_220;
static uint8_t sensor_list_220[2] = {BMI2_ACCEL, BMI2_GYRO};

static TUYA_IIC_BASE_CFG_T g_bmi220_i2c_cfg = {
    .role = TUYA_IIC_MODE_MASTER, .speed = TUYA_IIC_BUS_SPEED_400K, .addr_width = TUYA_IIC_ADDRESS_7BIT};

/* External: BMI270 config file (we reuse it for chip ID 0x26) */
extern const uint8_t bmi260_config_file[];

/***********************************************************
***********************static functions*********************
***********************************************************/

static float lsb_to_mps2(int16_t val, float g_range, uint8_t bit_width)
{
    float half_scale = ((float)(1 << bit_width) / 2.0f);
    return (GRAVITY_EARTH * val * g_range) / half_scale;
}

static float lsb_to_dps(int16_t val, float dps, uint8_t bit_width)
{
    float half_scale = ((float)(1 << bit_width) / 2.0f);
    return (dps / half_scale) * val;
}

static int8_t set_accel_gyro_config_220(struct bmi2_dev *dev)
{
    int8_t rslt;
    struct bmi2_sens_config config[2];

    config[ACCEL].type = BMI2_ACCEL;
    config[GYRO].type = BMI2_GYRO;

    rslt = bmi270_get_sensor_config(config, 2, dev);
    if (rslt != BMI2_OK) {
        PR_ERR("BMI220: get sensor config failed: %d", rslt);
        return rslt;
    }

    rslt = bmi2_map_data_int(BMI2_DRDY_INT, BMI2_INT1, dev);
    if (rslt != BMI2_OK) {
        PR_ERR("BMI220: map data int failed: %d", rslt);
        return rslt;
    }

    /* Accelerometer config */
    config[ACCEL].cfg.acc.odr = BMI2_ACC_ODR_200HZ;
    config[ACCEL].cfg.acc.range = BMI2_ACC_RANGE_16G;
    config[ACCEL].cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;
    config[ACCEL].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;

    /* Gyroscope config */
    config[GYRO].cfg.gyr.odr = BMI2_GYR_ODR_200HZ;
    config[GYRO].cfg.gyr.range = BMI2_GYR_RANGE_2000;
    config[GYRO].cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;
    config[GYRO].cfg.gyr.noise_perf = BMI2_POWER_OPT_MODE;
    config[GYRO].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;

    rslt = bmi270_set_sensor_config(config, 2, dev);
    if (rslt != BMI2_OK) {
        PR_ERR("BMI220: set sensor config failed: %d", rslt);
    }

    return rslt;
}

/***********************************************************
***********************public functions*********************
***********************************************************/

OPERATE_RET board_bmi220_init(bmi220_dev_t *dev)
{
    OPERATE_RET ret;
    int8_t rslt;

    if (!dev) {
        PR_ERR("BMI220: Invalid device pointer");
        return OPRT_INVALID_PARM;
    }

    if (dev->initialized) {
        return OPRT_OK;
    }

    /* Configure I2C pins */
    tkl_io_pinmux_config(TUYA_GPIO_NUM_20, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_21, TUYA_IIC0_SDA);

    ret = tkl_i2c_init(BMI220_I2C_PORT, &g_bmi220_i2c_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("BMI220: Failed to initialize I2C: %d", ret);
        return ret;
    }

    dev->i2c_port = BMI220_I2C_PORT;
    dev->i2c_addr = BMI220_I2C_ADDR;

    /* Initialize BMI2 interface (sets up I2C read/write callbacks) */
    bmi2_dev_220.intf_ptr = &(dev->i2c_port);
    rslt = bmi2_interface_init(&bmi2_dev_220, BMI2_I2C_INTF);
    if (rslt != BMI2_OK) {
        PR_ERR("BMI220: interface init failed: %d", rslt);
        return OPRT_COM_ERROR;
    }

    /* Key: set expected chip_id to 0x26 (our actual chip) instead of BMI270's 0x24 */
    bmi2_dev_220.chip_id = BMI220_CHIP_ID;

    /* Use BMI270 config file (same BMI2 family, compatible internal engine) */
    bmi2_dev_220.config_file_ptr = bmi260_config_file;

    /* Call bmi270_init-equivalent: set config_size and call bmi2_sec_init */
    /* We replicate what bmi270_init() does, but with our chip_id */
    {
        /* Get config file size - BMI270 config is 8192 bytes */
        bmi2_dev_220.config_size = 8192;

        /* Enable variant features (same as BMI270) */
        bmi2_dev_220.variant_feature = BMI2_GYRO_CROSS_SENS_ENABLE | BMI2_CRT_RTOSK_ENABLE;

        /* I2C mode: no dummy byte */
        bmi2_dev_220.dummy_byte = 0;

        PR_NOTICE("BMI220: Uploading config file (chip_id=0x%02X)...", BMI220_CHIP_ID);

        /* bmi2_sec_init will: read chip_id, verify match, soft reset, upload config, check status */
        rslt = bmi2_sec_init(&bmi2_dev_220);
        if (rslt != BMI2_OK) {
            PR_ERR("BMI220: bmi2_sec_init failed: %d (chip_id read=0x%02X)", rslt, bmi2_dev_220.chip_id);
            return OPRT_COM_ERROR;
        }
        PR_NOTICE("BMI220: Config file uploaded successfully");
    }

    /* Configure accel and gyro */
    rslt = set_accel_gyro_config_220(&bmi2_dev_220);
    if (rslt != BMI2_OK) {
        PR_ERR("BMI220: sensor config failed: %d", rslt);
        return OPRT_COM_ERROR;
    }

    /* Enable accel and gyro */
    rslt = bmi270_sensor_enable(sensor_list_220, 2, &bmi2_dev_220);
    if (rslt != BMI2_OK) {
        PR_ERR("BMI220: sensor enable failed: %d", rslt);
        return OPRT_COM_ERROR;
    }

    dev->initialized = true;
    PR_NOTICE("BMI220: Initialized successfully (Chip ID: 0x%02X, ACC:16G@200Hz, GYR:2000dps@200Hz)",
              BMI220_CHIP_ID);
    return OPRT_OK;
}

OPERATE_RET board_bmi220_register(void)
{
    return board_bmi220_init(&g_bmi220_dev);
}

OPERATE_RET board_bmi220_deinit(bmi220_dev_t *dev)
{
    if (!dev || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    tkl_i2c_deinit(dev->i2c_port);
    dev->initialized = false;
    return OPRT_OK;
}

OPERATE_RET board_bmi220_read_data(bmi220_dev_t *dev, bmi220_sensor_data_t *data)
{
    int8_t rslt;
    struct bmi2_sens_data sensor_data = {{0}};

    if (!dev || !data || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    rslt = bmi2_get_sensor_data(&sensor_data, &bmi2_dev_220);
    if (rslt != BMI2_OK) {
        return OPRT_COM_ERROR;
    }

    data->acc_x = lsb_to_mps2(sensor_data.acc.x, 16, bmi2_dev_220.resolution);
    data->acc_y = lsb_to_mps2(sensor_data.acc.y, 16, bmi2_dev_220.resolution);
    data->acc_z = lsb_to_mps2(sensor_data.acc.z, 16, bmi2_dev_220.resolution);
    data->gyr_x = lsb_to_dps(sensor_data.gyr.x, 2000, bmi2_dev_220.resolution);
    data->gyr_y = lsb_to_dps(sensor_data.gyr.y, 2000, bmi2_dev_220.resolution);
    data->gyr_z = lsb_to_dps(sensor_data.gyr.z, 2000, bmi2_dev_220.resolution);
    data->temp = 0;

    return OPRT_OK;
}

OPERATE_RET board_bmi220_read_accel(bmi220_dev_t *dev, float *acc_x, float *acc_y, float *acc_z)
{
    int8_t rslt;
    struct bmi2_sens_data sensor_data = {{0}};

    if (!dev || !acc_x || !acc_y || !acc_z || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    rslt = bmi2_get_sensor_data(&sensor_data, &bmi2_dev_220);
    if (rslt != BMI2_OK) {
        return OPRT_COM_ERROR;
    }

    *acc_x = lsb_to_mps2(sensor_data.acc.x, 16, bmi2_dev_220.resolution);
    *acc_y = lsb_to_mps2(sensor_data.acc.y, 16, bmi2_dev_220.resolution);
    *acc_z = lsb_to_mps2(sensor_data.acc.z, 16, bmi2_dev_220.resolution);

    return OPRT_OK;
}

OPERATE_RET board_bmi220_read_gyro(bmi220_dev_t *dev, float *gyr_x, float *gyr_y, float *gyr_z)
{
    int8_t rslt;
    struct bmi2_sens_data sensor_data = {{0}};

    if (!dev || !gyr_x || !gyr_y || !gyr_z || !dev->initialized) {
        return OPRT_INVALID_PARM;
    }

    rslt = bmi2_get_sensor_data(&sensor_data, &bmi2_dev_220);
    if (rslt != BMI2_OK) {
        return OPRT_COM_ERROR;
    }

    *gyr_x = lsb_to_dps(sensor_data.gyr.x, 2000, bmi2_dev_220.resolution);
    *gyr_y = lsb_to_dps(sensor_data.gyr.y, 2000, bmi2_dev_220.resolution);
    *gyr_z = lsb_to_dps(sensor_data.gyr.z, 2000, bmi2_dev_220.resolution);

    return OPRT_OK;
}

bmi220_dev_t *board_bmi220_get_handle(void)
{
    return &g_bmi220_dev;
}

bool board_bmi220_is_ready(bmi220_dev_t *dev)
{
    if (!dev) {
        return false;
    }
    return dev->initialized;
}

OPERATE_RET board_bmi220_scan_i2c(TUYA_I2C_NUM_E port)
{
    OPERATE_RET ret;
    uint8_t reg = 0x00;
    uint8_t chip_id = 0;

    PR_DEBUG("BMI220: Scanning I2C bus %d...", port);

    ret = tkl_i2c_master_send(port, BMI220_I2C_ADDR, &reg, 1, FALSE);
    if (ret == OPRT_OK) {
        ret = tkl_i2c_master_receive(port, BMI220_I2C_ADDR, &chip_id, 1, TRUE);
        if (ret == OPRT_OK) {
            PR_DEBUG("BMI220: Found at 0x%02X, chip_id=0x%02X", BMI220_I2C_ADDR, chip_id);
            return OPRT_OK;
        }
    }

    ret = tkl_i2c_master_send(port, BMI220_I2C_ADDR_ALT, &reg, 1, FALSE);
    if (ret == OPRT_OK) {
        ret = tkl_i2c_master_receive(port, BMI220_I2C_ADDR_ALT, &chip_id, 1, TRUE);
        if (ret == OPRT_OK) {
            PR_DEBUG("BMI220: Found at 0x%02X, chip_id=0x%02X", BMI220_I2C_ADDR_ALT, chip_id);
            return OPRT_OK;
        }
    }

    PR_ERR("BMI220: Not found on I2C bus %d", port);
    return OPRT_COM_ERROR;
}
