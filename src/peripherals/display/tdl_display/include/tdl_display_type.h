/**
 * @file tdl_display_type.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDL_DISPLAY_TYPE_H__
#define __TDL_DISPLAY_TYPE_H__

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
typedef enum  {
    TUYA_DISPLAY_RGB = 0,
    TUYA_DISPLAY_8080,
    TUYA_DISPLAY_QSPI,
    TUYA_DISPLAY_SPI,
    TUYA_DISPLAY_I2C,
}TUYA_DISPLAY_TYPE_E;

typedef void*  TDL_DISP_HANDLE_T;

typedef enum {
    DISP_FB_TP_SRAM = 0,
    DISP_FB_TP_PSRAM,
} DISP_FB_RAM_TP_E;

typedef struct TDL_DISP_FRAME_BUFF_T TDL_DISP_FRAME_BUFF_T;


typedef void (*FRAME_BUFF_FREE_CB)(TDL_DISP_FRAME_BUFF_T *frame_buff);

struct TDL_DISP_FRAME_BUFF_T {
    DISP_FB_RAM_TP_E type;
    TUYA_DISPLAY_PIXEL_FMT_E fmt;
    uint16_t x_start;
    uint16_t y_start;
    uint16_t width;
    uint16_t height;
    FRAME_BUFF_FREE_CB free_cb;
    uint32_t len;
    uint8_t *frame;
    void *sys_param;    //reserved for system use, user do not use
};

typedef struct {
    TUYA_DISPLAY_TYPE_E      type;
    TUYA_DISPLAY_ROTATION_E  rotation;
    uint16_t                 width;
    uint16_t                 height;
    TUYA_DISPLAY_PIXEL_FMT_E fmt;
    bool                     is_swap;
    bool                     has_vram;
} TDL_DISP_DEV_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/


#ifdef __cplusplus
}
#endif

#endif /* __TDL_DISPLAY_TYPE_H__ */
