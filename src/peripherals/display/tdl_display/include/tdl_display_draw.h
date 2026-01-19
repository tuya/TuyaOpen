/**
 * @file tdl_display_draw.h
 * @brief Display frame buffer drawing interface definitions.
 *
 * This header provides function declarations and macros for drawing operations
 * on display frame buffers in Tuya display modules.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_DISPLAY_DRAW_H__
#define __TDL_DISPLAY_DRAW_H__

#include "tuya_cloud_types.h"
#include "tdl_display_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef struct {
    uint16_t x0;
    uint16_t y0;
    uint16_t x1;
    uint16_t y1;
} TDL_DISP_RECT_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Draws a point on the display frame buffer.
 *
 * @param fb Pointer to the frame buffer structure.
 * @param x X coordinate of the point.
 * @param y Y coordinate of the point.
 * @param color Color value of the point.
 * @param is_swap Whether to swap byte order for RGB565 format.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tdl_disp_draw_point(TDL_DISP_FRAME_BUFF_T *fb, uint16_t x, uint16_t y, uint32_t color, bool is_swap);

/**
 * @brief Fills a rectangular area in the display frame buffer with a specified color.
 *
 * @param fb Pointer to the frame buffer structure.
 * @param rect Pointer to the rectangle structure specifying the area to fill.
 * @param color Color value to fill.
 * @param is_swap Whether to swap byte order for RGB565 format.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tdl_disp_draw_fill(TDL_DISP_FRAME_BUFF_T *fb, TDL_DISP_RECT_T *rect, uint32_t color, bool is_swap);

/**
 * @brief Fills the entire display frame buffer with a specified color.
 *
 * @param fb Pointer to the frame buffer structure.
 * @param color Color value to fill.
 * @param is_swap Whether to swap byte order for RGB565 format.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tdl_disp_draw_fill_full(TDL_DISP_FRAME_BUFF_T *fb, uint32_t color, bool is_swap);

/**
 * @brief Rotates a display frame buffer to the specified angle.
 *
 * @param rot Rotation angle (90, 180, 270 degrees).
 * @param in_fb Pointer to the input frame buffer structure.
 * @param out_fb Pointer to the output frame buffer structure.
 * @param is_swap Flag indicating whether to swap the frame buffers(rgb565).
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tdl_disp_draw_rotate(TUYA_DISPLAY_ROTATION_E rot, \
                                   TDL_DISP_FRAME_BUFF_T *in_fb, \
                                   TDL_DISP_FRAME_BUFF_T *out_fb,\
                                   bool is_swap);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_DISPLAY_DRAW_H__ */
