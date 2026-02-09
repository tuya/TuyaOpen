/**
 * @file tal_image_yuv422_to_binary.c
 * @brief YUV422 to binary image conversion algorithms implementation.
 *
 * This file implements 9 different algorithms for converting YUV422 camera data
 * to binary format: fixed threshold, adaptive threshold, Otsu's method, Bayer
 * dithering (4/8/16 levels), and error diffusion methods (Floyd-Steinberg,
 * Stucki, Jarvis-Judice-Ninke).
 *
 * All algorithms rotate 90 degrees counter-clockwise and crop to desired size.
 * Output format: MSB first bitmap, color mapping controlled by invert_colors parameter.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include <string.h>
#include "tuya_cloud_types.h"
#include "tal_memory.h"
#include "tal_log.h"
#include "tal_image_yuv422_to_binary.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************Bayer Matrices***********************
***********************************************************/
static const uint8_t bayer_2x2[2][2] = {{0, 2}, {3, 1}};
static const uint8_t bayer_3x3[3][3] = {{0, 7, 3}, {6, 4, 2}, {1, 5, 8}};
static const uint8_t bayer_4x4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

/***********************************************************
*********************Threshold Calculation******************
***********************************************************/
/**
 * @brief Calculates adaptive threshold based on average luminance.
 *
 * @param yuv422_data Pointer to the YUV422 image data.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @return Calculated threshold value (0-255).
 */
static uint8_t calculate_adaptive_threshold(const uint8_t *yuv422_data, int src_width, int src_height)
{
    uint32_t luminance_sum = 0;
    int total_pixels = src_width * src_height;

    for (int y = 0; y < src_height; y++) {
        int row_offset = y * src_width * 2;
        for (int x = 0; x < src_width; x++) {
            int yuv_index = row_offset + x * 2 + 1; /* Y component */
            luminance_sum += yuv422_data[yuv_index];
        }
    }

    return (uint8_t)(luminance_sum / total_pixels);
}

/**
 * @brief Calculates optimal threshold using Otsu's method.
 *
 * @param yuv422_data Pointer to the YUV422 image data.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @return Calculated optimal threshold value (0-255).
 */
static uint8_t calculate_otsu_threshold(const uint8_t *yuv422_data, int src_width, int src_height)
{
    int histogram[256] = {0};
    int total_pixels = src_width * src_height;

    /* Build histogram */
    for (int y = 0; y < src_height; y++) {
        int row_offset = y * src_width * 2;
        for (int x = 0; x < src_width; x++) {
            int yuv_index = row_offset + x * 2 + 1;
            uint8_t luminance = yuv422_data[yuv_index];
            histogram[luminance]++;
        }
    }

    /* Calculate optimal threshold */
    float sum = 0;
    for (int i = 0; i < 256; i++) {
        sum += i * histogram[i];
    }

    float sum_background = 0;
    int weight_background = 0;
    float max_variance = 0;
    uint8_t optimal_threshold = 0;

    for (int t = 0; t < 256; t++) {
        weight_background += histogram[t];
        if (weight_background == 0)
            continue;

        int weight_foreground = total_pixels - weight_background;
        if (weight_foreground == 0)
            break;

        sum_background += t * histogram[t];

        float mean_background = sum_background / weight_background;
        float mean_foreground = (sum - sum_background) / weight_foreground;

        float variance = (float)weight_background * weight_foreground * (mean_background - mean_foreground) *
                         (mean_background - mean_foreground);

        if (variance > max_variance) {
            max_variance = variance;
            optimal_threshold = t;
        }
    }

    return optimal_threshold;
}

/***********************************************************
****************Simple Threshold Conversion*****************
***********************************************************/
/**
 * @brief Converts YUV422 to binary using threshold method with rotation and cropping.
 *
 * @param yuv422_data Pointer to the source YUV422 image data.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @param binary_data Pointer to the destination binary image buffer.
 * @param dst_width Width of the destination image.
 * @param dst_height Height of the destination image.
 * @param threshold Threshold value for binary conversion (0-255).
 * @param invert Color inversion flag (1: bit=1->white, 0: bit=1->black).
 * @return 0 on success, negative value on error.
 */
static int yuv422_to_binary_crop_threshold(const uint8_t *yuv422_data, int src_width, int src_height,
                                           uint8_t *binary_data, int dst_width, int dst_height, uint8_t threshold,
                                           int invert)
{
    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2; /* Dynamic: (src_width - dst_height) / 2 */

    for (int dst_y = 0; dst_y < dst_height; dst_y++) {
        int row_offset = dst_y * binary_stride;

        for (int dst_x = 0; dst_x < dst_width; dst_x++) {
            /* Rotate 90° CCW: (dst_x, dst_y) -> (src_y=height-1-dst_x, src_x=dst_y+offset) */
            int src_x = dst_y + crop_offset;
            int src_y = src_height - 1 - dst_x;

            if (src_x < 0 || src_x >= src_width || src_y < 0 || src_y >= src_height) {
                continue;
            }

            int yuv_index = src_y * src_width * 2 + src_x * 2 + 1;
            uint8_t luminance = yuv422_data[yuv_index];

            /* Apply threshold with color inversion control */
            /* invert=0 (printer): luminance < threshold -> bit=1 (black) */
            /* invert=1 (LVGL):    luminance >= threshold -> bit=1 (white) */
            int should_set_bit = invert ? (luminance >= threshold) : (luminance < threshold);

            if (should_set_bit) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }
        }
    }

    return 0;
}

/***********************************************************
********************Bayer Dithering Methods*****************
***********************************************************/
/**
 * @brief Converts YUV422 to binary using 4-level Bayer dithering (2x2 matrix).
 *
 * @param yuv422_data Pointer to the source YUV422 image data.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @param binary_data Pointer to the destination binary image buffer.
 * @param dst_width Width of the destination image.
 * @param dst_height Height of the destination image.
 * @param invert Color inversion flag (1: bit=1->white, 0: bit=1->black).
 * @return 0 on success, negative value on error.
 */
static int yuv422_to_bayer4_dither(const uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                   int dst_width, int dst_height, int invert)
{
    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2; /* Dynamic: (src_width - dst_height) / 2 */

    for (int dst_y = 0; dst_y < dst_height; dst_y++) {
        int row_offset = dst_y * binary_stride;

        for (int dst_x = 0; dst_x < dst_width; dst_x++) {
            int src_x = dst_y + crop_offset;
            int src_y = src_height - 1 - dst_x;

            if (src_x < 0 || src_x >= src_width || src_y < 0 || src_y >= src_height) {
                continue;
            }

            int yuv_index = src_y * src_width * 2 + src_x * 2 + 1;
            uint8_t luminance = yuv422_data[yuv_index];

            /* 4-level Bayer dithering (2x2 matrix, threshold 0-3) */
            uint8_t bayer_value = bayer_2x2[dst_y % 2][dst_x % 2];
            uint8_t gray_level = luminance / 85; /* Map 0-255 to 0-3 */

            int should_set_bit =
                invert ? (gray_level >= bayer_value && luminance >= 32) : (gray_level < bayer_value || luminance < 32);
            if (should_set_bit) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }
        }
    }

    return 0;
}

/**
 * @brief Converts YUV422 to binary using 8-level Bayer dithering (3x3 matrix).
 *
 * @param yuv422_data Pointer to the source YUV422 image data.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @param binary_data Pointer to the destination binary image buffer.
 * @param dst_width Width of the destination image.
 * @param dst_height Height of the destination image.
 * @param invert Color inversion flag (1: bit=1->white, 0: bit=1->black).
 * @return 0 on success, negative value on error.
 */
static int yuv422_to_bayer8_dither(const uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                   int dst_width, int dst_height, int invert)
{
    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2; /* Dynamic: (src_width - dst_height) / 2 */

    for (int dst_y = 0; dst_y < dst_height; dst_y++) {
        int row_offset = dst_y * binary_stride;

        for (int dst_x = 0; dst_x < dst_width; dst_x++) {
            int src_x = dst_y + crop_offset;
            int src_y = src_height - 1 - dst_x;

            if (src_x < 0 || src_x >= src_width || src_y < 0 || src_y >= src_height) {
                continue;
            }

            int yuv_index = src_y * src_width * 2 + src_x * 2 + 1;
            uint8_t luminance = yuv422_data[yuv_index];

            /* 8-level Bayer dithering (3x3 matrix, threshold 0-8) */
            uint8_t bayer_value = bayer_3x3[dst_y % 3][dst_x % 3];
            uint8_t gray_level = luminance / 32; /* Map 0-255 to 0-7 */

            int should_set_bit =
                invert ? (gray_level >= bayer_value && luminance >= 16) : (gray_level < bayer_value || luminance < 16);
            if (should_set_bit) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }
        }
    }

    return 0;
}

/**
 * @brief Converts YUV422 to binary using 16-level Bayer dithering (4x4 matrix).
 *
 * @param yuv422_data Pointer to the source YUV422 image data.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @param binary_data Pointer to the destination binary image buffer.
 * @param dst_width Width of the destination image.
 * @param dst_height Height of the destination image.
 * @param invert Color inversion flag (1: bit=1->white, 0: bit=1->black).
 * @return 0 on success, negative value on error.
 */
static int yuv422_to_bayer16_dither(const uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                    int dst_width, int dst_height, int invert)
{
    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2; /* Dynamic: (src_width - dst_height) / 2 */

    for (int dst_y = 0; dst_y < dst_height; dst_y++) {
        int row_offset = dst_y * binary_stride;

        for (int dst_x = 0; dst_x < dst_width; dst_x++) {
            int src_x = dst_y + crop_offset;
            int src_y = src_height - 1 - dst_x;

            if (src_x < 0 || src_x >= src_width || src_y < 0 || src_y >= src_height) {
                continue;
            }

            int yuv_index = src_y * src_width * 2 + src_x * 2 + 1;
            uint8_t luminance = yuv422_data[yuv_index];

            /* 16-level Bayer dithering (4x4 matrix, threshold 0-15) */
            uint8_t bayer_value = bayer_4x4[dst_y % 4][dst_x % 4];
            uint8_t gray_level = luminance / 17; /* Map 0-255 to 0-15 */

            int should_set_bit = invert ? (gray_level >= bayer_value) : (gray_level < bayer_value);
            if (should_set_bit) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }
        }
    }

    return 0;
}

/***********************************************************
**************Error Diffusion Methods***********************
***********************************************************/
/**
 * @brief Converts YUV422 to binary using Floyd-Steinberg error diffusion.
 *
 * @param yuv422_data Pointer to the source YUV422 image data.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @param binary_data Pointer to the destination binary image buffer.
 * @param dst_width Width of the destination image.
 * @param dst_height Height of the destination image.
 * @param invert Color inversion flag (1: bit=1->white, 0: bit=1->black).
 * @return 0 on success, negative value on error.
 */
static int yuv422_to_floyd_steinberg(const uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                     int dst_width, int dst_height, int invert)
{
    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2; /* Dynamic: (src_width - dst_height) / 2 */

    /* Allocate error buffers with padding */
    int16_t *error_buffer = (int16_t *)Malloc((dst_width + 2) * 2 * sizeof(int16_t));
    if (!error_buffer) {
        return -1;
    }

    int16_t *curr_row = error_buffer + 1;
    int16_t *next_row = error_buffer + dst_width + 3;
    memset(error_buffer, 0, (dst_width + 2) * 2 * sizeof(int16_t));

    for (int dst_y = 0; dst_y < dst_height; dst_y++) {
        int row_offset = dst_y * binary_stride;

        for (int dst_x = 0; dst_x < dst_width; dst_x++) {
            int src_x = dst_y + crop_offset;
            int src_y = src_height - 1 - dst_x;

            if (src_x < 0 || src_x >= src_width || src_y < 0 || src_y >= src_height) {
                continue;
            }

            int yuv_index = src_y * src_width * 2 + src_x * 2 + 1;
            int16_t luminance = (int16_t)yuv422_data[yuv_index] + curr_row[dst_x];

            if (luminance < 0)
                luminance = 0;
            if (luminance > 255)
                luminance = 255;

            uint8_t new_pixel = (luminance >= 128) ? 255 : 0;
            int16_t error = luminance - new_pixel;

            /* Printer: bit=1->black */
            int should_set_bit = invert ? (new_pixel == 255) : (new_pixel == 0);
            if (should_set_bit) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }

            /* Floyd-Steinberg error diffusion */
            if (dst_x < dst_width - 1)
                curr_row[dst_x + 1] += (error * 7) / 16;
            if (dst_x > 0)
                next_row[dst_x - 1] += (error * 3) / 16;
            next_row[dst_x] += (error * 5) / 16;
            if (dst_x < dst_width - 1)
                next_row[dst_x + 1] += error / 16;
        }

        /* Swap rows */
        int16_t *temp = curr_row;
        curr_row = next_row;
        next_row = temp;
        memset(next_row - 1, 0, (dst_width + 2) * sizeof(int16_t));
    }

    Free(error_buffer);

    return 0;
}

/**
 * @brief Converts YUV422 to binary using Stucki error diffusion.
 *
 * @param yuv422_data Pointer to the source YUV422 image data.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @param binary_data Pointer to the destination binary image buffer.
 * @param dst_width Width of the destination image.
 * @param dst_height Height of the destination image.
 * @param invert Color inversion flag (1: bit=1->white, 0: bit=1->black).
 * @return 0 on success, negative value on error.
 */
static int yuv422_to_stucki(const uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                            int dst_width, int dst_height, int invert)
{
    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2; /* Dynamic: (src_width - dst_height) / 2 */

    /* Allocate 3 error rows with padding */
    int16_t *error_buffer = (int16_t *)Malloc((dst_width + 4) * 3 * sizeof(int16_t));
    if (!error_buffer) {
        return -1;
    }

    int16_t *curr_row = error_buffer + 2;
    int16_t *next_row1 = error_buffer + dst_width + 6;
    int16_t *next_row2 = error_buffer + 2 * (dst_width + 4) + 2;
    memset(error_buffer, 0, (dst_width + 4) * 3 * sizeof(int16_t));

    for (int dst_y = 0; dst_y < dst_height; dst_y++) {
        int row_offset = dst_y * binary_stride;

        for (int dst_x = 0; dst_x < dst_width; dst_x++) {
            int src_x = dst_y + crop_offset;
            int src_y = src_height - 1 - dst_x;

            if (src_x < 0 || src_x >= src_width || src_y < 0 || src_y >= src_height) {
                continue;
            }

            int yuv_index = src_y * src_width * 2 + src_x * 2 + 1;
            int16_t luminance = (int16_t)yuv422_data[yuv_index] + curr_row[dst_x];

            if (luminance < 0)
                luminance = 0;
            if (luminance > 255)
                luminance = 255;

            uint8_t new_pixel = (luminance >= 128) ? 255 : 0;
            int16_t error = luminance - new_pixel;

            int should_set_bit = invert ? (new_pixel == 255) : (new_pixel == 0);
            if (should_set_bit) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }

            /* Stucki error diffusion (divisor: 42) */
            if (dst_x < dst_width - 1)
                curr_row[dst_x + 1] += (error * 8) / 42;
            if (dst_x < dst_width - 2)
                curr_row[dst_x + 2] += (error * 4) / 42;
            if (dst_x > 1)
                next_row1[dst_x - 2] += (error * 2) / 42;
            if (dst_x > 0)
                next_row1[dst_x - 1] += (error * 4) / 42;
            next_row1[dst_x] += (error * 8) / 42;
            if (dst_x < dst_width - 1)
                next_row1[dst_x + 1] += (error * 4) / 42;
            if (dst_x < dst_width - 2)
                next_row1[dst_x + 2] += (error * 2) / 42;
            if (dst_x > 1)
                next_row2[dst_x - 2] += error / 42;
            if (dst_x > 0)
                next_row2[dst_x - 1] += (error * 2) / 42;
            next_row2[dst_x] += (error * 4) / 42;
            if (dst_x < dst_width - 1)
                next_row2[dst_x + 1] += (error * 2) / 42;
            if (dst_x < dst_width - 2)
                next_row2[dst_x + 2] += error / 42;
        }

        /* Rotate rows */
        int16_t *temp = curr_row;
        curr_row = next_row1;
        next_row1 = next_row2;
        next_row2 = temp;
        memset(next_row2 - 2, 0, (dst_width + 4) * sizeof(int16_t));
    }

    Free(error_buffer);

    return 0;
}

/**
 * @brief Converts YUV422 to binary using Jarvis-Judice-Ninke error diffusion.
 *
 * @param yuv422_data Pointer to the source YUV422 image data.
 * @param src_width Width of the source image.
 * @param src_height Height of the source image.
 * @param binary_data Pointer to the destination binary image buffer.
 * @param dst_width Width of the destination image.
 * @param dst_height Height of the destination image.
 * @param invert Color inversion flag (1: bit=1->white, 0: bit=1->black).
 * @return 0 on success, negative value on error.
 */
static int yuv422_to_jarvis(const uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                            int dst_width, int dst_height, int invert)
{
    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2; /* Dynamic: (src_width - dst_height) / 2 */

    /* Allocate 3 error rows with padding */
    int16_t *error_buffer = (int16_t *)Malloc((dst_width + 4) * 3 * sizeof(int16_t));
    if (!error_buffer) {
        return -1;
    }

    int16_t *curr_row = error_buffer + 2;
    int16_t *next_row1 = error_buffer + dst_width + 6;
    int16_t *next_row2 = error_buffer + 2 * (dst_width + 4) + 2;
    memset(error_buffer, 0, (dst_width + 4) * 3 * sizeof(int16_t));

    for (int dst_y = 0; dst_y < dst_height; dst_y++) {
        int row_offset = dst_y * binary_stride;

        for (int dst_x = 0; dst_x < dst_width; dst_x++) {
            int src_x = dst_y + crop_offset;
            int src_y = src_height - 1 - dst_x;

            if (src_x < 0 || src_x >= src_width || src_y < 0 || src_y >= src_height) {
                continue;
            }

            int yuv_index = src_y * src_width * 2 + src_x * 2 + 1;
            int16_t luminance = (int16_t)yuv422_data[yuv_index] + curr_row[dst_x];

            if (luminance < 0)
                luminance = 0;
            if (luminance > 255)
                luminance = 255;

            uint8_t new_pixel = (luminance >= 128) ? 255 : 0;
            int16_t error = luminance - new_pixel;

            int should_set_bit = invert ? (new_pixel == 255) : (new_pixel == 0);
            if (should_set_bit) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }

            /* Jarvis-Judice-Ninke error diffusion (divisor: 48) */
            if (dst_x < dst_width - 1)
                curr_row[dst_x + 1] += (error * 7) / 48;
            if (dst_x < dst_width - 2)
                curr_row[dst_x + 2] += (error * 5) / 48;
            if (dst_x > 1)
                next_row1[dst_x - 2] += (error * 3) / 48;
            if (dst_x > 0)
                next_row1[dst_x - 1] += (error * 5) / 48;
            next_row1[dst_x] += (error * 7) / 48;
            if (dst_x < dst_width - 1)
                next_row1[dst_x + 1] += (error * 5) / 48;
            if (dst_x < dst_width - 2)
                next_row1[dst_x + 2] += (error * 3) / 48;
            if (dst_x > 1)
                next_row2[dst_x - 2] += error / 48;
            if (dst_x > 0)
                next_row2[dst_x - 1] += (error * 3) / 48;
            next_row2[dst_x] += (error * 5) / 48;
            if (dst_x < dst_width - 1)
                next_row2[dst_x + 1] += (error * 3) / 48;
            if (dst_x < dst_width - 2)
                next_row2[dst_x + 2] += error / 48;
        }

        /* Rotate rows */
        int16_t *temp = curr_row;
        curr_row = next_row1;
        next_row1 = next_row2;
        next_row2 = temp;
        memset(next_row2 - 2, 0, (dst_width + 4) * sizeof(int16_t));
    }

    Free(error_buffer);

    return 0;
}


/**
 * @brief Converts YUV422 format image to binary (monochrome) format.
 *
 * @param conv_cfg Pointer to the conversion configuration structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET tal_image_format_yuv422_to_binary(TAL_IMAGE_YUV422_TO_BINARY_T *conv_cfg)
{
    if (conv_cfg == NULL || conv_cfg->in_buf == NULL || conv_cfg->out_buf == NULL) {
        return OPRT_INVALID_PARM;
    }

    /* Clear output buffer */
    int bitmap_size = (conv_cfg->out_width + 7) / 8 * conv_cfg->out_height;

    memset(conv_cfg->out_buf, 0, bitmap_size);

    switch (conv_cfg->method) {
    case TAL_IMAGE_MONO_MTH_FIXED:
        return yuv422_to_binary_crop_threshold(conv_cfg->in_buf, conv_cfg->in_width, conv_cfg->in_height,
                                               conv_cfg->out_buf, conv_cfg->out_width, conv_cfg->out_height,
                                               conv_cfg->fixed_threshold, conv_cfg->invert_colors);
    case TAL_IMAGE_MONO_MTH_ADAPTIVE: {
        uint8_t threshold = calculate_adaptive_threshold(conv_cfg->in_buf, conv_cfg->in_width, conv_cfg->in_height);
        return yuv422_to_binary_crop_threshold(conv_cfg->in_buf, conv_cfg->in_width, conv_cfg->in_height,
                                               conv_cfg->out_buf, conv_cfg->out_width, conv_cfg->out_height, threshold,
                                               conv_cfg->invert_colors);
    }
    case TAL_IMAGE_MONO_MTH_OTSU: {
        uint8_t threshold = calculate_otsu_threshold(conv_cfg->in_buf, conv_cfg->in_width, conv_cfg->in_height);
        return yuv422_to_binary_crop_threshold(conv_cfg->in_buf, conv_cfg->in_width, conv_cfg->in_height,
                                               conv_cfg->out_buf, conv_cfg->out_width, conv_cfg->out_height, threshold,
                                               conv_cfg->invert_colors);
    }
    case TAL_IMAGE_MONO_MTH_BAYER4_DITHER:
        return yuv422_to_bayer4_dither(conv_cfg->in_buf, conv_cfg->in_width, conv_cfg->in_height, conv_cfg->out_buf,
                                       conv_cfg->out_width, conv_cfg->out_height, conv_cfg->invert_colors);

    case TAL_IMAGE_MONO_MTH_BAYER8_DITHER:
        return yuv422_to_bayer8_dither(conv_cfg->in_buf, conv_cfg->in_width, conv_cfg->in_height, conv_cfg->out_buf,
                                       conv_cfg->out_width, conv_cfg->out_height, conv_cfg->invert_colors);
    case TAL_IMAGE_MONO_MTH_BAYER16_DITHER:
        return yuv422_to_bayer16_dither(conv_cfg->in_buf, conv_cfg->in_width, conv_cfg->in_height, conv_cfg->out_buf,
                                        conv_cfg->out_width, conv_cfg->out_height, conv_cfg->invert_colors);

    case TAL_IMAGE_MONO_MTH_FLOYD_STEINBERG:
        return yuv422_to_floyd_steinberg(conv_cfg->in_buf, conv_cfg->in_width, conv_cfg->in_height,
                                         conv_cfg->out_buf, conv_cfg->out_width, conv_cfg->out_height,
                                         conv_cfg->invert_colors);

    case TAL_IMAGE_MONO_MTH_STUCKI:
        return yuv422_to_stucki(conv_cfg->in_buf, conv_cfg->in_width, conv_cfg->in_height, conv_cfg->out_buf,
                                conv_cfg->out_width, conv_cfg->out_height, conv_cfg->invert_colors);

    case TAL_IMAGE_MONO_MTH_JARVIS:
        return yuv422_to_jarvis(conv_cfg->in_buf, conv_cfg->in_width, conv_cfg->in_height, conv_cfg->out_buf,
                                conv_cfg->out_width, conv_cfg->out_height, conv_cfg->invert_colors);

    default:
        return OPRT_COM_ERROR;
    }
}