/**
 * @file tdd_disp_mcu8080_st7796.c
 * @brief ST7796S LCD driver implementation with MCU 8080 parallel interface
 *
 * This file provides the implementation for ST7796S TFT LCD displays using MCU 8080
 * parallel interface. It includes the initialization sequence, display control
 * functions, and hardware-specific configurations for ST7796S displays connected
 * via 8080 parallel bus, optimized for high-resolution displays up to 320x480.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_log.h"

#include "tdd_disp_st7796s.h"
#include "tdd_display_mcu8080.h"

/***********************************************************
***********************const define**********************
***********************************************************/
static const uint32_t cST7796S_INIT_SEQ[] = {
    1,    0,    0x01, 
    1,    120,  0x28, 
    2,    0,    0xF0, 0xC3,
    2,    0,    0xF0, 0x96,
    2,    0,    0x35, 0x00,
    3,    0,    0x44, 0x00, 0x01,
    3,    0,    0xb1, 0x60, 0x11,  
    2,    0,    0x36, 0x98,
    2,    0,    0x3A, 0x55,
    2,    0,    0xB4, 0x01,
    2,    0,    0xB7, 0xC6,
    9,    0,    0xE8, 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xa5, 0x33,
    2,    0,    0xC2, 0xA7,
    2,    0,    0xC5, 0x2B,
    15,   0,    0xE0, 0xF0, 0x09, 0x13, 0x12, 0x12, 0x2B, 0x3C, 0x44, 0x4B, 0x1B, 0x18, 0x17, 0x1D, 0x21,
    15,   0,    0xE1, 0xF0, 0x09, 0x13, 0x0C, 0x0D, 0x27, 0x3B, 0x44, 0x4D, 0x0B, 0x17, 0x17, 0x1D, 0x21,
    2,    0,    0xF0, 0x3C,
    2,    0,    0xF0, 0x96,
    1,    150,    0x11, 
    1,    0,    0x29, 
    0 // Terminate list
};

static TDD_DISP_MCU8080_CFG_T sg_disp_mcu8080_cfg = {
    .cmd_caset  = ST7796S_CASET,
    .cmd_raset  = ST7796S_RASET,
    .cmd_ramwr  = ST7796S_RAMWR,
    .cmd_ramwrc = ST7796S_RAMWRC,
    .init_seq = cST7796S_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Sets the initialization sequence for the ST7796S display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_mcu8080_st7796s_set_init_seq(const uint32_t *init_seq)
{
    if(NULL == init_seq) {
        return OPRT_INVALID_PARM;
    }

    sg_disp_mcu8080_cfg.init_seq = init_seq;

    return OPRT_OK;
}

/**
 * @brief Registers an ST7796S TFT display device using the MCU8080 interface with the display management system.
 *
 * This function configures and registers a display device for the ST7796S series of TFT LCDs 
 * using the MCU8080 parallel interface. It copies configuration parameters from the provided 
 * device configuration and uses a predefined initialization sequence specific to ST7796S.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the MCU8080 device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_mcu8080_st7796s_register(char *name, DISP_MCU8080_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_mcu8080_st7796s_register: %s", name);

    sg_disp_mcu8080_cfg.cfg.width = dev_cfg->width;
    sg_disp_mcu8080_cfg.cfg.height = dev_cfg->height;
    sg_disp_mcu8080_cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_mcu8080_cfg.cfg.clk = dev_cfg->clk;
    sg_disp_mcu8080_cfg.cfg.data_bits = dev_cfg->data_bits;

    sg_disp_mcu8080_cfg.in_fmt = dev_cfg->pixel_fmt;
    sg_disp_mcu8080_cfg.rotation = dev_cfg->rotation;
    sg_disp_mcu8080_cfg.te_pin = dev_cfg->te_pin;
    sg_disp_mcu8080_cfg.te_mode = dev_cfg->te_mode;
    sg_disp_mcu8080_cfg.is_swap = false;

    memcpy(&sg_disp_mcu8080_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_mcu8080_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdd_disp_mcu8080_device_register(name, &sg_disp_mcu8080_cfg);
}
