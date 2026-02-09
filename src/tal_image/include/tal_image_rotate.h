/**
 * @file tal_image_rotate.h
 * @brief Image rotation interface definitions.
 *
 * This header provides function declarations for rotating images in various
 * pixel formats (RGB888, RGB565, monochrome) by 90, 180, and 270 degrees.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_IMAGE_ROTATE_H__
#define __TAL_IMAGE_ROTATE_H__

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


/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Rotates RGB888 image 90 degrees clockwise.
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 */
void tal_image_rgb888_rotate90(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height);

/**
 * @brief Rotates RGB888 image 180 degrees.
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 */
void tal_image_rgb888_rotate180(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height);

/**
 * @brief Rotates RGB888 image 270 degrees clockwise (or 90 degrees counter-clockwise).
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 */
void tal_image_rgb888_rotate270(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height);

/**
 * @brief Rotates RGB565 image 90 degrees clockwise.
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @param is_swap Whether to swap byte order during rotation.
 */
void tal_image_rgb565_rotate90(uint16_t * src, uint16_t * dst, uint32_t src_width, uint32_t src_height, bool is_swap);

/**
 * @brief Rotates RGB565 image 180 degrees.
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @param is_swap Whether to swap byte order during rotation.
 */
void tal_image_rgb565_rotate180(uint16_t * src, uint16_t * dst, uint32_t src_width, uint32_t src_height, bool is_swap);

/**
 * @brief Rotates RGB565 image 270 degrees clockwise (or 90 degrees counter-clockwise).
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @param is_swap Whether to swap byte order during rotation.
 */
void tal_image_rgb565_rotate270(uint16_t * src, uint16_t * dst, uint32_t src_width, uint32_t src_height, bool is_swap);

/**
 * @brief Rotates monochrome image 90 degrees clockwise.
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 */
void tal_image_mono_rotate90(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height);

/**
 * @brief Rotates monochrome image 180 degrees.
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 */
void tal_image_mono_rotate180(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height);

/**
 * @brief Rotates monochrome image 270 degrees clockwise (or 90 degrees counter-clockwise).
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 */
void tal_image_mono_rotate270(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_IMAGE_ROTATE_H__ */
