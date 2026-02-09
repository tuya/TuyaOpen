/**
 * @file tdl_display_draw_rotate.c
 * @brief Display frame buffer rotation implementation.
 *
 * This file provides software-based rotation functions for RGB888 and RGB565
 * frame buffers, supporting 90, 180, and 270 degree rotation for Tuya display modules.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tal_image_rotate.h"
#include "tdl_display_draw.h"
/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Rotates RGB888 format frame buffer.
 *
 * @param rot Rotation angle (90, 180, 270 degrees).
 * @param in_fb Pointer to the input frame buffer structure.
 * @param out_fb Pointer to the output frame buffer structure.
 */
static void __tdl_disp_draw_sw_rotate_rgb888(TUYA_DISPLAY_ROTATION_E rot, \
                                            TDL_DISP_FRAME_BUFF_T *in_fb, \
                                            TDL_DISP_FRAME_BUFF_T *out_fb)
{
    switch(rot) {
        case TUYA_DISPLAY_ROTATION_90:
            out_fb->width  = in_fb->height;
            out_fb->height = in_fb->width;
            tal_image_rgb888_rotate90(in_fb->frame, out_fb->frame, in_fb->width, in_fb->height);
        break; 
        case TUYA_DISPLAY_ROTATION_180:
            tal_image_rgb888_rotate180(in_fb->frame, out_fb->frame, in_fb->width, in_fb->height);
        break;
        case TUYA_DISPLAY_ROTATION_270:
            out_fb->width = in_fb->height;
            out_fb->height = in_fb->width;
            tal_image_rgb888_rotate270(in_fb->frame, out_fb->frame, in_fb->width, in_fb->height);
        break;       
        default:
            break;
    }
}

/**
 * @brief Rotates RGB565 format frame buffer.
 *
 * @param rot Rotation angle (90, 180, 270 degrees).
 * @param in_fb Pointer to the input frame buffer structure.
 * @param out_fb Pointer to the output frame buffer structure.
 * @param is_swap Flag indicating whether to swap byte order for RGB565 format.
 */
static void __tdl_disp_draw_sw_rotate_rgb565(TUYA_DISPLAY_ROTATION_E rot, \
                                            TDL_DISP_FRAME_BUFF_T *in_fb, \
                                            TDL_DISP_FRAME_BUFF_T *out_fb,
                                            bool is_swap)
{
    switch(rot) {
        case TUYA_DISPLAY_ROTATION_90:
            out_fb->width  = in_fb->height;
            out_fb->height = in_fb->width;
            tal_image_rgb565_rotate90((uint16_t *)in_fb->frame, (uint16_t *)out_fb->frame, in_fb->width, in_fb->height, is_swap);
        break; 
        case TUYA_DISPLAY_ROTATION_180:
            tal_image_rgb565_rotate180((uint16_t *)in_fb->frame, (uint16_t *)out_fb->frame, in_fb->width, in_fb->height, is_swap);
        break;
        case TUYA_DISPLAY_ROTATION_270:
            out_fb->width = in_fb->height;
            out_fb->height = in_fb->width;
            tal_image_rgb565_rotate270((uint16_t *)in_fb->frame, (uint16_t *)out_fb->frame, in_fb->width, in_fb->height, is_swap);
        break;       
        default:
            break;
    }
}

/**
 * @brief Rotates monochrome format frame buffer.
 *
 * @param rot Rotation angle (90, 180, 270 degrees).
 * @param in_fb Pointer to the input frame buffer structure.
 * @param out_fb Pointer to the output frame buffer structure.
 */
static void __tdl_disp_draw_sw_rotate_mono(TUYA_DISPLAY_ROTATION_E rot, \
                                            TDL_DISP_FRAME_BUFF_T *in_fb, \
                                            TDL_DISP_FRAME_BUFF_T *out_fb)
{
    switch(rot) {
        case TUYA_DISPLAY_ROTATION_90:
            out_fb->width  = in_fb->height;
            out_fb->height = in_fb->width;
            tal_image_mono_rotate90(in_fb->frame, out_fb->frame, in_fb->width, in_fb->height);
        break; 
        case TUYA_DISPLAY_ROTATION_180:
            tal_image_mono_rotate180(in_fb->frame, out_fb->frame, in_fb->width, in_fb->height);
        break;
        case TUYA_DISPLAY_ROTATION_270:
            out_fb->width = in_fb->height;
            out_fb->height = in_fb->width;
            tal_image_mono_rotate270(in_fb->frame, out_fb->frame, in_fb->width, in_fb->height);
        break;       
        default:
            break;
    }
}

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
                                   bool is_swap)
{
    if (NULL == in_fb || NULL == out_fb ||\
        NULL == in_fb->frame || NULL == out_fb->frame) {
        return OPRT_INVALID_PARM;
    }

    if(TUYA_DISPLAY_ROTATION_0 == rot) {
        return OPRT_INVALID_PARM;
    } 

    if(in_fb->fmt != out_fb->fmt) {
        PR_ERR("Input and output frame formats do not match");
        return OPRT_INVALID_PARM;
    }

    if(in_fb->len < out_fb->len) {
        PR_NOTICE("output frame lengths is less than input frame lengths");
    }
    
    switch(in_fb->fmt) {
        case TUYA_PIXEL_FMT_RGB888:
            __tdl_disp_draw_sw_rotate_rgb888(rot, in_fb, out_fb);
        break;
        case TUYA_PIXEL_FMT_RGB565:
            __tdl_disp_draw_sw_rotate_rgb565(rot, in_fb, out_fb, is_swap);
        break;
        case TUYA_PIXEL_FMT_MONOCHROME:
            __tdl_disp_draw_sw_rotate_mono(rot, in_fb, out_fb);
        break;
        default:
            PR_ERR("Unsupported pixel format for rotation: %d", in_fb->fmt);
            return OPRT_NOT_SUPPORTED;
    }
    
    return OPRT_OK;
}

/**
 * @brief Rotates a rectangle coordinate to the specified angle.
 *
 * @param rot Rotation angle (90, 180, 270 degrees).
 * @param width Display width (used for coordinate transformation).
 * @param height Display height (used for coordinate transformation).
 * @param rect Pointer to the rectangle structure to be rotated (modified in place).
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tdl_disp_rotate_rect(TUYA_DISPLAY_ROTATION_E rot,\
                                 uint16_t width, uint16_t height,\
                                 TDL_DISP_RECT_T *rect)
{
    if (NULL == rect) {
        return OPRT_INVALID_PARM;
    }

    if (TUYA_DISPLAY_ROTATION_0 == rot) {
        return OPRT_OK;
    }

    uint16_t w = (rect->x1 >= rect->x0) ? (rect->x1 - rect->x0 + 1) : 0;
    uint16_t h = (rect->y1 >= rect->y0) ? (rect->y1 - rect->y0 + 1) : 0;
    uint16_t x0_orig = rect->x0;
    uint16_t y0_orig = rect->y0;
    uint16_t x1_orig = rect->x1;
    uint16_t y1_orig = rect->y1;

    switch(rot) {
        case TUYA_DISPLAY_ROTATION_90:
            rect->y1 = height - x0_orig - 1;
            rect->x0 = y0_orig;
            rect->x1 = rect->x0 + h - 1;
            rect->y0 = rect->y1 - w + 1;
            break;
        case TUYA_DISPLAY_ROTATION_180:
            rect->y1 = height - y0_orig - 1;
            rect->y0 = rect->y1 - h + 1;
            rect->x1 = width - x0_orig - 1;
            rect->x0 = rect->x1 - w + 1;
            break;
        case TUYA_DISPLAY_ROTATION_270:
            rect->x0 = width - y1_orig - 1;
            rect->y1 = x1_orig;
            rect->x1 = rect->x0 + h - 1;
            rect->y0 = rect->y1 - w + 1;
            break;
        default:
            return OPRT_INVALID_PARM;
    }

    return OPRT_OK;
}