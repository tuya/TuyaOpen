/**
 * @file tal_image_frame_to_rgb.c
 * @brief Camera frame to RGB565 conversion, dispatched by frame format.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include <string.h>

#include "tuya_cloud_types.h"

#include "tal_image_frame_to_rgb.h"
#include "tal_image_yuv422_to_rgb.h"

#if defined(ENABLE_NV12_TO_RGB565) && (ENABLE_NV12_TO_RGB565 == 1)
#include "tkl_image_codec.h"
#define TAL_IMAGE_NV12_SUPPORTED 1
#else
#define TAL_IMAGE_NV12_SUPPORTED 0
#endif

#define TAL_IMAGE_RGB565_ROW_ALIGN 16U

#if TAL_IMAGE_NV12_SUPPORTED
static uint32_t __tal_image_rgb565_stored_height(uint16_t height)
{
    return ((uint32_t)height + TAL_IMAGE_RGB565_ROW_ALIGN - 1U) / TAL_IMAGE_RGB565_ROW_ALIGN *
           TAL_IMAGE_RGB565_ROW_ALIGN;
}

static OPERATE_RET __tal_image_nv12_to_rgb565(const uint8_t *in_buf, uint32_t in_len, uint16_t width,
                                              uint16_t height, uint8_t *out_buf, uint32_t out_len)
{
    TUYA_IMAGE_CODEC_BUF_T in;
    TUYA_IMAGE_CODEC_BUF_T out;
    uint32_t               nv12_len;
    uint32_t               stored_height;

    nv12_len      = (uint32_t)width * height * 3U / 2U;
    stored_height = __tal_image_rgb565_stored_height(height);

    if ((0U != in_len) && (in_len < nv12_len)) {
        return OPRT_INVALID_PARM;
    }

    if (out_len < (uint32_t)width * stored_height * 2U) {
        return OPRT_INVALID_PARM;
    }

    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));

    in.buffer = (uint8_t *)in_buf;
    in.length = nv12_len;
    in.format = TUYA_FRAME_FMT_NV12;
    in.width  = width;
    in.height = height;

    out.buffer = out_buf;
    out.length = (uint32_t)width * stored_height * 2U;
    out.format = TUYA_FRAME_FMT_RGB565;
    out.width  = width;
    out.height = height;

    return tkl_image_codec_convert(&in, &out);
}
#endif

uint32_t tal_image_frame_to_rgb565_size(TUYA_FRAME_FMT_E fmt, uint16_t width, uint16_t height)
{
    (void)fmt;

    if ((0U == width) || (0U == height)) {
        return 0U;
    }

#if TAL_IMAGE_NV12_SUPPORTED
    if (TUYA_FRAME_FMT_NV12 == fmt) {
        return (uint32_t)width * __tal_image_rgb565_stored_height(height) * 2U;
    }
#endif

    return (uint32_t)width * height * 2U;
}

OPERATE_RET tal_image_frame_to_rgb565(TUYA_FRAME_FMT_E fmt, const uint8_t *in_buf, uint32_t in_len,
                                      uint16_t width, uint16_t height, uint8_t *out_buf, uint32_t out_len)
{
    TAL_IMAGE_YUV422_TO_RGB_T conv;

    (void)fmt;

    if ((NULL == in_buf) || (NULL == out_buf) || (0U == width) || (0U == height) || (0U == out_len)) {
        return OPRT_INVALID_PARM;
    }

#if TAL_IMAGE_NV12_SUPPORTED
    if (TUYA_FRAME_FMT_NV12 == fmt) {
        return __tal_image_nv12_to_rgb565(in_buf, in_len, width, height, out_buf, out_len);
    }
#endif

    memset(&conv, 0, sizeof(conv));

    conv.in_buf     = (uint8_t *)in_buf;
    conv.in_width   = width;
    conv.in_height  = height;
    conv.out_buf    = out_buf;
    conv.out_width  = width;
    conv.out_height = height;

    return tal_image_convert_yuv422_to_rgb565(&conv);
}
