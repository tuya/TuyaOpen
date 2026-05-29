/**
 * @file ai_ui_image_album.c
 * @brief AI UI image album — bridges image_album with platform UI callbacks
 *        for image browsing, thumbnail views, and picture selection.
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include "tal_api.h"

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
#include "image_album.h"
#include "image_album_scan.h"
#include "image_album_thumb.h"
#include "ai_ui_image_album.h"

/***********************************************************
************************macro define************************
***********************************************************/
#if defined(ENABLE_IMAGE_ALBUM_STORAGE_MEM) && (ENABLE_IMAGE_ALBUM_STORAGE_MEM)
#define AI_UI_ALBUM_STORAGE_TP   IMAGE_ALBUM_STORAGE_TP_MEMORY
#elif defined(ENABLE_IMAGE_ALBUM_STORAGE_SD) && (ENABLE_IMAGE_ALBUM_STORAGE_SD)
#define AI_UI_ALBUM_STORAGE_TP   IMAGE_ALBUM_STORAGE_TP_SD
#else
#define AI_UI_ALBUM_STORAGE_TP   IMAGE_ALBUM_STORAGE_TP_CUSTOM
#endif

#if defined(COMP_AI_PICTURE_ALBUM_NAME)
#define AI_UI_IMAG_ALBUUM_NAME COMP_AI_PICTURE_ALBUM_NAME
#else 
#define AI_UI_IMAG_ALBUUM_NAME "ai_image_album"
#endif

#define AI_UI_ALBUM_THUMB_W      80
#define AI_UI_ALBUM_THUMB_H      80
#define AI_UI_ALBUM_SELECT_MAX   3

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char                    album_name[ALBUM_NAME_MAX_LEN + 1];
    IMAGE_ALBUM_HANDLE      album_hdl;
    IMAGE_ALBUM_SCAN_HANDLE scan_hdl;
    uint32_t                img_count;

    ALBUM_THUMB_ITER_HANDLE thumb_iter;
    ALBUM_THUMB_BATCH_T     thumb_batch;
    AI_UI_IMG_T            *thumb_arr;
    uint32_t                thumb_arr_cnt;
} AI_UI_ALBUM_CTX_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_UI_ALBUM_INTFS_T sg_album_intfs;
static AI_UI_ALBUM_CTX_T   sg_ctx;

/***********************************************************
***********************function define**********************
***********************************************************/

static IMAGE_ALBUM_HANDLE __get_album_hdl(void)
{
    return sg_ctx.album_hdl;
}

/**
 * @brief Ensure album name and handle are initialized.
 *        Called by pages that can be opened independently of the album view page
 *        (e.g. select page from chat popup), where ai_ui_image_album_open() may
 *        not have been called yet or was previously cleared by ai_ui_image_album_close().
 */
static void __ensure_album_ctx(void)
{
    if (sg_ctx.album_name[0] != '\0' && sg_ctx.album_hdl != NULL) {
        return;
    }
    sg_ctx.album_hdl = image_album_find_by_name(AI_UI_IMAG_ALBUUM_NAME);
    if (sg_ctx.album_hdl != NULL) {
        strncpy(sg_ctx.album_name, AI_UI_IMAG_ALBUUM_NAME, ALBUM_NAME_MAX_LEN);
        sg_ctx.album_name[ALBUM_NAME_MAX_LEN] = '\0';
    }
}

/**
 * @brief Read image data from album and call disp_image callback.
 */
static void __disp_image_by_name(IMAGE_ALBUM_HANDLE album_hdl, const char *filename)
{
    uint8_t *file_data = NULL;
    size_t   file_size = 0;

    OPERATE_RET rt = image_album_read(album_hdl, filename,
                                      AI_UI_ALBUM_STORAGE_TP,
                                      &file_data, &file_size);
    if (rt != OPRT_OK || file_data == NULL || file_size == 0) {
        PR_ERR("read image %s failed, rt:%d", filename, rt);
        return;
    }

    if (sg_album_intfs.disp_image) {
        AI_UI_IMG_T img = {0};
        img.name = (char *)filename;
        img.data = file_data;
        img.len  = (uint32_t)file_size;
        sg_album_intfs.disp_image(&img);
    }

    image_album_free_file_data(file_data);
}

/**
 * @brief Build AI_UI_IMG_T array from a thumb batch.
 *        Item pointers reference the batch's buffers (no copy).
 */
static void __build_thumb_item_arr(ALBUM_THUMB_BATCH_T *batch,
                                   AI_UI_IMG_T **out_arr,
                                   uint32_t *out_cnt)
{
    uint32_t i;

    *out_arr = NULL;
    *out_cnt = 0;

    if (batch == NULL || batch->count == 0) {
        return;
    }

    AI_UI_IMG_T *arr = (AI_UI_IMG_T *)tal_malloc(sizeof(AI_UI_IMG_T) * batch->count);
    if (arr == NULL) {
        return;
    }
    memset(arr, 0, sizeof(AI_UI_IMG_T) * batch->count);

    for (i = 0; i < batch->count; i++) {
        arr[i].name   = batch->items[i].filename;
        arr[i].width  = batch->items[i].thumb.width;
        arr[i].height = batch->items[i].thumb.height;
        arr[i].data   = batch->items[i].thumb.buf;
        arr[i].len    = batch->items[i].thumb.size;
    }

    *out_arr = arr;
    *out_cnt = batch->count;
}

/**
 * @brief Free thumbnail resources stored in the context.
 */
static void __free_thumb_resources(void)
{
    if (sg_ctx.thumb_arr) {
        tal_free(sg_ctx.thumb_arr);
        sg_ctx.thumb_arr = NULL;
        sg_ctx.thumb_arr_cnt = 0;
    }
    if (sg_ctx.thumb_batch.count > 0) {
        image_album_thumb_batch_free(&sg_ctx.thumb_batch);
        memset(&sg_ctx.thumb_batch, 0, sizeof(ALBUM_THUMB_BATCH_T));
    }
    if (sg_ctx.thumb_iter) {
        image_album_thumb_iter_deinit(sg_ctx.thumb_iter);
        sg_ctx.thumb_iter = NULL;
    }
}

/**
 * @brief Fetch all thumbnails from the album into a batch.
 *
 * @param[out] out_iter  thumb iterator handle (caller must deinit)
 * @param[out] out_batch filled batch (caller must free)
 * @return OPRT_OK on success
 */
static OPERATE_RET __get_thumb_batch(ALBUM_THUMB_ITER_HANDLE *out_iter,
                                     ALBUM_THUMB_BATCH_T *out_batch)
{
    OPERATE_RET rt;

    IMAGE_ALBUM_SORT_OPT_T sort_opt = {
        .key   = IMAGE_ALBUM_SORT_SAVE_SEQ,
        .order = IMAGE_ALBUM_SORT_DESC,
    };

    ALBUM_THUMB_ITER_HANDLE iter = NULL;
    rt = image_album_thumb_iter_init(sg_ctx.album_name,
                                     AI_UI_ALBUM_STORAGE_TP,
                                     &sort_opt, &iter);
    if (rt != OPRT_OK) {
        return rt;
    }

    uint32_t total = image_album_thumb_iter_count(iter);
    if (total == 0) {
        image_album_thumb_iter_deinit(iter);
        return OPRT_NOT_FOUND;
    }

    ALBUM_THUMB_CFG_T cfg = {
        .width  = AI_UI_ALBUM_THUMB_W,
        .height = AI_UI_ALBUM_THUMB_H,
        .fmt    = ALBUM_THUMB_FMT_RGB565,
        .fit    = ALBUM_THUMB_FIT_COVER,
    };

    memset(out_batch, 0, sizeof(ALBUM_THUMB_BATCH_T));
    rt = image_album_thumb_iter_next(iter, &cfg, total, out_batch);
    *out_iter = iter;

    return rt;
}

/**
 * @brief Register album display callbacks.
 */
OPERATE_RET ai_ui_image_album_register(AI_UI_ALBUM_INTFS_T *intfs)
{
    TUYA_CHECK_NULL_RETURN(intfs, OPRT_INVALID_PARM);

    memcpy(&sg_album_intfs, intfs, sizeof(AI_UI_ALBUM_INTFS_T));

    return OPRT_OK;
}

/**
 * @brief Open the specified album and display the first image.
 *
 * @param[in] image_name album name used to find the album handle
 */
void ai_ui_image_album_open(void)
{
    OPERATE_RET rt;

    /* Close any previous scan */
    if (sg_ctx.scan_hdl) {
        image_album_scan_deinit(sg_ctx.scan_hdl);
        sg_ctx.scan_hdl  = NULL;
        sg_ctx.img_count = 0;
    }

    sg_ctx.album_hdl = image_album_find_by_name(AI_UI_IMAG_ALBUUM_NAME);
    if (sg_ctx.album_hdl == NULL) {
        PR_WARN("album not found: %s", AI_UI_IMAG_ALBUUM_NAME);
        /* Still open the page to show empty state */
        if (sg_album_intfs.disp_open) {
            sg_album_intfs.disp_open();
        }
        return;
    }

    strncpy(sg_ctx.album_name, AI_UI_IMAG_ALBUUM_NAME, ALBUM_NAME_MAX_LEN);
    sg_ctx.album_name[ALBUM_NAME_MAX_LEN] = '\0';

    rt = image_album_scan_init(sg_ctx.album_name,
                               AI_UI_ALBUM_STORAGE_TP,
                               &sg_ctx.scan_hdl);
    if (rt != OPRT_OK) {
        PR_ERR("scan init failed, rt:%d", rt);
        if (sg_album_intfs.disp_open) {
            sg_album_intfs.disp_open();
        }
        return;
    }

    sg_ctx.img_count = image_album_scan_get_count(sg_ctx.scan_hdl);

    if (sg_album_intfs.disp_open) {
        sg_album_intfs.disp_open();
    }

    /* Seek to the last image (newest) and display it */
    if (sg_ctx.img_count > 0) {
        image_album_scan_seek(sg_ctx.scan_hdl, sg_ctx.img_count - 1);
        ALBUM_IMAGE_ITEM_T item;
        memset(&item, 0, sizeof(item));
        rt = image_album_scan_next(sg_ctx.scan_hdl, &item);
        if (rt == OPRT_OK) {
            __disp_image_by_name(sg_ctx.album_hdl, item.filename);
        }
    }
}

/**
 * @brief Display the next image in scan order.
 */
void ai_ui_image_album_view_next(void)
{
    if (sg_ctx.scan_hdl == NULL) {
        PR_ERR("album not open");
        return;
    }

    ALBUM_IMAGE_ITEM_T item;
    memset(&item, 0, sizeof(item));
    OPERATE_RET rt = image_album_scan_next(sg_ctx.scan_hdl, &item);
    if (rt != OPRT_OK) {
        PR_ERR("scan next failed, rt:%d", rt);
        return;
    }

    __disp_image_by_name(__get_album_hdl(), item.filename);
}

/**
 * @brief Display the previous image in scan order.
 */
void ai_ui_image_album_view_prev(void)
{
    if (sg_ctx.scan_hdl == NULL) {
        PR_ERR("album not open");
        return;
    }

    ALBUM_IMAGE_ITEM_T item;
    memset(&item, 0, sizeof(item));
    OPERATE_RET rt = image_album_scan_prev(sg_ctx.scan_hdl, &item);
    if (rt != OPRT_OK) {
        PR_ERR("scan prev failed, rt:%d", rt);
        return;
    }

    __disp_image_by_name(__get_album_hdl(), item.filename);
}

/**
 * @brief Open all-thumbnails view: allocate thumb list and display.
 */
void ai_ui_image_album_view_all_open(void)
{
    __free_thumb_resources();
    __ensure_album_ctx();

    OPERATE_RET rt = __get_thumb_batch(&sg_ctx.thumb_iter, &sg_ctx.thumb_batch);
    if (rt != OPRT_OK) {
        PR_WARN("get thumb batch failed (empty album?), rt:%d", rt);
        __free_thumb_resources();
        /* Show empty all page */
        if (sg_album_intfs.disp_all_img_thumb_list) {
            sg_album_intfs.disp_all_img_thumb_list(NULL, 0);
        }
        return;
    }

    __build_thumb_item_arr(&sg_ctx.thumb_batch, &sg_ctx.thumb_arr, &sg_ctx.thumb_arr_cnt);

    if (sg_album_intfs.disp_all_img_thumb_list) {
        sg_album_intfs.disp_all_img_thumb_list(sg_ctx.thumb_arr, sg_ctx.thumb_arr_cnt);
    }
}

/**
 * @brief Close all-thumbnails view: free thumb list resources.
 */
void ai_ui_image_album_view_all_close(void)
{
    __free_thumb_resources();

    if (sg_album_intfs.disp_close) {
        sg_album_intfs.disp_close();
    }
}

/**
 * @brief Open select-thumbnails view: allocate thumb list and display in selection mode.
 */
void ai_ui_image_album_select_open(void)
{
    __free_thumb_resources();
    __ensure_album_ctx();

    OPERATE_RET rt = __get_thumb_batch(&sg_ctx.thumb_iter, &sg_ctx.thumb_batch);
    if (rt != OPRT_OK) {
        PR_WARN("get thumb batch failed (empty album?), rt:%d", rt);
        __free_thumb_resources();
        /* Show empty select page */
        if (sg_album_intfs.disp_select_img_thumb_list) {
            sg_album_intfs.disp_select_img_thumb_list(NULL, 0, AI_UI_ALBUM_SELECT_MAX);
        }
        return;
    }

    __build_thumb_item_arr(&sg_ctx.thumb_batch, &sg_ctx.thumb_arr, &sg_ctx.thumb_arr_cnt);

    if (sg_album_intfs.disp_select_img_thumb_list) {
        sg_album_intfs.disp_select_img_thumb_list(sg_ctx.thumb_arr, sg_ctx.thumb_arr_cnt, AI_UI_ALBUM_SELECT_MAX);
    }
}

/**
 * @brief Close select-thumbnails view: free thumb list resources.
 */
void ai_ui_image_album_select_close(void)
{
    __free_thumb_resources();

    if (sg_album_intfs.disp_close) {
        sg_album_intfs.disp_close();
    }
}

/**
 * @brief Fill an AI_UI_IMG_T with full-resolution image data by name.
 *        Caller takes ownership of img->data and must free with image_album_free_file_data().
 */
void ai_ui_image_album_get_img(char *name, AI_UI_IMG_T *img)
{
    if (name == NULL || img == NULL) {
        return;
    }

    IMAGE_ALBUM_HANDLE album_hdl = __get_album_hdl();
    if (album_hdl == NULL) {
        /* sg_ctx may have been cleared by a prior close — look it up directly */
        album_hdl = image_album_find_by_name(AI_UI_IMAG_ALBUUM_NAME);
        if (album_hdl == NULL) {
            PR_ERR("album not found");
            return;
        }
    }

    ALBUM_IMAGE_ITEM_T item;
    memset(&item, 0, sizeof(item));
    OPERATE_RET rt = image_album_item_retain_locked(album_hdl, name,
                                                    AI_UI_ALBUM_STORAGE_TP, &item);
    if (rt != OPRT_OK) {
        PR_ERR("retain item failed, rt:%d", rt);
        return;
    }

    uint8_t *file_data = NULL;
    size_t   file_size = 0;
    rt = image_album_read(album_hdl, name, AI_UI_ALBUM_STORAGE_TP,
                          &file_data, &file_size);

    image_album_item_release_locked(album_hdl, name);

    if (rt != OPRT_OK) {
        PR_ERR("read image failed, rt:%d", rt);
        return;
    }

    img->name   = name;
    img->width  = (uint16_t)item.attr.width;
    img->height = (uint16_t)item.attr.height;
    img->data   = file_data;
    img->len    = (uint32_t)file_size;
}

/**
 * @brief Get the latest (newest) image from the album.
 *        Caller takes ownership of img->data and must free with image_album_free_file_data().
 *
 * @param[out] img  Filled with image metadata and data on success.
 * @return OPRT_OK on success, error code otherwise.
 */
OPERATE_RET ai_ui_image_album_get_latest_img(AI_UI_IMG_T *img)
{
    if (img == NULL) {
        return OPRT_INVALID_PARM;
    }

    IMAGE_ALBUM_HANDLE album_hdl = __get_album_hdl();
    if (album_hdl == NULL) {
        album_hdl = image_album_find_by_name(AI_UI_IMAG_ALBUUM_NAME);
        if (album_hdl == NULL) {
            PR_ERR("album not found: %s", AI_UI_IMAG_ALBUUM_NAME);
            return OPRT_NOT_FOUND;
        }
    }

    IMAGE_ALBUM_SCAN_HANDLE scan_hdl = NULL;
    OPERATE_RET rt = image_album_scan_init(AI_UI_IMAG_ALBUUM_NAME,
                                           AI_UI_ALBUM_STORAGE_TP,
                                           &scan_hdl);
    if (rt != OPRT_OK) {
        PR_ERR("scan init failed, rt:%d", rt);
        return rt;
    }

    uint32_t count = image_album_scan_get_count(scan_hdl);
    if (count == 0) {
        image_album_scan_deinit(scan_hdl);
        return OPRT_NOT_FOUND;
    }

    image_album_scan_seek(scan_hdl, count - 1);

    ALBUM_IMAGE_ITEM_T item;
    memset(&item, 0, sizeof(item));
    rt = image_album_scan_next(scan_hdl, &item);
    if (rt != OPRT_OK) {
        PR_ERR("scan next failed, rt:%d", rt);
        image_album_scan_deinit(scan_hdl);
        return rt;
    }

    image_album_scan_deinit(scan_hdl);

    ai_ui_image_album_get_img(item.filename, img);
    if (img->data == NULL) {
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

void ai_ui_image_album_free_img(AI_UI_IMG_T *img)
{
    if (NULL == img || NULL == img->data) {
        return;
    }

    image_album_free_file_data(img->data);
    memset(img, 0x00, sizeof(AI_UI_IMG_T));
}

/**
 * @brief Reload album scan and display the newest image.
 *        If the album becomes empty, close it.
 */
void ai_ui_image_album_reload(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* Refresh scan */
    if (sg_ctx.scan_hdl) {
        image_album_scan_deinit(sg_ctx.scan_hdl);
        sg_ctx.scan_hdl = NULL;
    }

    rt = image_album_scan_init(sg_ctx.album_name,
                               AI_UI_ALBUM_STORAGE_TP,
                               &sg_ctx.scan_hdl);
    if (rt != OPRT_OK) {
        PR_ERR("rescan failed, rt:%d", rt);
        return;
    }

    sg_ctx.img_count = image_album_scan_get_count(sg_ctx.scan_hdl);

    if (sg_ctx.img_count > 0) {
        /* Seek to last (newest) and display */
        image_album_scan_seek(sg_ctx.scan_hdl, sg_ctx.img_count - 1);
        ALBUM_IMAGE_ITEM_T item;
        memset(&item, 0, sizeof(item));
        rt = image_album_scan_next(sg_ctx.scan_hdl, &item);
        if (rt == OPRT_OK) {
            __disp_image_by_name(__get_album_hdl(), item.filename);
        }
    } else {
        /* Album is now empty — stay on the view page and show empty state.
         * Do NOT call ai_ui_image_album_close(): that hides album pages without
         * going through the page stack, which leaves chat UI broken (hidden
         * content/+ button that never gets restored). */
        if (sg_album_intfs.disp_image) {
            sg_album_intfs.disp_image(NULL);
        }
    }
}

/**
 * @brief Close the album view — releases all resources (thumb + scan) and notifies UI.
 */
void ai_ui_image_album_close(void)
{
    __free_thumb_resources();

    if (sg_ctx.scan_hdl) {
        image_album_scan_deinit(sg_ctx.scan_hdl);
        sg_ctx.scan_hdl  = NULL;
        sg_ctx.img_count = 0;
    }

    if (sg_album_intfs.disp_close) {
        sg_album_intfs.disp_close();
    }
}

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
void ai_ui_image_album_show_print_result(bool ok)
{
    if (sg_album_intfs.disp_print_result) {
        sg_album_intfs.disp_print_result(ok);
    }
}
#endif

#endif