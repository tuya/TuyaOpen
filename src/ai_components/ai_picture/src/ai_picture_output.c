/**
 * @file ai_picture_output.c
 * @brief Picture output module implementation.
 *        Accumulates streamed JPEG chunks into a contiguous buffer,
 *        saves the completed picture to the album, and notifies listeners.
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "cJSON.h"
#include "tuya_ai_agent.h"
#include "ai_user_event.h"

#include "ai_picture_output.h"

#if defined(ENABLE_COMP_AI_PICTURE_HOSTING_DLD) && (ENABLE_COMP_AI_PICTURE_HOSTING_DLD == 1)
#include "tuya_file_storage_dld.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_PICTURE_OUTPUT_WIDTH_KEY  "sys.device.img_resize.width"
#define AI_PICTURE_OUTPUT_HEIGHT_KEY "sys.device.img_resize.height"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    bool     is_start;
    uint32_t total_size;
    uint32_t offset;
    uint8_t *acc_buf;
} AI_PICTURE_STREAM_T;

typedef struct {
    uint16_t set_width;
    uint16_t set_height;
} AI_PICTURE_OUTPUT_CTX_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_PICTURE_OUTPUT_CTX_T sg_picture_output;
static AI_PICTURE_STREAM_T     sg_ai_pic_stream;

/***********************************************************
***********************function define**********************
***********************************************************/
extern IMAGE_ALBUM_HANDLE ai_picture_get_album_handle(void);

/**
 * @brief Free partial JPEG accumulator and clear session state
 * @return none
 */
static void __ai_picture_output_accum_reset(void)
{
    if (sg_ai_pic_stream.acc_buf != NULL) {
        Free(sg_ai_pic_stream.acc_buf);
    }
    memset(&sg_ai_pic_stream, 0, sizeof(AI_PICTURE_STREAM_T));
}

/**
 * @brief Event callback to push output picture dimensions to AI agent custom params
 * @param[in] data unused
 * @return 0 on success
 */
static int __set_output_picture_size_cb(void *data)
{
    (void)data;

    cJSON *custom_param = cJSON_CreateObject();
    if(NULL ==  custom_param) {
        return OPRT_CR_CJSON_ERR;
    }

    cJSON *cj_width = cJSON_CreateObject();
    cJSON_AddNumberToObject(cj_width, "value", sg_picture_output.set_width);
    cJSON_AddItemToObject(custom_param, AI_PICTURE_OUTPUT_WIDTH_KEY, cj_width);

    cJSON *cj_height = cJSON_CreateObject();
    cJSON_AddNumberToObject(cj_height, "value", sg_picture_output.set_height);
    cJSON_AddItemToObject(custom_param, AI_PICTURE_OUTPUT_HEIGHT_KEY, cj_height);

    char *out = cJSON_PrintUnformatted(custom_param);
    cJSON_Delete(custom_param);

    if(out) {
        tuya_ai_agent_set_session_param(out);
        PR_DEBUG("%s", out);
        cJSON_free(out);
    }else {
        PR_ERR("cjson printunformatted failed");
    }

    return 0;
}

/**
 * @brief Set the desired output picture dimensions for AI image generation
 * @param[in] width desired width in pixels
 * @param[in] height desired height in pixels
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_output_set_size(uint16_t width, uint16_t height)
{
    OPERATE_RET rt = OPRT_OK;

    sg_picture_output.set_width  = width;
    sg_picture_output.set_height = height;

    TUYA_CALL_ERR_LOG(tal_event_subscribe(EVENT_AI_CLIENT_RUN,
                                          "set_output_picture_size",
                                          __set_output_picture_size_cb,
                                          SUBSCRIBE_TYPE_NORMAL));

    return rt;
}

/**
 * @brief Accumulate a JPEG chunk and save to album when all chunks are received
 * @param[in] data JPEG chunk data
 * @param[in] len chunk length in bytes
 * @param[in] total_len total expected JPEG size in bytes
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_output_save_to_album(uint8_t *data, uint32_t len, uint32_t total_len)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == data || len == 0 || total_len == 0) {
        PR_ERR("invalid param, data:%p, len:%u, total_len:%u", data, len, total_len);
        return OPRT_INVALID_PARM;
    }

    if (false == sg_ai_pic_stream.is_start) {
        PR_NOTICE("[pic_chain] start accumulating, total_len:%u", total_len);
        sg_ai_pic_stream.acc_buf = (uint8_t *)Malloc((size_t)total_len);
        if (sg_ai_pic_stream.acc_buf == NULL) {
            PR_ERR("[pic_chain] malloc %u bytes failed", total_len);
            return OPRT_MALLOC_FAILED;
        }

        sg_ai_pic_stream.total_size = total_len;
        sg_ai_pic_stream.offset     = 0;
        sg_ai_pic_stream.is_start   = true;
    } else {
        if (sg_ai_pic_stream.total_size != total_len) {
            PR_ERR("get total size:%u is different %u", total_len, sg_ai_pic_stream.total_size);
            __ai_picture_output_accum_reset();
            return OPRT_COM_ERROR;
        }
    }

    if (len > total_len - sg_ai_pic_stream.offset) {
        PR_ERR("chunk overflow: offset=%u len=%u total=%u", sg_ai_pic_stream.offset, len, sg_ai_pic_stream.total_size);
        __ai_picture_output_accum_reset();
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    memcpy(sg_ai_pic_stream.acc_buf + sg_ai_pic_stream.offset, data, (size_t)len);
    sg_ai_pic_stream.offset += len;
    PR_DEBUG("[pic_chain] chunk accumulated, offset:%u/%u", sg_ai_pic_stream.offset, sg_ai_pic_stream.total_size);

    if (sg_ai_pic_stream.offset >= sg_ai_pic_stream.total_size) {
        PR_NOTICE("[pic_chain] all chunks received, total:%u, saving to album", sg_ai_pic_stream.total_size);
        char name[AI_PICTURE_NAME_MAX_LEN + 1] = {0};

        rt = ai_picture_save_to_album(sg_ai_pic_stream.acc_buf, sg_ai_pic_stream.total_size, NULL, name);
        if (rt != OPRT_OK) {
            PR_ERR("[pic_chain] save to album failed, rt:%d", rt);
        } else {
            PR_NOTICE("[pic_chain] save to album success, name:%s, notify PICTURE_GENERATED", name);
            ai_user_event_notify(AI_USER_EVT_GENERATE_PICTURE, name);
        }

        __ai_picture_output_accum_reset();
    }

    return rt;
}

#if defined(ENABLE_COMP_AI_PICTURE_HOSTING_DLD) && (ENABLE_COMP_AI_PICTURE_HOSTING_DLD == 1)

typedef struct {
    uint8_t  *buf;
    uint32_t  buf_capacity;
    uint32_t  received_len;
    char      cloud_name[AI_PICTURE_NAME_MAX_LEN + 1];
} AI_PICTURE_DLD_FILE_T;

static AI_PICTURE_DLD_FILE_T sg_dld_file;

static OPERATE_RET __ai_picture_dld_notify(FILE_DL_NOTIFY_TYPE_E type, void *info, char **errmsg)
{
    switch (type) {
    case FILE_DL_TYPE_START:
        memset(&sg_dld_file, 0, sizeof(AI_PICTURE_DLD_FILE_T));
        break;

    case FILE_DL_TYPE_TRANS_START: {
        FILE_DL_TRANS_START_INFO_T *ts = (FILE_DL_TRANS_START_INFO_T *)info;
        memset(&sg_dld_file, 0, sizeof(AI_PICTURE_DLD_FILE_T));
        if (ts && ts->file_name) {
            strncpy(sg_dld_file.cloud_name, ts->file_name, AI_PICTURE_NAME_MAX_LEN);
        }
        sg_dld_file.buf = (uint8_t *)Malloc(COMP_AI_PICTURE_DLD_MAX_FILE_SIZE);
        if (sg_dld_file.buf == NULL) {
            PR_ERR("dld malloc failed, size:%u", COMP_AI_PICTURE_DLD_MAX_FILE_SIZE);
            return OPRT_MALLOC_FAILED;
        }
        sg_dld_file.buf_capacity = COMP_AI_PICTURE_DLD_MAX_FILE_SIZE;
        break;
    }

    case FILE_DL_TYPE_TRANS: {
        FILE_DL_TRANS_INFO_T *t = (FILE_DL_TRANS_INFO_T *)info;
        if (!t || !t->data || t->len == 0 || !sg_dld_file.buf) {
            break;
        }
        if (sg_dld_file.received_len + t->len > sg_dld_file.buf_capacity) {
            PR_ERR("dld overflow: received=%u chunk=%u cap=%u",
                   sg_dld_file.received_len, t->len, sg_dld_file.buf_capacity);
            return OPRT_BUFFER_NOT_ENOUGH;
        }
        memcpy(sg_dld_file.buf + sg_dld_file.received_len, t->data, t->len);
        sg_dld_file.received_len += t->len;
        break;
    }

    case FILE_DL_TYPE_TRANS_END: {
        FILE_DL_TRANS_END_INFO_T *te = (FILE_DL_TRANS_END_INFO_T *)info;
        char saved_name[AI_PICTURE_NAME_MAX_LEN + 1] = {0};
        OPERATE_RET save_rt = OPRT_COM_ERROR;

        if (te && te->ret == 0 && sg_dld_file.buf && sg_dld_file.received_len > 0) {
            save_rt = ai_picture_save_to_album(sg_dld_file.buf, sg_dld_file.received_len,
                                              sg_dld_file.cloud_name[0] ? sg_dld_file.cloud_name : NULL,
                                              saved_name);
            if (save_rt != OPRT_OK) {
                PR_ERR("dld save to album failed, rt:%d", save_rt);
            }
        }

        /* Free the large download buffer BEFORE notifying the UI thread.
         * This ensures the heap is settled (no concurrent large Free racing
         * with the UI thread's Malloc for the RGB565 decode buffer) when the
         * UI thread begins processing the event. */
        if (sg_dld_file.buf) {
            Free(sg_dld_file.buf);
        }
        memset(&sg_dld_file, 0, sizeof(AI_PICTURE_DLD_FILE_T));

        if (save_rt == OPRT_OK) {
            ai_user_event_notify(AI_USER_EVT_GET_PICTURE_FROM_APP, saved_name);
        }
        break;
    }

    case FILE_DL_TYPE_END:
        break;

    case FILE_DL_TYPE_QUERY: {
        FILE_DL_QUERY_DEL_INFO_T *qd = (FILE_DL_QUERY_DEL_INFO_T *)info;
        if (qd) {
            tuya_file_storage_dl_mq_rept(NULL, qd->file_name, NULL, "query");
        }
        break;
    }

    case FILE_DL_TYPE_DELETE: {
        FILE_DL_QUERY_DEL_INFO_T *qd = (FILE_DL_QUERY_DEL_INFO_T *)info;
        if (qd) {
            image_album_delete(ai_picture_get_album_handle(), qd->file_name);
            tuya_file_storage_dl_mq_rept(NULL, qd->file_name, NULL, "delete");
        }
        break;
    }

    default:
        break;
    }
    return OPRT_OK;
}

OPERATE_RET ai_picture_output_dld_init(uint16_t width, uint16_t height)
{
    FILE_DL_CONFIG_CB_T cfg;
    memset(&cfg, 0, sizeof(FILE_DL_CONFIG_CB_T));
    snprintf(cfg.resolution, sizeof(cfg.resolution), "%d*%d", width, height);
    strncpy(cfg.allow_formats[0], "jpg", sizeof(cfg.allow_formats[0]) - 1);
    cfg.max_per_file_size = COMP_AI_PICTURE_DLD_MAX_FILE_SIZE;
    cfg.max_file_cnt      = COMP_AI_PICTURE_DLD_MAX_FILE_CNT;
    cfg.unit_size         = COMP_AI_PICTURE_DLD_UNIT_SIZE;
    cfg.notify_cb         = __ai_picture_dld_notify;

    return tuya_file_storage_dl_set_config(cfg);
}

#endif /* ENABLE_COMP_AI_PICTURE_HOSTING_DLD */
