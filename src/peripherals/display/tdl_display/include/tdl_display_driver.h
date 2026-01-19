/**
 * @file tdl_display_driver.h
 * @brief TDL display driver interface definitions
 *
 * This file defines the high-level display driver interface for the TDL (Tuya Display Layer)
 * system. It provides abstraction layer functions and data structures for managing different
 * types of display controllers including SPI, QSPI, RGB, and MCU 8080 interfaces, enabling
 * unified display operations across various hardware configurations.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_DISPLAY_DRIVER_H__
#define __TDL_DISPLAY_DRIVER_H__

#include "tuya_cloud_types.h"
#include "tdl_display_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define DISPLAY_DEV_NAME_MAX_LEN 32

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_GPIO_NUM_E   pin;
    TUYA_GPIO_LEVEL_E active_level;
} TUYA_DISPLAY_IO_CTRL_T;

typedef struct {
    TUYA_PWM_NUM_E       id;
    TUYA_PWM_BASE_CFG_T  cfg;
} TUYA_DISPLAY_PWM_CTRL_T;

typedef enum  {
    TUYA_DISP_BL_TP_NONE,
    TUYA_DISP_BL_TP_GPIO,
    TUYA_DISP_BL_TP_PWM,
    TUYA_DISP_BL_TP_CUSTOM,
}TUYA_DISPLAY_BL_TYPE_E;

typedef struct {
    TUYA_DISPLAY_BL_TYPE_E    type;
    union {
        TUYA_DISPLAY_IO_CTRL_T   gpio;
        TUYA_DISPLAY_PWM_CTRL_T  pwm;
    };
} TUYA_DISPLAY_BL_CTRL_T;

typedef void*  TDD_DISP_DEV_HANDLE_T;

typedef OPERATE_RET (*TDD_DISPLAY_SEQ_INIT_CB)(void);

typedef struct {
    TUYA_DISPLAY_TYPE_E       type;
    uint16_t                  width;
    uint16_t                  height;
    bool                      is_swap;
    bool                      has_vram;
    TUYA_DISPLAY_PIXEL_FMT_E  fmt;
    TUYA_DISPLAY_ROTATION_E   rotation;
    TUYA_DISPLAY_BL_CTRL_T    bl;
    TUYA_DISPLAY_IO_CTRL_T    power;
} TDD_DISP_DEV_INFO_T;

typedef struct {
    OPERATE_RET (*open)(TDD_DISP_DEV_HANDLE_T device);
    OPERATE_RET (*flush)(TDD_DISP_DEV_HANDLE_T device, TDL_DISP_FRAME_BUFF_T *frame_buff);
    OPERATE_RET (*close)(TDD_DISP_DEV_HANDLE_T device);
} TDD_DISP_INTFS_T;

typedef TDL_DISP_FRAME_BUFF_T *(*TDD_DISP_CONVERT_FB_CB)(TDL_DISP_FRAME_BUFF_T *frame_buff);
typedef OPERATE_RET (*TDD_SET_BACKLIGHT_CB)(uint8_t brightness, void *arg);

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Registers a display device with the display management system.
 *
 * This function creates and initializes a new display device entry in the internal 
 * device list, binding it with the provided name, hardware interfaces, callbacks, 
 * and device information.
 *
 * @param name Name of the display device (used for identification).
 * @param tdd_hdl Handle to the low-level display driver instance.
 * @param intfs Pointer to the display interface functions (open, flush, close, etc.).
 * @param dev_info Pointer to the display device information structure.
 *
 * @return Returns OPRT_OK on success, or an appropriate error code if registration fails.
 */
OPERATE_RET tdl_disp_device_register(char *name, TDD_DISP_DEV_HANDLE_T tdd_hdl, \
                                     TDD_DISP_INTFS_T *intfs, TDD_DISP_DEV_INFO_T *dev_info);

/**
 * @brief Registers a custom backlight control callback for a display device
 * 
 * @param name Name of the display device
 * @param set_bl_cb Pointer to the custom backlight control callback function
 * @param arg User-defined argument to be passed to the callback function
 * 
 * @return OPERATE_RET Returns OPRT_OK on success, OPRT_INVALID_PARM if parameters are NULL,
 *                     or OPRT_COM_ERROR if the display device is not found
 */
OPERATE_RET tdl_disp_custom_backlight_register(char *name, TDD_SET_BACKLIGHT_CB set_bl_cb, void *arg);
                                
#ifdef __cplusplus
}
#endif

#endif /* __TDL_DISPLAY_DRIVER_H__ */
