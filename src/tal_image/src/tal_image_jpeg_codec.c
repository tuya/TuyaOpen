/**
 * @file tal_image_jpeg_codec.c
 * @brief JPEG decode implementation: libjpeg-turbo or tjpgd backend.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_image_jpeg_codec.h"
#include "tuya_error_code.h"
#include "tal_memory.h"
#include "tal_mutex.h"
#include <string.h>

#if defined(ENABLE_LIBJPEGTURBO)
#include "turbojpeg.h"
#else
#include "tjpgd/tjpgd.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#if !defined(ENABLE_LIBJPEGTURBO)
#define TJPGD_WORKBUF_SIZE  (10*1024)
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
#if !defined(ENABLE_LIBJPEGTURBO)
/** Memory stream for tjpgd input */
typedef struct {
    const uint8_t *data;
    uint32_t       size;
    uint32_t       pos;
} jpeg_mem_stream_t;
#endif

/***********************************************************
*********************static declarations*******************
***********************************************************/
#if !defined(ENABLE_LIBJPEGTURBO)
static size_t tjpgd_input_func(JDEC *jd, uint8_t *buf, size_t ndata);
static int    tjpgd_outfunc_rgb888(JDEC *jd, void *bitmap, JRECT *rect);
static int    tjpgd_outfunc_rgb565(JDEC *jd, void *bitmap, JRECT *rect);
static int    tjpgd_outfunc_gray(JDEC *jd, void *bitmap, JRECT *rect);
#endif

/** Output context for tjpgd outfunc */
typedef struct {
    uint8_t  *out_buf;
    uint16_t  out_width;
    uint16_t  out_height;
    uint8_t   format; /* 0: rgb888, 1: rgb565, 2: gray */
} tjpgd_out_ctx_t;

#if !defined(ENABLE_LIBJPEGTURBO)
static tjpgd_out_ctx_t s_out_ctx;
/* Mutex protecting s_out_ctx and jd_decomp. Required on SMP platforms (e.g.
 * T5AI dual-core) where concurrent callers (display decode vs. printer decode)
 * would otherwise race on the shared global output context. */
static MUTEX_HANDLE sg_jpeg_decode_mutex = NULL;

static void __jpeg_decode_mutex_ensure(void)
{
    if (sg_jpeg_decode_mutex == NULL) {
        tal_mutex_create_init(&sg_jpeg_decode_mutex);
    }
}
#endif

/* Nearest-neighbor scale of an 8-bit grayscale image */
static void __gray_scale_nn(const uint8_t *src, uint16_t sw, uint16_t sh,
                             uint8_t *dst, uint16_t dw, uint16_t dh)
{
    uint16_t dy, dx;
    for (dy = 0; dy < dh; dy++) {
        uint16_t sy = (uint16_t)((uint32_t)dy * sh / dh);
        for (dx = 0; dx < dw; dx++) {
            uint16_t sx = (uint16_t)((uint32_t)dx * sw / dw);
            dst[(uint32_t)dy * dw + dx] = src[(uint32_t)sy * sw + sx];
        }
    }
}

/***********************************************************
*********************static functions***********************
***********************************************************/

#if !defined(ENABLE_LIBJPEGTURBO)
static size_t tjpgd_input_func(JDEC *jd, uint8_t *buf, size_t ndata)
{
    jpeg_mem_stream_t *stream;
    uint32_t left;
    size_t   n;

    if (jd == NULL || jd->device == NULL) {
        return 0;
    }
    stream = (jpeg_mem_stream_t *)jd->device;

    if (stream->pos >= stream->size) {
        return 0;
    }
    left = stream->size - stream->pos;
    n    = (ndata <= (size_t)left) ? ndata : (size_t)left;

    if (buf != NULL && n > 0) {
        memcpy(buf, stream->data + stream->pos, n);
    }
    stream->pos += (uint32_t)n;
    return n;
}

static int tjpgd_outfunc_rgb888(JDEC *jd, void *bitmap, JRECT *rect)
{
    const uint8_t *src;
    uint8_t       *dst;
    uint16_t       y, w, stride;

    (void)jd;
    if (bitmap == NULL || rect == NULL || s_out_ctx.out_buf == NULL) {
        return 0;
    }
    w      = (uint16_t)(rect->right - rect->left + 1);
    stride = s_out_ctx.out_width * 3u;
    src    = (const uint8_t *)bitmap;

    for (y = rect->top; y <= rect->bottom; y++) {
        dst = s_out_ctx.out_buf + (uint32_t)y * stride + (uint32_t)rect->left * 3u;
        memcpy(dst, src, (size_t)w * 3u);
        src += (size_t)w * 3u;
    }
    return 1;
}

static int tjpgd_outfunc_rgb565(JDEC *jd, void *bitmap, JRECT *rect)
{
    const uint8_t *src;
    uint16_t      *dst;
    uint16_t       y, x, w, stride;
    uint16_t       r, g, b, rgb565;

    (void)jd;
    if (bitmap == NULL || rect == NULL || s_out_ctx.out_buf == NULL) {
        return 0;
    }
    w      = (uint16_t)(rect->right - rect->left + 1);
    stride = s_out_ctx.out_width;
    src    = (const uint8_t *)bitmap;

    /* tjpgd outputs pixels in B-G-R order */
    for (y = rect->top; y <= rect->bottom; y++) {
        dst = (uint16_t *)(s_out_ctx.out_buf + (uint32_t)y * stride * 2u) + rect->left;
        for (x = 0; x < w; x++) {
            b      = (uint16_t)src[0];
            g      = (uint16_t)src[1];
            r      = (uint16_t)src[2];
            src   += 3;
            rgb565 = (uint16_t)((r & 0xF8u) << 8 | (g & 0xFCu) << 3 | (b >> 3));
            *dst++ = rgb565;
        }
    }
    return 1;
}

static int tjpgd_outfunc_gray(JDEC *jd, void *bitmap, JRECT *rect)
{
    const uint8_t *src;
    uint8_t       *dst;
    uint16_t       y, x, w, stride;
    uint16_t       gray;

    (void)jd;
    if (bitmap == NULL || rect == NULL || s_out_ctx.out_buf == NULL) {
        return 0;
    }
    w      = (uint16_t)(rect->right - rect->left + 1);
    stride = s_out_ctx.out_width;
    src    = (const uint8_t *)bitmap;

    /* tjpgd outputs pixels in B-G-R order */
    for (y = rect->top; y <= rect->bottom; y++) {
        dst = s_out_ctx.out_buf + (uint32_t)y * stride + rect->left;
        for (x = 0; x < w; x++) {
            /* Y = 0.299*R + 0.587*G + 0.114*B; src order is B, G, R */
            gray = (uint16_t)((117u * (uint16_t)src[0] + 601u * (uint16_t)src[1] + 306u * (uint16_t)src[2]) >> 10);
            if (gray > 255u) {
                gray = 255u;
            }
            *dst++ = (uint8_t)gray;
            src   += 3;
        }
    }
    return 1;
}
#endif

/***********************************************************
*********************public functions***********************
***********************************************************/

OPERATE_RET tal_image_jpeg_get_info(const uint8_t *jpeg_data,
                                    uint32_t       jpeg_size,
                                    TAL_IMAGE_JPEG_INFO_T *info)
{
    if (jpeg_data == NULL || info == NULL) {
        return OPRT_INVALID_PARM;
    }

    // if (jpeg_size < TAL_IMAGE_JPEG_MIN_READ_SIZE) {
    //     // Just a warning, don't return error. We might have a very small JPEG or just enough header.
    //     // return OPRT_INVALID_PARM;
    // }
    
    if (jpeg_data[0] != 0xFFu || jpeg_data[1] != 0xD8u) {
        return OPRT_INVALID_PARM;
    }

#if defined(ENABLE_LIBJPEGTURBO)
    {
        tjhandle    handle;
        int         w, h, subsamp;
        int         ret;

        handle = tjInitDecompress();
        if (handle == NULL) {
            return OPRT_COM_ERROR;
        }
        ret = tjDecompressHeader2(handle, (unsigned char *)jpeg_data, (unsigned long)jpeg_size,
                                 &w, &h, &subsamp);
        tjDestroy(handle);
        if (ret != 0) {
            return OPRT_COM_ERROR;
        }
        if (w <= 0 || h <= 0 || w > 65535 || h > 65535) {
            return OPRT_INVALID_PARM;
        }
        info->width        = (uint16_t)w;
        info->height       = (uint16_t)h;
        info->n_components = (subsamp == TJSAMP_GRAY) ? (uint8_t)1 : (uint8_t)3;
    }
#else
    {
        JDEC            jd;
        uint8_t        *workbuf;
        jpeg_mem_stream_t stream;
        JRESULT         rc;

        workbuf = (uint8_t *)Malloc(TJPGD_WORKBUF_SIZE);
        if (workbuf == NULL) {
            return OPRT_MALLOC_FAILED;
        }

        memset(&stream, 0, sizeof(stream));
        stream.data = jpeg_data;
        stream.size = jpeg_size;
        stream.pos  = 0;

        rc = jd_prepare(&jd, tjpgd_input_func, workbuf, (size_t)TJPGD_WORKBUF_SIZE, &stream);
        if (rc != JDR_OK) {
            Free(workbuf);
            return OPRT_COM_ERROR;
        }
        info->width        = jd.width;
        info->height       = jd.height;
        info->n_components = jd.ncomp;
        Free(workbuf);
    }
#endif
    return OPRT_OK;
}

OPERATE_RET tal_image_jpeg_decode_rgb565(const uint8_t *jpeg_data,
                                         uint32_t       jpeg_size,
                                         TAL_IMAGE_JPEG_OUTPUT_T *out)
{
    if (jpeg_data == NULL || out == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (out->out_buf == NULL || out->out_buf_size == 0) {
        return OPRT_INVALID_PARM;
    }

#if defined(ENABLE_LIBJPEGTURBO)
    {
        tjhandle  handle;
        int       w, h, subsamp;
        int       ret;
        uint32_t  need_rgb;
        uint8_t  *rgb_buf = NULL;
        uint32_t  i, pixels;
        const uint8_t *s;
        uint16_t *d;

        handle = tjInitDecompress();
        if (handle == NULL) {
            return OPRT_COM_ERROR;
        }
        ret = tjDecompressHeader2(handle, (unsigned char *)jpeg_data, (unsigned long)jpeg_size,
                                 &w, &h, &subsamp);
        if (ret != 0) {
            tjDestroy(handle);
            return OPRT_COM_ERROR;
        }
        if (w <= 0 || h <= 0 || (unsigned int)w != out->out_width || (unsigned int)h != out->out_height) {
            tjDestroy(handle);
            return OPRT_INVALID_PARM;
        }
        need_rgb = (uint32_t)w * (uint32_t)h * 3u;
        if (subsamp == TJSAMP_GRAY) {
            ret = tjDecompress2(handle, jpeg_data, (unsigned long)jpeg_size,
                               out->out_buf, w, 0, h, TJPF_GRAY, 0);
            tjDestroy(handle);
            if (ret != 0) {
                return OPRT_COM_ERROR;
            }
            /* In-place convert gray (1 byte/pixel) to RGB565 (2 bytes/pixel) from end to avoid overwrite */
            pixels = (uint32_t)w * (uint32_t)h;
            s      = out->out_buf + pixels - 1;
            d      = (uint16_t *)out->out_buf + (pixels - 1);
            for (i = pixels; i > 0; i--) {
                uint16_t g = (uint16_t)*s--;
                *d-- = (uint16_t)((g & 0xF8u) << 8 | (g & 0xFCu) << 3 | (g >> 3));
            }
        } else {
            rgb_buf = (uint8_t *)Malloc(need_rgb);
            if (rgb_buf == NULL) {
                tjDestroy(handle);
                return OPRT_MALLOC_FAILED;
            }
            ret = tjDecompress2(handle, jpeg_data, (unsigned long)jpeg_size,
                               rgb_buf, w, 0, h, TJPF_RGB, 0);
            tjDestroy(handle);
            if (ret != 0) {
                Free(rgb_buf);
                return OPRT_COM_ERROR;
            }
            pixels = (uint32_t)w * (uint32_t)h;
            s      = rgb_buf + (pixels - 1) * 3u;
            d      = (uint16_t *)out->out_buf + (pixels - 1);
            for (i = pixels; i > 0; i--) {
                uint16_t r = (uint16_t)s[0], g = (uint16_t)s[1], b = (uint16_t)s[2];
                *d-- = (uint16_t)((r & 0xF8u) << 8 | (g & 0xFCu) << 3 | (b >> 3));
                s -= 3;
            }
            Free(rgb_buf);
        }
    }
#else
    {
        JDEC              jd;
        uint8_t          *workbuf;
        jpeg_mem_stream_t stream;
        JRESULT           rc;

        workbuf = (uint8_t *)Malloc(TJPGD_WORKBUF_SIZE);
        if (workbuf == NULL) {
            return OPRT_MALLOC_FAILED;
        }
        memset(&stream, 0, sizeof(stream));
        stream.data = jpeg_data;
        stream.size = jpeg_size;
        stream.pos  = 0;

        rc = jd_prepare(&jd, tjpgd_input_func, workbuf, (size_t)TJPGD_WORKBUF_SIZE, &stream);
        if (rc != JDR_OK) {
            Free(workbuf);
            return OPRT_COM_ERROR;
        }
        if (jd.width != out->out_width || jd.height != out->out_height) {
            Free(workbuf);
            return OPRT_INVALID_PARM;
        }
        __jpeg_decode_mutex_ensure();
        tal_mutex_lock(sg_jpeg_decode_mutex);
        s_out_ctx.out_buf    = out->out_buf;
        s_out_ctx.out_width  = out->out_width;
        s_out_ctx.out_height = out->out_height;
        rc                   = jd_decomp(&jd, tjpgd_outfunc_rgb565, 0);
        tal_mutex_unlock(sg_jpeg_decode_mutex);
        Free(workbuf);
        if (rc != JDR_OK) {
            return OPRT_COM_ERROR;
        }
    }
#endif
    return OPRT_OK;
}

OPERATE_RET tal_image_jpeg_decode_rgb888(const uint8_t *jpeg_data,
                                         uint32_t       jpeg_size,
                                         TAL_IMAGE_JPEG_OUTPUT_T *out)
{
    if (jpeg_data == NULL || out == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (out->out_buf == NULL || out->out_buf_size == 0) {
        return OPRT_INVALID_PARM;
    }

#if defined(ENABLE_LIBJPEGTURBO)
    {
        tjhandle handle;
        int      w, h, subsamp;
        int      ret;

        handle = tjInitDecompress();
        if (handle == NULL) {
            return OPRT_COM_ERROR;
        }
        ret = tjDecompressHeader2(handle, (unsigned char *)jpeg_data, (unsigned long)jpeg_size,
                                 &w, &h, &subsamp);
        if (ret != 0) {
            tjDestroy(handle);
            return OPRT_COM_ERROR;
        }
        if (w <= 0 || h <= 0 || (unsigned int)w != out->out_width || (unsigned int)h != out->out_height) {
            tjDestroy(handle);
            return OPRT_INVALID_PARM;
        }
        ret = tjDecompress2(handle, jpeg_data, (unsigned long)jpeg_size,
                           out->out_buf, w, 0, h, TJPF_RGB, 0);
        tjDestroy(handle);
        if (ret != 0) {
            return OPRT_COM_ERROR;
        }
    }
#else
    {
        JDEC              jd;
        uint8_t          *workbuf;
        jpeg_mem_stream_t stream;
        JRESULT           rc;

        workbuf = (uint8_t *)Malloc(TJPGD_WORKBUF_SIZE);
        if (workbuf == NULL) {
            return OPRT_MALLOC_FAILED;
        }
        memset(&stream, 0, sizeof(stream));
        stream.data = jpeg_data;
        stream.size = jpeg_size;
        stream.pos  = 0;

        rc = jd_prepare(&jd, tjpgd_input_func, workbuf, (size_t)TJPGD_WORKBUF_SIZE, &stream);
        if (rc != JDR_OK) {
            Free(workbuf);
            return OPRT_COM_ERROR;
        }
        if (jd.width != out->out_width || jd.height != out->out_height) {
            Free(workbuf);
            return OPRT_INVALID_PARM;
        }
        __jpeg_decode_mutex_ensure();
        tal_mutex_lock(sg_jpeg_decode_mutex);
        s_out_ctx.out_buf    = out->out_buf;
        s_out_ctx.out_width  = out->out_width;
        s_out_ctx.out_height = out->out_height;
        rc                   = jd_decomp(&jd, tjpgd_outfunc_rgb888, 0);
        tal_mutex_unlock(sg_jpeg_decode_mutex);
        Free(workbuf);
        if (rc != JDR_OK) {
            return OPRT_COM_ERROR;
        }
    }
#endif
    return OPRT_OK;
}

OPERATE_RET tal_image_jpeg_decode_gray(const uint8_t *jpeg_data,
                                       uint32_t       jpeg_size,
                                       TAL_IMAGE_JPEG_OUTPUT_T *out)
{
    if (jpeg_data == NULL || out == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (out->out_buf == NULL || out->out_buf_size == 0) {
        return OPRT_INVALID_PARM;
    }

#if defined(ENABLE_LIBJPEGTURBO)
    {
        tjhandle handle;
        int      w, h, subsamp;
        int      ret;

        handle = tjInitDecompress();
        if (handle == NULL) {
            return OPRT_COM_ERROR;
        }
        ret = tjDecompressHeader2(handle, (unsigned char *)jpeg_data, (unsigned long)jpeg_size,
                                 &w, &h, &subsamp);
        if (ret != 0) {
            tjDestroy(handle);
            return OPRT_COM_ERROR;
        }
        if (w <= 0 || h <= 0 ||
            out->out_width  == 0 || out->out_width  > (unsigned int)w ||
            out->out_height == 0 || out->out_height > (unsigned int)h) {
            tjDestroy(handle);
            return OPRT_INVALID_PARM;
        }
        if ((unsigned int)w == out->out_width && (unsigned int)h == out->out_height) {
            ret = tjDecompress2(handle, jpeg_data, (unsigned long)jpeg_size,
                               out->out_buf, w, 0, h, TJPF_GRAY, 0);
            tjDestroy(handle);
            if (ret != 0) {
                return OPRT_COM_ERROR;
            }
        } else {
            /* Decode to full gray, then scale down */
            uint32_t full_size = (uint32_t)w * (uint32_t)h;
            uint8_t *tmp = (uint8_t *)Malloc(full_size);
            if (tmp == NULL) {
                tjDestroy(handle);
                return OPRT_MALLOC_FAILED;
            }
            ret = tjDecompress2(handle, jpeg_data, (unsigned long)jpeg_size,
                               tmp, w, 0, h, TJPF_GRAY, 0);
            tjDestroy(handle);
            if (ret != 0) {
                Free(tmp);
                return OPRT_COM_ERROR;
            }
            __gray_scale_nn(tmp, (uint16_t)w, (uint16_t)h,
                            out->out_buf, out->out_width, out->out_height);
            Free(tmp);
        }
    }
#else
    {
        JDEC              jd;
        uint8_t          *workbuf;
        jpeg_mem_stream_t stream;
        JRESULT           rc;

        workbuf = (uint8_t *)Malloc(TJPGD_WORKBUF_SIZE);
        if (workbuf == NULL) {
            return OPRT_MALLOC_FAILED;
        }
        memset(&stream, 0, sizeof(stream));
        stream.data = jpeg_data;
        stream.size = jpeg_size;
        stream.pos  = 0;

        rc = jd_prepare(&jd, tjpgd_input_func, workbuf, (size_t)TJPGD_WORKBUF_SIZE, &stream);
        if (rc != JDR_OK) {
            Free(workbuf);
            return OPRT_COM_ERROR;
        }
        if (out->out_width  == 0 || out->out_width  > jd.width ||
            out->out_height == 0 || out->out_height > jd.height) {
            Free(workbuf);
            return OPRT_INVALID_PARM;
        }

        /* Find the largest power-of-2 downscale where decoded size >= target */
        uint8_t  scale = 0;
        uint16_t dec_w = jd.width;
        uint16_t dec_h = jd.height;
        while (scale < 3 &&
               (dec_w >> 1u) >= out->out_width &&
               (dec_h >> 1u) >= out->out_height) {
            scale++;
            dec_w = (uint16_t)(dec_w >> 1u);
            dec_h = (uint16_t)(dec_h >> 1u);
        }

        __jpeg_decode_mutex_ensure();
        if (dec_w == out->out_width && dec_h == out->out_height) {
            /* Exact match at this scale — decode directly into output */
            tal_mutex_lock(sg_jpeg_decode_mutex);
            s_out_ctx.out_buf    = out->out_buf;
            s_out_ctx.out_width  = out->out_width;
            s_out_ctx.out_height = out->out_height;
            rc = jd_decomp(&jd, tjpgd_outfunc_gray, scale);
            tal_mutex_unlock(sg_jpeg_decode_mutex);
            Free(workbuf);
            if (rc != JDR_OK) {
                return OPRT_COM_ERROR;
            }
        } else {
            /* Decode to power-of-2 size, then nearest-neighbor scale to target */
            uint32_t tmp_size = (uint32_t)dec_w * dec_h;
            uint8_t *tmp = (uint8_t *)Malloc(tmp_size);
            if (tmp == NULL) {
                Free(workbuf);
                return OPRT_MALLOC_FAILED;
            }
            tal_mutex_lock(sg_jpeg_decode_mutex);
            s_out_ctx.out_buf    = tmp;
            s_out_ctx.out_width  = dec_w;
            s_out_ctx.out_height = dec_h;
            rc = jd_decomp(&jd, tjpgd_outfunc_gray, scale);
            tal_mutex_unlock(sg_jpeg_decode_mutex);
            Free(workbuf);
            if (rc != JDR_OK) {
                Free(tmp);
                return OPRT_COM_ERROR;
            }
            __gray_scale_nn(tmp, dec_w, dec_h,
                            out->out_buf, out->out_width, out->out_height);
            Free(tmp);
        }
    }
#endif
    return OPRT_OK;
}

OPERATE_RET tal_image_jpeg_decode_bitmap(const uint8_t *jpeg_data,
                                          uint32_t       jpeg_size,
                                          TAL_IMAGE_JPEG_OUTPUT_T *out,
                                          uint8_t        threshold)
{
    if (jpeg_data == NULL || out == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (out->out_buf == NULL || out->out_buf_size == 0) {
        return OPRT_INVALID_PARM;
    }

    uint16_t w = out->out_width;
    uint16_t h = out->out_height;
    uint32_t bytes_per_line = ((uint32_t)w + 7u) / 8u;
    uint32_t bmp_need = bytes_per_line * h;

    if (out->out_buf_size < bmp_need) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    uint32_t gray_size = (uint32_t)w * h;
    uint8_t *gray_buf = (uint8_t *)Malloc(gray_size);
    if (gray_buf == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    TAL_IMAGE_JPEG_OUTPUT_T gray_out = {
        .out_buf      = gray_buf,
        .out_buf_size = gray_size,
        .out_width    = w,
        .out_height   = h,
    };
    OPERATE_RET rt = tal_image_jpeg_decode_gray(jpeg_data, jpeg_size, &gray_out);
    if (rt != OPRT_OK) {
        Free(gray_buf);
        return rt;
    }

    int16_t thr = (int16_t)threshold;
    uint8_t *bmp = out->out_buf;
    memset(bmp, 0, bmp_need);

    /*
     * JPEG DCT quantization introduces ±15-20 noise on flat (white/black)
     * regions. Without clamping, these near-white pixels (e.g. 238 instead of
     * 255) carry a small negative error that Floyd-Steinberg diffuses to
     * neighbours; the accumulated error eventually flips a pixel black,
     * producing visible dot rows on what should be a clean white background.
     * Snap clearly-white / clearly-black pixels to their extremes first so the
     * dithering error is zero and cannot scatter dots.
     */
    int16_t white_clamp = thr + (255 - thr) / 3;  /* ~85 % of way to white */
    int16_t black_clamp = thr / 3;                 /* ~33 % of way to black */

    for (uint16_t y = 0; y < h; y++) {
        uint8_t *row  = gray_buf + (uint32_t)y * w;
        uint8_t *orow = bmp + (uint32_t)y * bytes_per_line;

        for (uint16_t x = 0; x < w; x++) {
            int16_t old_px = (int16_t)row[x];
            /* Clamp near-white / near-black to absorb JPEG quantization noise */
            if (old_px >= white_clamp) {
                old_px = 255;
            } else if (old_px <= black_clamp) {
                old_px = 0;
            }
            uint8_t new_px = (old_px < thr) ? 0 : 255;
            int16_t err    = old_px - (int16_t)new_px;

            if (new_px == 0) {
                orow[x >> 3] |= (0x80u >> (x & 7));
            }

            if (x + 1 < w) {
                int16_t v = (int16_t)row[x + 1] + (err * 7 / 16);
                row[x + 1] = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
            }
            if (y + 1 < h) {
                uint8_t *next = gray_buf + (uint32_t)(y + 1) * w;
                if (x > 0) {
                    int16_t v = (int16_t)next[x - 1] + (err * 3 / 16);
                    next[x - 1] = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
                }
                {
                    int16_t v = (int16_t)next[x] + (err * 5 / 16);
                    next[x] = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
                }
                if (x + 1 < w) {
                    int16_t v = (int16_t)next[x + 1] + (err * 1 / 16);
                    next[x + 1] = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
                }
            }
        }
    }

    Free(gray_buf);
    return OPRT_OK;
}
