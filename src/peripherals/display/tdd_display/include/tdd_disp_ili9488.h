/**
 * @file tdd_disp_ili9488.h
 * @brief ILI9488 LCD display driver header file
 *
 * This file contains the register definitions, command definitions, and function
 * declarations for the ILI9488 LCD display controller. The ILI9488 is a 16.7M-color
 * single-chip SOC driver for a TFT liquid crystal display with resolution of up to
 * 320x480, supporting RGB interface for high-speed data transfer.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISP_ILI9488_H__
#define __TDD_DISP_ILI9488_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* ILI9488 commands */
#define ILI9488_NOP     0x00
#define ILI9488_SWRESET 0x01
#define ILI9488_RDDID   0x04
#define ILI9488_RDDST   0x09

#define ILI9488_SLPIN  0x10
#define ILI9488_SLPOUT 0x11
#define ILI9488_PTLON  0x12
#define ILI9488_NORON  0x13

#define ILI9488_RDMODE     0x0A
#define ILI9488_RDMADCTL   0x0B
#define ILI9488_RDPIXFMT   0x0C
#define ILI9488_RDIMGFMT   0x0D
#define ILI9488_RDSELFDIAG 0x0F

#define ILI9488_INVOFF   0x20
#define ILI9488_INVON    0x21
#define ILI9488_GAMMASET 0x26
#define ILI9488_DISPOFF  0x28
#define ILI9488_DISPON   0x29

#define ILI9488_CASET 0x2A
#define ILI9488_PASET 0x2B
#define ILI9488_RAMWR 0x2C
#define ILI9488_RAMRD 0x2E

#define ILI9488_PTLAR  0x30
#define ILI9488_MADCTL 0x36
#define ILI9488_PIXFMT 0x3A

#define ILI9488_IFMODE  0xB0
#define ILI9488_FRMCTR1 0xB1
#define ILI9488_FRMCTR2 0xB2
#define ILI9488_FRMCTR3 0xB3
#define ILI9488_INVCTR  0xB4
#define ILI9488_PRCTR   0xB5
#define ILI9488_DFUNCTR 0xB6

#define ILI9488_PWCTR1 0xC0
#define ILI9488_PWCTR2 0xC1
#define ILI9488_PWCTR3 0xC2
#define ILI9488_PWCTR4 0xC3
#define ILI9488_PWCTR5 0xC4
#define ILI9488_VMCTR1 0xC5
#define ILI9488_VMCTR2 0xC7

#define ILI9488_RDID1 0xDA
#define ILI9488_RDID2 0xDB
#define ILI9488_RDID3 0xDC
#define ILI9488_RDID4 0xDD

#define ILI9488_GMCTRP1  0xE0
#define ILI9488_GMCTRN1  0xE1
#define ILI9488_SETIMAGE 0xE9

#define ILI9488_ACTRL3 0xF7
#define ILI9488_ACTRL4 0xF8

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Sets the initialization sequence for the ILI9488 display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_rgb_ili9488_set_init_seq(const uint8_t *init_seq);

/**
 * @brief Registers an ILI9488 RGB LCD display device with the display management system.
 *
 * This function configures and registers a display device for the ILI9488 series of RGB LCDs 
 * using software SPI. It copies configuration parameters from the provided device configuration 
 * and sets up the initialization sequence specific to ILI9488.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the RGB display device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_rgb_ili9488_register(char *name, DISP_RGB_DEVICE_CFG_T *dev_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_RGB_ILI9488_H__ */
