/**
 * @file tdd_touch_cst816x.c
 * @brief CST816X series capacitive touch controller driver implementation
 *
 * This file implements the TDD (Tuya Device Driver) layer for the CST816X series
 * capacitive touch controllers (CST816S, CST816D, CST816T, CST820, CST716).
 * It provides initialization, single-point touch reading, and device registration
 * functions with I2C communication interface and gesture support.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "tkl_i2c.h"

#include "tdl_touch_driver.h"
#include "tdd_touch_cst816x.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDD_TOUCH_I2C_CFG_T i2c_cfg;
} TDD_TOUCH_INFO_T;

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
static OPERATE_RET __tdd_i2c_cst816x_open(TDD_TOUCH_DEV_HANDLE_T device)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_TOUCH_INFO_T *info = (TDD_TOUCH_INFO_T *)device;
    TUYA_IIC_BASE_CFG_T cfg;
    uint8_t chip_id = 0, tmp = 0;

    if (info == NULL) {
        return OPRT_INVALID_PARM;
    }

    tdd_touch_i2c_pinmux_config(&(info->i2c_cfg));

    /*i2c init*/
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    TUYA_CALL_ERR_RETURN(tkl_i2c_init(info->i2c_cfg.port, &cfg));

    TUYA_CALL_ERR_RETURN(
        tdd_touch_i2c_port_read(info->i2c_cfg.port, CST816_ADDR, REG_CHIP_ID, 1, &chip_id, sizeof(chip_id)));
    PR_DEBUG("Touch Chip id: 0x%08x\r\n", chip_id);

    tmp = 0x01;
    tdd_touch_i2c_port_write(info->i2c_cfg.port, CST816_ADDR, REG_DIS_AUTOSLEEP, 1, &tmp, 1);

    tmp = IRQ_EN_MOTION;
    tdd_touch_i2c_port_write(info->i2c_cfg.port, CST816_ADDR, REG_IRQ_CTL, 1, &tmp, 1);

    return OPRT_OK;
}

static OPERATE_RET __tdd_i2c_cst816x_read(TDD_TOUCH_DEV_HANDLE_T device, uint8_t max_num, TDL_TOUCH_POS_T *point,
                                          uint8_t *point_num)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_TOUCH_INFO_T *info = (TDD_TOUCH_INFO_T *)device;
    uint8_t read_num;
    uint8_t buf[13];

    if (info == NULL || point == NULL || point_num == NULL || max_num == 0) {
        return OPRT_INVALID_PARM;
    }

    *point_num = 0;

    TUYA_CALL_ERR_RETURN(tdd_touch_i2c_port_read(info->i2c_cfg.port, CST816_ADDR, REG_STATUS, 1, buf, sizeof(buf)));
    /* get point number */
    read_num = buf[REG_TOUCH_NUM] & 3;
    if (read_num > max_num) {
        read_num = max_num;
    } else if (read_num == 0) {
        return 0;
    }

    /* get point coordinates */
    for (uint8_t i = 0; i < read_num; i++) {
        point[i].x = ((buf[REG_XPOS_HIGH] & 0x0f) << 8) + buf[REG_XPOS_LOW];
        point[i].y = ((buf[REG_YPOS_HIGH] & 0x0f) << 8) + buf[REG_YPOS_LOW];
    }

    *point_num = read_num;

    return OPRT_OK;
}

static OPERATE_RET __tdd_i2c_cst816x_close(TDD_TOUCH_DEV_HANDLE_T device)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_TOUCH_INFO_T *info = (TDD_TOUCH_INFO_T *)device;

    if (info == NULL) {
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(tkl_i2c_deinit(info->i2c_cfg.port));

    return OPRT_OK;
}

OPERATE_RET tdd_touch_i2c_cst816x_register(char *name, TDD_TOUCH_I2C_CFG_T *cfg)
{
    TDD_TOUCH_INFO_T *tdd_info = NULL;
    TDD_TOUCH_INTFS_T infs;

    if (name == NULL || cfg == NULL) {
        return OPRT_INVALID_PARM;
    }

    tdd_info = (TDD_TOUCH_INFO_T *)tal_malloc(sizeof(TDD_TOUCH_INFO_T));
    if (NULL == tdd_info) {
        return OPRT_MALLOC_FAILED;
    }
    memset(tdd_info, 0, sizeof(TDD_TOUCH_INFO_T));
    tdd_info->i2c_cfg = *cfg;

    memset(&infs, 0, sizeof(TDD_TOUCH_INTFS_T));
    infs.open = __tdd_i2c_cst816x_open;
    infs.read = __tdd_i2c_cst816x_read;
    infs.close = __tdd_i2c_cst816x_close;

    return tdl_touch_device_register(name, tdd_info, &infs);
}