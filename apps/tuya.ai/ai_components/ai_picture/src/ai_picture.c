/**
 * @file ai_picture.c
 * @brief Picture album management implementation.
 *        Wraps image_album / image_album_scan / image_album_thumb to provide
 *        album lifecycle, sequential photo browsing, deletion, and thumbnails.
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "ai_picture.h"
#include "ai_user_event.h"

#include "ai_picture_output.h"

#include "image_album_scan.h"
#include "image_album_thumb.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    IMAGE_ALBUM_HANDLE      album_hdl;
    IMAGE_ALBUM_SCAN_HANDLE album_scan_hdl;
    uint32_t                scan_img_count;
    /** 1-based index of the picture currently displayed; 0 if unknown / album empty */
    uint32_t cur_view_pos;
    char     cur_name[ALBUM_FILENAME_MAX_LEN + 1];
} AI_PICTURE_CTX_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_PICTURE_CTX_T sg_picture_ctx;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Trim album to max image count, deleting oldest images when exceeded
 * @param[in] album_handle album handle
 * @param[in] max_cnt maximum allowed image count
 * @return none
 */
static void __album_trim_oldest(IMAGE_ALBUM_HANDLE album_handle, uint32_t max_cnt)
{
    uint32_t count = 0;

    if (OPRT_OK != image_album_get_committed_count(album_handle, &count)) {
        return;
    }

    while (count > max_cnt) {
        ALBUM_IMAGE_ITEM_T oldest;
        memset(&oldest, 0x00, sizeof(ALBUM_IMAGE_ITEM_T));
        if (OPRT_OK != image_album_get_next_item(album_handle, NULL, AI_PICTURE_GET_STORAGE_TP, &oldest)) {
            break;
        }
        PR_DEBUG("album full(%d/%d), delete oldest: %s", count, max_cnt, oldest.filename);
        image_album_delete(album_handle, oldest.filename);
        count--;
    }
}

/**
 * @brief Initialize the picture album module
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    IMAGE_ALBUM_INIT_CFG_T album_init = {
        .storage_mask = AI_PICTURE_ALBUM_STORAGE_MASK,
        .recover_tp   = AI_PICTURE_ALBUM_RECOVER_TP,
    };

    TUYA_CALL_ERR_RETURN(image_album_init(COMP_AI_PICTURE_ALBUM_NAME, 
                                          &album_init, 
                                          &sg_picture_ctx.album_hdl));

    TUYA_CALL_ERR_LOG(ai_picture_output_set_size(COMP_AI_PICTURE_DEF_OUTPIUT_WIDTH,
                                                 COMP_AI_PICTURE_DEF_OUTPIUT_HEIGHT));

#if defined(ENABLE_COMP_AI_PICTURE_HOSTING_DLD) && (ENABLE_COMP_AI_PICTURE_HOSTING_DLD == 1)
    TUYA_CALL_ERR_LOG(ai_picture_output_dld_init(COMP_AI_PICTURE_DEF_OUTPIUT_WIDTH,
                                                 COMP_AI_PICTURE_DEF_OUTPIUT_HEIGHT));
#endif

    return rt;
}

/**
 * @brief Get the internal album handle (for use by sibling modules)
 * @return album handle, NULL if not initialized
 */
IMAGE_ALBUM_HANDLE ai_picture_get_album_handle(void)
{
    return sg_picture_ctx.album_hdl;
}

/**
 * @brief Save a JPEG picture to the album with a timestamp-based filename
 * @param[in] picture JPEG data buffer
 * @param[in] len JPEG data length in bytes
 * @param[out] name filled with the generated filename (may be NULL)
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_save_to_album(uint8_t *picture, uint32_t len, const char *in_name, char name[AI_PICTURE_NAME_MAX_LEN + 1])
{
    OPERATE_RET        rt           = OPRT_OK;
    IMAGE_ALBUM_HANDLE album_handle = ai_picture_get_album_handle();

    if (NULL == album_handle) {
        PR_ERR("album handle is null");
        return OPRT_COM_ERROR;
    }

    if (NULL == picture || len == 0) {
        PR_ERR("picture:%p, total_len:%d", picture, len);
        return OPRT_INVALID_PARM;
    }

    SYS_TICK_T timestamp                       = tal_time_get_posix_ms();
    char       filename[AI_PICTURE_NAME_MAX_LEN + 1] = {0};
    if (in_name && in_name[0] != '\0') {
        strncpy(filename, in_name, AI_PICTURE_NAME_MAX_LEN);
    } else {
        snprintf(filename, sizeof(filename), "ai_pic_%llu", timestamp);
    }

    ALBUM_IMAGE_SAVE_INFO_T info = {
        .filename  = filename,
        .format    = ALBUM_IMAGE_FORMAT_JPEG,
        .file_data = picture,
        .file_size = len,
        .timestamp = timestamp,
    };

    TUYA_CALL_ERR_RETURN(image_album_save(album_handle, &info));

    PR_NOTICE("[pic_chain] album saved, filename:%s, size:%u", filename, len);
    if (name) {
        strncpy(name, info.filename, AI_PICTURE_NAME_MAX_LEN);
    }

    __album_trim_oldest(album_handle, COMP_AI_PICTURE_ALBUM_MAX_IMAGE_CNT);

    return OPRT_OK;
}

char *ai_picture_get_album_name(void)
{
    return COMP_AI_PICTURE_ALBUM_NAME;
}
