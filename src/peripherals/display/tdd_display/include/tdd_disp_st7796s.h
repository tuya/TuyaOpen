/**
 * @file tdd_disp_st7796s.h
 * @brief Header file for the ST7796S display driver module.
 *
 * This file contains the definitions and function declarations required 
 * for interacting with the ST7796S display using MCU8080 interface.
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
#define ST7796S_CASET     0x2A // Column Address Set
#define ST7796S_RASET     0x2B // Row Address Set
#define ST7796S_RAMWR     0x2C
#define ST7796S_RAMWRC    0x3C
/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers an ST7796S TFT display device using the MCU8080 interface with the display management system.
 *
 * This function configures and registers a display device for the ST7796S series of TFT LCDs 
 * using the MCU8080 parallel interface. It copies configuration parameters from the provided 
 * device configuration and uses a predefined initialization sequence specific to ST7796S.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the MCU8080 device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_mcu8080_st7796s_register(char *name, DISP_MCU8080_DEVICE_CFG_T *dev_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ST7735S_H__ */
