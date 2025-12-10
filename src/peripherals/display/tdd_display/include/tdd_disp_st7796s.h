/**
 * @file tdd_disp_st7796s.h
 * @brief ST7796S LCD display driver header file
 *
 * This file contains the register definitions, command definitions, and function
 * declarations for the ST7796S LCD display controller. The ST7796S is a single-chip
 * controller/driver for 262K-color graphic TFT-LCD, supporting resolutions up to
 * 320x480, with MCU 8080 parallel interface for fast data transfer.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISP_ST7796_H__
#define __TDD_DISP_ST7796_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define ST7796S_CASET  0x2A // Column Address Set
#define ST7796S_RASET  0x2B // Row Address Set
#define ST7796S_RAMWR  0x2C
#define ST7796S_RAMWRC 0x3C
/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Sets the initialization sequence for the ST7796S display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_mcu8080_st7796s_set_init_seq(const uint32_t *init_seq);

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

#endif /* __TDD_DISP_ST7796S_H__ */
