/**
 * @file tal_image_yuv422_to_rgb.c
 * @brief YUV422 to RGB color space conversion implementation.
 *
 * This file provides functions for converting YUV422 format images to RGB565
 * and RGB888 formats, with support for hardware acceleration via DMA2D when available.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include <string.h>
#include "tuya_cloud_types.h"
#include "tal_log.h"
#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
#include "tal_dma2d.h"
#endif

#include "tal_image_yuv422_to_rgb.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TAL_IMAGE_DMA2D_TIMEOUT_MS      2000

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
static TAL_DMA2D_HANDLE_T sg_disp_dma2d_hdl = NULL;
#endif

/***********************************************************
***********************function define**********************
***********************************************************/
#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
/**
 * @brief Hardware-accelerated conversion from YUV422 to RGB565 using DMA2D.
 *
 * @param conv_cfg Pointer to the conversion configuration structure.
 * @return OPERATE_RET Operation result code.
 */
static OPERATE_RET __dma2d_convert_yuv422_to_rgb565(TAL_IMAGE_YUV422_TO_RGB_T *conv_cfg)
{
    OPERATE_RET rt = OPRT_OK;
    TKL_DMA2D_FRAME_INFO_T in_frame = {0};
    TKL_DMA2D_FRAME_INFO_T out_frame = {0};

    if (conv_cfg == NULL || conv_cfg->in_buf == NULL || conv_cfg->out_buf == NULL) {
        return OPRT_INVALID_PARM;
    }

    memset(&in_frame, 0x00, sizeof(TKL_DMA2D_FRAME_INFO_T));
    memset(&out_frame, 0x00, sizeof(TKL_DMA2D_FRAME_INFO_T));

    if (NULL == sg_disp_dma2d_hdl) {
        TUYA_CALL_ERR_RETURN(tal_dma2d_init(&sg_disp_dma2d_hdl));
    }

    in_frame.type  = TUYA_FRAME_FMT_YUV422;
    in_frame.width  = conv_cfg->in_width;
    in_frame.height = conv_cfg->in_height;
    in_frame.pbuf   = conv_cfg->in_buf;

    out_frame.type  = TUYA_FRAME_FMT_RGB565;
    out_frame.width  = conv_cfg->out_width;
    out_frame.height = conv_cfg->out_height;
    out_frame.pbuf   = conv_cfg->out_buf;

    in_frame.width_cp  =  (conv_cfg->in_width <= conv_cfg->out_width) ? conv_cfg->in_width : conv_cfg->out_width;
    in_frame.height_cp =  (conv_cfg->in_height <= conv_cfg->out_height) ? conv_cfg->in_height : conv_cfg->out_height;
    
    TUYA_CALL_ERR_RETURN(tal_dma2d_convert(sg_disp_dma2d_hdl, &in_frame, &out_frame));

    TUYA_CALL_ERR_RETURN(tal_dma2d_wait_finish(sg_disp_dma2d_hdl, TAL_IMAGE_DMA2D_TIMEOUT_MS));

    return rt;
}

/**
 * @brief Hardware-accelerated conversion from YUV422 to RGB888 using DMA2D.
 *
 * @param conv_cfg Pointer to the conversion configuration structure.
 * @return OPERATE_RET Operation result code.
 */
static OPERATE_RET __dma2d_convert_yuv422_to_rgb888(TAL_IMAGE_YUV422_TO_RGB_T *conv_cfg)
{
    OPERATE_RET rt = OPRT_OK;
    TKL_DMA2D_FRAME_INFO_T in_frame = {0};
    TKL_DMA2D_FRAME_INFO_T out_frame = {0};

    if (conv_cfg == NULL || conv_cfg->in_buf == NULL || conv_cfg->out_buf == NULL) {
        return OPRT_INVALID_PARM;
    }

    memset(&in_frame, 0x00, sizeof(TKL_DMA2D_FRAME_INFO_T));
    memset(&out_frame, 0x00, sizeof(TKL_DMA2D_FRAME_INFO_T));

    if (NULL == sg_disp_dma2d_hdl) {
        TUYA_CALL_ERR_RETURN(tal_dma2d_init(&sg_disp_dma2d_hdl));
    }

    in_frame.type  = TUYA_FRAME_FMT_YUV422;
    in_frame.width  = conv_cfg->in_width;
    in_frame.height = conv_cfg->in_height;
    in_frame.pbuf   = conv_cfg->in_buf;

    out_frame.type  = TUYA_FRAME_FMT_RGB888;
    out_frame.width  = conv_cfg->out_width;
    out_frame.height = conv_cfg->out_height;
    out_frame.pbuf   = conv_cfg->out_buf;

    in_frame.width_cp  =  (conv_cfg->in_width <= conv_cfg->out_width) ? conv_cfg->in_width : conv_cfg->out_width;
    in_frame.height_cp =  (conv_cfg->in_height <= conv_cfg->out_height) ? conv_cfg->in_height : conv_cfg->out_height;
    
    TUYA_CALL_ERR_RETURN(tal_dma2d_convert(sg_disp_dma2d_hdl, &in_frame, &out_frame));

    TUYA_CALL_ERR_RETURN(tal_dma2d_wait_finish(sg_disp_dma2d_hdl, TAL_IMAGE_DMA2D_TIMEOUT_MS));

    return rt;
}
#endif

/**
 * @brief Software implementation: Convert YUV422 (UYVY format) to RGB565
 * @param conv_cfg Conversion configuration
 * @return OPRT_OK on success
 * 
 * YUV422 format: U0 Y0 V0 Y1 U2 Y2 V2 Y3 ...
 * Each 4 bytes represent 2 pixels
 * Conversion formula:
 *   R = Y + 1.402 * (V - 128)
 *   G = Y - 0.344 * (U - 128) - 0.714 * (V - 128)
 *   B = Y + 1.772 * (U - 128)
 */
static OPERATE_RET __sw_convert_yuv422_to_rgb565(TAL_IMAGE_YUV422_TO_RGB_T *conv_cfg)
{
    if (conv_cfg == NULL || conv_cfg->in_buf == NULL || conv_cfg->out_buf == NULL) {
        return OPRT_INVALID_PARM;
    }

    uint16_t width  = (conv_cfg->in_width <= conv_cfg->out_width) ? conv_cfg->in_width : conv_cfg->out_width;
    uint16_t height = (conv_cfg->in_height <= conv_cfg->out_height) ? conv_cfg->in_height : conv_cfg->out_height;
    uint16_t out_stride = conv_cfg->out_width * 2; /* RGB565: 2 bytes per pixel */

    const uint8_t *yuv_ptr = conv_cfg->in_buf;
    uint16_t *rgb_ptr = (uint16_t *)conv_cfg->out_buf;
    uint16_t in_stride = conv_cfg->in_width * 2; /* YUV422: 2 bytes per pixel */

    for (uint16_t y = 0; y < height; y++) {
        const uint8_t *yuv_row = yuv_ptr;
        uint16_t *rgb_row = rgb_ptr;
        
        for (uint16_t x = 0; x < width; x += 2) {
            /* Read UYVY format: U0 Y0 V0 Y1 */
            uint8_t u0 = yuv_row[0];
            uint8_t y0 = yuv_row[1];
            uint8_t v0 = yuv_row[2];
            uint8_t y1 = yuv_row[3];
            yuv_row += 4;

            /* Convert first pixel (Y0, U0, V0) */
            int32_t c0 = y0 - 16;
            int32_t d  = u0 - 128;
            int32_t e  = v0 - 128;

            int32_t r0 = (298 * c0 + 409 * e + 128) >> 8;
            int32_t g0 = (298 * c0 - 100 * d - 208 * e + 128) >> 8;
            int32_t b0 = (298 * c0 + 516 * d + 128) >> 8;

            /* Clamp to [0, 255] */
            r0 = (r0 < 0) ? 0 : (r0 > 255) ? 255 : r0;
            g0 = (g0 < 0) ? 0 : (g0 > 255) ? 255 : g0;
            b0 = (b0 < 0) ? 0 : (b0 > 255) ? 255 : b0;

            /* Convert to RGB565: RRRRR GGGGGG BBBBB */
            uint16_t rgb0 = ((r0 >> 3) << 11) | ((g0 >> 2) << 5) | (b0 >> 3);

            /* Convert second pixel (Y1, U0, V0) - reuse same U and V */
            int32_t c1 = y1 - 16;
            int32_t r1 = (298 * c1 + 409 * e + 128) >> 8;
            int32_t g1 = (298 * c1 - 100 * d - 208 * e + 128) >> 8;
            int32_t b1 = (298 * c1 + 516 * d + 128) >> 8;

            /* Clamp to [0, 255] */
            r1 = (r1 < 0) ? 0 : (r1 > 255) ? 255 : r1;
            g1 = (g1 < 0) ? 0 : (g1 > 255) ? 255 : g1;
            b1 = (b1 < 0) ? 0 : (b1 > 255) ? 255 : b1;

            /* Convert to RGB565 */
            uint16_t rgb1 = ((r1 >> 3) << 11) | ((g1 >> 2) << 5) | (b1 >> 3);

            /* Write output pixels */
            if (x < width) {
                rgb_row[x] = rgb0;
            }
            if (x + 1 < width) {
                rgb_row[x + 1] = rgb1;
            }
        }
        /* Move to next row */
        yuv_ptr += in_stride;
        rgb_ptr = (uint16_t *)((uint8_t *)rgb_ptr + out_stride);
    }

    return OPRT_OK;
}

/**
 * @brief Software implementation: Convert YUV422 (UYVY format) to RGB888
 * @param conv_cfg Conversion configuration
 * @return OPRT_OK on success
 */
static OPERATE_RET __sw_convert_yuv422_to_rgb888(TAL_IMAGE_YUV422_TO_RGB_T *conv_cfg)
{
    if (conv_cfg == NULL || conv_cfg->in_buf == NULL || conv_cfg->out_buf == NULL) {
        return OPRT_INVALID_PARM;
    }

    uint16_t width  = (conv_cfg->in_width <= conv_cfg->out_width) ? conv_cfg->in_width : conv_cfg->out_width;
    uint16_t height = (conv_cfg->in_height <= conv_cfg->out_height) ? conv_cfg->in_height : conv_cfg->out_height;
    uint16_t out_stride = conv_cfg->out_width * 3; /* RGB888: 3 bytes per pixel */

    const uint8_t *yuv_ptr = conv_cfg->in_buf;
    uint8_t *rgb_ptr = conv_cfg->out_buf;
    uint16_t in_stride = conv_cfg->in_width * 2; /* YUV422: 2 bytes per pixel */

    for (uint16_t y = 0; y < height; y++) {
        const uint8_t *yuv_row = yuv_ptr;
        uint8_t *rgb_row = rgb_ptr;
        
        for (uint16_t x = 0; x < width; x += 2) {
            /* Read UYVY format: U0 Y0 V0 Y1 */
            uint8_t u0 = yuv_row[0];
            uint8_t y0 = yuv_row[1];
            uint8_t v0 = yuv_row[2];
            uint8_t y1 = yuv_row[3];
            yuv_row += 4;

            /* Convert first pixel (Y0, U0, V0) */
            int32_t c0 = y0 - 16;
            int32_t d  = u0 - 128;
            int32_t e  = v0 - 128;

            int32_t r0 = (298 * c0 + 409 * e + 128) >> 8;
            int32_t g0 = (298 * c0 - 100 * d - 208 * e + 128) >> 8;
            int32_t b0 = (298 * c0 + 516 * d + 128) >> 8;

            /* Clamp to [0, 255] */
            r0 = (r0 < 0) ? 0 : (r0 > 255) ? 255 : r0;
            g0 = (g0 < 0) ? 0 : (g0 > 255) ? 255 : g0;
            b0 = (b0 < 0) ? 0 : (b0 > 255) ? 255 : b0;

            /* Convert second pixel (Y1, U0, V0) - reuse same U and V */
            int32_t c1 = y1 - 16;
            int32_t r1 = (298 * c1 + 409 * e + 128) >> 8;
            int32_t g1 = (298 * c1 - 100 * d - 208 * e + 128) >> 8;
            int32_t b1 = (298 * c1 + 516 * d + 128) >> 8;

            /* Clamp to [0, 255] */
            r1 = (r1 < 0) ? 0 : (r1 > 255) ? 255 : r1;
            g1 = (g1 < 0) ? 0 : (g1 > 255) ? 255 : g1;
            b1 = (b1 < 0) ? 0 : (b1 > 255) ? 255 : b1;

            /* Write output pixels (RGB888 format: R G B) */
            if (x < width) {
                rgb_row[x * 3 + 0] = (uint8_t)r0;
                rgb_row[x * 3 + 1] = (uint8_t)g0;
                rgb_row[x * 3 + 2] = (uint8_t)b0;
            }
            if (x + 1 < width) {
                rgb_row[(x + 1) * 3 + 0] = (uint8_t)r1;
                rgb_row[(x + 1) * 3 + 1] = (uint8_t)g1;
                rgb_row[(x + 1) * 3 + 2] = (uint8_t)b1;
            }
        }
        /* Move to next row */
        yuv_ptr += in_stride;
        rgb_ptr += out_stride;
    }

    return OPRT_OK;
}

/**
 * @brief Convert YUV422 to RGB565 format
 * @param conv_cfg Conversion configuration structure
 * @return OPRT_OK on success, error code otherwise
 * 
 * This function will use DMA2D hardware acceleration if available,
 * otherwise falls back to software implementation.
 */
OPERATE_RET tal_image_convert_yuv422_to_rgb565(TAL_IMAGE_YUV422_TO_RGB_T *conv_cfg)
{
    if (conv_cfg == NULL || conv_cfg->in_buf == NULL || conv_cfg->out_buf == NULL) {
        return OPRT_INVALID_PARM;
    }

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
    return __dma2d_convert_yuv422_to_rgb565(conv_cfg);
#else
    return __sw_convert_yuv422_to_rgb565(conv_cfg);
#endif

}

/**
 * @brief Convert YUV422 to RGB888 format
 * @param conv_cfg Conversion configuration structure
 * @return OPRT_OK on success, error code otherwise
 * 
 * This function will use DMA2D hardware acceleration if available,
 * otherwise falls back to software implementation.
 */
OPERATE_RET tal_image_convert_yuv422_to_rgb888(TAL_IMAGE_YUV422_TO_RGB_T *conv_cfg)
{
    if (conv_cfg == NULL || conv_cfg->in_buf == NULL || conv_cfg->out_buf == NULL) {
        return OPRT_INVALID_PARM;
    }

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
    return __dma2d_convert_yuv422_to_rgb888(conv_cfg);
#else
    return __sw_convert_yuv422_to_rgb888(conv_cfg);
#endif
}

/**
 * @brief Software implementation: Converts YUV422 format to RGB565 format.
 *
 * @param conv_cfg Pointer to the conversion configuration structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tal_image_sw_convert_yuv422_to_rgb565(TAL_IMAGE_YUV422_TO_RGB_T *conv_cfg)
{
    return __sw_convert_yuv422_to_rgb565(conv_cfg);
}

/**
 * @brief Software implementation: Converts YUV422 format to RGB888 format.
 *
 * @param conv_cfg Pointer to the conversion configuration structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tal_image_sw_convert_yuv422_to_rgb888(TAL_IMAGE_YUV422_TO_RGB_T *conv_cfg)
{
    return __sw_convert_yuv422_to_rgb888(conv_cfg);
}