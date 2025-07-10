/**
 * @file tdl_display_qspi.c
 * @brief TDL display QSPI interface implementation
 *
 * This file implements the QSPI (Quad SPI) interface functionality for the TDL display
 * system. It provides hardware abstraction for displays using QSPI interface, enabling
 * high-speed data transfer through quad SPI communication. The implementation handles
 * QSPI initialization, data transmission, and display controller communication.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tal_api.h"

#if defined(ENABLE_QSPI) && (ENABLE_QSPI == 1)
#include "tkl_qspi.h"
#include "tkl_gpio.h"
#include "tdl_display_manage.h"
#include "tdl_display_driver.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    DISP_QSPI_BASE_CFG_T cfg;
    const uint8_t *init_seq;
} DISP_QSPI_DEV_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

static OPERATE_RET __disp_qspi_gpio_init(DISP_QSPI_BASE_CFG_T *p_cfg)
{
    TUYA_GPIO_BASE_CFG_T pin_cfg;
    OPERATE_RET rt = OPRT_OK;

    if (NULL == p_cfg) {
        return OPRT_INVALID_PARM;
    }

    pin_cfg.mode = TUYA_GPIO_PUSH_PULL;
    pin_cfg.direct = TUYA_GPIO_OUTPUT;
    pin_cfg.level = TUYA_GPIO_LEVEL_LOW;

    TUYA_CALL_ERR_RETURN(tkl_gpio_init(p_cfg->cs_pin, &pin_cfg));
    TUYA_CALL_ERR_RETURN(tkl_gpio_init(p_cfg->dc_pin, &pin_cfg));
    TUYA_CALL_ERR_RETURN(tkl_gpio_init(p_cfg->rst_pin, &pin_cfg));

    return rt;
}

static OPERATE_RET __disp_qspi_init(TUYA_SPI_NUM_E port, uint32_t spi_clk)
{
    OPERATE_RET rt = OPRT_OK;

    /*spi init*/
    TUYA_QSPI_BASE_CFG_T qspi_cfg = {
        .role = TUYA_QSPI_ROLE_MASTER,
        .mode = TUYA_QSPI_MODE0,
        .baudrate = spi_clk,
        .is_dma = 1,
    };

    PR_NOTICE("spi init %d\r\n", qspi_cfg.baudrate);
    TUYA_CALL_ERR_RETURN(tkl_qspi_init(port, &qspi_cfg));

    return rt;
}

static OPERATE_RET __disp_qspi_send_cmd(DISP_QSPI_BASE_CFG_T *p_cfg, uint8_t cmd)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == p_cfg) {
        return OPRT_INVALID_PARM;
    }

    tkl_gpio_write(p_cfg->cs_pin, TUYA_GPIO_LEVEL_LOW);
    tkl_gpio_write(p_cfg->dc_pin, TUYA_GPIO_LEVEL_LOW);

    tkl_qspi_send_cmd(p_cfg->port, cmd);

    tkl_gpio_write(p_cfg->cs_pin, TUYA_GPIO_LEVEL_HIGH);

    return rt;
}

static OPERATE_RET __disp_qspi_send_data(DISP_QSPI_BASE_CFG_T *p_cfg, uint8_t *data, uint32_t data_len)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == p_cfg || NULL == data || data_len == 0) {
        return OPRT_INVALID_PARM;
    }

    tkl_gpio_write(p_cfg->cs_pin, TUYA_GPIO_LEVEL_LOW);
    tkl_gpio_write(p_cfg->dc_pin, TUYA_GPIO_LEVEL_HIGH);

    tkl_qspi_send_data_indirect_mode(p_cfg->port, data, data_len);

    tkl_gpio_write(p_cfg->cs_pin, TUYA_GPIO_LEVEL_HIGH);

    return rt;
}

static void __disp_qspi_set_window(DISP_QSPI_BASE_CFG_T *p_cfg, uint32_t width, uint32_t height)
{
    uint8_t lcd_data[4];

    if (NULL == p_cfg) {
        return;
    }

    lcd_data[0] = 0;
    lcd_data[1] = 0;
    lcd_data[2] = (width >> 8) & 0xFF;
    lcd_data[3] = (width & 0xFF) - 1;
    __disp_qspi_send_cmd(p_cfg, p_cfg->cmd_caset);
    __disp_qspi_send_data(p_cfg, lcd_data, 4);

    lcd_data[0] = 0;
    lcd_data[1] = 0;
    lcd_data[2] = (height >> 8) & 0xFF;
    lcd_data[3] = (height & 0xFF) - 1;
    __disp_qspi_send_cmd(p_cfg, p_cfg->cmd_raset);
    __disp_qspi_send_data(p_cfg, lcd_data, 4);
}

static void __tdd_disp_reset(TUYA_GPIO_NUM_E rst_pin)
{
    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(100);

    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_LOW);
    tal_system_sleep(100);

    tkl_gpio_write(rst_pin, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(100);
}

static void __tdd_disp_init_seq(DISP_QSPI_BASE_CFG_T *p_cfg, const uint8_t *init_seq)
{
    uint8_t *init_line = (uint8_t *)init_seq, *p_data = NULL;
    uint8_t data_len = 0, sleep_time = 0, cmd = 0;

    __tdd_disp_reset(p_cfg->rst_pin);

    while (*init_line) {
        data_len = init_line[0] - 1;
        sleep_time = init_line[1];
        cmd = init_line[2];

        if (data_len) {
            p_data = &init_line[3];
        } else {
            p_data = NULL;
        }

        __disp_qspi_send_cmd(p_cfg, cmd);
        __disp_qspi_send_data(p_cfg, p_data, data_len);

        tal_system_sleep(sleep_time);
        init_line += init_line[0] + 2;
    }
}

static OPERATE_RET __tdd_display_qspi_open(TDD_DISP_DEV_HANDLE_T device)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_QSPI_DEV_T *disp_qspi_dev = NULL;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }
    disp_qspi_dev = (DISP_QSPI_DEV_T *)device;

    TUYA_CALL_ERR_RETURN(__disp_qspi_init(disp_qspi_dev->cfg.port, disp_qspi_dev->cfg.spi_clk));
    TUYA_CALL_ERR_RETURN(__disp_qspi_gpio_init(&(disp_qspi_dev->cfg)));

    __tdd_disp_init_seq(&(disp_qspi_dev->cfg), disp_qspi_dev->init_seq);

    return OPRT_OK;
}

static OPERATE_RET __tdd_display_qspi_flush(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_QSPI_DEV_T *disp_qspi_dev = NULL;

    if (NULL == device || NULL == frame_buff) {
        return OPRT_INVALID_PARM;
    }

    disp_qspi_dev = (DISP_QSPI_DEV_T *)device;

    __disp_qspi_set_window(&disp_qspi_dev->cfg, frame_buff->width, frame_buff->height);
    __disp_qspi_send_cmd(&disp_qspi_dev->cfg, disp_qspi_dev->cfg.cmd_ramwr);
    __disp_qspi_send_data(&disp_qspi_dev->cfg, frame_buff->frame, frame_buff->len);

    return rt;
}

static OPERATE_RET __tdd_display_qspi_close(TDD_DISP_DEV_HANDLE_T device)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tdl_disp_qspi_device_register(char *name, TDD_DISP_QSPI_CFG_T *spi)
{
    OPERATE_RET rt = OPRT_OK;
    DISP_QSPI_DEV_T *disp_qspi_dev = NULL;
    TDD_DISP_DEV_INFO_T disp_qspi_dev_info;

    if (NULL == name || NULL == spi) {
        return OPRT_INVALID_PARM;
    }

    disp_qspi_dev = tal_malloc(sizeof(DISP_QSPI_DEV_T));
    if (NULL == disp_qspi_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memcpy(&disp_qspi_dev->cfg, &spi->cfg, sizeof(DISP_QSPI_BASE_CFG_T));

    disp_qspi_dev->init_seq = spi->init_seq;

    disp_qspi_dev_info.type = TUYA_DISPLAY_QSPI;
    disp_qspi_dev_info.width = spi->cfg.width;
    disp_qspi_dev_info.height = spi->cfg.height;
    disp_qspi_dev_info.fmt = spi->cfg.pixel_fmt;
    disp_qspi_dev_info.rotation = spi->rotation;

    memcpy(&disp_qspi_dev_info.bl, &spi->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));
    memcpy(&disp_qspi_dev_info.power, &spi->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));

    TDD_DISP_INTFS_T disp_qspi_intfs = {
        .open = __tdd_display_qspi_open,
        .flush = __tdd_display_qspi_flush,
        .close = __tdd_display_qspi_close,
    };

    TUYA_CALL_ERR_RETURN(
        tdl_disp_device_register(name, (TDD_DISP_DEV_HANDLE_T)disp_qspi_dev, &disp_qspi_intfs, &disp_qspi_dev_info));

    return OPRT_OK;
}

#endif