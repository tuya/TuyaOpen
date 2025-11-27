/**
 * @file pixel_font_adapter.h
 * @brief Font adapter for Adafruit GFX font format compatibility
 *
 * This header provides adapter structures compatible with Adafruit GFX font format
 * without requiring the Adafruit_GFX.h library.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __PIXEL_FONT_ADAPTER_H__
#define __PIXEL_FONT_ADAPTER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
// PROGMEM is Arduino-specific, define as empty for standard C
#ifndef PROGMEM
#define PROGMEM
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Glyph structure (compatible with Adafruit GFX)
 */
typedef struct {
    uint16_t bitmapOffset; ///< Pointer into GFXfont->bitmap
    uint8_t width;         ///< Bitmap dimensions in pixels
    uint8_t height;        ///< Bitmap dimensions in pixels
    uint8_t xAdvance;      ///< Distance to advance cursor (x axis)
    int8_t xOffset;        ///< X dist from cursor pos to UL corner
    int8_t yOffset;        ///< Y dist from cursor pos to UL corner
} GFXglyph;

/**
 * @brief Font structure (compatible with Adafruit GFX)
 */
typedef struct {
    uint8_t *bitmap;  ///< Glyph bitmaps, concatenated
    GFXglyph *glyph;  ///< Glyph array
    uint8_t first;    ///< ASCII extents (first char)
    uint8_t last;     ///< ASCII extents (last char)
    uint8_t yAdvance; ///< Newline distance (y axis)
} GFXfont;

#ifdef __cplusplus
}
#endif

#endif /* __PIXEL_FONT_ADAPTER_H__ */
