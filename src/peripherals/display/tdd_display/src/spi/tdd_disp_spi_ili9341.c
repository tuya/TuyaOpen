/**
 * @file tdd_disp_spi_ili9341.c
 * @brief ILI9341 LCD driver implementation with SPI interface
 *
 * This file provides the implementation for ILI9341 TFT LCD displays using SPI interface.
 * It includes the initialization sequence, display control functions, and hardware-specific
 * configurations for ILI9341 displays. The ILI9341 supports 240x320 resolution with
 * 262K colors and is widely used in embedded display applications.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "tal_log.h"

#include "tdd_disp_ili9341.h"
#include "tdl_display_driver.h"

/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cILI9341_INIT_SEQ[] = {
    1, 100, ILI9341_SWRESET,              // Software reset
    1, 50,  ILI9341_SLPOUT,               // Exit sleep mode
    3, 0,   ILI9341_DSIPCTRL, 0x0a, 0xc2, // Display Function Control
    2, 0,   ILI9341_COLMOD,   0x55,       // Set colour mode to 16 bit
    2, 0,   ILI9341_MADCTL,   0x08,       // Set MADCTL: row then column, refresh is bottom to top
    1, 10,  ILI9341_DISPON,               // Main screen turn on, then wait 500 ms
    0                                     // Terminate list
};

static TDD_DISP_SPI_CFG_T sg_disp_spi_cfg = {
    .cfg =
        {
            .cmd_caset = ILI9341_CASET,
            .cmd_raset = ILI9341_RASET,
            .cmd_ramwr = ILI9341_RAMWR,
        },

    .init_seq = cILI9341_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
OPERATE_RET tdd_disp_spi_ili9341_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_spi_ili9341_register: %s", name);

    sg_disp_spi_cfg.cfg.width = dev_cfg->width;
    sg_disp_spi_cfg.cfg.height = dev_cfg->height;
    sg_disp_spi_cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_spi_cfg.cfg.port = dev_cfg->port;
    sg_disp_spi_cfg.cfg.spi_clk = dev_cfg->spi_clk;
    sg_disp_spi_cfg.cfg.cs_pin = dev_cfg->cs_pin;
    sg_disp_spi_cfg.cfg.dc_pin = dev_cfg->dc_pin;
    sg_disp_spi_cfg.cfg.rst_pin = dev_cfg->rst_pin;
    sg_disp_spi_cfg.rotation = dev_cfg->rotation;

    memcpy(&sg_disp_spi_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_spi_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdl_disp_spi_device_register(name, &sg_disp_spi_cfg);
}