/**
 * @file tdd_disp_st7735s.h
 * @brief Header file for the ST7735S display driver module.
 *
 * This file contains the definitions and function declarations required 
 * for interacting with the ST7735S display using QSPI interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_DISP_ST7789_H__
#define __TDD_DISP_ST7789_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define ST7735S_CASET     0x2A // Column Address Set
#define ST7735S_RASET     0x2B // Row Address Set
#define ST7735S_RAMWR     0x2C

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers an ST7735S TFT display device using the QSPI interface with the display management system.
 *
 * This function configures and registers a display device for the ST7735S series of TFT LCDs 
 * using the QSPI communication protocol. It copies configuration parameters from the provided 
 * device configuration and uses a predefined initialization sequence specific to ST7735S.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the QSPI device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_qspi_st7735s_register(char *name, DISP_QSPI_DEVICE_CFG_T *dev_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ST7735S_H__ */
