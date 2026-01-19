/**
 * @file tdl_display_fb_manage.h
 * @brief Frame buffer management header for display system
 *
 * This header file defines the interface for frame buffer pool management,
 * including initialization, allocation, and release of frame buffers for
 * display operations. It provides thread-safe frame buffer management with
 * support for multiple pixel formats.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_DISPLAY_FB_MANAGE_H__
#define __TDL_DISPLAY_FB_MANAGE_H__

#include "tuya_cloud_types.h"
#include "tdl_display_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define TDL_DISP_FB_MANAGE_MAX_NUM 6

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void* TDL_FB_MANAGE_HANDLE_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initialize frame buffer manager
 * @param handle Pointer to store the frame buffer manager handle
 * @return OPRT_OK on success, OPRT_MALLOC_FAILED on memory allocation failure
 */
OPERATE_RET tdl_disp_fb_manage_init(TDL_FB_MANAGE_HANDLE_T *handle);

/**
 * @brief Add a frame buffer to the manager
 * @param handle Frame buffer manager handle
 * @param fmt Pixel format of the frame buffer
 * @param width Width of the frame buffer in pixels
 * @param height Height of the frame buffer in pixels
 * @return OPRT_OK on success, OPRT_INVALID_PARM if max number exceeded or invalid parameters,
 *         OPRT_MALLOC_FAILED on memory allocation failure
 */
OPERATE_RET tdl_disp_fb_manage_add(TDL_FB_MANAGE_HANDLE_T handle, TUYA_DISPLAY_PIXEL_FMT_E fmt,\
                                   uint16_t width, uint16_t height);

/**
 * @brief Get a free frame buffer from the manager
 * @param handle Frame buffer manager handle
 * @return Pointer to an available frame buffer, or NULL on error or if no buffer
 *         becomes available after waiting
 */
TDL_DISP_FRAME_BUFF_T *tdl_disp_get_free_fb(TDL_FB_MANAGE_HANDLE_T handle);

/**
 * @brief Release frame buffer manager and free all resources
 * @param handle Pointer to the frame buffer manager handle (will be set to NULL)
 * @return OPRT_OK on success, OPRT_MALLOC_FAILED if handle is invalid,
 *         or timeout error if frame buffers are not released in time
 */
OPERATE_RET tdl_disp_fb_manage_release(TDL_FB_MANAGE_HANDLE_T *handle);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_DISPLAY_FB_MANAGE_H__ */
