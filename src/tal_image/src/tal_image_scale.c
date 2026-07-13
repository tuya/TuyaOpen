/**
 * @file tal_image_scale.c
 * @brief Raw pixel image scaling implementation.
 *
 * Supports RGB888, RGB565, YUV422 and grayscale formats with nearest-neighbor
 * and bilinear interpolation. Bicubic falls back to bilinear.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include <string.h>
#include "tal_image_scale.h"
#include "tal_image_jpeg_codec.h"
#include "tuya_error_code.h"
#include "tal_memory.h"
#include "tal_log.h"


/***********************************************************
************************macro define************************
***********************************************************/
#define SCALE_FP_SHIFT  16u
#define SCALE_FP_ONE    (1u << SCALE_FP_SHIFT)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void (*__scale_fn_t)(const uint8_t *, uint16_t, uint16_t,
                              uint8_t *, uint16_t, uint16_t);

typedef OPERATE_RET (*__jpeg_decode_fn_t)(const uint8_t *, uint32_t,
                                          TAL_IMAGE_JPEG_OUTPUT_T *);
typedef OPERATE_RET (*__image_scale_fn_t)(const TAL_IMAGE_SCALE_IN_T *,
                                          TAL_IMAGE_SCALE_OUT_T *);

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Resolve output width/height from SIZE or RATIO mode.
 */
static OPERATE_RET __resolve_out_size(const TAL_IMAGE_SCALE_IN_T *in,
                                      uint16_t *out_w, uint16_t *out_h)
{
    if (in->mode == TAL_IMAGE_SCALE_MODE_SIZE) {
        if (in->out_width == 0 || in->out_height == 0) {
            PR_ERR("out_width/out_height must be non-zero");
            return OPRT_INVALID_PARM;
        }
        *out_w = in->out_width;
        *out_h = in->out_height;
    } else {
        if (in->ratio_x == 0 || in->ratio_y == 0) {
            PR_ERR("ratio_x/ratio_y must be non-zero");
            return OPRT_INVALID_PARM;
        }
        *out_w = (uint16_t)((uint32_t)in->width  * in->ratio_x / 100);
        *out_h = (uint16_t)((uint32_t)in->height * in->ratio_y / 100);
        if (*out_w == 0) *out_w = 1;
        if (*out_h == 0) *out_h = 1;
    }
    return OPRT_OK;
}

/* ------------------------------------------------------------------ */
/*  Nearest-neighbor                                                   */
/* ------------------------------------------------------------------ */

static void __scale_nearest_rgb888(const uint8_t *src, uint16_t sw, uint16_t sh,
                                   uint8_t *dst, uint16_t dw, uint16_t dh)
{
    for (uint32_t dy = 0; dy < dh; dy++) {
        uint32_t sy = (uint32_t)dy * sh / dh;
        for (uint32_t dx = 0; dx < dw; dx++) {
            uint32_t sx = (uint32_t)dx * sw / dw;
            const uint8_t *s = src + (sy * sw + sx) * 3;
            uint8_t       *d = dst + (dy * dw + dx) * 3;
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
        }
    }
}

static void __scale_nearest_rgb565(const uint8_t *src, uint16_t sw, uint16_t sh,
                                   uint8_t *dst, uint16_t dw, uint16_t dh)
{
    const uint16_t *s16 = (const uint16_t *)src;
    uint16_t       *d16 = (uint16_t *)dst;

    for (uint32_t dy = 0; dy < dh; dy++) {
        uint32_t sy = (uint32_t)dy * sh / dh;
        for (uint32_t dx = 0; dx < dw; dx++) {
            uint32_t sx = (uint32_t)dx * sw / dw;
            d16[dy * dw + dx] = s16[sy * sw + sx];
        }
    }
}

static void __scale_nearest_yuv422(const uint8_t *src, uint16_t sw, uint16_t sh,
                                   uint8_t *dst, uint16_t dw, uint16_t dh)
{
    /* Packed YUV422: Y0 U0 Y1 V0 per 4-byte macro-pixel (YUYV) */
    for (uint32_t dy = 0; dy < dh; dy++) {
        uint32_t sy = (uint32_t)dy * sh / dh;
        for (uint32_t dx = 0; dx < dw; dx++) {
            uint32_t sx = (uint32_t)dx * sw / dw;
            /* source macro-pixel index (2 pixels per 4 bytes) */
            uint32_t s_macro = sy * (sw / 2) + sx / 2;
            uint8_t  y  = (sx & 1) ? src[s_macro * 4 + 2] : src[s_macro * 4];
            uint8_t  u  = src[s_macro * 4 + 1];
            uint8_t  v  = src[s_macro * 4 + 3];
            uint32_t d_macro = dy * (dw / 2) + dx / 2;
            if (dx & 1) {
                dst[d_macro * 4 + 2] = y;
            } else {
                dst[d_macro * 4 + 0] = y;
                dst[d_macro * 4 + 1] = u;
                dst[d_macro * 4 + 3] = v;
            }
        }
    }
}

static void __scale_nearest_gray(const uint8_t *src, uint16_t sw, uint16_t sh,
                                 uint8_t *dst, uint16_t dw, uint16_t dh)
{
    for (uint32_t dy = 0; dy < dh; dy++) {
        uint32_t sy = (uint32_t)dy * sh / dh;
        for (uint32_t dx = 0; dx < dw; dx++) {
            uint32_t sx = (uint32_t)dx * sw / dw;
            dst[dy * dw + dx] = src[sy * sw + sx];
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Bilinear interpolation                                             */
/* ------------------------------------------------------------------ */

static void __scale_bilinear_rgb888(const uint8_t *src, uint16_t sw, uint16_t sh,
                                    uint8_t *dst, uint16_t dw, uint16_t dh)
{
    for (uint32_t dy = 0; dy < dh; dy++) {
        uint32_t sy_fp = (dh > 1) ? (uint32_t)((uint64_t)dy * (sh - 1) * SCALE_FP_ONE / (dh - 1)) : 0;
        uint32_t sy0   = sy_fp >> SCALE_FP_SHIFT;
        uint32_t sy1   = (sy0 + 1 < sh) ? sy0 + 1 : sy0;
        uint32_t fy    = sy_fp & (SCALE_FP_ONE - 1);

        for (uint32_t dx = 0; dx < dw; dx++) {
            uint32_t sx_fp = (dw > 1) ? (uint32_t)((uint64_t)dx * (sw - 1) * SCALE_FP_ONE / (dw - 1)) : 0;
            uint32_t sx0   = sx_fp >> SCALE_FP_SHIFT;
            uint32_t sx1   = (sx0 + 1 < sw) ? sx0 + 1 : sx0;
            uint32_t fx    = sx_fp & (SCALE_FP_ONE - 1);

            const uint8_t *p00 = src + (sy0 * sw + sx0) * 3;
            const uint8_t *p01 = src + (sy0 * sw + sx1) * 3;
            const uint8_t *p10 = src + (sy1 * sw + sx0) * 3;
            const uint8_t *p11 = src + (sy1 * sw + sx1) * 3;
            uint8_t       *d   = dst + (dy * dw + dx) * 3;

            for (int c = 0; c < 3; c++) {
                uint32_t top    = ((uint32_t)p00[c] * (SCALE_FP_ONE - fx) + (uint32_t)p01[c] * fx) >> SCALE_FP_SHIFT;
                uint32_t bottom = ((uint32_t)p10[c] * (SCALE_FP_ONE - fx) + (uint32_t)p11[c] * fx) >> SCALE_FP_SHIFT;
                d[c] = (uint8_t)((top * (SCALE_FP_ONE - fy) + bottom * fy) >> SCALE_FP_SHIFT);
            }
        }
    }
}

static void __scale_bilinear_rgb565(const uint8_t *src, uint16_t sw, uint16_t sh,
                                    uint8_t *dst, uint16_t dw, uint16_t dh)
{
    const uint16_t *s16 = (const uint16_t *)src;
    uint16_t       *d16 = (uint16_t *)dst;

    for (uint32_t dy = 0; dy < dh; dy++) {
        uint32_t sy_fp = (dh > 1) ? (uint32_t)((uint64_t)dy * (sh - 1) * SCALE_FP_ONE / (dh - 1)) : 0;
        uint32_t sy0   = sy_fp >> SCALE_FP_SHIFT;
        uint32_t sy1   = (sy0 + 1 < sh) ? sy0 + 1 : sy0;
        uint32_t fy    = sy_fp & (SCALE_FP_ONE - 1);

        for (uint32_t dx = 0; dx < dw; dx++) {
            uint32_t sx_fp = (dw > 1) ? (uint32_t)((uint64_t)dx * (sw - 1) * SCALE_FP_ONE / (dw - 1)) : 0;
            uint32_t sx0   = sx_fp >> SCALE_FP_SHIFT;
            uint32_t sx1   = (sx0 + 1 < sw) ? sx0 + 1 : sx0;
            uint32_t fx    = sx_fp & (SCALE_FP_ONE - 1);

            uint16_t p00 = s16[sy0 * sw + sx0];
            uint16_t p01 = s16[sy0 * sw + sx1];
            uint16_t p10 = s16[sy1 * sw + sx0];
            uint16_t p11 = s16[sy1 * sw + sx1];

            /* Interpolate each channel (R:5, G:6, B:5) independently */
            uint32_t r = (((p00 >> 11) & 0x1F) * (SCALE_FP_ONE - fx) + ((p01 >> 11) & 0x1F) * fx) >> SCALE_FP_SHIFT;
            uint32_t g = (((p00 >>  5) & 0x3F) * (SCALE_FP_ONE - fx) + ((p01 >>  5) & 0x3F) * fx) >> SCALE_FP_SHIFT;
            uint32_t b = ((( p00      ) & 0x1F) * (SCALE_FP_ONE - fx) + (( p01      ) & 0x1F) * fx) >> SCALE_FP_SHIFT;

            uint32_t r2 = (((p10 >> 11) & 0x1F) * (SCALE_FP_ONE - fx) + ((p11 >> 11) & 0x1F) * fx) >> SCALE_FP_SHIFT;
            uint32_t g2 = (((p10 >>  5) & 0x3F) * (SCALE_FP_ONE - fx) + ((p11 >>  5) & 0x3F) * fx) >> SCALE_FP_SHIFT;
            uint32_t b2 = ((( p10      ) & 0x1F) * (SCALE_FP_ONE - fx) + (( p11      ) & 0x1F) * fx) >> SCALE_FP_SHIFT;

            uint32_t ro = (r * (SCALE_FP_ONE - fy) + r2 * fy) >> SCALE_FP_SHIFT;
            uint32_t go = (g * (SCALE_FP_ONE - fy) + g2 * fy) >> SCALE_FP_SHIFT;
            uint32_t bo = (b * (SCALE_FP_ONE - fy) + b2 * fy) >> SCALE_FP_SHIFT;

            d16[dy * dw + dx] = (uint16_t)((ro << 11) | (go << 5) | bo);
        }
    }
}

static void __scale_bilinear_gray(const uint8_t *src, uint16_t sw, uint16_t sh,
                                  uint8_t *dst, uint16_t dw, uint16_t dh)
{
    for (uint32_t dy = 0; dy < dh; dy++) {
        uint32_t sy_fp = (dh > 1) ? (uint32_t)((uint64_t)dy * (sh - 1) * SCALE_FP_ONE / (dh - 1)) : 0;
        uint32_t sy0   = sy_fp >> SCALE_FP_SHIFT;
        uint32_t sy1   = (sy0 + 1 < sh) ? sy0 + 1 : sy0;
        uint32_t fy    = sy_fp & (SCALE_FP_ONE - 1);

        for (uint32_t dx = 0; dx < dw; dx++) {
            uint32_t sx_fp = (dw > 1) ? (uint32_t)((uint64_t)dx * (sw - 1) * SCALE_FP_ONE / (dw - 1)) : 0;
            uint32_t sx0   = sx_fp >> SCALE_FP_SHIFT;
            uint32_t sx1   = (sx0 + 1 < sw) ? sx0 + 1 : sx0;
            uint32_t fx    = sx_fp & (SCALE_FP_ONE - 1);

            uint32_t top    = ((uint32_t)src[sy0 * sw + sx0] * (SCALE_FP_ONE - fx) + (uint32_t)src[sy0 * sw + sx1] * fx) >> SCALE_FP_SHIFT;
            uint32_t bottom = ((uint32_t)src[sy1 * sw + sx0] * (SCALE_FP_ONE - fx) + (uint32_t)src[sy1 * sw + sx1] * fx) >> SCALE_FP_SHIFT;
            dst[dy * dw + dx] = (uint8_t)((top * (SCALE_FP_ONE - fy) + bottom * fy) >> SCALE_FP_SHIFT);
        }
    }
}

/* YUV422 bilinear: interpolate Y, U, V channels separately */
static void __scale_bilinear_yuv422(const uint8_t *src, uint16_t sw, uint16_t sh,
                                    uint8_t *dst, uint16_t dw, uint16_t dh)
{
    /* Expand to planar Y/U/V, scale each, repack to YUYV */
    uint32_t src_pixels = (uint32_t)sw * sh;
    uint32_t dst_pixels = (uint32_t)dw * dh;

    uint8_t *sy_plane = (uint8_t *)Malloc(src_pixels);
    uint8_t *su_plane = (uint8_t *)Malloc(src_pixels);
    uint8_t *sv_plane = (uint8_t *)Malloc(src_pixels);
    uint8_t *dy_plane = (uint8_t *)Malloc(dst_pixels);
    uint8_t *du_plane = (uint8_t *)Malloc(dst_pixels);
    uint8_t *dv_plane = (uint8_t *)Malloc(dst_pixels);

    if (!sy_plane || !su_plane || !sv_plane || !dy_plane || !du_plane || !dv_plane) {
        PR_ERR("malloc failed for YUV422 bilinear planes");
        goto cleanup;
    }

    /* Unpack YUYV → planar */
    for (uint32_t i = 0; i < src_pixels / 2; i++) {
        uint8_t y0 = src[i * 4 + 0];
        uint8_t u  = src[i * 4 + 1];
        uint8_t y1 = src[i * 4 + 2];
        uint8_t v  = src[i * 4 + 3];
        sy_plane[i * 2]     = y0;
        sy_plane[i * 2 + 1] = y1;
        su_plane[i * 2]     = u;
        su_plane[i * 2 + 1] = u;
        sv_plane[i * 2]     = v;
        sv_plane[i * 2 + 1] = v;
    }

    __scale_bilinear_gray(sy_plane, sw, sh, dy_plane, dw, dh);
    __scale_bilinear_gray(su_plane, sw, sh, du_plane, dw, dh);
    __scale_bilinear_gray(sv_plane, sw, sh, dv_plane, dw, dh);

    /* Repack planar → YUYV */
    for (uint32_t i = 0; i < dst_pixels / 2; i++) {
        dst[i * 4 + 0] = dy_plane[i * 2];
        dst[i * 4 + 1] = du_plane[i * 2];
        dst[i * 4 + 2] = dy_plane[i * 2 + 1];
        dst[i * 4 + 3] = dv_plane[i * 2];
    }

cleanup:
    if (sy_plane) Free(sy_plane);
    if (su_plane) Free(su_plane);
    if (sv_plane) Free(sv_plane);
    if (dy_plane) Free(dy_plane);
    if (du_plane) Free(du_plane);
    if (dv_plane) Free(dv_plane);
}

/* ------------------------------------------------------------------ */
/*  Generic scale dispatcher (allocates output buffer)                */
/* ------------------------------------------------------------------ */


static OPERATE_RET __scale(const TAL_IMAGE_SCALE_IN_T *in, TAL_IMAGE_SCALE_OUT_T *out,
                           uint32_t bpp,
                           __scale_fn_t fn_nearest, __scale_fn_t fn_bilinear)
{
    OPERATE_RET rt;
    uint16_t out_w, out_h;

    if (!in || !in->buf || in->width == 0 || in->height == 0 || !out) {
        return OPRT_INVALID_PARM;
    }

    rt = __resolve_out_size(in, &out_w, &out_h);
    if (rt != OPRT_OK) {
        return rt;
    }

    uint32_t buf_size = (uint32_t)out_w * out_h * bpp;
    uint8_t *buf = (uint8_t *)Malloc(buf_size);
    if (!buf) {
        PR_ERR("malloc %u bytes failed", buf_size);
        return OPRT_MALLOC_FAILED;
    }

    switch (in->method) {
    case TAL_IMAGE_SCALE_MTH_NEAREST:
        fn_nearest(in->buf, in->width, in->height, buf, out_w, out_h);
        break;
    case TAL_IMAGE_SCALE_MTH_BILINEAR:
    case TAL_IMAGE_SCALE_MTH_BICUBIC:  /* bicubic falls back to bilinear */
    default:
        fn_bilinear(in->buf, in->width, in->height, buf, out_w, out_h);
        break;
    }

    out->buf    = buf;
    out->size   = buf_size;
    out->width  = out_w;
    out->height = out_h;
    return OPRT_OK;
}

/**
 * @brief Resolve JPEG output dimensions from SIZE or RATIO mode.
 *
 * JPEG dimensions are not known until the header is parsed, so this function
 * takes the decoded width/height as input for RATIO mode calculation.
 */
static OPERATE_RET __resolve_jpeg_out_size(const TAL_IMAGE_JPEG_SCALE_IN_T *in,
                                           uint16_t src_w, uint16_t src_h,
                                           uint16_t *out_w, uint16_t *out_h)
{
    if (in->mode == TAL_IMAGE_SCALE_MODE_SIZE) {
        if (in->out_width == 0 || in->out_height == 0) {
            PR_ERR("out_width/out_height must be non-zero");
            return OPRT_INVALID_PARM;
        }
        *out_w = in->out_width;
        *out_h = in->out_height;
    } else {
        if (in->ratio_x == 0 || in->ratio_y == 0) {
            PR_ERR("ratio_x/ratio_y must be non-zero");
            return OPRT_INVALID_PARM;
        }
        *out_w = (uint16_t)((uint32_t)src_w * in->ratio_x / 100);
        *out_h = (uint16_t)((uint32_t)src_h * in->ratio_y / 100);
        if (*out_w == 0) *out_w = 1;
        if (*out_h == 0) *out_h = 1;
    }
    return OPRT_OK;
}

/**
 * @brief Common JPEG decode-and-scale path.
 *
 * Decodes the JPEG to a full-resolution raw buffer, then scales to the
 * requested output size. The intermediate buffer is freed before returning.
 *
 * @param in       JPEG scale input parameters.
 * @param out      Output buffer (allocated internally).
 * @param bpp      Bytes per pixel of the decoded format.
 * @param decode   Function to decode the JPEG to raw pixels.
 * @param scale    Function to scale the raw pixels.
 */
static OPERATE_RET __jpeg_scale(const TAL_IMAGE_JPEG_SCALE_IN_T *in,
                                TAL_IMAGE_SCALE_OUT_T *out,
                                uint32_t bpp,
                                __jpeg_decode_fn_t decode,
                                __image_scale_fn_t scale)
{
    OPERATE_RET rt;

    if (!in || !in->data || in->size == 0 || !out) {
        return OPRT_INVALID_PARM;
    }

    /* Step 1: parse JPEG header to get source dimensions */
    TAL_IMAGE_JPEG_INFO_T info = {0};
    rt = tal_image_jpeg_get_info(in->data, in->size, &info);
    if (rt != OPRT_OK) {
        PR_ERR("jpeg get info failed: %d", rt);
        return rt;
    }

    /* Step 2: resolve output dimensions */
    uint16_t out_w, out_h;
    rt = __resolve_jpeg_out_size(in, info.width, info.height, &out_w, &out_h);
    if (rt != OPRT_OK) {
        return rt;
    }

    /* Step 3: decode JPEG to full-resolution raw buffer */
    uint32_t decode_size = (uint32_t)info.width * info.height * bpp;
    uint8_t *decode_buf  = (uint8_t *)Malloc(decode_size);
    if (!decode_buf) {
        PR_ERR("malloc %u bytes failed for JPEG decode", decode_size);
        return OPRT_MALLOC_FAILED;
    }

    TAL_IMAGE_JPEG_OUTPUT_T jpeg_out = {
        .out_buf      = decode_buf,
        .out_buf_size = decode_size,
        .out_width    = info.width,
        .out_height   = info.height,
    };

    rt = decode(in->data, in->size, &jpeg_out);
    if (rt != OPRT_OK) {
        PR_ERR("jpeg decode failed: %d", rt);
        Free(decode_buf);
        return rt;
    }

    /* Step 4: scale decoded raw buffer to target size */
    TAL_IMAGE_SCALE_IN_T scale_in = {
        .method     = in->method,
        .mode       = TAL_IMAGE_SCALE_MODE_SIZE,
        .buf        = decode_buf,
        .width      = info.width,
        .height     = info.height,
        .out_width  = out_w,
        .out_height = out_h,
    };

    rt = scale(&scale_in, out);

    Free(decode_buf);
    return rt;
}


/**
 * @brief Frees an output buffer allocated by any scale function.
 *
 * Clears all fields of out after freeing. Passing NULL or a struct with
 * buf == NULL is safe and has no effect.
 *
 * @param[in] out  Pointer to the output structure to free.
 */
void tal_image_scale_buf_free(TAL_IMAGE_SCALE_OUT_T *out)
{
    if (out && out->buf) {
        Free(out->buf);
        out->buf    = NULL;
        out->size   = 0;
        out->width  = 0;
        out->height = 0;
    }
}

/**
 * @brief Scales an RGB888 image to a new resolution.
 *
 * Supports nearest-neighbor and bilinear interpolation (bicubic falls back
 * to bilinear). Output buffer is allocated internally; release with
 * tal_image_scale_buf_free().
 *
 * @param[in]  in   Source image parameters (buffer, dimensions, target size or ratio).
 * @param[out] out  Allocated output buffer, its byte size, and actual dimensions.
 * @return OPERATE_RET  OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tal_image_rgb888_scale(const TAL_IMAGE_SCALE_IN_T *in, TAL_IMAGE_SCALE_OUT_T *out)
{
    return __scale(in, out, 3, __scale_nearest_rgb888, __scale_bilinear_rgb888);
}

/**
 * @brief Scales an RGB565 image to a new resolution.
 *
 * Supports nearest-neighbor and bilinear interpolation (bicubic falls back
 * to bilinear). Output buffer is allocated internally; release with
 * tal_image_scale_buf_free().
 *
 * @param[in]  in   Source image parameters (buffer, dimensions, target size or ratio).
 * @param[out] out  Allocated output buffer, its byte size, and actual dimensions.
 * @return OPERATE_RET  OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tal_image_rgb565_scale(const TAL_IMAGE_SCALE_IN_T *in, TAL_IMAGE_SCALE_OUT_T *out)
{
    return __scale(in, out, 2, __scale_nearest_rgb565, __scale_bilinear_rgb565);
}

/**
 * @brief Scales a packed YUV422 (YUYV) image to a new resolution.
 *
 * Source width must be even. Bilinear interpolation unpacks to planar Y/U/V,
 * interpolates each channel independently, then repacks to YUYV.
 * Output buffer is allocated internally; release with tal_image_scale_buf_free().
 *
 * @param[in]  in   Source image parameters (buffer, dimensions, target size or ratio).
 * @param[out] out  Allocated output buffer, its byte size, and actual dimensions.
 * @return OPERATE_RET  OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tal_image_yuv422_scale(const TAL_IMAGE_SCALE_IN_T *in, TAL_IMAGE_SCALE_OUT_T *out)
{
    /* YUV422 requires even width */
    if (in && in->width % 2 != 0) {
        PR_ERR("YUV422 requires even width");
        return OPRT_INVALID_PARM;
    }
    return __scale(in, out, 2, __scale_nearest_yuv422, __scale_bilinear_yuv422);
}

/**
 * @brief Scales an 8-bit grayscale image to a new resolution.
 *
 * Supports nearest-neighbor and bilinear interpolation (bicubic falls back
 * to bilinear). Output buffer is allocated internally; release with
 * tal_image_scale_buf_free().
 *
 * @param[in]  in   Source image parameters (buffer, dimensions, target size or ratio).
 * @param[out] out  Allocated output buffer, its byte size, and actual dimensions.
 * @return OPERATE_RET  OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tal_image_gray_scale(const TAL_IMAGE_SCALE_IN_T *in, TAL_IMAGE_SCALE_OUT_T *out)
{
    return __scale(in, out, 1, __scale_nearest_gray, __scale_bilinear_gray);
}

/**
 * @brief Decodes a JPEG stream and scales it to RGB565 output.
 *
 * Decodes the full JPEG at its native resolution, then scales to the requested
 * size. A temporary full-resolution buffer is allocated and freed internally.
 * Output buffer is allocated internally; release with tal_image_scale_buf_free().
 *
 * @param[in]  in   JPEG stream, its size, and target dimensions or ratio.
 * @param[out] out  Allocated RGB565 output buffer, its byte size, and actual dimensions.
 * @return OPERATE_RET  OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tal_image_jpeg_scale_rgb565(const TAL_IMAGE_JPEG_SCALE_IN_T *in,
                                        TAL_IMAGE_SCALE_OUT_T *out)
{
    return __jpeg_scale(in, out, 2,
                        tal_image_jpeg_decode_rgb565,
                        tal_image_rgb565_scale);
}

/**
 * @brief Decodes a JPEG stream and scales it to RGB888 output.
 *
 * Decodes the full JPEG at its native resolution, then scales to the requested
 * size. A temporary full-resolution buffer is allocated and freed internally.
 * Output buffer is allocated internally; release with tal_image_scale_buf_free().
 *
 * @param[in]  in   JPEG stream, its size, and target dimensions or ratio.
 * @param[out] out  Allocated RGB888 output buffer, its byte size, and actual dimensions.
 * @return OPERATE_RET  OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tal_image_jpeg_scale_rgb888(const TAL_IMAGE_JPEG_SCALE_IN_T *in,
                                        TAL_IMAGE_SCALE_OUT_T *out)
{
    return __jpeg_scale(in, out, 3,
                        tal_image_jpeg_decode_rgb888,
                        tal_image_rgb888_scale);
}

/**
 * @brief Decodes a JPEG stream and scales it to 8-bit grayscale output.
 *
 * Decodes the full JPEG at its native resolution, then scales to the requested
 * size. A temporary full-resolution buffer is allocated and freed internally.
 * Output buffer is allocated internally; release with tal_image_scale_buf_free().
 *
 * @param[in]  in   JPEG stream, its size, and target dimensions or ratio.
 * @param[out] out  Allocated grayscale output buffer, its byte size, and actual dimensions.
 * @return OPERATE_RET  OPRT_OK on success, error code otherwise.
 */
OPERATE_RET tal_image_jpeg_scale_gray(const TAL_IMAGE_JPEG_SCALE_IN_T *in,
                                      TAL_IMAGE_SCALE_OUT_T *out)
{
    return __jpeg_scale(in, out, 1,
                        tal_image_jpeg_decode_gray,
                        tal_image_gray_scale);
}