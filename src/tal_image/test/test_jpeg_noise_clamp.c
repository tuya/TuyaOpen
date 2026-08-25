/* src/tal_image/test/test_jpeg_noise_clamp.c
 *
 * Host-side coverage for tal_image_jpeg_noise_clamp.h. Not a legacy oracle
 * (the old per-pixel-in-loop clamp order is an accepted, documented behavior
 * change, not a preserved guarantee) -- just pins down this module's own
 * contract: which methods get clamped, and what the clamp band does.
 */
#include <stdio.h>
#include <string.h>
#include "tal_image_dither_core.h"
#include "tal_image_jpeg_noise_clamp.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("PASS: %s\n", msg); } \
} while (0)

static void test_should_clamp_matches_error_diffusion_methods(void)
{
    static const TAL_IMAGE_MONO_METHOD_E clamp_methods[] = {
        TAL_IMAGE_MONO_MTH_FLOYD_STEINBERG,
        TAL_IMAGE_MONO_MTH_STUCKI,
        TAL_IMAGE_MONO_MTH_JARVIS,
        TAL_IMAGE_MONO_MTH_EDGE_ATKINSON,
        TAL_IMAGE_MONO_MTH_GAMMA_SERPENTINE,
    };
    static const TAL_IMAGE_MONO_METHOD_E no_clamp_methods[] = {
        TAL_IMAGE_MONO_MTH_FIXED,
        TAL_IMAGE_MONO_MTH_ADAPTIVE,
        TAL_IMAGE_MONO_MTH_OTSU,
        TAL_IMAGE_MONO_MTH_BAYER8_DITHER,
        TAL_IMAGE_MONO_MTH_BAYER4_DITHER,
        TAL_IMAGE_MONO_MTH_BAYER16_DITHER,
    };

    for (size_t i = 0; i < sizeof(clamp_methods) / sizeof(clamp_methods[0]); i++) {
        CHECK(tal_image_jpeg_should_clamp_noise(clamp_methods[i]) != 0,
              "error-diffusion method is clamped");
    }
    for (size_t i = 0; i < sizeof(no_clamp_methods) / sizeof(no_clamp_methods[0]); i++) {
        CHECK(tal_image_jpeg_should_clamp_noise(no_clamp_methods[i]) == 0,
              "threshold/Bayer method is NOT clamped");
    }
    CHECK(sizeof(clamp_methods) / sizeof(clamp_methods[0]) +
          sizeof(no_clamp_methods) / sizeof(no_clamp_methods[0]) == TAL_IMAGE_MONO_MTH_COUNT,
          "every method is covered by exactly one of the two lists above");
}

static void test_clamp_snaps_near_extremes_leaves_midtones(void)
{
    /* threshold=128 (the value both in-tree callers use): white_clamp =
     * 128 + (255-128)/3 = 170, black_clamp = 128/3 = 42. */
    uint8_t gray[8] = {0, 10, 42, 43, 150, 169, 170, 255};
    tal_image_jpeg_clamp_noise(gray, 8, 1, 128);

    CHECK(gray[0] == 0,   "0 (already black) stays 0");
    CHECK(gray[1] == 0,   "10 (<= black_clamp) snaps to 0");
    CHECK(gray[2] == 0,   "42 (== black_clamp) snaps to 0");
    CHECK(gray[3] == 43,  "43 (> black_clamp) is left alone");
    CHECK(gray[4] == 150, "150 (deep midtone) is left alone");
    CHECK(gray[5] == 169, "169 (< white_clamp) is left alone");
    CHECK(gray[6] == 255, "170 (== white_clamp) snaps to 255");
    CHECK(gray[7] == 255, "255 (already white) stays 255");
}

static void test_clamp_band_follows_threshold(void)
{
    /* threshold=200: white_clamp = 200 + 55/3 = 218, black_clamp = 66. A
     * pixel that was left alone at threshold=128 (150) is still a midtone
     * here and must still be left alone; the bands themselves must move. */
    uint8_t gray[4] = {60, 70, 217, 218};
    tal_image_jpeg_clamp_noise(gray, 4, 1, 200);

    CHECK(gray[0] == 0,   "60 (<= black_clamp=66 at threshold=200) snaps to 0");
    CHECK(gray[1] == 70,  "70 (> black_clamp=66 at threshold=200) is left alone");
    CHECK(gray[2] == 217, "217 (< white_clamp=218 at threshold=200) is left alone");
    CHECK(gray[3] == 255, "218 (== white_clamp=218 at threshold=200) snaps to 255");
}

static void test_clamp_covers_full_plane_not_just_first_row(void)
{
    uint8_t gray[3 * 3] = {
        255, 255, 255,
        128, 240, 128,
        10,  128, 10,
    };
    tal_image_jpeg_clamp_noise(gray, 3, 3, 128);
    CHECK(gray[3 * 1 + 1] == 255, "row 1's near-white pixel is clamped, not just row 0");
    CHECK(gray[3 * 2 + 0] == 0,   "row 2's near-black pixel is clamped, not just row 0");
}

int main(void)
{
    test_should_clamp_matches_error_diffusion_methods();
    test_clamp_snaps_near_extremes_leaves_midtones();
    test_clamp_band_follows_threshold();
    test_clamp_covers_full_plane_not_just_first_row();

    if (failures == 0) {
        printf("\nAll checks passed\n");
        return 0;
    }
    printf("\n%d check(s) FAILED\n", failures);
    return 1;
}
