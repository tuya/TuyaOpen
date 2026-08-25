/* src/tal_image/src/tal_image_dither_core.c */
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
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
                /* 270 CCW = 90 CW. Centered crop (new capability, no legacy to match). */
                src_x = (src_width - 1) - (dst_y + crop_x);
                src_y = dst_x + crop_y;
                break;
            case TAL_IMAGE_ROTATE_90:
            default:
                /* Legacy formula from the pre-unification code, preserved verbatim:
                 * crop_offset = (src_width - dst_height) / 2 applied only to src_x;
                 * src_y has NO offset (asymmetric crop -- intentional, matches
                 * production behavior for the real dst<=src combos this ships with). */
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

/*
 * Threshold-scope note (Adaptive / Otsu / Edge-Atkinson):
 *
 * calc_adaptive_threshold() and calc_otsu_threshold() below, and the
 * frame-mean threshold computed inline in dither_edge_atkinson(), all derive
 * their statistics from `gray`, which by the time it reaches this file is
 * already the DESTINATION plane -- i.e. it has already been rotated and
 * cropped by tal_image_extract_gray_from_yuv422() to just the pixels that
 * will actually be displayed/printed.
 *
 * The pre-unification, app-local implementation computed the equivalent
 * statistics over the FULL source camera frame, before cropping, so pixels
 * that were discarded by the crop still influenced the threshold.
 *
 * This is a real, intentional behavior change introduced by the new
 * extract-then-dither architecture, and it affects exactly these three of
 * the 11 methods (all other methods are either fixed/local and unaffected by
 * this scope, or shown byte-identical to the legacy behavior otherwise).
 * Arguably it is the more correct behavior -- the threshold now reflects
 * what's actually on screen rather than pixels that were thrown away -- but
 * it is a visible difference from the old output and is called out here so
 * it isn't mistaken for an accidental regression.
 */
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
                gray_level = (uint8_t)((uint16_t)lum * 8 / 255);
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
#define EDGE_ATKINSON_GAMMA          2.0f
#define GAMMA_SERPENTINE_GAMMA       1.45f
#define GAMMA_SERPENTINE_THRESHOLD   128

static void build_gamma_lut(uint8_t *lut, float gamma)
{
    for (int i = 0; i < 256; i++) {
        float v = powf((float)i / 255.0f, 1.0f / gamma) * 255.0f;
        lut[i] = (uint8_t)(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v));
    }
}

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
    static uint8_t gamma_lut[256];
    static bool gamma_lut_ready = false;
    if (!gamma_lut_ready) {
        build_gamma_lut(gamma_lut, EDGE_ATKINSON_GAMMA);
        gamma_lut_ready = true;
    }

    /* Frame-mean adaptive black/white split, gamma-corrected to match the
     * gamma-corrected luminance channel it's compared against. Computed over
     * the destination (post-crop/rotate) plane -- see the threshold-scope
     * note above calc_adaptive_threshold(). */
    uint32_t sum = 0;
    uint32_t total = (uint32_t)width * height;
    for (uint32_t i = 0; i < total; i++) sum += gray[i];
    uint8_t black_thresh = gamma_lut[(uint8_t)(sum / total)];

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

            uint8_t gamma_center = gamma_lut[center];
            int16_t lum = (int16_t)gamma_center + curr_row[x];
            if (lum < 0) lum = 0;
            if (lum > 255) lum = 255;

            bool is_edge = edge_mag > EDGE_ATKINSON_THRESHOLD && gamma_center < EDGE_ATKINSON_MAX_BRIGHTNESS;
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
    static uint8_t gamma_lut[256];
    static bool gamma_lut_ready = false;
    if (!gamma_lut_ready) {
        build_gamma_lut(gamma_lut, GAMMA_SERPENTINE_GAMMA);
        gamma_lut_ready = true;
    }

    int16_t *error_buffer = (int16_t *)scratch;
    memset(error_buffer, 0, (size_t)(width + 2) * 2 * sizeof(int16_t));
    int16_t *curr_row = error_buffer + 1;
    int16_t *next_row = error_buffer + width + 3;

    for (int y = 0; y < height; y++) {
        int direction = (y % 2 == 0) ? 1 : -1;
        int x = (direction == 1) ? 0 : width - 1;

        for (int count = 0; count < width; count++, x += direction) {
            uint8_t gamma_corrected = gamma_lut[gray[y * width + x]];
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
