/**
 * @file image_album_scan.c
 * @brief Image album picture scanning iterator implementation
 *
 * Holds the image list via @ref image_album_retain_locked(), sizes the snapshot by counting
 * @ref image_album_get_next_item() results for the scan @a storage_tp, then fills entries, optional
 * sort by index permutation, and calls
 * @ref image_album_release_locked() on @ref image_album_scan_deinit().
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "image_album_priv.h"
#include "tal_log.h"
#include <string.h>

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Scan iterator state (module-internal, heap-allocated)
 *
 * @a entries is an array of @ref ALBUM_IMAGE_ITEM_T (shallow snapshot; pointers alias live nodes while
 * album retain is held).
 */
typedef struct {
    ALBUM_IMAGE_ITEM_T      *entries;
    uint32_t                *sorted_indices;
    uint32_t                 item_count;
    IMAGE_ALBUM_HANDLE       album_handle;
    IMAGE_ALBUM_STORAGE_TP_E storage_tp;
    bool_t                   active;
    int32_t                  index;      /**< current position, 1-based. 0 = before first item */
    uint32_t                 sorted_count;
    IMAGE_ALBUM_SORT_OPT_T   sort_opt;
    bool_t                   sort_applied;
} ALBUM_SCAN_CTX_T;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Sort scan indices using insertion sort
 * @param[in,out] scan Scan context
 * @return none
 */
static void __sort_indices(ALBUM_SCAN_CTX_T *scan)
{
    uint32_t *indices = scan->sorted_indices;
    uint32_t count = scan->sorted_count;
    ALBUM_IMAGE_ITEM_T *ent = scan->entries;
    IMAGE_ALBUM_SORT_KEY_E key = scan->sort_opt.key;
    bool_t desc = (scan->sort_opt.order == IMAGE_ALBUM_SORT_DESC);
    uint32_t i;

    if (indices == NULL || ent == NULL || count == 0U) {
        return;
    }

    for (i = 1U; i < count; i++) {
        uint32_t tmp = indices[i];
        int j = (int)i - 1;

        while (j >= 0) {
            bool_t should_swap = FALSE;
            uint32_t jidx = indices[j];

            switch (key) {
            case IMAGE_ALBUM_SORT_SAVE_SEQ:
                should_swap = desc
                    ? (ent[jidx].attr.seq < ent[tmp].attr.seq)
                    : (ent[jidx].attr.seq > ent[tmp].attr.seq);
                break;
            case IMAGE_ALBUM_SORT_SAVE_TIME:
                should_swap = desc
                    ? (ent[jidx].attr.timestamp < ent[tmp].attr.timestamp)
                    : (ent[jidx].attr.timestamp > ent[tmp].attr.timestamp);
                break;
            case IMAGE_ALBUM_SORT_FILE_SIZE:
                should_swap = desc
                    ? (ent[jidx].attr.file_size < ent[tmp].attr.file_size)
                    : (ent[jidx].attr.file_size > ent[tmp].attr.file_size);
                break;
            default:
                break;
            }
            if (!should_swap) {
                break;
            }
            indices[(uint32_t)j + 1U] = indices[j];
            j--;
        }
        indices[(uint32_t)j + 1U] = tmp;
    }
}

/**
 * @brief Build identity permutation for @a scan->entries
 * @param[in,out] scan Scan context
 * @return OPRT_OK on success
 */
static OPERATE_RET __scan_build_indices(ALBUM_SCAN_CTX_T *scan)
{
    uint32_t i;

    scan->sorted_count = scan->item_count;

    if (scan->item_count == 0U) {
        scan->sorted_indices = NULL;
        return OPRT_OK;
    }

    scan->sorted_indices = (uint32_t *)Malloc(scan->item_count * sizeof(uint32_t));
    if (scan->sorted_indices == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    for (i = 0U; i < scan->item_count; i++) {
        scan->sorted_indices[i] = i;
    }

    return OPRT_OK;
}

/**
 * @brief Collect committed items (caller must have called @ref image_album_retain_locked)
 * @param[in] album Album handle
 * @param[in,out] scan Scan context (sets @a entries and @a item_count)
 * @return OPRT_OK on success
 */
static OPERATE_RET __scan_collect(IMAGE_ALBUM_HANDLE album, ALBUM_SCAN_CTX_T *scan)
{
    OPERATE_RET rt;
    uint32_t n;
    uint32_t i;
    ALBUM_IMAGE_ITEM_T *entries = NULL;
    ALBUM_IMAGE_ITEM_T it;
    char prev[ALBUM_FILENAME_MAX_LEN];
    const char *prev_arg = NULL;

    scan->entries = NULL;
    scan->item_count = 0U;

    rt = image_album_get_committed_count(album,  &n);
    if (rt != OPRT_OK) {
        return rt;
    }

    if (n == 0U) {
        return OPRT_OK;
    }

    entries = (ALBUM_IMAGE_ITEM_T *)Malloc((size_t)n * sizeof(ALBUM_IMAGE_ITEM_T));
    if (entries == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    for (i = 0U; i < n; i++) {
        rt = image_album_get_next_item(album, prev_arg, scan->storage_tp, &it);
        if (rt != OPRT_OK) {
            break;
        }
        memset(&entries[i], 0, sizeof(ALBUM_IMAGE_ITEM_T));
        entries[i].filename = it.filename;
        entries[i].path = it.path;
        entries[i].attr = it.attr;
        strncpy(prev, it.filename, sizeof(prev) - 1U);
        prev[sizeof(prev) - 1U] = '\0';
        prev_arg = prev;
        scan->item_count++;
        PR_DEBUG("item_count:%d",  scan->item_count);
    }

    scan->entries = entries;

    return OPRT_OK;
}

/**
 * @brief Apply sort option for the current scan
 * @param[in,out] scan Scan context
 * @param[in] sort_opt Sort option, NULL means none + ASC
 * @return OPRT_OK on success
 */
static OPERATE_RET __scan_set_sort(ALBUM_SCAN_CTX_T *scan, const IMAGE_ALBUM_SORT_OPT_T *sort_opt)
{
    if (!scan->active) {
        return OPRT_COM_ERROR;
    }

    if (scan->index > 0) {
        PR_ERR("set_sort must be called before first scan_next");
        return OPRT_COM_ERROR;
    }

    if (sort_opt != NULL) {
        if (sort_opt->key >= IMAGE_ALBUM_SORT_MAX || sort_opt->order >= IMAGE_ALBUM_SORT_ORDER_MAX) {
            return OPRT_INVALID_PARM;
        }
        scan->sort_opt = *sort_opt;
    } else {
        scan->sort_opt.key = IMAGE_ALBUM_SORT_NONE;
        scan->sort_opt.order = IMAGE_ALBUM_SORT_ASC;
    }

    scan->sort_applied = FALSE;
    return OPRT_OK;
}

/**
 * @brief Get the next picture item in the scan iteration
 * @param[in,out] scan Scan context
 * @param[out] item Picture item to fill
 * @return OPRT_OK on success, OPRT_NOT_FOUND at end of iteration
 */
static OPERATE_RET __scan_next(ALBUM_SCAN_CTX_T *scan, ALBUM_IMAGE_ITEM_T *item)
{
    uint32_t pic_idx;

    if (!scan->active) {
        return OPRT_COM_ERROR;
    }

    if (!scan->sort_applied && scan->sort_opt.key != IMAGE_ALBUM_SORT_NONE) {
        __sort_indices(scan);
        scan->sort_applied = TRUE;
    }

    if (scan->sorted_indices == NULL || scan->entries == NULL) {
        return OPRT_NOT_FOUND;
    }

    PR_DEBUG("scan_next: index=%d, sorted_count=%d", scan->index, scan->sorted_count);

    if (scan->index >= (int32_t)scan->sorted_count) {
        return OPRT_NOT_FOUND;
    }

    scan->index++;
    pic_idx = scan->sorted_indices[scan->index - 1];
    *item = scan->entries[pic_idx];

    return OPRT_OK;
}

/**
 * @brief Get the previous picture item in the scan iteration
 * @param[in,out] scan Scan context
 * @param[out] item Picture item to fill
 * @return OPRT_OK on success, OPRT_NOT_FOUND when no previous item
 */
static OPERATE_RET __scan_prev(ALBUM_SCAN_CTX_T *scan, ALBUM_IMAGE_ITEM_T *item)
{
    uint32_t pic_idx;

    if (!scan->active) {
        return OPRT_COM_ERROR;
    }

    if (!scan->sort_applied && scan->sort_opt.key != IMAGE_ALBUM_SORT_NONE) {
        __sort_indices(scan);
        scan->sort_applied = TRUE;
    }

    if (scan->sorted_indices == NULL || scan->entries == NULL) {
        return OPRT_NOT_FOUND;
    }

    PR_DEBUG("scan_prev: index=%d, sorted_count=%d", scan->index, scan->sorted_count);

    if (scan->index <= 1) {
        return OPRT_NOT_FOUND;
    }

    scan->index--;
    pic_idx = scan->sorted_indices[scan->index - 1];
    *item = scan->entries[pic_idx];

    return OPRT_OK;
}

/**
 * @brief Initialize picture scanner by album name
 * @param[in] name Album name (registered via @ref image_album_init)
 * @param[out] scan_handle Receives opaque scan handle
 * @return OPRT_OK on success, error code otherwise
 */
int image_album_scan_init(char *name, IMAGE_ALBUM_STORAGE_TP_E storage_tp, IMAGE_ALBUM_SCAN_HANDLE *scan_handle)
{
    IMAGE_ALBUM_HANDLE album_handle;
    ALBUM_SCAN_CTX_T *scan;
    OPERATE_RET rt;

    if (name == NULL || scan_handle == NULL) {
        return (int)OPRT_INVALID_PARM;
    }
    if (storage_tp == 0) {
        return (int)OPRT_INVALID_PARM;
    }

    *scan_handle = NULL;
    album_handle = image_album_find_by_name(name);
    if (album_handle == NULL) {
        return (int)OPRT_NOT_FOUND;
    }

    scan = (ALBUM_SCAN_CTX_T *)Malloc(sizeof(ALBUM_SCAN_CTX_T));
    if (scan == NULL) {
        return (int)OPRT_MALLOC_FAILED;
    }
    memset(scan, 0, sizeof(ALBUM_SCAN_CTX_T));
    scan->album_handle = album_handle;
    scan->storage_tp = storage_tp;

    rt = image_album_retain_locked(album_handle);
    if (rt != OPRT_OK) {
        Free(scan);
        return (int)rt;
    }

    rt = __scan_collect(album_handle, scan);
    if (rt != OPRT_OK) {
        image_album_release_locked(album_handle);
        Free(scan);
        return (int)rt;
    }

    rt = __scan_build_indices(scan);
    if (rt != OPRT_OK) {
        Free(scan->entries);
        scan->entries = NULL;
        scan->item_count = 0U;
        image_album_release_locked(album_handle);
        Free(scan);
        return (int)rt;
    }

    scan->index = 0;
    scan->sort_opt.key = IMAGE_ALBUM_SORT_NONE;
    scan->sort_opt.order = IMAGE_ALBUM_SORT_ASC;
    scan->sort_applied = FALSE;
    scan->active = TRUE;
    *scan_handle = (IMAGE_ALBUM_SCAN_HANDLE)scan;
    return (int)OPRT_OK;
}

/**
 * @brief Get total count of pictures in the current scan
 * @param[in] scan_handle Scan handle from @ref image_album_scan_init
 * @return Picture count, or 0 if handle invalid or scan inactive
 */
uint32_t image_album_scan_get_count(IMAGE_ALBUM_SCAN_HANDLE scan_handle)
{
    ALBUM_SCAN_CTX_T *scan;

    if (scan_handle == NULL) {
        return 0U;
    }
    scan = (ALBUM_SCAN_CTX_T *)scan_handle;
    if (!scan->active) {
        return 0U;
    }

    PR_DEBUG("scan->item_count:%d", scan->item_count);

    return scan->item_count;
}

/**
 * @brief Set sort option for scan iteration
 * @param[in] scan_handle Scan handle from @ref image_album_scan_init
 * @param[in] sort_opt Sort option, NULL means IMAGE_ALBUM_SORT_NONE + ASC
 * @return OPRT_OK on success
 */
OPERATE_RET image_album_scan_set_sort(IMAGE_ALBUM_SCAN_HANDLE scan_handle,
                                      const IMAGE_ALBUM_SORT_OPT_T *sort_opt)
{
    ALBUM_SCAN_CTX_T *scan;

    if (scan_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    scan = (ALBUM_SCAN_CTX_T *)scan_handle;
    return __scan_set_sort(scan, sort_opt);
}

/**
 * @brief Get the next picture item in the scan iteration
 * @param[in] scan_handle Scan handle from @ref image_album_scan_init
 * @param[out] item Picture item structure to fill
 * @return OPRT_OK on success, OPRT_NOT_FOUND at end of iteration
 */
OPERATE_RET image_album_scan_next(IMAGE_ALBUM_SCAN_HANDLE scan_handle, ALBUM_IMAGE_ITEM_T *item)
{
    ALBUM_SCAN_CTX_T *scan;

    if (scan_handle == NULL || item == NULL) {
        return OPRT_INVALID_PARM;
    }

    scan = (ALBUM_SCAN_CTX_T *)scan_handle;
    return __scan_next(scan, item);
}

/**
 * @brief Get the previous picture item in the scan iteration
 * @param[in] scan_handle Scan handle from @ref image_album_scan_init
 * @param[out] item Picture item structure to fill
 * @return OPRT_OK on success, OPRT_NOT_FOUND when no previous item
 */
OPERATE_RET image_album_scan_prev(IMAGE_ALBUM_SCAN_HANDLE scan_handle, ALBUM_IMAGE_ITEM_T *item)
{
    ALBUM_SCAN_CTX_T *scan;

    if (scan_handle == NULL || item == NULL) {
        return OPRT_INVALID_PARM;
    }

    scan = (ALBUM_SCAN_CTX_T *)scan_handle;
    return __scan_prev(scan, item);
}

/**
 * @brief Deinitialize picture scanner and release resources
 * @param[in] scan_handle Scan handle from @ref image_album_scan_init
 * @return OPRT_OK on success
 */
OPERATE_RET image_album_scan_deinit(IMAGE_ALBUM_SCAN_HANDLE scan_handle)
{
    ALBUM_SCAN_CTX_T *scan;
    IMAGE_ALBUM_HANDLE alb;

    if (scan_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    scan = (ALBUM_SCAN_CTX_T *)scan_handle;
    if (!scan->active) {
        return OPRT_COM_ERROR;
    }

    scan->active = FALSE;
    alb = scan->album_handle;

    if (scan->sorted_indices != NULL) {
        Free(scan->sorted_indices);
        scan->sorted_indices = NULL;
    }

    Free(scan->entries);
    scan->entries = NULL;
    scan->item_count = 0U;

    image_album_release_locked(alb);
    Free(scan);

    return OPRT_OK;
}

/**
 * @brief Seek the scan iterator to an absolute position
 * @param[in] scan_handle Scan handle from @ref image_album_scan_init
 * @param[in] pos         1-based target position (1 = first item, 0 = reset to before first)
 * @return OPRT_OK on success
 */
OPERATE_RET image_album_scan_seek(IMAGE_ALBUM_SCAN_HANDLE scan_handle, uint32_t pos)
{
    ALBUM_SCAN_CTX_T *scan;

    if (scan_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    scan = (ALBUM_SCAN_CTX_T *)scan_handle;

    if (!scan->active) {
        return OPRT_COM_ERROR;
    }

    if (!scan->sort_applied && scan->sort_opt.key != IMAGE_ALBUM_SORT_NONE) {
        __sort_indices(scan);
        scan->sort_applied = TRUE;
    }

    scan->index = (int32_t)pos;
    PR_DEBUG("scan_seek: pos=%d, index=%d, sorted_count=%d", pos, scan->index, scan->sorted_count);
    return OPRT_OK;
}
