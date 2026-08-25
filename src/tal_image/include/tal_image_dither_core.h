/**
 * @file tal_image_dither_core.h
 * @brief Shared monochrome dithering core: 11 methods, plus a rotation-aware
 * YUV422 grayscale extractor, used by both the YUV422 camera path and the
 * JPEG path.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __TAL_IMAGE_DITHER_CORE_H__
#define __TAL_IMAGE_DITHER_CORE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Canonical home of the dithering method enum -- shared by the YUV422
 * camera path and the JPEG printer path so both select from the same list. */
typedef enum {
    TAL_IMAGE_MONO_MTH_FIXED = 0,        /* Fixed threshold */
    TAL_IMAGE_MONO_MTH_ADAPTIVE,         /* Adaptive threshold */
    TAL_IMAGE_MONO_MTH_OTSU,             /* Otsu's method */
    TAL_IMAGE_MONO_MTH_BAYER8_DITHER,    /* 8-level grayscale Bayer dithering (3x3) */
    TAL_IMAGE_MONO_MTH_BAYER4_DITHER,    /* 4-level grayscale Bayer dithering (2x2) */
    TAL_IMAGE_MONO_MTH_BAYER16_DITHER,   /* 16-level grayscale Bayer dithering (4x4) */
    TAL_IMAGE_MONO_MTH_FLOYD_STEINBERG,  /* Floyd-Steinberg error diffusion */
    TAL_IMAGE_MONO_MTH_STUCKI,           /* Stucki error diffusion */
    TAL_IMAGE_MONO_MTH_JARVIS,           /* Jarvis-Judice-Ninke error diffusion */
    TAL_IMAGE_MONO_MTH_EDGE_ATKINSON,    /* Edge-locked Atkinson dithering */
    TAL_IMAGE_MONO_MTH_GAMMA_SERPENTINE, /* Gamma-corrected serpentine Floyd-Steinberg */
    TAL_IMAGE_MONO_MTH_COUNT
} TAL_IMAGE_MONO_METHOD_E;

/* Clockwise (CW) rotation applied before cropping, to compensate for a
 * camera module mounted at an angle relative to the screen. */
typedef enum {
    TAL_IMAGE_ROTATE_0 = 0,
    TAL_IMAGE_ROTATE_90,
    TAL_IMAGE_ROTATE_180,
    TAL_IMAGE_ROTATE_270,
} TAL_IMAGE_ROTATE_E;

/**
 * @brief Extract a dst_width x dst_height 8-bit grayscale plane from a YUV422
 * frame, applying a CW rotation and a centered crop (ROTATE_90 uses the
 * legacy asymmetric crop for backward compatibility -- see .c).
 *
 * @param yuv422_data Source YUV422 buffer (2 bytes/pixel, Y at odd offset).
 * @param src_width Source width.
 * @param src_height Source height.
 * @param gray_out Destination buffer, must hold dst_width*dst_height bytes.
 * @param dst_width Destination width (post-rotation).
 * @param dst_height Destination height (post-rotation).
 * @param rotate CW rotation to apply before cropping.
 */
void tal_image_extract_gray_from_yuv422(const uint8_t *yuv422_data, int src_width, int src_height,
                                         uint8_t *gray_out, int dst_width, int dst_height,
                                         TAL_IMAGE_ROTATE_E rotate);

/**
 * @brief Bytes of scratch memory tal_image_dither_gray_to_binary() needs for
 * the given method at the given width. Returns 0 for methods that need none
 * (all threshold/Bayer methods). Caller allocates and passes this buffer in
 * -- the core never allocates memory itself, so it stays host-testable.
 */
uint32_t tal_image_dither_scratch_size(TAL_IMAGE_MONO_METHOD_E method, uint16_t width);

/**
 * @brief Dither an 8-bit grayscale plane to a 1bpp MSB-first bitmap.
 *
 * @param gray_buf width*height grayscale input.
 * @param out_buf Output buffer, must hold ((width+7)/8)*height bytes.
 * @param out_buf_size Size of out_buf, for a bounds check.
 * @param fixed_threshold Only used by TAL_IMAGE_MONO_MTH_FIXED.
 * @param invert_colors 1: bit=1->white (LVGL), 0: bit=1->black (printer).
 * @param scratch_buf Caller-provided scratch, sized via tal_image_dither_scratch_size(). May be NULL if that returns 0.
 * @param scratch_buf_size Size of scratch_buf, for a bounds check.
 * @return 0 on success, negative on error.
 */
int tal_image_dither_gray_to_binary(const uint8_t *gray_buf, uint16_t width, uint16_t height,
                                     uint8_t *out_buf, uint32_t out_buf_size,
                                     TAL_IMAGE_MONO_METHOD_E method, uint8_t fixed_threshold,
                                     uint8_t invert_colors, void *scratch_buf, uint32_t scratch_buf_size);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_IMAGE_DITHER_CORE_H__ */
