/**
 * @file tal_image_jpeg_noise_clamp.h
 * @brief Anti-dot-row noise clamp for the JPEG-to-bitmap path, split out as a
 * small dependency-free module (stdint.h only) so it can be unit-tested on
 * the host -- tal_image_jpeg_codec.c itself pulls in tuya_cloud_types.h and
 * cannot be built outside a real TuyaOpen project.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __TAL_IMAGE_JPEG_NOISE_CLAMP_H__
#define __TAL_IMAGE_JPEG_NOISE_CLAMP_H__

#include <stdint.h>
#include "tal_image_dither_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Whether tal_image_jpeg_clamp_noise() should run before dithering
 * with the given method. Only the error-diffusion methods (which propagate
 * quantization noise to neighbouring pixels) need it; Threshold/Bayer/
 * Adaptive/Otsu make an independent per-pixel decision and never propagate
 * error, so clamping first would only distort their input (see
 * tal_image_jpeg_clamp_noise()'s doc for why this matters for Adaptive/Otsu
 * specifically).
 *
 * @param method Dithering method.
 * @return 1 if tal_image_jpeg_clamp_noise() should be applied, 0 otherwise.
 */
int tal_image_jpeg_should_clamp_noise(TAL_IMAGE_MONO_METHOD_E method);

/**
 * @brief Snap near-white/near-black pixels to pure white/black in place.
 *
 * JPEG DCT quantization introduces +-15-20 noise on flat (white/black)
 * regions. Without this, near-white pixels (e.g. 238 instead of 255) carry a
 * small negative error that error-diffusion dithering diffuses to
 * neighbours; the accumulated error eventually flips a pixel black,
 * producing visible dot rows on what should be a clean white background.
 * Snapping clearly-white/clearly-black pixels to their extremes first makes
 * that diffused error zero so it cannot scatter dots.
 *
 * Applying this to Adaptive/Otsu would distort the very histogram/mean
 * statistics those methods derive their black/white split from -- do not
 * call this unless tal_image_jpeg_should_clamp_noise() said to.
 *
 * @param gray_buf width*height grayscale plane, modified in place.
 * @param width Plane width.
 * @param height Plane height.
 * @param threshold Center of the clamp band (typically the same value passed
 * as the overall binarization threshold, though this function only uses it
 * to derive the clamp band, never as a binarization split itself).
 */
void tal_image_jpeg_clamp_noise(uint8_t *gray_buf, uint16_t width, uint16_t height, uint8_t threshold);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_IMAGE_JPEG_NOISE_CLAMP_H__ */
