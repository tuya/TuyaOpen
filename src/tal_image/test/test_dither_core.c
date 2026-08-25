/* src/tal_image/test/test_dither_core.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tal_image_dither_core.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("PASS: %s\n", msg); } \
} while (0)

/* Legacy formula, copied verbatim from the pre-unification code, used as the
 * regression oracle for ROTATE_90 -- this is the ONLY place this formula
 * should ever be duplicated again. */
static void legacy_rotate90_extract(const uint8_t *yuv422_data, int src_width, int src_height,
                                     uint8_t *gray_out, int dst_width, int dst_height)
{
    int crop_offset = (src_width - dst_height) / 2;
    for (int dst_y = 0; dst_y < dst_height; dst_y++) {
        for (int dst_x = 0; dst_x < dst_width; dst_x++) {
            int src_x = dst_y + crop_offset;
            int src_y = src_height - 1 - dst_x;
            uint8_t gray = 255;
            if (src_x >= 0 && src_x < src_width && src_y >= 0 && src_y < src_height) {
                gray = yuv422_data[src_y * src_width * 2 + src_x * 2 + 1];
            }
            gray_out[dst_y * dst_width + dst_x] = gray;
        }
    }
}

static void test_rotate90_matches_legacy(void)
{
    /* Real dimensions from camera_screen.c: 480x480 sensor, two real dst sizes. */
    const int src_w = 480, src_h = 480;
    uint8_t *yuv = malloc((size_t)src_w * src_h * 2);
    for (int i = 0; i < src_w * src_h; i++) {
        yuv[i * 2] = (uint8_t)(i * 37);       /* U/V: irrelevant, extractor ignores it */
        yuv[i * 2 + 1] = (uint8_t)(i * 73 + 11); /* deterministic pseudo-random luma */
    }

    int cases[2][2] = { {240, 168}, {384, 384} };
    for (int c = 0; c < 2; c++) {
        int dst_w = cases[c][0], dst_h = cases[c][1];
        uint8_t *expected = malloc((size_t)dst_w * dst_h);
        uint8_t *actual = malloc((size_t)dst_w * dst_h);
        legacy_rotate90_extract(yuv, src_w, src_h, expected, dst_w, dst_h);
        tal_image_extract_gray_from_yuv422(yuv, src_w, src_h, actual, dst_w, dst_h, TAL_IMAGE_ROTATE_90);
        char msg[128];
        snprintf(msg, sizeof(msg), "ROTATE_90 matches legacy formula for %dx%d dst", dst_w, dst_h);
        CHECK(memcmp(expected, actual, (size_t)dst_w * dst_h) == 0, msg);
        free(expected);
        free(actual);
    }
    free(yuv);
}

/* 4x4 source with a distinct marker in each corner: TL=10, TR=20, BL=30, BR=40,
 * background=0. Used to sanity-check that each rotation lands markers where
 * expected, independent of any legacy formula. */
static void build_marker_yuv(uint8_t *yuv, int w, int h)
{
    memset(yuv, 0, (size_t)w * h * 2);
    for (int i = 0; i < w * h; i++) yuv[i * 2 + 1] = 0;
    yuv[(0 * w + 0) * 2 + 1] = 10;         /* top-left */
    yuv[(0 * w + (w - 1)) * 2 + 1] = 20;   /* top-right */
    yuv[((h - 1) * w + 0) * 2 + 1] = 30;   /* bottom-left */
    yuv[((h - 1) * w + (w - 1)) * 2 + 1] = 40; /* bottom-right */
}

static void test_rotate0_identity(void)
{
    uint8_t yuv[4 * 4 * 2];
    build_marker_yuv(yuv, 4, 4);
    uint8_t gray[16];
    tal_image_extract_gray_from_yuv422(yuv, 4, 4, gray, 4, 4, TAL_IMAGE_ROTATE_0);
    CHECK(gray[0 * 4 + 0] == 10, "ROTATE_0 top-left marker unchanged");
    CHECK(gray[0 * 4 + 3] == 20, "ROTATE_0 top-right marker unchanged");
    CHECK(gray[3 * 4 + 0] == 30, "ROTATE_0 bottom-left marker unchanged");
    CHECK(gray[3 * 4 + 3] == 40, "ROTATE_0 bottom-right marker unchanged");
}

static void test_rotate180_flips_both_axes(void)
{
    uint8_t yuv[4 * 4 * 2];
    build_marker_yuv(yuv, 4, 4);
    uint8_t gray[16];
    tal_image_extract_gray_from_yuv422(yuv, 4, 4, gray, 4, 4, TAL_IMAGE_ROTATE_180);
    /* top-left source marker should land at bottom-right of output, etc. */
    CHECK(gray[3 * 4 + 3] == 10, "ROTATE_180 top-left marker lands bottom-right");
    CHECK(gray[3 * 4 + 0] == 20, "ROTATE_180 top-right marker lands bottom-left");
    CHECK(gray[0 * 4 + 3] == 30, "ROTATE_180 bottom-left marker lands top-right");
    CHECK(gray[0 * 4 + 0] == 40, "ROTATE_180 bottom-right marker lands top-left");
}

static void test_rotate270_matches_three_legacy_steps(void)
{
    /* Expected values verified independently by composing the legacy R90 step
     * three times in a numpy simulation (R90 applied to its own output, 3x),
     * confirming R90 applied 4x returns the identity -- i.e. it's a genuine
     * 90-degree rotation step, not an ad-hoc formula. */
    uint8_t yuv[4 * 4 * 2];
    build_marker_yuv(yuv, 4, 4);
    uint8_t gray[16];
    tal_image_extract_gray_from_yuv422(yuv, 4, 4, gray, 4, 4, TAL_IMAGE_ROTATE_270);
    CHECK(gray[0 * 4 + 0] == 20, "ROTATE_270 top-left = source top-right");
    CHECK(gray[0 * 4 + 3] == 40, "ROTATE_270 top-right = source bottom-right");
    CHECK(gray[3 * 4 + 0] == 10, "ROTATE_270 bottom-left = source top-left");
    CHECK(gray[3 * 4 + 3] == 30, "ROTATE_270 bottom-right = source bottom-left");
}

/* Rotation-group consistency, independent of any legacy formula: ROTATE_90
 * twice == ROTATE_180 once, and four times == identity. */
static void gray_to_fake_yuv422(const uint8_t *gray, int w, int h, uint8_t *yuv_out)
{
    for (int i = 0; i < w * h; i++) {
        yuv_out[i * 2]     = 0;
        yuv_out[i * 2 + 1] = gray[i];
    }
}

static void test_rotate90_twice_equals_rotate180(void)
{
    uint8_t yuv[4 * 4 * 2];
    build_marker_yuv(yuv, 4, 4);

    uint8_t once[16];
    tal_image_extract_gray_from_yuv422(yuv, 4, 4, once, 4, 4, TAL_IMAGE_ROTATE_90);

    uint8_t once_yuv[4 * 4 * 2];
    gray_to_fake_yuv422(once, 4, 4, once_yuv);
    uint8_t twice[16];
    tal_image_extract_gray_from_yuv422(once_yuv, 4, 4, twice, 4, 4, TAL_IMAGE_ROTATE_90);

    uint8_t direct180[16];
    tal_image_extract_gray_from_yuv422(yuv, 4, 4, direct180, 4, 4, TAL_IMAGE_ROTATE_180);

    CHECK(memcmp(twice, direct180, 16) == 0, "ROTATE_90 composed twice == ROTATE_180 applied once");
}

static void test_rotate90_four_times_is_identity(void)
{
    uint8_t yuv[4 * 4 * 2];
    build_marker_yuv(yuv, 4, 4);

    uint8_t cur[16] = {0};
    uint8_t stage_yuv[4 * 4 * 2];
    memcpy(stage_yuv, yuv, sizeof(yuv));

    uint8_t original_gray[16];
    for (int i = 0; i < 16; i++) original_gray[i] = yuv[i * 2 + 1];

    for (int step = 0; step < 4; step++) {
        tal_image_extract_gray_from_yuv422(stage_yuv, 4, 4, cur, 4, 4, TAL_IMAGE_ROTATE_90);
        gray_to_fake_yuv422(cur, 4, 4, stage_yuv);
    }

    CHECK(memcmp(cur, original_gray, 16) == 0, "ROTATE_90 composed four times returns the identity");
}

static void test_fixed_threshold(void)
{
    /* 1 row, 8 pixels: half dark half light, threshold 128 */
    uint8_t gray[8] = {0, 50, 100, 127, 128, 200, 255, 255};
    uint8_t out[1] = {0};
    tal_image_dither_gray_to_binary(gray, 8, 1, out, 1, TAL_IMAGE_MONO_MTH_FIXED, 128, 0, NULL, 0);
    /* invert=0 (printer): lum < threshold -> bit=1. Pixels 0-3 (0,50,100,127) < 128 -> bits 7..4 set. */
    CHECK(out[0] == 0xF0, "FIXED threshold packs bits MSB-first, dark=1 for invert=0");
}

static void test_bayer4_known_pattern(void)
{
    /* All-mid-gray (128) 2x2 image against the 2x2 Bayer matrix {{0,2},{3,1}}.
     * gray_level = 128/85 = 1. invert=0: should_set = (1 < bayer_value) || (128<32, false).
     * bayer[0][0]=0 -> 1<0 false. bayer[0][1]=2 -> 1<2 true. bayer[1][0]=3 -> true. bayer[1][1]=1 -> 1<1 false. */
    uint8_t gray[4] = {128, 128, 128, 128};
    uint8_t out[2] = {0, 0}; /* stride = (2+7)/8 = 1 byte per row */
    tal_image_dither_gray_to_binary(gray, 2, 2, out, 2, TAL_IMAGE_MONO_MTH_BAYER4_DITHER, 0, 0, NULL, 0);
    CHECK((out[0] & 0x80) == 0, "BAYER4 (0,0) not set");
    CHECK((out[0] & 0x40) != 0, "BAYER4 (1,0) set");
    CHECK((out[1] & 0x80) != 0, "BAYER4 (0,1) set");
    CHECK((out[1] & 0x40) == 0, "BAYER4 (1,1) not set");
}

static void test_adaptive_threshold_uses_mean(void)
{
    uint8_t gray[4] = {0, 0, 255, 255}; /* mean = 127 */
    uint8_t out[1] = {0};
    tal_image_dither_gray_to_binary(gray, 4, 1, out, 1, TAL_IMAGE_MONO_MTH_ADAPTIVE, 0, 0, NULL, 0);
    /* invert=0: lum < 127 -> bit=1. Pixels 0,1 (0,0) < 127 -> set; pixels 2,3 (255,255) not. */
    CHECK(out[0] == 0xC0, "ADAPTIVE thresholds at the frame mean (127)");
}

static void test_bayer8_all_white_no_speckle(void)
{
    /* All-white 3x3 image (lum=255 everywhere) against the 3x3 Bayer matrix.
     * With the fix, gray_level = (255 * 8 / 255) = 8 (max), so all comparisons
     * should yield false for invert=0, producing no black speckles. */
    uint8_t gray[9] = {255, 255, 255, 255, 255, 255, 255, 255, 255};
    uint8_t out[3] = {0, 0, 0}; /* stride = (3+7)/8 = 1 byte per row */
    tal_image_dither_gray_to_binary(gray, 3, 3, out, 3, TAL_IMAGE_MONO_MTH_BAYER8_DITHER, 0, 0, NULL, 0);
    CHECK(out[0] == 0, "BAYER8 all-white row 0 has no black speckles");
    CHECK(out[1] == 0, "BAYER8 all-white row 1 has no black speckles");
    CHECK(out[2] == 0, "BAYER8 all-white row 2 has no black speckles");
}

static void test_floyd_steinberg_preserves_average_tone(void)
{
    /* 16x16 uniform mid-gray (100/255 ~= 39% bright, i.e. 61% dark) should
     * dither to roughly (255-100)/255 ~= 61% black pixels (invert=0, dark->1)
     * if error diffusion is conserving tone -- NOT 39%, which is the
     * brightness fraction, not its complement. */
    const int w = 16, h = 16;
    uint8_t gray[16 * 16];
    memset(gray, 100, sizeof(gray));
    uint8_t out[16 * 16 / 8];
    void *scratch = malloc(tal_image_dither_scratch_size(TAL_IMAGE_MONO_MTH_FLOYD_STEINBERG, w));
    tal_image_dither_gray_to_binary(gray, w, h, out, sizeof(out), TAL_IMAGE_MONO_MTH_FLOYD_STEINBERG,
                                     0, 0, scratch, tal_image_dither_scratch_size(TAL_IMAGE_MONO_MTH_FLOYD_STEINBERG, w));
    free(scratch);

    int black_count = 0;
    for (int i = 0; i < w * h; i++) {
        int byte = i / 8, bit = 7 - (i % 8);
        if (out[byte] & (1 << bit)) black_count++;
    }
    double black_fraction = (double)black_count / (w * h);
    /* (255-100)/255 = 60.8% dark. Allow +/-10 percentage points for a small 16x16 sample. */
    CHECK(black_fraction > 0.50 && black_fraction < 0.70, "Floyd-Steinberg on uniform gray-100 gives ~61% black");
}

static void test_edge_atkinson_locks_sharp_edge_to_black(void)
{
    /* 8x1 row: left half bright (250), right half dark (5) -- a sharp edge
     * in the middle. The edge pixels around the transition should be forced
     * black (bit=1 for invert=0) regardless of the adaptive threshold. */
    const int w = 8, h = 1;
    uint8_t gray[8] = {250, 250, 250, 250, 5, 5, 5, 5};
    uint8_t out[1] = {0};
    void *scratch = malloc(tal_image_dither_scratch_size(TAL_IMAGE_MONO_MTH_EDGE_ATKINSON, w));
    tal_image_dither_gray_to_binary(gray, w, h, out, sizeof(out), TAL_IMAGE_MONO_MTH_EDGE_ATKINSON,
                                     0, 0, scratch, tal_image_dither_scratch_size(TAL_IMAGE_MONO_MTH_EDGE_ATKINSON, w));
    free(scratch);
    /* Pixels 3 and 4 straddle the edge and are dark-side-adjacent -> expect bit 4 (x=3) or bit 3 (x=4) set. */
    CHECK((out[0] & 0x08) != 0 || (out[0] & 0x10) != 0, "Edge-Atkinson locks something black at a sharp transition");
}

static void test_gamma_serpentine_brightens_dark_uniform_image(void)
{
    /* Uniform dark gray (70/255, mean luma ~27%) should dither to LESS than
     * 73% black (which is what a non-gamma-corrected diffusion would give)
     * because gamma=1.45 brightens shadows before thresholding. */
    const int w = 16, h = 16;
    uint8_t gray[16 * 16];
    memset(gray, 70, sizeof(gray));
    uint8_t out[16 * 16 / 8];
    void *scratch = malloc(tal_image_dither_scratch_size(TAL_IMAGE_MONO_MTH_GAMMA_SERPENTINE, w));
    tal_image_dither_gray_to_binary(gray, w, h, out, sizeof(out), TAL_IMAGE_MONO_MTH_GAMMA_SERPENTINE,
                                     0, 0, scratch, tal_image_dither_scratch_size(TAL_IMAGE_MONO_MTH_GAMMA_SERPENTINE, w));
    free(scratch);

    int black_count = 0;
    for (int i = 0; i < w * h; i++) {
        int byte = i / 8, bit = 7 - (i % 8);
        if (out[byte] & (1 << bit)) black_count++;
    }
    double black_fraction = (double)black_count / (w * h);
    CHECK(black_fraction < 0.65, "Gamma-Serpentine brightens gray-70 to well under the un-gamma-corrected 73% black");
}

int main(void)
{
    test_rotate90_matches_legacy();
    test_rotate0_identity();
    test_rotate180_flips_both_axes();
    test_rotate270_matches_three_legacy_steps();
    test_rotate90_twice_equals_rotate180();
    test_rotate90_four_times_is_identity();
    test_fixed_threshold();
    test_bayer4_known_pattern();
    test_adaptive_threshold_uses_mean();
    test_bayer8_all_white_no_speckle();
    test_floyd_steinberg_preserves_average_tone();
    test_edge_atkinson_locks_sharp_edge_to_black();
    test_gamma_serpentine_brightens_dark_uniform_image();

    if (failures) {
        printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
