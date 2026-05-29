/**
 * @file image_album.h
 * @brief Image Album Management Component - Main Interface
 *
 * This provides APIs for managing local picture albums:
 * - Support for hybrid storage: in-memory + optional filesystem persistence
 * - Store pictures (JPEG/PNG) to the album
 * - Read pictures by path (original file bytes, no decoding)
 * - Iterate through pictures with metadata (filename, path, size, format, resolution)
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */

#ifndef __IMAGE_ALBUM_H__
#define __IMAGE_ALBUM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "image_album_storage.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define ALBUM_FILENAME_MAX_LEN   64
#define ALBUM_NAME_MAX_LEN       64

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Opaque handle to album manager instance
 */
typedef void* IMAGE_ALBUM_HANDLE;

/**
 * @brief Picture format type
 */
typedef enum {
    ALBUM_IMAGE_FORMAT_JPEG = 0,
    ALBUM_IMAGE_FORMAT_PNG,
    ALBUM_IMAGE_FORMAT_MAX
} ALBUM_IMAGE_FORMAT_E;

/**
 * @brief App-provided picture metadata used for sorting.
 */
typedef struct {
    const char            *filename;
    ALBUM_IMAGE_FORMAT_E   format;
    const uint8_t         *file_data;
    size_t                 file_size;
    uint64_t               seq;
    uint64_t               timestamp;
} ALBUM_IMAGE_SAVE_INFO_T;

/**
 * @brief Optional parameters for @ref image_album_init()
 */
typedef struct {
    /**
     * @brief Enabled storage backend(s), OR of @ref IMAGE_ALBUM_STORAGE_TP_*.
     *
     * 0 means @ref IMAGE_ALBUM_STORAGE_TP_ALL (every registered backend participates).
     */
    IMAGE_ALBUM_STORAGE_TP_E storage_mask;

    /**
     * @brief Storage backend(s) used for image list recovery at init, OR of @ref IMAGE_ALBUM_STORAGE_TP_*.
     *
     * Determines which backend is the source of truth for the image list on startup.
     * 0 means no recovery (image list starts empty).
     * Example: @c IMAGE_ALBUM_STORAGE_TP_SD to recover from SD card only.
     */
    IMAGE_ALBUM_STORAGE_TP_E recover_tp;
} IMAGE_ALBUM_INIT_CFG_T;

/**
 * @brief Picture attributes shared by list node and scan iterator item
 *
 * Holds size, app metadata, format, and decoded pixel dimensions (filled at save).
 */
typedef struct {
    size_t                 file_size;
    uint64_t               seq;
    uint64_t               timestamp;
    ALBUM_IMAGE_FORMAT_E   format;
    uint32_t               width;
    uint32_t               height;
} ALBUM_IMAGE_ATTR_T;

/**
 * @brief Picture item structure (iterator mode)
 *
 * Pointers alias live node storage while retained by scan; @a attr is copied per iteration step.
 */
typedef struct {
    char                   *filename;
    /**
     * @brief Opaque handle for the backend selected by @a storage_tp on item APIs (not auto).
     * @note With multiple backends, each keeps its own handle internally; use @ref image_album_read
     *       by filename if you need a different channel.
     */
    void                   *path;
    ALBUM_IMAGE_ATTR_T      attr;
} ALBUM_IMAGE_ITEM_T;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Album initialization
 *
 * Initializes the album manager. If root_path does not exist,
 * it will be created automatically.
 *
 * @param[in] cfg Optional @c storage_mask (see @ref IMAGE_ALBUM_INIT_CFG_T). NULL means all backends.
 * @param[out] album_handle Handle to the album manager
 *
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET image_album_init(char *name, const IMAGE_ALBUM_INIT_CFG_T *cfg, IMAGE_ALBUM_HANDLE *album_handle);

/**
 * @brief Album deinitialization
 *
 * Releases all resources associated with the album manager.
 *
 * @param[in] album_handle Handle to the album manager
 *
 * @return OPRT_OK on success, error code otherwise
 * @return OPRT_RESOURCE_NOT_READY if any picture or album retain count is non-zero
 */
OPERATE_RET image_album_deinit(IMAGE_ALBUM_HANDLE album_handle);

/**
 * @brief find album handle by registered name
 * @param[in] name Album name
 * @return Album handle, or NULL if @a name is NULL or no match
 */
 IMAGE_ALBUM_HANDLE image_album_find_by_name(const char *name);

/**
 * @brief Save picture file with app metadata (sequence/time).
 *
 * Stores the picture according to the configured storage mode.
 * If a picture with the same filename already exists, returns @c OPRT_COM_ERROR (no overwrite).
 * If the entry is retained (@ref image_album_item_retain_locked), returns @c OPRT_RESOURCE_NOT_READY.
 *
 * @param[in] album_handle Handle to the album manager
 * @param[in] filename Picture filename used as unique key in album
 * @param[in] format Picture format (JPEG or PNG)
 * @param[in] file_data Picture file bytes (original encoded data)
 * @param[in] file_size Size of file_data in bytes
 * @param[in] meta App-provided metadata, NULL means {save_seq=0, save_time_ms=0}
 *
 * @return OPRT_OK on success, error code otherwise
 * @return OPRT_COM_ERROR if @a filename already exists
 * @return OPRT_RESOURCE_NOT_READY if the album list is held (@ref image_album_retain_locked)
 * @note JPEG/PNG width and height are parsed from @a file_data before storage save (best-effort; 0 if unknown).
 */
OPERATE_RET image_album_save(IMAGE_ALBUM_HANDLE album_handle, ALBUM_IMAGE_SAVE_INFO_T *info);

/**
 * @brief Read picture file from album
 *
 * Reads a picture from album by filename.
 * Returns the original encoded file bytes.
 *
 * @param[in] album_handle Handle to the album manager
 * @param[in] filename Picture filename (e.g., "photo.jpg")
 * @param[in] storage_tp Storage channel: @c 0 tries every initialized backend in slot order until one
 *            succeeds; otherwise a non-zero mask of @ref IMAGE_ALBUM_STORAGE_TP_* (can OR multiple bits)
 *            restricts attempts to backends whose @c type intersects the mask (slot order).
 * @param[out] file_data Pointer to allocated picture data buffer
 *            Caller must free this buffer using image_album_free_file_data()
 * @param[out] file_size Size of allocated file_data in bytes
 *
 * @return OPRT_OK on success, error code otherwise
 * @return OPRT_NOT_FOUND if no matching backend has the object or read fails on all selected backends
 */
OPERATE_RET image_album_read(IMAGE_ALBUM_HANDLE album_handle,
                            const char *filename,
                            IMAGE_ALBUM_STORAGE_TP_E storage_tp,
                            uint8_t **file_data,
                            size_t *file_size);

/**
 * @brief Free picture data buffer allocated by image_album_read()
 *
 * @param[in] file_data Pointer to buffer allocated by image_album_read()
 *
 * @return OPRT_OK on success
 * @return OPRT_INVALID_PARM if file_data is NULL
 */
OPERATE_RET image_album_free_file_data(uint8_t *file_data);

/**
 * @brief Delete a committed picture by filename
 *
 * @param[in] album_handle Album handle
 * @param[in] filename Picture key (same as save/read)
 *
 * @return OPRT_OK on success (removed immediately, or marked for deferred removal while the album list
 *         or this entry is retained)
 * @return OPRT_NOT_FOUND if missing or not yet committed
 * @note While @ref image_album_retain_locked is held, no node is freed until the matching release.
 *       While @ref image_album_item_retain_locked is held on an entry, that entry is not freed until
 *       the last item release (and album retain, if any, is cleared).
 */
OPERATE_RET image_album_delete(IMAGE_ALBUM_HANDLE album_handle, const char *filename);

/**
 * @brief Get first or next committed picture metadata in insert order
 *
 *
 * @param[in] album_handle Album handle
 * @param[in] filename Current key, or @c NULL / empty for the first matching item
 * @param[in] storage_tp Non-zero backend selector (@ref IMAGE_ALBUM_STORAGE_TP_*); auto (@c 0) is not allowed
 * @param[out] item Filled with shallow view (filename points at live node; @a path is for @a storage_tp)
 *
 * @return OPRT_OK on success
 * @return OPRT_INVALID_PARM if bad parameters or @a storage_tp is zero
 * @return OPRT_NOT_FOUND if there is no matching next (or first) image on the selected backend(s)
 * @note Copy @a item fields if the album may change. Entries scheduled for deferred delete are skipped.
 */
OPERATE_RET image_album_get_next_item(IMAGE_ALBUM_HANDLE album_handle,
                                      const char *filename,
                                      IMAGE_ALBUM_STORAGE_TP_E storage_tp,
                                      ALBUM_IMAGE_ITEM_T *item);

/**
 * @brief Count all visible committed pictures (any backend with stored data)
 *
 * @param[in] album_handle Album handle
 * @param[out] count Number of visible committed images (insert order; not filtered by storage channel)
 *
 * @return OPRT_OK on success
 * @return OPRT_INVALID_PARM if @a album_handle or @a count is NULL
 * @note Locks the album mutex briefly. For a per-backend count matching @ref image_album_get_next_item,
 *       iterate with @ref image_album_get_next_item until @c OPRT_NOT_FOUND (e.g. scan uses this pattern).
 */
OPERATE_RET image_album_get_committed_count(IMAGE_ALBUM_HANDLE album_handle, uint32_t *count);

/**
 * @brief Increment album-wide retain count
 *
 * Locks the album mutex internally. While count > 0, @ref image_album_deinit() returns
 * @c OPRT_RESOURCE_NOT_READY. Adding pictures (@ref image_album_save)
 * stays allowed. @ref image_album_delete schedules removal and frees nodes after the last matching
 * @ref image_album_release_locked() when safe (see @ref image_album_delete).
 * Pair each successful retain with @ref image_album_release_locked().
 *
 * @param[in] album_handle Album handle
 *
 * @return OPRT_OK on success
 * @return OPRT_INVALID_PARM if @a album_handle is NULL
 * @return OPRT_EXCEED_UPPER_LIMIT if refcount would overflow
 * @note Do not call from ISR.
 */
OPERATE_RET image_album_retain_locked(IMAGE_ALBUM_HANDLE album_handle);

/**
 * @brief Decrement album-wide retain count
 *
 * Locks the album mutex internally.
 *
 * @param[in] album_handle Album handle
 *
 * @return OPRT_OK on success
 * @return OPRT_INVALID_PARM if @a album_handle is NULL or retain underflow
 * @note When the count reaches zero, deferred @ref image_album_delete operations are applied for any
 *       node that is not still item-retained.
 * @note Do not call from ISR.
 */
OPERATE_RET image_album_release_locked(IMAGE_ALBUM_HANDLE album_handle);

/**
 * @brief Retain picture by filename (usage refcount)
 *
 * Increments an internal per-file reference count so the album defers trimming/removing
 * this entry until the last matching @ref image_album_file_release().
 * Callers identify pictures by @a filename only (no node handle).
 *
 * @param[in] album_handle Album handle
 * @param[in] filename Picture key (same rules as save/read)
 * @param[in] storage_tp Non-zero backend selector for @a item->path; auto (@c 0) is not allowed
 * @param[out] item Filled like @ref image_album_get_next_item for this file
 *
 * @return OPRT_OK on success, OPRT_NOT_FOUND if missing, not visible, no path on @a storage_tp, or deferred delete
 * @return OPRT_INVALID_PARM if @a storage_tp is zero
 * @note @a item->filename points at internal storage; duplicate filename still yields @c OPRT_COM_ERROR
 *       from save. @ref image_album_deinit() returns @c OPRT_RESOURCE_NOT_READY until all item
 *       releases (and album retain, if any) allow teardown.
 */
OPERATE_RET image_album_item_retain_locked(IMAGE_ALBUM_HANDLE album_handle,
                                           const char *filename,
                                           IMAGE_ALBUM_STORAGE_TP_E storage_tp,
                                           ALBUM_IMAGE_ITEM_T *item);

/**
 * @brief Release one retain from @ref image_album_item_retain_locked()
 *
 * @param[in] album_handle Album handle
 * @param[in] filename Picture key
 *
 * @return OPRT_OK on success, OPRT_NOT_FOUND if missing, OPRT_INVALID_PARM if retain underflow
 * @note If the entry was @ref image_album_delete'd while retained, the last successful release may
 *       free the node when the album list is not held.
 */
OPERATE_RET image_album_item_release_locked(IMAGE_ALBUM_HANDLE album_handle, const char *filename);


#ifdef __cplusplus
}
#endif

#endif // __IMAGE_ALBUM_H__
