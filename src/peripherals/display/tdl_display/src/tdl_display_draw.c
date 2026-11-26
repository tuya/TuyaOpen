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
#include "tal_log.h"
#include "tkl_memory.h"

#include "tdl_display_format.h"
#include "tdl_display_draw.h"

#define TDL_DISP_PATTERN_BLOCK 64U

#if (defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)) || \
    (defined(ENABLE_PSRAM) && (ENABLE_PSRAM == 1))
#define TDL_DISP_SAFE_PSRAM_ACCESS 1
#else
#define TDL_DISP_SAFE_PSRAM_ACCESS 0
#endif

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
 * @brief Validates if a rectangle is within the frame buffer bounds.
 *
 * @param rect Pointer to the rectangle structure to validate.
 * @param fb Pointer to the frame buffer structure.
 * @return true if rectangle is valid and within bounds, false otherwise.
 */
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
        /* Rectangle is completely within the framebuffer */
        return true;
    }

    PR_ERR("Rectangle out of bounds: x0=%d, y0=%d, x1=%d, y1=%d [%d %d %d %d]", rect->x0, rect->y0, \
                                                                                rect->x1, rect->y1,\
                                                                                fb->x_start, fb->y_start,\
                                                                                x_end, y_end);
    return false;
}

static OPERATE_RET __disp_fb_write(TDL_DISP_FRAME_BUFF_T *fb, uint32_t offset,
                                   const void *src, uint32_t len)
{
    if (fb == NULL || fb->frame == NULL || src == NULL ||
        offset > fb->len || len > fb->len - offset) {
        return OPRT_INVALID_PARM;
    }
    if (len == 0U) {
        return OPRT_OK;
    }

#if TDL_DISP_SAFE_PSRAM_ACCESS
    if (fb->type == DISP_FB_TP_PSRAM) {
        return tkl_system_memcpy(fb->frame + offset, src, len) != NULL ?
               OPRT_OK : OPRT_COM_ERROR;
    }
#endif
    memcpy(fb->frame + offset, src, len);
    return OPRT_OK;
}

static OPERATE_RET __disp_fb_read(TDL_DISP_FRAME_BUFF_T *fb, uint32_t offset,
                                  void *dst, uint32_t len)
{
    if (fb == NULL || fb->frame == NULL || dst == NULL ||
        offset > fb->len || len > fb->len - offset) {
        return OPRT_INVALID_PARM;
    }
    if (len == 0U) {
        return OPRT_OK;
    }

#if TDL_DISP_SAFE_PSRAM_ACCESS
    if (fb->type == DISP_FB_TP_PSRAM) {
        return tkl_system_memcpy(dst, fb->frame + offset, len) != NULL ?
               OPRT_OK : OPRT_COM_ERROR;
    }
#endif
    memcpy(dst, fb->frame + offset, len);
    return OPRT_OK;
}

static OPERATE_RET __disp_fb_write_pattern(TDL_DISP_FRAME_BUFF_T *fb, uint32_t offset,
                                           const uint8_t *pattern, uint8_t pattern_len,
                                           uint32_t len)
{
    uint8_t block[TDL_DISP_PATTERN_BLOCK];
    uint32_t block_len = 0U;

    if (pattern == NULL || pattern_len == 0U || pattern_len > sizeof(block)) {
        return OPRT_INVALID_PARM;
    }
    if (offset > fb->len || len > fb->len - offset) {
        return OPRT_INVALID_PARM;
    }
    if (len == 0U) {
        return OPRT_OK;
    }

    block_len = (sizeof(block) / pattern_len) * pattern_len;
    for (uint32_t i = 0U; i < block_len; ++i) {
        block[i] = pattern[i % pattern_len];
    }

    while (len != 0U) {
        uint32_t chunk = len < block_len ? len : block_len;
        OPERATE_RET rt = __disp_fb_write(fb, offset, block, chunk);
        if (rt != OPRT_OK) {
            return rt;
        }
        offset += chunk;
        len -= chunk;
    }
    return OPRT_OK;
}

static bool __disp_fb_color_pattern(TDL_DISP_FRAME_BUFF_T *fb, uint32_t color,
                                    bool is_swap, uint8_t pattern[4], uint8_t *pattern_len)
{
    switch (fb->fmt) {
        case TUYA_PIXEL_FMT_RGB565: {
            uint16_t color16 = (uint16_t)(color & 0xFFFFU);
            if (is_swap) {
                color16 = WORD_SWAP(color16);
            }
            pattern[0] = (uint8_t)color16;
            pattern[1] = (uint8_t)(color16 >> 8U);
            *pattern_len = 2U;
            return true;
        }
        case TUYA_PIXEL_FMT_RGB888:
            pattern[0] = (uint8_t)color;
            pattern[1] = (uint8_t)(color >> 8U);
            pattern[2] = (uint8_t)(color >> 16U);
            *pattern_len = 3U;
            return true;
        case TUYA_PIXEL_FMT_MONOCHROME:
            pattern[0] = color ? 0xFFU : 0x00U;
            *pattern_len = 1U;
            return true;
        case TUYA_PIXEL_FMT_I2: {
            uint8_t color2 = (uint8_t)(color & 0x03U);
            pattern[0] = (uint8_t)(color2 | (color2 << 2U) | (color2 << 4U) | (color2 << 6U));
            *pattern_len = 1U;
            return true;
        }
        default:
            return false;
    }
}

static uint32_t __disp_fb_packed_bytes(uint32_t pixels, uint8_t bpp)
{
    return bpp < 8U ? (pixels * bpp + 7U) / 8U : pixels * (bpp / 8U);
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

    uint32_t pixel_index = (uint32_t)(y - fb->y_start) * fb->width +
                           (uint32_t)(x - fb->x_start);
    if (fb->fmt == TUYA_PIXEL_FMT_RGB565 || fb->fmt == TUYA_PIXEL_FMT_RGB888) {
        uint8_t pattern[4] = {0};
        uint8_t pattern_len = 0U;
        if (!__disp_fb_color_pattern(fb, color, is_swap, pattern, &pattern_len)) {
            return OPRT_NOT_SUPPORTED;
        }
        return __disp_fb_write(fb, pixel_index * pattern_len, pattern, pattern_len);
    }

    if (fb->fmt != TUYA_PIXEL_FMT_MONOCHROME && fb->fmt != TUYA_PIXEL_FMT_I2) {
        PR_ERR("Unsupported pixel format for draw point: %d", fb->fmt);
        return OPRT_NOT_SUPPORTED;
    }

    uint8_t bits_per_pixel = (fb->fmt == TUYA_PIXEL_FMT_MONOCHROME) ? 1U : 2U;
    uint32_t byte_index = (pixel_index * bits_per_pixel) / 8U;
    uint8_t bit_offset = (uint8_t)((pixel_index * bits_per_pixel) % 8U);
    uint8_t value = 0U;
    if (__disp_fb_read(fb, byte_index, &value, 1U) != OPRT_OK) {
        return OPRT_COM_ERROR;
    }

    if (fb->fmt == TUYA_PIXEL_FMT_MONOCHROME) {
        if (color) {
            value |= (uint8_t)(1U << bit_offset);
        } else {
            value &= (uint8_t)~(1U << bit_offset);
        }
    } else if (fb->fmt == TUYA_PIXEL_FMT_I2) {
        uint8_t mask = (uint8_t)(0x03U << bit_offset);
        value = (uint8_t)((value & ~mask) | ((color & 0x03U) << bit_offset));
    }
    rt = __disp_fb_write(fb, byte_index, &value, 1U);
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
    uint8_t bpp = 0;

    if(NULL == fb || NULL == fb->frame || NULL == rect ||\
       fb->width == 0 || fb->height == 0) {
        return OPRT_INVALID_PARM;
    }

    if(false == __is_rect_valid(rect, fb)) {
        return OPRT_INVALID_PARM;
    }

    width = rect->x1 - rect->x0 + 1;
    height = rect->y1 - rect->y0 + 1;

    bpp = tdl_disp_get_fmt_bpp(fb->fmt);
    if (bpp == 0U) {
        return OPRT_NOT_SUPPORTED;
    }

    if (bpp < 8U) {
        for(uint32_t j = 0; j < height ; j++) {
            for(uint32_t i = 0; i < width; i++) {
                TUYA_CALL_ERR_RETURN(tdl_disp_draw_point(fb, rect->x0+i, rect->y0+j, color, is_swap));
            }
        }
        return OPRT_OK;
    }

    uint8_t pattern[4] = {0};
    uint8_t pattern_len = 0U;
    if (!__disp_fb_color_pattern(fb, color, is_swap, pattern, &pattern_len)) {
        return OPRT_NOT_SUPPORTED;
    }
    uint32_t bytes_per_pixel = bpp / 8U;
    uint32_t dst_line_sz = fb->width * bytes_per_pixel;
    uint32_t fill_len = width * bytes_per_pixel;
    for (uint32_t j = 0U; j < height; ++j) {
        uint32_t offset = (uint32_t)(rect->y0 - fb->y_start + j) * dst_line_sz +
                          (uint32_t)(rect->x0 - fb->x_start) * bytes_per_pixel;
        TUYA_CALL_ERR_RETURN(__disp_fb_write_pattern(fb, offset, pattern, pattern_len, fill_len));
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
    if(NULL == fb || NULL == fb->frame ||\
       fb->width == 0 || fb->height == 0) {
        return OPRT_INVALID_PARM;
    }

    uint8_t bpp = tdl_disp_get_fmt_bpp(fb->fmt);
    if (bpp == 0U) {
        return OPRT_NOT_SUPPORTED;
    }

    uint8_t pattern[4] = {0};
    uint8_t pattern_len = 0U;
    if (!__disp_fb_color_pattern(fb, color, is_swap, pattern, &pattern_len)) {
        return OPRT_NOT_SUPPORTED;
    }

    uint32_t fill_len = __disp_fb_packed_bytes((uint32_t)fb->width * fb->height, bpp);
    return __disp_fb_write_pattern(fb, 0U, pattern, pattern_len, fill_len);
}

/**
 * @brief Copies pixel data from a source buffer to a rectangular area in the frame buffer.
 *
 * @param fb Pointer to the frame buffer structure.
 * @param rect Pointer to the rectangle structure specifying the destination area.
 * @param buf Pointer to the source buffer containing pixel data (tightly packed, no padding).
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tdl_disp_draw_copy(TDL_DISP_FRAME_BUFF_T *fb, TDL_DISP_RECT_T *rect, uint8_t *buf)
{
    uint32_t dst_width = 0, dst_height = 0, dst_line_sz = 0;
    uint32_t src_buf_offset = 0, src_line_sz = 0, src_line_offset = 0;
    uint8_t bpp = 0;

    if(NULL == fb || NULL == fb->frame || NULL == rect ||\
       NULL == buf || fb->width == 0 || fb->height == 0) {
        return OPRT_INVALID_PARM;
    }

    if(false == __is_rect_valid(rect, fb)) {
        return OPRT_INVALID_PARM;
    }

    bpp = tdl_disp_get_fmt_bpp(fb->fmt);
    if(bpp == 0) {
        PR_ERR("Unsupported pixel format for draw copy: %d", fb->fmt);
        return OPRT_NOT_SUPPORTED;
    }

    dst_width = rect->x1 - rect->x0 + 1;
    dst_height = rect->y1 - rect->y0 + 1;
    dst_line_sz = (dst_width * bpp + 7) / 8;  /* Source buffer line size (tightly packed) */
    src_line_sz = (fb->width * bpp + 7) / 8;  /* Destination framebuffer line size */
    
    /* Calculate source line offset in bytes */
    /* rect->x0 - fb->x_start gives pixel offset, then convert to byte offset */
    /* Note: For bit-packed formats (bpp < 8), this assumes rect->x0 is byte-aligned */
    /* For u8g2 usage, rect->x0 is always 8-pixel aligned, so this is correct */
    src_line_offset = ((rect->x0 - fb->x_start) * bpp) / 8;

    /* 
     * Note: This function assumes that:
     * 1. Source buffer (buf) has the same pixel format as destination (fb->fmt)
     * 2. Source buffer is tightly packed (no padding between rows)
     * 3. For bit-packed formats (bpp < 8), rect->x0 should be byte-aligned if possible
     *    (i.e., (rect->x0 * bpp) % 8 == 0) for correct byte-to-byte copy
     * 
     * If rect->x0 is not byte-aligned for bit-packed formats, the copy will still work
     * but may copy extra bits at the boundaries. For u8g2 usage, rect->x0 is always
     * 8-pixel aligned, so this is not an issue.
     */
    for(uint32_t j = 0; j < dst_height ; j++) {
        src_buf_offset = (rect->y0 - fb->y_start + j) * src_line_sz + src_line_offset;
        OPERATE_RET rt = __disp_fb_write(fb, src_buf_offset,
                                         &buf[j * dst_line_sz], dst_line_sz);
        if (rt != OPRT_OK) {
            return rt;
        }
    }

    return OPRT_OK;
}
