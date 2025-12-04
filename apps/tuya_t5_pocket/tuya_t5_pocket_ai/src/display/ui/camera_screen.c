/**
 * @file camera_screen.c
 * @brief Implementation of the camera screen with binary conversion control
 *
 * This file contains the implementation of the camera screen which displays
 * camera feed on the left side and binary conversion settings on the right side.
 *
 * @copyright Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 */

#include "camera_screen.h"
#include <stdio.h>
#include <string.h>

#ifdef ENABLE_LVGL_HARDWARE
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "board_com_api.h"

#include "tdl_display_manage.h"
#include "tdl_display_draw.h"
#include "tdl_camera_manage.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define CAMERA_WIDTH  240
#define CAMERA_HEIGHT 240 // Camera captures 240x240
#define CAMERA_FPS    15

#define CAMERA_AREA_WIDTH  240                                 // Left side for camera (240 pixels wide)
#define CAMERA_AREA_HEIGHT 168                                 // Display area height (crop from 240)
#define CAMERA_CROP_OFFSET 36                                  // Crop offset: (240-168)/2 = 36, display middle 168 rows
#define INFO_AREA_X        240                                 // Right side starts at x=240
#define INFO_AREA_WIDTH    (AI_PET_SCREEN_WIDTH - INFO_AREA_X) // Right side width (384-240=144)
#define INFO_AREA_HEIGHT   168                                 // Info area height to match camera

#define THRESHOLD_STEP 4   // Threshold adjustment step
#define THRESHOLD_MIN  0   // Minimum threshold
#define THRESHOLD_MAX  255 // Maximum threshold

#ifndef CAMERA_NAME
#define CAMERA_NAME "camera"
#endif

#ifndef DISPLAY_NAME
#define DISPLAY_NAME "display"
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
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

typedef struct {
    BINARY_METHOD_E method;
    uint8_t fixed_threshold;
} BINARY_CONFIG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static lv_obj_t *ui_camera_screen;
static lv_obj_t *camera_canvas;   // Canvas for camera display
static lv_obj_t *method_label;    // Binary method label
static lv_obj_t *threshold_label; // Threshold value label
static lv_obj_t *status_label;    // Camera status label
static lv_timer_t *update_timer;  // Timer for updating display

#ifdef ENABLE_LVGL_HARDWARE
static uint8_t *canvas_buffer = NULL; // Canvas buffer for monochrome image
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb = NULL;
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb_1 = NULL;
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb_2 = NULL;

// YUV422 raw data buffers (double buffering for camera input)
static uint8_t *sg_yuv422_buffer_1 = NULL;     // YUV422 buffer 1 (240x240x2 bytes)
static uint8_t *sg_yuv422_buffer_2 = NULL;     // YUV422 buffer 2 (240x240x2 bytes)
static uint8_t *sg_yuv422_write_buffer = NULL; // Current write buffer pointer

static TDL_CAMERA_HANDLE_T sg_tdl_camera_hdl = NULL;
static bool camera_running = false;
static volatile bool frame_ready = false;       // Flag indicating new frame is ready
static volatile uint8_t write_buffer_index = 0; // Buffer being written by camera (0 or 1)
static volatile uint8_t read_buffer_index = 0;  // Buffer being read for display (0 or 1)
static MUTEX_HANDLE sg_buffer_mutex = NULL;     // Mutex to protect buffer access

static BINARY_CONFIG_T sg_binary_config = {
    .method = BINARY_METHOD_FIXED,
    .fixed_threshold = 128,
};

// Calculated threshold for adaptive and otsu methods
static uint8_t sg_calculated_threshold = 128;
#endif

Screen_t camera_screen = {
    .init = camera_screen_init,
    .deinit = camera_screen_deinit,
    .screen_obj = &ui_camera_screen,
    .name = "camera",
};

/***********************************************************
********************function declaration********************
***********************************************************/
static void keyboard_event_cb(lv_event_t *e);
static void update_info_display(void);
static void update_timer_cb(lv_timer_t *timer);
static OPERATE_RET camera_init(void);
static OPERATE_RET camera_start(void);
static void camera_stop(void);
static int yuv422_to_binary_with_config(uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                        int dst_width, int dst_height, BINARY_CONFIG_T *config);
/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Get method name string
 */
static const char *get_method_name(BINARY_METHOD_E method)
{
#ifdef ENABLE_LVGL_HARDWARE
    switch (method) {
    case BINARY_METHOD_FIXED:
        return "Fixed";
    case BINARY_METHOD_ADAPTIVE:
        return "Adaptive";
    case BINARY_METHOD_OTSU:
        return "Otsu";
    case BINARY_METHOD_BAYER4_DITHER:
        return "Bayer8";
    case BINARY_METHOD_BAYER8_DITHER:
        return "Bayer4";
    case BINARY_METHOD_BAYER16_DITHER:
        return "Bayer16";
    case BINARY_METHOD_FLOYD_STEINBERG:
        return "Floyd-Steinberg";
    case BINARY_METHOD_STUCKI:
        return "Stucki";
    case BINARY_METHOD_JARVIS:
        return "Jarvis";
    default:
        return "Unknown";
    }
#else
    return "N/A";
#endif
}

/**
 * @brief Update info area display
 */
static void update_info_display(void)
{
    char buf[64];

#ifdef ENABLE_LVGL_HARDWARE
    // Update method label
    snprintf(buf, sizeof(buf), "Method:\n%s", get_method_name(sg_binary_config.method));
    lv_label_set_text(method_label, buf);

    // Update threshold label based on method
    if (sg_binary_config.method == BINARY_METHOD_BAYER8_DITHER ||
        sg_binary_config.method == BINARY_METHOD_BAYER4_DITHER ||
        sg_binary_config.method == BINARY_METHOD_BAYER16_DITHER ||
        sg_binary_config.method == BINARY_METHOD_FLOYD_STEINBERG || sg_binary_config.method == BINARY_METHOD_STUCKI ||
        sg_binary_config.method == BINARY_METHOD_JARVIS) {
        snprintf(buf, sizeof(buf), "Threshold:\nN/A");
    } else {
        // For adaptive and otsu, show calculated threshold
        snprintf(buf, sizeof(buf), "Threshold:\n%d", sg_calculated_threshold);
    }
    lv_label_set_text(threshold_label, buf);

    // Update status
    snprintf(buf, sizeof(buf), "Status:\n%s", camera_running ? "Running" : "Stopped");
    lv_label_set_text(status_label, buf);
#else
    lv_label_set_text(method_label, "Method:\nN/A");
    lv_label_set_text(threshold_label, "Threshold:\nN/A");
    lv_label_set_text(status_label, "Status:\nDisabled");
#endif
}

/**
 * @brief Timer callback for updating display (runs in LVGL context)
 * @note Now handles image processing (YUV422 -> binary conversion)
 */
static void update_timer_cb(lv_timer_t *timer)
{
    (void)timer;

#ifdef ENABLE_LVGL_HARDWARE

    // Check if new frame is ready and process it
    if (frame_ready && canvas_buffer && camera_canvas && sg_buffer_mutex) {
        // Lock mutex to safely access buffer indices and data
        tal_mutex_lock(sg_buffer_mutex);

        // Get the YUV422 buffer with latest frame data
        uint8_t *yuv422_source = (write_buffer_index == 0) ? sg_yuv422_buffer_1 : sg_yuv422_buffer_2;
        read_buffer_index = write_buffer_index;
        frame_ready = false; // Clear flag

        tal_mutex_unlock(sg_buffer_mutex);

        // Process YUV422 data to binary in LVGL timer context (safe for LVGL operations)
        // Get output buffer (toggle between two buffers for double buffering)
        TDL_DISP_FRAME_BUFF_T *output_fb = (read_buffer_index == 0) ? sg_p_display_fb_1 : sg_p_display_fb_2;

        // Convert YUV422 (240x240) to binary (240x168 cropped) with rotation
        yuv422_to_binary_with_config(yuv422_source, CAMERA_WIDTH, CAMERA_HEIGHT, output_fb->frame, CAMERA_AREA_WIDTH,
                                     CAMERA_AREA_HEIGHT, &sg_binary_config);

        // Copy processed binary data to canvas buffer
        // For LVGL I1 format: palette (8 bytes) + bitmap data
        uint32_t bitmap_size = (CAMERA_AREA_WIDTH + 7) / 8 * CAMERA_AREA_HEIGHT;
        memcpy(canvas_buffer + 8, output_fb->frame, bitmap_size);

        // Invalidate canvas to trigger redraw
        lv_obj_invalidate(camera_canvas);
    }
#endif

    update_info_display();
}

#ifdef ENABLE_LVGL_HARDWARE

/**
 * @brief 2x2 Bayer matrix for 4-level grayscale dithering (threshold 0-3)
 */
static const uint8_t bayer_2x2[2][2] = {{0, 2}, {3, 1}};

/**
 * @brief 3x3 Bayer matrix for 8-level grayscale dithering (threshold 0-8)
 */
static const uint8_t bayer_3x3[3][3] = {{0, 7, 3}, {6, 4, 2}, {1, 5, 8}};

/**
 * @brief 4x4 Bayer matrix for 16-level grayscale dithering (threshold 0-15)
 */
static const uint8_t bayer_4x4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

/**
 * @brief Convert YUV422 to binary with 4-level Bayer dithering (2x2 matrix)
 * @param yuv422_data Source YUV422 data (240x240)
 * @param src_width Source width (240)
 * @param src_height Source height (240)
 * @param binary_data Output binary data buffer
 * @param dst_width Destination width (240)
 * @param dst_height Destination height (168, cropped from middle)
 * @note Rotation: counter-clockwise 90 degrees, then crop to display area
 */
static int yuv422_to_bayer4_dither_crop(uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                        int dst_width, int dst_height)
{
    if (!yuv422_data || !binary_data || src_width <= 0 || src_height <= 0) {
        return -1;
    }

    int binary_stride = (dst_width + 7) / 8;
    memset(binary_data, 0x00, binary_stride * dst_height);
    int crop_offset = (src_width - dst_height) / 2;

    // 4-level Bayer dithering with rotation (2x2 matrix)
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

            // Map luminance to 0-3 range for 4-level grayscale
            float normalized = luminance * 3.0f / 255.0f;

            // Get Bayer threshold from 2x2 matrix (0-3)
            uint8_t threshold = bayer_2x2[dst_y % 2][dst_x % 2];

            // Apply dithering: if normalized luminance >= threshold, set pixel to white
            if (normalized >= threshold) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }
        }
    }
    return 0;
}

/**
 * @brief Convert YUV422 to binary with 16-level Bayer dithering (4x4 matrix)
 * @param yuv422_data Source YUV422 data (240x240)
 * @param src_width Source width (240)
 * @param src_height Source height (240)
 * @param binary_data Output binary data buffer
 * @param dst_width Destination width (240)
 * @param dst_height Destination height (168, cropped from middle)
 * @note Rotation: counter-clockwise 90 degrees, then crop to display area
 */
static int yuv422_to_bayer16_dither_crop(uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                         int dst_width, int dst_height)
{
    if (!yuv422_data || !binary_data || src_width <= 0 || src_height <= 0) {
        return -1;
    }

    int binary_stride = (dst_width + 7) / 8;
    memset(binary_data, 0x00, binary_stride * dst_height);
    int crop_offset = (src_width - dst_height) / 2;

    // 16-level Bayer dithering with rotation (4x4 matrix)
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

            // Map luminance to 0-15 range for 16-level grayscale
            float normalized = luminance * 15.0f / 255.0f;

            // Get Bayer threshold from 4x4 matrix (0-15)
            uint8_t threshold = bayer_4x4[dst_y & 0x03][dst_x & 0x03]; // Optimized: & 0x03 == % 4

            // Apply dithering: if normalized luminance >= threshold, set pixel to white
            if (normalized >= threshold) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }
        }
    }

    return 0;
}

/**
 * @brief Convert YUV422 to binary with 8-level Bayer dithering (3x3 matrix)
 * @param yuv422_data Source YUV422 data (240x240)
 * @param src_width Source width (240)
 * @param src_height Source height (240)
 * @param binary_data Output binary data buffer
 * @param dst_width Destination width (240)
 * @param dst_height Destination height (168, cropped from middle)
 * @note Rotation: counter-clockwise 90 degrees, then crop to display area
 */
static int yuv422_to_bayer8_dither_crop(uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                        int dst_width, int dst_height)
{
    if (!yuv422_data || !binary_data || src_width <= 0 || src_height <= 0) {
        return -1;
    }

    int binary_stride = (dst_width + 7) / 8;
    memset(binary_data, 0x00, binary_stride * dst_height);
    int crop_offset = (src_width - dst_height) / 2;

    // 8-level Bayer dithering with rotation
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

            // Map luminance to 0-8 range
            float normalized = luminance * 8.0f / 255.0f;

            // Get Bayer threshold (0-8)
            uint8_t threshold = bayer_3x3[dst_y % 3][dst_x % 3];

            // Apply dithering: if normalized luminance >= threshold, set pixel to white
            if (normalized >= threshold) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }
        }
    }

    return 0;
}

/**
 * @brief Convert YUV422 to binary using threshold with crop and 270 degree rotation (counter-clockwise 90)
 * @param yuv422_data Source YUV422 data (240x240)
 * @param src_width Source width (240)
 * @param src_height Source height (240)
 * @param binary_data Output binary data buffer
 * @param dst_width Destination width (240)
 * @param dst_height Destination height (168, cropped from middle)
 * @param threshold Binary threshold value
 * @note Rotation: counter-clockwise 90 degrees, then crop to display area
 */
static int yuv422_to_binary_crop(uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                 int dst_width, int dst_height, uint8_t threshold)
{
    if (!yuv422_data || !binary_data || src_width <= 0 || src_height <= 0) {
        return -1;
    }

    int binary_stride = (dst_width + 7) / 8;
    memset(binary_data, 0x00, binary_stride * dst_height); // Initialize to 0 (all black)

    // For counter-clockwise 90 degree rotation:
    // Original point (x, y) -> Rotated point (y, width-1-x)
    // Since we need to crop from 240x240 to 240x168, we crop from the source
    int crop_offset = (src_width - dst_height) / 2; // 36 pixels offset from left/right

    // Process with 90 degree counter-clockwise rotation
    // IMPORTANT: LVGL I1 format - bit=1: palette[1], bit=0: palette[0]
    // We set: palette[0]=black, palette[1]=white
    // So: bright pixel (luminance >= threshold) -> bit=1 -> white
    //     dark pixel (luminance < threshold) -> bit=0 -> black
    // LVGL I1 uses MSB first: bit 7 = leftmost pixel in byte
    for (int dst_y = 0; dst_y < dst_height; dst_y++) {
        int row_offset = dst_y * binary_stride;

        for (int dst_x = 0; dst_x < dst_width; dst_x++) {
            // Reverse transform: find source pixel for this destination pixel
            // Counter-clockwise 90: dst(x,y) comes from src(src_height-1-y, x)
            // After crop adjustment
            int src_x = dst_y + crop_offset;    // Map dst_y to cropped src_x range
            int src_y = src_height - 1 - dst_x; // Reverse mapping for y

            // Bounds check
            if (src_x < 0 || src_x >= src_width || src_y < 0 || src_y >= src_height) {
                continue;
            }

            int yuv_index = src_y * src_width * 2 + src_x * 2 + 1; // UYVY format
            uint8_t luminance = yuv422_data[yuv_index];

            // LVGL I1 format: MSB first (bit 7 = leftmost pixel, bit 0 = rightmost)
            // luminance >= threshold -> white pixel -> set bit to 1
            if (luminance >= threshold) {
                int byte_index = row_offset + (dst_x >> 3); // dst_x / 8
                int bit_position = 7 - (dst_x & 0x07);      // MSB first: 7,6,5,4,3,2,1,0
                binary_data[byte_index] |= (1 << bit_position);
            }
        }
    }

    return 0;
}

/**
 * @brief Convert YUV422 to binary with Floyd-Steinberg error diffusion
 * @param yuv422_data Source YUV422 data (240x240)
 * @param src_width Source width (240)
 * @param src_height Source height (240)
 * @param binary_data Output binary data buffer
 * @param dst_width Destination width (240)
 * @param dst_height Destination height (168, cropped from middle)
 * @note Rotation: counter-clockwise 90 degrees, then crop to display area
 * @note Floyd-Steinberg: [    *  7/16]
 *                        [3/16 5/16 1/16]
 */
static int yuv422_to_floyd_steinberg_crop(uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                          int dst_width, int dst_height)
{
    if (!yuv422_data || !binary_data || src_width <= 0 || src_height <= 0) {
        return -1;
    }

    int binary_stride = (dst_width + 7) / 8;
    memset(binary_data, 0x00, binary_stride * dst_height);
    int crop_offset = (src_width - dst_height) / 2;

    // Allocate error buffer (2 rows for current and next row)
    int16_t *error_buffer = (int16_t *)tal_malloc((dst_width + 2) * 2 * sizeof(int16_t));
    if (!error_buffer) {
        return -1;
    }
    memset(error_buffer, 0, (dst_width + 2) * 2 * sizeof(int16_t));

    int16_t *curr_row = error_buffer + 1;             // Current row with padding
    int16_t *next_row = error_buffer + dst_width + 3; // Next row

    // Process with rotation and error diffusion
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

            // Clamp to valid range
            if (luminance < 0)
                luminance = 0;
            if (luminance > 255)
                luminance = 255;

            // Determine output pixel (threshold at 128)
            uint8_t new_pixel = (luminance >= 128) ? 255 : 0;
            int16_t error = luminance - new_pixel;

            // Set pixel in output
            if (new_pixel == 255) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }

            // Floyd-Steinberg error diffusion
            // Right: 7/16
            if (dst_x < dst_width - 1) {
                curr_row[dst_x + 1] += (error * 7) >> 4;
            }
            // Bottom-left: 3/16
            if (dst_x > 0) {
                next_row[dst_x - 1] += (error * 3) >> 4;
            }
            // Bottom: 5/16
            next_row[dst_x] += (error * 5) >> 4;
            // Bottom-right: 1/16
            if (dst_x < dst_width - 1) {
                next_row[dst_x + 1] += error >> 4;
            }
        }

        // Swap rows for next iteration
        int16_t *temp = curr_row;
        curr_row = next_row;
        next_row = temp;
        memset(next_row - 1, 0, (dst_width + 2) * sizeof(int16_t));
    }

    tal_free(error_buffer);
    return 0;
}

/**
 * @brief Convert YUV422 to binary with Stucki error diffusion
 * @param yuv422_data Source YUV422 data (240x240)
 * @param src_width Source width (240)
 * @param src_height Source height (240)
 * @param binary_data Output binary data buffer
 * @param dst_width Destination width (240)
 * @param dst_height Destination height (168, cropped from middle)
 * @note Rotation: counter-clockwise 90 degrees, then crop to display area
 * @note Stucki: [        *    8/42  4/42]
 *               [2/42  4/42  8/42  4/42  2/42]
 *               [1/42  2/42  4/42  2/42  1/42]
 */
static int yuv422_to_stucki_crop(uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                 int dst_width, int dst_height)
{
    if (!yuv422_data || !binary_data || src_width <= 0 || src_height <= 0) {
        return -1;
    }

    int binary_stride = (dst_width + 7) / 8;
    memset(binary_data, 0x00, binary_stride * dst_height);
    int crop_offset = (src_width - dst_height) / 2;

    // Allocate error buffer (3 rows: current, next, next+1)
    int16_t *error_buffer = (int16_t *)tal_malloc((dst_width + 4) * 3 * sizeof(int16_t));
    if (!error_buffer) {
        return -1;
    }
    memset(error_buffer, 0, (dst_width + 4) * 3 * sizeof(int16_t));

    int16_t *curr_row = error_buffer + 2;
    int16_t *next_row1 = error_buffer + dst_width + 6;
    int16_t *next_row2 = error_buffer + (dst_width + 4) * 2 + 2;

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

            if (new_pixel == 255) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }

            // Stucki error diffusion (divisor: 42)
            // Current row
            if (dst_x < dst_width - 1)
                curr_row[dst_x + 1] += (error * 8) / 42;
            if (dst_x < dst_width - 2)
                curr_row[dst_x + 2] += (error * 4) / 42;

            // Next row (1)
            if (dst_x > 1)
                next_row1[dst_x - 2] += (error * 2) / 42;
            if (dst_x > 0)
                next_row1[dst_x - 1] += (error * 4) / 42;
            next_row1[dst_x] += (error * 8) / 42;
            if (dst_x < dst_width - 1)
                next_row1[dst_x + 1] += (error * 4) / 42;
            if (dst_x < dst_width - 2)
                next_row1[dst_x + 2] += (error * 2) / 42;

            // Next row (2)
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

        // Rotate rows
        int16_t *temp = curr_row;
        curr_row = next_row1;
        next_row1 = next_row2;
        next_row2 = temp;
        memset(next_row2 - 2, 0, (dst_width + 4) * sizeof(int16_t));
    }

    tal_free(error_buffer);
    return 0;
}

/**
 * @brief Convert YUV422 to binary with Jarvis-Judice-Ninke error diffusion
 * @param yuv422_data Source YUV422 data (240x240)
 * @param src_width Source width (240)
 * @param src_height Source height (240)
 * @param binary_data Output binary data buffer
 * @param dst_width Destination width (240)
 * @param dst_height Destination height (168, cropped from middle)
 * @note Rotation: counter-clockwise 90 degrees, then crop to display area
 * @note Jarvis: [        *    7/48  5/48]
 *               [3/48  5/48  7/48  5/48  3/48]
 *               [1/48  3/48  5/48  3/48  1/48]
 */
static int yuv422_to_jarvis_crop(uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                 int dst_width, int dst_height)
{
    if (!yuv422_data || !binary_data || src_width <= 0 || src_height <= 0) {
        return -1;
    }

    int binary_stride = (dst_width + 7) / 8;
    memset(binary_data, 0x00, binary_stride * dst_height);
    int crop_offset = (src_width - dst_height) / 2;

    // Allocate error buffer (3 rows)
    int16_t *error_buffer = (int16_t *)tal_malloc((dst_width + 4) * 3 * sizeof(int16_t));
    if (!error_buffer) {
        return -1;
    }
    memset(error_buffer, 0, (dst_width + 4) * 3 * sizeof(int16_t));

    int16_t *curr_row = error_buffer + 2;
    int16_t *next_row1 = error_buffer + dst_width + 6;
    int16_t *next_row2 = error_buffer + (dst_width + 4) * 2 + 2;

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

            if (new_pixel == 255) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }

            // Jarvis-Judice-Ninke error diffusion (divisor: 48)
            // Current row
            if (dst_x < dst_width - 1)
                curr_row[dst_x + 1] += (error * 7) / 48;
            if (dst_x < dst_width - 2)
                curr_row[dst_x + 2] += (error * 5) / 48;

            // Next row (1)
            if (dst_x > 1)
                next_row1[dst_x - 2] += (error * 3) / 48;
            if (dst_x > 0)
                next_row1[dst_x - 1] += (error * 5) / 48;
            next_row1[dst_x] += (error * 7) / 48;
            if (dst_x < dst_width - 1)
                next_row1[dst_x + 1] += (error * 5) / 48;
            if (dst_x < dst_width - 2)
                next_row1[dst_x + 2] += (error * 3) / 48;

            // Next row (2)
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

        // Rotate rows
        int16_t *temp = curr_row;
        curr_row = next_row1;
        next_row1 = next_row2;
        next_row2 = temp;
        memset(next_row2 - 2, 0, (dst_width + 4) * sizeof(int16_t));
    }

    tal_free(error_buffer);
    return 0;
}

/**
 * @brief Calculate adaptive threshold from source image
 */
static uint8_t calculate_adaptive_threshold(uint8_t *yuv422_data, int src_width, int src_height)
{
    uint32_t luminance_sum = 0;
    int total_pixels = src_width * src_height;

    for (int y = 0; y < src_height; y++) {
        int row_offset = y * src_width * 2;
        for (int x = 0; x < src_width; x++) {
            int yuv_index = row_offset + x * 2 + 1;
            luminance_sum += yuv422_data[yuv_index];
        }
    }

    return (uint8_t)(luminance_sum / total_pixels);
}

/**
 * @brief Calculate Otsu threshold from source image
 */
static uint8_t calculate_otsu_threshold(uint8_t *yuv422_data, int src_width, int src_height)
{
    int histogram[256] = {0};
    int total_pixels = src_width * src_height;

    for (int y = 0; y < src_height; y++) {
        int row_offset = y * src_width * 2;
        for (int x = 0; x < src_width; x++) {
            int yuv_index = row_offset + x * 2 + 1;
            uint8_t luminance = yuv422_data[yuv_index];
            histogram[luminance]++;
        }
    }

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

/**
 * @brief Convert YUV422 to binary with config (240x240 -> 240x168 cropped)
 */
static int yuv422_to_binary_with_config(uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                        int dst_width, int dst_height, BINARY_CONFIG_T *config)
{
    if (!yuv422_data || !binary_data || !config || src_width <= 0 || src_height <= 0) {
        return -1;
    }

    // Special handling for Bayer dithering modes
    // if (config->method == BINARY_METHOD_BAYER8_DITHER) {
    //     sg_calculated_threshold = 0;
    //     return yuv422_to_bayer8_dither_crop(yuv422_data, src_width, src_height, binary_data, dst_width, dst_height);
    // }

    // if (config->method == BINARY_METHOD_BAYER4_DITHER) {
    //     sg_calculated_threshold = 0;
    //     return yuv422_to_bayer4_dither_crop(yuv422_data, src_width, src_height, binary_data, dst_width, dst_height);
    // }

    // if (config->method == BINARY_METHOD_BAYER16_DITHER) {
    //     sg_calculated_threshold = 0;
    //     return yuv422_to_bayer16_dither_crop(yuv422_data, src_width, src_height, binary_data, dst_width, dst_height);
    // }

    // // Error diffusion algorithms
    // if (config->method == BINARY_METHOD_FLOYD_STEINBERG) {
    //     return yuv422_to_floyd_steinberg_crop(yuv422_data, src_width, src_height, binary_data, dst_width,
    //     dst_height);
    // }

    // if (config->method == BINARY_METHOD_STUCKI) {
    //     return yuv422_to_stucki_crop(yuv422_data, src_width, src_height, binary_data, dst_width, dst_height);
    // }

    // if (config->method == BINARY_METHOD_JARVIS) {
    //     return yuv422_to_jarvis_crop(yuv422_data, src_width, src_height, binary_data, dst_width, dst_height);
    // }

    switch (config->method) {
    case BINARY_METHOD_FIXED:
        sg_calculated_threshold = config->fixed_threshold;
        break;

    case BINARY_METHOD_ADAPTIVE:
        sg_calculated_threshold = calculate_adaptive_threshold(yuv422_data, src_width, src_height);
        break;

    case BINARY_METHOD_OTSU:
        sg_calculated_threshold = calculate_otsu_threshold(yuv422_data, src_width, src_height);
        break;

    case BINARY_METHOD_BAYER4_DITHER:
        return yuv422_to_bayer4_dither_crop(yuv422_data, src_width, src_height, binary_data, dst_width, dst_height);

    case BINARY_METHOD_BAYER8_DITHER:
        return yuv422_to_bayer8_dither_crop(yuv422_data, src_width, src_height, binary_data, dst_width, dst_height);

    case BINARY_METHOD_BAYER16_DITHER:
        return yuv422_to_bayer16_dither_crop(yuv422_data, src_width, src_height, binary_data, dst_width, dst_height);

    case BINARY_METHOD_FLOYD_STEINBERG:
        return yuv422_to_floyd_steinberg_crop(yuv422_data, src_width, src_height, binary_data, dst_width, dst_height);

    case BINARY_METHOD_STUCKI:
        return yuv422_to_stucki_crop(yuv422_data, src_width, src_height, binary_data, dst_width, dst_height);

    case BINARY_METHOD_JARVIS:
        return yuv422_to_jarvis_crop(yuv422_data, src_width, src_height, binary_data, dst_width, dst_height);

    default:
        return -1;
    }

    return yuv422_to_binary_crop(yuv422_data, src_width, src_height, binary_data, dst_width, dst_height,
                                 sg_calculated_threshold);
}

/**
 * @brief Camera frame callback - only receive and save raw YUV422 data
 * @note This callback runs in camera thread context, should not call LVGL APIs
 * @note Image processing is now done in timer callback
 */
static OPERATE_RET camera_frame_callback(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    if (NULL == hdl || NULL == frame) {
        return OPRT_INVALID_PARM;
    }

    if (!camera_running || !sg_buffer_mutex || !sg_yuv422_write_buffer) {
        return OPRT_OK;
    }

    // Lock mutex to safely copy data
    tal_mutex_lock(sg_buffer_mutex);

    // Determine current write buffer index
    uint8_t current_write_index = (sg_yuv422_write_buffer == sg_yuv422_buffer_1) ? 0 : 1;

    // Copy raw YUV422 data to buffer (240x240x2 = 115200 bytes)
    uint32_t yuv422_size = frame->width * frame->height * 2;
    memcpy(sg_yuv422_write_buffer, frame->data, yuv422_size);

    // Mark which buffer contains the new frame
    write_buffer_index = current_write_index;

    // Toggle YUV422 buffer for next capture
    sg_yuv422_write_buffer = (sg_yuv422_write_buffer == sg_yuv422_buffer_1) ? sg_yuv422_buffer_2 : sg_yuv422_buffer_1;

    // Set flag to notify LVGL timer that new frame is ready
    frame_ready = true;

    tal_mutex_unlock(sg_buffer_mutex);

    return OPRT_OK;
}

/**
 * @brief Initialize camera hardware
 */
static OPERATE_RET camera_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_NOTICE("Camera init starting...");

    // Create mutex for buffer synchronization
    rt = tal_mutex_create_init(&sg_buffer_mutex);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to create buffer mutex: %d", rt);
        return rt;
    }
    PR_DEBUG("Buffer mutex created");

    // Allocate YUV422 raw data buffers (240x240x2 = 115200 bytes each)
    uint32_t yuv422_size = CAMERA_WIDTH * CAMERA_HEIGHT * 2;
    sg_yuv422_buffer_1 = (uint8_t *)tal_psram_malloc(yuv422_size);
    if (NULL == sg_yuv422_buffer_1) {
        PR_ERR("Failed to allocate YUV422 buffer 1");
        return OPRT_MALLOC_FAILED;
    }

    sg_yuv422_buffer_2 = (uint8_t *)tal_psram_malloc(yuv422_size);
    if (NULL == sg_yuv422_buffer_2) {
        PR_ERR("Failed to allocate YUV422 buffer 2");
        tal_psram_free(sg_yuv422_buffer_1);
        return OPRT_MALLOC_FAILED;
    }

    sg_yuv422_write_buffer = sg_yuv422_buffer_1;
    PR_DEBUG("YUV422 buffers allocated: %d bytes each", yuv422_size);

    // Create frame buffers for binary output (240x168 after crop)
    uint32_t frame_len = (CAMERA_AREA_WIDTH + 7) / 8 * CAMERA_AREA_HEIGHT;
    PR_DEBUG("Binary frame buffer size: %d bytes", frame_len);

    sg_p_display_fb_1 = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if (NULL == sg_p_display_fb_1) {
        PR_ERR("create frame buff 1 failed");
        return OPRT_MALLOC_FAILED;
    }
    sg_p_display_fb_1->fmt = TUYA_PIXEL_FMT_MONOCHROME;
    sg_p_display_fb_1->width = CAMERA_AREA_WIDTH;
    sg_p_display_fb_1->height = CAMERA_AREA_HEIGHT;

    sg_p_display_fb_2 = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if (NULL == sg_p_display_fb_2) {
        PR_ERR("create frame buff 2 failed");
        return OPRT_MALLOC_FAILED;
    }
    sg_p_display_fb_2->fmt = TUYA_PIXEL_FMT_MONOCHROME;
    sg_p_display_fb_2->width = CAMERA_AREA_WIDTH;
    sg_p_display_fb_2->height = CAMERA_AREA_HEIGHT;

    sg_p_display_fb = sg_p_display_fb_1;

    // Find camera device
    sg_tdl_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    if (NULL == sg_tdl_camera_hdl) {
        PR_ERR("camera dev %s not found", CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    PR_NOTICE("camera init success");
    return OPRT_OK;
}

/**
 * @brief Start camera capture
 */
static OPERATE_RET camera_start(void)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_CAMERA_CFG_T cfg;

    PR_NOTICE("Starting camera...");

    if (camera_running) {
        PR_WARN("Camera already running");
        return OPRT_OK;
    }

    if (!sg_buffer_mutex) {
        PR_ERR("Buffer mutex not initialized");
        return OPRT_INVALID_PARM;
    }

    memset(&cfg, 0, sizeof(TDL_CAMERA_CFG_T));
    cfg.fps = CAMERA_FPS;
    cfg.width = CAMERA_WIDTH;
    cfg.height = CAMERA_HEIGHT;
    cfg.out_fmt = TDL_CAMERA_FMT_YUV422;
    cfg.get_frame_cb = camera_frame_callback;

    PR_DEBUG("Camera config: %dx%d @ %d fps, callback=%p", cfg.width, cfg.height, cfg.fps, cfg.get_frame_cb);

    rt = tdl_camera_dev_open(sg_tdl_camera_hdl, &cfg);
    if (OPRT_OK == rt) {
        camera_running = true;
        update_info_display(); // Update UI to show running status
        PR_NOTICE("Camera started successfully");
    } else {
        PR_ERR("Camera start failed: %d", rt);
    }

    return rt;
}

/**
 * @brief Stop camera capture
 */
static void camera_stop(void)
{
    if (camera_running && sg_tdl_camera_hdl) {
        camera_running = false; // Set flag first to stop frame processing
        tdl_camera_dev_close(sg_tdl_camera_hdl);
        PR_NOTICE("camera stopped");
    }
}
#endif // ENABLE_LVGL_HARDWARE

/**
 * @brief Keyboard event callback
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Key pressed: %d\n", camera_screen.name, key);

    switch (key) {
    case KEY_UP:
#ifdef ENABLE_LVGL_HARDWARE
        // Increase threshold (only in fixed mode)
        if (sg_binary_config.method == BINARY_METHOD_FIXED) {
            if (sg_binary_config.fixed_threshold <= THRESHOLD_MAX - THRESHOLD_STEP) {
                sg_binary_config.fixed_threshold += THRESHOLD_STEP;
            } else {
                sg_binary_config.fixed_threshold = THRESHOLD_MAX;
            }
            printf("Threshold increased to %d\n", sg_binary_config.fixed_threshold);
        }
#endif
        break;

    case KEY_DOWN:
#ifdef ENABLE_LVGL_HARDWARE
        // Decrease threshold (only in fixed mode)
        if (sg_binary_config.method == BINARY_METHOD_FIXED) {
            if (sg_binary_config.fixed_threshold >= THRESHOLD_MIN + THRESHOLD_STEP) {
                sg_binary_config.fixed_threshold -= THRESHOLD_STEP;
            } else {
                sg_binary_config.fixed_threshold = THRESHOLD_MIN;
            }
            printf("Threshold decreased to %d\n", sg_binary_config.fixed_threshold);
        }
#endif
        break;

    case KEY_LEFT:
#ifdef ENABLE_LVGL_HARDWARE
        // Previous method
        if (sg_binary_config.method > 0) {
            sg_binary_config.method--;
        } else {
            sg_binary_config.method = BINARY_METHOD_COUNT - 1;
        }
        printf("Method changed to %s\n", get_method_name(sg_binary_config.method));
#endif
        break;

    case KEY_RIGHT:
#ifdef ENABLE_LVGL_HARDWARE
        // Next method
        sg_binary_config.method = (sg_binary_config.method + 1) % BINARY_METHOD_COUNT;
        printf("Method changed to %s\n", get_method_name(sg_binary_config.method));
#endif
        break;

    case KEY_ENTER:
#ifdef ENABLE_LVGL_HARDWARE
        // Toggle camera on/off
        if (camera_running) {
            camera_stop();
        } else {
            camera_start();
        }
#endif
        break;

    case KEY_ESC:
        // Return to previous screen
        printf("ESC key pressed, going back\n");
        screen_back();
        break;

    default:
        break;
    }
}

/**
 * @brief Initialize camera screen
 */
void camera_screen_init(void)
{
    printf("[%s] Initializing camera screen\n", camera_screen.name);

    // Create full screen container (must be full screen for screen object)
    // But set it completely transparent so it doesn't interfere with camera area
    ui_camera_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_camera_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    // lv_obj_set_style_bg_color(ui_camera_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ui_camera_screen, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_camera_screen, 0, 0);
    lv_obj_set_style_pad_all(ui_camera_screen, 0, 0);
    lv_obj_clear_flag(ui_camera_screen, LV_OBJ_FLAG_SCROLLABLE);

#ifdef ENABLE_LVGL_HARDWARE
    // Create canvas for camera display (left side 240x168)
    camera_canvas = lv_canvas_create(ui_camera_screen);
    lv_obj_set_pos(camera_canvas, 0, 0);
    lv_obj_set_size(camera_canvas, CAMERA_AREA_WIDTH, CAMERA_AREA_HEIGHT);

    // Allocate canvas buffer for monochrome 1-bit indexed image
    // For LVGL v9: I1 format needs palette (8 bytes) + bitmap data
    uint32_t bitmap_size = ((CAMERA_AREA_WIDTH + 7) / 8) * CAMERA_AREA_HEIGHT;
    uint32_t canvas_buf_size = bitmap_size + 8; // 8 bytes for 2-color palette (2 * 4 bytes)
    canvas_buffer = (uint8_t *)tal_psram_malloc(canvas_buf_size);
    if (canvas_buffer) {
        // Initialize buffer: clear palette area and fill bitmap with test pattern
        memset(canvas_buffer, 0, 8);                  // Clear palette area
        memset(canvas_buffer + 8, 0x00, bitmap_size); // Initialize bitmap (0x00 = all index 0)

        lv_canvas_set_buffer(camera_canvas, canvas_buffer, CAMERA_AREA_WIDTH, CAMERA_AREA_HEIGHT, LV_COLOR_FORMAT_I1);

        // Set palette: LVGL I1 format: bit=0->palette[0], bit=1->palette[1]
        // Our logic: luminance >= threshold -> bit=1 (bright/white)
        //            luminance < threshold -> bit=0 (dark/black)
        // Therefore: palette[0]=black (for bit=0), palette[1]=white (for bit=1)
        lv_canvas_set_palette(camera_canvas, 0, lv_color32_make(0x00, 0x00, 0x00, 0xFF)); // black for bit=0
        lv_canvas_set_palette(camera_canvas, 1, lv_color32_make(0xFF, 0xFF, 0xFF, 0xFF)); // white for bit=1

        PR_NOTICE("Canvas initialized: %dx%d, buffer size=%d (palette:8 + bitmap:%d)", CAMERA_AREA_WIDTH,
                  CAMERA_AREA_HEIGHT, canvas_buf_size, bitmap_size);
    } else {
        PR_ERR("Failed to allocate canvas buffer");
    }
#endif

    // Info display area positioned at right side ONLY
    // Left side (0-240) stays empty for camera TDL direct refresh
    lv_obj_t *info_panel = lv_obj_create(ui_camera_screen);
    lv_obj_set_pos(info_panel, INFO_AREA_X, 0); // Position at x=240 (right side)
    lv_obj_set_size(info_panel, INFO_AREA_WIDTH, INFO_AREA_HEIGHT);
    lv_obj_set_style_bg_color(info_panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(info_panel, LV_OPA_90, 0);
    lv_obj_set_style_border_width(info_panel, 2, 0);
    lv_obj_set_style_border_color(info_panel, lv_color_black(), 0);
    lv_obj_set_style_pad_all(info_panel, 0, 0);
    lv_obj_clear_flag(info_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Method label
    method_label = lv_label_create(info_panel);
    lv_obj_set_pos(method_label, 10, 10);
    lv_obj_set_width(method_label, INFO_AREA_WIDTH - 20);
    lv_obj_set_style_text_color(method_label, lv_color_black(), 0);

    // Threshold label
    threshold_label = lv_label_create(info_panel);
    lv_obj_set_pos(threshold_label, 10, 60);
    lv_obj_set_width(threshold_label, INFO_AREA_WIDTH - 20);
    lv_obj_set_style_text_color(threshold_label, lv_color_black(), 0);

    // Status label
    status_label = lv_label_create(info_panel);
    lv_obj_set_pos(status_label, 10, 110);
    lv_obj_set_width(status_label, INFO_AREA_WIDTH - 20);
    lv_obj_set_style_text_color(status_label, lv_color_black(), 0);

#ifdef ENABLE_LVGL_HARDWARE
    // Initialize camera hardware
    OPERATE_RET rt = camera_init();
    if (OPRT_OK != rt) {
        PR_ERR("Camera initialization failed: %d", rt);
    } else {
        // Auto-start camera
        camera_start();
    }
#endif

    // Create 100ms timer for updating display
    update_timer = lv_timer_create(update_timer_cb, 20, NULL);

    // Add keyboard event callback
    lv_obj_add_event_cb(ui_camera_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_camera_screen);
    lv_group_focus_obj(ui_camera_screen);

    printf("[%s] Camera screen initialized\n", camera_screen.name);
}

/**
 * @brief Deinitialize camera screen
 */
void camera_screen_deinit(void)
{
    printf("[%s] Deinitializing camera screen\n", camera_screen.name);

    // Delete timer
    if (update_timer) {
        lv_timer_delete(update_timer);
        update_timer = NULL;
    }

#ifdef ENABLE_LVGL_HARDWARE
    // Stop camera if running
    camera_stop();

    // Free YUV422 buffers
    if (sg_yuv422_buffer_1) {
        tal_psram_free(sg_yuv422_buffer_1);
        sg_yuv422_buffer_1 = NULL;
    }
    if (sg_yuv422_buffer_2) {
        tal_psram_free(sg_yuv422_buffer_2);
        sg_yuv422_buffer_2 = NULL;
    }
    sg_yuv422_write_buffer = NULL;

    // Clean up frame buffers
    if (sg_p_display_fb_1) {
        tdl_disp_free_frame_buff(sg_p_display_fb_1);
        sg_p_display_fb_1 = NULL;
    }
    if (sg_p_display_fb_2) {
        tdl_disp_free_frame_buff(sg_p_display_fb_2);
        sg_p_display_fb_2 = NULL;
    }
    sg_p_display_fb = NULL;

    // Free canvas buffer
    if (canvas_buffer) {
        tal_psram_free(canvas_buffer);
        canvas_buffer = NULL;
    }

    // Release mutex
    if (sg_buffer_mutex) {
        tal_mutex_release(sg_buffer_mutex);
        sg_buffer_mutex = NULL;
    }
#endif

    // Remove event callback and clean up UI
    if (ui_camera_screen) {
        lv_obj_remove_event_cb(ui_camera_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_camera_screen);
    }

    printf("[%s] Camera screen deinitialized\n", camera_screen.name);
}
