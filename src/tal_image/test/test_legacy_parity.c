/* src/tal_image/test/test_legacy_parity.c
 *
 * Host-side legacy-parity harness (best-effort verification, NOT part of the
 * embedded build -- same pattern as test_dither_core.c).
 *
 * This byte-for-byte compares the OLD (deleted) app-local dithering
 * implementation -- apps/tuya_t5_pocket/tuya_t5_pocket_ai/src/expand/src/
 * yuv422_to_binary.c as it existed at commit 3f9747a4 (git show
 * 3f9747a4b30922ae57c3d15f88957703b9532019:...) -- against the NEW shared
 * tal_image_dither_core pipeline (tal_image_extract_gray_from_yuv422() +
 * tal_image_dither_gray_to_binary()), across all 11 methods, both real size
 * combos the tuya_t5_pocket app uses (240x168 and 384x384, both cropped from
 * a 480x480 camera source -- see CAMERA_WIDTH/HEIGHT/AREA_WIDTH/AREA_HEIGHT
 * and PRINT_WIDTH/HEIGHT in the now-deleted camera_screen.c), and both
 * invert_colors values.
 *
 * The old algorithm code below is copied verbatim from that commit (only
 * whitespace/formatting preserved as-is), with:
 *   - tal_psram_malloc/tal_psram_free shimmed to malloc/free via #define
 *     (the old code has no other TAL/SDK dependency -- it only ever calls
 *     these two allocator functions, nothing else from tal_api.h).
 *   - All internal enum/type/function names given a `legacy_` prefix (or
 *     LEGACY_ prefix for macros/types) to avoid any collision with the new
 *     core's identically-named internals (bayer tables, EDGE_ATKINSON_*
 *     macros, etc.) -- though since this is a separate translation unit from
 *     tal_image_dither_core.c, only linked together, no actual collision is
 *     possible; the prefixing is purely for human readability while reading
 *     this file side-by-side with the new core.
 *
 * Expected result per the final reviewer's independent analysis (this
 * harness's job is to CONFIRM OR REFUTE that, not assume it):
 *   - Fixed, Bayer4, Bayer16, Floyd-Steinberg, Stucki, Jarvis, and
 *     Gamma-Serpentine: byte-identical.
 *   - Bayer8: differs (intentional bug fix -- old had a speckle bug, see
 *     fe983cba "fix(tal_image): correct BAYER8 gray level calculation").
 *   - Adaptive, Otsu, Edge-Atkinson: differ (threshold now scoped to the
 *     destination/cropped plane, not the full source frame -- see the
 *     threshold-scope comment above calc_adaptive_threshold() in
 *     tal_image_dither_core.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "tal_image_dither_core.h"

/* ===================================================================
 * Shim: the old code's only two SDK dependencies.
 * =================================================================== */
#define tal_psram_malloc malloc
#define tal_psram_free   free

/* ===================================================================
 * BEGIN: copied from apps/tuya_t5_pocket/tuya_t5_pocket_ai/src/expand/
 * inc/yuv422_to_binary.h @ 3f9747a4, renamed with a LEGACY_/legacy_ prefix.
 * =================================================================== */
typedef enum {
    LEGACY_BINARY_METHOD_FIXED = 0,
    LEGACY_BINARY_METHOD_ADAPTIVE,
    LEGACY_BINARY_METHOD_OTSU,
    LEGACY_BINARY_METHOD_BAYER8_DITHER,
    LEGACY_BINARY_METHOD_BAYER4_DITHER,
    LEGACY_BINARY_METHOD_BAYER16_DITHER,
    LEGACY_BINARY_METHOD_FLOYD_STEINBERG,
    LEGACY_BINARY_METHOD_STUCKI,
    LEGACY_BINARY_METHOD_JARVIS,
    LEGACY_BINARY_METHOD_EDGE_ATKINSON,
    LEGACY_BINARY_METHOD_GAMMA_SERPENTINE,
    LEGACY_BINARY_METHOD_COUNT
} LEGACY_BINARY_METHOD_E;

typedef struct {
    LEGACY_BINARY_METHOD_E method;
    uint8_t fixed_threshold;
} LEGACY_BINARY_CONFIG_T;

typedef struct {
    const uint8_t *yuv422_data;
    int src_width;
    int src_height;
    uint8_t *binary_data;
    int dst_width;
    int dst_height;
    const LEGACY_BINARY_CONFIG_T *config;
    int invert_colors;
} LEGACY_YUV422_TO_BINARY_PARAMS_T;

/* ===================================================================
 * BEGIN: copied from apps/tuya_t5_pocket/tuya_t5_pocket_ai/src/expand/
 * src/yuv422_to_binary.c @ 3f9747a4, renamed with a legacy_ prefix.
 * =================================================================== */
#define LEGACY_EDGE_ATKINSON_THRESHOLD      200
#define LEGACY_EDGE_ATKINSON_MAX_BRIGHTNESS 166
#define LEGACY_EDGE_ATKINSON_GAMMA          2.0f
#define LEGACY_GAMMA_SERPENTINE_GAMMA       1.45f
#define LEGACY_GAMMA_SERPENTINE_THRESHOLD   128

static const uint8_t legacy_bayer_2x2[2][2] = {{0, 2}, {3, 1}};
static const uint8_t legacy_bayer_3x3[3][3] = {{0, 7, 3}, {6, 4, 2}, {1, 5, 8}};
static const uint8_t legacy_bayer_4x4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static uint8_t legacy_calculate_adaptive_threshold(const uint8_t *yuv422_data, int src_width, int src_height)
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

static uint8_t legacy_calculate_otsu_threshold(const uint8_t *yuv422_data, int src_width, int src_height)
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

static int legacy_yuv422_to_binary_crop_threshold(const uint8_t *yuv422_data, int src_width, int src_height,
                                           uint8_t *binary_data, int dst_width, int dst_height, uint8_t threshold,
                                           int invert)
{
    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2;

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

static int legacy_yuv422_to_bayer4_dither(const uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                   int dst_width, int dst_height, int invert)
{
    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2;

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

            uint8_t bayer_value = legacy_bayer_2x2[dst_y % 2][dst_x % 2];
            uint8_t gray_level = luminance / 85;

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

static int legacy_yuv422_to_bayer8_dither(const uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                   int dst_width, int dst_height, int invert)
{
    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2;

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

            /* Intentional legacy bug, preserved verbatim: divides by 32 (0-7
             * range) but compares against the 3x3 matrix whose max value is
             * 8, so gray_level never reaches 8 -- causes speckling on pure
             * white input. Fixed in the new core (fe983cba). */
            uint8_t bayer_value = legacy_bayer_3x3[dst_y % 3][dst_x % 3];
            uint8_t gray_level = luminance / 32;

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

static int legacy_yuv422_to_bayer16_dither(const uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                    int dst_width, int dst_height, int invert)
{
    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2;

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

            uint8_t bayer_value = legacy_bayer_4x4[dst_y % 4][dst_x % 4];
            uint8_t gray_level = luminance / 17;

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

static int legacy_yuv422_to_floyd_steinberg(const uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                     int dst_width, int dst_height, int invert)
{
    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2;

    int16_t *error_buffer = (int16_t *)tal_psram_malloc((dst_width + 2) * 2 * sizeof(int16_t));
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

            int should_set_bit = invert ? (new_pixel == 255) : (new_pixel == 0);
            if (should_set_bit) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }

            if (dst_x < dst_width - 1)
                curr_row[dst_x + 1] += (error * 7) / 16;
            if (dst_x > 0)
                next_row[dst_x - 1] += (error * 3) / 16;
            next_row[dst_x] += (error * 5) / 16;
            if (dst_x < dst_width - 1)
                next_row[dst_x + 1] += error / 16;
        }

        int16_t *temp = curr_row;
        curr_row = next_row;
        next_row = temp;
        memset(next_row - 1, 0, (dst_width + 2) * sizeof(int16_t));
    }

    tal_psram_free(error_buffer);
    return 0;
}

static int legacy_yuv422_to_stucki(const uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                            int dst_width, int dst_height, int invert)
{
    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2;

    int16_t *error_buffer = (int16_t *)tal_psram_malloc((dst_width + 4) * 3 * sizeof(int16_t));
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

        int16_t *temp = curr_row;
        curr_row = next_row1;
        next_row1 = next_row2;
        next_row2 = temp;
        memset(next_row2 - 2, 0, (dst_width + 4) * sizeof(int16_t));
    }

    tal_psram_free(error_buffer);
    return 0;
}

static inline uint8_t legacy_get_luma_clamped(const uint8_t *yuv422_data, int src_width, int src_height, int x, int y)
{
    if (x < 0)
        x = 0;
    if (x >= src_width)
        x = src_width - 1;
    if (y < 0)
        y = 0;
    if (y >= src_height)
        y = src_height - 1;
    return yuv422_data[y * src_width * 2 + x * 2 + 1];
}

static int legacy_yuv422_to_edge_atkinson(const uint8_t *yuv422_data, int src_width, int src_height,
                                         uint8_t *binary_data, int dst_width, int dst_height, int invert)
{
    static uint8_t edge_atkinson_gamma_lut[256];
    static bool    edge_atkinson_gamma_lut_ready = false;

    if (!edge_atkinson_gamma_lut_ready) {
        for (int i = 0; i < 256; i++) {
            float v = powf((float)i / 255.0f, 1.0f / LEGACY_EDGE_ATKINSON_GAMMA) * 255.0f;
            edge_atkinson_gamma_lut[i] = (uint8_t)(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v));
        }
        edge_atkinson_gamma_lut_ready = true;
    }

    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2;
    uint8_t black_thresh = edge_atkinson_gamma_lut[legacy_calculate_adaptive_threshold(yuv422_data, src_width, src_height)];

    int16_t *error_buffer = (int16_t *)tal_psram_malloc((dst_width + 4) * 3 * sizeof(int16_t));
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

            int center = legacy_get_luma_clamped(yuv422_data, src_width, src_height, src_x, src_y);
            int sum8 = legacy_get_luma_clamped(yuv422_data, src_width, src_height, src_x - 1, src_y - 1) +
                       legacy_get_luma_clamped(yuv422_data, src_width, src_height, src_x, src_y - 1) +
                       legacy_get_luma_clamped(yuv422_data, src_width, src_height, src_x + 1, src_y - 1) +
                       legacy_get_luma_clamped(yuv422_data, src_width, src_height, src_x - 1, src_y) +
                       legacy_get_luma_clamped(yuv422_data, src_width, src_height, src_x + 1, src_y) +
                       legacy_get_luma_clamped(yuv422_data, src_width, src_height, src_x - 1, src_y + 1) +
                       legacy_get_luma_clamped(yuv422_data, src_width, src_height, src_x, src_y + 1) +
                       legacy_get_luma_clamped(yuv422_data, src_width, src_height, src_x + 1, src_y + 1);
            int edge_mag = 8 * center - sum8;
            if (edge_mag < 0)
                edge_mag = -edge_mag;

            uint8_t gamma_center = edge_atkinson_gamma_lut[center];
            int16_t luminance = (int16_t)gamma_center + curr_row[dst_x];
            if (luminance < 0)
                luminance = 0;
            if (luminance > 255)
                luminance = 255;

            int is_edge = edge_mag > LEGACY_EDGE_ATKINSON_THRESHOLD && gamma_center < LEGACY_EDGE_ATKINSON_MAX_BRIGHTNESS;
            uint8_t new_pixel = is_edge ? 0 : ((luminance >= black_thresh) ? 255 : 0);

            int should_set_bit = invert ? (new_pixel == 255) : (new_pixel == 0);
            if (should_set_bit) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }

            if (!is_edge) {
                int16_t error = luminance - new_pixel;
                if (dst_x < dst_width - 1)
                    curr_row[dst_x + 1] += error / 8;
                if (dst_x < dst_width - 2)
                    curr_row[dst_x + 2] += error / 8;
                if (dst_x > 0)
                    next_row1[dst_x - 1] += error / 8;
                next_row1[dst_x] += error / 8;
                if (dst_x < dst_width - 1)
                    next_row1[dst_x + 1] += error / 8;
                next_row2[dst_x] += error / 8;
            }
        }

        int16_t *temp = curr_row;
        curr_row = next_row1;
        next_row1 = next_row2;
        next_row2 = temp;
        memset(next_row2 - 2, 0, (dst_width + 4) * sizeof(int16_t));
    }

    tal_psram_free(error_buffer);
    return 0;
}

static int legacy_yuv422_to_gamma_serpentine(const uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                                  int dst_width, int dst_height, int invert)
{
    static uint8_t gamma_lut[256];
    static bool    gamma_lut_ready = false;

    if (!gamma_lut_ready) {
        for (int i = 0; i < 256; i++) {
            float v = powf((float)i / 255.0f, 1.0f / LEGACY_GAMMA_SERPENTINE_GAMMA) * 255.0f;
            gamma_lut[i] = (uint8_t)(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v));
        }
        gamma_lut_ready = true;
    }

    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2;

    int16_t *error_buffer = (int16_t *)tal_psram_malloc((dst_width + 2) * 2 * sizeof(int16_t));
    if (!error_buffer) {
        return -1;
    }

    int16_t *curr_row = error_buffer + 1;
    int16_t *next_row = error_buffer + dst_width + 3;
    memset(error_buffer, 0, (dst_width + 2) * 2 * sizeof(int16_t));

    for (int dst_y = 0; dst_y < dst_height; dst_y++) {
        int row_offset = dst_y * binary_stride;
        int direction = (dst_y % 2 == 0) ? 1 : -1;
        int dst_x = (direction == 1) ? 0 : dst_width - 1;

        for (int count = 0; count < dst_width; count++, dst_x += direction) {
            int src_x = dst_y + crop_offset;
            int src_y = src_height - 1 - dst_x;

            if (src_x < 0 || src_x >= src_width || src_y < 0 || src_y >= src_height) {
                continue;
            }

            int yuv_index = src_y * src_width * 2 + src_x * 2 + 1;
            uint8_t gamma_corrected = gamma_lut[yuv422_data[yuv_index]];

            int16_t luminance = (int16_t)gamma_corrected + curr_row[dst_x];
            if (luminance < 0)
                luminance = 0;
            if (luminance > 255)
                luminance = 255;

            uint8_t new_pixel = (luminance >= LEGACY_GAMMA_SERPENTINE_THRESHOLD) ? 255 : 0;
            int16_t error = luminance - new_pixel;

            int should_set_bit = invert ? (new_pixel == 255) : (new_pixel == 0);
            if (should_set_bit) {
                int byte_index = row_offset + (dst_x >> 3);
                int bit_position = 7 - (dst_x & 0x07);
                binary_data[byte_index] |= (1 << bit_position);
            }

            if (dst_x + direction >= 0 && dst_x + direction < dst_width)
                curr_row[dst_x + direction] += (error * 7) / 16;
            if (dst_y < dst_height - 1) {
                if (dst_x - direction >= 0 && dst_x - direction < dst_width)
                    next_row[dst_x - direction] += (error * 3) / 16;
                next_row[dst_x] += (error * 5) / 16;
                if (dst_x + direction >= 0 && dst_x + direction < dst_width)
                    next_row[dst_x + direction] += error / 16;
            }
        }

        int16_t *temp = curr_row;
        curr_row = next_row;
        next_row = temp;
        memset(next_row - 1, 0, (dst_width + 2) * sizeof(int16_t));
    }

    tal_psram_free(error_buffer);
    return 0;
}

static int legacy_yuv422_to_jarvis(const uint8_t *yuv422_data, int src_width, int src_height, uint8_t *binary_data,
                            int dst_width, int dst_height, int invert)
{
    int binary_stride = (dst_width + 7) / 8;
    int crop_offset = (src_width - dst_height) / 2;

    int16_t *error_buffer = (int16_t *)tal_psram_malloc((dst_width + 4) * 3 * sizeof(int16_t));
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

        int16_t *temp = curr_row;
        curr_row = next_row1;
        next_row1 = next_row2;
        next_row2 = temp;
        memset(next_row2 - 2, 0, (dst_width + 4) * sizeof(int16_t));
    }

    tal_psram_free(error_buffer);
    return 0;
}

static int legacy_yuv422_to_binary(const LEGACY_YUV422_TO_BINARY_PARAMS_T *params)
{
    if (!params || !params->yuv422_data || !params->binary_data || !params->config) {
        return -1;
    }

    int bitmap_size = (params->dst_width + 7) / 8 * params->dst_height;
    memset(params->binary_data, 0, bitmap_size);

    switch (params->config->method) {
    case LEGACY_BINARY_METHOD_FIXED:
        return legacy_yuv422_to_binary_crop_threshold(params->yuv422_data, params->src_width, params->src_height,
                                               params->binary_data, params->dst_width, params->dst_height,
                                               params->config->fixed_threshold, params->invert_colors);

    case LEGACY_BINARY_METHOD_ADAPTIVE: {
        uint8_t threshold = legacy_calculate_adaptive_threshold(params->yuv422_data, params->src_width, params->src_height);
        return legacy_yuv422_to_binary_crop_threshold(params->yuv422_data, params->src_width, params->src_height,
                                               params->binary_data, params->dst_width, params->dst_height, threshold,
                                               params->invert_colors);
    }

    case LEGACY_BINARY_METHOD_OTSU: {
        uint8_t threshold = legacy_calculate_otsu_threshold(params->yuv422_data, params->src_width, params->src_height);
        return legacy_yuv422_to_binary_crop_threshold(params->yuv422_data, params->src_width, params->src_height,
                                               params->binary_data, params->dst_width, params->dst_height, threshold,
                                               params->invert_colors);
    }

    case LEGACY_BINARY_METHOD_BAYER4_DITHER:
        return legacy_yuv422_to_bayer4_dither(params->yuv422_data, params->src_width, params->src_height, params->binary_data,
                                       params->dst_width, params->dst_height, params->invert_colors);

    case LEGACY_BINARY_METHOD_BAYER8_DITHER:
        return legacy_yuv422_to_bayer8_dither(params->yuv422_data, params->src_width, params->src_height, params->binary_data,
                                       params->dst_width, params->dst_height, params->invert_colors);

    case LEGACY_BINARY_METHOD_BAYER16_DITHER:
        return legacy_yuv422_to_bayer16_dither(params->yuv422_data, params->src_width, params->src_height, params->binary_data,
                                        params->dst_width, params->dst_height, params->invert_colors);

    case LEGACY_BINARY_METHOD_FLOYD_STEINBERG:
        return legacy_yuv422_to_floyd_steinberg(params->yuv422_data, params->src_width, params->src_height,
                                         params->binary_data, params->dst_width, params->dst_height,
                                         params->invert_colors);

    case LEGACY_BINARY_METHOD_STUCKI:
        return legacy_yuv422_to_stucki(params->yuv422_data, params->src_width, params->src_height, params->binary_data,
                                params->dst_width, params->dst_height, params->invert_colors);

    case LEGACY_BINARY_METHOD_JARVIS:
        return legacy_yuv422_to_jarvis(params->yuv422_data, params->src_width, params->src_height, params->binary_data,
                                params->dst_width, params->dst_height, params->invert_colors);

    case LEGACY_BINARY_METHOD_EDGE_ATKINSON:
        return legacy_yuv422_to_edge_atkinson(params->yuv422_data, params->src_width, params->src_height,
                                             params->binary_data, params->dst_width, params->dst_height,
                                             params->invert_colors);

    case LEGACY_BINARY_METHOD_GAMMA_SERPENTINE:
        return legacy_yuv422_to_gamma_serpentine(params->yuv422_data, params->src_width, params->src_height,
                                      params->binary_data, params->dst_width, params->dst_height,
                                      params->invert_colors);

    default:
        return -1;
    }
}
/* ===================================================================
 * END copied legacy code.
 * =================================================================== */

/* ===================================================================
 * Driver: synthetic YUV422 input, run both pipelines, compare.
 * =================================================================== */

/* Method names, indexed by the (numerically-identical) old/new enum value. */
static const char *method_name(int m)
{
    switch (m) {
    case 0:  return "Fixed";
    case 1:  return "Adaptive";
    case 2:  return "Otsu";
    case 3:  return "Bayer8";
    case 4:  return "Bayer4";
    case 5:  return "Bayer16";
    case 6:  return "Floyd-Steinberg";
    case 7:  return "Stucki";
    case 8:  return "Jarvis";
    case 9:  return "Edge-Atkinson";
    case 10: return "Gamma-Serpentine";
    default: return "?";
    }
}

/* Methods expected to differ from legacy, and why -- used only for the
 * final PASS/FAIL-vs-expectation classification, never to hide a real diff. */
static int expected_to_differ(int m)
{
    return (m == LEGACY_BINARY_METHOD_BAYER8_DITHER ||
            m == LEGACY_BINARY_METHOD_ADAPTIVE ||
            m == LEGACY_BINARY_METHOD_OTSU ||
            m == LEGACY_BINARY_METHOD_EDGE_ATKINSON);
}

/* Deterministic synthetic YUV422 frame: a DIAGONAL (x+y) gradient, so no
 * crop of the source frame shares the same mean as the full frame, plus
 * periodic hard edges and pseudo-random noise so adaptive/Otsu/edge-locking
 * methods all have something non-trivial to chew on. U/V bytes are
 * irrelevant filler.
 *
 * An earlier version of this pattern used an x-only gradient, which is
 * (nearly) symmetric about the center of the x range; because the
 * ROTATE_90 crop this app uses happens to be centered in x, that made the
 * cropped-region mean land suspiciously close to the full-frame mean,
 * masking the very threshold-scope difference (finding #1) this harness
 * exists to surface -- diagonal full-frame mean ~127 vs cropped-region mean
 * ~159 (240x168) / ~140 (384x384), confirmed with a standalone mean-check
 * before trusting this harness's PASS/FAIL verdicts on Adaptive/Otsu/
 * Edge-Atkinson. */
static void build_synthetic_yuv422(uint8_t *yuv, int w, int h)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int base = ((x + y) * 255) / (w - 1 + h - 1);
            int stripe = ((x / 20) % 2) ? 40 : -40;
            int noise = (int)((x * 31 + y * 17) % 23) - 11;
            int luma = base + stripe + noise;
            if (luma < 0) luma = 0;
            if (luma > 255) luma = 255;
            int idx = (y * w + x) * 2;
            yuv[idx] = 128;              /* U/V: ignored by both pipelines */
            yuv[idx + 1] = (uint8_t)luma;
        }
    }
}

static int failures = 0;
static int total_cases = 0;

static void run_case(const uint8_t *yuv, int src_w, int src_h, int dst_w, int dst_h,
                      int method, int invert)
{
    total_cases++;
    uint32_t need = (uint32_t)((dst_w + 7) / 8) * dst_h;

    /* --- old pipeline --- */
    uint8_t *old_out = (uint8_t *)malloc(need);
    LEGACY_BINARY_CONFIG_T cfg = { .method = (LEGACY_BINARY_METHOD_E)method, .fixed_threshold = 128 };
    LEGACY_YUV422_TO_BINARY_PARAMS_T params = {
        .yuv422_data = yuv,
        .src_width = src_w,
        .src_height = src_h,
        .binary_data = old_out,
        .dst_width = dst_w,
        .dst_height = dst_h,
        .config = &cfg,
        .invert_colors = invert,
    };
    int old_rc = legacy_yuv422_to_binary(&params);

    /* --- new pipeline --- */
    uint8_t *gray = (uint8_t *)malloc((size_t)dst_w * dst_h);
    tal_image_extract_gray_from_yuv422(yuv, src_w, src_h, gray, dst_w, dst_h, TAL_IMAGE_ROTATE_90);

    uint32_t scratch_size = tal_image_dither_scratch_size((TAL_IMAGE_MONO_METHOD_E)method, (uint16_t)dst_w);
    void *scratch = scratch_size ? malloc(scratch_size) : NULL;
    uint8_t *new_out = (uint8_t *)malloc(need);
    int new_rc = tal_image_dither_gray_to_binary(gray, (uint16_t)dst_w, (uint16_t)dst_h, new_out, need,
                                                  (TAL_IMAGE_MONO_METHOD_E)method, 128, (uint8_t)invert,
                                                  scratch, scratch_size);

    if (old_rc != 0 || new_rc != 0) {
        printf("FAIL: %-16s %dx%d invert=%d -- pipeline returned error (old_rc=%d new_rc=%d)\n",
               method_name(method), dst_w, dst_h, invert, old_rc, new_rc);
        failures++;
    } else {
        int diff_bits = 0;
        for (uint32_t i = 0; i < need; i++) {
            uint8_t x = old_out[i] ^ new_out[i];
            while (x) { diff_bits += x & 1; x >>= 1; }
        }
        int total_bits = (int)need * 8;
        double pct = 100.0 * diff_bits / total_bits;
        int identical = (diff_bits == 0);
        int should_differ = expected_to_differ(method);

        if (identical && !should_differ) {
            printf("PASS: %-16s %dx%d invert=%d -- byte-identical, as expected\n",
                   method_name(method), dst_w, dst_h, invert);
        } else if (!identical && should_differ) {
            printf("PASS: %-16s %dx%d invert=%d -- differs as expected (%d/%d bits, %.2f%%)\n",
                   method_name(method), dst_w, dst_h, invert, diff_bits, total_bits, pct);
        } else if (identical && should_differ) {
            printf("NOTE: %-16s %dx%d invert=%d -- expected a difference but got byte-identical output\n",
                   method_name(method), dst_w, dst_h, invert);
        } else {
            printf("FAIL: %-16s %dx%d invert=%d -- UNEXPECTED difference (%d/%d bits, %.2f%%) -- possible new bug\n",
                   method_name(method), dst_w, dst_h, invert, diff_bits, total_bits, pct);
            failures++;
        }
    }

    free(old_out);
    free(gray);
    free(scratch);
    free(new_out);
}

int main(void)
{
    /* Real dimensions from the deleted camera_screen.c:
     * CAMERA_WIDTH/HEIGHT = 480x480 (sensor), CAMERA_AREA_WIDTH/HEIGHT =
     * 240x168 (display preview), PRINT_WIDTH/HEIGHT = 384x384 (printer). */
    const int src_w = 480, src_h = 480;
    uint8_t *yuv = (uint8_t *)malloc((size_t)src_w * src_h * 2);
    build_synthetic_yuv422(yuv, src_w, src_h);

    int dst_sizes[2][2] = { {240, 168}, {384, 384} };

    for (int s = 0; s < 2; s++) {
        int dst_w = dst_sizes[s][0], dst_h = dst_sizes[s][1];
        for (int invert = 0; invert <= 1; invert++) {
            for (int m = 0; m < LEGACY_BINARY_METHOD_COUNT; m++) {
                run_case(yuv, src_w, src_h, dst_w, dst_h, m, invert);
            }
        }
    }

    free(yuv);

    printf("\n%d case(s) run, %d failure(s)\n", total_cases, failures);
    if (failures) {
        printf("LEGACY PARITY: FAILED\n");
        return 1;
    }
    printf("LEGACY PARITY: all differences match expectations\n");
    return 0;
}
