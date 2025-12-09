/**
 * @file tdd_disp_rgb_st7701sn.c
 * @brief st7701sn LCD driver implementation with RGB interface
 *
 * This file provides the implementation for st7701sn TFT LCD displays using RGB interface.
 * It includes the initialization sequence, display control functions, and hardware-specific
 * configurations for st7701sn displays connected via RGB parallel interface, optimized for
 * high-resolution displays up to 320x480 with 16.7M colors.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tdd_disp_st7701sn.h"
#include "tdd_display_rgb.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cST7701SN_INIT_SEQ[] = {
    1, 10,  0x11,
    6,  0,  0xFF, 0x77, 0x01, 0x00, 0x00, 0x13,
    2,  0,  0xEF, 0x08,
    6,  0,  0xFF, 0x77, 0x01, 0x00, 0x00, 0x10,
    3,  0,  0xC0, 0xE9, 0x03,
    3,  0,  0xC1, 0x0C, 0x02,
    3,  0,  0xC2, 0x07, 0x08,
    2,  0,  0xC7, 0x04,
    2,  0,  0xC6, 0x21,
    2,  0,  0xCC, 0x10,
    17, 0,  0xB0, 0x00, 0x0B, 0x0C, 0x0E, 0x14, 0x06, 0x00, 0x09, 0x08, 0x1E, 0x05, 0x12, 0x10, 0x2B, 0x34, 0x1F,
    17, 0,  0xB1, 0x04, 0x07, 0x12, 0x09, 0x0A, 0x04, 0x00, 0x08, 0x08, 0x1F, 0x01, 0x0E, 0x0E, 0x2D, 0x36, 0x1F,
    6,  0,  0xFF, 0x77, 0x01, 0x00, 0x00, 0x11,
    2,  0,  0xB0, 0x35,
    2,  0,  0xB1, 0x4C,
    2,  0,  0xB2, 0x87,
    2,  0,  0xB3, 0x80,
    2,  0,  0xB5, 0x49,
    2,  0,  0xB7, 0x85,
    2,  0,  0xB8, 0x21,
    2,  0,  0xB9, 0x10,
    2,  0,  0xBC, 0x33,
    2,  0,  0xC0, 0x89,
    2,  0,  0xC1, 0x78,
    2,  0,  0xC2, 0x78,
    2,  0,  0xD0, 0x88,
    4,  0,  0xE0, 0x00, 0x00, 0x02,
    12, 0,  0xE1, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20,
    14, 0,  0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    5,  0,  0xE3, 0x00, 0x00, 0x33, 0x00,
    3,  0,  0xE4, 0x22, 0x00,
    17, 0,  0xE5, 0x04, 0x5C, 0xA0, 0xA0, 0x06, 0x5C, 0xA0, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    5,  0,  0xE6, 0x00, 0x00, 0x33, 0x00,
    3,  0,  0xE7, 0x22, 0x00,
    17, 0,  0xE8, 0x05, 0x5C, 0xA0, 0xA0, 0x07, 0x5C, 0xA0, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    8,  0,  0xEB, 0x02, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00,
    3,  0,  0xEC, 0x00, 0x00,
    17, 0,  0xED, 0xFA, 0x45, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xB0, 0x54, 0xAF,
    7,  0,  0xEF, 0x08, 0x08, 0x08, 0x45, 0x3F, 0x54,
    7,  0,  0xFF, 0x77, 0x01, 0x00, 0x00, 0x00, 0x11,
    1, 120, 0x11,
    6,  0,  0xFF, 0x77, 0x01, 0x00, 0x00, 0x00,
    2,  0,  0x3A, 0x55,
    2,  0,  0x36, 0x10,
    1,  0,  0x11, 
    1,  0,  0x29,
    0,

};

static TDD_DISP_RGB_CFG_T sg_disp_rgb = {
    .cfg =
        {
            .clk = 30000000,
            .out_data_clk_edge = TUYA_RGB_DATA_IN_RISING_EDGE,
            .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
            .hsync_back_porch  = 46,
            .hsync_front_porch = 48,
            .vsync_back_porch  = 24,
            .vsync_front_porch = 24,
            .hsync_pulse_width = 2,
            .vsync_pulse_width = 2,
        },
};

static TDD_DISP_SW_SPI_CFG_T sg_sw_spi_cfg;
static const uint8_t *sg_disp_init_seq = cST7701SN_INIT_SEQ;
/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __tdd_disp_st7701sn_seq_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tdd_disp_sw_spi_init(&sg_sw_spi_cfg));

    tdd_disp_sw_spi_lcd_init_seq(sg_disp_init_seq);

    return rt;
}

/**
 * @brief Sets the initialization sequence for the st7701sn display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_rgb_st7701sn_set_init_seq(const uint8_t *init_seq)
{
    if(NULL == init_seq) {
        return OPRT_INVALID_PARM;
    }

    sg_disp_init_seq = init_seq;

    return OPRT_OK;
}

/**
 * @brief Registers an st7701sn RGB LCD display device with the display management system.
 *
 * This function configures and registers a display device for the st7701sn series of RGB LCDs 
 * using software SPI. It copies configuration parameters from the provided device configuration 
 * and sets up the initialization sequence specific to st7701sn.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the RGB display device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_rgb_st7701sn_register(char *name, DISP_RGB_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    memcpy(&sg_sw_spi_cfg, &dev_cfg->sw_spi_cfg, sizeof(TDD_DISP_SW_SPI_CFG_T));

    sg_disp_rgb.init_cb = __tdd_disp_st7701sn_seq_init;

    sg_disp_rgb.cfg.width = dev_cfg->width;
    sg_disp_rgb.cfg.height = dev_cfg->height;
    sg_disp_rgb.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_rgb.rotation = dev_cfg->rotation;
    sg_disp_rgb.is_swap = false;

    memcpy(&sg_disp_rgb.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_rgb.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdd_disp_rgb_device_register(name, &sg_disp_rgb);
}