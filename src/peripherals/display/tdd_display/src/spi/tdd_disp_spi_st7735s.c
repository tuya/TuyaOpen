/**
 * @file tdd_disp_spi_st7735s.c
 * @brief ST7735S LCD driver implementation with SPI interface
 *
 * This file provides the implementation for ST7735S TFT LCD displays using SPI
 * interface. It includes the initialization sequence, display control
 * functions, and hardware-specific configurations for ST7735S displays, enabling
 * high-speed data transfer through SPI communication.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_log.h"

#include "tdd_disp_st7735s.h"
#include "tdd_display_spi.h"

/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cST7735S_INIT_SEQ[] = {
    1,    0,    0x01, 
    1,    100,  0x11, 
    4,    100,  0xB1, 0x02, 0x35, 0x36,
    4,    0,    0xB2, 0x02, 0x35, 0x36,
    7,    0,    0xB3, 0x02, 0x35, 0x36, 0x02, 0x35, 0x36,
    2,    0,    0xB4, 0x00,
    4,    0,    0xC0, 0xa2, 0x02, 0x84,
    2,    0,    0xC1, 0xC5,
    3,    0,    0xC2, 0x0D, 0x00,
    3,    0,    0xC3, 0x8A, 0x2A,
    3,    0,    0xC4, 0x8D, 0xEE,
    2,    0,    0xC5, 0x02,
    17,   0,    0xE0, 0x12, 0x1C, 0x10, 0x18, 0x33, 0x2C, 0x25, 0x28, 0x28, 0x27, 0x2F, 0x3C, 0x00, 0x03, 0x03, 0x10,
    17,   0,    0xE1, 0x12, 0x1C, 0x10, 0x18, 0x2D, 0x28, 0x23, 0x28, 0x28, 0x26, 0x2F, 0x3B, 0x00, 0x03, 0x03, 0x10,
    2,    0,    0x3A, 0x05, 
    2,    0,    0x36, 0x08,
    1,    0,    0x29,
    1,    0,    0x2C,
    0 // Terminate list
};

static TDD_DISP_SPI_CFG_T sg_disp_spi_cfg = {
    .cfg =
        {
            .cmd_caset = ST7735S_CASET,
            .cmd_raset = ST7735S_RASET,
            .cmd_ramwr = ST7735S_RAMWR,
        },
        
    .is_swap = true,
    .init_seq = cST7735S_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Sets the initialization sequence for the ST7735S display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_spi_st7735s_set_init_seq(const uint8_t *init_seq)
{
    if(NULL == init_seq) {
        return OPRT_INVALID_PARM;
    }

    sg_disp_spi_cfg.init_seq = init_seq;

    return OPRT_OK;
}

/**
 * @brief Registers an ST7735S TFT display device using the SPI interface with the display management system.
 *
 * This function configures and registers a display device for the ST7735S series of TFT LCDs 
 * using the SPI communication protocol. It copies configuration parameters from the provided 
 * device configuration and uses a predefined initialization sequence specific to ST7735S.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the SPI device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_spi_st7735s_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_spi_st7735s_register: %s", name);

    sg_disp_spi_cfg.cfg.width = dev_cfg->width;
    sg_disp_spi_cfg.cfg.height = dev_cfg->height;
    sg_disp_spi_cfg.cfg.x_offset  = dev_cfg->x_offset;
    sg_disp_spi_cfg.cfg.y_offset  = dev_cfg->y_offset;
    sg_disp_spi_cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_spi_cfg.cfg.port = dev_cfg->port;
    sg_disp_spi_cfg.cfg.spi_clk = dev_cfg->spi_clk;
    sg_disp_spi_cfg.cfg.cs_pin = dev_cfg->cs_pin;
    sg_disp_spi_cfg.cfg.dc_pin = dev_cfg->dc_pin;
    sg_disp_spi_cfg.cfg.rst_pin = dev_cfg->rst_pin;
    sg_disp_spi_cfg.rotation = dev_cfg->rotation;

    memcpy(&sg_disp_spi_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_spi_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdd_disp_spi_device_register(name, &sg_disp_spi_cfg);
}