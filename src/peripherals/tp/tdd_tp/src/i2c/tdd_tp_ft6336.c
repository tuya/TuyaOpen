/**
 * @file tdd_tp_ft6336.c
 * @brief FT6336 capacitive tp controller driver implementation
 *
 * This file implements the TDD (Tuya Device Driver) layer for the FT6336 capacitive
 * tp controller. It provides initialization, multi-point tp reading, and device
 * registration functions with I2C communication interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "tkl_i2c.h"

#include "tdl_tp_driver.h"
#include "tdd_tp_ft6336.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define UNACTIVE_LEVEL(x) (((x) == TUYA_GPIO_LEVEL_LOW) ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW)

#define FT6336_POINT_NUM 2

/* Touch point registers */
#define FT6336_TP1_REG 0x03
#define FT6336_TP2_REG 0x09

/* Touch event types */
#define FT6336_EVENT_PRESS_DOWN 0x00
#define FT6336_EVENT_LIFT_UP    0x01
#define FT6336_EVENT_CONTACT    0x02
#define FT6336_EVENT_NO_EVENT   0x03

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
static const uint8_t sg_tp_reg_tbl[FT6336_POINT_NUM] = {FT6336_TP1_REG, FT6336_TP2_REG};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __ft6336_reset(TUYA_GPIO_NUM_E rst_pin)
{
    OPERATE_RET          rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T gpio_cfg;

    if (rst_pin >= TUYA_GPIO_NUM_MAX) {
        return;
    }

    gpio_cfg.mode   = TUYA_GPIO_PUSH_PULL;
    gpio_cfg.direct = TUYA_GPIO_OUTPUT;
    gpio_cfg.level  = TUYA_GPIO_LEVEL_HIGH;
    TUYA_CALL_ERR_LOG(tkl_gpio_init(rst_pin, &gpio_cfg));

    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_LOW);
    tal_system_sleep(10);
    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(300);
}

static OPERATE_RET __tdd_i2c_ft6336_open(TDD_TP_DEV_HANDLE_T device)
{
    OPERATE_RET         rt   = OPRT_OK;
    TDD_TP_INFO_T      *info = (TDD_TP_INFO_T *)device;
    TUYA_IIC_BASE_CFG_T cfg;
    uint8_t             chip_id = 0, firmware_id = 0;

    if (info == NULL) {
        return OPRT_INVALID_PARM;
    }

    // init reset pin
    __ft6336_reset(info->rst_pin);

    tdd_tp_i2c_pinmux_config(&(info->i2c_cfg));

    /*i2c init*/
    cfg.role       = TUYA_IIC_MODE_MASTER;
    cfg.speed      = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    TUYA_CALL_ERR_RETURN(tkl_i2c_init(info->i2c_cfg.port, &cfg));

    tdd_tp_i2c_port_read(info->i2c_cfg.port, FT6336_I2C_ADDR, FT6336_REG_CIPHER, 1, &chip_id, sizeof(chip_id));
    PR_DEBUG("Tp Chip id: 0x%02x\r\n", chip_id);

    tdd_tp_i2c_port_read(info->i2c_cfg.port, FT6336_I2C_ADDR, FT6336_REG_FIRMID, 1, &firmware_id, sizeof(firmware_id));
    PR_DEBUG("Tp Firmware id: 0x%02x\r\n", firmware_id);

    if (info->intr_pin < TUYA_GPIO_NUM_MAX) {
        TUYA_GPIO_BASE_CFG_T gpio_cfg;

        gpio_cfg.mode   = TUYA_GPIO_PULLUP;
        gpio_cfg.direct = TUYA_GPIO_INPUT;
        TUYA_CALL_ERR_LOG(tkl_gpio_init(info->intr_pin, &gpio_cfg));
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_i2c_ft6336_read(TDD_TP_DEV_HANDLE_T device, uint8_t max_num, TDL_TP_POS_T *point,
                                         uint8_t *point_num)
{
    OPERATE_RET    rt       = OPRT_OK;
    TDD_TP_INFO_T *info     = (TDD_TP_INFO_T *)device;
    uint8_t        read_num = 0, td_status = 0;
    uint8_t        buf[4];
    uint8_t        event;

    if (info == NULL || point == NULL || point_num == NULL || max_num == 0) {
        return OPRT_INVALID_PARM;
    }

    if (info->intr_pin < TUYA_GPIO_NUM_MAX) {
        TUYA_GPIO_LEVEL_E intr_lv = TUYA_GPIO_LEVEL_NONE;

        tkl_gpio_read(info->intr_pin, &intr_lv);
        if (TUYA_GPIO_LEVEL_HIGH == intr_lv) { // no detect touch interrupt
            *point_num = 0;
            return OPRT_OK;
        }
    }

    TUYA_CALL_ERR_RETURN(
        tdd_tp_i2c_port_read(info->i2c_cfg.port, FT6336_I2C_ADDR, FT6336_REG_TD_STATUS, 1, &td_status, 1));

    /* get point number */
    read_num = td_status & 0x0F;
    if (read_num > FT6336_POINT_NUM) {
        read_num = FT6336_POINT_NUM;
    }
    if (read_num > max_num) {
        read_num = max_num;
    }
    if (read_num == 0) {
        *point_num = 0;
        return OPRT_OK;
    }

    *point_num = 0;

    /* get point coordinates */
    for (uint8_t i = 0; i < read_num; i++) {
        TUYA_CALL_ERR_RETURN(tdd_tp_i2c_port_read(info->i2c_cfg.port, FT6336_I2C_ADDR, sg_tp_reg_tbl[i], 1, buf, 4));

        /* check event type */
        event = (buf[0] >> 6) & 0x03;
        if (event == FT6336_EVENT_PRESS_DOWN || event == FT6336_EVENT_CONTACT) {
            point[*point_num].x = ((buf[0] & 0x0f) << 8) + buf[1];
            point[*point_num].y = ((buf[2] & 0x0f) << 8) + buf[3];
            (*point_num)++;
        }
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_i2c_ft6336_close(TDD_TP_DEV_HANDLE_T device)
{
    OPERATE_RET    rt   = OPRT_OK;
    TDD_TP_INFO_T *info = (TDD_TP_INFO_T *)device;

    if (info == NULL) {
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(tkl_i2c_deinit(info->i2c_cfg.port));

    return OPRT_OK;
}

OPERATE_RET tdd_tp_i2c_ft6336_register(char *name, TDD_TP_FT6336_INFO_T *cfg)
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
    infs.open  = __tdd_i2c_ft6336_open;
    infs.read  = __tdd_i2c_ft6336_read;
    infs.close = __tdd_i2c_ft6336_close;

    return tdl_tp_device_register(name, tdd_info, &cfg->tp_cfg, &infs);
}
