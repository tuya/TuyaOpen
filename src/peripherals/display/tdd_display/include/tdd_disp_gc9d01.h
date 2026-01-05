/**
 * @file tdd_disp_GC9D01.h
 * @brief GC9D01 LCD display driver header file
 *
 * This file contains the register definitions, command definitions, and function
 * declarations for the GC9D01 LCD display controller. The GC9D01 is a single-chip
 * solution for 240x240 resolution displays with 262K colors, supporting SPI interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISP_GC9D01_H__
#define __TDD_DISP_GC9D01_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* GC9D01 commands */

#define GC9D01_CASET   0x2A // Column Address Set
#define GC9D01_RASET   0x2B // Row Address Set
#define GC9D01_RAMWR   0x2C


/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers a GC9D01 TFT display device using the SPI interface with the display management system.
 *
 * This function configures and registers a display device for the GC9D01 series of TFT LCDs 
 * using the SPI communication protocol. It copies configuration parameters from the provided 
 * device configuration and uses a predefined initialization sequence specific to GC9D01.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the SPI device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_spi_gc9d01_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_GC9D01_H__ */
