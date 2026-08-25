/**
 * @file tal_image_yuv422_to_binary.c
 * @brief YUV422 to binary image conversion -- thin wrapper over tal_image_dither_core.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include <string.h>
#include "tuya_cloud_types.h"
#include "tal_memory.h"
#include "tal_log.h"
#include "tal_image_yuv422_to_binary.h"

OPERATE_RET tal_image_format_yuv422_to_binary(TAL_IMAGE_YUV422_TO_BINARY_T *conv_cfg)
{
    if (conv_cfg == NULL || conv_cfg->in_buf == NULL || conv_cfg->out_buf == NULL) {
        return OPRT_INVALID_PARM;
    }

    uint32_t gray_size = (uint32_t)conv_cfg->out_width * conv_cfg->out_height;
    uint8_t *gray_buf = (uint8_t *)Malloc(gray_size);
    if (!gray_buf) {
        return OPRT_MALLOC_FAILED;
    }

    tal_image_extract_gray_from_yuv422(conv_cfg->in_buf, conv_cfg->in_width, conv_cfg->in_height, gray_buf,
                                        conv_cfg->out_width, conv_cfg->out_height, conv_cfg->rotate);

    uint32_t scratch_size = tal_image_dither_scratch_size(conv_cfg->method, conv_cfg->out_width);
    void *scratch = NULL;
    if (scratch_size > 0) {
        scratch = Malloc(scratch_size);
        if (!scratch) {
            Free(gray_buf);
            return OPRT_MALLOC_FAILED;
        }
    }

    uint32_t out_buf_size = (uint32_t)((conv_cfg->out_width + 7) / 8) * conv_cfg->out_height;
    int rt = tal_image_dither_gray_to_binary(gray_buf, conv_cfg->out_width, conv_cfg->out_height,
                                              conv_cfg->out_buf, out_buf_size, conv_cfg->method,
                                              conv_cfg->fixed_threshold, conv_cfg->invert_colors,
                                              scratch, scratch_size);

    if (scratch) Free(scratch);
    Free(gray_buf);

    return (rt == 0) ? OPRT_OK : OPRT_COM_ERROR;
}
