/**
 * @file sh3001.c
 * @brief SH3001 6-axis IMU driver implementation (NiceMCU-T5-0.96IPS)
 * @version 0.1
 * @date 2026-08-10
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#include "sh3001.h"

#include "tal_api.h"
#include "tkl_i2c.h"
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define SH3001_ACC_XL     0x00
#define SH3001_GYRO_XL    0x06
#define SH3001_CHIP_ID    0x0F
#define SH3001_ACC_CONF0  0x22
#define SH3001_ACC_CONF1  0x23
#define SH3001_ACC_CONF2  0x25
#define SH3001_GYRO_CONF0 0x28
#define SH3001_GYRO_CONF1 0x29
#define SH3001_GYRO_CONF2 0x2B

/* ACC_CONF1 ODR */
#define SH3001_ODR_500HZ 0x01
/* ACC_CONF2 range */
#define SH3001_ACC_RANGE_8G 0x03
/* ACC_CONF0: BW = ODR * 0.25, filter enable */
#define SH3001_ACC_CONF0_DEFAULT 0x01

/* Gyro: 500Hz ODR, +/-2000 dps (vendor demo defaults) */
#define SH3001_GYRO_CONF0_DEFAULT 0x01
#define SH3001_GYRO_CONF1_DEFAULT 0x00
#define SH3001_GYRO_CONF2_DEFAULT 0x00

/* Sensitivity used after the config above */
#define SH3001_ACC_LSB_DIV_8G    4096.0f
#define SH3001_GYR_LSB_DIV_2000  16.4f

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Write one register
 * @param[in] dev device handle
 * @param[in] reg register address
 * @param[in] value register value
 * @return OPRT_OK on success
 */
static OPERATE_RET __sh3001_write_reg(sh3001_dev_t *dev, uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};

    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }

    return tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, buf, sizeof(buf), false);
}

/**
 * @brief Read one or more registers
 * @param[in] dev device handle
 * @param[in] reg start register
 * @param[out] buf output buffer
 * @param[in] len byte count
 * @return OPRT_OK on success
 */
static OPERATE_RET __sh3001_read_regs(sh3001_dev_t *dev, uint8_t reg, uint8_t *buf, uint8_t len)
{
    OPERATE_RET rt = OPRT_OK;

    if ((NULL == dev) || (NULL == buf) || (0 == len)) {
        return OPRT_INVALID_PARM;
    }

    rt = tkl_i2c_master_send(dev->i2c_port, dev->i2c_addr, &reg, 1, false);
    if (OPRT_OK != rt) {
        return rt;
    }

    return tkl_i2c_master_receive(dev->i2c_port, dev->i2c_addr, buf, len, false);
}

/**
 * @brief Pack little-endian int16 from two bytes
 * @param[in] lo low byte
 * @param[in] hi high byte
 * @return signed sample
 */
static int16_t __sh3001_to_s16(uint8_t lo, uint8_t hi)
{
    return (int16_t)((uint16_t)lo | ((uint16_t)hi << 8));
}

OPERATE_RET sh3001_get_chip_id(sh3001_dev_t *dev, uint8_t *chip_id)
{
    if ((NULL == dev) || (NULL == chip_id)) {
        return OPRT_INVALID_PARM;
    }

    return __sh3001_read_regs(dev, SH3001_CHIP_ID, chip_id, 1);
}

OPERATE_RET sh3001_init(sh3001_dev_t *dev, TUYA_I2C_NUM_E i2c_port, uint8_t i2c_addr)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t chip_id = 0;
    uint8_t retry = 0;

    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c_port = i2c_port;
    dev->i2c_addr = i2c_addr;
    dev->acc_lsb_div = SH3001_ACC_LSB_DIV_8G;
    dev->gyr_lsb_div = SH3001_GYR_LSB_DIV_2000;

    do {
        rt = sh3001_get_chip_id(dev, &chip_id);
        if ((OPRT_OK == rt) && (SH3001_CHIP_ID_VAL == chip_id)) {
            break;
        }
        retry++;
        tal_system_sleep(10);
    } while (retry < 3);

    if (SH3001_CHIP_ID_VAL != chip_id) {
        PR_ERR("SH3001 CHIP_ID mismatch: 0x%02X (expect 0x%02X), rt=%d", chip_id, SH3001_CHIP_ID_VAL, rt);
        return OPRT_NOT_FOUND;
    }

    /* Accel: filter enable, 500Hz ODR, +/-8g */
    TUYA_CALL_ERR_RETURN(__sh3001_write_reg(dev, SH3001_ACC_CONF0, SH3001_ACC_CONF0_DEFAULT));
    TUYA_CALL_ERR_RETURN(__sh3001_write_reg(dev, SH3001_ACC_CONF1, SH3001_ODR_500HZ));
    TUYA_CALL_ERR_RETURN(__sh3001_write_reg(dev, SH3001_ACC_CONF2, SH3001_ACC_RANGE_8G));

    /* Gyro basic bring-up (POR-safe defaults used by vendor demos) */
    TUYA_CALL_ERR_RETURN(__sh3001_write_reg(dev, SH3001_GYRO_CONF0, SH3001_GYRO_CONF0_DEFAULT));
    TUYA_CALL_ERR_RETURN(__sh3001_write_reg(dev, SH3001_GYRO_CONF1, SH3001_GYRO_CONF1_DEFAULT));
    TUYA_CALL_ERR_RETURN(__sh3001_write_reg(dev, SH3001_GYRO_CONF2, SH3001_GYRO_CONF2_DEFAULT));

    dev->initialized = true;
    PR_NOTICE("SH3001 init OK (addr=0x%02X, CHIP_ID=0x%02X)", i2c_addr, chip_id);
    return OPRT_OK;
}

OPERATE_RET sh3001_read_accel(sh3001_dev_t *dev, float *x, float *y, float *z)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t raw[6] = {0};

    if ((NULL == dev) || (false == dev->initialized) || (NULL == x) || (NULL == y) || (NULL == z)) {
        return OPRT_INVALID_PARM;
    }

    rt = __sh3001_read_regs(dev, SH3001_ACC_XL, raw, sizeof(raw));
    if (OPRT_OK != rt) {
        return rt;
    }

    *x = (float)__sh3001_to_s16(raw[0], raw[1]) / dev->acc_lsb_div;
    *y = (float)__sh3001_to_s16(raw[2], raw[3]) / dev->acc_lsb_div;
    *z = (float)__sh3001_to_s16(raw[4], raw[5]) / dev->acc_lsb_div;
    return OPRT_OK;
}

OPERATE_RET sh3001_read_gyro(sh3001_dev_t *dev, float *x, float *y, float *z)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t raw[6] = {0};

    if ((NULL == dev) || (false == dev->initialized) || (NULL == x) || (NULL == y) || (NULL == z)) {
        return OPRT_INVALID_PARM;
    }

    rt = __sh3001_read_regs(dev, SH3001_GYRO_XL, raw, sizeof(raw));
    if (OPRT_OK != rt) {
        return rt;
    }

    *x = (float)__sh3001_to_s16(raw[0], raw[1]) / dev->gyr_lsb_div;
    *y = (float)__sh3001_to_s16(raw[2], raw[3]) / dev->gyr_lsb_div;
    *z = (float)__sh3001_to_s16(raw[4], raw[5]) / dev->gyr_lsb_div;
    return OPRT_OK;
}

OPERATE_RET sh3001_read_sensor_data(sh3001_dev_t *dev, sh3001_data_t *data)
{
    OPERATE_RET rt = OPRT_OK;

    if ((NULL == dev) || (NULL == data)) {
        return OPRT_INVALID_PARM;
    }

    rt = sh3001_read_accel(dev, &data->acc_x, &data->acc_y, &data->acc_z);
    if (OPRT_OK != rt) {
        return rt;
    }

    return sh3001_read_gyro(dev, &data->gyr_x, &data->gyr_y, &data->gyr_z);
}
