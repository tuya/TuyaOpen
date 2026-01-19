/**
 * @file ai_chat_ui_manage.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tuya_ringbuf.h"

#include "ai_ui_icon_font.h"
#include "ai_ui_stream_text.h"
#include "ai_ui_manage.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_MSG_MAX_BUF_LEN  1024


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    AI_UI_DISP_TYPE_E type;
    int               len;
    char             *data;
} AI_UI_MSG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_UI_INTFS_T sg_ui_intfs;
static QUEUE_HANDLE  sg_ui_queue_hdl;
static THREAD_HANDLE sg_ui_thrd_hdl;
/***********************************************************
***********************function define**********************
***********************************************************/
static void __ui_disp_msg_handle(AI_UI_MSG_T *msg_data)
{
    if(NULL == msg_data) {
        return;
    }

    switch(msg_data->type) {
        case AI_UI_DISP_USER_MSG: {
            if(sg_ui_intfs.disp_user_msg) {
                sg_ui_intfs.disp_user_msg(msg_data->data);
            }
        } 
        break;
        case AI_UI_DISP_AI_MSG: {
            if(sg_ui_intfs.disp_ai_msg) {
                sg_ui_intfs.disp_ai_msg(msg_data->data);
            }
        } 
        break;
        case AI_UI_DISP_AI_MSG_STREAM_START: {
            if(sg_ui_intfs.disp_ai_msg_stream_start) {
                sg_ui_intfs.disp_ai_msg_stream_start();
            }

#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
            ai_ui_stream_text_start();
#endif
        } 
        break;
        case AI_UI_DISP_AI_MSG_STREAM_DATA: {

#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
            ai_ui_stream_text_write(msg_data->data);
#else 
            if(sg_ui_intfs.disp_ai_msg_stream_data) {
                sg_ui_intfs.disp_ai_msg_stream_data(msg_data->data);
            }
#endif
        } 
        break;
        case AI_UI_DISP_AI_MSG_STREAM_END: {
#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
            ai_ui_stream_text_end();
#else 
            if(sg_ui_intfs.disp_ai_msg_stream_end) {
                sg_ui_intfs.disp_ai_msg_stream_end();
            }
#endif
        } 
        break;
        case AI_UI_DISP_AI_MSG_STREAM_INTERRUPT: {
#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
            ai_ui_stream_text_end();
            ai_ui_stream_text_reset();
#else
        if(sg_ui_intfs.disp_ai_msg_stream_end) {
            sg_ui_intfs.disp_ai_msg_stream_end();
        }
#endif
        } 
        break;
        case AI_UI_DISP_SYSTEM_MSG: {
            if(sg_ui_intfs.disp_system_msg) {
                sg_ui_intfs.disp_system_msg(msg_data->data);
            }
        } 
        break;
        case AI_UI_DISP_EMOTION: {
            if(sg_ui_intfs.disp_emotion) {
                sg_ui_intfs.disp_emotion(msg_data->data);
            }
        } 
        break;
        case AI_UI_DISP_STATUS: {
            if(sg_ui_intfs.disp_ai_mode_state) {
                sg_ui_intfs.disp_ai_mode_state(msg_data->data);
            }
        } 
        break;
        case AI_UI_DISP_NOTIFICATION: {
            if(sg_ui_intfs.disp_notification) {
                sg_ui_intfs.disp_notification(msg_data->data);
            }
        } 
        break;
        case AI_UI_DISP_NETWORK: {
            if(sg_ui_intfs.disp_wifi_state) {
                sg_ui_intfs.disp_wifi_state(((AI_UI_WIFI_STATUS_E*)msg_data->data)[0]);
            }
        } 
        break ;
        case AI_UI_DISP_CHAT_MODE: {
            if(sg_ui_intfs.disp_ai_chat_mode) {                 
                sg_ui_intfs.disp_ai_chat_mode(msg_data->data);
            }
        } 
        break ;
        default:
            if(sg_ui_intfs.disp_other_msg) {
                sg_ui_intfs.disp_other_msg(msg_data->type, (uint8_t *)msg_data->data, msg_data->len);
            }
        break;    
    }
}

static void __ai_chat_ui_task(void *args)
{
    AI_UI_MSG_T msg_data = {0};

    (void)args;

    for (;;) {
        memset(&msg_data, 0, sizeof(AI_UI_MSG_T));
        tal_queue_fetch(sg_ui_queue_hdl, &msg_data, SEM_WAIT_FOREVER);

        __ui_disp_msg_handle(&msg_data);

        if (msg_data.data) {
            Free(msg_data.data);
        }
        msg_data.data = NULL;
    }
}

#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
static void __ai_chat_ui_stream_text_disp(char *string)
{
    if(NULL == string) {
        if(sg_ui_intfs.disp_ai_msg_stream_end) {
            sg_ui_intfs.disp_ai_msg_stream_end();
        }
    }else {
        if(sg_ui_intfs.disp_ai_msg_stream_data) {
            sg_ui_intfs.disp_ai_msg_stream_data(string);
        }
    }
}
#endif

OPERATE_RET ai_ui_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tal_queue_create_init(&sg_ui_queue_hdl, sizeof(AI_UI_MSG_T), 8));

    THREAD_CFG_T cfg;
    memset(&cfg, 0x00, sizeof(THREAD_CFG_T));
    cfg.thrdname = "ai_ui";
    cfg.priority = THREAD_PRIO_2;
    cfg.stackDepth = 1024 * 4;

    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_ui_thrd_hdl, NULL, NULL, __ai_chat_ui_task, NULL, &cfg));

#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
    TUYA_CALL_ERR_RETURN(ai_ui_stream_text_init(__ai_chat_ui_stream_text_disp));
#endif

    if(sg_ui_intfs.disp_init) {
        TUYA_CALL_ERR_RETURN(sg_ui_intfs.disp_init());
    }
    
    PR_DEBUG("ai chat ui init success");   

    return OPRT_OK;
}

OPERATE_RET ai_ui_disp_msg(AI_UI_DISP_TYPE_E tp, uint8_t *data, int len)
{
    AI_UI_MSG_T msg_data;

    msg_data.type = tp;
    msg_data.len = len;
    if (len && data != NULL) {
        msg_data.data = (char *)Malloc(len + 1);
        if (NULL == msg_data.data) {
            return OPRT_MALLOC_FAILED;
        }
        memcpy(msg_data.data, data, len);
        msg_data.data[len] = 0; //"\0"
    } else {
        msg_data.data = NULL;
    }

    return tal_queue_post(sg_ui_queue_hdl, &msg_data, SEM_WAIT_FOREVER);
}

OPERATE_RET ai_ui_camera_start(uint16_t width, uint16_t height)
{
    if(sg_ui_intfs.disp_camera_start) {
        return sg_ui_intfs.disp_camera_start(width, height);
    }

    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET ai_ui_camera_flush(uint8_t *data, uint16_t width, uint16_t height)
{
    if(sg_ui_intfs.disp_camera_flush) {
        return sg_ui_intfs.disp_camera_flush(data, width, height);
    }

    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET ai_ui_camera_end(void)
{
    if(sg_ui_intfs.disp_camera_end) {
        return sg_ui_intfs.disp_camera_end();
    }

    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET ai_ui_register(AI_UI_INTFS_T *intfs)
{
    TUYA_CHECK_NULL_RETURN(intfs, OPRT_INVALID_PARM);

    memcpy(&sg_ui_intfs, intfs, sizeof(AI_UI_INTFS_T));

    return OPRT_OK;
}

