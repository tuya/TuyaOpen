/**
 * @file tdd_disp_st7306.h
 * @brief ST7306 display driver interface definitions.
 *
 * This header provides macro definitions and function declarations for
 * controlling the ST7306 display via SPI interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISP_ST7306_H__
#define __TDD_DISP_ST7306_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

/***********************************************************
***********************macro define***********************
***********************************************************/


/***********************************************************
***********************type define***********************
***********************************************************/
#define ST7306_CASET     0x2A // Column Address Set
#define ST7306_RASET     0x2B // Row Address Set
#define ST7306_RAMWR     0x2C

/***********************************************************
***********************function declare**********************
***********************************************************/
/**
 * @brief Sets the initialization sequence for the ST7306 display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_spi_mono_st7306_set_init_seq(uint8_t *init_seq);

/**
 * @brief Registers an ST7306 display device using I2 pixel format over SPI with the display management system.
 *
 * This function creates and initializes a new ST7306 display device instance with I2 pixel format, 
 * configures its frame buffer and hardware-specific settings, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the SPI device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_spi_i2_st7306_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg);

#endif // __TDD_DISP_SPI_ST7306_H__
