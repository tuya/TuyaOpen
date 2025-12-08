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

#include "tdd_display_spi.h"
#include "tdd_disp_ili9341.h"

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
        
    .is_swap = true,
    .init_seq = cILI9341_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Sets the initialization sequence for the ILI9341 display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_spi_ili9341_set_init_seq(const uint8_t *init_seq)
{
    if(NULL == init_seq) {
        return OPRT_INVALID_PARM;
    }

    sg_disp_spi_cfg.init_seq = init_seq;

    return OPRT_OK;
}

/**
 * @brief Registers an ILI9341 TFT display device using the SPI interface with the display management system.
 *
 * This function configures and registers a display device for the ILI9341 series of TFT LCDs 
 * using the SPI communication protocol. It copies configuration parameters from the provided 
 * device configuration and uses a predefined initialization sequence specific to ILI9341.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the SPI device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_spi_ili9341_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_spi_ili9341_register: %s", name);

    sg_disp_spi_cfg.cfg.width     = dev_cfg->width;
    sg_disp_spi_cfg.cfg.height    = dev_cfg->height;
    sg_disp_spi_cfg.cfg.x_offset  = dev_cfg->x_offset;
    sg_disp_spi_cfg.cfg.y_offset  = dev_cfg->y_offset;
    sg_disp_spi_cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_spi_cfg.cfg.port      = dev_cfg->port;
    sg_disp_spi_cfg.cfg.spi_clk   = dev_cfg->spi_clk;
    sg_disp_spi_cfg.cfg.cs_pin    = dev_cfg->cs_pin;
    sg_disp_spi_cfg.cfg.dc_pin    = dev_cfg->dc_pin;
    sg_disp_spi_cfg.cfg.rst_pin   = dev_cfg->rst_pin;
    sg_disp_spi_cfg.rotation      = dev_cfg->rotation;

    memcpy(&sg_disp_spi_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_spi_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdd_disp_spi_device_register(name, &sg_disp_spi_cfg);
}