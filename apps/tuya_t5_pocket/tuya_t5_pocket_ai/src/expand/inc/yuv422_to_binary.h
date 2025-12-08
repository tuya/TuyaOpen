/**
 * @file yuv422_to_binary.h
 * @brief YUV422 to binary image conversion algorithms for thermal printer
 *
 * Provides 9 different algorithms for converting YUV422 camera data to binary format:
 * 1. Fixed threshold
 * 2. Adaptive threshold
 * 3. Otsu's method
 * 4-6. Bayer dithering (4/8/16 levels)
 * 7-9. Error diffusion (Floyd-Steinberg, Stucki, Jarvis)
 *
 * All algorithms rotate 90° counter-clockwise and crop to desired size.
 * Output format: MSB first bitmap for thermal printer (bit=1->black, bit=0->white)
 *
 * @copyright Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 */

#ifndef YUV422_TO_BINARY_H
#define YUV422_TO_BINARY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Binary conversion method enum (for printer)
 */
typedef enum {
    BINARY_METHOD_FIXED = 0,       // Fixed threshold
    BINARY_METHOD_ADAPTIVE,        // Adaptive threshold
    BINARY_METHOD_OTSU,            // Otsu's method
    BINARY_METHOD_BAYER8_DITHER,   // 8-level grayscale Bayer dithering (3x3)
    BINARY_METHOD_BAYER4_DITHER,   // 4-level grayscale Bayer dithering (2x2)
    BINARY_METHOD_BAYER16_DITHER,  // 16-level grayscale Bayer dithering (4x4)
    BINARY_METHOD_FLOYD_STEINBERG, // Floyd-Steinberg error diffusion
    BINARY_METHOD_STUCKI,          // Stucki error diffusion
    BINARY_METHOD_JARVIS,          // Jarvis-Judice-Ninke error diffusion
    BINARY_METHOD_COUNT            // Total number of methods
} BINARY_METHOD_E;

/**
 * @brief Binary conversion configuration
 */
typedef struct {
    BINARY_METHOD_E method;
    uint8_t fixed_threshold;
} BINARY_CONFIG_T;

/**
 * @brief YUV422 to binary conversion parameters
 */
typedef struct {
    const uint8_t *yuv422_data;    // Source YUV422 data
    int src_width;                 // Source width (e.g., 384 or 480)
    int src_height;                // Source height (e.g., 384 or 480)
    uint8_t *binary_data;          // Output binary buffer (pre-allocated)
    int dst_width;                 // Destination width (e.g., 240 or 384)
    int dst_height;                // Destination height (e.g., 168 or 384)
    const BINARY_CONFIG_T *config; // Conversion configuration
    int invert_colors;             // 1: bit=1->white (LVGL), 0: bit=1->black (printer)
} YUV422_TO_BINARY_PARAMS_T;

/**
 * @brief Convert YUV422 to binary format with selected algorithm (universal interface)
 * @param params Conversion parameters structure
 * @return 0 on success, -1 on error
 * @note All algorithms rotate 90° counter-clockwise and crop to desired size
 * @note Caller specifies color mapping via invert_colors parameter
 */
int yuv422_to_binary(const YUV422_TO_BINARY_PARAMS_T *params);

/**
 * @brief Convert YUV422 to printer binary format (convenience wrapper)
 * @param params Conversion parameters (invert_colors will be set to 0 for printer)
 * @return 0 on success, -1 on error
 * @note Output: bit=1->black, bit=0->white (for thermal printer)
 */
int yuv422_to_printer_binary(const YUV422_TO_BINARY_PARAMS_T *params);

/**
 * @brief Convert YUV422 to LVGL I1 format binary (convenience wrapper)
 * @param params Conversion parameters (invert_colors will be set to 1 for LVGL)
 * @return 0 on success, -1 on error
 * @note Output: bit=1->white, bit=0->black (for LVGL display)
 */
int yuv422_to_lvgl_binary(const YUV422_TO_BINARY_PARAMS_T *params);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* YUV422_TO_BINARY_H */
