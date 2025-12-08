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
 * @brief Binary conversion configuration (for printer)
 */
typedef struct {
    BINARY_METHOD_E method;
    uint8_t fixed_threshold;
} BINARY_CONFIG_T;

/**
 * @brief Convert YUV422 to printer binary format with selected algorithm
 * @param yuv422_data Source YUV422 data (e.g., 384x384)
 * @param src_width Source width
 * @param src_height Source height
 * @param binary_data Output binary buffer (pre-allocated)
 * @param dst_width Destination width (e.g., 240)
 * @param dst_height Destination height (e.g., 168)
 * @param config Binary conversion configuration
 * @return 0 on success, -1 on error
 */
int yuv422_to_printer_binary(const uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                             int dst_width, int dst_height, const BINARY_CONFIG_T *config);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* YUV422_TO_BINARY_H */
