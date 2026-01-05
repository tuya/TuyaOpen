/**
 * @file tdd_display_mcu8080.h
 * @brief MCU8080 display driver interface definitions.
 *
 * This header provides macro definitions and function declarations for
 * controlling MCU8080 displays via parallel interface.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_DISPLAY_MCU8080_H__
#define __TDD_DISPLAY_MCU8080_H__

#include "tuya_cloud_types.h"
#include "tdl_display_driver.h"

#if defined(ENABLE_MCU8080) && (ENABLE_MCU8080 == 1)

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
    TUYA_8080_BASE_CFG_T        cfg;
    TUYA_DISPLAY_BL_CTRL_T      bl;
    TUYA_DISPLAY_IO_CTRL_T      power;
    TUYA_DISPLAY_ROTATION_E     rotation;
    TUYA_DISPLAY_PIXEL_FMT_E    in_fmt;
    bool                        is_swap; 
    TUYA_GPIO_NUM_E             te_pin;
    TUYA_GPIO_IRQ_E             te_mode;
    uint8_t                     cmd_caset;
    uint8_t                     cmd_raset;
    uint8_t                     cmd_ramwr;
    uint8_t                     cmd_ramwrc;
    uint8_t                     x_offset;
    uint8_t                     y_offset;
    const uint32_t             *init_seq; // Initialization commands for the display
    TDD_DISP_CONVERT_FB_CB      convert_cb; // Optional framebuffer conversion callback
}TDD_DISP_MCU8080_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers an MCU8080 display device with the display management system.
 *
 * This function creates and initializes a new MCU8080 display device instance, 
 * configures its interface functions, and registers it under the specified name.
 *
 * @param name Name of the display device (used for identification).
 * @param mcu8080 Pointer to the MCU8080 display device configuration structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdd_disp_mcu8080_device_register(char *name, TDD_DISP_MCU8080_CFG_T *mcu8080);

#ifdef __cplusplus
}
#endif

#endif

#endif /* __TDD_DISPLAY_MCU8080_H__ */