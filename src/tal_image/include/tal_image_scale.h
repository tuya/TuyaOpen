/**
 * @file tal_image_scale.h
 * @brief Image scaling interface definitions.
 *
 * This header provides function declarations for scaling images in various
 * pixel formats (RGB888, RGB565, YUV422, grayscale) using configurable
 * interpolation methods (nearest neighbor, bilinear, bicubic).
 * It also provides decode-and-scale operations for JPEG streams
 * that produce raw pixel output at a target resolution in one pass.
 *
 * Output buffers are allocated internally by the scale functions.
 * The caller must release them with tal_image_scale_buf_free() after use.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_IMAGE_SCALE_H__
#define __TAL_IMAGE_SCALE_H__

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
 * @brief Interpolation method used during scaling.
 */
typedef enum {
    TAL_IMAGE_SCALE_MTH_NEAREST = 0,    /**< Nearest neighbor: fastest, lowest quality */
    TAL_IMAGE_SCALE_MTH_BILINEAR,       /**< Bilinear interpolation: balanced speed and quality */
    TAL_IMAGE_SCALE_MTH_BICUBIC,        /**< Bicubic interpolation: highest quality, most CPU intensive */
    TAL_IMAGE_SCALE_MTH_COUNT           /**< Total number of methods, not a valid selection */
} TAL_IMAGE_SCALE_METHOD_E;

/**
 * @brief Scaling mode: fixed output size or proportional ratio.
 */
typedef enum {
    TAL_IMAGE_SCALE_MODE_SIZE = 0,  /**< Scale to an explicit out_width x out_height */
    TAL_IMAGE_SCALE_MODE_RATIO,     /**< Scale by ratio_x / ratio_y (e.g. 0.5 = half) */
} TAL_IMAGE_SCALE_MODE_E;

/**
 * @brief Input parameters for raw pixel image scaling operations.
 *
 * Set mode to select between fixed-size and ratio-based scaling.
 * The pixel format is determined by which scale function is called.
 */
typedef struct {
    TAL_IMAGE_SCALE_METHOD_E  method;     /**< Interpolation method */
    TAL_IMAGE_SCALE_MODE_E    mode;       /**< Scaling mode: fixed size or ratio */
    const uint8_t            *buf;        /**< Pointer to source pixel buffer */
    uint16_t                  width;      /**< Source image width in pixels */
    uint16_t                  height;     /**< Source image height in pixels */
    union {
        struct {
            uint16_t out_width;  /**< Target width in pixels (mode == SIZE) */
            uint16_t out_height; /**< Target height in pixels (mode == SIZE) */
        };
        struct {
            uint32_t ratio_x;    /**< Horizontal scale percentage (mode == RATIO, e.g. 50 = 50%, 200 = 200%) */
            uint32_t ratio_y;    /**< Vertical scale percentage   (mode == RATIO, e.g. 50 = 50%, 200 = 200%) */
        };
    };
} TAL_IMAGE_SCALE_IN_T;

/**
 * @brief Input parameters for JPEG decode-and-scale operations.
 *
 * Source image dimensions are parsed from the JPEG header.
 * Set mode to select between fixed-size and ratio-based scaling.
 * The output pixel format is determined by which scale function is called.
 */
typedef struct {
    TAL_IMAGE_SCALE_METHOD_E  method;  /**< Interpolation method */
    TAL_IMAGE_SCALE_MODE_E    mode;    /**< Scaling mode: fixed size or ratio */
    const uint8_t            *data;    /**< Pointer to compressed JPEG stream */
    uint32_t                  size;    /**< Size of data in bytes */
    union {
        struct {
            uint16_t out_width;  /**< Target width in pixels (mode == SIZE) */
            uint16_t out_height; /**< Target height in pixels (mode == SIZE) */
        };
        struct {
            uint32_t ratio_x;    /**< Horizontal scale percentage (mode == RATIO, e.g. 50 = 50%, 200 = 200%) */
            uint32_t ratio_y;    /**< Vertical scale percentage   (mode == RATIO, e.g. 50 = 50%, 200 = 200%) */
        };
    };
} TAL_IMAGE_JPEG_SCALE_IN_T;

/**
 * @brief Output produced by all scale functions.
 *
 * buf is allocated internally and must be released with tal_image_scale_buf_free().
 * width and height reflect the actual dimensions of the scaled image.
 */
typedef struct {
    uint8_t  *buf;    /**< Internally allocated output pixel buffer */
    uint32_t  size;   /**< Size of buf in bytes */
    uint16_t  width;  /**< Actual width of the scaled image in pixels */
    uint16_t  height; /**< Actual height of the scaled image in pixels */
} TAL_IMAGE_SCALE_OUT_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Frees an output buffer allocated by any scale function.
 *
 * Passing NULL is safe and has no effect.
 *
 * @param[in] out  Pointer to the output structure whose buf field is to be freed.
 */
void tal_image_scale_buf_free(TAL_IMAGE_SCALE_OUT_T *out);

/**
 * @brief Scales an RGB888 image to a new resolution.
 *
 * @param[in]  in   Source image parameters.
 * @param[out] out  Allocated output buffer and its size.
 * @return OPERATE_RET  OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tal_image_rgb888_scale(const TAL_IMAGE_SCALE_IN_T *in, TAL_IMAGE_SCALE_OUT_T *out);

/**
 * @brief Scales an RGB565 image to a new resolution.
 *
 * @param[in]  in   Source image parameters.
 * @param[out] out  Allocated output buffer and its size.
 * @return OPERATE_RET  OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tal_image_rgb565_scale(const TAL_IMAGE_SCALE_IN_T *in, TAL_IMAGE_SCALE_OUT_T *out);

/**
 * @brief Scales a packed YUV422 image to a new resolution.
 *
 * @param[in]  in   Source image parameters.
 * @param[out] out  Allocated output buffer and its size.
 * @return OPERATE_RET  OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tal_image_yuv422_scale(const TAL_IMAGE_SCALE_IN_T *in, TAL_IMAGE_SCALE_OUT_T *out);

/**
 * @brief Scales an 8-bit grayscale image to a new resolution.
 *
 * @param[in]  in   Source image parameters.
 * @param[out] out  Allocated output buffer and its size.
 * @return OPERATE_RET  OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tal_image_gray_scale(const TAL_IMAGE_SCALE_IN_T *in, TAL_IMAGE_SCALE_OUT_T *out);

/**
 * @brief Decodes a JPEG stream and scales it to RGB565 output.
 *
 * @param[in]  in   JPEG stream and target dimensions.
 * @param[out] out  Allocated output buffer and its size.
 * @return OPERATE_RET  OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tal_image_jpeg_scale_rgb565(const TAL_IMAGE_JPEG_SCALE_IN_T *in, TAL_IMAGE_SCALE_OUT_T *out);

/**
 * @brief Decodes a JPEG stream and scales it to RGB888 output.
 *
 * @param[in]  in   JPEG stream and target dimensions.
 * @param[out] out  Allocated output buffer and its size.
 * @return OPERATE_RET  OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tal_image_jpeg_scale_rgb888(const TAL_IMAGE_JPEG_SCALE_IN_T *in, TAL_IMAGE_SCALE_OUT_T *out);

/**
 * @brief Decodes a JPEG stream and scales it to 8-bit grayscale output.
 *
 * @param[in]  in   JPEG stream and target dimensions.
 * @param[out] out  Allocated output buffer and its size.
 * @return OPERATE_RET  OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tal_image_jpeg_scale_gray(const TAL_IMAGE_JPEG_SCALE_IN_T *in, TAL_IMAGE_SCALE_OUT_T *out);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_IMAGE_SCALE_H__ */
