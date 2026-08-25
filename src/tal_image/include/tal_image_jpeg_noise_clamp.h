/**
 * @file tal_image_jpeg_noise_clamp.h
 * @brief Anti-dot-row noise clamp for the JPEG-to-bitmap path. Split out as a
 * small dependency-free module (stdint.h only) so it's unit-testable on the
 * host, unlike tal_image_jpeg_codec.c which pulls in tuya_cloud_types.h.
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
 * with the given method. Only the error-diffusion methods propagate
 * quantization noise to neighbours; Threshold/Bayer/Adaptive/Otsu make an
 * independent per-pixel decision, so clamping first would only distort them.
 *
 * @param method Dithering method.
 * @return 1 if tal_image_jpeg_clamp_noise() should be applied, 0 otherwise.
 */
int tal_image_jpeg_should_clamp_noise(TAL_IMAGE_MONO_METHOD_E method);

/**
 * @brief Snap near-white/near-black pixels to pure white/black in place.
 *
 * JPEG DCT quantization leaves +-15-20 noise on flat regions; error-diffusion
 * dithering would diffuse that noise until it flips a pixel, producing
 * visible dot rows. Snapping near-extreme pixels to their extremes first
 * makes the diffused error zero. Do not call this for Adaptive/Otsu -- it
 * would distort the histogram/mean their split is derived from.
 *
 * @param gray_buf width*height grayscale plane, modified in place.
 * @param width Plane width.
 * @param height Plane height.
 * @param threshold Center of the clamp band (only used to derive the band,
 * never as a binarization split itself).
 */
void tal_image_jpeg_clamp_noise(uint8_t *gray_buf, uint16_t width, uint16_t height, uint8_t threshold);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_IMAGE_JPEG_NOISE_CLAMP_H__ */
