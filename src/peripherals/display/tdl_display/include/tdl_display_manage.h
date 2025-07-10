/**
 * @file tdl_display_manage.h
 * @brief TDL display management system header file
 *
 * This file provides the core display management functionality for the TDL (Tuya Display Layer)
 * system. It includes display device registration, initialization, control functions, and
 * hardware abstraction for various display interfaces. The management system handles display
 * lifecycle, power control, backlight control, and provides unified API for display operations.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_DISPLAY_MANAGE_H__
#define __TDL_DISPLAY_MANAGE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void *TDL_DISP_HANDLE_T;

typedef enum {
    DISP_FB_TP_SRAM = 0,
    DISP_FB_TP_PSRAM,
} DISP_FB_RAM_TP_E;

typedef struct TDL_DISP_FRAME_BUFF_T TDL_DISP_FRAME_BUFF_T;

typedef void (*FRAME_BUFF_FREE_CB)(TDL_DISP_FRAME_BUFF_T *frame_buff);

struct TDL_DISP_FRAME_BUFF_T {
    DISP_FB_RAM_TP_E type;
    TUYA_DISPLAY_PIXEL_FMT_E fmt;
    uint16_t width;
    uint16_t height;
    FRAME_BUFF_FREE_CB free_cb;
    uint32_t len;
    uint8_t *frame;
};

typedef struct {
    TUYA_DISPLAY_TYPE_E type;
    TUYA_DISPLAY_ROTATION_E rotation;
    uint16_t width;
    uint16_t height;
    TUYA_DISPLAY_PIXEL_FMT_E fmt;
} TDL_DISP_DEV_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/
TDL_DISP_HANDLE_T tdl_disp_find_dev(char *name);

OPERATE_RET tdl_disp_dev_open(TDL_DISP_HANDLE_T disp_hdl);

OPERATE_RET tdl_disp_dev_flush(TDL_DISP_HANDLE_T disp_hdl, TDL_DISP_FRAME_BUFF_T *frame_buff);

OPERATE_RET tdl_disp_dev_get_info(TDL_DISP_HANDLE_T disp_hdl, TDL_DISP_DEV_INFO_T *dev_info);

OPERATE_RET tdl_disp_set_brightness(TDL_DISP_HANDLE_T disp_hdl, uint8_t brightness);

OPERATE_RET tdl_disp_dev_close(TDL_DISP_HANDLE_T disp_hdl);

TDL_DISP_FRAME_BUFF_T *tdl_disp_create_frame_buff(DISP_FB_RAM_TP_E type, uint32_t len);

void tdl_disp_free_frame_buff(TDL_DISP_FRAME_BUFF_T *frame_buff);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_DISPLAY_MANAGE_H__ */
