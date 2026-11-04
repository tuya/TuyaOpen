/**
 * @file tdd_disp_mcu8080_ili9488.c
 * @brief ILI9488 LCD driver implementation with MCU 8080 parallel interface
 *
 * This file provides the implementation for ILI9488 TFT LCD displays using the
 * MCU 8080 parallel interface. The initialization sequence selects the 16-bit
 * (RGB565) pixel format and the DBI (8080) interface so the panel matches the
 * TUYA_PIXEL_FMT_RGB565 contract the TKL 8080 backend exposes.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_log.h"

#include "tdd_disp_ili9488.h"
#include "tdd_display_mcu8080.h"

/***********************************************************
***********************const define**********************
***********************************************************/
/*
 * Layout matches the other MCU8080 panels: {count, delay_ms, cmd, data...}
 * where count is one command byte plus its data bytes.
 *
 * This reproduces the vendor sequence in apps/common/ui/lcd_driver/lcd_ili9488.c
 * (code1[]) verbatim. That sequence is the one proven on the AC79 DevKit panel;
 * the generic TuyaOpen RGB ILI9488 sequence used before it differed in MADCTL
 * (0x48 vs 0xD8) and in 0xB6/0xB5, which flipped the picture into a negative.
 *
 * PIXFMT is 0x55 (16 bits/pixel) because the TKL 8080 path only accepts
 * TUYA_PIXEL_FMT_RGB565; the RGB driver's sequence uses 0x70 (18-bit) and must
 * not be reused here.
 */
const uint32_t cILI9488_MCU8080_INIT_SEQ[] = {
    1, 0,   0x21,                                   // Display inversion on
    2, 0,   0x35,           0x00,                   // Tearing effect line on
    3, 0,   0xB1,           0x90, 0x11,             // Frame rate: ~60 fps
    2, 0,   ILI9488_MADCTL, 0xD8,                   // MY + MX + ML + BGR
    2, 0,   0xC1,           0x41,                   // Power control 2
    5, 0,   0xF7,           0xA9, 0x51, 0x2C, 0x82, // Adjust control 3
    2, 0,   0xC1,           0x41,                   // Power control 2 (again, as vendor)
    3, 0,   0xC0,           0x0F, 0x0F,             // Power control 3
    2, 0,   0xC2,           0x22,                   // Power control 4
    4, 0,   0xC5,           0x00, 0x53, 0x80,       // VCOM control
    2, 0,   0xB4,           0x02,                   // Display inversion control
    2, 0,   0xB7,           0xC6,                   // Entry mode set
    3, 0,   0xB6,           0x02, 0x42,             // Display function control
    3, 0,   0xBE,           0x00, 0x04,             // HS lane control
    2, 0,   0xE9,           0x00,                   // Set image function
    2, 0,   ILI9488_PIXFMT, 0x55,                   // 16 bit / RGB565
    1, 200, 0x11,                                   // Sleep out
    1, 0,   ILI9488_DISPON,                         // Display on
    0                                               // Terminate list
};

static TDD_DISP_MCU8080_CFG_T sg_disp_mcu8080_cfg = {
    .cmd_caset  = ILI9488_CASET,
    .cmd_raset  = ILI9488_PASET,
    .cmd_ramwr  = ILI9488_RAMWR,
    .cmd_ramwrc = ILI9488_RAMWRC,
    .init_seq   = cILI9488_MCU8080_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Sets the initialization sequence for the ILI9488 MCU8080 display
 *
 * @param init_seq Pointer to the initialization sequence array
 *
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_mcu8080_ili9488_set_init_seq(const uint32_t *init_seq)
{
    if (NULL == init_seq) {
        return OPRT_INVALID_PARM;
    }

    sg_disp_mcu8080_cfg.init_seq = init_seq;

    return OPRT_OK;
}

/**
 * @brief Registers an ILI9488 TFT display device using the MCU8080 interface
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the MCU8080 device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_mcu8080_ili9488_register(char *name, DISP_MCU8080_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_mcu8080_ili9488_register: %s", name);

    sg_disp_mcu8080_cfg.cfg.width     = dev_cfg->width;
    sg_disp_mcu8080_cfg.cfg.height    = dev_cfg->height;
    sg_disp_mcu8080_cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_mcu8080_cfg.cfg.clk       = dev_cfg->clk;
    sg_disp_mcu8080_cfg.cfg.data_bits = dev_cfg->data_bits;

    sg_disp_mcu8080_cfg.in_fmt   = dev_cfg->pixel_fmt;
    sg_disp_mcu8080_cfg.rotation = dev_cfg->rotation;
    sg_disp_mcu8080_cfg.te_pin   = dev_cfg->te_pin;
    sg_disp_mcu8080_cfg.te_mode  = dev_cfg->te_mode;
    sg_disp_mcu8080_cfg.x_offset = dev_cfg->x_offset;
    sg_disp_mcu8080_cfg.y_offset = dev_cfg->y_offset;
    /* The ILI9488 samples RGB565 high byte first. LVGL hands us a native
     * (little-endian) RGB565 buffer, so the bytes have to be swapped or every
     * colour lands on the wrong channel while white/black still look right.
     * The vendor driver does the same packing in ili9488_clear_screen(). */
    sg_disp_mcu8080_cfg.is_swap = true;

    memcpy(&sg_disp_mcu8080_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_mcu8080_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdd_disp_mcu8080_device_register(name, &sg_disp_mcu8080_cfg);
}
