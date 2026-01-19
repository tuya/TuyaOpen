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
static void __rotate90_rgb888(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height)
{
    uint32_t src_stride = src_width * 3;
    uint32_t dst_stride = src_height * 3;
    uint32_t src_index = 0, dst_index = 0;

    for(uint32_t x = 0; x < src_width; ++x) {
        for(uint32_t y = 0; y < src_height; ++y) {
            src_index = y * src_stride + x * 3;
            dst_index = (src_width - x - 1) * dst_stride + y * 3;
            dst[dst_index] = src[src_index];       /*Red*/
            dst[dst_index + 1] = src[src_index + 1]; /*Green*/
            dst[dst_index + 2] = src[src_index + 2]; /*Blue*/
        }
    }
}

static void __rotate180_rgb888(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height)
{
    uint32_t src_stride = src_width * 3;
    uint32_t dst_stride = src_width * 3;
    uint32_t src_index = 0, dst_index = 0;

    for(uint32_t y = 0; y < src_height; ++y) {
        for(uint32_t x = 0; x < src_width; ++x) {
            src_index = y * src_stride + x * 3;
            dst_index = (src_height - y - 1) * dst_stride + (src_width - x - 1) * 3;
            dst[dst_index] = src[src_index];
            dst[dst_index + 1] = src[src_index + 1];
            dst[dst_index + 2] = src[src_index + 2];
        }
    }
}

static void __rotate270_rgb888(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height)
{
    uint32_t src_stride = src_width * 3;
    uint32_t dst_stride = src_height * 3;
    uint32_t src_index = 0, dst_index = 0;

    for(uint32_t x = 0; x < src_width; ++x) {
        for(uint32_t y = 0; y < src_height; ++y) {
            src_index = y * src_stride + x * 3;
            dst_index = x * dst_stride + (src_height - y - 1) * 3;
            dst[dst_index] = src[src_index];       /*Red*/
            dst[dst_index + 1] = src[src_index + 1]; /*Green*/
            dst[dst_index + 2] = src[src_index + 2]; /*Blue*/
        }
    }
}

static void __tdl_disp_draw_sw_rotate_rgb888(TUYA_DISPLAY_ROTATION_E rot, \
                                            TDL_DISP_FRAME_BUFF_T *in_fb, \
                                            TDL_DISP_FRAME_BUFF_T *out_fb)
{
    switch(rot) {
        case TUYA_DISPLAY_ROTATION_90:
            out_fb->width  = in_fb->height;
            out_fb->height = in_fb->width;
            __rotate90_rgb888(in_fb->frame, out_fb->frame, in_fb->width, in_fb->height);
        break; 
        case TUYA_DISPLAY_ROTATION_180:
            __rotate180_rgb888(in_fb->frame, out_fb->frame, in_fb->width, in_fb->height);
        break;
        case TUYA_DISPLAY_ROTATION_270:
            out_fb->width = in_fb->height;
            out_fb->height = in_fb->width;
            __rotate270_rgb888(in_fb->frame, out_fb->frame, in_fb->width, in_fb->height);
        break;       
        default:
            break;
    }
}

static void __rotate270_rgb565(uint16_t * src, uint16_t * dst, uint32_t src_width, uint32_t src_height, bool is_swap)
{
    uint32_t src_stride = src_width;
    uint32_t dst_stride = src_height;
    uint32_t src_index = 0, dst_index = 0;

    for(uint32_t x = 0; x < src_width; ++x) {
        dst_index = x * dst_stride;
        src_index = x;
        for(uint32_t y = 0; y < src_height; ++y) {
            if(true == is_swap) {
                dst[dst_index + (src_height - y - 1)] = WORD_SWAP(src[src_index]);
            }else {
                dst[dst_index + (src_height - y - 1)] = src[src_index];
            }

            src_index += src_stride;
        }
    }
}

static void __rotate180_rgb565(uint16_t * src, uint16_t * dst, uint32_t src_width, uint32_t src_height, bool is_swap)
{
    uint32_t src_stride = src_width;
    uint32_t dst_stride = src_width;
    uint32_t src_index = 0, dst_index = 0;

    for(uint32_t y = 0; y < src_height; ++y) {
        dst_index = (src_height - y - 1) * dst_stride;
        src_index = y * src_stride;
        for(uint32_t x = 0; x < src_width; ++x) {
            if(true == is_swap) {
                dst[dst_index + src_width - x - 1] = WORD_SWAP(src[src_index + x]);
            }else {
                dst[dst_index + src_width - x - 1] = src[src_index + x];
            }
        }
    }
}

static void __rotate90_rgb565(uint16_t * src, uint16_t * dst, uint32_t src_width, uint32_t src_height, bool is_swap)
{
    uint32_t src_stride = src_width;
    uint32_t dst_stride = src_height;
    uint32_t src_index = 0, dst_index = 0;

    for(uint32_t x = 0; x < src_width; ++x) {
        dst_index = (src_width - x - 1);
        src_index = x;
        for(uint32_t y = 0; y < src_height; ++y) {
            if(true == is_swap) {
                dst[dst_index * dst_stride + y] = WORD_SWAP(src[src_index]);
            }else {
                dst[dst_index * dst_stride + y] = src[src_index];
            }
            src_index += src_stride;
        }
    }
}

static void __tdl_disp_draw_sw_rotate_rgb565(TUYA_DISPLAY_ROTATION_E rot, \
                                            TDL_DISP_FRAME_BUFF_T *in_fb, \
                                            TDL_DISP_FRAME_BUFF_T *out_fb,
                                            bool is_swap)
{
    switch(rot) {
        case TUYA_DISPLAY_ROTATION_90:
            out_fb->width  = in_fb->height;
            out_fb->height = in_fb->width;
            __rotate90_rgb565((uint16_t *)in_fb->frame, (uint16_t *)out_fb->frame, in_fb->width, in_fb->height, is_swap);
        break; 
        case TUYA_DISPLAY_ROTATION_180:
            __rotate180_rgb565((uint16_t *)in_fb->frame, (uint16_t *)out_fb->frame, in_fb->width, in_fb->height, is_swap);
        break;
        case TUYA_DISPLAY_ROTATION_270:
            out_fb->width = in_fb->height;
            out_fb->height = in_fb->width;
            __rotate270_rgb565((uint16_t *)in_fb->frame, (uint16_t *)out_fb->frame, in_fb->width, in_fb->height, is_swap);
        break;       
        default:
            break;
    }
}

static void __rotate270_monochrome(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height)
{
    uint32_t src_stride = (src_width+7)/8;
    uint32_t dst_stride = (src_height+7)/8;
    uint32_t src_index = 0, dst_index = 0;
    uint32_t src_bit_idx = 0, dst_bit_idx = 0, pixel  = 0; 

    for(uint32_t x = 0; x < src_width; ++x) {
        for(uint32_t y = 0; y < src_height; ++y) {
            src_index   = y * src_stride + x / 8;
            src_bit_idx = x % 8;
            pixel = (src[src_index] >> src_bit_idx) & 0x01;

            dst_index = (src_width -1 - x) * dst_stride + y / 8;
            dst_bit_idx = y % 8;

            if(pixel) {
                dst[dst_index] |= (1 << dst_bit_idx);
            }else {
                dst[dst_index] &= ~(1 << dst_bit_idx);
            }
        }
    }
}

static void __rotate180_monochrome(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height)
{
    uint32_t src_stride = (src_width+7)/8;
    uint32_t dst_stride = (src_width+7)/8;
    uint32_t src_index = 0, dst_index = 0;
    uint32_t src_bit_idx = 0, dst_bit_idx = 0, pixel  = 0; 

    for(uint32_t x = 0; x < src_width; ++x) {
        for(uint32_t y = 0; y < src_height; ++y) {
            src_index   = y * src_stride + x / 8;
            src_bit_idx = x % 8;
            pixel = (src[src_index] >> src_bit_idx) & 0x01;

            dst_index = (src_height -1 - y) * dst_stride + (src_width -1 -x) / 8;
            dst_bit_idx = (src_width -1 -x) % 8;

            if(pixel) {
                dst[dst_index] |= (1 << dst_bit_idx);
            }else {
                dst[dst_index] &= ~(1 << dst_bit_idx);
            }
        }
    }
}

static void __rotate90_monochrome(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height)
{
    uint32_t src_stride = (src_width+7)/8;
    uint32_t dst_stride = (src_height+7)/8;
    uint32_t src_index = 0, dst_index = 0;
    uint32_t src_bit_idx = 0, dst_bit_idx = 0, pixel  = 0; 

    for(uint32_t x = 0; x < src_width; ++x) {
        for(uint32_t y = 0; y < src_height; ++y) {
            src_index   = y * src_stride + x / 8;
            src_bit_idx = x % 8;
            pixel = (src[src_index] >> src_bit_idx) & 0x01;

            dst_index = (x) * dst_stride + (src_height -1 -y) / 8;
            dst_bit_idx = (src_height -1 -y) % 8;

            if(pixel) {
                dst[dst_index] |= (1 << dst_bit_idx);
            }else {
                dst[dst_index] &= ~(1 << dst_bit_idx);
            }
        }
    }
}

static void __tdl_disp_draw_sw_rotate_mono(TUYA_DISPLAY_ROTATION_E rot, \
                                            TDL_DISP_FRAME_BUFF_T *in_fb, \
                                            TDL_DISP_FRAME_BUFF_T *out_fb)
{
    switch(rot) {
        case TUYA_DISPLAY_ROTATION_90:
            out_fb->width  = in_fb->height;
            out_fb->height = in_fb->width;
            __rotate90_monochrome(in_fb->frame, out_fb->frame, in_fb->width, in_fb->height);
        break; 
        case TUYA_DISPLAY_ROTATION_180:
            __rotate180_monochrome(in_fb->frame, out_fb->frame, in_fb->width, in_fb->height);
        break;
        case TUYA_DISPLAY_ROTATION_270:
            out_fb->width = in_fb->height;
            out_fb->height = in_fb->width;
            __rotate270_monochrome(in_fb->frame, out_fb->frame, in_fb->width, in_fb->height);
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
        PR_NOTICE("No rotation needed");
        return OPRT_OK;
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