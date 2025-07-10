/**
 * @file tdd_disp_mcu8080_st7789.c
 * @brief ST7789 LCD driver implementation with MCU 8080 parallel interface
 *
 * This file provides the implementation for ST7789 TFT LCD displays using MCU 8080
 * parallel interface. It includes the initialization sequence, display control
 * functions, and hardware-specific configurations for ST7789 displays connected
 * via 8080 parallel bus, providing high-speed data transfer capabilities.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "tal_log.h"

#include "tdd_disp_st7789.h"
#include "tdl_display_driver.h"

/***********************************************************
***********************const define**********************
***********************************************************/
const uint32_t cST7789_INIT_SEQ[] = {
    1,    100,  ST7789_SWRESET, // Software reset
    1,    50,   ST7789_SLPOUT,  // Exit sleep mode
    2,    10,   ST7789_COLMOD,
    0x55, // Set colour mode to 16 bit
    2,    0,    ST7789_VCMOFSET,
    0x1a, // VCOM
    6,    0,    ST7789_PORCTRL,
    0x0c, 0x0c, 0x00,
    0x33, 0x33, // Porch Setting
    1,    0,    ST7789_INVOFF,
    2,    0,    ST7789_GCTRL,
    0x56, // Gate Control
    2,    0,    ST7789_VCOMS,
    0x18, // VCOMS setting
    2,    0,    ST7789_LCMCTRL,
    0x2c, // LCM control
    2,    0,    ST7789_VDVVRHEN,
    0x01, // VDV and VRH command enable
    2,    0,    ST7789_VRHS,
    0x1f, // VRH set
    2,    0,    ST7789_VDVSET,
    0x20, // VDV setting
    2,    0,    ST7789_FRCTR2,
    0x0f, // FR Control 2
    3,    0,    ST7789_PWCTRL1,
    0xa6, 0xa1, // Power control 1
    2,    0,    ST7789_PWCTRL2,
    0x03, // Power control 2

    2,    0,    ST7789_MADCTL,
    0x00, // Set MADCTL: row then column, refresh is bottom to top
    15,   0,    ST7789_PVGAMCTRL,
    0xd0, 0x0d, 0x14,
    0x0b, 0x0b, 0x07,
    0x3a, 0x44, 0x50,
    0x08, 0x13, 0x13,
    0x2d, 0x32, // Positive voltage gamma control
    15,   0,    ST7789_NVGAMCTRL,
    0xd0, 0x0d, 0x14,
    0x0b, 0x0b, 0x07,
    0x3a, 0x44, 0x50,
    0x08, 0x13, 0x13,
    0x2d, 0x32, // Negative voltage gamma control
    1,    0,    ST7789_SPI2EN,
    1,    10,   ST7789_INVON,
    1,    10,   ST7789_DISPON, // Main screen turn on, then wait 500 ms
    0                          // Terminate list
};

static TDD_DISP_MCU8080_CFG_T sg_disp_mcu8080_cfg = {
    .cmd_caset = ST7789_CASET,
    .cmd_raset = ST7789_RASET,
    .cmd_ramwr = ST7789_RAMWR,
    .cmd_ramwr = ST7789_RAMWRC,
    .init_seq = cST7789_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/
OPERATE_RET tdd_disp_mcu8080_st7789_register(char *name, DISP_MCU8080_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_mcu8080_st7789_register: %s", name);

    sg_disp_mcu8080_cfg.cfg.width = dev_cfg->width;
    sg_disp_mcu8080_cfg.cfg.height = dev_cfg->height;
    sg_disp_mcu8080_cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_mcu8080_cfg.cfg.clk = dev_cfg->clk;
    sg_disp_mcu8080_cfg.cfg.data_bits = dev_cfg->data_bits;

    sg_disp_mcu8080_cfg.rotation = dev_cfg->rotation;
    sg_disp_mcu8080_cfg.te_pin = dev_cfg->te_pin;
    sg_disp_mcu8080_cfg.te_mode = dev_cfg->te_mode;

    memcpy(&sg_disp_mcu8080_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_mcu8080_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdl_disp_mcu8080_device_register(name, &sg_disp_mcu8080_cfg);
}
