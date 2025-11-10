/**
 * @file tdd_disp_spi_st7735s.c
 * @brief st7735s LCD driver implementation with SPI interface
 *
 * This file provides the implementation for st7735s TFT LCD displays using SPI interface.
 * It includes the initialization sequence, display control functions, and hardware-specific
 * configurations for st7735s displays. The st7735s supports up to 240x320 resolution with
 * 262K colors and is optimized for high-performance display applications.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "tal_log.h"

#include "tdd_disp_spi_st7735s_xlt.h"
#include "tdd_display_spi.h"

/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cST7735S_INIT_SEQ[] = {
    //1,    0,    0x01, 
    1,    120,  0x11, 
    1,    0,  0x21, 
    1,    0,  0x21, 
    4,    100,  0xB1, 0x05,0x3A,0x3A,
    4,    0,    0xB2, 0x05,0x3A,0x3A,
    7,    0,    0xB3, 0x05,0x3A,0x3A, 0x05,0x3A,0x3A,
    2,    0,    0xB4, 0x03,
    4,    0,    0xC0, 0x62,0x02,0x04,
    2,    0,    0xC1, 0xC0,
    3,    0,    0xC2, 0x0D, 0x00,
    3,    0,    0xC3, 0x8A, 0x6A,
    3,    0,    0xC4, 0x8D, 0xEE,
    2,    0,    0xC5, 0x0E,
    17,   0,    0xE0, 0x10,0x0E,0x02,0x03,0x0E,0x07,0x02,0x07,0x0A,0x12,0x27,0x37,0x00,0x0D,0x0E,0x10,
    17,   0,    0xE1, 0x10,0x0E,0x03,0x03,0x0F,0x06,0x02,0x08,0x0A,0x13,0x26,0x36,0x00,0x0D,0x0E,0x10,
    2,    0,    0x3A, 0x05, 
    2,    0,    0x36, 0xA8,
    1,    0,    0x29,

    // 5,    0,    0x2a, 0x00, 0x1A, 0x00, 0x1A,
    // 5,    0,    0x2b, 0x00, 0x1, 0x00, 0x1,
    // 1,    0,    0x2c,

    0 // Terminate list                    // Terminate list
};

static TDD_DISP_SPI_CFG_T sg_disp_spi_cfg = {
    .cfg =
        {
            .cmd_caset = ST7735S_CASET,
            .cmd_raset = ST7735S_RASET,
            .cmd_ramwr = ST7735S_RAMWR,
        },

    .init_seq = cST7735S_INIT_SEQ,
    .is_swap = true,
    .set_window_cb = NULL, // Default callback to set window
};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __tdd_disp_st7735s_set_window(DISP_SPI_BASE_CFG_T *p_cfg, uint16_t x_start, uint16_t y_start,\
                                           uint16_t x_end, uint16_t y_end)
{
    uint8_t lcd_data[4];

    if (NULL == p_cfg) {
        return;
    } 
    
    lcd_data[0] = (x_start >> 8) & 0xFF;
    lcd_data[1] = (x_start & 0xFF) +1;
    lcd_data[2] = (x_end >> 8) & 0xFF;
    lcd_data[3] = (x_end & 0xFF) +1;
    tdd_disp_spi_send_cmd(p_cfg, p_cfg->cmd_caset);
    tdd_disp_spi_send_data(p_cfg, lcd_data, 4);

    lcd_data[0] = (y_start >> 8) & 0xFF;
    lcd_data[1] = (y_start & 0xFF) +0x1A;
    lcd_data[2] = (y_end >> 8) & 0xFF;
    lcd_data[3] = (y_end & 0xFF) +0x1A;
    tdd_disp_spi_send_cmd(p_cfg, p_cfg->cmd_raset);
    tdd_disp_spi_send_data(p_cfg, lcd_data, 4);

}

/**
 * @brief Registers an st7735s TFT display device using the SPI interface with the display management system.
 *
 * This function configures and registers a display device for the st7735s series of TFT LCDs 
 * using the SPI communication protocol. It copies configuration parameters from the provided 
 * device configuration and uses a predefined initialization sequence specific to st7735s.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the SPI device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_spi_st7735s_xlt_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_spi_st7735s_register: %s", name);

    sg_disp_spi_cfg.cfg.width = dev_cfg->width;
    sg_disp_spi_cfg.cfg.height = dev_cfg->height;
    sg_disp_spi_cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_spi_cfg.cfg.port = dev_cfg->port;
    sg_disp_spi_cfg.cfg.spi_clk = dev_cfg->spi_clk;
    sg_disp_spi_cfg.cfg.cs_pin = dev_cfg->cs_pin;
    sg_disp_spi_cfg.cfg.dc_pin = dev_cfg->dc_pin;
    sg_disp_spi_cfg.cfg.rst_pin = dev_cfg->rst_pin;
    sg_disp_spi_cfg.rotation = dev_cfg->rotation;
    sg_disp_spi_cfg.set_window_cb = __tdd_disp_st7735s_set_window; 

    memcpy(&sg_disp_spi_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_spi_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdd_disp_spi_device_register(name, &sg_disp_spi_cfg);
}
