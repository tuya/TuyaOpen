/**
 * @file ai_picture_output.c
 * @brief Picture output implementation.
 *
 * This file provides functions for downloading pictures from URLs via HTTP
 * and outputting them through callback functions, with event notification support.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "http_session.h"
#include "mix_method.h"

#include "ai_picture_output.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define GET_IMAGE_BUF_LEN 1024

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    GET_IMGE_CMD_START = 0,
    GET_IMGE_CMD_STOP,
}GET_IMGE_CMD_T;


typedef struct {
    GET_IMGE_CMD_T cmd;
    char *url;
}GET_IMGE_MSG_T;

typedef struct{
    bool             is_start;
    THREAD_HANDLE    task;
    QUEUE_HANDLE     queue;
    uint32_t         wait_queue_time;
    http_session_t   session;
    uint32_t         img_size;
    uint32_t         offset;
    uint8_t          rev_buf[GET_IMAGE_BUF_LEN];
}GET_IMAGE_CTX_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static GET_IMAGE_CTX_T            *sg_get_img_ctx = NULL;
static AI_PICTURE_OUTPUT_NOTIFY_CB sg_notify_cb = NULL;
static AI_PICTURE_OUTPUT_CB        sg_output_cb = NULL;
/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Notify picture output event to registered callback.
 *
 * @param event Event type to notify.
 */
static void __notify_picture_event(AI_PICTURE_OUTPUT_EVENT_E event)
{
    AI_PICTURE_OUTPUT_NOTIFY_T info;

    if(NULL == sg_get_img_ctx) {
        return;
    }   

    if(sg_notify_cb) {
        info.event = event;
        info.total_size = sg_get_img_ctx->img_size;

        sg_notify_cb(&info);
    }
}

/**
 * @brief Stop downloading picture stream and cleanup resources.
 */
static void __download_picture_stream_stop(void)
{
    if(NULL == sg_get_img_ctx) {
        return;
    }

    sg_get_img_ctx->is_start = false;
    sg_get_img_ctx->offset = 0;
    sg_get_img_ctx->img_size = 0;

    if(sg_get_img_ctx->session) {
        PR_NOTICE("Closing HTTP session");
        http_close_session(&sg_get_img_ctx->session);
        sg_get_img_ctx->session = NULL;   
    }
}

/**
 * @brief Start downloading picture stream from URL.
 *
 * @param url Pointer to the picture URL string.
 */
static void __download_picture_stream_start(char *url)
{
    OPERATE_RET rt = OPRT_OK;
    http_resp_t *response = NULL;
    uint32_t  image_size = 0;

    if(NULL == url || NULL == sg_get_img_ctx) {
        PR_ERR("url is null");
        return;
    }

    rt = http_open_session(&sg_get_img_ctx->session, url, 5000);
    if(rt != OPRT_OK) {
        PR_ERR("http_open_session failed: %d", rt);
        goto __START_ERR;
    }
    PR_NOTICE("HTTP session opened successfully");

    http_req_t request;
    memset(&request, 0, sizeof(http_req_t));
    request.type = HTTP_GET;
    request.version = HTTP_VER_1_1;

    TUYA_CALL_ERR_GOTO(http_send_request(sg_get_img_ctx->session, &request, 0), __START_ERR);

    TUYA_CALL_ERR_GOTO(http_get_response_hdr(sg_get_img_ctx->session, &response), __START_ERR);
    if(response->status_code != 200) {
        PR_ERR("http response code invalid: %d", response->status_code);
        goto __START_ERR;
    }

    image_size = response->content_length;

    http_free_response_hdr(&response); 

    sg_get_img_ctx->img_size = image_size;
    sg_get_img_ctx->offset = 0;

    __notify_picture_event(AI_PICTURE_OUTPUT_START);

    sg_get_img_ctx->wait_queue_time = 1;
    sg_get_img_ctx->is_start = true;

    return;

__START_ERR:
    if(response) {
        http_free_response_hdr(&response);
    }

    __download_picture_stream_stop();

    return;
}

/**
 * @brief Download picture stream data and output through callback.
 */
static void __download_picture_stream(void)
{
    if(NULL == sg_get_img_ctx || false == sg_get_img_ctx->is_start) {
        return;
    }

    int len = http_read_content(sg_get_img_ctx->session, \
                                sg_get_img_ctx->rev_buf,\
                                GET_IMAGE_BUF_LEN);

    if(len > 0) {
        if(sg_output_cb) {
            bool is_eof = (sg_get_img_ctx->offset + len >= sg_get_img_ctx->img_size) ? true : false;
            sg_output_cb(sg_get_img_ctx->rev_buf, len, is_eof);
        }

        sg_get_img_ctx->offset += len;
    }else if(len == 0) {
        PR_NOTICE("download image complete, size: %d offset:%d", sg_get_img_ctx->img_size, sg_get_img_ctx->offset);

        __download_picture_stream_stop();
        __notify_picture_event(AI_PICTURE_OUTPUT_SUCCESS);
    }else {
        PR_ERR("download image error: %d", len);

        __download_picture_stream_stop();
        __notify_picture_event(AI_PICTURE_OUTPUT_FAILED);
    }
}

/**
 * @brief Picture download task thread function.
 *
 * @param arg Thread argument (unused).
 */
static void __download_picture_task(void *arg)
{
    GET_IMGE_MSG_T msg = {0};

    while(tal_thread_get_state(sg_get_img_ctx->task) == THREAD_STATE_RUNNING) {
        if (tal_queue_fetch(sg_get_img_ctx->queue, &msg, sg_get_img_ctx->wait_queue_time) != OPRT_OK) {
            if(true == sg_get_img_ctx->is_start) {
                __download_picture_stream();
            }

            continue;
        }

        switch(msg.cmd) {
            case GET_IMGE_CMD_START:
                __download_picture_stream_start(msg.url);
                break;
            case GET_IMGE_CMD_STOP:
                __download_picture_stream_stop();
                break;
            default:
                break;
        }

        if(msg.url) {
            tal_free(msg.url);
            msg.url = NULL;
        }
    
    }
}
/**
 * @brief Initialize the picture output module.
 *
 * @param cfg Pointer to the output configuration structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_picture_output_init(AI_PICTURE_OUTPUT_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(cfg, OPRT_INVALID_PARM);

    sg_get_img_ctx = (GET_IMAGE_CTX_T *)Malloc(sizeof(GET_IMAGE_CTX_T));
    TUYA_CHECK_NULL_RETURN(sg_get_img_ctx, OPRT_MALLOC_FAILED);
    memset(sg_get_img_ctx, 0, sizeof(GET_IMAGE_CTX_T));

    TUYA_CALL_ERR_RETURN(tal_queue_create_init(&sg_get_img_ctx->queue, sizeof(GET_IMGE_MSG_T), 2));

    THREAD_CFG_T thrd_param;

    memset(&thrd_param, 0, sizeof(THREAD_CFG_T));
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "get_picture_task";

    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_get_img_ctx->task, NULL, NULL,\
                                                     __download_picture_task, NULL,\
                                                     &thrd_param));

    sg_notify_cb = cfg->notify_cb;
    sg_output_cb = cfg->output_cb;

    return rt;
}

/**
 * @brief Start downloading and outputting picture from URL.
 *
 * @param url Pointer to the picture URL string.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_picture_output_start(char *url)
{
    OPERATE_RET rt = OPRT_OK;
    GET_IMGE_MSG_T msg = {0};

    TUYA_CHECK_NULL_RETURN(url, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(sg_get_img_ctx, OPRT_INVALID_PARM);

    memset(&msg, 0, sizeof(GET_IMGE_MSG_T));

    msg.cmd = GET_IMGE_CMD_START;
    msg.url = mm_strdup(url);

    TUYA_CALL_ERR_RETURN(tal_queue_post(sg_get_img_ctx->queue, &msg, QUEUE_WAIT_FOREVER));

    return rt;
}

/**
 * @brief Stop picture output and cleanup resources.
 *
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_picture_output_stop(void)
{
    OPERATE_RET rt = OPRT_OK;
    GET_IMGE_MSG_T msg = {0};

    TUYA_CHECK_NULL_RETURN(sg_get_img_ctx, OPRT_INVALID_PARM);

    memset(&msg, 0, sizeof(GET_IMGE_MSG_T));
    msg.cmd = GET_IMGE_CMD_STOP;

    TUYA_CALL_ERR_RETURN(tal_queue_post(sg_get_img_ctx->queue, &msg, QUEUE_WAIT_FOREVER));

    return rt;
}

/**
 * @brief Check if picture output module is initialized.
 *
 * @return true if initialized, false otherwise.
 */
bool ai_picture_is_init(void)
{
    if(NULL == sg_get_img_ctx) {
        return false;
    }

    return true;
}