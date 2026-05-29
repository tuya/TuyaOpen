/**
 * @file example_album.c
 * @brief Album management: save, trim, and browse photos using scan iterator
 * @version 0.3
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "example_album.h"
#include "image_album_scan.h"
#include "image_album_thumb.h"
#include "tal_api.h"

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static IMAGE_ALBUM_HANDLE      s_album_hdl    = NULL;
static IMAGE_ALBUM_SCAN_HANDLE s_scan_hdl     = NULL;
static uint32_t                s_browse_total  = 0;
static int32_t                 s_browse_index  = -1;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */

/**
 * @brief Initialize image album with memory storage
 * @return OPRT_OK on success
 */
OPERATE_RET example_album_init(void)
{
    IMAGE_ALBUM_INIT_CFG_T cfg = {
        .storage_mask = IMAGE_ALBUM_STORAGE_TP_ALL,

#if defined(ENABLE_IMAGE_ALBUM_STORAGE_SD) && (ENABLE_IMAGE_ALBUM_STORAGE_SD)
        .recover_tp   = IMAGE_ALBUM_STORAGE_TP_SD,
#else 
        .recover_tp   = 0,      
#endif
    };

    return image_album_init(EXAMPLE_ALBUM_NAME, &cfg, &s_album_hdl);
}

/**
 * @brief Delete oldest images when album exceeds max count
 * @return none
 */
static void __trim_oldest(void)
{
    UINT32_T count = 0;

    if (OPRT_OK != image_album_get_committed_count(s_album_hdl, &count)) {
        return;
    }

    while (count > EXAMPLE_ALBUM_MAX_PHOTOS) {
        ALBUM_IMAGE_ITEM_T oldest;
        memset(&oldest, 0, sizeof(ALBUM_IMAGE_ITEM_T));
        if (OPRT_OK != image_album_get_next_item(s_album_hdl, NULL,
                                                  IMAGE_ALBUM_STORAGE_TP_MEMORY, &oldest)) {
            break;
        }
        image_album_delete(s_album_hdl, oldest.filename);
        count--;
    }
}

/**
 * @brief Save a JPEG photo to the album, auto-trim oldest if full
 * @param[in] jpeg_data JPEG file bytes
 * @param[in] jpeg_len JPEG data length
 * @return OPRT_OK on success
 */
OPERATE_RET example_album_save_photo(uint8_t *jpeg_data, uint32_t jpeg_len)
{
    OPERATE_RET rt = OPRT_OK;

    if (jpeg_data == NULL || jpeg_len == 0 || s_album_hdl == NULL) {
        return OPRT_INVALID_PARM;
    }

    char filename[ALBUM_FILENAME_MAX_LEN] = {0};
    SYS_TICK_T ts = tal_time_get_posix_ms();
    snprintf(filename, sizeof(filename), "pic_%llu.jpeg", ts);

    ALBUM_IMAGE_SAVE_INFO_T info = {
        .filename  = filename,
        .format    = ALBUM_IMAGE_FORMAT_JPEG,
        .file_data = jpeg_data,
        .file_size = jpeg_len,
        .timestamp = ts,
    };

    rt = image_album_save(s_album_hdl, &info);
    if (rt != OPRT_OK) {
        PR_ERR("album save failed, rt=%d", rt);
        return rt;
    }

    __trim_oldest();

    PR_NOTICE("photo saved: %s (%u bytes)", filename, jpeg_len);

    return OPRT_OK;
}

/**
 * @brief Get current photo count in album
 * @return count Photo count
 */
uint32_t example_album_get_count(void)
{
    uint32_t count = 0;
    OPERATE_RET rt = OPRT_OK;

    rt = image_album_get_committed_count(s_album_hdl, &count);
    if(rt != OPRT_OK) {
        PR_ERR("get commit count :%d", rt);
        return 0;
    }

    return count;
}

/**
 * @brief Re-init scan iterator from the beginning
 * @return OPRT_OK on success
 */
static OPERATE_RET __scan_reinit(void)
{
    IMAGE_ALBUM_STORAGE_TP_E storage_tp = IMAGE_ALBUM_STORAGE_TP_MEMORY;

    if (s_scan_hdl) {
        image_album_scan_deinit(s_scan_hdl);
        s_scan_hdl = NULL;
    }

#if defined(ENABLE_IMAGE_ALBUM_STORAGE_SD) && (ENABLE_IMAGE_ALBUM_STORAGE_SD)
    storage_tp = IMAGE_ALBUM_STORAGE_TP_SD;
#endif

    return image_album_scan_init(EXAMPLE_ALBUM_NAME, storage_tp, &s_scan_hdl);
}

/**
 * @brief Fill photo output from scan item and read JPEG data
 * @param[in] item Scan item with filename and attributes
 * @param[out] photo Photo metadata
 * @param[out] jpeg_data Receives Malloc'd JPEG buffer
 * @param[out] jpeg_len JPEG data length
 * @return OPRT_OK on success
 */
static OPERATE_RET __browse_read_item(const ALBUM_IMAGE_ITEM_T *item,
                                      EXAMPLE_ALBUM_PHOTO_T *photo,
                                      uint8_t **jpeg_data, size_t *jpeg_len)
{
    IMAGE_ALBUM_STORAGE_TP_E storage_tp = 0;

    memset(photo, 0, sizeof(EXAMPLE_ALBUM_PHOTO_T));
    strncpy(photo->name, item->filename, ALBUM_FILENAME_MAX_LEN);
    photo->name[ALBUM_FILENAME_MAX_LEN] = '\0';
    photo->width     = item->attr.width;
    photo->height    = item->attr.height;
    photo->file_size = item->attr.file_size;
    photo->index     = (uint32_t)s_browse_index + 1;
    photo->total     = s_browse_total;

#if defined(ENABLE_IMAGE_ALBUM_STORAGE_SD) && (ENABLE_IMAGE_ALBUM_STORAGE_SD)
    storage_tp = IMAGE_ALBUM_STORAGE_TP_SD;
#endif

    return image_album_read(image_album_find_by_name(EXAMPLE_ALBUM_NAME),
                            item->filename, storage_tp, jpeg_data, jpeg_len);
}

/**
 * @brief Start album browse session using scan iterator
 * @return OPRT_OK on success, OPRT_NOT_FOUND if album is empty
 */
OPERATE_RET example_album_browse_start(void)
{
    OPERATE_RET rt = OPRT_OK;
    IMAGE_ALBUM_STORAGE_TP_E storage_tp = IMAGE_ALBUM_STORAGE_TP_MEMORY;

    s_browse_total = 0;
    s_browse_index = -1;

#if defined(ENABLE_IMAGE_ALBUM_STORAGE_SD) && (ENABLE_IMAGE_ALBUM_STORAGE_SD)
    storage_tp = IMAGE_ALBUM_STORAGE_TP_SD;
#endif

    rt = image_album_scan_init(EXAMPLE_ALBUM_NAME, storage_tp, &s_scan_hdl);
    if (rt != OPRT_OK) {
        return rt;
    }

    s_browse_total = image_album_scan_get_count(s_scan_hdl);
    PR_NOTICE("album img num:%d", s_browse_total);

    return OPRT_OK;
}

/**
 * @brief Get the next photo in browse session (wraps to first)
 * @param[out] photo Photo metadata
 * @param[out] jpeg_data Receives Malloc'd JPEG buffer (caller must call image_album_free_file_data)
 * @param[out] jpeg_len JPEG data length
 * @return OPRT_OK on success, OPRT_NOT_FOUND when album is empty
 */
OPERATE_RET example_album_browse_next(EXAMPLE_ALBUM_PHOTO_T *photo,
                                      uint8_t **jpeg_data, size_t *jpeg_len)
{
    if (photo == NULL || jpeg_data == NULL || jpeg_len == NULL || s_browse_total == 0) {
        return OPRT_INVALID_PARM;
    }

    ALBUM_IMAGE_ITEM_T item;
    OPERATE_RET rt = OPRT_OK;

    s_browse_index++;
    if ((uint32_t)s_browse_index >= s_browse_total) {
        s_browse_index = 0;
        rt = __scan_reinit();
        if (rt != OPRT_OK) {
            return rt;
        }
    }

    rt = image_album_scan_next(s_scan_hdl, &item);
    if (rt != OPRT_OK) {
        return rt;
    }

    return __browse_read_item(&item, photo, jpeg_data, jpeg_len);
}

/**
 * @brief Get the previous photo in browse session (wraps to last)
 * @param[out] photo Photo metadata
 * @param[out] jpeg_data Receives Malloc'd JPEG buffer (caller must call image_album_free_file_data)
 * @param[out] jpeg_len JPEG data length
 * @return OPRT_OK on success, OPRT_NOT_FOUND when album is empty
 */
OPERATE_RET example_album_browse_prev(EXAMPLE_ALBUM_PHOTO_T *photo,
                                      uint8_t **jpeg_data, size_t *jpeg_len)
{
    if (photo == NULL || jpeg_data == NULL || jpeg_len == NULL || s_browse_total == 0) {
        return OPRT_INVALID_PARM;
    }

    ALBUM_IMAGE_ITEM_T item;
    OPERATE_RET rt = OPRT_OK;

    s_browse_index--;
    if (s_browse_index < 0) {
        s_browse_index = (int32_t)s_browse_total - 1;
        rt = __scan_reinit();
        if (rt != OPRT_OK) {
            return rt;
        }
        for (uint32_t i = 0; i < s_browse_total; i++) {
            rt = image_album_scan_next(s_scan_hdl, &item);
            if (rt != OPRT_OK) {
                return rt;
            }
        }
        return __browse_read_item(&item, photo, jpeg_data, jpeg_len);
    }

    rt = image_album_scan_prev(s_scan_hdl, &item);
    if (rt != OPRT_OK) {
        return rt;
    }

    return __browse_read_item(&item, photo, jpeg_data, jpeg_len);
}

void example_album_release_jpeg_data(uint8_t *jpeg_data)
{
    image_album_free_file_data(jpeg_data);
}

OPERATE_RET example_album_delete_current(const char *filename)
{
    if (s_album_hdl == NULL || filename == NULL || s_scan_hdl == NULL) {
        return OPRT_INVALID_PARM;
    }

    uint32_t deleted_index = (uint32_t)s_browse_index;

    /* Release scan lock before deleting */
    image_album_scan_deinit(s_scan_hdl);
    s_scan_hdl = NULL;

    OPERATE_RET rt = image_album_delete(s_album_hdl, filename);
    if (rt != OPRT_OK) {
        PR_ERR("delete %s failed: %d", filename, rt);
    }

    s_browse_total = (s_browse_total > 0) ? s_browse_total - 1 : 0;

    if (s_browse_total == 0) {
        s_browse_index = -1;
        return rt;
    }

    rt = __scan_reinit();
    if (rt != OPRT_OK) {
        return rt;
    }

    /* Seek to the position we want to show: same index, or last if we deleted the last */
    uint32_t seek_to = (deleted_index >= s_browse_total) ? s_browse_total - 1 : deleted_index;

    ALBUM_IMAGE_ITEM_T tmp;
    for (uint32_t i = 0; i < seek_to; i++) {
        image_album_scan_next(s_scan_hdl, &tmp);
    }
    s_browse_index = (int32_t)seek_to - 1;

    return OPRT_OK;
}

/**
 * @brief End album browse session, release scan iterator
 * @return none
 */
void example_album_browse_end(void)
{
    if (s_scan_hdl) {
        image_album_scan_deinit(s_scan_hdl);
        s_scan_hdl = NULL;
    }
    s_browse_total = 0;
    s_browse_index = -1;
}

OPERATE_RET example_album_browse_seek(uint32_t idx)
{
    if (s_scan_hdl == NULL) {
        return OPRT_INVALID_PARM;
    }
    /* Set index so that after the ++ in browse_next, s_browse_index == idx */
    s_browse_index = (int32_t)idx - 1;
    return image_album_scan_seek(s_scan_hdl, idx);
}

OPERATE_RET example_album_thumb_start(ALBUM_THUMB_ITER_HANDLE *iter)
{
    IMAGE_ALBUM_STORAGE_TP_E storage_tp = IMAGE_ALBUM_STORAGE_TP_MEMORY;

#if defined(ENABLE_IMAGE_ALBUM_STORAGE_SD) && (ENABLE_IMAGE_ALBUM_STORAGE_SD)
    storage_tp = IMAGE_ALBUM_STORAGE_TP_SD;
#endif

    return image_album_thumb_iter_init(EXAMPLE_ALBUM_NAME, storage_tp, NULL, iter);
}

void example_album_thumb_end(ALBUM_THUMB_ITER_HANDLE iter)
{
    if (iter != NULL) {
        image_album_thumb_iter_deinit(iter);
    }
}
