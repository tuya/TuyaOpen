/**
 * @file tal_image_dither_core.c
 * @brief Shared monochrome dithering core implementation.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include <stdint.h>
#include <string.h>
#include "tal_image_dither_core.h"

static inline uint8_t yuv422_luma(const uint8_t *yuv422_data, int src_width, int x, int y)
{
    return yuv422_data[y * src_width * 2 + x * 2 + 1];
}

void tal_image_extract_gray_from_yuv422(const uint8_t *yuv422_data, int src_width, int src_height,
                                         uint8_t *gray_out, int dst_width, int dst_height,
                                         TAL_IMAGE_ROTATE_E rotate)
{
    int crop_x, crop_y;

    if (!yuv422_data || !gray_out || src_width <= 0 || src_height <= 0 || dst_width <= 0 || dst_height <= 0) {
        return;
    }

    if (rotate == TAL_IMAGE_ROTATE_0 || rotate == TAL_IMAGE_ROTATE_180) {
        crop_x = (src_width - dst_width) / 2;
        crop_y = (src_height - dst_height) / 2;
    } else {
        /* ROTATE_90 / ROTATE_270: axes swap, output width pairs with source height. */
        crop_x = (src_width - dst_height) / 2;
        crop_y = (src_height - dst_width) / 2;
    }

    for (int dst_y = 0; dst_y < dst_height; dst_y++) {
        for (int dst_x = 0; dst_x < dst_width; dst_x++) {
            int src_x, src_y;

            switch (rotate) {
            case TAL_IMAGE_ROTATE_0:
                src_x = dst_x + crop_x;
                src_y = dst_y + crop_y;
                break;
            case TAL_IMAGE_ROTATE_180:
                src_x = (src_width - 1 - crop_x) - dst_x;
                src_y = (src_height - 1 - crop_y) - dst_y;
                break;
            case TAL_IMAGE_ROTATE_270:
                /* CCW 90, the reverse of ROTATE_90. Centered crop. */
                src_x = (src_width - 1) - (dst_y + crop_x);
                src_y = dst_x + crop_y;
                break;
            case TAL_IMAGE_ROTATE_90:
            default:
                /* Legacy formula, preserved verbatim: asymmetric crop (offset
                 * only on src_x, none on src_y) matches production behavior. */
                src_x = dst_y + (src_width - dst_height) / 2;
                src_y = src_height - 1 - dst_x;
                break;
            }

            uint8_t gray;
            if (src_x < 0 || src_x >= src_width || src_y < 0 || src_y >= src_height) {
                gray = 255; /* out of range: default to white (never hit by today's real dst<=src sizes) */
            } else {
                gray = yuv422_luma(yuv422_data, src_width, src_x, src_y);
            }
            gray_out[dst_y * dst_width + dst_x] = gray;
        }
    }
}

static const uint8_t bayer_2x2[2][2] = {{0, 2}, {3, 1}};
static const uint8_t bayer_3x3[3][3] = {{0, 7, 3}, {6, 4, 2}, {1, 5, 8}};
static const uint8_t bayer_4x4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

/* Threshold-scope note (Adaptive / Otsu / Edge-Atkinson): these derive their
 * statistics from `gray`, which is already the cropped/rotated destination
 * plane, not the full source frame the legacy per-app code used. Intentional
 * behavior change -- reflects what's actually shown, not discarded pixels. */
static uint8_t calc_adaptive_threshold(const uint8_t *gray, uint16_t width, uint16_t height)
{
    uint32_t sum = 0;
    uint32_t total = (uint32_t)width * height;
    for (uint32_t i = 0; i < total; i++) sum += gray[i];
    return (uint8_t)(sum / total);
}

/* Computed over the destination (post-crop/rotate) plane -- see the
 * threshold-scope note above calc_adaptive_threshold(). */
static uint8_t calc_otsu_threshold(const uint8_t *gray, uint16_t width, uint16_t height)
{
    int histogram[256] = {0};
    uint32_t total = (uint32_t)width * height;
    for (uint32_t i = 0; i < total; i++) histogram[gray[i]]++;

    float sum = 0;
    for (int i = 0; i < 256; i++) sum += (float)i * histogram[i];

    float sum_bg = 0;
    uint32_t weight_bg = 0;
    float max_variance = 0;
    uint8_t optimal = 0;

    for (int t = 0; t < 256; t++) {
        weight_bg += (uint32_t)histogram[t];
        if (weight_bg == 0) continue;
        uint32_t weight_fg = total - weight_bg;
        if (weight_fg == 0) break;
        sum_bg += (float)t * histogram[t];
        float mean_bg = sum_bg / weight_bg;
        float mean_fg = (sum - sum_bg) / weight_fg;
        float variance = (float)weight_bg * weight_fg * (mean_bg - mean_fg) * (mean_bg - mean_fg);
        if (variance > max_variance) {
            max_variance = variance;
            optimal = (uint8_t)t;
        }
    }
    return optimal;
}

static void set_bit(uint8_t *out_buf, int stride, int x, int y)
{
    out_buf[y * stride + (x >> 3)] |= (0x80 >> (x & 7));
}

static int dither_threshold(const uint8_t *gray, uint16_t width, uint16_t height, uint8_t *out_buf,
                             uint8_t threshold, int invert)
{
    int stride = (width + 7) / 8;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t lum = gray[y * width + x];
            int should_set = invert ? (lum >= threshold) : (lum < threshold);
            if (should_set) set_bit(out_buf, stride, x, y);
        }
    }
    return 0;
}

static int dither_bayer(const uint8_t *gray, uint16_t width, uint16_t height, uint8_t *out_buf,
                         int invert, int levels)
{
    int stride = (width + 7) / 8;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t lum = gray[y * width + x];
            uint8_t bayer_value, gray_level;
            int should_set;
            if (levels == 4) {
                bayer_value = bayer_2x2[y % 2][x % 2];
                gray_level = lum / 85;
                should_set = invert ? (gray_level >= bayer_value && lum >= 32)
                                    : (gray_level < bayer_value || lum < 32);
            } else if (levels == 8) {
                bayer_value = bayer_3x3[y % 3][x % 3];
                /* *9/256, not *8/255: the old divisor left bucket 8 reachable
                 * only at lum==255, effectively dead. */
                gray_level = (uint8_t)((uint16_t)lum * 9 / 256);
                should_set = invert ? (gray_level >= bayer_value && lum >= 16)
                                    : (gray_level < bayer_value || lum < 16);
            } else {
                bayer_value = bayer_4x4[y % 4][x % 4];
                gray_level = lum / 17;
                should_set = invert ? (gray_level >= bayer_value) : (gray_level < bayer_value);
            }
            if (should_set) set_bit(out_buf, stride, x, y);
        }
    }
    return 0;
}

static int dither_floyd_steinberg(const uint8_t *gray, uint16_t width, uint16_t height, uint8_t *out_buf,
                                   int invert, void *scratch)
{
    int stride = (width + 7) / 8;
    int16_t *error_buffer = (int16_t *)scratch;
    memset(error_buffer, 0, (size_t)(width + 2) * 2 * sizeof(int16_t));
    int16_t *curr_row = error_buffer + 1;
    int16_t *next_row = error_buffer + width + 3;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int16_t lum = (int16_t)gray[y * width + x] + curr_row[x];
            if (lum < 0) lum = 0;
            if (lum > 255) lum = 255;
            uint8_t new_pixel = (lum >= 128) ? 255 : 0;
            int16_t error = lum - new_pixel;
            int should_set = invert ? (new_pixel == 255) : (new_pixel == 0);
            if (should_set) set_bit(out_buf, stride, x, y);

            if (x < width - 1) curr_row[x + 1] += (int16_t)((error * 7) / 16);
            if (x > 0) next_row[x - 1] += (int16_t)((error * 3) / 16);
            next_row[x] += (int16_t)((error * 5) / 16);
            if (x < width - 1) next_row[x + 1] += (int16_t)(error / 16);
        }
        int16_t *tmp = curr_row; curr_row = next_row; next_row = tmp;
        memset(next_row - 1, 0, (size_t)(width + 2) * sizeof(int16_t));
    }
    return 0;
}

static int dither_stucki(const uint8_t *gray, uint16_t width, uint16_t height, uint8_t *out_buf,
                          int invert, void *scratch)
{
    int stride = (width + 7) / 8;
    int16_t *error_buffer = (int16_t *)scratch;
    memset(error_buffer, 0, (size_t)(width + 4) * 3 * sizeof(int16_t));
    int16_t *curr_row = error_buffer + 2;
    int16_t *next_row1 = error_buffer + width + 6;
    int16_t *next_row2 = error_buffer + 2 * (width + 4) + 2;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int16_t lum = (int16_t)gray[y * width + x] + curr_row[x];
            if (lum < 0) lum = 0;
            if (lum > 255) lum = 255;
            uint8_t new_pixel = (lum >= 128) ? 255 : 0;
            int16_t error = lum - new_pixel;
            int should_set = invert ? (new_pixel == 255) : (new_pixel == 0);
            if (should_set) set_bit(out_buf, stride, x, y);

            if (x < width - 1) curr_row[x + 1] += (int16_t)((error * 8) / 42);
            if (x < width - 2) curr_row[x + 2] += (int16_t)((error * 4) / 42);
            if (x > 1) next_row1[x - 2] += (int16_t)((error * 2) / 42);
            if (x > 0) next_row1[x - 1] += (int16_t)((error * 4) / 42);
            next_row1[x] += (int16_t)((error * 8) / 42);
            if (x < width - 1) next_row1[x + 1] += (int16_t)((error * 4) / 42);
            if (x < width - 2) next_row1[x + 2] += (int16_t)((error * 2) / 42);
            if (x > 1) next_row2[x - 2] += (int16_t)(error / 42);
            if (x > 0) next_row2[x - 1] += (int16_t)((error * 2) / 42);
            next_row2[x] += (int16_t)((error * 4) / 42);
            if (x < width - 1) next_row2[x + 1] += (int16_t)((error * 2) / 42);
            if (x < width - 2) next_row2[x + 2] += (int16_t)(error / 42);
        }
        int16_t *tmp = curr_row; curr_row = next_row1; next_row1 = next_row2; next_row2 = tmp;
        memset(next_row2 - 2, 0, (size_t)(width + 4) * sizeof(int16_t));
    }
    return 0;
}

static int dither_jarvis(const uint8_t *gray, uint16_t width, uint16_t height, uint8_t *out_buf,
                          int invert, void *scratch)
{
    int stride = (width + 7) / 8;
    int16_t *error_buffer = (int16_t *)scratch;
    memset(error_buffer, 0, (size_t)(width + 4) * 3 * sizeof(int16_t));
    int16_t *curr_row = error_buffer + 2;
    int16_t *next_row1 = error_buffer + width + 6;
    int16_t *next_row2 = error_buffer + 2 * (width + 4) + 2;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int16_t lum = (int16_t)gray[y * width + x] + curr_row[x];
            if (lum < 0) lum = 0;
            if (lum > 255) lum = 255;
            uint8_t new_pixel = (lum >= 128) ? 255 : 0;
            int16_t error = lum - new_pixel;
            int should_set = invert ? (new_pixel == 255) : (new_pixel == 0);
            if (should_set) set_bit(out_buf, stride, x, y);

            if (x < width - 1) curr_row[x + 1] += (int16_t)((error * 7) / 48);
            if (x < width - 2) curr_row[x + 2] += (int16_t)((error * 5) / 48);
            if (x > 1) next_row1[x - 2] += (int16_t)((error * 3) / 48);
            if (x > 0) next_row1[x - 1] += (int16_t)((error * 5) / 48);
            next_row1[x] += (int16_t)((error * 7) / 48);
            if (x < width - 1) next_row1[x + 1] += (int16_t)((error * 5) / 48);
            if (x < width - 2) next_row1[x + 2] += (int16_t)((error * 3) / 48);
            if (x > 1) next_row2[x - 2] += (int16_t)(error / 48);
            if (x > 0) next_row2[x - 1] += (int16_t)((error * 3) / 48);
            next_row2[x] += (int16_t)((error * 5) / 48);
            if (x < width - 1) next_row2[x + 1] += (int16_t)((error * 3) / 48);
            if (x < width - 2) next_row2[x + 2] += (int16_t)(error / 48);
        }
        int16_t *tmp = curr_row; curr_row = next_row1; next_row1 = next_row2; next_row2 = tmp;
        memset(next_row2 - 2, 0, (size_t)(width + 4) * sizeof(int16_t));
    }
    return 0;
}

#define EDGE_ATKINSON_THRESHOLD      200
#define EDGE_ATKINSON_MAX_BRIGHTNESS 166 /* ~0.65 * 255 */
#define GAMMA_SERPENTINE_THRESHOLD   128

/* gamma_lut[i] = round(pow(i/255, 1/gamma) * 255), gamma = 2.0. Precomputed
 * instead of built at runtime with powf(): avoids libm and the lazy-init
 * race the runtime version had between the camera and printer tasks. */
static const uint8_t edge_atkinson_gamma_lut[256] = {
    0,   15,  22,  27,  31,  35,  39,  42,  45,  47,  50,  52,  55,  57,  59,  61,
    63,  65,  67,  69,  71,  73,  74,  76,  78,  79,  81,  82,  84,  85,  87,  88,
    90,  91,  93,  94,  95,  97,  98,  99,  100, 102, 103, 104, 105, 107, 108, 109,
    110, 111, 112, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
    127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 141,
    142, 143, 144, 145, 146, 147, 148, 148, 149, 150, 151, 152, 153, 153, 154, 155,
    156, 157, 158, 158, 159, 160, 161, 162, 162, 163, 164, 165, 165, 166, 167, 168,
    168, 169, 170, 171, 171, 172, 173, 174, 174, 175, 176, 177, 177, 178, 179, 179,
    180, 181, 182, 182, 183, 184, 184, 185, 186, 186, 187, 188, 188, 189, 190, 190,
    191, 192, 192, 193, 194, 194, 195, 196, 196, 197, 198, 198, 199, 200, 200, 201,
    201, 202, 203, 203, 204, 205, 205, 206, 206, 207, 208, 208, 209, 210, 210, 211,
    211, 212, 213, 213, 214, 214, 215, 216, 216, 217, 217, 218, 218, 219, 220, 220,
    221, 221, 222, 222, 223, 224, 224, 225, 225, 226, 226, 227, 228, 228, 229, 229,
    230, 230, 231, 231, 232, 233, 233, 234, 234, 235, 235, 236, 236, 237, 237, 238,
    238, 239, 240, 240, 241, 241, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246,
    247, 247, 248, 248, 249, 249, 250, 250, 251, 251, 252, 252, 253, 253, 254, 255,
};

/* gamma = 1.45. See edge_atkinson_gamma_lut above for why precomputed. */
static const uint8_t gamma_serpentine_gamma_lut[256] = {
    0,   5,   9,   11,  14,  16,  19,  21,  23,  25,  27,  29,  30,  32,  34,  36,
    37,  39,  40,  42,  44,  45,  47,  48,  49,  51,  52,  54,  55,  56,  58,  59,
    60,  62,  63,  64,  66,  67,  68,  69,  71,  72,  73,  74,  75,  77,  78,  79,
    80,  81,  82,  84,  85,  86,  87,  88,  89,  90,  91,  92,  94,  95,  96,  97,
    98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
    114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129,
    129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 140, 141, 142, 143,
    144, 145, 146, 147, 148, 149, 149, 150, 151, 152, 153, 154, 155, 155, 156, 157,
    158, 159, 160, 161, 161, 162, 163, 164, 165, 166, 166, 167, 168, 169, 170, 171,
    171, 172, 173, 174, 175, 176, 176, 177, 178, 179, 180, 180, 181, 182, 183, 184,
    184, 185, 186, 187, 188, 188, 189, 190, 191, 192, 192, 193, 194, 195, 195, 196,
    197, 198, 199, 199, 200, 201, 202, 202, 203, 204, 205, 205, 206, 207, 208, 208,
    209, 210, 211, 211, 212, 213, 214, 214, 215, 216, 217, 217, 218, 219, 220, 220,
    221, 222, 223, 223, 224, 225, 225, 226, 227, 228, 228, 229, 230, 231, 231, 232,
    233, 233, 234, 235, 236, 236, 237, 238, 238, 239, 240, 241, 241, 242, 243, 243,
    244, 245, 245, 246, 247, 248, 248, 249, 250, 250, 251, 252, 252, 253, 254, 255,
};

/* Clamps to the destination (post-crop) plane's own bounds -- legacy clamped
 * against the full source frame instead, so border-pixel edge magnitude can
 * differ slightly. Accepted: the core has no access to the uncropped frame. */
static uint8_t gray_clamped(const uint8_t *gray, uint16_t width, uint16_t height, int x, int y)
{
    if (x < 0) x = 0;
    if (x >= width) x = width - 1;
    if (y < 0) y = 0;
    if (y >= height) y = height - 1;
    return gray[y * width + x];
}

static int dither_edge_atkinson(const uint8_t *gray, uint16_t width, uint16_t height, uint8_t *out_buf,
                                 int invert, void *scratch)
{
    int stride = (width + 7) / 8;

    /* Frame-mean adaptive split, gamma-corrected to match the luminance
     * channel it's compared against -- see the threshold-scope note above. */
    uint32_t sum = 0;
    uint32_t total = (uint32_t)width * height;
    for (uint32_t i = 0; i < total; i++) sum += gray[i];
    uint8_t black_thresh = edge_atkinson_gamma_lut[(uint8_t)(sum / total)];

    int16_t *error_buffer = (int16_t *)scratch;
    memset(error_buffer, 0, (size_t)(width + 4) * 3 * sizeof(int16_t));
    int16_t *curr_row = error_buffer + 2;
    int16_t *next_row1 = error_buffer + width + 6;
    int16_t *next_row2 = error_buffer + 2 * (width + 4) + 2;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int center = gray_clamped(gray, width, height, x, y);
            int sum8 = gray_clamped(gray, width, height, x - 1, y - 1) +
                       gray_clamped(gray, width, height, x, y - 1) +
                       gray_clamped(gray, width, height, x + 1, y - 1) +
                       gray_clamped(gray, width, height, x - 1, y) +
                       gray_clamped(gray, width, height, x + 1, y) +
                       gray_clamped(gray, width, height, x - 1, y + 1) +
                       gray_clamped(gray, width, height, x, y + 1) +
                       gray_clamped(gray, width, height, x + 1, y + 1);
            int edge_mag = 8 * center - sum8;
            if (edge_mag < 0) edge_mag = -edge_mag;

            uint8_t gamma_center = edge_atkinson_gamma_lut[center];
            int16_t lum = (int16_t)gamma_center + curr_row[x];
            if (lum < 0) lum = 0;
            if (lum > 255) lum = 255;

            int is_edge = edge_mag > EDGE_ATKINSON_THRESHOLD && gamma_center < EDGE_ATKINSON_MAX_BRIGHTNESS;
            uint8_t new_pixel = is_edge ? 0 : ((lum >= black_thresh) ? 255 : 0);
            int should_set = invert ? (new_pixel == 255) : (new_pixel == 0);
            if (should_set) set_bit(out_buf, stride, x, y);

            if (!is_edge) {
                int16_t error = lum - new_pixel;
                if (x < width - 1) curr_row[x + 1] += (int16_t)(error / 8);
                if (x < width - 2) curr_row[x + 2] += (int16_t)(error / 8);
                if (x > 0) next_row1[x - 1] += (int16_t)(error / 8);
                next_row1[x] += (int16_t)(error / 8);
                if (x < width - 1) next_row1[x + 1] += (int16_t)(error / 8);
                next_row2[x] += (int16_t)(error / 8);
            }
        }
        int16_t *tmp = curr_row; curr_row = next_row1; next_row1 = next_row2; next_row2 = tmp;
        memset(next_row2 - 2, 0, (size_t)(width + 4) * sizeof(int16_t));
    }
    return 0;
}

static int dither_gamma_serpentine(const uint8_t *gray, uint16_t width, uint16_t height, uint8_t *out_buf,
                                    int invert, void *scratch)
{
    int stride = (width + 7) / 8;

    int16_t *error_buffer = (int16_t *)scratch;
    memset(error_buffer, 0, (size_t)(width + 2) * 2 * sizeof(int16_t));
    int16_t *curr_row = error_buffer + 1;
    int16_t *next_row = error_buffer + width + 3;

    for (int y = 0; y < height; y++) {
        int direction = (y % 2 == 0) ? 1 : -1;
        int x = (direction == 1) ? 0 : width - 1;

        for (int count = 0; count < width; count++, x += direction) {
            uint8_t gamma_corrected = gamma_serpentine_gamma_lut[gray[y * width + x]];
            int16_t lum = (int16_t)gamma_corrected + curr_row[x];
            if (lum < 0) lum = 0;
            if (lum > 255) lum = 255;
            uint8_t new_pixel = (lum >= GAMMA_SERPENTINE_THRESHOLD) ? 255 : 0;
            int16_t error = lum - new_pixel;
            int should_set = invert ? (new_pixel == 255) : (new_pixel == 0);
            if (should_set) set_bit(out_buf, stride, x, y);

            if (x + direction >= 0 && x + direction < width)
                curr_row[x + direction] += (int16_t)((error * 7) / 16);
            if (y < height - 1) {
                if (x - direction >= 0 && x - direction < width)
                    next_row[x - direction] += (int16_t)((error * 3) / 16);
                next_row[x] += (int16_t)((error * 5) / 16);
                if (x + direction >= 0 && x + direction < width)
                    next_row[x + direction] += (int16_t)(error / 16);
            }
        }
        int16_t *tmp = curr_row; curr_row = next_row; next_row = tmp;
        memset(next_row - 1, 0, (size_t)(width + 2) * sizeof(int16_t));
    }
    return 0;
}

uint32_t tal_image_dither_scratch_size(TAL_IMAGE_MONO_METHOD_E method, uint16_t width)
{
    switch (method) {
    case TAL_IMAGE_MONO_MTH_FLOYD_STEINBERG:
    case TAL_IMAGE_MONO_MTH_GAMMA_SERPENTINE:
        return (uint32_t)(width + 2) * 2 * sizeof(int16_t);
    case TAL_IMAGE_MONO_MTH_STUCKI:
    case TAL_IMAGE_MONO_MTH_JARVIS:
    case TAL_IMAGE_MONO_MTH_EDGE_ATKINSON:
        return (uint32_t)(width + 4) * 3 * sizeof(int16_t);
    default:
        return 0;
    }
}

int tal_image_dither_gray_to_binary(const uint8_t *gray_buf, uint16_t width, uint16_t height,
                                     uint8_t *out_buf, uint32_t out_buf_size,
                                     TAL_IMAGE_MONO_METHOD_E method, uint8_t fixed_threshold,
                                     uint8_t invert_colors, void *scratch_buf, uint32_t scratch_buf_size)
{
    if (!gray_buf || !out_buf || width == 0 || height == 0) return -1;
    uint32_t need = (uint32_t)((width + 7) / 8) * height;
    if (out_buf_size < need) return -1;
    memset(out_buf, 0, need);

    switch (method) {
    case TAL_IMAGE_MONO_MTH_FIXED:
        return dither_threshold(gray_buf, width, height, out_buf, fixed_threshold, invert_colors);
    case TAL_IMAGE_MONO_MTH_ADAPTIVE:
        return dither_threshold(gray_buf, width, height, out_buf,
                                 calc_adaptive_threshold(gray_buf, width, height), invert_colors);
    case TAL_IMAGE_MONO_MTH_OTSU:
        return dither_threshold(gray_buf, width, height, out_buf,
                                 calc_otsu_threshold(gray_buf, width, height), invert_colors);
    case TAL_IMAGE_MONO_MTH_BAYER4_DITHER:
        return dither_bayer(gray_buf, width, height, out_buf, invert_colors, 4);
    case TAL_IMAGE_MONO_MTH_BAYER8_DITHER:
        return dither_bayer(gray_buf, width, height, out_buf, invert_colors, 8);
    case TAL_IMAGE_MONO_MTH_BAYER16_DITHER:
        return dither_bayer(gray_buf, width, height, out_buf, invert_colors, 16);
    case TAL_IMAGE_MONO_MTH_FLOYD_STEINBERG:
        if (scratch_buf_size < tal_image_dither_scratch_size(method, width)) return -1;
        return dither_floyd_steinberg(gray_buf, width, height, out_buf, invert_colors, scratch_buf);
    case TAL_IMAGE_MONO_MTH_STUCKI:
        if (scratch_buf_size < tal_image_dither_scratch_size(method, width)) return -1;
        return dither_stucki(gray_buf, width, height, out_buf, invert_colors, scratch_buf);
    case TAL_IMAGE_MONO_MTH_JARVIS:
        if (scratch_buf_size < tal_image_dither_scratch_size(method, width)) return -1;
        return dither_jarvis(gray_buf, width, height, out_buf, invert_colors, scratch_buf);
    case TAL_IMAGE_MONO_MTH_EDGE_ATKINSON:
        if (scratch_buf_size < tal_image_dither_scratch_size(method, width)) return -1;
        return dither_edge_atkinson(gray_buf, width, height, out_buf, invert_colors, scratch_buf);
    case TAL_IMAGE_MONO_MTH_GAMMA_SERPENTINE:
        if (scratch_buf_size < tal_image_dither_scratch_size(method, width)) return -1;
        return dither_gamma_serpentine(gray_buf, width, height, out_buf, invert_colors, scratch_buf);
    default:
        return -1;
    }
}
