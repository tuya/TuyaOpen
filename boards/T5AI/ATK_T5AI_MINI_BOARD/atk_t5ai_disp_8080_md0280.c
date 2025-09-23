/**
 * @file atk_t5ai_disp_md0280.c
 * @brief ST7789 LCD driver implementation with MCU 8080 parallel interface
 *
 * This file provides the implementation for ST7789 TFT LCD displays using MCU 8080
 * parallel interface. It includes the initialization sequence, display control
 * functions, and hardware-specific configurations for ST7789 displays connected
 * via 8080 parallel bus, providing high-speed data transfer capabilities.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tkl_8080.h"

#include "atk_t5ai_disp_md0280.h"
#include "tdd_display_mcu8080.h"

/***********************************************************
***********************const define**********************
***********************************************************/
static const uint32_t cST7789_INIT_SEQ[] = {
    1,    100,  ST7789_SWRESET,                                 // Software reset
    1,    50,   ST7789_SLPOUT,                                  // Exit sleep mode
    2,    10,   ST7789_COLMOD,    0x05,                         // Set colour mode to 16 bit
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
    1,    10,   ST7789_DISPON, // Main screen turn on, then wait 500 ms
    0                          // Terminate list
};

static TDD_DISP_MCU8080_CFG_T sg_disp_mcu8080_cfg = {
    .cmd_caset = ST7789_CASET,
    .cmd_raset = ST7789_RASET,
    .cmd_ramwr = ST7789_RAMWR,
    .cmd_ramwrc = ST7789_RAMWRC,
    .init_seq = cST7789_INIT_SEQ,
};

static TDL_DISP_FRAME_BUFF_T *sg_conv_fb = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static TDL_DISP_FRAME_BUFF_T *__disp_8080_create_convert_fb(uint32_t width, uint32_t height)
{
    TDL_DISP_FRAME_BUFF_T *fb = NULL;
    uint32_t fb_size = 0;

    fb_size = width * height * 2;
    fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, fb_size);
    if(NULL == fb) {
        PR_ERR("create conv fb failed");
        return NULL;
    }
    fb->fmt    = TUYA_PIXEL_FMT_RGB565;
    fb->width  = width;
    fb->height = height;

    return fb;
}

static void __disp_8080_release_convert_fb(TDL_DISP_FRAME_BUFF_T *fb)
{
    if (fb) {
        tdl_disp_free_frame_buff(fb);
    }
}

static TDL_DISP_FRAME_BUFF_T *__disp_8080_convert_fb(TDL_DISP_FRAME_BUFF_T *frame_buff)
{
    uint32_t i=0;
    uint16_t rgb565 = 0, *p_buf16 = NULL;
    uint16_t *p_dst_buf16 = NULL;

    if(NULL == frame_buff) {
        PR_ERR("Invalid parameter\n");
        return NULL;
    }

    if(sg_conv_fb == NULL) {
        sg_conv_fb = __disp_8080_create_convert_fb(frame_buff->width, frame_buff->height);
    }else {
        if(sg_conv_fb->width != frame_buff->width ||
           sg_conv_fb->height != frame_buff->height) {
            __disp_8080_release_convert_fb(sg_conv_fb);
            sg_conv_fb = __disp_8080_create_convert_fb(frame_buff->width, frame_buff->height);
        }
    }

    if(NULL == sg_conv_fb) {
        PR_ERR("create conv fb failed");
        return NULL;
    }

    p_buf16 = (uint16_t *)frame_buff->frame;
    p_dst_buf16 = (uint16_t *)sg_conv_fb->frame;
    for(i=0; i<frame_buff->len/2; i++) {
        rgb565 = p_buf16[i];
        p_dst_buf16[i] = ((rgb565 & 0x0FFE)>>1) | ((rgb565 & 0xE000)>>2);
    }

    return sg_conv_fb;
}

OPERATE_RET atk_disp_8080_md0280_register(char *name, ATK_DISP_8080_7789_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_mcu8080_st7789_register: %s", name);

    sg_disp_mcu8080_cfg.cfg.width = dev_cfg->width;
    sg_disp_mcu8080_cfg.cfg.height = dev_cfg->height;
    sg_disp_mcu8080_cfg.cfg.pixel_fmt = TUYA_PIXEL_FMT_RGB666;
    sg_disp_mcu8080_cfg.cfg.clk = 12*1000000;
    sg_disp_mcu8080_cfg.cfg.data_bits = 18;

    sg_disp_mcu8080_cfg.in_fmt = TUYA_PIXEL_FMT_RGB565;
    sg_disp_mcu8080_cfg.rotation = dev_cfg->rotation;
    sg_disp_mcu8080_cfg.te_pin = TUYA_GPIO_NUM_MAX;
    sg_disp_mcu8080_cfg.is_swap = false;
    sg_disp_mcu8080_cfg.convert_cb = __disp_8080_convert_fb;

    memcpy(&sg_disp_mcu8080_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_mcu8080_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdd_disp_mcu8080_device_register(name, &sg_disp_mcu8080_cfg);
}
