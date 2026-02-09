/**
 * @file tal_image_yuv422_to_rgb.h
 * @brief YUV422 to RGB color space conversion interface definitions.
 *
 * This header provides function declarations for converting YUV422 format
 * images to RGB565 and RGB888 formats, with optional hardware acceleration support.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_IMAGE_YUV422_TO_RGB_H__
#define __TAL_IMAGE_YUV422_TO_RGB_H__

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
typedef struct {
    uint8_t *in_buf;
    uint16_t in_width;
    uint16_t in_height;
    uint8_t *out_buf;
    uint16_t out_width;
    uint16_t out_height;
    uint8_t  rotate;
} TAL_IMAGE_YUV422_TO_RGB_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Converts YUV422 format to RGB565 format.
 *
 * @param conv_cfg Pointer to the conversion configuration structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tal_image_convert_yuv422_to_rgb565(TAL_IMAGE_YUV422_TO_RGB_T *conv_cfg);

/**
 * @brief Converts YUV422 format to RGB888 format.
 *
 * @param conv_cfg Pointer to the conversion configuration structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tal_image_convert_yuv422_to_rgb888(TAL_IMAGE_YUV422_TO_RGB_T *conv_cfg);

/**
 * @brief Software implementation: Converts YUV422 format to RGB565 format.
 *
 * @param conv_cfg Pointer to the conversion configuration structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tal_image_sw_convert_yuv422_to_rgb565(TAL_IMAGE_YUV422_TO_RGB_T *conv_cfg);

/**
 * @brief Software implementation: Converts YUV422 format to RGB888 format.
 *
 * @param conv_cfg Pointer to the conversion configuration structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tal_image_sw_convert_yuv422_to_rgb888(TAL_IMAGE_YUV422_TO_RGB_T *conv_cfg);


#ifdef __cplusplus
}
#endif

#endif /* __TAL_IMAGE_YUV422_TO_RGB_H__ */
