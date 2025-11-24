/**
 * @file board_pixel_api.h
 * @brief Header file for simple LED pixel drawing API
 *
 * This header provides a simple, single-file API for drawing on the LED pixel matrix.
 * It includes frame management, drawing primitives (line, box, circle), text rendering,
 * bitmap support, and GIF animation support.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BOARD_PIXEL_API_H__
#define __BOARD_PIXEL_API_H__

#include "tuya_cloud_types.h"
#include "tdl_pixel_dev_manage.h"
#include "tdl_pixel_color_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define PIXEL_MATRIX_WIDTH  32
#define PIXEL_MATRIX_HEIGHT 32
#define PIXEL_MATRIX_TOTAL  (PIXEL_MATRIX_WIDTH * PIXEL_MATRIX_HEIGHT + 3)

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Color enumeration with 32 predefined colors
 */
typedef enum {
    PIXEL_COLOR_BLACK = 0,
    PIXEL_COLOR_WHITE,
    PIXEL_COLOR_RED,
    PIXEL_COLOR_GREEN,
    PIXEL_COLOR_BLUE,
    PIXEL_COLOR_YELLOW,
    PIXEL_COLOR_CYAN,
    PIXEL_COLOR_MAGENTA,
    PIXEL_COLOR_ORANGE,
    PIXEL_COLOR_PURPLE,
    PIXEL_COLOR_PINK,
    PIXEL_COLOR_LIME,
    PIXEL_COLOR_NAVY,
    PIXEL_COLOR_MAROON,
    PIXEL_COLOR_OLIVE,
    PIXEL_COLOR_TEAL,
    PIXEL_COLOR_SILVER,
    PIXEL_COLOR_GRAY,
    PIXEL_COLOR_DARK_RED,
    PIXEL_COLOR_DARK_GREEN,
    PIXEL_COLOR_DARK_BLUE,
    PIXEL_COLOR_DARK_YELLOW,
    PIXEL_COLOR_DARK_CYAN,
    PIXEL_COLOR_DARK_MAGENTA,
    PIXEL_COLOR_LIGHT_RED,
    PIXEL_COLOR_LIGHT_GREEN,
    PIXEL_COLOR_LIGHT_BLUE,
    PIXEL_COLOR_LIGHT_YELLOW,
    PIXEL_COLOR_LIGHT_CYAN,
    PIXEL_COLOR_LIGHT_MAGENTA,
    PIXEL_COLOR_GOLD,
    PIXEL_COLOR_VIOLET,
    PIXEL_COLOR_COUNT // Total count of colors
} PIXEL_COLOR_ENUM_E;

/**
 * @brief Frame handle (opaque pointer)
 */
typedef void *PIXEL_FRAME_HANDLE_T;

/**
 * @brief GIF handle (opaque pointer)
 */
typedef void *PIXEL_GIF_HANDLE_T;

/**
 * @brief Font enumeration for easy font selection
 */
typedef enum {
    PIXEL_FONT_PICOPIXEL = 0,
    PIXEL_FONT_FREEMONO_9PT,
    PIXEL_FONT_FREEMONO_12PT,
    PIXEL_FONT_FREEMONO_18PT,
    PIXEL_FONT_FREEMONO_24PT,
    PIXEL_FONT_FREEMONO_BOLD_9PT,
    PIXEL_FONT_FREEMONO_BOLD_12PT,
    PIXEL_FONT_FREEMONO_BOLD_18PT,
    PIXEL_FONT_FREEMONO_BOLD_24PT,
    PIXEL_FONT_COUNT // Total count of fonts
} PIXEL_FONT_ENUM_E;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Create a new frame buffer
 *
 * @return Frame handle on success, NULL on failure
 */
PIXEL_FRAME_HANDLE_T board_pixel_frame_create(void);

/**
 * @brief Render frame to the LED matrix
 *
 * @param[in] frame Frame handle
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_frame_render(PIXEL_FRAME_HANDLE_T frame);

/**
 * @brief Destroy frame and free resources
 *
 * @param[in] frame Frame handle (can be NULL)
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_frame_destroy(PIXEL_FRAME_HANDLE_T frame);

/**
 * @brief Clear frame (set all pixels to black)
 *
 * @param[in] frame Frame handle
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_frame_clear(PIXEL_FRAME_HANDLE_T frame);

/**
 * @brief Draw a line from (x1, y1) to (x2, y2)
 *
 * @param[in] frame Frame handle
 * @param[in] x1 Start X coordinate (0-31)
 * @param[in] y1 Start Y coordinate (0-31)
 * @param[in] x2 End X coordinate (0-31)
 * @param[in] y2 End Y coordinate (0-31)
 * @param[in] color Color enumeration
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_draw_line(PIXEL_FRAME_HANDLE_T frame, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2,
                                  PIXEL_COLOR_ENUM_E color);

/**
 * @brief Draw a filled box (rectangle)
 *
 * @param[in] frame Frame handle
 * @param[in] x Top-left X coordinate (0-31)
 * @param[in] y Top-left Y coordinate (0-31)
 * @param[in] width Box width
 * @param[in] height Box height
 * @param[in] color Color enumeration
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_draw_box(PIXEL_FRAME_HANDLE_T frame, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                 PIXEL_COLOR_ENUM_E color);

/**
 * @brief Draw a circle
 *
 * @param[in] frame Frame handle
 * @param[in] center_x Center X coordinate (0-31)
 * @param[in] center_y Center Y coordinate (0-31)
 * @param[in] radius Circle radius
 * @param[in] color Color enumeration
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_draw_circle(PIXEL_FRAME_HANDLE_T frame, uint32_t center_x, uint32_t center_y, uint32_t radius,
                                    PIXEL_COLOR_ENUM_E color);

/**
 * @brief Draw a filled circle
 *
 * @param[in] frame Frame handle
 * @param[in] center_x Center X coordinate (0-31)
 * @param[in] center_y Center Y coordinate (0-31)
 * @param[in] radius Circle radius
 * @param[in] color Color enumeration
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_draw_circle_filled(PIXEL_FRAME_HANDLE_T frame, uint32_t center_x, uint32_t center_y,
                                           uint32_t radius, PIXEL_COLOR_ENUM_E color);

/**
 * @brief Set a single pixel at (x, y)
 *
 * @param[in] frame Frame handle
 * @param[in] x X coordinate (0-31)
 * @param[in] y Y coordinate (0-31)
 * @param[in] color Color enumeration
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_set_pixel(PIXEL_FRAME_HANDLE_T frame, uint32_t x, uint32_t y, PIXEL_COLOR_ENUM_E color);

/**
 * @brief Add text (English) using font enumeration
 *
 * @param[in] frame Frame handle
 * @param[in] x Start X coordinate (0-31)
 * @param[in] y Start Y coordinate (0-31)
 * @param[in] text Text string to display
 * @param[in] color Color enumeration
 * @param[in] font Font enumeration
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_draw_text(PIXEL_FRAME_HANDLE_T frame, uint32_t x, uint32_t y, const char *text,
                                  PIXEL_COLOR_ENUM_E color, PIXEL_FONT_ENUM_E font);

/**
 * @brief Add text (English) using specified font pointer (advanced usage)
 *
 * @param[in] frame Frame handle
 * @param[in] x Start X coordinate (0-31)
 * @param[in] y Start Y coordinate (0-31)
 * @param[in] text Text string to display
 * @param[in] color Color enumeration
 * @param[in] font Font structure pointer (from font header files)
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_draw_text_with_font(PIXEL_FRAME_HANDLE_T frame, uint32_t x, uint32_t y, const char *text,
                                            PIXEL_COLOR_ENUM_E color, const void *font);

/**
 * @brief Draw a bitmap image at position (x, y)
 *
 * @param[in] frame Frame handle
 * @param[in] x Top-left X coordinate (0-31)
 * @param[in] y Top-left Y coordinate (0-31)
 * @param[in] bitmap Bitmap data array (RGB format, row-major order)
 * @param[in] width Bitmap width
 * @param[in] height Bitmap height
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_draw_bitmap(PIXEL_FRAME_HANDLE_T frame, uint32_t x, uint32_t y, const uint8_t *bitmap,
                                    uint32_t width, uint32_t height);

/**
 * @brief Create a GIF animation handle from bitmap frames
 *
 * @param[in] frames Array of bitmap data (each frame is RGB format, row-major order)
 * @param[in] frame_count Number of frames
 * @param[in] width Frame width
 * @param[in] height Frame height
 * @param[in] delay_ms Delay between frames in milliseconds
 * @return GIF handle on success, NULL on failure
 */
PIXEL_GIF_HANDLE_T board_pixel_gif_create(const uint8_t **frames, uint32_t frame_count, uint32_t width, uint32_t height,
                                          uint32_t delay_ms);

/**
 * @brief Draw current frame of GIF at position (x, y) and advance to next frame
 *
 * @param[in] frame Frame handle to draw into
 * @param[in] gif GIF handle
 * @param[in] x Top-left X coordinate (0-31)
 * @param[in] y Top-left Y coordinate (0-31)
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_draw_gif(PIXEL_FRAME_HANDLE_T frame, PIXEL_GIF_HANDLE_T gif, uint32_t x, uint32_t y);

/**
 * @brief Reset GIF animation to first frame
 *
 * @param[in] gif GIF handle
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_gif_reset(PIXEL_GIF_HANDLE_T gif);

/**
 * @brief Destroy GIF handle and free resources
 *
 * @param[in] gif GIF handle (can be NULL)
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_gif_destroy(PIXEL_GIF_HANDLE_T gif);

/**
 * @brief Convert HSV color space to RGB
 *
 * @param[in] hue Hue in degrees (0-360)
 * @param[in] saturation Saturation (0.0-1.0)
 * @param[in] value Value/brightness (0.0-1.0)
 * @param[out] r Output red component (0-255)
 * @param[out] g Output green component (0-255)
 * @param[out] b Output blue component (0-255)
 */
void board_pixel_hsv_to_rgb(float hue, float saturation, float value, uint32_t *r, uint32_t *g, uint32_t *b);

/**
 * @brief Convert HSV color space to PIXEL_COLOR_T
 *
 * @param[in] hue Hue in degrees (0-360)
 * @param[in] saturation Saturation (0.0-1.0)
 * @param[in] value Value/brightness (0.0-1.0)
 * @param[in] brightness Brightness multiplier (0.0-1.0, typically 0.05-0.1)
 * @param[in] color_resolution Color resolution (typically 1000)
 * @return PIXEL_COLOR_T structure with converted color
 */
PIXEL_COLOR_T board_pixel_hsv_to_pixel_color(float hue, float saturation, float value, float brightness,
                                             uint32_t color_resolution);

/**
 * @brief Convert 2D matrix coordinates to LED index
 *
 * @param[in] x X coordinate (0-31)
 * @param[in] y Y coordinate (0-31)
 * @return LED index, or PIXEL_MATRIX_TOTAL if out of bounds
 */
uint32_t board_pixel_matrix_coord_to_led_index(uint32_t x, uint32_t y);

/**
 * @brief Get pixel device handle (for advanced usage)
 * Note: This function will initialize the pixel device if not already initialized
 *
 * @param[out] handle Pointer to receive the pixel handle
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET board_pixel_get_handle(PIXEL_HANDLE_T *handle);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_PIXEL_API_H__ */
