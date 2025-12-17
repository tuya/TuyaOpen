/**
 * @file tdd_disp_rgb_st7701s.c
 * @brief ST7701S LCD driver implementation with RGB interface
 *
 * This file provides the implementation for ST7701S TFT LCD displays using RGB interface.
 * It includes the initialization sequence, display control functions, and hardware-specific
 * configurations for ST7701S displays connected via RGB parallel interface, optimized for
 * high-resolution displays up to 320x480 with 16.7M colors.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tdd_disp_st7701s.h"
#include "tdd_display_rgb.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cST7701S_INIT_SEQ[] = {
    6,  0,  0xFF, 0x77, 0x01, 0x00, 0x00, 0x13,
    2,  0,  0xEF, 0x08,
    6,  0,  0xFF, 0x77, 0x01, 0x00, 0x00, 0x10,
    3,  0,  0xC0, 0x3B, 0x00,
    3,  0,  0xC1, 0x0D, 0x02,
    3,  0,  0xC2, 0x21, 0x08,
    2,  0,  0xCD, 0x08, //18-bit/pixel: MDT=0:D[21:16]=R,D[13:8]=G,D[5:0]=B(CDH=00) ;
    17, 0,  0xB0, 0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08, 0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18,
    17, 0,  0xB1, 0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08, 0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18,
    6,  0,  0xFF, 0x77, 0x01, 0x00, 0x00, 0x11,
    2,  0,  0xB0, 0x60,
    2,  0,  0xB1, 0x30,
    2,  0,  0xB2, 0x87,
    2,  0,  0xB3, 0x80,
    2,  0,  0xB5, 0x49,
    2,  0,  0xB7, 0x85,
    2,  0,  0xB8, 0x21,
    2,  0,  0xC1, 0x78,
    2,  20, 0xC2, 0x78,
    4,  0,  0xE0, 0x00, 0x1B, 0x02,
    12, 0,  0xE1, 0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x44, 0x44,
    13, 0,  0xE2, 0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00, 0x00, 0xEC, 0xA0, 0x00, 0x00,
    5,  0,  0xE3, 0x00, 0x00, 0x11, 0x11,
    3,  0,  0xE4, 0x44, 0x44,
    17, 0,  0xE5, 0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0, 0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0, 
    7,  0,  0xEF, 0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F, 
    6,  0,  0xFF, 0x77, 0x01, 0x00, 0x00, 0x00,  
    2,  0,  0x3A, 0x66,
    6,  0,  0xFF, 0x77, 0x01, 0x00, 0x00, 0x13,  
    3,  0,  0xE8, 0x00, 0x0E,
    6,  0,  0xFF, 0x77, 0x01, 0x00, 0x00, 0x00, 
    1, 120, 0x11, 
    6,  0,  0xFF, 0x77, 0x01, 0x00, 0x00, 0x13, 
    3, 10,  0xE8, 0x00, 0x0C, 
    3,  0,  0xE8, 0x00, 0x00,
    6,  0,  0xFF, 0x77, 0x01, 0x00, 0x00, 0x00, 
    2,  0,  0x36, 0x00,
    1,  0,  0x21,  
    1, 20,  0x29,  
    0,
};

static TDD_DISP_RGB_CFG_T sg_disp_rgb = {
    .cfg =
        {
            .clk = 26000000,
            .out_data_clk_edge = TUYA_RGB_DATA_IN_RISING_EDGE,
            .pixel_fmt = TUYA_PIXEL_FMT_RGB565,
            .hsync_pulse_width = 2,
            .vsync_pulse_width = 2,
            .hsync_back_porch = 10, //40,  //3
            .hsync_front_porch = 10, //5,  //2
            .vsync_back_porch = 10, //8,   //1
            .vsync_front_porch = 10, //8,  //1
        },
};

static TDD_DISP_SW_SPI_CFG_T sg_sw_spi_cfg;
static const uint8_t *sg_disp_init_seq = cST7701S_INIT_SEQ;
/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __tdd_disp_st7701s_seq_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tdd_disp_sw_spi_init(&sg_sw_spi_cfg));

    tdd_disp_sw_spi_lcd_init_seq(sg_disp_init_seq);

    return rt;
}

/**
 * @brief Sets the initialization sequence for the ST7701S display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_rgb_st7701s_set_init_seq(const uint8_t *init_seq)
{
    if(NULL == init_seq) {
        return OPRT_INVALID_PARM;
    }

    sg_disp_init_seq = init_seq;

    return OPRT_OK;
}

/**
 * @brief Registers an ST7701S RGB LCD display device with the display management system.
 *
 * This function configures and registers a display device for the ST7701S series of RGB LCDs 
 * using software SPI. It copies configuration parameters from the provided device configuration 
 * and sets up the initialization sequence specific to ST7701S.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the RGB display device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_rgb_st7701s_register(char *name, DISP_RGB_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    memcpy(&sg_sw_spi_cfg, &dev_cfg->sw_spi_cfg, sizeof(TDD_DISP_SW_SPI_CFG_T));

    sg_disp_rgb.init_cb = __tdd_disp_st7701s_seq_init;

    sg_disp_rgb.cfg.width = dev_cfg->width;
    sg_disp_rgb.cfg.height = dev_cfg->height;
    sg_disp_rgb.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_rgb.rotation = dev_cfg->rotation;
    sg_disp_rgb.is_swap = false;

    memcpy(&sg_disp_rgb.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_rgb.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdd_disp_rgb_device_register(name, &sg_disp_rgb);
}