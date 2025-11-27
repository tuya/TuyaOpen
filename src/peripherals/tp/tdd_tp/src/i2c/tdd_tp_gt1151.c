/**
 * @file tdd_tp_gt1151.c
 * @brief GT1151 capacitive tp controller driver implementation
 *
 * This file implements the TDD (Tuya Device Driver) layer for the GT1151 capacitive
 * tp controller. It provides initialization, multi-point tp reading, and
 * device registration functions for the GT1151 IC, supporting configurable number
 * of tp points with I2C communication interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "tkl_i2c.h"

#include "tdl_tp_driver.h"
#include "tdd_tp_gt1151.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDD_TP_I2C_CFG_T i2c_cfg;
} TDD_TP_INFO_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static uint8_t sg_point_data[GT1151_POINT_INFO_TOTAL_SIZE] = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __tdd_i2c_gt1151_open(TDD_TP_DEV_HANDLE_T device)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_TP_INFO_T *info = (TDD_TP_INFO_T *)device;
    TUYA_IIC_BASE_CFG_T cfg;
    uint32_t product_id = 0;

    if (info == NULL) {
        return OPRT_INVALID_PARM;
    }

    tdd_tp_i2c_pinmux_config(&(info->i2c_cfg));

    /*i2c init*/
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    TUYA_CALL_ERR_RETURN(tkl_i2c_init(info->i2c_cfg.port, &cfg));

    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_port_read(info->i2c_cfg.port, GT1151_I2C_SLAVE_ADDR, GT1151_PRODUCT_ID, 2,
                                                 (uint8_t *)&product_id, sizeof(product_id)));
    PR_DEBUG("Tp Product id: 0x%08x\r\n", product_id);

    return OPRT_OK;
}

static OPERATE_RET __tdd_i2c_gt1151_read(TDD_TP_DEV_HANDLE_T device, uint8_t max_num, TDL_TP_POS_T *point,
                                         uint8_t *point_num)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_TP_INFO_T *info = (TDD_TP_INFO_T *)device;
    uint8_t read_num, status;

    if (info == NULL || point == NULL || point_num == NULL || max_num == 0) {
        return OPRT_INVALID_PARM;
    }

    read_num = MIN(max_num, GT1151_POINT_INFO_NUM);

    *point_num = 0;

    TUYA_CALL_ERR_RETURN(
        tdd_tp_i2c_port_read(info->i2c_cfg.port, GT1151_I2C_SLAVE_ADDR, GT1151_STATUS, 2, &status, 1));
    if (status == 0 || (status & 0x80) == 0) {
        /* no tp */
        return 0;
    }

    read_num = MIN(read_num, (status & 0x0f));
    memset(sg_point_data, 0, sizeof(sg_point_data));
    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_port_read(info->i2c_cfg.port, GT1151_I2C_SLAVE_ADDR, GT1151_POINT1_REG, 2,
                                                 sg_point_data, sizeof(sg_point_data)));

    /* get point coordinates */
    for (uint8_t i = 0; i < read_num; i++) {
        uint8_t *p_data = &sg_point_data[i * 8];
        point[i].x = (uint16_t)p_data[2] << 8 | p_data[1];
        point[i].y = (uint16_t)p_data[4] << 8 | p_data[3];
    }

    *point_num = read_num;

    // clear status
    sg_point_data[0] = 0;
    TUYA_CALL_ERR_RETURN(
        tdd_tp_i2c_port_write(info->i2c_cfg.port, GT1151_I2C_SLAVE_ADDR, GT1151_STATUS, 2, &sg_point_data[0], 1));

    return OPRT_OK;
}

static OPERATE_RET __tdd_i2c_gt1151_close(TDD_TP_DEV_HANDLE_T device)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_TP_INFO_T *info = (TDD_TP_INFO_T *)device;

    if (info == NULL) {
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(tkl_i2c_deinit(info->i2c_cfg.port));

    return OPRT_OK;
}

OPERATE_RET tdd_tp_i2c_gt1151_register(char *name, TDD_TP_GT1151_INFO_T *cfg)
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
    tdd_info->i2c_cfg = cfg->i2c_cfg;

    memset(&infs, 0, sizeof(TDD_TP_INTFS_T));
    infs.open = __tdd_i2c_gt1151_open;
    infs.read = __tdd_i2c_gt1151_read;
    infs.close = __tdd_i2c_gt1151_close;

    return tdl_tp_device_register(name, tdd_info, &cfg->tp_cfg, &infs);
}