/**
 * @file pixel_art_types.h
 * @brief Common pixel art data types
 */

#ifndef __PIXEL_ART_TYPES_H__
#define __PIXEL_ART_TYPES_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RGB pixel structure
 */
typedef struct {
    uint8_t r; // Red component (0-255)
    uint8_t g; // Green component (0-255)
    uint8_t b; // Blue component (0-255)
} pixel_rgb_t;

/**
 * @brief Pixel art frame structure
 */
typedef struct {
    const pixel_rgb_t *pixels; // Pixel data array
    uint16_t width;            // Frame width
    uint16_t height;           // Frame height
} pixel_frame_t;

/**
 * @brief Pixel art data structure (for animated GIFs)
 */
typedef struct {
    const pixel_frame_t *frames; // Array of frames
    uint16_t frame_count;        // Number of frames
    uint16_t width;              // Frame width
    uint16_t height;             // Frame height
} pixel_art_t;

#ifdef __cplusplus
}
#endif

#endif /* __PIXEL_ART_TYPES_H__ */
