/**
 * @file tdl_pixel_color_manage.h
 * @brief TDL layer color management for LED pixel devices
 *
 * This header file provides the TDL (Tuya Device Layer) interface for LED pixel
 * color management and manipulation. It includes functions for setting pixel colors
 * (single and multiple), color shifting operations, pixel color copying, and
 * specialized color effects. The module provides high-level abstractions for
 * managing LED strip color operations across different pixel controller types.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_PIXEL_COLOR_MANAGE_H__
#define __TDL_PIXEL_COLOR_MANAGE_H__

#include "tdl_pixel_dev_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
******************************macro define****************************
*********************************************************************/

/*********************************************************************
****************************typedef define****************************
*********************************************************************/
typedef unsigned int PIXEL_SHIFT_DIR_T;
#define PIXEL_SHIFT_RIGHT 0 // index min->max
#define PIXEL_SHIFT_LEFT  1 // index max->min

typedef unsigned int PIXEL_M_SHIFT_DIR_T;
#define PIXEL_SHIFT_CLOSE 0 // Towards each other
#define PIXEL_SHIFT_FAR   1 // Away from each other

/*********************************************************************
****************************function define***************************
*********************************************************************/
/**
 * @brief    Set the color of a pixel segment (single)
 *
 * @param[in]    handle           Device handle
 * @param[in]    index_start      Start index of the pixel
 * @param[in]    pixel_num        Length of the pixel segment
 * @param[in]    color            Target color
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_set_single_color(PIXEL_HANDLE_T handle, uint32_t index_start, uint32_t pixel_num, PIXEL_COLOR_T *color);

/**
 * @brief        Set the color of a pixel segment (multiple)
 *
 * @param[in]    handle           Device handle
 * @param[in]    index_start      Start index of the pixel
 * @param[in]    pixel_num        Length of the pixel segment
 * @param[in]    color_arr        Target color group
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_set_multi_color(PIXEL_HANDLE_T handle, uint32_t index_start, uint32_t pixel_num,
                              PIXEL_COLOR_T *color_arr);

/**
 * @brief       Set the color of a pixel segment on a background color
 *
 * @param[in]     handle           Device handle
 * @param[in]    index_start      Start index of the pixel
 * @param[in]    pixel_num        Length of the pixel segment
 * @param[in]    backcolor        Background color
 * @param[in]    color            Target color
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_set_single_color_with_backcolor(PIXEL_HANDLE_T handle, uint32_t index_start, uint32_t pixel_num,
                                              PIXEL_COLOR_T *backcolor, PIXEL_COLOR_T *color);

/**
 * @brief       Cyclically shift pixel colors
 *
 * @param[in]    handle           Device handle
 * @param[in]    dir              Direction of movement
 * @param[in]    index_start      Start index
 * @param[in]    end_start        End index
 * @param[in]    move_step        Movement step
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_cycle_shift_color(PIXEL_HANDLE_T handle, PIXEL_SHIFT_DIR_T dir, uint32_t index_start, uint32_t index_end,
                                uint32_t move_step);

/**
 * @brief        Mirror and cyclically shift pixel colors
 *
 * @param[in]    handle           Device handle
 * @param[in]    dir              Direction of movement
 * @param[in]    index_start      Start index
 * @param[in]    end_start        End index
 * @param[in]    move_step        Movement step
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_mirror_cycle_shift_color(PIXEL_HANDLE_T handle, PIXEL_M_SHIFT_DIR_T dir, uint32_t index_start,
                                       uint32_t index_end, uint32_t move_step);

/**
 * @brief        Get pixel color
 *
 * @param[in]    handle           Device handle
 * @param[in]    index            Index
 * @param[out]   color            Color
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_get_color(PIXEL_HANDLE_T handle, uint32_t index, PIXEL_COLOR_T *color);

/**
 * @brief        Copy pixel color
 *
 * @param[in]    handle           Device handle
 * @param[in]    dst_idx          Destination index
 * @param[in]    src_idx          Source index
 * @param[in]    len              Number of pixels to copy
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_copy_color(PIXEL_HANDLE_T handle, uint32_t dst_idx, uint32_t src_idx, uint32_t len);

/**
 * @brief    Set all pixels to a single color
 *
 * @param[in]    handle           Device handle
 * @param[in]    color            Target color
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_set_single_color_all(PIXEL_HANDLE_T handle, PIXEL_COLOR_T *color);

/**
 * @brief    Set only the white color, not the colored light
 *
 * @param[in]    handle           Device handle
 * @param[in]    color            Target color
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
int tdl_pixel_set_single_white_all(PIXEL_HANDLE_T handle, PIXEL_COLOR_T *color);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*__TDL_PIXEL_COLOR_MANAGE_H__*/
