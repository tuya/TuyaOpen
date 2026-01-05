/**
 * @file tdl_display_draw.c
 * @brief Display frame buffer drawing implementation.
 *
 * This file provides functions for drawing points, filling rectangles, and
 * filling entire frame buffers for Tuya display modules, supporting multiple pixel formats.
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
static void __disp_mono_write_point(uint32_t x, uint32_t y, bool enable, TDL_DISP_FRAME_BUFF_T *fb)
{
    uint32_t write_byte_index = y * (fb->width/8) + x/8;
    uint8_t write_bit = x%8;

    if (enable) {
        fb->frame[write_byte_index] |= (1 << write_bit);
    } else {
        fb->frame[write_byte_index] &= ~(1 << write_bit);
    }
}

static void __disp_i2_write_point(uint32_t x, uint32_t y, uint8_t color, TDL_DISP_FRAME_BUFF_T *fb)
{
    uint32_t write_byte_index = y * (fb->width/4) + x/4;
    uint8_t write_bit = (x%4)*2;
    uint8_t cleared = fb->frame[write_byte_index] & (~(0x03 << write_bit)); // Clear the bits we are going to write

    fb->frame[write_byte_index] = cleared | ((color & 0x03) << write_bit);
}

static void __disp_rgb565_write_point(uint32_t x, uint32_t y, uint16_t color, TDL_DISP_FRAME_BUFF_T *fb, bool is_swap)
{
    uint32_t write_byte_index = (y * fb->width + x);
    uint16_t *p_buf16 = (uint16_t *)fb->frame;

    if(is_swap) {
        p_buf16[write_byte_index] = WORD_SWAP(color);
    }else {
        p_buf16[write_byte_index] = color;
    }
}

static void __disp_rgb888_write_point(uint32_t x, uint32_t y, uint32_t color, TDL_DISP_FRAME_BUFF_T *fb)
{
    uint32_t write_byte_index = (y * fb->width + x)*3;

    fb->frame[write_byte_index]     =  color & 0xFF; // B
    fb->frame[write_byte_index + 1] = (color >> 8) & 0xFF; // G
    fb->frame[write_byte_index + 2] = (color >> 16) & 0xFF; // R
}

static bool __is_rect_valid(TDL_DISP_RECT_T *rect, TDL_DISP_FRAME_BUFF_T *fb)
{
    uint16_t x_end = 0, y_end = 0;

    if(NULL == rect || NULL == fb) {
        return false;
    }

    if(rect->x0 > rect->x1 || rect->y0 > rect->y1) {
        PR_ERR("Invalid rectangle: x0=%d, y0=%d, x1=%d, y1=%d", rect->x0, rect->y0, rect->x1, rect->y1);
        return false;
    }

    x_end = fb->x_start + fb->width - 1;
    y_end = fb->y_start + fb->height - 1;

    if(rect->x0 >= fb->x_start &&  rect->x1 <= x_end &&\
       rect->y0 >= fb->y_start && rect->y1 <= y_end) {
        // Rectangle is completely within the framebuffer
        return true;
    }

    PR_ERR("Rectangle out of bounds: x0=%d, y0=%d, x1=%d, y1=%d [%d %d %d %d]", rect->x0, rect->y0, \
                                                                                rect->x1, rect->y1,\
                                                                                fb->x_start, fb->y_start,\
                                                                                x_end, y_end);
    return false;
}

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
OPERATE_RET tdl_disp_draw_point(TDL_DISP_FRAME_BUFF_T *fb, uint16_t x, uint16_t y, uint32_t color, bool is_swap)
{
    OPERATE_RET rt = OPRT_OK;

    if(NULL == fb || NULL == fb->frame ||\
       fb->width == 0 || fb->height == 0) {
        return OPRT_INVALID_PARM;
    }

    if(x >= (fb->x_start + fb->width) || x < fb->x_start) {
        PR_ERR("x:%d out of range [%d %d]", x, fb->x_start, fb->x_start + fb->width-1);
        return OPRT_INVALID_PARM;
    }

    if(y >= (fb->y_start + fb->height) || y < fb->y_start) {
        PR_ERR("y:%d out of range [%d %d]", y, fb->y_start, fb->y_start + fb->height-1);
        return OPRT_INVALID_PARM;
    }

    switch(fb->fmt) {
        case TUYA_PIXEL_FMT_RGB565: {
            uint16_t color_16 = (uint16_t)(color & 0xFFFF);
            __disp_rgb565_write_point(x - fb->x_start, y - fb->y_start, color_16, fb, is_swap);
        }
        break;
        case TUYA_PIXEL_FMT_RGB888: {
            __disp_rgb888_write_point(x - fb->x_start, y - fb->y_start, color, fb);
        }
        break;
        case TUYA_PIXEL_FMT_MONOCHROME: {
            bool enable = (color) ? false : true;
            __disp_mono_write_point(x - fb->x_start, y - fb->y_start, enable, fb);
        }
        break;
        case TUYA_PIXEL_FMT_I2: {
            uint8_t color_2 = (uint8_t)(color & 0x03);
            __disp_i2_write_point(x - fb->x_start, y - fb->y_start, color_2, fb);
        }
        break;
        default:
            PR_ERR("Unsupported pixel format for draw point: %d", fb->fmt);
            return OPRT_NOT_SUPPORTED;
    }

    return rt;
}

/**
 * @brief Fills a rectangular area in the display frame buffer with a specified color.
 *
 * @param fb Pointer to the frame buffer structure.
 * @param rect Pointer to the rectangle structure specifying the area to fill.
 * @param color Color value to fill.
 * @param is_swap Whether to swap byte order for RGB565 format.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tdl_disp_draw_fill(TDL_DISP_FRAME_BUFF_T *fb, TDL_DISP_RECT_T *rect, uint32_t color, bool is_swap)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t width = 0, height = 0;

    if(NULL == fb || NULL == fb->frame || NULL == rect ||\
       fb->width == 0 || fb->height == 0) {
        return OPRT_INVALID_PARM;
    }

    if(false == __is_rect_valid(rect, fb)) {
        return OPRT_INVALID_PARM;
    }

    width = rect->x1 - rect->x0 + 1;
    height = rect->y1 - rect->y0 + 1;

    for(uint32_t j = 0; j < height ; j++) {
        for(uint32_t i = 0; i < width; i++) {
            TUYA_CALL_ERR_RETURN(tdl_disp_draw_point(fb, rect->x0+i, rect->y0+j, color, is_swap));
        }
    }

    return rt;
}

/**
 * @brief Fills the entire display frame buffer with a specified color.
 *
 * @param fb Pointer to the frame buffer structure.
 * @param color Color value to fill.
 * @param is_swap Whether to swap byte order for RGB565 format.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tdl_disp_draw_fill_full(TDL_DISP_FRAME_BUFF_T *fb, uint32_t color, bool is_swap)
{
    OPERATE_RET rt = OPRT_OK;

    if(NULL == fb || NULL == fb->frame ||\
       fb->width == 0 || fb->height == 0) {
        return OPRT_INVALID_PARM;
    }

    for(uint32_t j = 0; j < fb->height ; j++) {
        for(uint32_t i = 0; i < fb->width; i++) {
            TUYA_CALL_ERR_RETURN(tdl_disp_draw_point(fb, fb->x_start+i, fb->y_start+j, color, is_swap));
        }
    }

    return rt;
}