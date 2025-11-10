/**
 * @file tdd_disp_spi_st7735s_xlt.h
 * @brief ST7735S XLT LCD display driver header file
 *
 * This file contains the register definitions, command definitions, and function
 * declarations for the ST7735S XLT LCD display controller. The ST7735S is a single-chip
 * controller/driver for 262K-color graphic TFT-LCD, supporting resolutions up to
 * 240x320, with SPI interface for high-speed data transfer. This XLT variant includes
 * customized initialization sequence and window setting optimizations.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISP_SPI_ST7735S_XLT_H__
#define __TDD_DISP_SPI_ST7735S_XLT_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define ST7735S_CASET 0x2A // Column Address Set
#define ST7735S_RASET 0x2B // Row Address Set
#define ST7735S_RAMWR 0x2C // Memory Write

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers an ST7735S XLT TFT display device using the SPI interface with the display management system.
 *
 * This function configures and registers a display device for the ST7735S XLT series of TFT LCDs 
 * using the SPI communication protocol. It copies configuration parameters from the provided 
 * device configuration and uses a predefined initialization sequence specific to ST7735S XLT variant.
 * The XLT variant includes optimized window setting with custom offsets.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the SPI device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_spi_st7735s_xlt_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_SPI_ST7735S_XLT_H__ */
