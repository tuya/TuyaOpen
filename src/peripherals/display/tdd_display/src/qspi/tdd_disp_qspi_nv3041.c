/**
 * @file tdd_disp_qspi_nv3041.c
 * @brief NV3041 QSPI display driver implementation
 *
 * This file implements the driver for NV3041 QSPI display controller, providing
 * functions for display initialization, device registration, and custom initialization
 * sequence configuration. The driver supports QSPI interface communication and frame-based
 * refresh method.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tdd_display_qspi.h"
#include "tdd_disp_nv3041.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
const uint8_t cNV3041_INIT_SEQ[] = {
    2,  0,  0xFF,  0xa5,
    2,  0,  0xe7,  0x10,
    2,  0,  0x35,  0x00,
    2,  0,  0x3a,  0x01,
    2,  0,  0x40,  0x01,
    2,  0,  0x41,  0x01,
    2,  0,  0x55,  0x01,
    2,  0,  0x44,  0x15,
    2,  0,  0x45,  0x15,
    2,  0,  0x7d,  0x03,
    2,  0,  0xc1,  0xbb,
    2,  0,  0xc2,  0x14,
    2,  0,  0xc3,  0x13,
    2,  0,  0xc6,  0x3e,
    2,  0,  0xc7,  0x25,
    2,  0,  0xc8,  0x11,
    2,  0,  0x7a,  0x7c,
    2,  0,  0x6f,  0x56,
    2,  0,  0x78,  0x2a,
    2,  0,  0x73,  0x08,
    2,  0,  0x74,  0x12,
    2,  0,  0xc9,  0x00,
    2,  0,  0x67,  0x11,
    2,  0,  0x51,  0x4b,
    2,  0,  0x52,  0x7c,
    2,  0,  0x53,  0x45,
    2,  0,  0x54,  0x77,
    2,  0,  0x46,  0x0a,
    2,  0,  0x47,  0x2a,
    2,  0,  0x48,  0x0a,
    2,  0,  0x49,  0x1a,
    2,  0,  0x56,  0x43,
    2,  0,  0x57,  0x42,
    2,  0,  0x58,  0x3c,
    2,  0,  0x59,  0x64,
    2,  0,  0x5A,  0x41,
    2,  0,  0x5B,  0x3c,
    2,  0,  0x5c,  0x02,
    2,  0,  0x5d,  0x3c,
    2,  0,  0x5e,  0x1f,
    2,  0,  0x60,  0x80,
    2,  0,  0x61,  0x3f,
    2,  0,  0x62,  0x21,
    2,  0,  0x63,  0x07,
    2,  0,  0x64,  0x0e,
    2,  0,  0x65,  0x01,
    2,  0,  0xca,  0x20,
    2,  0,  0xcb,  0x52,
    2,  0,  0xcc,  0x10,
    2,  0,  0xcd,  0x42,
    2,  0,  0xd0,  0x20,
    2,  0,  0xd1,  0x52,
    2,  0,  0xd2,  0x10,
    2,  0,  0xd3,  0x42,
    2,  0,  0xd4,  0x0a,
    2,  0,  0xd5,  0x32,
    2,  0,  0x6e,  0x14,
    2,  0,  0xe5,  0x06,
    2,  0,  0xe6,  0x00,
    2,  0,  0xf8,  0x06,
    2,  0,  0xf9,  0x00,
    2,  0,  0x80,  0x08,
    2,  0,  0xa0,  0x08,
    2,  0,  0x81,  0x0a,
    2,  0,  0xa1,  0x0a,
    2,  0,  0x82,  0x09,
    2,  0,  0xa2,  0x09,
    2,  0,  0x86,  0x38,
    2,  0,  0xa6,  0x2a,
    2,  0,  0x87,  0x4a,
    2,  0,  0xa7,  0x40,
    2,  0,  0x83,  0x39,
    2,  0,  0xa3,  0x39,
    2,  0,  0x84,  0x37,
    2,  0,  0xa4,  0x37,
    2,  0,  0x85,  0x28,
    2,  0,  0xa5,  0x28,
    2,  0,  0x88,  0x0b,
    2,  0,  0xa8,  0x04,
    2,  0,  0x89,  0x13,
    2,  0,  0xa9,  0x09,
    2,  0,  0x8a,  0x1b,
    2,  0,  0xaa,  0x11,
    2,  0,  0x8b,  0x11,
    2,  0,  0xab,  0x0d,
    2,  0,  0x8c,  0x14,
    2,  0,  0xac,  0x13,
    2,  0,  0x8d,  0x15,
    2,  0,  0xad,  0x0e,
    2,  0,  0x8e,  0x10,
    2,  0,  0xae,  0x0f,
    2,  0,  0x8f,  0x18,
    2,  0,  0xaf,  0x0e,
    2,  0,  0x90,  0x07,
    2,  0,  0xb0,  0x05,
    2,  0,  0x91,  0x11,
    2,  0,  0xb1,  0x0e,
    2,  0,  0x92,  0x19,
    2,  0,  0xb2,  0x14,
    2,  0,  0xff,  0x00,
    1, 120,  0x11, 
    1,  0,   0x21,       // Display Inversion ON
    1,  20,  0x29,
    0,
};

static TDD_DISP_QSPI_CFG_T sg_disp_qspi_cfg = {
    .cfg = {
        .refresh_method = QSPI_REFRESH_BY_FRAME,
        .pixel_pre_cmd = {
            .cmd = NV3041_WRITE_COLOR,
            .addr[0] = NV3041_ADDR_0,
            .addr[1] = NV3041_ADDR_1,
            .addr[2] = NV3041_ADDR_2,
            .addr_size  = NV3041_ADDR_LEN,
            .cmd_lines  = TUYA_QSPI_1WIRE,
            .addr_lines = TUYA_QSPI_1WIRE,
        },
        .has_vram = true,
        .cmd_caset = NV3041_CASET,
        .cmd_raset = NV3041_RASET,
        .cmd_ramwr = NV3041_WRITE_REG,
    },
    .is_swap = true,
    .init_seq = cNV3041_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Sets the initialization sequence for the NV3041 display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_qspi_nv3041_set_init_seq(const uint8_t *init_seq)
{
    if(NULL == init_seq) {
        return OPRT_INVALID_PARM;
    }

    sg_disp_qspi_cfg.init_seq = init_seq;

    return OPRT_OK;
}

/**
 * @brief Registers the NV3041 QSPI display device with the display driver
 * 
 * @param name Device name to register
 * @param dev_cfg Pointer to the QSPI device configuration structure
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if parameters are NULL
 */
OPERATE_RET tdd_disp_qspi_nv3041_register(char *name, DISP_QSPI_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_qspi_nv3041_register: %s", name);

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

    return tdd_disp_qspi_device_register(name, &sg_disp_qspi_cfg);
}