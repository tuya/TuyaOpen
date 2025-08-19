/**
 * @file tdd_disp_qspi_co5300.c
 * @brief CO5300 LCD driver implementation with QSPI interface
 *
 * This file provides the implementation for CO5300 TFT LCD displays using QSPI
 * (Quad SPI) interface. It includes the initialization sequence, display control
 * functions, and hardware-specific configurations for CO5300 displays, enabling
 * high-speed data transfer through quad SPI communication.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_log.h"

#include "tdd_disp_co5300.h"
#include "tdl_display_driver.h"

/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cCO5300_INIT_SEQ[] = {
    2,    0,    0xFE, 0x20,
    2,    0,    0x19, 0x10,
    2,    0,    0x1C, 0xA0,

    2,    0,    0xFE, 0x00,
    2,    0,    0xC4, 0x80,
    2,    0,    0x3A, 0x55,
    2,    0,    0x35, 0x00,
    2,    0,    0x53, 0x20,
    2,    0,    0x51, 0xFF,
    2,    0,    0x63, 0xFF,
    5,    0,    0x2A, 0x00, 0x06, 0x01, 0xD7,
    5,    200,  0x2B, 0x00, 0x00, 0x01, 0xD1,
    1,    200,  0x11,
    1,    0,    0x29,
    0 // Terminate list
};

static TDD_DISP_QSPI_CFG_T sg_disp_qspi_cfg = {
    .cfg =
        {
            .cmd_caset = CO5300_CASET,
            .cmd_raset = CO5300_RASET,
            .cmd_ramwr = CO5300_RAMWR,
        },

    .init_seq = cCO5300_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Registers an CO5300 TFT display device using the QSPI interface with the display management system.
 *
 * This function configures and registers a display device for the CO5300 series of TFT LCDs 
 * using the QSPI communication protocol. It copies configuration parameters from the provided 
 * device configuration and uses a predefined initialization sequence specific to CO5300.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the QSPI device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_qspi_co5300_register(char *name, DISP_QSPI_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }
    PR_NOTICE("12345");
    PR_NOTICE("tdd_disp_qspi_co5300_register: %s", name);
    // return 0;
    sg_disp_qspi_cfg.cfg.width = dev_cfg->width;
    sg_disp_qspi_cfg.cfg.height = dev_cfg->height;
    sg_disp_qspi_cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_qspi_cfg.cfg.port = dev_cfg->port;
    sg_disp_qspi_cfg.cfg.spi_clk = dev_cfg->spi_clk;
    sg_disp_qspi_cfg.cfg.cs_pin = dev_cfg->cs_pin;
    sg_disp_qspi_cfg.cfg.dc_pin = dev_cfg->te_pin;
    sg_disp_qspi_cfg.cfg.rst_pin = dev_cfg->rst_pin;
    sg_disp_qspi_cfg.rotation = dev_cfg->rotation;

    // memcpy(&sg_disp_qspi_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    // memcpy(&sg_disp_qspi_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdl_disp_qspi_device_register(name, &sg_disp_qspi_cfg);
}