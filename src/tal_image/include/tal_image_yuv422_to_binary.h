/**
 * @file tal_image_yuv422_to_binary.h
 * @brief YUV422 to binary image conversion interface definitions.
 *
 * This header provides function declarations and type definitions for converting
 * YUV422 format images to binary (monochrome) format using various algorithms:
 * fixed/adaptive/Otsu thresholding, Bayer dithering (4/8/16 levels), error
 * diffusion (Floyd-Steinberg, Stucki, Jarvis), edge-locked Atkinson dithering,
 * and gamma-corrected serpentine Floyd-Steinberg. The actual algorithms live in
 * tal_image_dither_core.h/.c, shared with the JPEG-to-bitmap path.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_IMAGE_YUV422_TO_BINARY_H__
#define __TAL_IMAGE_YUV422_TO_BINARY_H__

#include "tuya_cloud_types.h"
#include "tal_image_dither_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Binary conversion configuration
 */
typedef struct {
    TAL_IMAGE_MONO_METHOD_E method;
    uint8_t                 fixed_threshold;
    uint8_t                 invert_colors; /* 1: bit=1->white (LVGL), 0: bit=1->black (printer) */
    uint8_t                *in_buf;
    uint16_t                in_width;
    uint16_t                in_height;
    uint8_t                *out_buf;
    uint16_t                out_width;
    uint16_t                out_height;
    TAL_IMAGE_ROTATE_E      rotate; /* CCW rotation to compensate for camera mounting angle */
} TAL_IMAGE_YUV422_TO_BINARY_T;

/**
 * @brief Converts YUV422 format image to binary (monochrome) format.
 *
 * @param conv_cfg Pointer to the conversion configuration structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tal_image_format_yuv422_to_binary(TAL_IMAGE_YUV422_TO_BINARY_T *conv_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_IMAGE_YUV422_TO_BINARY_H__ */
