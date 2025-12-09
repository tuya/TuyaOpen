/**
 * @file tdd_disp_rgb_ili9488.c
 * @brief ILI9488 LCD driver implementation with RGB interface
 *
 * This file provides the implementation for ILI9488 TFT LCD displays using RGB interface.
 * It includes the initialization sequence, display control functions, and hardware-specific
 * configurations for ILI9488 displays connected via RGB parallel interface, optimized for
 * high-resolution displays up to 320x480 with 16.7M colors.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tdd_disp_ili9488.h"
#include "tdd_display_rgb.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cILI9488_INIT_SEQ[] = {
    3,  0,   ILI9488_PWCTR1,   0x0E, 0x0E,
    2,  0,   ILI9488_PWCTR2,   0x46,
    4,  0,   ILI9488_VMCTR1,   0x00, 0x2D, 0x80,
    2,  0,   ILI9488_IFMODE,   0x00,
    2,  0,   ILI9488_FRMCTR1,  0xA0,
    2,  0,   ILI9488_INVCTR,   0x02,
    5,  0,   ILI9488_PRCTR,    0x08, 0x0C, 0x50, 0x64,
    3,  0,   ILI9488_DFUNCTR,  0x32, 0x02,
    2,  0,   ILI9488_MADCTL,   0x48,
    2,  0,   ILI9488_PIXFMT,   0x70,
    2,  0,   ILI9488_INVON,    0x00,
    2,  0,   ILI9488_SETIMAGE, 0x01,
    5,  0,   ILI9488_ACTRL3,   0xA9, 0x51, 0x2C, 0x82,
    3,  0,   ILI9488_ACTRL4,   0x21, 0x05,
    16, 0,   ILI9488_GMCTRP1,  0x00, 0x0C, 0x10, 0x03, 0x0F, 0x05, 0x37, 0x66, 0x4D, 0x03, 0x0C, 0x0A, 0x2F, 0x35, 0x0F,
    16, 0,   ILI9488_GMCTRN1,  0x00, 0x0F, 0x16, 0x06, 0x13, 0x07, 0x3B, 0x35, 0x51, 0x07, 0x10, 0x0D, 0x36, 0x3B, 0x0F,
    1,  120, ILI9488_SLPOUT, 
    1,  20,  ILI9488_DISPON,
    0,
};

static TDD_DISP_RGB_CFG_T sg_disp_rgb = {
    .cfg =
        {
            .clk = 15000000,
            .out_data_clk_edge = TUYA_RGB_DATA_IN_RISING_EDGE,
            .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
            .hsync_back_porch = 80,
            .hsync_front_porch = 80,
            .vsync_back_porch = 8,
            .vsync_front_porch = 8,
            .hsync_pulse_width = 20,
            .vsync_pulse_width = 4,
        },
};

static TDD_DISP_SW_SPI_CFG_T sg_sw_spi_cfg;
static const uint8_t *sg_disp_init_seq = cILI9488_INIT_SEQ;
/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __tdd_disp_ili9488_seq_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tdd_disp_sw_spi_init(&sg_sw_spi_cfg));

    tdd_disp_sw_spi_lcd_init_seq(sg_disp_init_seq);

    return rt;
}

/**
 * @brief Sets the initialization sequence for the ILI9488 display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_rgb_ili9488_set_init_seq(const uint8_t *init_seq)
{
    if(NULL == init_seq) {
        return OPRT_INVALID_PARM;
    }

    sg_disp_init_seq = init_seq;

    return OPRT_OK;
}

/**
 * @brief Registers an ILI9488 RGB LCD display device with the display management system.
 *
 * This function configures and registers a display device for the ILI9488 series of RGB LCDs 
 * using software SPI. It copies configuration parameters from the provided device configuration 
 * and sets up the initialization sequence specific to ILI9488.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the RGB display device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_rgb_ili9488_register(char *name, DISP_RGB_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    memcpy(&sg_sw_spi_cfg, &dev_cfg->sw_spi_cfg, sizeof(TDD_DISP_SW_SPI_CFG_T));

    sg_disp_rgb.init_cb = __tdd_disp_ili9488_seq_init;

    sg_disp_rgb.cfg.width = dev_cfg->width;
    sg_disp_rgb.cfg.height = dev_cfg->height;
    sg_disp_rgb.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_rgb.rotation = dev_cfg->rotation;
    sg_disp_rgb.is_swap = false;

    memcpy(&sg_disp_rgb.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_rgb.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdd_disp_rgb_device_register(name, &sg_disp_rgb);
}