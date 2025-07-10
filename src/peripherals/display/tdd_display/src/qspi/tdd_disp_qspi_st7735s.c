/**
 * @file tdd_disp_qspi_st7735s.c
 * @brief ST7735S LCD driver implementation with QSPI interface
 *
 * This file provides the implementation for ST7735S TFT LCD displays using QSPI
 * (Quad SPI) interface. It includes the initialization sequence, display control
 * functions, and hardware-specific configurations for ST7735S displays, enabling
 * high-speed data transfer through quad SPI communication.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "tal_log.h"

#include "tdd_disp_st7735s.h"
#include "tdl_display_driver.h"

/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cST7735S_INIT_SEQ[] = {
    1,    0,    0x01, 1,    100,  0x11, 4,    100,  0xB1, 0x02, 0x35, 0x36, 4,    0,    0xB2, 0x02, 0x35, 0x36, 7,
    0,    0xB3, 0x02, 0x35, 0x36, 0x02, 0x35, 0x36, 2,    0,    0xB4, 0x00, 4,    0,    0xC0, 0xa2, 0x02, 0x84, 2,
    0,    0xC1, 0xC5, 3,    0,    0xC2, 0x0D, 0x00, 3,    0,    0xC3, 0x8A, 0x2A, 3,    0,    0xC4, 0x8D, 0xEE, 2,
    0,    0xC5, 0x02, 17,   0,    0xE0, 0x12, 0x1C, 0x10, 0x18, 0x33, 0x2C, 0x25, 0x28, 0x28, 0x27, 0x2F, 0x3C, 0x00,
    0x03, 0x03, 0x10, 17,   0,    0xE1, 0x12, 0x1C, 0x10, 0x18, 0x2D, 0x28, 0x23, 0x28, 0x28, 0x26, 0x2F, 0x3B, 0x00,
    0x03, 0x03, 0x10, 2,    0,    0x3A, 0x05, 2,    0,    0x36, 0x08, 1,    0,    0x29, 1,    0,    0x2C,
    0 // Terminate list
};

static TDD_DISP_QSPI_CFG_T sg_disp_qspi_cfg = {
    .cfg =
        {
            .cmd_caset = ST7735S_CASET,
            .cmd_raset = ST7735S_RASET,
            .cmd_ramwr = ST7735S_RAMWR,
        },

    .init_seq = cST7735S_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/
OPERATE_RET tdd_disp_qspi_st7735s_register(char *name, DISP_QSPI_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_qspi_st7735s_register: %s", name);

    sg_disp_qspi_cfg.cfg.width = dev_cfg->width;
    sg_disp_qspi_cfg.cfg.height = dev_cfg->height;
    sg_disp_qspi_cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_qspi_cfg.cfg.port = dev_cfg->port;
    sg_disp_qspi_cfg.cfg.spi_clk = dev_cfg->spi_clk;
    sg_disp_qspi_cfg.cfg.cs_pin = dev_cfg->cs_pin;
    sg_disp_qspi_cfg.cfg.dc_pin = dev_cfg->dc_pin;
    sg_disp_qspi_cfg.cfg.rst_pin = dev_cfg->rst_pin;
    sg_disp_qspi_cfg.rotation = dev_cfg->rotation;

    memcpy(&sg_disp_qspi_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_qspi_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdl_disp_qspi_device_register(name, &sg_disp_qspi_cfg);
}