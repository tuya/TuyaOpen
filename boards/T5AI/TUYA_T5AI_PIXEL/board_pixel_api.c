/**
 * @file board_pixel_api.c
 * @brief Implementation of simple LED pixel drawing API
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "board_pixel_api.h"
#include "tdl_pixel_dev_manage.h"
#include "tdl_pixel_color_manage.h"
#include "tal_log.h"
#include "tal_api.h"
#include "tkl_memory.h"
#include "fonts/Fonts/Picopixel.h"
#include "fonts/Fonts/FreeMono9pt7b.h"
#include "fonts/Fonts/FreeMono12pt7b.h"
#include "fonts/Fonts/FreeMono18pt7b.h"
#include "fonts/Fonts/FreeMono24pt7b.h"
#include "fonts/Fonts/FreeMonoBold9pt7b.h"
#include "fonts/Fonts/FreeMonoBold12pt7b.h"
#include "fonts/Fonts/FreeMonoBold18pt7b.h"
#include "fonts/Fonts/FreeMonoBold24pt7b.h"
#include <string.h>
#include <math.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define COLOR_RESOLUTION 1000u
#define BRIGHTNESS       0.05f

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Internal frame structure
 */
typedef struct {
    PIXEL_COLOR_T pixels[PIXEL_MATRIX_TOTAL];
    bool initialized;
} pixel_frame_t;

/**
 * @brief Internal GIF structure
 */
typedef struct {
    const uint8_t **frames;
    uint32_t frame_count;
    uint32_t current_frame;
    uint32_t width;
    uint32_t height;
    uint32_t delay_ms;
    uint32_t last_update_time;
} pixel_gif_t;

/***********************************************************
***********************variable define**********************
***********************************************************/
static PIXEL_HANDLE_T g_pixel_handle = NULL;
static bool g_pixel_initialized = false;

/***********************************************************
********************function declaration********************
***********************************************************/
static uint32_t matrix_coord_to_led_index(uint32_t x, uint32_t y);
static PIXEL_COLOR_T color_enum_to_pixel_color(PIXEL_COLOR_ENUM_E color);
static OPERATE_RET init_pixel_device(void);
static const GFXfont *font_enum_to_font_ptr(PIXEL_FONT_ENUM_E font);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Convert 2D matrix coordinates to LED index
 */
static uint32_t matrix_coord_to_led_index(uint32_t x, uint32_t y)
{
    if (x >= PIXEL_MATRIX_WIDTH || y >= PIXEL_MATRIX_HEIGHT) {
        return 0;
    }

    uint32_t led_index;
    if (y % 2 == 0) {
        // Even row: left to right
        led_index = y * PIXEL_MATRIX_WIDTH + x;
    } else {
        // Odd row: right to left
        led_index = (y + 1) * PIXEL_MATRIX_WIDTH - 1 - x;
    }
    return led_index;
}

/**
 * @brief Convert color enumeration to PIXEL_COLOR_T
 */
static PIXEL_COLOR_T color_enum_to_pixel_color(PIXEL_COLOR_ENUM_E color)
{
    PIXEL_COLOR_T pixel_color = {0};
    uint32_t r = 0, g = 0, b = 0;

    // Define 32 colors in RGB (0-255)
    switch (color) {
    case PIXEL_COLOR_BLACK:
        r = 0;
        g = 0;
        b = 0;
        break;
    case PIXEL_COLOR_WHITE:
        r = 255;
        g = 255;
        b = 255;
        break;
    case PIXEL_COLOR_RED:
        r = 255;
        g = 0;
        b = 0;
        break;
    case PIXEL_COLOR_GREEN:
        r = 0;
        g = 255;
        b = 0;
        break;
    case PIXEL_COLOR_BLUE:
        r = 0;
        g = 0;
        b = 255;
        break;
    case PIXEL_COLOR_YELLOW:
        r = 255;
        g = 255;
        b = 0;
        break;
    case PIXEL_COLOR_CYAN:
        r = 0;
        g = 255;
        b = 255;
        break;
    case PIXEL_COLOR_MAGENTA:
        r = 255;
        g = 0;
        b = 255;
        break;
    case PIXEL_COLOR_ORANGE:
        r = 255;
        g = 165;
        b = 0;
        break;
    case PIXEL_COLOR_PURPLE:
        r = 128;
        g = 0;
        b = 128;
        break;
    case PIXEL_COLOR_PINK:
        r = 255;
        g = 192;
        b = 203;
        break;
    case PIXEL_COLOR_LIME:
        r = 0;
        g = 255;
        b = 0;
        break;
    case PIXEL_COLOR_NAVY:
        r = 0;
        g = 0;
        b = 128;
        break;
    case PIXEL_COLOR_MAROON:
        r = 128;
        g = 0;
        b = 0;
        break;
    case PIXEL_COLOR_OLIVE:
        r = 128;
        g = 128;
        b = 0;
        break;
    case PIXEL_COLOR_TEAL:
        r = 0;
        g = 128;
        b = 128;
        break;
    case PIXEL_COLOR_SILVER:
        r = 192;
        g = 192;
        b = 192;
        break;
    case PIXEL_COLOR_GRAY:
        r = 128;
        g = 128;
        b = 128;
        break;
    case PIXEL_COLOR_DARK_RED:
        r = 139;
        g = 0;
        b = 0;
        break;
    case PIXEL_COLOR_DARK_GREEN:
        r = 0;
        g = 100;
        b = 0;
        break;
    case PIXEL_COLOR_DARK_BLUE:
        r = 0;
        g = 0;
        b = 139;
        break;
    case PIXEL_COLOR_DARK_YELLOW:
        r = 184;
        g = 134;
        b = 11;
        break;
    case PIXEL_COLOR_DARK_CYAN:
        r = 0;
        g = 139;
        b = 139;
        break;
    case PIXEL_COLOR_DARK_MAGENTA:
        r = 139;
        g = 0;
        b = 139;
        break;
    case PIXEL_COLOR_LIGHT_RED:
        r = 255;
        g = 102;
        b = 102;
        break;
    case PIXEL_COLOR_LIGHT_GREEN:
        r = 144;
        g = 238;
        b = 144;
        break;
    case PIXEL_COLOR_LIGHT_BLUE:
        r = 173;
        g = 216;
        b = 230;
        break;
    case PIXEL_COLOR_LIGHT_YELLOW:
        r = 255;
        g = 255;
        b = 224;
        break;
    case PIXEL_COLOR_LIGHT_CYAN:
        r = 224;
        g = 255;
        b = 255;
        break;
    case PIXEL_COLOR_LIGHT_MAGENTA:
        r = 255;
        g = 119;
        b = 255;
        break;
    case PIXEL_COLOR_GOLD:
        r = 255;
        g = 215;
        b = 0;
        break;
    case PIXEL_COLOR_VIOLET:
        r = 238;
        g = 130;
        b = 238;
        break;
    default:
        r = 0;
        g = 0;
        b = 0;
        break;
    }

    // Convert RGB (0-255) to COLOR_RESOLUTION scale with brightness
    // Note: LED hardware expects GRB order, so we swap red and green
    pixel_color.red = (uint32_t)((g * COLOR_RESOLUTION * BRIGHTNESS) / 255);   // Red channel gets green data
    pixel_color.green = (uint32_t)((r * COLOR_RESOLUTION * BRIGHTNESS) / 255); // Green channel gets red data
    pixel_color.blue = (uint32_t)((b * COLOR_RESOLUTION * BRIGHTNESS) / 255);  // Blue stays the same
    pixel_color.warm = 0;
    pixel_color.cold = 0;

    return pixel_color;
}

/**
 * @brief Convert font enumeration to font pointer
 */
static const GFXfont *font_enum_to_font_ptr(PIXEL_FONT_ENUM_E font)
{
    switch (font) {
    case PIXEL_FONT_PICOPIXEL:
        return &Picopixel;
    case PIXEL_FONT_FREEMONO_9PT:
        return &FreeMono9pt7b;
    case PIXEL_FONT_FREEMONO_12PT:
        return &FreeMono12pt7b;
    case PIXEL_FONT_FREEMONO_18PT:
        return &FreeMono18pt7b;
    case PIXEL_FONT_FREEMONO_24PT:
        return &FreeMono24pt7b;
    case PIXEL_FONT_FREEMONO_BOLD_9PT:
        return &FreeMonoBold9pt7b;
    case PIXEL_FONT_FREEMONO_BOLD_12PT:
        return &FreeMonoBold12pt7b;
    case PIXEL_FONT_FREEMONO_BOLD_18PT:
        return &FreeMonoBold18pt7b;
    case PIXEL_FONT_FREEMONO_BOLD_24PT:
        return &FreeMonoBold24pt7b;
    default:
        return &Picopixel; // Default to Picopixel
    }
}

/**
 * @brief Initialize pixel device
 */
static OPERATE_RET init_pixel_device(void)
{
    if (g_pixel_initialized && g_pixel_handle != NULL) {
        return OPRT_OK;
    }

    char device_name[32] = "pixel";
#if defined(PIXEL_DEVICE_NAME)
    strncpy(device_name, PIXEL_DEVICE_NAME, sizeof(device_name) - 1);
    device_name[sizeof(device_name) - 1] = '\0';
#endif

    OPERATE_RET rt;
    uint32_t retry_count = 0;
    const uint32_t max_retries = 10;
    const uint32_t retry_delay_ms = 100;

    while (retry_count < max_retries) {
        rt = tdl_pixel_dev_find(device_name, &g_pixel_handle);
        if (OPRT_OK == rt && g_pixel_handle != NULL) {
            break;
        }
        retry_count++;
        if (retry_count < max_retries) {
            tal_system_sleep(retry_delay_ms);
        }
    }

    if (OPRT_OK != rt || g_pixel_handle == NULL) {
        PR_ERR("Failed to find pixel device '%s' after %d retries: %d", device_name, retry_count, rt);
        return (rt != OPRT_OK) ? rt : OPRT_COM_ERROR;
    }

    PIXEL_DEV_CONFIG_T pixels_cfg = {
        .pixel_num = PIXEL_MATRIX_TOTAL,
        .pixel_resolution = COLOR_RESOLUTION,
    };
    rt = tdl_pixel_dev_open(g_pixel_handle, &pixels_cfg);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to open pixel device: %d", rt);
        g_pixel_handle = NULL;
        return rt;
    }

    g_pixel_initialized = true;
    return OPRT_OK;
}

/**
 * @brief Create a new frame buffer
 */
PIXEL_FRAME_HANDLE_T board_pixel_frame_create(void)
{
    pixel_frame_t *frame = (pixel_frame_t *)tkl_system_malloc(sizeof(pixel_frame_t));
    if (frame == NULL) {
        PR_ERR("Failed to allocate frame memory");
        return NULL;
    }

    memset(frame, 0, sizeof(pixel_frame_t));
    frame->initialized = true;

    return (PIXEL_FRAME_HANDLE_T)frame;
}

/**
 * @brief Render frame to the LED matrix
 */
OPERATE_RET board_pixel_frame_render(PIXEL_FRAME_HANDLE_T frame)
{
    if (frame == NULL) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = init_pixel_device();
    if (OPRT_OK != rt) {
        return rt;
    }

    pixel_frame_t *f = (pixel_frame_t *)frame;
    if (!f->initialized) {
        return OPRT_INVALID_PARM;
    }

    // Copy frame buffer to LED device
    rt = tdl_pixel_set_multi_color(g_pixel_handle, 0, PIXEL_MATRIX_TOTAL, f->pixels);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to set pixel colors: %d", rt);
        return rt;
    }

    // Refresh display
    rt = tdl_pixel_dev_refresh(g_pixel_handle);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to refresh pixel device: %d", rt);
        return rt;
    }

    return OPRT_OK;
}

/**
 * @brief Destroy frame and free resources
 */
OPERATE_RET board_pixel_frame_destroy(PIXEL_FRAME_HANDLE_T frame)
{
    if (frame == NULL) {
        return OPRT_OK;
    }

    pixel_frame_t *f = (pixel_frame_t *)frame;
    if (f->initialized) {
        f->initialized = false;
        tkl_system_free(f);
    }

    return OPRT_OK;
}

/**
 * @brief Clear frame (set all pixels to black)
 */
OPERATE_RET board_pixel_frame_clear(PIXEL_FRAME_HANDLE_T frame)
{
    if (frame == NULL) {
        return OPRT_INVALID_PARM;
    }

    pixel_frame_t *f = (pixel_frame_t *)frame;
    if (!f->initialized) {
        return OPRT_INVALID_PARM;
    }

    PIXEL_COLOR_T black = {0};
    for (uint32_t i = 0; i < PIXEL_MATRIX_TOTAL; i++) {
        f->pixels[i] = black;
    }

    return OPRT_OK;
}

/**
 * @brief Set a single pixel at (x, y)
 */
OPERATE_RET board_pixel_set_pixel(PIXEL_FRAME_HANDLE_T frame, uint32_t x, uint32_t y, PIXEL_COLOR_ENUM_E color)
{
    if (frame == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (x >= PIXEL_MATRIX_WIDTH || y >= PIXEL_MATRIX_HEIGHT) {
        return OPRT_INVALID_PARM;
    }

    pixel_frame_t *f = (pixel_frame_t *)frame;
    if (!f->initialized) {
        return OPRT_INVALID_PARM;
    }

    uint32_t index = matrix_coord_to_led_index(x, y);
    if (index >= PIXEL_MATRIX_TOTAL) {
        return OPRT_INVALID_PARM;
    }

    f->pixels[index] = color_enum_to_pixel_color(color);

    return OPRT_OK;
}

/**
 * @brief Draw a line from (x1, y1) to (x2, y2)
 */
OPERATE_RET board_pixel_draw_line(PIXEL_FRAME_HANDLE_T frame, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2,
                                  PIXEL_COLOR_ENUM_E color)
{
    if (frame == NULL) {
        return OPRT_INVALID_PARM;
    }

    pixel_frame_t *f = (pixel_frame_t *)frame;
    if (!f->initialized) {
        return OPRT_INVALID_PARM;
    }

    PIXEL_COLOR_T pixel_color = color_enum_to_pixel_color(color);

    // Bresenham's line algorithm
    int32_t dx = (int32_t)x2 - (int32_t)x1;
    int32_t dy = (int32_t)y2 - (int32_t)y1;
    int32_t abs_dx = (dx < 0) ? -dx : dx;
    int32_t abs_dy = (dy < 0) ? -dy : dy;

    int32_t sx = (x1 < x2) ? 1 : -1;
    int32_t sy = (y1 < y2) ? 1 : -1;
    int32_t err = abs_dx - abs_dy;

    int32_t x = (int32_t)x1;
    int32_t y = (int32_t)y1;

    while (1) {
        if (x >= 0 && x < (int32_t)PIXEL_MATRIX_WIDTH && y >= 0 && y < (int32_t)PIXEL_MATRIX_HEIGHT) {
            uint32_t index = matrix_coord_to_led_index((uint32_t)x, (uint32_t)y);
            if (index < PIXEL_MATRIX_TOTAL) {
                f->pixels[index] = pixel_color;
            }
        }

        if (x == (int32_t)x2 && y == (int32_t)y2) {
            break;
        }

        int32_t e2 = 2 * err;
        if (e2 > -abs_dy) {
            err -= abs_dy;
            x += sx;
        }
        if (e2 < abs_dx) {
            err += abs_dx;
            y += sy;
        }
    }

    return OPRT_OK;
}

/**
 * @brief Draw a filled box (rectangle)
 */
OPERATE_RET board_pixel_draw_box(PIXEL_FRAME_HANDLE_T frame, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                 PIXEL_COLOR_ENUM_E color)
{
    if (frame == NULL) {
        return OPRT_INVALID_PARM;
    }

    pixel_frame_t *f = (pixel_frame_t *)frame;
    if (!f->initialized) {
        return OPRT_INVALID_PARM;
    }

    PIXEL_COLOR_T pixel_color = color_enum_to_pixel_color(color);

    for (uint32_t py = y; py < y + height && py < PIXEL_MATRIX_HEIGHT; py++) {
        for (uint32_t px = x; px < x + width && px < PIXEL_MATRIX_WIDTH; px++) {
            uint32_t index = matrix_coord_to_led_index(px, py);
            if (index < PIXEL_MATRIX_TOTAL) {
                f->pixels[index] = pixel_color;
            }
        }
    }

    return OPRT_OK;
}

/**
 * @brief Draw a circle outline
 */
OPERATE_RET board_pixel_draw_circle(PIXEL_FRAME_HANDLE_T frame, uint32_t center_x, uint32_t center_y, uint32_t radius,
                                    PIXEL_COLOR_ENUM_E color)
{
    if (frame == NULL) {
        return OPRT_INVALID_PARM;
    }

    pixel_frame_t *f = (pixel_frame_t *)frame;
    if (!f->initialized) {
        return OPRT_INVALID_PARM;
    }

    PIXEL_COLOR_T pixel_color = color_enum_to_pixel_color(color);

    // Midpoint circle algorithm
    int32_t x = 0;
    int32_t y = (int32_t)radius;
    int32_t d = 1 - (int32_t)radius;

    while (x <= y) {
        // Draw 8 points of symmetry
        int32_t points[8][2] = {
            {(int32_t)center_x + x, (int32_t)center_y + y}, {(int32_t)center_x - x, (int32_t)center_y + y},
            {(int32_t)center_x + x, (int32_t)center_y - y}, {(int32_t)center_x - x, (int32_t)center_y - y},
            {(int32_t)center_x + y, (int32_t)center_y + x}, {(int32_t)center_x - y, (int32_t)center_y + x},
            {(int32_t)center_x + y, (int32_t)center_y - x}, {(int32_t)center_x - y, (int32_t)center_y - x}};

        for (int i = 0; i < 8; i++) {
            int32_t px = points[i][0];
            int32_t py = points[i][1];
            if (px >= 0 && px < (int32_t)PIXEL_MATRIX_WIDTH && py >= 0 && py < (int32_t)PIXEL_MATRIX_HEIGHT) {
                uint32_t index = matrix_coord_to_led_index((uint32_t)px, (uint32_t)py);
                if (index < PIXEL_MATRIX_TOTAL) {
                    f->pixels[index] = pixel_color;
                }
            }
        }

        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }

    return OPRT_OK;
}

/**
 * @brief Draw a filled circle
 */
OPERATE_RET board_pixel_draw_circle_filled(PIXEL_FRAME_HANDLE_T frame, uint32_t center_x, uint32_t center_y,
                                           uint32_t radius, PIXEL_COLOR_ENUM_E color)
{
    if (frame == NULL) {
        return OPRT_INVALID_PARM;
    }

    pixel_frame_t *f = (pixel_frame_t *)frame;
    if (!f->initialized) {
        return OPRT_INVALID_PARM;
    }

    PIXEL_COLOR_T pixel_color = color_enum_to_pixel_color(color);

    // Draw filled circle by checking distance from center
    for (uint32_t py = 0; py < PIXEL_MATRIX_HEIGHT; py++) {
        for (uint32_t px = 0; px < PIXEL_MATRIX_WIDTH; px++) {
            int32_t dx = (int32_t)px - (int32_t)center_x;
            int32_t dy = (int32_t)py - (int32_t)center_y;
            uint32_t distance_sq = (uint32_t)(dx * dx + dy * dy);
            uint32_t radius_sq = radius * radius;

            if (distance_sq <= radius_sq) {
                uint32_t index = matrix_coord_to_led_index(px, py);
                if (index < PIXEL_MATRIX_TOTAL) {
                    f->pixels[index] = pixel_color;
                }
            }
        }
    }

    return OPRT_OK;
}

/**
 * @brief Render a single character from font
 */
static void render_char_from_font(pixel_frame_t *f, uint32_t x, uint32_t y, char c, PIXEL_COLOR_T color,
                                  const GFXfont *font)
{
    if (font == NULL || c < font->first || c > font->last) {
        return; // Character not in font
    }

    GFXglyph *glyph = &font->glyph[c - font->first];
    uint8_t *bitmap = font->bitmap;

    uint8_t *bitmap_ptr = &bitmap[glyph->bitmapOffset];
    uint8_t bit = 0;
    uint8_t bits = 0;

    int32_t cursor_x = (int32_t)x + glyph->xOffset;
    int32_t cursor_y = (int32_t)y + glyph->yOffset;

    for (uint8_t row = 0; row < glyph->height; row++) {
        for (uint8_t col = 0; col < glyph->width; col++) {
            if (bit == 0) {
                bits = *bitmap_ptr++;
            }

            if (bits & 0x80) {
                int32_t px = cursor_x + col;
                int32_t py = cursor_y + row;

                if (px >= 0 && px < (int32_t)PIXEL_MATRIX_WIDTH && py >= 0 && py < (int32_t)PIXEL_MATRIX_HEIGHT) {
                    uint32_t index = matrix_coord_to_led_index((uint32_t)px, (uint32_t)py);
                    if (index < PIXEL_MATRIX_TOTAL) {
                        f->pixels[index] = color;
                    }
                }
            }

            bits <<= 1;
            bit++;
            if (bit >= 8) {
                bit = 0;
            }
        }
    }
}

/**
 * @brief Add text (English) using font enumeration
 */
OPERATE_RET board_pixel_draw_text(PIXEL_FRAME_HANDLE_T frame, uint32_t x, uint32_t y, const char *text,
                                  PIXEL_COLOR_ENUM_E color, PIXEL_FONT_ENUM_E font)
{
    const GFXfont *font_ptr = font_enum_to_font_ptr(font);
    return board_pixel_draw_text_with_font(frame, x, y, text, color, font_ptr);
}

/**
 * @brief Add text (English) using specified font pointer (advanced usage)
 */
OPERATE_RET board_pixel_draw_text_with_font(PIXEL_FRAME_HANDLE_T frame, uint32_t x, uint32_t y, const char *text,
                                            PIXEL_COLOR_ENUM_E color, const void *font)
{
    if (frame == NULL || text == NULL || font == NULL) {
        return OPRT_INVALID_PARM;
    }

    pixel_frame_t *f = (pixel_frame_t *)frame;
    if (!f->initialized) {
        return OPRT_INVALID_PARM;
    }

    PIXEL_COLOR_T pixel_color = color_enum_to_pixel_color(color);
    const GFXfont *gfx_font = (const GFXfont *)font;

    uint32_t cursor_x = x;
    uint32_t cursor_y = y;

    // Render each character
    for (const char *p = text; *p; p++) {
        char c = *p;

        // Handle newline
        if (c == '\n') {
            cursor_x = x;
            cursor_y += gfx_font->yAdvance;
            continue;
        }

        // Skip characters outside font range
        if (c < gfx_font->first || c > gfx_font->last) {
            continue;
        }

        GFXglyph *glyph = &gfx_font->glyph[c - gfx_font->first];

        // Render the character
        render_char_from_font(f, cursor_x, cursor_y, c, pixel_color, gfx_font);

        // Advance cursor
        cursor_x += glyph->xAdvance;

        // Wrap if exceeds matrix width
        if (cursor_x >= PIXEL_MATRIX_WIDTH) {
            cursor_x = x;
            cursor_y += gfx_font->yAdvance;
        }
    }

    return OPRT_OK;
}

/**
 * @brief Draw a bitmap image at position (x, y)
 */
OPERATE_RET board_pixel_draw_bitmap(PIXEL_FRAME_HANDLE_T frame, uint32_t x, uint32_t y, const uint8_t *bitmap,
                                    uint32_t width, uint32_t height)
{
    if (frame == NULL || bitmap == NULL) {
        return OPRT_INVALID_PARM;
    }

    pixel_frame_t *f = (pixel_frame_t *)frame;
    if (!f->initialized) {
        return OPRT_INVALID_PARM;
    }

    // Bitmap is expected to be in RGB format (3 bytes per pixel), row-major order
    for (uint32_t py = 0; py < height; py++) {
        for (uint32_t px = 0; px < width; px++) {
            uint32_t dst_x = x + px;
            uint32_t dst_y = y + py;

            if (dst_x >= PIXEL_MATRIX_WIDTH || dst_y >= PIXEL_MATRIX_HEIGHT) {
                continue;
            }

            // Get RGB from bitmap (3 bytes per pixel)
            uint32_t src_idx = (py * width + px) * 3;
            uint8_t r = bitmap[src_idx];
            uint8_t g = bitmap[src_idx + 1];
            uint8_t b = bitmap[src_idx + 2];

            // Convert to PIXEL_COLOR_T (note: GRB order swap)
            PIXEL_COLOR_T pixel_color = {0};
            pixel_color.red = (uint32_t)((g * COLOR_RESOLUTION * BRIGHTNESS) / 255);
            pixel_color.green = (uint32_t)((r * COLOR_RESOLUTION * BRIGHTNESS) / 255);
            pixel_color.blue = (uint32_t)((b * COLOR_RESOLUTION * BRIGHTNESS) / 255);
            pixel_color.warm = 0;
            pixel_color.cold = 0;

            uint32_t index = matrix_coord_to_led_index(dst_x, dst_y);
            if (index < PIXEL_MATRIX_TOTAL) {
                f->pixels[index] = pixel_color;
            }
        }
    }

    return OPRT_OK;
}

/**
 * @brief Create a GIF animation handle from bitmap frames
 */
PIXEL_GIF_HANDLE_T board_pixel_gif_create(const uint8_t **frames, uint32_t frame_count, uint32_t width, uint32_t height,
                                          uint32_t delay_ms)
{
    if (frames == NULL || frame_count == 0) {
        return NULL;
    }

    pixel_gif_t *gif = (pixel_gif_t *)tkl_system_malloc(sizeof(pixel_gif_t));
    if (gif == NULL) {
        PR_ERR("Failed to allocate GIF memory");
        return NULL;
    }

    gif->frames = frames;
    gif->frame_count = frame_count;
    gif->current_frame = 0;
    gif->width = width;
    gif->height = height;
    gif->delay_ms = delay_ms;
    gif->last_update_time = 0;

    return (PIXEL_GIF_HANDLE_T)gif;
}

/**
 * @brief Draw current frame of GIF at position (x, y) and advance to next frame
 */
OPERATE_RET board_pixel_draw_gif(PIXEL_FRAME_HANDLE_T frame, PIXEL_GIF_HANDLE_T gif, uint32_t x, uint32_t y)
{
    if (frame == NULL || gif == NULL) {
        return OPRT_INVALID_PARM;
    }

    pixel_gif_t *g = (pixel_gif_t *)gif;
    if (g->current_frame >= g->frame_count) {
        return OPRT_INVALID_PARM;
    }

    // Draw current frame
    const uint8_t *current_frame_data = g->frames[g->current_frame];
    OPERATE_RET rt = board_pixel_draw_bitmap(frame, x, y, current_frame_data, g->width, g->height);
    if (OPRT_OK != rt) {
        return rt;
    }

    // Advance to next frame (simple implementation - caller should manage timing)
    g->current_frame++;
    if (g->current_frame >= g->frame_count) {
        g->current_frame = 0; // Loop
    }

    return OPRT_OK;
}

/**
 * @brief Reset GIF animation to first frame
 */
OPERATE_RET board_pixel_gif_reset(PIXEL_GIF_HANDLE_T gif)
{
    if (gif == NULL) {
        return OPRT_INVALID_PARM;
    }

    pixel_gif_t *g = (pixel_gif_t *)gif;
    g->current_frame = 0;

    return OPRT_OK;
}

/**
 * @brief Destroy GIF handle and free resources
 */
OPERATE_RET board_pixel_gif_destroy(PIXEL_GIF_HANDLE_T gif)
{
    if (gif == NULL) {
        return OPRT_OK;
    }

    pixel_gif_t *g = (pixel_gif_t *)gif;
    tkl_system_free(g);

    return OPRT_OK;
}

/**
 * @brief Convert HSV color space to RGB
 */
void board_pixel_hsv_to_rgb(float hue, float saturation, float value, uint32_t *r, uint32_t *g, uint32_t *b)
{
    if (r == NULL || g == NULL || b == NULL) {
        return;
    }

    // Normalize hue to 0-360
    while (hue < 0.0f)
        hue += 360.0f;
    while (hue >= 360.0f)
        hue -= 360.0f;

    float h = hue / 60.0f;
    float c = value * saturation;
    float x = c * (1.0f - fabsf(fmodf(h, 2.0f) - 1.0f));
    float m = value - c;

    float rf, gf, bf;
    if (h < 1.0f) {
        rf = c;
        gf = x;
        bf = 0;
    } else if (h < 2.0f) {
        rf = x;
        gf = c;
        bf = 0;
    } else if (h < 3.0f) {
        rf = 0;
        gf = c;
        bf = x;
    } else if (h < 4.0f) {
        rf = 0;
        gf = x;
        bf = c;
    } else if (h < 5.0f) {
        rf = x;
        gf = 0;
        bf = c;
    } else {
        rf = c;
        gf = 0;
        bf = x;
    }

    *r = (uint32_t)((rf + m) * 255.0f);
    *g = (uint32_t)((gf + m) * 255.0f);
    *b = (uint32_t)((bf + m) * 255.0f);
}

/**
 * @brief Convert HSV color space to PIXEL_COLOR_T
 */
PIXEL_COLOR_T board_pixel_hsv_to_pixel_color(float hue, float saturation, float value, float brightness,
                                             uint32_t color_resolution)
{
    PIXEL_COLOR_T pixel_color = {0};
    uint32_t r, g, b;

    board_pixel_hsv_to_rgb(hue, saturation, value, &r, &g, &b);

    // Convert RGB (0-255) to COLOR_RESOLUTION scale with brightness
    // Note: LED hardware expects GRB order, so we swap red and green
    pixel_color.red = (uint32_t)((g * color_resolution * brightness) / 255);   // Red channel gets green data
    pixel_color.green = (uint32_t)((r * color_resolution * brightness) / 255); // Green channel gets red data
    pixel_color.blue = (uint32_t)((b * color_resolution * brightness) / 255);  // Blue stays the same
    pixel_color.warm = 0;
    pixel_color.cold = 0;

    return pixel_color;
}

/**
 * @brief Convert 2D matrix coordinates to LED index
 */
uint32_t board_pixel_matrix_coord_to_led_index(uint32_t x, uint32_t y)
{
    return matrix_coord_to_led_index(x, y);
}

/**
 * @brief Get pixel device handle (for advanced usage)
 */
OPERATE_RET board_pixel_get_handle(PIXEL_HANDLE_T *handle)
{
    if (handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = init_pixel_device();
    if (OPRT_OK != rt) {
        return rt;
    }

    *handle = g_pixel_handle;
    return OPRT_OK;
}
