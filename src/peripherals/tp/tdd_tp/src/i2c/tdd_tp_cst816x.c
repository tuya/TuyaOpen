/**
 * @file tdd_tp_cst816x.c
 * @brief CST816X series capacitive tp controller driver implementation
 *
 * This file implements the TDD (Tuya Device Driver) layer for the CST816X series
 * capacitive tp controllers (CST816S, CST816D, CST816T, CST820, CST716).
 * It provides initialization, single-point tp reading, and device registration
 * functions with I2C communication interface and gesture support.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "tkl_i2c.h"

#include "tdl_tp_driver.h"
#include "tdd_tp_cst816x.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define UNACTIVE_LEVEL(x) 	    (((x) == TUYA_GPIO_LEVEL_LOW)? TUYA_GPIO_LEVEL_HIGH: TUYA_GPIO_LEVEL_LOW)

#define CST816_POINT_NUM        1

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_GPIO_NUM_E  rst_pin;
    TUYA_GPIO_NUM_E  intr_pin;
    TDD_TP_I2C_CFG_T i2c_cfg;
} TDD_TP_INFO_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
// static const uint8_t CST820_CHIP_ID = 0xB7;
// static const uint8_t CST816S_CHIP_ID = 0xB4;
// static const uint8_t CST816D_CHIP_ID = 0xB6;
// static const uint8_t CST816T_CHIP_ID = 0xB5;
// static const uint8_t CST716_CHIP_ID  = 0x20;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __cst816x_reset(TUYA_GPIO_NUM_E rst_pin)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T gpio_cfg;    

    if(rst_pin >= TUYA_GPIO_NUM_MAX) {
        return;
    }

    gpio_cfg.mode   = TUYA_GPIO_PUSH_PULL;
    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.level  = TUYA_GPIO_LEVEL_HIGH;
    TUYA_CALL_ERR_LOG(tkl_gpio_init(rst_pin, &gpio_cfg));

    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_LOW);
    tal_system_sleep(5);
    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(50);
}

static OPERATE_RET __tdd_i2c_cst816x_open(TDD_TP_DEV_HANDLE_T device)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_TP_INFO_T *info = (TDD_TP_INFO_T *)device;
    TUYA_IIC_BASE_CFG_T cfg;
    uint8_t chip_id = 0, tmp = 0;

    if (info == NULL) {
        return OPRT_INVALID_PARM;
    }

    // init reset pin
    __cst816x_reset(info->rst_pin);

    tdd_tp_i2c_pinmux_config(&(info->i2c_cfg));

    /*i2c init*/
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    TUYA_CALL_ERR_RETURN(tkl_i2c_init(info->i2c_cfg.port, &cfg));

    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_port_read(info->i2c_cfg.port, CST816_ADDR, \
                                              REG_CHIP_ID, 1, &chip_id, sizeof(chip_id)));
    PR_DEBUG("Tp Chip id: 0x%08x\r\n", chip_id);

    tmp = 0x01;
    tdd_tp_i2c_port_write(info->i2c_cfg.port, CST816_ADDR, REG_DIS_AUTOSLEEP, 1, &tmp, 1);

    if(info->intr_pin < TUYA_GPIO_NUM_MAX) {
        TUYA_GPIO_BASE_CFG_T gpio_cfg;

        gpio_cfg.mode   = TUYA_GPIO_PULLUP;
        gpio_cfg.direct = TUYA_GPIO_INPUT;
        TUYA_CALL_ERR_LOG(tkl_gpio_init(info->intr_pin, &gpio_cfg));

        tmp = IRQ_EN_MOTION;
        tdd_tp_i2c_port_write(info->i2c_cfg.port, CST816_ADDR, REG_IRQ_CTL, 1, &tmp, 1);
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_i2c_cst816x_read(TDD_TP_DEV_HANDLE_T device, uint8_t max_num, TDL_TP_POS_T *point,
                                          uint8_t *point_num)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_TP_INFO_T *info = (TDD_TP_INFO_T *)device;
    uint8_t read_num =0, finger_num = 0;
    uint8_t buf[13];

    if (info == NULL || point == NULL || point_num == NULL || max_num == 0) {
        return OPRT_INVALID_PARM;
    }

    if(info->intr_pin < TUYA_GPIO_NUM_MAX) { 
        TUYA_GPIO_LEVEL_E intr_lv = TUYA_GPIO_LEVEL_NONE;

        tkl_gpio_read(info->intr_pin, &intr_lv);
        if(TUYA_GPIO_LEVEL_HIGH == intr_lv) { // no dectect touch intrrupt
            *point_num = 0;
            return OPRT_OK;
        }
    }

    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_port_read(info->i2c_cfg.port, CST816_ADDR, REG_STATUS, 1, buf, sizeof(buf)));

    /* get point number */
    read_num = CST816_POINT_NUM;
    if (read_num > max_num) {
        read_num = max_num;
    } else if (read_num == 0) {
        *point_num = 0;
        return OPRT_OK;
    }

    finger_num = buf[REG_FINGER_NUM];

    if(finger_num) {
        /* get point coordinates */
        for (uint8_t i = 0; i < read_num; i++) {
            point[i].x = ((buf[REG_XPOS_HIGH] & 0x0f) << 8) + buf[REG_XPOS_LOW];
            point[i].y = ((buf[REG_YPOS_HIGH] & 0x0f) << 8) + buf[REG_YPOS_LOW];
        }

        *point_num = read_num;
    }else {
        *point_num = 0;
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_i2c_cst816x_close(TDD_TP_DEV_HANDLE_T device)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_TP_INFO_T *info = (TDD_TP_INFO_T *)device;

    if (info == NULL) {
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(tkl_i2c_deinit(info->i2c_cfg.port));

    return OPRT_OK;
}

OPERATE_RET tdd_tp_i2c_cst816x_register(char *name, TDD_TP_CST816X_INFO_T *cfg)
{
    TDD_TP_INFO_T *tdd_info = NULL;
    TDD_TP_INTFS_T infs;

    if (name == NULL || cfg == NULL) {
        return OPRT_INVALID_PARM;
    }

    tdd_info = (TDD_TP_INFO_T *)tal_malloc(sizeof(TDD_TP_INFO_T));
    if (NULL == tdd_info) {
        return OPRT_MALLOC_FAILED;
    }
    memset(tdd_info, 0, sizeof(TDD_TP_INFO_T));
    tdd_info->rst_pin  = cfg->rst_pin;
    tdd_info->intr_pin = cfg->intr_pin;
    memcpy(&tdd_info->i2c_cfg, &cfg->i2c_cfg, sizeof(TDD_TP_I2C_CFG_T));

    memset(&infs, 0, sizeof(TDD_TP_INTFS_T));
    infs.open = __tdd_i2c_cst816x_open;
    infs.read = __tdd_i2c_cst816x_read;
    infs.close = __tdd_i2c_cst816x_close;

    return tdl_tp_device_register(name, tdd_info, &cfg->tp_cfg, &infs);
}