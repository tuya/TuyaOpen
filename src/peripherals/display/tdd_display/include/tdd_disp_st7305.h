/**
 * @file tdd_disp_st7305.h
 * @brief ST7305 monochrome LCD display driver header file
 *
 * This file contains the register definitions, command definitions, and function
 * declarations for the ST7305 LCD display controller. The ST7305 is a monochrome
 * display controller designed for small-size LCDs, commonly used in industrial
 * and embedded applications with SPI interface support.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
 
#ifndef __TDD_DISP_ST7305_H__
#define __TDD_DISP_ST7305_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

/***********************************************************
***********************macro define***********************
***********************************************************/

/***********************************************************
***********************type define***********************
***********************************************************/
#define ST7305_CASET 0x2A // Column Address Set
#define ST7305_RASET 0x2B // Row Address Set
#define ST7305_RAMWR 0x2C

/***********************************************************
***********************function declare**********************
***********************************************************/
/**
 * @brief Sets the initialization sequence for the ST7305 display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_spi_mono_st7305_set_init_seq(uint8_t *init_seq);

/**
 * @brief Registers an ST7305 monochrome display device using the SPI interface with the display management system.
 *
 * This function creates and initializes a new ST7305 display device instance, 
 * configures its frame buffer and hardware-specific settings, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the SPI device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_spi_mono_st7305_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg);

#endif // __TDD_DISP_SPI_ST7305_H__
