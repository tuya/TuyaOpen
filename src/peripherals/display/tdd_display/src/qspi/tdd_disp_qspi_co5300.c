/**
 * @file tdd_disp_qspi_co5300.c
 * @brief CO5300 QSPI display driver implementation
 * 
 * This file implements the driver for CO5300 QSPI display controller, providing
 * functions for display initialization, device registration, backlight control,
 * and custom initialization sequence configuration.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tdd_display_qspi.h"
#include "tdd_disp_co5300.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define CO5300_X_OFFSET         6
#define CO5300_Y_OFFSET         0

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
const uint8_t cCO5300_INIT_SEQ[] = {
    2,  0,   0xFE, 0x20,
    2,  0,   0x19, 0x10,
    2,  0,   0x1C, 0xa0,
    2,  0,   0xFE, 0x00,
    2,  0,   0xC4, 0x80,
    2,  0,   0x3A, 0x55,    
    2,  0,   0x35, 0x00,
    2,  0,   0x53, 0x20,
    2,  0,   0x51, 0xFF,
    2,  0,   0x63, 0xFF,    
    1, 200,  0x11,
    1,  0,   0x29,
    0, 
};

static TDD_DISP_QSPI_CFG_T sg_disp_qspi_cfg = {
    .cfg = {
        .refresh_method = QSPI_REFRESH_BY_FRAME,
        .pixel_pre_cmd = {
            .cmd = CO5300_WRITE_COLOR,
            .addr[0] = CO5300_ADDR_0,
            .addr[1] = CO5300_ADDR_1,
            .addr[2] = CO5300_ADDR_2,
            .addr_size  = CO5300_ADDR_LEN,
            .cmd_lines  = TUYA_QSPI_1WIRE,
            .addr_lines = TUYA_QSPI_1WIRE,
        },
        .has_vram = true,
        .cmd_caset = CO5300_CASET,
        .cmd_raset = CO5300_RASET,
        .cmd_ramwr = CO5300_WRITE_REG,
    },
    .is_swap = true,
    .init_seq = cCO5300_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Sets the initialization sequence for the CO5300 display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_qspi_co5300_set_init_seq(const uint8_t *init_seq)
{
    if(NULL == init_seq) {
        return OPRT_INVALID_PARM;
    }

    sg_disp_qspi_cfg.init_seq = init_seq;

    return OPRT_OK;
}

/**
 * @brief Send command to set backlight brightness for CO5300 display via QSPI
 * 
 * @param brightness Backlight brightness value (0-100)
 * @param arg Pointer to additional arguments or context (can be NULL)
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or error code on failure
 */
 OPERATE_RET tdd_qspi_co5300_send_cmd_set_bl(uint8_t brightness, void *arg)
 {
     // map brightness to 0-255
     brightness = (uint8_t)(((uint32_t)brightness * 255) / 100);

     if (brightness == 0) {
         brightness = 5;
     }

     return tdd_disp_qspi_send_cmd(&sg_disp_qspi_cfg.cfg, CO5300_BL, &brightness, 1);
 }

/**
 * @brief Registers the CO5300 QSPI display device with the display driver
 * 
 * @param name Device name to register
 * @param dev_cfg Pointer to the QSPI device configuration structure
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if parameters are NULL
 */
OPERATE_RET tdd_disp_qspi_co5300_register(char *name, DISP_QSPI_DEVICE_CFG_T *dev_cfg)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_qspi_co5300_register: %s", name);

    sg_disp_qspi_cfg.cfg.width     = dev_cfg->width;
    sg_disp_qspi_cfg.cfg.height    = dev_cfg->height;
    sg_disp_qspi_cfg.cfg.x_offset  = dev_cfg->x_offset;
    sg_disp_qspi_cfg.cfg.y_offset  = dev_cfg->y_offset;

    sg_disp_qspi_cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_qspi_cfg.cfg.port      = dev_cfg->port;
    sg_disp_qspi_cfg.cfg.freq_hz   = dev_cfg->spi_clk;
    sg_disp_qspi_cfg.cfg.rst_pin   = dev_cfg->rst_pin;
    sg_disp_qspi_cfg.rotation      = dev_cfg->rotation;

    memcpy(&sg_disp_qspi_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_qspi_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    TUYA_CALL_ERR_RETURN(tdd_disp_qspi_device_register(name, &sg_disp_qspi_cfg));

    return OPRT_OK;
}