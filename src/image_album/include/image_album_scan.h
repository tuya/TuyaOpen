/**
 * @file image_album_scan.h
 * @brief Image Album Picture Scanning Interface - Iterator Mode
 *
 * Provides lightweight iterator APIs for scanning pictures in the album
 * with metadata extraction (filename, path, size, format, resolution).
 *
 * Design optimized for embedded systems:
 * - Iterator pattern: process pictures one-by-one without loading all into memory
 * - Fixed-size buffers: no repeated malloc/free for each picture
 * - Optional lite mode: skip resolution parsing for lower memory footprint
 * - Single handle model: simplified API, no concurrent scanning
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */

#ifndef __IMAGE_ALBUM_SCAN_H__
#define __IMAGE_ALBUM_SCAN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "image_album.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void* IMAGE_ALBUM_SCAN_HANDLE;

/**
 * @brief Sort field used by scan APIs.
 */
typedef enum {
    IMAGE_ALBUM_SORT_NONE = 0,
    IMAGE_ALBUM_SORT_SAVE_SEQ,
    IMAGE_ALBUM_SORT_SAVE_TIME,
    IMAGE_ALBUM_SORT_FILE_SIZE,
    IMAGE_ALBUM_SORT_MAX
} IMAGE_ALBUM_SORT_KEY_E;

/**
 * @brief Sort order used by scan APIs.
 */
typedef enum {
    IMAGE_ALBUM_SORT_ASC = 0,
    IMAGE_ALBUM_SORT_DESC,
    IMAGE_ALBUM_SORT_ORDER_MAX
} IMAGE_ALBUM_SORT_ORDER_E;

/**
 * @brief Scan sort option.
 */
typedef struct {
    IMAGE_ALBUM_SORT_KEY_E key;
    IMAGE_ALBUM_SORT_ORDER_E order;
} IMAGE_ALBUM_SORT_OPT_T;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Initialize picture scanner
 *
 * Sets up iterator mode to enumerate pictures in the album.
 * Supports both full metadata (with resolution) and lite (fast, no resolution) scanning.
 *
 * @param[in] name Album name (registered via image_album_init)
 * @param[in] storage_tp Non-zero backend selector for @a path in each item (same as @ref image_album_get_next_item);
 *            auto (@c 0) is not allowed
 * @param[out] scan_handle Receives scan handle for subsequent scan APIs
 *
 * @return OPRT_OK on success, error code otherwise
 *
 * @note Calls @ref image_album_retain_locked() until @ref image_album_scan_deinit (which releases it).
 *       Iterator @a filename / @a path alias live nodes; valid until deinit.
 */
int image_album_scan_init(char *name,
                          IMAGE_ALBUM_STORAGE_TP_E storage_tp,
                          IMAGE_ALBUM_SCAN_HANDLE *scan_handle);

/**
 * @brief Get total count of pictures in album
 *
 * Counts all valid pictures (jpg/jpeg/png files) without loading metadata.
 * Must be called after image_album_scan_init() and before iterating.
 *
 * @param[in] scan_handle Scan handle
 * @param[out] count Number of valid pictures found
 *
 * @return OPRT_OK on success, error code otherwise
 *
 * @note Allows caller to pre-allocate buffers or know expected iteration count
 */
uint32_t image_album_scan_get_count(IMAGE_ALBUM_SCAN_HANDLE scan_handle);

/**
 * @brief Set scan sort option.
 *
 * Must be called after image_album_scan_init() and before
 * the first image_album_scan_next().
 *
 * @param[in] scan_handle Scan handle
 * @param[in] sort_opt Sort option, NULL means IMAGE_ALBUM_SORT_NONE + ASC
 *
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET image_album_scan_set_sort(IMAGE_ALBUM_SCAN_HANDLE scan_handle,
                                      const IMAGE_ALBUM_SORT_OPT_T *sort_opt);

/**
 * @brief Get next picture item in iteration
 *
 * Iterates through pictures one by one, filling fixed-size buffer.
 * Fields @a filename and @a path point into the live node; @a attr is copied each step (see @ref ALBUM_IMAGE_ATTR_T).
 *
 * @param[in] scan_handle Scan handle
 * @param[out] item Picture item (fixed-size buffer, caller-provided)
 *
 * @return OPRT_OK on success, error code otherwise
 *
 * @note Memory-efficient: reuses the same output buffer for each call
 * @note Valid while scan is active and album list retain from @ref image_album_scan_init is held
 */
OPERATE_RET image_album_scan_next(IMAGE_ALBUM_SCAN_HANDLE scan_handle, ALBUM_IMAGE_ITEM_T *item);

/**
 * @brief Get previous picture item in iteration
 *
 * Moves the iterator one step backward and returns that item.
 * Requires at least one @ref image_album_scan_next call before the first
 * @ref image_album_scan_prev. Returns OPRT_NOT_FOUND when already at the
 * beginning (no previous item exists).
 *
 * After scan_prev returns item[N], a subsequent scan_next will return item[N+1].
 *
 * @param[in] scan_handle Scan handle
 * @param[out] item Picture item (fixed-size buffer, caller-provided)
 *
 * @return OPRT_OK on success, OPRT_NOT_FOUND when no previous item exists
 *
 * @note Memory-efficient: reuses the same output buffer for each call
 * @note Valid while scan is active and album list retain from @ref image_album_scan_init is held
 */
OPERATE_RET image_album_scan_prev(IMAGE_ALBUM_SCAN_HANDLE scan_handle, ALBUM_IMAGE_ITEM_T *item);

/**
 * @brief Deinitialize picture scanner
 *
 * Releases scanning resources and marks album as ready for other operations.
 * Must be called when scanning is complete.
 *
 * @param[in] scan_handle Scan handle
 *
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET image_album_scan_deinit(IMAGE_ALBUM_SCAN_HANDLE scan_handle);

/**
 * @brief Seek the scan iterator to an absolute position.
 *
 * Sets the internal cursor to @a pos (1-based) so that the item at @a pos
 * becomes the current item. A subsequent scan_next() returns pos+1,
 * a subsequent scan_prev() returns pos-1.
 * Seeking to 0 rewinds to before the first item (initial state).
 *
 * @param[in] scan_handle  Scan handle from image_album_scan_init()
 * @param[in] pos          1-based target position (0 = reset to initial state)
 *
 * @return OPRT_OK on success
 * @return OPRT_INVALID_PARM if @a scan_handle is NULL
 */
OPERATE_RET image_album_scan_seek(IMAGE_ALBUM_SCAN_HANDLE scan_handle, uint32_t pos);

#ifdef __cplusplus
}
#endif

#endif // __IMAGE_ALBUM_SCAN_H__
