/**
 * @file tal_image_yuv422_to_binary.h
 * @brief YUV422 to binary image conversion interface definitions.
 *
 * This header provides function declarations and type definitions for converting
 * YUV422 format images to binary (monochrome) format using various algorithms.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_IMAGE_YUV422_TO_BINARY_H__
#define __TAL_IMAGE_YUV422_TO_BINARY_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Binary conversion method enum
 */
typedef enum {
    TAL_IMAGE_MONO_MTH_FIXED = 0,       /* Fixed threshold */
    TAL_IMAGE_MONO_MTH_ADAPTIVE,        /* Adaptive threshold */
    TAL_IMAGE_MONO_MTH_OTSU,            /* Otsu's method */
    TAL_IMAGE_MONO_MTH_BAYER8_DITHER,   /* 8-level grayscale Bayer dithering (3x3) */
    TAL_IMAGE_MONO_MTH_BAYER4_DITHER,   /* 4-level grayscale Bayer dithering (2x2) */
    TAL_IMAGE_MONO_MTH_BAYER16_DITHER,  /* 16-level grayscale Bayer dithering (4x4) */
    TAL_IMAGE_MONO_MTH_FLOYD_STEINBERG, /* Floyd-Steinberg error diffusion */
    TAL_IMAGE_MONO_MTH_STUCKI,          /* Stucki error diffusion */
    TAL_IMAGE_MONO_MTH_JARVIS,          /* Jarvis-Judice-Ninke error diffusion */
    TAL_IMAGE_MONO_MTH_COUNT            /* Total number of methods */
} TAL_IMAGE_MONO_METHOD_E;

/**
 * @brief Binary conversion configuration
 */
typedef struct {
    TAL_IMAGE_MONO_METHOD_E method;
    uint8_t                 fixed_threshold;
    uint8_t                 invert_colors;             /* 1: bit=1->white (LVGL), 0: bit=1->black (printer) */
    uint8_t                *in_buf;
    uint16_t                in_width;
    uint16_t                in_height;
    uint8_t                *out_buf;
    uint16_t                out_width;
    uint16_t                out_height;
    uint8_t                 rotate;
} TAL_IMAGE_YUV422_TO_BINARY_T;

/***********************************************************
********************function declaration********************
***********************************************************/
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
