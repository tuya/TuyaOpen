/**
 * @file tdd_tp_i2c.c
 * @brief I2C communication utilities for tp controller devices
 *
 * This file provides I2C communication functions for tp controller devices
 * in the TDD (Tuya Device Driver) layer. It includes pin multiplexing configuration,
 * I2C read/write operations with register address support for various tp ICs.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tal_api.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"
#include "tdd_tp_i2c.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
void tdd_tp_i2c_pinmux_config(TDD_TP_I2C_CFG_T *cfg)
{
    if (cfg == NULL) {
        return;
    }

    if (cfg->scl_pin < TUYA_IO_PIN_MAX) {
        if (cfg->port == TUYA_I2C_NUM_0) {
            tkl_io_pinmux_config(cfg->scl_pin, TUYA_IIC0_SCL);
        } else if (cfg->port == TUYA_I2C_NUM_1) {
            tkl_io_pinmux_config(cfg->scl_pin, TUYA_IIC1_SCL);
        } else if (cfg->port == TUYA_I2C_NUM_2) {
            tkl_io_pinmux_config(cfg->scl_pin, TUYA_IIC2_SCL);
        } else {
            ;
        }
    }

    if (cfg->sda_pin < TUYA_IO_PIN_MAX) {
        if (cfg->port == TUYA_I2C_NUM_0) {
            tkl_io_pinmux_config(cfg->sda_pin, TUYA_IIC0_SDA);
        } else if (cfg->port == TUYA_I2C_NUM_1) {
            tkl_io_pinmux_config(cfg->sda_pin, TUYA_IIC1_SDA);
        } else if (cfg->port == TUYA_I2C_NUM_2) {
            tkl_io_pinmux_config(cfg->sda_pin, TUYA_IIC2_SDA);
        } else {
            ;
        }
    }

    return;
}

OPERATE_RET tdd_tp_i2c_port_read(TUYA_I2C_NUM_E port, uint16_t dev_addr, uint16_t reg_addr, uint8_t reg_addr_len,
                                    uint8_t *data, uint8_t data_len)
{
    OPERATE_RET ret = OPRT_OK;
    uint8_t cmd_bytes[2];

    if (1 == reg_addr_len) {
        cmd_bytes[0] = (uint8_t)(reg_addr & 0xFF);
    } else if (2 == reg_addr_len) {
        cmd_bytes[0] = (uint8_t)(reg_addr >> 8);
        cmd_bytes[1] = (uint8_t)(reg_addr & 0xFF);
    } else {
        return OPRT_INVALID_PARM;
    }

    ret = tkl_i2c_master_send(port, dev_addr, cmd_bytes, reg_addr_len, FALSE);
    if (ret != OPRT_OK) {
        PR_ERR("send addr fail");
        return ret;
    }

    ret = tkl_i2c_master_receive(port, dev_addr, data, data_len, FALSE);
    if (ret != OPRT_OK) {
        PR_ERR("send data fail");
    }

    return ret;
}

OPERATE_RET tdd_tp_i2c_port_write(TUYA_I2C_NUM_E port, uint16_t dev_addr, uint16_t reg_addr, uint8_t reg_addr_len,
                                     uint8_t *data, uint8_t data_len)
{
    OPERATE_RET ret = OPRT_OK;
    uint8_t *buf = NULL;

    buf = (uint8_t *)tal_malloc(data_len + 2);
    if (buf == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    if (1 == reg_addr_len) {
        buf[0] = (uint8_t)(reg_addr & 0xFF);
        memcpy(&buf[1], data, data_len);
    } else if (2 == reg_addr_len) {
        buf[0] = (uint8_t)(reg_addr >> 8);
        buf[1] = (uint8_t)(reg_addr & 0xFF);
        memcpy(&buf[2], data, data_len);
    } else {
        tal_free(buf);
        return OPRT_INVALID_PARM;
    }

    ret = tkl_i2c_master_send(port, dev_addr, buf, data_len + reg_addr_len, FALSE);

    tal_free(buf);

    return ret;
}