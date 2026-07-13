/**
 * @file ai_picture_input.c
 * @brief Picture input queue implementation.
 *        Queues album pictures by retain-locking them (no data read at add time),
 *        then reads and sends each one at send time to minimize peak memory usage.
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include "string.h"
#include "tal_api.h"
#include "lang_config.h"
#include "tuya_ai_agent.h"
#include "ai_user_event.h"
#include "ai_agent.h"

#include "ai_picture.h"
#include "ai_picture_input.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    ALBUM_IMAGE_ITEM_T pic;
    char              *text;
} AI_PICTURE_INPUT_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_PICTURE_INPUT_T sg_pic_input[AI_PICTURE_INPUT_MAX_NUM];
static int8_t             sg_pic_num = 0;

/***********************************************************
********************function declaration********************
***********************************************************/
extern IMAGE_ALBUM_HANDLE ai_picture_get_album_handle(void);

/**
 * @brief Add a picture from album to the input queue (retain-lock only, data read deferred)
 * @param[in] filename album picture filename
 * @param[in] text optional text to send along with the picture (NULL if none)
 * @return OPRT_OK on success, OPRT_EXCEED_UPPER_LIMIT if queue is full
 */
OPERATE_RET ai_picture_input_add_from_album(char *filename, char *text)
{
    OPERATE_RET rt = OPRT_OK;

    if (sg_pic_num >= AI_PICTURE_INPUT_MAX_NUM) {
        return OPRT_EXCEED_UPPER_LIMIT;
    }

    IMAGE_ALBUM_HANDLE hdl = ai_picture_get_album_handle();
    ALBUM_IMAGE_ITEM_T item;
    memset(&item, 0x00, sizeof(ALBUM_IMAGE_ITEM_T));
    TUYA_CALL_ERR_RETURN(image_album_item_retain_locked(hdl, filename, AI_PICTURE_GET_STORAGE_TP, &item));
    PR_DEBUG("add pic width:%d height:%d name:%s", item.attr.width, item.attr.height, item.filename);

    AI_PICTURE_INPUT_T *slot = &sg_pic_input[sg_pic_num];
    memcpy(&slot->pic, &item, sizeof(ALBUM_IMAGE_ITEM_T));


    PR_DEBUG("ai_picture_input_add name:%s, width:%d, height:%d ", slot->pic.filename, 
                                                                   slot->pic.attr.width,
                                                                   slot->pic.attr.height);

    if (text && strlen(text) > 0) {
        slot->text = tal_malloc(strlen(text) + 1);
        if (slot->text) {
            strcpy(slot->text, text);
        }
    } else {
        slot->text = NULL;
    }

    sg_pic_num++;

    return OPRT_OK;
}

/**
 * @brief Remove a queued picture by filename and release its retain-lock
 * @param[in] filename album picture filename to remove
 * @return OPRT_OK on success, OPRT_NOT_FOUND if not in queue
 */
OPERATE_RET ai_picture_input_del_from_album(char *filename)
{
    OPERATE_RET         rt   = OPRT_OK;
    AI_PICTURE_INPUT_T *slot = NULL;
    uint8_t             idx  = 0;

    for (int i = 0; i < sg_pic_num; i++) {
        if (0 == strcmp(filename, sg_pic_input[i].pic.filename)) {
            slot = &sg_pic_input[i];
            idx  = i;
            break;
        }
    }

    if (NULL == slot) {
        return OPRT_NOT_FOUND;
    }

    TUYA_CALL_ERR_RETURN(image_album_item_release_locked(ai_picture_get_album_handle(), slot->pic.filename));

    if (slot->text) {
        tal_free(slot->text);
        slot->text = NULL;
    }

    uint32_t move_len = (AI_PICTURE_INPUT_MAX_NUM - (idx + 1)) * sizeof(AI_PICTURE_INPUT_T);
    if (move_len) {
        memmove(&sg_pic_input[idx], &sg_pic_input[idx + 1], move_len);
    }

    sg_pic_num--;

    return OPRT_OK;
}

/**
 * @brief Send all queued pictures to the AI agent and clear the queue.
 *        Each picture is read, sent, and freed one at a time to save memory.
 *        Fires AI_AI_EVENT_SEND_PICTURE_END when at least one picture was sent.
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_input_from_album(void)
{
    OPERATE_RET rt       = OPRT_OK;
    bool        has_data = false;

    for (uint32_t idx = 0; idx < sg_pic_num; idx++) {
        AI_PICTURE_INPUT_T *slot      = &sg_pic_input[idx];
        uint64_t            timestamp = 0;

        /* read image data at send time to save memory */
        uint8_t           *file_data = NULL;
        size_t             file_size = 0;
        IMAGE_ALBUM_HANDLE hdl       = ai_picture_get_album_handle();
        rt = image_album_read(hdl, slot->pic.filename, AI_PICTURE_GET_STORAGE_TP, &file_data, &file_size);
        if (rt != OPRT_OK || file_data == NULL) {
            PR_ERR("ai_picture_input_send read failed: %s, rt=%d", slot->pic.filename, rt);
            TUYA_CALL_ERR_LOG(image_album_item_release_locked(hdl, slot->pic.filename));
            continue;
        }

        has_data  = true;
        timestamp = tal_system_get_millisecond();
        PR_DEBUG("ai_picture_input_send name:%s, file_size:%d", slot->pic.filename, &file_size);

        TUYA_CALL_ERR_RETURN(tuya_ai_image_input(timestamp, file_data, file_size, file_size));

        image_album_free_file_data(file_data);

        if (slot->text) {
            TUYA_CALL_ERR_LOG(tuya_ai_text_input((uint8_t *)slot->text, strlen(slot->text), strlen(slot->text)));
            tal_free(slot->text);
            slot->text = NULL;
        }

        TUYA_CALL_ERR_LOG(image_album_item_release_locked(hdl, slot->pic.filename));
    }

    memset(sg_pic_input, 0x00, sizeof(sg_pic_input));
    sg_pic_num = 0;

    if (has_data) {
        ai_user_event_notify(AI_USER_EVT_SEND_PICTURE_END, NULL);
    }

    return OPRT_OK;
}

/**
 * @brief Get the number of pictures currently queued
 * @return number of queued pictures
 */
uint32_t ai_picture_input_get_num(void)
{
    return sg_pic_num;
}

/**
 * @brief Send a single image with a recognition prompt to the AI agent
 * @param[in] data JPEG image data
 * @param[in] len JPEG data length in bytes
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_input_recognize(uint8_t *data, uint32_t len)
{
    OPERATE_RET rt   = OPRT_OK;
    uint64_t   timestamp = tal_system_get_millisecond();
    char       *text = RECOGNIZE_IMAGE_PROMPT;

    if (NULL == data || 0 == len) {
        return OPRT_INVALID_PARM;
    }

    tuya_ai_input_start(TRUE);
    TUYA_CALL_ERR_RETURN(tuya_ai_image_input(timestamp, (uint8_t *)data, len, len));
    TUYA_CALL_ERR_RETURN(tuya_ai_text_input((uint8_t *)text, strlen(text), strlen(text)));
    tuya_ai_input_stop();

    return rt;
}

#ifdef __cplusplus
}
#endif
