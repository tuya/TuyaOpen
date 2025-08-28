/**
 * @file tdd_display_rgb.h
 * @brief RGB display driver interface definitions.
 *
 * This header provides macro definitions and function declarations for
 * controlling RGB displays via parallel RGB interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISPLAY_RGB_H__
#define __TDD_DISPLAY_RGB_H__

#include "tuya_cloud_types.h"
#include "tdl_display_driver.h"

#if defined(ENABLE_RGB) && (ENABLE_RGB == 1)

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_RGB_BASE_CFG_T         cfg;
    TUYA_DISPLAY_BL_CTRL_T      bl;
    TUYA_DISPLAY_IO_CTRL_T      power;
    TDD_DISPLAY_SEQ_INIT_CB     init_cb; 
    TUYA_DISPLAY_ROTATION_E     rotation;
    bool                        is_swap; 
}TDD_DISP_RGB_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers an RGB display device with the display management system.
 *
 * This function creates and initializes a new RGB display device instance, 
 * configures its interface functions, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param rgb Pointer to the RGB display device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_rgb_device_register(char *name, TDD_DISP_RGB_CFG_T *rgb);

#ifdef __cplusplus
}
#endif

#endif

#endif /* __TDD_DISPLAY_RGB_H__ */