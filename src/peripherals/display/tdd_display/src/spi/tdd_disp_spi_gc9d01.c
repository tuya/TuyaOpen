/**
 * @file tdd_disp_spi_gc9a01.c
 * @brief GC9D01 LCD driver implementation with SPI interface
 *
 * This file provides the implementation for GC9D01 TFT LCD displays using SPI interface.
 * It includes the initialization sequence, display control functions, and hardware-specific
 * configurations for GC9D01 displays. The GC9D01 is a circular display controller
 * optimized for 240x240 resolution with 262K colors.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "tal_log.h"

#include "tdd_display_spi.h"
#include "tdd_disp_gc9d01.h"

/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cGC9D01_INIT_SEQ[] = {
    1,    0,    0xFE,
    1,    0,    0xEF,
    2,    0,    0x80,   0xFF,
    2,    0,    0x81,   0xFF,
    2,    0,    0x82,   0xFF,
    2,    0,    0x83,   0xFF,
    2,    0,    0x84,   0xFF,
    2,    0,    0x85,   0xFF,
    2,    0,    0x86,   0xFF,
    2,    0,    0x87,   0xFF,
    2,    0,    0x88,   0xFF,
    2,    0,    0x89,   0xFF,
    2,    0,    0x8A,   0xFF,
    2,    0,    0x8B,   0xFF,
    2,    0,    0x8C,   0xFF,
    2,    0,    0x8D,   0xFF,
    2,    0,    0x8E,   0xFF,
    2,    0,    0x8F,   0xFF,
    2,    0,    0x3A,   0x05,
    2,    0,    0xEC,   0x01,
    8,    0,    0x74,   0x02,   0x0E,   0x00,   0x00,   0x00,   0x00,   0x00,
    2,    0,    0x98,   0x3E,
    2,    0,    0x99,   0x3E,
    3,    0,    0xB5,   0x0D,   0x0D,
    5,    0,    0x60,   0x38,   0x0F,   0x79,   0x67,    
    5,    0,    0x61,   0x38,   0x11,   0x79,   0x67,
    7,    0,    0x64,   0x38,   0x17,   0x71,   0x5F,   0x79,   0x67,
    7,    0,    0x65,   0x38,   0x13,   0x71,   0x5B,   0x79,   0x67,
    3,    0,    0x6A,   0x00,   0x00,
    8,    0,    0x6C,   0x22,   0x02,   0x22,   0x02,   0x22,   0x22,   0x50,
    33,   0,    0x6E,   0x03,   0x03,   0x01,   0x01,   0x00,   0x00,   0x0F,   0x0F,    
                        0x0D,   0x0D,   0x0B,   0x0B,   0x09,   0x09,   0x00,   0x00,   0x00,   0x00,
                        0x0A,   0x0A,   0x0C,   0x0C,   0x0E,   0x0E,   0x10,   0x10,   0x00,   0x00,
                        0x02,   0x02,   0x04,   0x04,
    2,    0,    0xBF,   0x01,
    2,    0,    0xF9,   0x40,
    2,    0,    0x9B,   0x3B,
    4,    0,    0x93,   0x33,   0x7F,   0x00,
    2,    0,    0x7E,   0x30,
    7,    0,    0x70,   0x0D,   0x02,   0x08,   0x0D,   0x02,   0x08,
    4,    0,    0x71,   0x0D,   0x02,   0x08,
    3,    0,    0x91,   0x0E,   0x09,
    2,    0,    0xC3,   0x19,
    2,    0,    0xC4,   0x19,
    2,    0,    0xC9,   0x3C,
    7,    0,    0xF0,   0x53,   0x15,   0x0A,   0x04,   0x00,   0x3E,
    7,    0,    0xF2,   0x53,   0x15,   0x0A,   0x04,   0x00,   0x3A,
    7,    0,    0xF1,   0x56,   0xA8,   0x7F,   0x33,   0x34,   0x5F,
    7,    0,    0xF3,   0x52,   0xA4,   0x7F,   0x33,   0x34,   0xDF,
    2,    0,    0x36,   0x00,
    1,    200,  0x11,
    1,    0,    0x29,
    1,    200,  0x2C,
    0                          // Terminate list
};

static TDD_DISP_SPI_CFG_T sg_disp_spi_cfg = {
    .cfg =
        {
            .cmd_caset = GC9D01_CASET,
            .cmd_raset = GC9D01_RASET,
            .cmd_ramwr = GC9D01_RAMWR,
        },

    .is_swap = true,
    .init_seq = cGC9D01_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Registers a GC9D01 TFT display device using the SPI interface with the display management system.
 *
 * This function configures and registers a display device for the GC9D01 series of TFT LCDs 
 * using the SPI communication protocol. It copies configuration parameters from the provided 
 * device configuration and uses a predefined initialization sequence specific to GC9D01.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the SPI device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_spi_gc9d01_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_spi_gc9a01_register: %s", name);

    sg_disp_spi_cfg.cfg.width = dev_cfg->width;
    sg_disp_spi_cfg.cfg.height = dev_cfg->height;
    sg_disp_spi_cfg.cfg.x_offset = dev_cfg->x_offset;
    sg_disp_spi_cfg.cfg.y_offset = dev_cfg->y_offset;
    sg_disp_spi_cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_spi_cfg.cfg.port = dev_cfg->port;
    sg_disp_spi_cfg.cfg.spi_clk = dev_cfg->spi_clk;
    sg_disp_spi_cfg.cfg.cs_pin = dev_cfg->cs_pin;
    sg_disp_spi_cfg.cfg.dc_pin = dev_cfg->dc_pin;
    sg_disp_spi_cfg.cfg.rst_pin = dev_cfg->rst_pin;
    sg_disp_spi_cfg.rotation = dev_cfg->rotation;

    memcpy(&sg_disp_spi_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_spi_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdd_disp_spi_device_register(name, &sg_disp_spi_cfg);
}