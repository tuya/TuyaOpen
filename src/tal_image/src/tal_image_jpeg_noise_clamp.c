/**
 * @file tal_image_jpeg_noise_clamp.c
 * @brief See tal_image_jpeg_noise_clamp.h.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include "tal_image_jpeg_noise_clamp.h"

int tal_image_jpeg_should_clamp_noise(TAL_IMAGE_MONO_METHOD_E method)
{
    return (method == TAL_IMAGE_MONO_MTH_FLOYD_STEINBERG) ||
           (method == TAL_IMAGE_MONO_MTH_STUCKI) ||
           (method == TAL_IMAGE_MONO_MTH_JARVIS) ||
           (method == TAL_IMAGE_MONO_MTH_EDGE_ATKINSON) ||
           (method == TAL_IMAGE_MONO_MTH_GAMMA_SERPENTINE);
}

void tal_image_jpeg_clamp_noise(uint8_t *gray_buf, uint16_t width, uint16_t height, uint8_t threshold)
{
    int16_t thr = (int16_t)threshold;
    int16_t white_clamp = thr + (255 - thr) / 3; /* ~85% of way to white */
    int16_t black_clamp = thr / 3;               /* ~33% of way to black */
    uint32_t total = (uint32_t)width * height;

    for (uint32_t i = 0; i < total; i++) {
        int16_t px = (int16_t)gray_buf[i];
        if (px >= white_clamp) {
            px = 255;
        } else if (px <= black_clamp) {
            px = 0;
        }
        gray_buf[i] = (uint8_t)px;
    }
}
