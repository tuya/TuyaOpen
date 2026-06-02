/**
 * @file image_album.c
 * @brief Image album management main API implementation
 *
 * This file implements the core album APIs including initialization, picture save,
 * read, and deinitialization. Storage operations are
 * delegated to the storage dispatch module (album_storage_*). Scan operations are
 * delegated to the scan module (album_scan_*).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "image_album_priv.h"
#include "tal_api.h"
#include "tal_image_jpeg_codec.h"
#include "tuya_cloud_types.h"
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/



/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Image list node (filename/path live in node; metrics in @ref ALBUM_IMAGE_ATTR_T)
 */
typedef struct {
    struct tuya_list_head   node;
    char                    filename[ALBUM_FILENAME_MAX_LEN + 1];
    void                   *path_slot[ALBUM_STORAGE_ENTRY_MAX_COUNT];
    ALBUM_IMAGE_ATTR_T      attr;
    uint32_t                use_refcnt;
    bool_t                  pending_delete;
} ALBUM_IMAGE_NODE_T;

/**
 * @brief Album manager context (handle backing @ref IMAGE_ALBUM_HANDLE)
 */
typedef struct {
    struct tuya_list_head   node;
    char                    name[ALBUM_NAME_MAX_LEN + 1];
    MUTEX_HANDLE            mutex;
    struct tuya_list_head   image_list;
    void                   *storage_handle;
    uint32_t                use_refcnt;
} IMAGE_ALBUM_NODE_T;


/***********************************************************
***********************variable define**********************
***********************************************************/
static const uint8_t s_album_png_sig[ALBUM_PNG_SIGNATURE_LEN] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
};

static struct tuya_list_head sg_album_list = LIST_HEAD_INIT(sg_album_list);

/***********************************************************
***********************function define**********************
***********************************************************/
static IMAGE_ALBUM_NODE_T *__find_album_node(const char *name)
{
    struct tuya_list_head *pos;
    tuya_list_for_each(pos, &sg_album_list) {
        IMAGE_ALBUM_NODE_T *node = tuya_list_entry(pos, IMAGE_ALBUM_NODE_T, node);
        if (strncmp(node->name, name, ALBUM_NAME_MAX_LEN) == 0) {
            return node;
        }
    }
    return NULL;
}

static ALBUM_IMAGE_NODE_T *__find_image_node(struct tuya_list_head *image_list, const char *filename)
{
    struct tuya_list_head *pos;
    tuya_list_for_each(pos, image_list) {
        ALBUM_IMAGE_NODE_T *node = tuya_list_entry(pos, ALBUM_IMAGE_NODE_T, node);
        if (strncmp(node->filename, filename, ALBUM_FILENAME_MAX_LEN) == 0) {
            return node;
        }
    }
    return NULL;
}

/**
 * @brief True if any storage slot holds this image
 * @param[in] img Image node
 * @return BOOL_T
 */
static BOOL_T __album_node_has_storage(const ALBUM_IMAGE_NODE_T *img)
{
    uint32_t i;

    if (img == NULL) {
        return FALSE;
    }
    for (i = 0; i < ALBUM_STORAGE_ENTRY_MAX_COUNT; i++) {
        if (img->path_slot[i] != NULL) {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief Committed entry visible to read/iterate (excludes deferred-delete)
 * @param[in] img Image node
 * @return BOOL_T
 */
static BOOL_T __album_node_visible_committed(const ALBUM_IMAGE_NODE_T *img)
{
    if (img == NULL || img->pending_delete) {
        return FALSE;
    }
    return __album_node_has_storage(img);
}

/**
 * @brief Validate filename key rules shared by album APIs
 * @param[in] filename Picture key
 * @return OPRT_OK or OPRT_INVALID_PARM
 */
static OPERATE_RET __validate_filename_key(const char *filename)
{
    if (filename == NULL || strlen(filename) == 0 || strlen(filename) >= ALBUM_FILENAME_MAX_LEN) {
        return OPRT_INVALID_PARM;
    }
    return OPRT_OK;
}

/**
 * @brief Validate @ref ALBUM_IMAGE_SAVE_INFO_T for save
 * @param[in] info Save metadata
 * @param[in] require_file_payload If TRUE, require non-NULL file_data
 * @return OPRT_OK or error
 */
static OPERATE_RET __validate_save_info(const ALBUM_IMAGE_SAVE_INFO_T *info, BOOL_T require_file_payload)
{
    OPERATE_RET rt;

    if (info == NULL || info->filename == NULL || info->file_size == 0) {
        return OPRT_INVALID_PARM;
    }
    if (require_file_payload && info->file_data == NULL) {
        return OPRT_INVALID_PARM;
    }
    rt = __validate_filename_key(info->filename);
    if (rt != OPRT_OK) {
        return rt;
    }
    if (info->format >= ALBUM_IMAGE_FORMAT_MAX) {
        return OPRT_INVALID_PARM;
    }
    return OPRT_OK;
}

/**
 * @brief Apply metadata from save info to an image node
 * @param[in,out] img Target node
 * @param[in] info Source metadata
 * @return none
 */
static void __album_apply_save_meta(ALBUM_IMAGE_NODE_T *img, const ALBUM_IMAGE_SAVE_INFO_T *info)
{
    img->attr.format = info->format;
    img->attr.file_size = info->file_size;
    img->attr.seq = info->seq;
    img->attr.timestamp = info->timestamp;
}

/**
 * @brief Extract PNG width/height from IHDR
 * @param[in] data Encoded bytes (at least IHDR prefix)
 * @param[in] size Length of @a data
 * @param[out] width Parsed width
 * @param[out] height Parsed height
 * @return none
 */
static void __album_parse_png_resolution(const uint8_t *data, size_t size,
                                         uint32_t *width, uint32_t *height)
{
    *width = 0;
    *height = 0;

    if (data == NULL || size < ALBUM_PNG_IHDR_MIN_LEN) {
        return;
    }

    if (memcmp(data, s_album_png_sig, ALBUM_PNG_SIGNATURE_LEN) != 0) {
        return;
    }

    *width  = ((uint32_t)data[16] << 24) | ((uint32_t)data[17] << 16) |
              ((uint32_t)data[18] << 8)  | (uint32_t)data[19];
    *height = ((uint32_t)data[20] << 24) | ((uint32_t)data[21] << 16) |
              ((uint32_t)data[22] << 8)  | (uint32_t)data[23];
}

/**
 * @brief Parse width/height from in-memory JPEG/PNG payload before persistence
 * @param[in] data Picture file bytes
 * @param[in] size Byte length of @a data
 * @param[in] format Declared format
 * @param[out] width Output width (0 if not parsed)
 * @param[out] height Output height (0 if not parsed)
 * @return none
 */
static void __album_parse_image_resolution_from_payload(const uint8_t *data, size_t size,
                                                        ALBUM_IMAGE_FORMAT_E format,
                                                        uint32_t *width, uint32_t *height)
{
    uint32_t jpeg_len;

    *width = 0;
    *height = 0;

    if (data == NULL || size == 0) {
        return;
    }

    if (format == ALBUM_IMAGE_FORMAT_JPEG) {
        TAL_IMAGE_JPEG_INFO_T info;
        memset(&info, 0, sizeof(info));
        jpeg_len = (uint32_t)size;
        // Do not truncate jpeg_len to TAL_IMAGE_JPEG_MIN_READ_SIZE. 
        // The decoder needs enough data to find the SOF marker to get width/height.
        if (tal_image_jpeg_get_info(data, jpeg_len, &info) == OPRT_OK) {
            *width = info.width;
            *height = info.height;
        }
    } else if (format == ALBUM_IMAGE_FORMAT_PNG) {
        __album_parse_png_resolution(data, size, width, height);
    }
}

/**
 * @brief Append a new image node for save (caller holds album mutex)
 * @param[in] album Album
 * @param[in] filename Key
 * @param[out] out_img New list node on success
 * @return OPRT_OK or error (no mutex change here)
 * @return OPRT_COM_ERROR if the filename already exists
 */
static OPERATE_RET __album_resolve_image_node_for_write(IMAGE_ALBUM_NODE_T *album,
                                                        const char *filename,
                                                        ALBUM_IMAGE_NODE_T **out_img)
{
    ALBUM_IMAGE_NODE_T *img;

    img = __find_image_node(&album->image_list, filename);

    if (img != NULL) {
        PR_ERR("save rejected: duplicate filename");
        return OPRT_COM_ERROR;
    }

    img = (ALBUM_IMAGE_NODE_T *)Malloc(sizeof(ALBUM_IMAGE_NODE_T));
    if (img == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(img, 0, sizeof(ALBUM_IMAGE_NODE_T));
    strncpy(img->filename, filename, ALBUM_FILENAME_MAX_LEN - 1);
    tuya_list_add_tail(&img->node, &album->image_list);

    *out_img = img;
    return OPRT_OK;
}

/**
 * @brief Remove backend object and free one image list node
 * @param[in] storage_handle Storage handle
 * @param[in] img Node to destroy
 * @return none
 */
static void __album_release_image_list_node(void *storage_handle, ALBUM_IMAGE_NODE_T *img)
{
    PR_NOTICE("release image list node, filename:%s", img->filename);
    album_storage_remove(storage_handle, (void **)img->path_slot);
    tuya_list_del(&img->node);
    Free(img);
}

/**
 * @brief Physically remove node if marked pending-delete and all locks are clear
 * @param[in] album Album (caller holds mutex)
 * @param[in] img Image node
 * @return none
 */
static void __album_flush_deferred_delete_if_ready(IMAGE_ALBUM_NODE_T *album, ALBUM_IMAGE_NODE_T *img)
{
    if (img == NULL || !img->pending_delete) {
        return;
    }
    if (album->use_refcnt > 0 || img->use_refcnt > 0) {
        return;
    }
    __album_release_image_list_node(album->storage_handle, img);
}

/**
 * @brief Run deferred delete for every node that is ready (caller holds mutex)
 * @param[in] album Album
 * @return none
 */
static void __album_flush_all_deferred_deletes(IMAGE_ALBUM_NODE_T *album)
{
    struct tuya_list_head *pos;
    struct tuya_list_head *tmp;
    ALBUM_IMAGE_NODE_T *img;

    tuya_list_for_each_safe(pos, tmp, &album->image_list) {
        img = tuya_list_entry(pos, ALBUM_IMAGE_NODE_T, node);
        __album_flush_deferred_delete_if_ready(album, img);
    }
}

/**
 * @brief Find first visible committed node with data on @a storage_tp after @a filename (mutex held)
 * @param[in] album Album
 * @param[in] filename @c NULL or empty: from list head; else next after this key (must match @a storage_tp)
 * @return Node or NULL
 */
static ALBUM_IMAGE_NODE_T *__album_find_next_item(IMAGE_ALBUM_NODE_T *album, const char *filename)
{
    struct tuya_list_head *pos;
    ALBUM_IMAGE_NODE_T *img;
    ALBUM_IMAGE_NODE_T *cur;

    if (filename == NULL || filename[0] == '\0') {
        tuya_list_for_each(pos, &album->image_list) {
            img = tuya_list_entry(pos, ALBUM_IMAGE_NODE_T, node);
            if (!__album_node_visible_committed(img)) {
                continue;
            }

            return img;
        }
        return NULL;
    }

    if (__validate_filename_key(filename) != OPRT_OK) {
        return NULL;
    }

    cur = __find_image_node(&album->image_list, filename);
    if (cur == NULL || !__album_node_visible_committed(cur)) {
        return NULL;
    }

    for (pos = cur->node.next; pos != &album->image_list; pos = pos->next) {
        img = tuya_list_entry(pos, ALBUM_IMAGE_NODE_T, node);
        if (!__album_node_visible_committed(img)) {
            continue;
        }

        return img;
    }

    return NULL;
}

/**
 * @brief Set @a item->path for @a storage_tp (mutex held)
 * @param[in] album Album
 * @param[in] img Image node
 * @param[in] storage_tp Non-zero backend selector
 * @param[in,out] item Item to fill
 * @return OPRT_OK or error from storage layer
 */
static OPERATE_RET __album_item_set_path_for_storage_tp(IMAGE_ALBUM_NODE_T *album,
                                                        ALBUM_IMAGE_NODE_T *img,
                                                        IMAGE_ALBUM_STORAGE_TP_E storage_tp,
                                                        ALBUM_IMAGE_ITEM_T *item)
{
    void *path = NULL;
    OPERATE_RET rt;

    rt = album_storage_get_path_for_type(album->storage_handle, (void **)img->path_slot, storage_tp, &path);
    if (rt != OPRT_OK) {
        return rt;
    }
    item->path = path;
    return OPRT_OK;
}

static OPERATE_RET __album_item_retain_locked(IMAGE_ALBUM_HANDLE album_handle,
                                              const char *filename,
                                              IMAGE_ALBUM_STORAGE_TP_E storage_tp,
                                              ALBUM_IMAGE_ITEM_T *item)
{
    IMAGE_ALBUM_NODE_T *album = (IMAGE_ALBUM_NODE_T *)album_handle;
    OPERATE_RET rt_path;

    if (album == NULL || item == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (storage_tp == 0) {
        return OPRT_INVALID_PARM;
    }

    ALBUM_IMAGE_NODE_T *img = __find_image_node(&album->image_list, filename);
    if (img == NULL || !__album_node_visible_committed(img)) {
        return OPRT_NOT_FOUND;
    }

    if (img->use_refcnt == UINT32_MAX) {
        return OPRT_EXCEED_UPPER_LIMIT;
    }

    rt_path = __album_item_set_path_for_storage_tp(album, img, storage_tp, item);
    if (rt_path != OPRT_OK) {
        return rt_path;
    }

    img->use_refcnt++;

    item->filename = img->filename;
    item->attr = img->attr;

    return OPRT_OK;
}

static OPERATE_RET __album_item_release_locked(IMAGE_ALBUM_HANDLE album_handle, const char *filename)
{
    IMAGE_ALBUM_NODE_T *album = (IMAGE_ALBUM_NODE_T *)album_handle;

    ALBUM_IMAGE_NODE_T *img = __find_image_node(&album->image_list, filename);

    if (img == NULL) {
        return OPRT_NOT_FOUND;
    }

    if (img->use_refcnt == 0) {
        return OPRT_OK;
    }

    img->use_refcnt--;
    __album_flush_deferred_delete_if_ready(album, img);

    return OPRT_OK;
}



/**
 * @brief Per-entry callback for storage recovery; creates or updates image nodes
 * @param[in] entry Recovered entry from storage backend
 * @param[in] user_data IMAGE_ALBUM_NODE_T pointer
 * @return OPRT_OK on success
 */
static OPERATE_RET __album_recover_entry_cb(const ALBUM_RECOVER_ENTRY_T *entry, void *user_data)
{
    IMAGE_ALBUM_NODE_T *album = (IMAGE_ALBUM_NODE_T *)user_data;

    if (entry == NULL || entry->filename == NULL || entry->file_size == 0) {
        return OPRT_OK;
    }

    ALBUM_IMAGE_NODE_T *img = __find_image_node(&album->image_list, entry->filename);
    if (img == NULL) {
        img = (ALBUM_IMAGE_NODE_T *)Malloc(sizeof(ALBUM_IMAGE_NODE_T));
        if (img == NULL) {
            return OPRT_MALLOC_FAILED;
        }
        memset(img, 0, sizeof(ALBUM_IMAGE_NODE_T));
        strncpy(img->filename, entry->filename, ALBUM_FILENAME_MAX_LEN - 1);
        tuya_list_add_tail(&img->node, &album->image_list);
    }

    if (entry->slot_index < ALBUM_STORAGE_ENTRY_MAX_COUNT) {
        img->path_slot[entry->slot_index] = entry->path;
    }

    img->attr.file_size = entry->file_size;

    return OPRT_OK;
}

/**
 * @brief Detect image format and parse resolution for recovered nodes
 * @param[in] album Album node (caller holds no mutex; called during init before publish)
 * @return none
 */
static void __album_recover_parse_attrs(IMAGE_ALBUM_NODE_T *album)
{
    struct tuya_list_head *pos;
    uint8_t hdr[ALBUM_PNG_IHDR_MIN_LEN];

    tuya_list_for_each(pos, &album->image_list) {
        ALBUM_IMAGE_NODE_T *img = tuya_list_entry(pos, ALBUM_IMAGE_NODE_T, node);
        size_t read_len = sizeof(hdr);

        if (img->attr.file_size < read_len) {
            read_len = img->attr.file_size;
        }
        if (read_len < 2) {
            continue;
        }

        memset(hdr, 0, sizeof(hdr));
        OPERATE_RET rt = album_storage_read(album->storage_handle, (void **)img->path_slot,
                                            read_len, hdr, 0);
        if (rt != OPRT_OK) {
            img->attr.format = ALBUM_IMAGE_FORMAT_JPEG;
            continue;
        }

        if (hdr[0] == 0xFF && hdr[1] == 0xD8) {
            img->attr.format = ALBUM_IMAGE_FORMAT_JPEG;
            /* JPEG SOF marker (contains width/height) can appear after variable-length
             * APP markers, so 24 bytes is not enough. Read the full file to locate it. */
            uint8_t *jpeg_buf = (uint8_t *)Malloc(img->attr.file_size);
            if (jpeg_buf != NULL) {
                if (album_storage_read(album->storage_handle, (void **)img->path_slot,
                                       img->attr.file_size, jpeg_buf, 0) == OPRT_OK) {
                    TAL_IMAGE_JPEG_INFO_T info;
                    memset(&info, 0, sizeof(info));
                    if (tal_image_jpeg_get_info(jpeg_buf, (uint32_t)img->attr.file_size,
                                                &info) == OPRT_OK) {
                        img->attr.width  = info.width;
                        img->attr.height = info.height;
                    }
                }
                Free(jpeg_buf);
            }
        } else if (read_len >= ALBUM_PNG_SIGNATURE_LEN &&
                   memcmp(hdr, s_album_png_sig, ALBUM_PNG_SIGNATURE_LEN) == 0) {
            img->attr.format = ALBUM_IMAGE_FORMAT_PNG;
            if (read_len >= ALBUM_PNG_IHDR_MIN_LEN) {
                __album_parse_png_resolution(hdr, read_len, &img->attr.width, &img->attr.height);
            }
        } else {
            img->attr.format = ALBUM_IMAGE_FORMAT_JPEG;
        }
    }
}

/**
 * @brief Sync recovered images to backends with empty path_slots
 * @param[in] album Album node (called during init, no mutex needed)
 * @return none
 * @note Reads full file from a backend with data, then saves to empty slots one image at a time.
 */
static void __album_recover_sync_backends(IMAGE_ALBUM_NODE_T *album)
{
    struct tuya_list_head *pos;

    tuya_list_for_each(pos, &album->image_list) {
        ALBUM_IMAGE_NODE_T *img = tuya_list_entry(pos, ALBUM_IMAGE_NODE_T, node);
        BOOL_T has_empty = FALSE;
        BOOL_T has_data = FALSE;
        uint32_t i;

        for (i = 0; i < ALBUM_STORAGE_ENTRY_MAX_COUNT; i++) {
            if (img->path_slot[i] != NULL) {
                has_data = TRUE;
            } else {
                has_empty = TRUE;
            }
        }

        if (!has_empty || !has_data) {
            continue;
        }

        uint8_t *buf = (uint8_t *)Malloc(img->attr.file_size);
        if (buf == NULL) {
            PR_WARN("sync skip %s: malloc %u failed", img->filename, (unsigned)img->attr.file_size);
            continue;
        }

        OPERATE_RET rt = album_storage_read(album->storage_handle, (void **)img->path_slot,
                                            img->attr.file_size, buf, 0);
        if (rt != OPRT_OK) {
            PR_WARN("sync skip %s: read failed rt=%d", img->filename, rt);
            Free(buf);
            continue;
        }

        album_storage_sync(album->storage_handle, img->filename, buf, img->attr.file_size,
                           (void **)img->path_slot);

        Free(buf);
    }
}

/**
 * @brief Initializes the album manager
 *
 * Allocates internal context, creates the mutex, and initializes the storage
 * layer which registers built-in backends based on the configured mode.
 * After storage init, recovers any previously persisted images from backends
 * that support recovery (e.g. SD card).
 *
 * @param[in] name Album name
 * @param[in] cfg Optional @ref IMAGE_ALBUM_INIT_CFG_T (NULL: all storage backends)
 * @param[out] album_handle Output handle to the album manager
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET image_album_init(char *name, const IMAGE_ALBUM_INIT_CFG_T *cfg, IMAGE_ALBUM_HANDLE *album_handle)
{
    if (name == NULL || album_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    IMAGE_ALBUM_NODE_T *album = __find_album_node(name);
    if(album != NULL) {
        PR_ERR("album already exists");
        return OPRT_COM_ERROR;
    }

    album = (IMAGE_ALBUM_NODE_T *)Malloc(sizeof(IMAGE_ALBUM_NODE_T));
    if (album == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(album, 0, sizeof(IMAGE_ALBUM_NODE_T));
    strncpy(album->name, name, ALBUM_NAME_MAX_LEN);
    IMAGE_ALBUM_STORAGE_TP_E storage_mask = IMAGE_ALBUM_STORAGE_TP_ALL;

    if (cfg != NULL && cfg->storage_mask != 0) {
        storage_mask = cfg->storage_mask;
    }
    INIT_LIST_HEAD(&album->image_list);
    
    OPERATE_RET rt = tal_mutex_create_init(&album->mutex);
    if (rt != OPRT_OK) {
        Free(album);
        return rt;
    }
    
    rt = album_storage_init(name, storage_mask, &album->storage_handle);
    if (rt != OPRT_OK) {
        tal_mutex_release(album->mutex);
        Free(album);
        return rt;
    }

    IMAGE_ALBUM_STORAGE_TP_E recover_tp = 0;
    if (cfg != NULL) {
        recover_tp = cfg->recover_tp;
    }

    if (recover_tp != 0) {
        album_storage_recover(album->storage_handle, recover_tp, __album_recover_entry_cb, album);
        __album_recover_parse_attrs(album);
        __album_recover_sync_backends(album);

        uint32_t recovered = 0;
        struct tuya_list_head *pos;
        tuya_list_for_each(pos, &album->image_list) {
            recovered++;
        }
        if (recovered > 0) {
            PR_NOTICE("album '%s' recovered %u images from storage", name, recovered);
        }
    }

    tuya_list_add_tail(&album->node, &sg_album_list);

    *album_handle = (IMAGE_ALBUM_HANDLE)album;

    return OPRT_OK;
}

/**
 * @brief Deinitializes the album manager and releases all resources
 *
 * Removes all pictures from every backend, deinitializes all backends, and releases the mutex.
 *
 * @param[in] album_handle Handle to the album manager
 * @return OPRT_OK on success, OPRT_INVALID_PARM if handle is NULL
 * @return OPRT_RESOURCE_NOT_READY while album or any picture retain count is non-zero
 */
OPERATE_RET image_album_deinit(IMAGE_ALBUM_HANDLE album_handle)
{
    if (album_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    IMAGE_ALBUM_NODE_T *album = (IMAGE_ALBUM_NODE_T *)album_handle;

    tal_mutex_lock(album->mutex);

    if (album->use_refcnt > 0) {
        tal_mutex_unlock(album->mutex);
        return OPRT_RESOURCE_NOT_READY;
    }

    struct tuya_list_head *pos;
    struct tuya_list_head *n;

    tuya_list_for_each(pos, &album->image_list) {
        ALBUM_IMAGE_NODE_T *img = tuya_list_entry(pos, ALBUM_IMAGE_NODE_T, node);
        if (img->use_refcnt > 0) {
            tal_mutex_unlock(album->mutex);
            return OPRT_RESOURCE_NOT_READY;
        }
    }

    tuya_list_for_each_safe(pos, n, &album->image_list) {
        ALBUM_IMAGE_NODE_T *img = tuya_list_entry(pos, ALBUM_IMAGE_NODE_T, node);
        __album_release_image_list_node(album->storage_handle, img);
    }

    tuya_list_del(&album->node);

    tal_mutex_unlock(album->mutex);
    tal_mutex_release(album->mutex);

    album_storage_deinit(album->storage_handle);

    Free(album);

    return OPRT_OK;
}

/**
 * @brief find album handle by registered name
 * @param[in] name Album name
 * @return Album handle, or NULL if @a name is NULL or no match
 */
 IMAGE_ALBUM_HANDLE image_album_find_by_name(const char *name)
 {
     if (name == NULL) {
         return NULL;
     }
     return (IMAGE_ALBUM_HANDLE)__find_album_node(name);
 }

/**
 * @brief Saves a picture file to the album with optional metadata
 *
 * Stores the picture through all registered backends. Duplicate @a filename returns an error.
 *
 * @param[in] album_handle Handle to the album manager
 * @param[in] info Picture save info
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET image_album_save(IMAGE_ALBUM_HANDLE album_handle, ALBUM_IMAGE_SAVE_INFO_T *info)
{
    OPERATE_RET rt;

    if (album_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __validate_save_info(info, TRUE);
    if (rt != OPRT_OK) {
        return rt;
    }

    IMAGE_ALBUM_NODE_T *album = (IMAGE_ALBUM_NODE_T *)album_handle;

    tal_mutex_lock(album->mutex);

    ALBUM_IMAGE_NODE_T *img = NULL;

    rt = __album_resolve_image_node_for_write(album, info->filename, &img);
    if (rt != OPRT_OK) {
        tal_mutex_unlock(album->mutex);
        return rt;
    }

    __album_apply_save_meta(img, info);
    __album_parse_image_resolution_from_payload(info->file_data, info->file_size, info->format,
                                                &img->attr.width, &img->attr.height);
    rt = album_storage_save(album->storage_handle, info->filename, info->file_data,
                            info->file_size, (void **)img->path_slot);
    if (rt != OPRT_OK) {
        tuya_list_del(&img->node);
        Free(img);
    }

    tal_mutex_unlock(album->mutex);
    return rt;
}

/**
 * @brief Reads a picture file from the album by filename
 *
 * Delegates to the storage layer; see @a storage_tp in @ref image_album_read() declaration.
 * Returns a newly allocated buffer. Caller must free via image_album_free_file_data().
 *
 * @param[in] album_handle Handle to the album manager
 * @param[in] filename Picture filename (e.g., "photo.jpg")
 * @param[in] storage_tp Storage channel selector (@c 0 = auto)
 * @param[out] file_data Pointer to receive allocated picture data buffer
 * @param[out] file_size Pointer to receive the size of file_data in bytes
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET image_album_read(IMAGE_ALBUM_HANDLE album_handle,
                             const char *filename,
                             IMAGE_ALBUM_STORAGE_TP_E storage_tp,
                             uint8_t **file_data,
                             size_t *file_size)
{
    OPERATE_RET rt;

    if (album_handle == NULL || file_data == NULL || file_size == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __validate_filename_key(filename);
    if (rt != OPRT_OK) {
        return rt;
    }

    IMAGE_ALBUM_NODE_T *album = (IMAGE_ALBUM_NODE_T *)album_handle;

    tal_mutex_lock(album->mutex);

    ALBUM_IMAGE_NODE_T *img = __find_image_node(&album->image_list, filename);

    if (img == NULL || !__album_node_visible_committed(img)) {
        tal_mutex_unlock(album->mutex);
        return OPRT_NOT_FOUND;
    }

    uint8_t *buf = (uint8_t *)Malloc(img->attr.file_size);
    if (buf == NULL) {
        tal_mutex_unlock(album->mutex);
        return OPRT_MALLOC_FAILED;
    }

    rt = album_storage_read(album->storage_handle, (void **)img->path_slot, img->attr.file_size, buf,
                            storage_tp);
    if (rt == OPRT_OK) {
        *file_data = buf;
        *file_size = img->attr.file_size;
    } else {
        Free(buf);
    }

    tal_mutex_unlock(album->mutex);
    return rt;
}

/**
 * @brief Frees a picture data buffer allocated by image_album_read()
 *
 * @param[in] file_data Pointer to the buffer to free
 * @return OPRT_OK on success, OPRT_INVALID_PARM if file_data is NULL
 */
 OPERATE_RET image_album_free_file_data(uint8_t *file_data)
 {
     if (file_data == NULL) {
         return OPRT_INVALID_PARM;
     }
     Free(file_data);
     return OPRT_OK;
 }

/**
 * @brief Delete a committed picture by filename
 *
 * @param[in] album_handle Album handle
 * @param[in] filename Picture key
 *
 * @return OPRT_OK on success, OPRT_NOT_FOUND if missing
 */
OPERATE_RET image_album_delete(IMAGE_ALBUM_HANDLE album_handle, const char *filename)
{
    OPERATE_RET rt;
    IMAGE_ALBUM_NODE_T *album;
    ALBUM_IMAGE_NODE_T *img;

    if (album_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __validate_filename_key(filename);
    if (rt != OPRT_OK) {
        return rt;
    }

    album = (IMAGE_ALBUM_NODE_T *)album_handle;

    tal_mutex_lock(album->mutex);

    img = __find_image_node(&album->image_list, filename);
    if (img == NULL || !__album_node_has_storage(img)) {
        PR_ERR("image not found or no storage, filename:%s", filename);
        tal_mutex_unlock(album->mutex);
        return OPRT_NOT_FOUND;
    }

    if (img->pending_delete) {
        PR_ERR("image is pending delete, filename:%s", filename);
        tal_mutex_unlock(album->mutex);
        return OPRT_OK;
    }

    if (album->use_refcnt > 0U || img->use_refcnt > 0U) {
        PR_ERR("album or image is retained, filename:%s", filename);
        img->pending_delete = TRUE;
        tal_mutex_unlock(album->mutex);
        return OPRT_OK;
    }

    __album_release_image_list_node(album->storage_handle, img);

    tal_mutex_unlock(album->mutex);
    return OPRT_OK;
}


/**
 * @brief Get first or next item metadata for @a storage_tp (insert order)
 * @param[in] album_handle Album handle
 * @param[in] filename Current key, or NULL / empty for first match
 * @param[in] storage_tp Non-zero backend selector
 * @param[out] item Output descriptor
 * @return OPRT_OK or error
 */
OPERATE_RET image_album_get_next_item(IMAGE_ALBUM_HANDLE album_handle,
                                      const char *filename,
                                      IMAGE_ALBUM_STORAGE_TP_E storage_tp,
                                      ALBUM_IMAGE_ITEM_T *item)
{
    IMAGE_ALBUM_NODE_T *album;
    ALBUM_IMAGE_NODE_T *img;
    OPERATE_RET rt;

    if (album_handle == NULL || item == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (storage_tp == 0) {
        return OPRT_INVALID_PARM;
    }

    album = (IMAGE_ALBUM_NODE_T *)album_handle;
    memset(item, 0, sizeof(ALBUM_IMAGE_ITEM_T));

    tal_mutex_lock(album->mutex);
    img = __album_find_next_item(album, filename);
    if (img == NULL) {
        tal_mutex_unlock(album->mutex);
        return OPRT_NOT_FOUND;
    }

    item->filename = img->filename;
    item->attr = img->attr;
    rt = __album_item_set_path_for_storage_tp(album, img, storage_tp, item);
    if (rt != OPRT_OK) {
        tal_mutex_unlock(album->mutex);
        return rt;
    }

    tal_mutex_unlock(album->mutex);

    return OPRT_OK;
}

/**
 * @brief Count visible committed images (insert order, all backends)
 * @param[in] album_handle Album handle
 * @param[out] count Output count
 * @return OPRT_OK or OPRT_INVALID_PARM
 */
OPERATE_RET image_album_get_committed_count(IMAGE_ALBUM_HANDLE album_handle, uint32_t *count)
{
    IMAGE_ALBUM_NODE_T *album;
    struct tuya_list_head *pos;
    uint32_t n = 0U;

    if (album_handle == NULL || count == NULL) {
        return OPRT_INVALID_PARM;
    }

    album = (IMAGE_ALBUM_NODE_T *)album_handle;
    tal_mutex_lock(album->mutex);
    tuya_list_for_each(pos, &album->image_list) {
        ALBUM_IMAGE_NODE_T *img = tuya_list_entry(pos, ALBUM_IMAGE_NODE_T, node);
        if (__album_node_visible_committed(img)) {
            n++;
        }
    }
    tal_mutex_unlock(album->mutex);
    *count = n;
    return OPRT_OK;
}

/**
 * @brief Increment album-wide refcount (takes album mutex internally)
 * @param[in] album_handle Album handle
 * @return OPRT_OK on success
 */
OPERATE_RET image_album_retain_locked(IMAGE_ALBUM_HANDLE album_handle)
{
    IMAGE_ALBUM_NODE_T *album;
    OPERATE_RET rt = OPRT_OK;

    if (album_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    album = (IMAGE_ALBUM_NODE_T *)album_handle;

    tal_mutex_lock(album->mutex);

    if (album->use_refcnt == UINT32_MAX) {
        rt = OPRT_EXCEED_UPPER_LIMIT;
    } else {
        album->use_refcnt++;
    }

    tal_mutex_unlock(album->mutex);
    return rt;
}

/**
 * @brief Decrement album-wide refcount (takes album mutex internally)
 * @param[in] album_handle Album handle
 * @return OPRT_OK on success
 */
OPERATE_RET image_album_release_locked(IMAGE_ALBUM_HANDLE album_handle)
{
    IMAGE_ALBUM_NODE_T *album;
    OPERATE_RET rt = OPRT_OK;

    if (album_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    album = (IMAGE_ALBUM_NODE_T *)album_handle;

    tal_mutex_lock(album->mutex);

    if (album->use_refcnt == 0) {
        rt = OPRT_INVALID_PARM;
    } else {
        album->use_refcnt--;
        if (album->use_refcnt == 0U) {
            __album_flush_all_deferred_deletes(album);
        }
    }

    tal_mutex_unlock(album->mutex);
    return rt;
}


/**
 * @brief Retain picture by filename (usage refcount)
 *
 * Increments an internal per-file reference count so the album defers trimming/removing
 * this entry until the last matching @ref image_album_file_release().
 * Callers identify pictures by @a filename only (no node handle).
 *
 * @param[in] album_handle Album handle
 * @param[in] filename Picture key (same rules as save/read)
 * @param[in] storage_tp Non-zero backend selector for @a item->path
 * @param[out] item Output descriptor
 *
 * @return OPRT_OK on success, OPRT_NOT_FOUND if missing or no path on @a storage_tp
 * @return OPRT_INVALID_PARM if @a storage_tp is zero
 * @note @a item->filename points at internal storage; duplicate filename still yields @c OPRT_COM_ERROR
 *       from save. @ref image_album_deinit() returns @c OPRT_RESOURCE_NOT_READY until all item
 *       releases (and album retain, if any) allow teardown.
 */
OPERATE_RET image_album_item_retain_locked(IMAGE_ALBUM_HANDLE album_handle,
                                           const char *filename,
                                           IMAGE_ALBUM_STORAGE_TP_E storage_tp,
                                           ALBUM_IMAGE_ITEM_T *item)
{
    OPERATE_RET rt;

    if (album_handle == NULL || item == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (storage_tp == 0) {
        return OPRT_INVALID_PARM;
    }

    rt = __validate_filename_key(filename);
    if (rt != OPRT_OK) {
        return rt;
    }

    IMAGE_ALBUM_NODE_T *album = (IMAGE_ALBUM_NODE_T *)album_handle;

    tal_mutex_lock(album->mutex);
    rt = __album_item_retain_locked(album_handle, filename, storage_tp, item);
    tal_mutex_unlock(album->mutex);

    return rt;
}

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
OPERATE_RET image_album_item_release_locked(IMAGE_ALBUM_HANDLE album_handle, const char *filename)
{
    OPERATE_RET rt;

    if (album_handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    rt = __validate_filename_key(filename);
    if (rt != OPRT_OK) {
        return rt;
    }

    IMAGE_ALBUM_NODE_T *album = (IMAGE_ALBUM_NODE_T *)album_handle;

    tal_mutex_lock(album->mutex);
    rt = __album_item_release_locked(album_handle, filename);
    tal_mutex_unlock(album->mutex);
    return rt;
}