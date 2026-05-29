/**
 * @file image_album_thumb.h
 * @brief Image Album Thumbnail Interface
 *
 * Provides two levels of thumbnail access:
 *
 * 1. **Single** – generate one thumbnail by filename:
 *    album_thumb_get() / album_thumb_free()
 *
 * 2. **Batch iterator** – enumerate thumbnails in sorted order, page by page:
 *    album_thumb_iter_init()
 *    album_thumb_iter_count()
 *    album_thumb_iter_next()
 *    album_thumb_batch_free()
 *    album_thumb_iter_deinit()
 *
 * Thumbnails are generated on demand: the original image is read from storage,
 * decoded, and scaled via tal_image_scale.
 * All output buffers are allocated internally and must be released with the
 * matching free function.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __IMAGE_ALBUM_THUMB_H__
#define __IMAGE_ALBUM_THUMB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "image_album.h"
#include "image_album_scan.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/

/**
 * @brief Opaque handle to a thumbnail iterator session.
 */
typedef void *ALBUM_THUMB_ITER_HANDLE;

/**
 * @brief Output pixel format.
 */
typedef enum {
    ALBUM_THUMB_FMT_RGB565 = 0, /**< Packed 16-bit RGB565, 2 bytes/pixel */
    ALBUM_THUMB_FMT_RGB888,     /**< Packed 24-bit RGB888, 3 bytes/pixel */
    ALBUM_THUMB_FMT_GRAY,       /**< 8-bit grayscale, 1 byte/pixel */
    ALBUM_THUMB_FMT_MAX
} ALBUM_THUMB_FMT_E;

/**
 * @brief Aspect-ratio fit mode.
 */
typedef enum {
    ALBUM_THUMB_FIT_STRETCH = 0, /**< Stretch to exact w x h (ignores aspect ratio) */
    ALBUM_THUMB_FIT_CONTAIN,     /**< Fit inside w x h, preserve aspect ratio (letterbox) */
    ALBUM_THUMB_FIT_COVER,       /**< Cover w x h, preserve aspect ratio (center crop) */
    ALBUM_THUMB_FIT_MAX
} ALBUM_THUMB_FIT_E;

/**
 * @brief Thumbnail visual parameters (size, format, fit mode).
 */
typedef struct {
    uint16_t         width;  /**< Desired width in pixels (must be > 0) */
    uint16_t         height; /**< Desired height in pixels (must be > 0) */
    ALBUM_THUMB_FMT_E fmt;   /**< Output pixel format */
    ALBUM_THUMB_FIT_E fit;   /**< Aspect-ratio fit mode */
} ALBUM_THUMB_CFG_T;

/**
 * @brief Single thumbnail pixel output.
 *
 * @a buf is allocated internally. Release with album_thumb_free() (single API)
 * or album_thumb_batch_free() (batch API).
 * @a width / @a height are the actual scaled dimensions, which may differ from
 * the requested size when fit is not STRETCH.
 */
typedef struct {
    uint8_t  *buf;    /**< Pixel buffer (NULL on failure) */
    uint32_t  size;   /**< Size of @a buf in bytes */
    uint16_t  width;  /**< Actual thumbnail width */
    uint16_t  height; /**< Actual thumbnail height */
} ALBUM_THUMB_T;

/**
 * @brief One item in a batch result: picture metadata + thumbnail pixels.
 */
typedef struct {
    char               filename[ALBUM_FILENAME_MAX_LEN]; /**< Picture filename (null-terminated copy) */
    ALBUM_IMAGE_ATTR_T attr;                             /**< Copied picture attributes */
    ALBUM_THUMB_T      thumb;                            /**< Generated thumbnail pixel data */
} ALBUM_THUMB_ITEM_T;

/**
 * @brief Batch of thumbnails returned by album_thumb_iter_next().
 *
 * @a items is an internally allocated array of @a count elements.
 * Release with album_thumb_batch_free().
 */
typedef struct {
    ALBUM_THUMB_ITEM_T *items; /**< Allocated item array */
    uint32_t            count; /**< Number of valid items */
} ALBUM_THUMB_BATCH_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/* -------------------------------------------------------
 * Single-image API
 * ----------------------------------------------------- */

/**
 * @brief Generate a thumbnail for one picture by filename.
 *
 * @param[in]  handle      Album handle from image_album_init()
 * @param[in]  filename    Picture key
 * @param[in]  storage_tp  Non-zero backend selector
 * @param[in]  cfg         Thumbnail visual parameters
 * @param[out] out         Receives pixel buffer and actual dimensions
 *
 * @return OPRT_OK on success
 * @return OPRT_INVALID_PARM if any pointer is NULL, storage_tp is zero,
 *         or cfg->width / cfg->height is zero
 * @return OPRT_NOT_FOUND if @a filename does not exist on the selected backend
 * @return OPRT_COM_ERROR if decoding or scaling fails
 */
OPERATE_RET album_thumb_get(IMAGE_ALBUM_HANDLE handle,
                            const char *filename,
                            IMAGE_ALBUM_STORAGE_TP_E storage_tp,
                            const ALBUM_THUMB_CFG_T *cfg,
                            ALBUM_THUMB_T *out);

/**
 * @brief Free a thumbnail allocated by album_thumb_get().
 *
 * Safe when out->buf is NULL. Sets out->buf = NULL, out->size = 0 on return.
 *
 * @param[in] out  Thumbnail to free
 *
 * @return OPRT_OK on success
 * @return OPRT_INVALID_PARM if @a out is NULL
 */
OPERATE_RET album_thumb_free(ALBUM_THUMB_T *out);

/* -------------------------------------------------------
 * Iterator + batch API
 * ----------------------------------------------------- */

/**
 * @brief Initialize a thumbnail iterator session.
 *
 * Internally calls image_album_scan_init() to acquire the album list retain
 * and build the sorted index. The retain is held until iter_deinit.
 *
 * @param[in]  name         Album name (same as passed to image_album_init())
 * @param[in]  storage_tp   Non-zero backend selector used for all batch reads
 * @param[in]  sort_opt     Sort key and order; NULL means SORT_NONE + ASC
 * @param[out] iter         Receives the iterator handle
 *
 * @return OPRT_OK on success
 * @return OPRT_INVALID_PARM if @a name or @a iter is NULL, or storage_tp is zero
 * @return OPRT_NOT_FOUND if no album with @a name is registered
 */
OPERATE_RET image_album_thumb_iter_init(char *name,
                                        IMAGE_ALBUM_STORAGE_TP_E storage_tp,
                                        const IMAGE_ALBUM_SORT_OPT_T *sort_opt,
                                        ALBUM_THUMB_ITER_HANDLE *iter);

/**
 * @brief Return the total number of pictures in this iterator session.
 *
 * @param[in] iter  Iterator handle
 *
 * @return Total count (0 if album is empty or handle is invalid)
 */
uint32_t image_album_thumb_iter_count(ALBUM_THUMB_ITER_HANDLE iter);

/**
 * @brief Fetch the next batch of thumbnails.
 *
 * Advances the cursor by up to @a n positions in sort order and generates
 * thumbnails for each picture. @a cfg may differ across calls (adaptive resolution).
 * @a batch->items is allocated internally; release with album_thumb_batch_free().
 *
 * @param[in]  iter    Iterator handle
 * @param[in]  cfg     Thumbnail visual parameters for this batch
 * @param[in]  n       Max items to fetch (must be > 0)
 * @param[out] batch   Receives the item array and actual count
 *
 * @return OPRT_OK on success (batch->count may be < n at the last page)
 * @return OPRT_NOT_FOUND when the iterator is exhausted (batch->count == 0)
 * @return OPRT_INVALID_PARM if any pointer is NULL or n is 0
 * @return OPRT_COM_ERROR if scaling fails for one or more items; successfully
 *         generated items are still returned (failed items have thumb.buf == NULL)
 *
 * @note Call iter_deinit + iter_init to restart from the beginning.
 */
OPERATE_RET image_album_thumb_iter_next(ALBUM_THUMB_ITER_HANDLE iter,
                                        const ALBUM_THUMB_CFG_T *cfg,
                                        uint32_t n,
                                        ALBUM_THUMB_BATCH_T *batch);

/**
 * @brief Fetch the previous batch of thumbnails.
 *
 * Moves the cursor back by up to @a n positions in sort order and generates
 * thumbnails for those pictures. Items are returned in reverse sort order
 * (i.e. the item closest to the current cursor comes first).
 * Requires at least one prior album_thumb_iter_next() call so there are
 * preceding items to return.
 *
 * @a batch->items is allocated internally; release with album_thumb_batch_free().
 *
 * After this call returns item[N-1]…item[N-n], a subsequent album_thumb_iter_next()
 * will return item[N]…item[N+n-1] again.
 *
 * @param[in]  iter    Iterator handle
 * @param[in]  cfg     Thumbnail visual parameters for this batch
 * @param[in]  n       Max items to fetch going backward (must be > 0)
 * @param[out] batch   Receives the item array and actual count
 *
 * @return OPRT_OK on success (batch->count may be < n at the first page)
 * @return OPRT_NOT_FOUND when already at the beginning (batch->count == 0)
 * @return OPRT_INVALID_PARM if any pointer is NULL or n is 0
 * @return OPRT_COM_ERROR if scaling fails for one or more items; successfully
 *         generated items are still returned (failed items have thumb.buf == NULL)
 */
OPERATE_RET image_album_thumb_iter_prev(ALBUM_THUMB_ITER_HANDLE iter,
                                        const ALBUM_THUMB_CFG_T *cfg,
                                        uint32_t n,
                                        ALBUM_THUMB_BATCH_T *batch);

/**
 * @brief Free a batch allocated by album_thumb_iter_next() or album_thumb_iter_prev().
 *
 * Frees all pixel buffers inside @a batch->items and the array itself.
 * Sets batch->items = NULL, batch->count = 0 on return.
 *
 * @param[in] batch  Batch to free
 *
 * @return OPRT_OK on success
 * @return OPRT_INVALID_PARM if @a batch is NULL
 */
OPERATE_RET image_album_thumb_batch_free(ALBUM_THUMB_BATCH_T *batch);

/**
 * @brief Deinitialize a thumbnail iterator session.
 *
 * Releases the album list retain acquired at iter_init.
 * Free all outstanding batches with album_thumb_batch_free() before calling this.
 *
 * @param[in] iter  Iterator handle
 *
 * @return OPRT_OK on success
 * @return OPRT_INVALID_PARM if @a iter is NULL
 */
OPERATE_RET image_album_thumb_iter_deinit(ALBUM_THUMB_ITER_HANDLE iter);

#ifdef __cplusplus
}
#endif

#endif /* __IMAGE_ALBUM_THUMB_H__ */
