/**
 * @file tdd_disp_st7701sn.h
 * @brief ST7701SN LCD display driver header file
 *
 * This file contains the register definitions, command definitions, and function
 * declarations for the ST7701SN LCD display controller. The ST7701SN is a 16.7M-color
 * single-chip SOC driver for a TFT liquid crystal display with resolution of up to
 * 320x480, supporting RGB interface for high-speed data transfer.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISP_ST7701SN_H__
#define __TDD_DISP_ST7701SN_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Sets the initialization sequence for the st7701sn display
 * 
 * @param init_seq Pointer to the initialization sequence array
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, or OPRT_INVALID_PARM if init_seq is NULL
 */
OPERATE_RET tdd_disp_rgb_st7701sn_set_init_seq(const uint8_t *init_seq);

/**
 * @brief Registers an ST7701SN RGB LCD display device with the display management system.
 *
 * This function configures and registers a display device for the ST7701SN series of RGB LCDs 
 * using software SPI. It copies configuration parameters from the provided device configuration 
 * and sets up the initialization sequence specific to ST7701SN.
 *
 * @param name Name of the display device (used for identification).
 * @param dev_cfg Pointer to the RGB display device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_rgb_st7701sn_register(char *name, DISP_RGB_DEVICE_CFG_T *dev_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_DISP_ST7701SN_H__ */
