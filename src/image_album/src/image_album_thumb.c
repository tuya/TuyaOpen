/**
 * @file image_album_thumb.c
 * @brief Thumbnail generation for image album.
 *
 * Iterator + batch are built directly on top of the scan API
 * (image_album_scan_*). The scan layer owns all list management and sorting;
 * this file only adds thumbnail generation on top.
 *
 * JPEG images are decoded and scaled in one pass via tal_image_jpeg_scale_*.
 * PNG is not currently supported (returns OPRT_NOT_SUPPORTED).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "image_album_priv.h"
#include "tal_image.h"
#include "tal_log.h"
#include <string.h>

/***********************************************************
 * Internal types
 ***********************************************************/

typedef struct {
    IMAGE_ALBUM_SCAN_HANDLE   scan;        /* owns the album list retain */
    IMAGE_ALBUM_HANDLE        album;       /* for image_album_read calls */
    IMAGE_ALBUM_STORAGE_TP_E  storage_tp;
    uint32_t                  total;       /* from scan_get_count */
    uint32_t                  cursor;      /* shadow of scan's internal position */
} ALBUM_THUMB_ITER_CTX_T;

/***********************************************************
 * Static helpers: thumbnail generation
 ***********************************************************/

static uint8_t __bpp(ALBUM_THUMB_FMT_E fmt)
{
    switch (fmt) {
    case ALBUM_THUMB_FMT_RGB565: return 2u;
    case ALBUM_THUMB_FMT_RGB888: return 3u;
    case ALBUM_THUMB_FMT_GRAY:   return 1u;
    default:                     return 0u;
    }
}

static ALBUM_IMAGE_FORMAT_E __detect_format(const uint8_t *data, size_t size)
{
    if (size >= 3u && data[0] == 0xFFu && data[1] == 0xD8u && data[2] == 0xFFu) {
        return ALBUM_IMAGE_FORMAT_JPEG;
    }
    if (size >= 4u && data[0] == 0x89u && data[1] == 0x50u &&
        data[2] == 0x4Eu && data[3] == 0x47u) {
        return ALBUM_IMAGE_FORMAT_PNG;
    }
    return ALBUM_IMAGE_FORMAT_MAX;
}

/**
 * @brief Compute scaled output dimensions for CONTAIN / COVER fit modes.
 *
 * Uses Q16 fixed-point arithmetic.
 */
static void __fit_size(uint16_t src_w, uint16_t src_h,
                       uint16_t dst_w, uint16_t dst_h,
                       ALBUM_THUMB_FIT_E fit,
                       uint16_t *out_w, uint16_t *out_h)
{
    uint32_t sx, sy, scale;

    if (fit == ALBUM_THUMB_FIT_STRETCH || src_w == 0u || src_h == 0u) {
        *out_w = dst_w;
        *out_h = dst_h;
        return;
    }

    sx = ((uint32_t)dst_w << 16u) / src_w;
    sy = ((uint32_t)dst_h << 16u) / src_h;

    scale = (fit == ALBUM_THUMB_FIT_CONTAIN)
            ? ((sx < sy) ? sx : sy)
            : ((sx > sy) ? sx : sy);   /* COVER */

    *out_w = (uint16_t)(((uint32_t)src_w * scale + (1u << 15u)) >> 16u);
    *out_h = (uint16_t)(((uint32_t)src_h * scale + (1u << 15u)) >> 16u);
    if (*out_w == 0u) { *out_w = 1u; }
    if (*out_h == 0u) { *out_h = 1u; }
}

/**
 * @brief Allocate a center-cropped copy of a pixel buffer.
 */
static OPERATE_RET __crop_center(const uint8_t *src,
                                  uint16_t src_w, uint16_t src_h,
                                  uint16_t crop_w, uint16_t crop_h,
                                  uint8_t bpp,
                                  uint8_t **out_buf, uint32_t *out_size)
{
    uint16_t off_x, off_y;
    uint32_t row_bytes, total;
    uint8_t *buf;
    uint16_t y;

    if (crop_w > src_w) { crop_w = src_w; }
    if (crop_h > src_h) { crop_h = src_h; }

    off_x = (src_w > crop_w) ? ((src_w - crop_w) / 2u) : 0u;
    off_y = (src_h > crop_h) ? ((src_h - crop_h) / 2u) : 0u;

    row_bytes = (uint32_t)crop_w * bpp;
    total     = row_bytes * crop_h;

    buf = (uint8_t *)Malloc(total);
    if (buf == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    for (y = 0u; y < crop_h; y++) {
        const uint8_t *src_row = src + ((uint32_t)(off_y + y) * src_w + off_x) * bpp;
        uint8_t       *dst_row = buf + (uint32_t)y * row_bytes;
        memcpy(dst_row, src_row, row_bytes);
    }

    *out_buf  = buf;
    *out_size = total;
    return OPRT_OK;
}

/**
 * @brief Decode + scale one JPEG image into a thumbnail pixel buffer.
 *
 * For CONTAIN/COVER the source dimensions are parsed first so the scale
 * target can be computed. COVER additionally crops the center of the result.
 * PNG is not currently supported.
 */
static OPERATE_RET __make_thumb(const uint8_t *file_data, size_t file_size,
                                ALBUM_IMAGE_FORMAT_E format,
                                const ALBUM_THUMB_CFG_T *cfg,
                                ALBUM_THUMB_T *out)
{
    OPERATE_RET rt;
    TAL_IMAGE_JPEG_SCALE_IN_T in;
    TAL_IMAGE_SCALE_OUT_T scale_out;
    uint16_t scale_w, scale_h;

    if (format != ALBUM_IMAGE_FORMAT_JPEG) {
        PR_ERR("thumb: format %d not supported", (int)format);
        return OPRT_NOT_SUPPORTED;
    }

    if (cfg->fit != ALBUM_THUMB_FIT_STRETCH) {
        TAL_IMAGE_JPEG_INFO_T info;
        rt = tal_image_jpeg_get_info(file_data, (uint32_t)file_size, &info);
        if (rt != OPRT_OK) {
            PR_ERR("thumb: jpeg_get_info failed %d", rt);
            return OPRT_COM_ERROR;
        }
        __fit_size(info.width, info.height, cfg->width, cfg->height,
                   cfg->fit, &scale_w, &scale_h);
    } else {
        scale_w = cfg->width;
        scale_h = cfg->height;
    }

    memset(&in, 0, sizeof(in));
    in.method     = TAL_IMAGE_SCALE_MTH_BILINEAR;
    in.mode       = TAL_IMAGE_SCALE_MODE_SIZE;
    in.data       = file_data;
    in.size       = (uint32_t)file_size;
    in.out_width  = scale_w;
    in.out_height = scale_h;

    memset(&scale_out, 0, sizeof(scale_out));

    switch (cfg->fmt) {
    case ALBUM_THUMB_FMT_RGB565:
        rt = tal_image_jpeg_scale_rgb565(&in, &scale_out);
        break;
    case ALBUM_THUMB_FMT_RGB888:
        rt = tal_image_jpeg_scale_rgb888(&in, &scale_out);
        break;
    case ALBUM_THUMB_FMT_GRAY:
        rt = tal_image_jpeg_scale_gray(&in, &scale_out);
        break;
    default:
        return OPRT_INVALID_PARM;
    }

    if (rt != OPRT_OK) {
        PR_ERR("thumb: scale failed %d", rt);
        return OPRT_COM_ERROR;
    }

    if (cfg->fit == ALBUM_THUMB_FIT_COVER &&
        (scale_out.width > cfg->width || scale_out.height > cfg->height)) {
        uint8_t *crop_buf  = NULL;
        uint32_t crop_size = 0u;

        rt = __crop_center(scale_out.buf,
                           scale_out.width, scale_out.height,
                           cfg->width, cfg->height,
                           __bpp(cfg->fmt), &crop_buf, &crop_size);
        tal_image_scale_buf_free(&scale_out);
        if (rt != OPRT_OK) {
            return rt;
        }
        out->buf    = crop_buf;
        out->size   = crop_size;
        out->width  = cfg->width;
        out->height = cfg->height;
    } else {
        out->buf    = scale_out.buf;
        out->size   = scale_out.size;
        out->width  = scale_out.width;
        out->height = scale_out.height;
    }

    return OPRT_OK;
}

/***********************************************************
 * Static helpers: batch fetch via scan
 ***********************************************************/

/**
 * @brief Advance @a scan by @a n steps, generating thumbnails.
 *
 * Each step: scan_next for metadata, image_album_read for data, __make_thumb
 * for pixels. Per-item failures set thumb.buf = NULL but do not stop the loop.
 *
 * Returns OPRT_COM_ERROR if any item failed; successful items are still in @a batch.
 */
static OPERATE_RET __advance_and_generate(ALBUM_THUMB_ITER_CTX_T *ctx,
                                           const ALBUM_THUMB_CFG_T *cfg,
                                           uint32_t n,
                                           ALBUM_THUMB_BATCH_T *batch)
{
    ALBUM_THUMB_ITEM_T *items;
    OPERATE_RET ret = OPRT_OK;
    uint32_t count = 0u;
    uint32_t i;

    items = (ALBUM_THUMB_ITEM_T *)Malloc(n * sizeof(ALBUM_THUMB_ITEM_T));
    if (items == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(items, 0, n * sizeof(ALBUM_THUMB_ITEM_T));

    for (i = 0u; i < n; i++) {
        ALBUM_IMAGE_ITEM_T scan_item;
        ALBUM_THUMB_ITEM_T *out_item = &items[count];
        uint8_t *file_data = NULL;
        size_t   file_size = 0u;
        OPERATE_RET rt;

        rt = image_album_scan_next(ctx->scan, &scan_item);
        if (rt != OPRT_OK) {
            break;  /* end of album */
        }
        ctx->cursor++;

        strncpy(out_item->filename, scan_item.filename, ALBUM_FILENAME_MAX_LEN - 1u);
        out_item->filename[ALBUM_FILENAME_MAX_LEN - 1u] = '\0';
        out_item->attr = scan_item.attr;

        rt = image_album_read(ctx->album, scan_item.filename, ctx->storage_tp,
                               &file_data, &file_size);
        if (rt != OPRT_OK) {
            PR_ERR("thumb: read '%s' failed %d", scan_item.filename, rt);
            ret = OPRT_COM_ERROR;
            count++;
            continue;
        }

        rt = __make_thumb(file_data, file_size,
                          __detect_format(file_data, file_size),
                          cfg, &out_item->thumb);
        image_album_free_file_data(file_data);
        if (rt != OPRT_OK) {
            PR_ERR("thumb: generate '%s' failed %d", scan_item.filename, rt);
            ret = OPRT_COM_ERROR;
        }
        count++;
    }

    if (count == 0u) {
        Free(items);
        batch->items = NULL;
        batch->count = 0u;
        return OPRT_NOT_FOUND;
    }

    batch->items = items;
    batch->count = count;
    return ret;
}

/***********************************************************
 * Public API: single-image
 ***********************************************************/

OPERATE_RET album_thumb_get(IMAGE_ALBUM_HANDLE handle,
                            const char *filename,
                            IMAGE_ALBUM_STORAGE_TP_E storage_tp,
                            const ALBUM_THUMB_CFG_T *cfg,
                            ALBUM_THUMB_T *out)
{
    OPERATE_RET rt;
    uint8_t *file_data = NULL;
    size_t   file_size = 0u;

    if (handle == NULL || filename == NULL || cfg == NULL || out == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (storage_tp == 0u || cfg->width == 0u || cfg->height == 0u) {
        return OPRT_INVALID_PARM;
    }

    memset(out, 0, sizeof(ALBUM_THUMB_T));

    rt = image_album_read(handle, filename, storage_tp, &file_data, &file_size);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = __make_thumb(file_data, file_size,
                      __detect_format(file_data, file_size),
                      cfg, out);
    image_album_free_file_data(file_data);
    return rt;
}

OPERATE_RET album_thumb_free(ALBUM_THUMB_T *out)
{
    if (out == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (out->buf != NULL) {
        Free(out->buf);
        out->buf  = NULL;
        out->size = 0u;
    }
    return OPRT_OK;
}

/***********************************************************
 * Public API: iterator + batch
 ***********************************************************/

OPERATE_RET image_album_thumb_iter_init(char *name,
                                        IMAGE_ALBUM_STORAGE_TP_E storage_tp,
                                        const IMAGE_ALBUM_SORT_OPT_T *sort_opt,
                                        ALBUM_THUMB_ITER_HANDLE *iter)
{
    ALBUM_THUMB_ITER_CTX_T *ctx;
    OPERATE_RET rt;

    if (name == NULL || iter == NULL || storage_tp == 0u) {
        return OPRT_INVALID_PARM;
    }

    *iter = NULL;

    ctx = (ALBUM_THUMB_ITER_CTX_T *)Malloc(sizeof(ALBUM_THUMB_ITER_CTX_T));
    if (ctx == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(ctx, 0, sizeof(ALBUM_THUMB_ITER_CTX_T));

    ctx->storage_tp = storage_tp;

    ctx->album = image_album_find_by_name(name);
    if (ctx->album == NULL) {
        Free(ctx);
        return OPRT_NOT_FOUND;
    }

    rt = image_album_scan_init(name, storage_tp, &ctx->scan);
    if (rt != OPRT_OK) {
        Free(ctx);
        return rt;
    }

    rt = image_album_scan_set_sort(ctx->scan, sort_opt);
    if (rt != OPRT_OK) {
        image_album_scan_deinit(ctx->scan);
        Free(ctx);
        return rt;
    }

    ctx->total  = image_album_scan_get_count(ctx->scan);
    ctx->cursor = 0u;

    *iter = (ALBUM_THUMB_ITER_HANDLE)ctx;
    return OPRT_OK;
}

uint32_t image_album_thumb_iter_count(ALBUM_THUMB_ITER_HANDLE iter)
{
    if (iter == NULL) {
        return 0u;
    }
    return ((ALBUM_THUMB_ITER_CTX_T *)iter)->total;
}

OPERATE_RET image_album_thumb_iter_next(ALBUM_THUMB_ITER_HANDLE iter,
                                        const ALBUM_THUMB_CFG_T *cfg,
                                        uint32_t n,
                                        ALBUM_THUMB_BATCH_T *batch)
{
    ALBUM_THUMB_ITER_CTX_T *ctx;

    if (iter == NULL || cfg == NULL || batch == NULL || n == 0u) {
        return OPRT_INVALID_PARM;
    }

    ctx = (ALBUM_THUMB_ITER_CTX_T *)iter;

    if (ctx->cursor >= ctx->total) {
        batch->items = NULL;
        batch->count = 0u;
        return OPRT_NOT_FOUND;
    }

    return __advance_and_generate(ctx, cfg, n, batch);
}

OPERATE_RET image_album_thumb_iter_prev(ALBUM_THUMB_ITER_HANDLE iter,
                                        const ALBUM_THUMB_CFG_T *cfg,
                                        uint32_t n,
                                        ALBUM_THUMB_BATCH_T *batch)
{
    ALBUM_THUMB_ITER_CTX_T *ctx;
    uint32_t new_start;
    OPERATE_RET rt;

    if (iter == NULL || cfg == NULL || batch == NULL || n == 0u) {
        return OPRT_INVALID_PARM;
    }

    ctx = (ALBUM_THUMB_ITER_CTX_T *)iter;

    /* mirror of scan_prev "index < 2" condition, extended to batch size n */
    if (ctx->cursor < 2u * n) {
        batch->items = NULL;
        batch->count = 0u;
        return OPRT_NOT_FOUND;
    }

    new_start = ctx->cursor - 2u * n;

    /* reposition scan to new_start using the seek interface */
    rt = image_album_scan_seek(ctx->scan, new_start);
    if (rt != OPRT_OK) {
        return rt;
    }
    ctx->cursor = new_start;

    return __advance_and_generate(ctx, cfg, n, batch);
}

OPERATE_RET image_album_thumb_batch_free(ALBUM_THUMB_BATCH_T *batch)
{
    uint32_t i;

    if (batch == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (batch->items != NULL) {
        for (i = 0u; i < batch->count; i++) {
            ALBUM_THUMB_T *t = &batch->items[i].thumb;
            if (t->buf != NULL) {
                Free(t->buf);
                t->buf  = NULL;
                t->size = 0u;
            }
        }
        Free(batch->items);
        batch->items = NULL;
        batch->count = 0u;
    }

    return OPRT_OK;
}

OPERATE_RET image_album_thumb_iter_deinit(ALBUM_THUMB_ITER_HANDLE iter)
{
    ALBUM_THUMB_ITER_CTX_T *ctx;

    if (iter == NULL) {
        return OPRT_INVALID_PARM;
    }

    ctx = (ALBUM_THUMB_ITER_CTX_T *)iter;
    image_album_scan_deinit(ctx->scan);
    Free(ctx);
    return OPRT_OK;
}
