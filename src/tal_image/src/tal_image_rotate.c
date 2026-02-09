/**
 * @file tal_image_rotate.c
 * @brief Image rotation implementation for various pixel formats.
 *
 * This file provides software-based rotation functions for RGB888, RGB565,
 * and monochrome image formats, supporting 90, 180, and 270 degree rotation.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tal_image_rotate.h"

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
 * @brief Rotates RGB888 image 90 degrees clockwise.
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 */
void tal_image_rgb888_rotate90(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height)
{
    uint32_t src_stride = src_width * 3;
    uint32_t dst_stride = src_height * 3;
    uint32_t src_index = 0, dst_index = 0;

    for(uint32_t x = 0; x < src_width; ++x) {
        for(uint32_t y = 0; y < src_height; ++y) {
            src_index = y * src_stride + x * 3;
            dst_index = (src_width - x - 1) * dst_stride + y * 3;
            dst[dst_index] = src[src_index];       /* Red */
            dst[dst_index + 1] = src[src_index + 1]; /* Green */
            dst[dst_index + 2] = src[src_index + 2]; /* Blue */
        }
    }
}

/**
 * @brief Rotates RGB888 image 180 degrees.
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 */
void tal_image_rgb888_rotate180(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height)
{
    uint32_t src_stride = src_width * 3;
    uint32_t dst_stride = src_width * 3;
    uint32_t src_index = 0, dst_index = 0;

    for(uint32_t y = 0; y < src_height; ++y) {
        for(uint32_t x = 0; x < src_width; ++x) {
            src_index = y * src_stride + x * 3;
            dst_index = (src_height - y - 1) * dst_stride + (src_width - x - 1) * 3;
            dst[dst_index] = src[src_index];
            dst[dst_index + 1] = src[src_index + 1];
            dst[dst_index + 2] = src[src_index + 2];
        }
    }
}

/**
 * @brief Rotates RGB888 image 270 degrees clockwise (or 90 degrees counter-clockwise).
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 */
void tal_image_rgb888_rotate270(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height)
{
    uint32_t src_stride = src_width * 3;
    uint32_t dst_stride = src_height * 3;
    uint32_t src_index = 0, dst_index = 0;

    for(uint32_t x = 0; x < src_width; ++x) {
        for(uint32_t y = 0; y < src_height; ++y) {
            src_index = y * src_stride + x * 3;
            dst_index = x * dst_stride + (src_height - y - 1) * 3;
            dst[dst_index] = src[src_index];       /* Red */
            dst[dst_index + 1] = src[src_index + 1]; /* Green */
            dst[dst_index + 2] = src[src_index + 2]; /* Blue */
        }
    }
}

/**
 * @brief Rotates RGB565 image 270 degrees clockwise (or 90 degrees counter-clockwise).
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @param is_swap Whether to swap byte order during rotation.
 */
void tal_image_rgb565_rotate270(uint16_t * src, uint16_t * dst, uint32_t src_width, uint32_t src_height, bool is_swap)
{
    uint32_t src_stride = src_width;
    uint32_t dst_stride = src_height;
    uint32_t src_index = 0, dst_index = 0;

    for(uint32_t x = 0; x < src_width; ++x) {
        dst_index = x * dst_stride;
        src_index = x;
        for(uint32_t y = 0; y < src_height; ++y) {
            if(true == is_swap) {
                dst[dst_index + (src_height - y - 1)] = WORD_SWAP(src[src_index]);
            }else {
                dst[dst_index + (src_height - y - 1)] = src[src_index];
            }

            src_index += src_stride;
        }
    }
}

/**
 * @brief Rotates RGB565 image 180 degrees.
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @param is_swap Whether to swap byte order during rotation.
 */
void tal_image_rgb565_rotate180(uint16_t * src, uint16_t * dst, uint32_t src_width, uint32_t src_height, bool is_swap)
{
    uint32_t src_stride = src_width;
    uint32_t dst_stride = src_width;
    uint32_t src_index = 0, dst_index = 0;

    for(uint32_t y = 0; y < src_height; ++y) {
        dst_index = (src_height - y - 1) * dst_stride;
        src_index = y * src_stride;
        for(uint32_t x = 0; x < src_width; ++x) {
            if(true == is_swap) {
                dst[dst_index + src_width - x - 1] = WORD_SWAP(src[src_index + x]);
            }else {
                dst[dst_index + src_width - x - 1] = src[src_index + x];
            }
        }
    }
}

/**
 * @brief Rotates RGB565 image 90 degrees clockwise.
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @param is_swap Whether to swap byte order during rotation.
 */
void tal_image_rgb565_rotate90(uint16_t * src, uint16_t * dst, uint32_t src_width, uint32_t src_height, bool is_swap)
{
    uint32_t src_stride = src_width;
    uint32_t dst_stride = src_height;
    uint32_t src_index = 0, dst_index = 0;

    for(uint32_t x = 0; x < src_width; ++x) {
        dst_index = (src_width - x - 1);
        src_index = x;
        for(uint32_t y = 0; y < src_height; ++y) {
            if(true == is_swap) {
                dst[dst_index * dst_stride + y] = WORD_SWAP(src[src_index]);
            }else {
                dst[dst_index * dst_stride + y] = src[src_index];
            }
            src_index += src_stride;
        }
    }
}

/**
 * @brief Rotates monochrome image 270 degrees clockwise (or 90 degrees counter-clockwise).
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 */
void tal_image_mono_rotate270(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height)
{
    uint32_t src_stride = (src_width+7)/8;
    uint32_t dst_stride = (src_height+7)/8;
    uint32_t src_index = 0, dst_index = 0;
    uint32_t src_bit_idx = 0, dst_bit_idx = 0, pixel  = 0; 

    for(uint32_t x = 0; x < src_width; ++x) {
        for(uint32_t y = 0; y < src_height; ++y) {
            src_index   = y * src_stride + x / 8;
            src_bit_idx = x % 8;
            pixel = (src[src_index] >> src_bit_idx) & 0x01;

            dst_index = (src_width -1 - x) * dst_stride + y / 8;
            dst_bit_idx = y % 8;

            if(pixel) {
                dst[dst_index] |= (1 << dst_bit_idx);
            }else {
                dst[dst_index] &= ~(1 << dst_bit_idx);
            }
        }
    }
}

/**
 * @brief Rotates monochrome image 180 degrees.
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 */
void tal_image_mono_rotate180(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height)
{
    uint32_t src_stride = (src_width+7)/8;
    uint32_t dst_stride = (src_width+7)/8;
    uint32_t src_index = 0, dst_index = 0;
    uint32_t src_bit_idx = 0, dst_bit_idx = 0, pixel  = 0; 

    for(uint32_t x = 0; x < src_width; ++x) {
        for(uint32_t y = 0; y < src_height; ++y) {
            src_index   = y * src_stride + x / 8;
            src_bit_idx = x % 8;
            pixel = (src[src_index] >> src_bit_idx) & 0x01;

            dst_index = (src_height -1 - y) * dst_stride + (src_width -1 -x) / 8;
            dst_bit_idx = (src_width -1 -x) % 8;

            if(pixel) {
                dst[dst_index] |= (1 << dst_bit_idx);
            }else {
                dst[dst_index] &= ~(1 << dst_bit_idx);
            }
        }
    }
}

/**
 * @brief Rotates monochrome image 90 degrees clockwise.
 *
 * @param src Pointer to the source image buffer.
 * @param dst Pointer to the destination image buffer.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 */
void tal_image_mono_rotate90(uint8_t * src, uint8_t * dst, uint32_t src_width, uint32_t src_height)
{
    uint32_t src_stride = (src_width+7)/8;
    uint32_t dst_stride = (src_height+7)/8;
    uint32_t src_index = 0, dst_index = 0;
    uint32_t src_bit_idx = 0, dst_bit_idx = 0, pixel  = 0; 

    for(uint32_t x = 0; x < src_width; ++x) {
        for(uint32_t y = 0; y < src_height; ++y) {
            src_index   = y * src_stride + x / 8;
            src_bit_idx = x % 8;
            pixel = (src[src_index] >> src_bit_idx) & 0x01;

            dst_index = (x) * dst_stride + (src_height -1 -y) / 8;
            dst_bit_idx = (src_height -1 -y) % 8;

            if(pixel) {
                dst[dst_index] |= (1 << dst_bit_idx);
            }else {
                dst[dst_index] &= ~(1 << dst_bit_idx);
            }
        }
    }
}