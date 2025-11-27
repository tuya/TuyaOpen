/**
 * @file tdd_tp_cst92xx.c
 * @brief tdd_tp_cst92xx module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "tal_api.h"

#include "tkl_i2c.h"

#include "tdl_tp_driver.h"
#include "tdd_tp_cst92xx.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define UNACTIVE_LEVEL(x) 	    (((x) == TUYA_GPIO_LEVEL_LOW)? TUYA_GPIO_LEVEL_HIGH: TUYA_GPIO_LEVEL_LOW)

#define CST92XX_ADDR 0x5A

/* CST92XX registers */
#define REG_CST92XX_DATA       0xD000
#define REG_CST92XX_PROJECT_ID 0xD204
#define REG_CST92XX_CMD_MODE   0xD101
#define REG_CST92XX_CHECKCODE  0xD1FC
#define REG_CST92XX_RESOLUTION 0xD1F8

/* CST92XX parameters */
#define CST92XX_ACK_VALUE        (0xAB)
#define CST92XX_MAX_TP_POINTS    (1)
#define CST92XX_DATA_LENGTH      (CST92XX_MAX_TP_POINTS * 5 + 5)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_GPIO_NUM_E  rst_pin;
    TDD_TP_I2C_CFG_T i2c_cfg;
} TDD_TP_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
static void __cst92xx_reset(TUYA_GPIO_NUM_E rst_pin)
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

static OPERATE_RET __cst92xx_read_config(TUYA_I2C_NUM_E port)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t read_buf[16] = {0};

    uint8_t cmd_mode[2] = {0xD1, 0x01};
    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_port_write(port, CST92XX_ADDR, REG_CST92XX_CMD_MODE, 2, cmd_mode, 2));
    tal_system_sleep(10);

    memset(read_buf, 0, sizeof(read_buf));
    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_port_read(port, CST92XX_ADDR, REG_CST92XX_CHECKCODE, 2, read_buf, 4));
    PR_INFO("Checkcode: 0x%02X%02X%02X%02X", read_buf[0], read_buf[1], read_buf[2], read_buf[3]);

    memset(read_buf, 0, sizeof(read_buf));
    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_port_read(port, CST92XX_ADDR, REG_CST92XX_RESOLUTION, 2, read_buf, 4));
    uint16_t res_x = (read_buf[1] << 8) | read_buf[0];
    uint16_t res_y = (read_buf[3] << 8) | read_buf[2];
    PR_INFO("Resolution: %d x %d", res_x, res_y);

    memset(read_buf, 0, sizeof(read_buf));
    TUYA_CALL_ERR_RETURN(tdd_tp_i2c_port_read(port, CST92XX_ADDR, REG_CST92XX_PROJECT_ID, 2, read_buf, 4));
    uint16_t chip_id = (read_buf[3] << 8) | read_buf[2];
    uint32_t project_id = (read_buf[1] << 8) | read_buf[0];
    PR_INFO("Chip ID: 0x%04X, Project ID: 0x%04lX", chip_id, project_id);

    return rt;
}

static OPERATE_RET __tdd_i2c_cst92xx_open(TDD_TP_DEV_HANDLE_T device)
{
    OPERATE_RET rt = OPRT_OK;

    TDD_TP_INFO_T *info = (TDD_TP_INFO_T *)device;

    TUYA_CHECK_NULL_RETURN(info, OPRT_INVALID_PARM);

    // init reset pin
    __cst92xx_reset(info->rst_pin);

    tdd_tp_i2c_pinmux_config(&(info->i2c_cfg));

    /*i2c init*/
    TUYA_IIC_BASE_CFG_T cfg;
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    TUYA_CALL_ERR_RETURN(tkl_i2c_init(info->i2c_cfg.port, &cfg));

    // read CST92XX chip config
    __cst92xx_read_config(info->i2c_cfg.port);

    return rt;
}

static OPERATE_RET __tdd_i2c_cst92xx_read(TDD_TP_DEV_HANDLE_T device, uint8_t max_num, TDL_TP_POS_T *point,
                                          uint8_t *point_num)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_TP_INFO_T *info = (TDD_TP_INFO_T *)device;

    uint8_t data[CST92XX_DATA_LENGTH] = {0};

    TUYA_CALL_ERR_RETURN(
        tdd_tp_i2c_port_read(info->i2c_cfg.port, CST92XX_ADDR, REG_CST92XX_DATA, 2, data, sizeof(data)));

    if (data[6] != CST92XX_ACK_VALUE) {
        *point_num = 0;
        return OPRT_OK;
    }

    uint8_t points = data[5] & 0x7F;
    points = (points > CST92XX_MAX_TP_POINTS) ? CST92XX_MAX_TP_POINTS : points;
    if (points > max_num) {
        points = max_num;
    }

    uint8_t point_cnt = 0;

    for (int i = 0; i < points; i++) {
        uint8_t *p = &data[i * 5 + (i ? 2 : 0)];
        uint8_t status = p[0] & 0x0F;

        if (status == 0x06) {
            point[point_cnt].x = ((p[1] << 4) | (p[3] >> 4));
            point[point_cnt].y = ((p[2] << 4) | (p[3] & 0x0F));
            // PR_DEBUG("Point %d: X=%d, Y=%d", point_cnt, point[point_cnt].x, point[point_cnt].y);
            point_cnt++;
        }
    }

    *point_num = point_cnt;

    return rt;
}

static OPERATE_RET __tdd_i2c_cst92xx_close(TDD_TP_DEV_HANDLE_T device)
{
    OPERATE_RET rt = OPRT_OK;

    return rt;
}

OPERATE_RET tdd_tp_i2c_cst92xx_register(char *name, TDD_TP_CST92XX_INFO_T *cfg)
{
    TDD_TP_INFO_T *tdd_info = NULL;
    TDD_TP_INTFS_T infs;

    TUYA_CHECK_NULL_RETURN(name, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(cfg, OPRT_INVALID_PARM);

    tdd_info = (TDD_TP_INFO_T *)tal_malloc(sizeof(TDD_TP_INFO_T));
    if (NULL == tdd_info) {
        return OPRT_MALLOC_FAILED;
    }
    memset(tdd_info, 0, sizeof(TDD_TP_INFO_T));

    tdd_info->rst_pin = cfg->rst_pin;
    memcpy(&tdd_info->i2c_cfg, &cfg->i2c_cfg, sizeof(TDD_TP_I2C_CFG_T));

    memset(&infs, 0, sizeof(TDD_TP_INTFS_T));
    infs.open = __tdd_i2c_cst92xx_open;
    infs.read = __tdd_i2c_cst92xx_read;
    infs.close = __tdd_i2c_cst92xx_close;

    return tdl_tp_device_register(name, tdd_info, &cfg->tp_cfg, &infs);
}
