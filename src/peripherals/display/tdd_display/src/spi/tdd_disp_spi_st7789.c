/**
 * @file tdd_disp_spi_st7789.c
 * @brief ST7789 LCD driver implementation with SPI interface
 *
 * This file provides the implementation for ST7789 TFT LCD displays using SPI interface.
 * It includes the initialization sequence, display control functions, and hardware-specific
 * configurations for ST7789 displays. The ST7789 supports up to 240x320 resolution with
 * 262K colors and is optimized for high-performance display applications.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "tal_log.h"

#include "tdd_display_spi.h"
#include "tdd_disp_st7789.h"

/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cST7789_INIT_SEQ[] = {
    1,    100,  ST7789_SWRESET,                                 // Software reset
    1,    50,   ST7789_SLPOUT,                                  // Exit sleep mode
    2,    10,   ST7789_COLMOD,    0x55,                         // Set colour mode to 16 bit
    2,    0,    ST7789_VCMOFSET,  0x1a,                         // VCOM
    6,    0,    ST7789_PORCTRL,   0x0c, 0x0c, 0x00, 0x33, 0x33, // Porch Setting
    1,    0,    ST7789_INVOFF, 
    2,    0,    ST7789_GCTRL,     0x56,                         // Gate Control
    2,    0,    ST7789_VCOMS,     0x18,                         // VCOMS setting
    2,    0,    ST7789_LCMCTRL,   0x2c,                         // LCM control
    2,    0,    ST7789_VDVVRHEN,  0x01,                         // VDV and VRH command enable
    2,    0,    ST7789_VRHS,      0x1f,                         // VRH set
    2,    0,    ST7789_VDVSET,    0x20,                         // VDV setting
    2,    0,    ST7789_FRCTR2,    0x0f,                         // FR Control 2
    3,    0,    ST7789_PWCTRL1,   0xa6, 0xa1,                   // Power control 1
    2,    0,    ST7789_PWCTRL2,   0x03,                         // Power control 2
    2,    0,    ST7789_MADCTL,    0x00, // Set MADCTL: row then column, refresh is bottom to top
    15,   0,    ST7789_PVGAMCTRL, 0xd0, 0x0d, 0x14, 0x0b, 0x0b, 0x07, 0x3a, 0x44, 0x50, 0x08, 0x13, 0x13, 0x2d, 0x32, // Positive voltage gamma control
    15,   0,    ST7789_NVGAMCTRL, 0xd0, 0x0d, 0x14, 0x0b, 0x0b, 0x07, 0x3a, 0x44, 0x50, 0x08, 0x13, 0x13, 0x2d, 0x32, // Negative voltage gamma control
    1,    0,    ST7789_SPI2EN, 
    1,    10,   ST7789_INVON, 
    1,    10,   ST7789_DISPON, // Main screen turn on, then wait 500 ms
    0                          // Terminate list
};

static TDD_DISP_SPI_CFG_T sg_disp_spi_cfg = {
    .cfg =
        {
            .cmd_caset = ST7789_CASET,
            .cmd_raset = ST7789_RASET,
            .cmd_ramwr = ST7789_RAMWR,
        },
         
    .is_swap = true,
    .init_seq = cST7789_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Sets the initialization sequence for the ST7789 display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_spi_st7789_set_init_seq(const uint8_t *init_seq)
{
    if(NULL == init_seq) {
        return OPRT_INVALID_PARM;
    }

    sg_disp_spi_cfg.init_seq = init_seq;

    return OPRT_OK;
}

/**
 * @brief Registers an ST7789 TFT display device using the SPI interface with the display management system.
 *
 * This function configures and registers a display device for the ST7789 series of TFT LCDs 
 * using the SPI communication protocol. It copies configuration parameters from the provided 
 * device configuration and uses a predefined initialization sequence specific to ST7789.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the SPI device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_spi_st7789_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_spi_st7789_register: %s", name);

    sg_disp_spi_cfg.cfg.width     = dev_cfg->width;
    sg_disp_spi_cfg.cfg.height    = dev_cfg->height;
    sg_disp_spi_cfg.cfg.x_offset  = dev_cfg->x_offset;
    sg_disp_spi_cfg.cfg.y_offset  = dev_cfg->y_offset;
    sg_disp_spi_cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;

    sg_disp_spi_cfg.cfg.port    = dev_cfg->port;
    sg_disp_spi_cfg.cfg.spi_clk = dev_cfg->spi_clk;
    sg_disp_spi_cfg.cfg.cs_pin  = dev_cfg->cs_pin;
    sg_disp_spi_cfg.cfg.dc_pin  = dev_cfg->dc_pin;
    sg_disp_spi_cfg.cfg.rst_pin = dev_cfg->rst_pin;
    sg_disp_spi_cfg.rotation    = dev_cfg->rotation;

    memcpy(&sg_disp_spi_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_spi_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdd_disp_spi_device_register(name, &sg_disp_spi_cfg);
}
