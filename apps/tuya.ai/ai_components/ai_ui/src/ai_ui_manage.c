/**
 * @file ai_ui_manage.c
 * @brief AI UI management implementation.
 *
 * This file provides functions for managing AI user interface, including
 * message queue handling, display interface registration, and camera/picture display.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include <string.h>

#include "ai_ui_icon_font.h"
#include "ai_ui_stream_text.h"

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
#include "ai_ui_manage.h"
#include "image_album.h"
#include "ai_ui_image_album.h"
#endif

#include "ai_ui_camera.h"
#include "ai_ui_page.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_MSG_MAX_BUF_LEN  1024


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    AI_UI_ACTION_E action;
    uint8_t       *data;
    uint32_t       len;
} AI_UI_ACTION_MSG_T;

typedef struct {
    AI_UI_DISP_TYPE_E type;
    int               len;
    char             *data;
    SEM_HANDLE        sync_sem;
} AI_UI_MSG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_UI_ACTION_CB sg_action_cb = NULL;
static QUEUE_HANDLE  sg_action_queue_hdl;
static THREAD_HANDLE sg_action_thrd_hdl;


static AI_UI_INTFS_T sg_ui_intfs;
static AI_UI_CHAT_INTFS_T sg_chat_intfs;
static QUEUE_HANDLE  sg_ui_queue_hdl;
static THREAD_HANDLE sg_ui_thrd_hdl;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief AI UI action dispatch task thread function.
 *
 * @param args Thread argument (unused).
 * @return none
 */
static void __ai_ui_action_task(void *args)
{
    AI_UI_ACTION_MSG_T msg_data = {0};

    (void)args;

    for (;;) {
        memset(&msg_data, 0, sizeof(AI_UI_ACTION_MSG_T));
        tal_queue_fetch(sg_action_queue_hdl, &msg_data, SEM_WAIT_FOREVER);

        if (msg_data.action >= AI_UI_ACT_MAX) {
            continue;
        }

        if (sg_action_cb != NULL) {
            sg_action_cb(msg_data.action, msg_data.data, msg_data.len);
        }

        if (msg_data.data) {
            Free(msg_data.data);
            msg_data.data = NULL;
        }
    }
}

static OPERATE_RET __ai_ui_action_manage_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tal_queue_create_init(&sg_action_queue_hdl, sizeof(AI_UI_ACTION_MSG_T), 8));

    THREAD_CFG_T thrd_cfg;
    memset(&thrd_cfg, 0x00, sizeof(THREAD_CFG_T));
    thrd_cfg.thrdname = "ai_ui_action";
    thrd_cfg.priority = THREAD_PRIO_2;
    thrd_cfg.stackDepth = 1024 * 4;
    thrd_cfg.psram_mode = 1;
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_action_thrd_hdl, NULL, NULL, __ai_ui_action_task, NULL, &thrd_cfg));

    return OPRT_OK;
}

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
/**
 * @brief Callback invoked when user taps an image link in the chat.
 *        Loads the full image from album by name and displays it.
 *
 * @param arg  Image name string (owned by the UI layer).
 */
static void __image_link_view_cb(void *arg)
{
    char *name = (char *)arg;
    if (name == NULL) {
        return;
    }

    AI_UI_IMG_T img = {0};
    ai_ui_image_album_get_img(name, &img);
    if (sg_chat_intfs.disp_image && img.data) {
        sg_chat_intfs.disp_image(&img);
    }
    if (img.data) {
        image_album_free_file_data(img.data);
    }

}
#endif

/***********************************************************
*******************page callback wrappers*******************
***********************************************************/
static OPERATE_RET __page_chat_open(void *arg)
{
    if(sg_chat_intfs.disp_open) {
        sg_chat_intfs.disp_open();
    }

    return OPRT_OK;
}

static OPERATE_RET __page_chat_close(void)
{
    if(sg_chat_intfs.disp_close) {
        sg_chat_intfs.disp_close();
    }

    return OPRT_OK;
}

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
static OPERATE_RET __page_camera_open(void *arg)
{
    return ai_ui_camera_open();
}

static OPERATE_RET __page_camera_close(void)
{
    return ai_ui_camera_close();
}
#endif

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
static OPERATE_RET __page_album_view_open(void *arg)
{
    ai_ui_image_album_open();
    return OPRT_OK;
}

static OPERATE_RET __page_album_view_close(void)
{
    /* Visibility is managed by album open callbacks (__disp_all_img_thumb_list,
     * __disp_select_img_thumb_list). Full teardown is done explicitly in
     * DISP_ALBUM_CLOSE before ai_ui_page_close() is called. */
    return OPRT_OK;
}

static OPERATE_RET __page_album_all_open(void *arg)
{
    ai_ui_image_album_view_all_open();
    return OPRT_OK;
}

static OPERATE_RET __page_album_all_close(void)
{
    ai_ui_image_album_view_all_close();
    return OPRT_OK;
}

static OPERATE_RET __page_album_select_open(void *arg)
{
    ai_ui_image_album_select_open();
    return OPRT_OK;
}

static OPERATE_RET __page_album_select_close(void)
{
    ai_ui_image_album_select_close();
    return OPRT_OK;
}
#endif

static void __page_register_all(void)
{
    AI_UI_PAGE_INTFS_T intfs;

    /* Chat */
    memset(&intfs, 0, sizeof(intfs));
    intfs.open   = __page_chat_open;
    intfs.close  = __page_chat_close;
    ai_ui_page_register(AI_UI_PAGE_CHAT, &intfs);

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    /* Camera */
    memset(&intfs, 0, sizeof(intfs));
    intfs.open  = __page_camera_open;
    intfs.close = __page_camera_close;
    ai_ui_page_register(AI_UI_PAGE_CAMERA, &intfs);
#endif


#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
    /* Album view (single image) — no page-specific close */
    memset(&intfs, 0, sizeof(intfs));
    intfs.open  = __page_album_view_open;
    intfs.close = __page_album_view_close;
    ai_ui_page_register(AI_UI_PAGE_ALBUM_VIEW, &intfs);

    /* Album all thumbnails */
    memset(&intfs, 0, sizeof(intfs));
    intfs.open  = __page_album_all_open;
    intfs.close = __page_album_all_close;
    ai_ui_page_register(AI_UI_PAGE_ALBUM_ALL, &intfs);

    /* Album select */
    memset(&intfs, 0, sizeof(intfs));
    intfs.open  = __page_album_select_open;
    intfs.close = __page_album_select_close;
    ai_ui_page_register(AI_UI_PAGE_ALBUM_SELECT, &intfs);
#endif

}

/**
 * @brief Handle UI display message based on message type.
 *
 * @param[in] msg_data Pointer to the message data structure.
 * @return none
 * @note Binary payloads (video flush, JPEG) use layouts documented per case; pointers
 *       passed to UI callbacks reference msg_data->data and are only valid for the
 *       duration of the callback.
 */
static void __ui_disp_msg_handle(AI_UI_MSG_T *msg_data)
{
    if (NULL == msg_data) {
        return;
    }

    switch (msg_data->type) {
        case AI_UI_DISP_USER_MSG: {
            if(sg_chat_intfs.disp_user_msg) {
                sg_chat_intfs.disp_user_msg(msg_data->data);
            }
        } break;
        case AI_UI_DISP_AI_MSG: {
            if(sg_chat_intfs.disp_ai_msg) {
                sg_chat_intfs.disp_ai_msg(msg_data->data);
            }
        }
        break;
        case AI_UI_DISP_AI_MSG_STREAM_START: {
            if(sg_chat_intfs.disp_ai_msg_stream_start) {
                sg_chat_intfs.disp_ai_msg_stream_start();
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
            if(sg_chat_intfs.disp_ai_msg_stream_data) {
                sg_chat_intfs.disp_ai_msg_stream_data(msg_data->data);
            }
#endif
        }
        break;
        case AI_UI_DISP_AI_MSG_STREAM_END: {
#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
            ai_ui_stream_text_end();
#else
            if(sg_chat_intfs.disp_ai_msg_stream_end) {
                sg_chat_intfs.disp_ai_msg_stream_end();
            }
#endif
        }
        break;
        case AI_UI_DISP_AI_MSG_STREAM_INTERRUPT: {
#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
            ai_ui_stream_text_end();
            ai_ui_stream_text_reset();
#else
        if(sg_chat_intfs.disp_ai_msg_stream_end) {
            sg_chat_intfs.disp_ai_msg_stream_end();
        }
#endif
        }
        break;
        case AI_UI_DISP_SYSTEM_MSG: {
            if(sg_chat_intfs.disp_system_msg) {
                sg_chat_intfs.disp_system_msg(msg_data->data);
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
        break;
        case AI_UI_DISP_CHAT_MODE: {
            if (sg_ui_intfs.disp_ai_chat_mode) {
                sg_ui_intfs.disp_ai_chat_mode(msg_data->data);
            }
        }
        break;

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
        case AI_UI_DISP_CAMERA_OPEN: {
            ai_ui_page_open(AI_UI_PAGE_CAMERA, NULL);
        }
        break;
        case AI_UI_DISP_CAMERA_FLUSH: {
            ai_ui_camera_flush((AI_UI_VIDEO_T *)msg_data->data);
        }
        break;
        case AI_UI_DISP_CAMERA_THUMB: {
            ai_ui_camera_set_thumbnail_jpeg((uint8_t *)msg_data->data, (uint32_t)msg_data->len);
        }
        break;
        case AI_UI_DISP_CAMERA_CLOSE: {
            ai_ui_page_close();
        }
        break;
#endif

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
        case AI_UI_DISP_USER_IMAGE_LINK: {
            if (sg_chat_intfs.disp_link && msg_data->data) {
                sg_chat_intfs.disp_link(FALSE, VIEW_IMAGE, __image_link_view_cb,
                                      msg_data->data, msg_data->len);
                PR_NOTICE("image link");
            }
        } break;

        case AI_UI_DISP_AI_IMAGE_LINK: {
            if (sg_chat_intfs.disp_link && msg_data->data) {
                sg_chat_intfs.disp_link(TRUE, VIEW_IMAGE, __image_link_view_cb,
                                      msg_data->data, msg_data->len);
                PR_NOTICE("image link");
            }
        } break;

        case AI_UI_DISP_ADD_CHAT_ATTACH_IMG: {
            AI_UI_IMG_T img = {0};
            ai_ui_image_album_get_img((char *)msg_data->data, &img);
            if (sg_chat_intfs.disp_add_chat_attch_img && img.data) {
                sg_chat_intfs.disp_add_chat_attch_img(&img);
            }
            if (img.data) {
                image_album_free_file_data(img.data);
            }
        } break;

        case AI_UI_DISP_CLEAR_CHAT_ATTACH:
            if (sg_chat_intfs.disp_clear_chat_attach) {
                sg_chat_intfs.disp_clear_chat_attach();
            }
            break;

        case AI_UI_DISP_ALBUM_OPEN:
            ai_ui_page_open(AI_UI_PAGE_ALBUM_VIEW, NULL);
            break;

        case AI_UI_DISP_ALBUM_VIEW_NEXT:
            ai_ui_image_album_view_next();
            break;

        case AI_UI_DISP_ALBUM_VIEW_PREV:
            ai_ui_image_album_view_prev();
            break;

        case AI_UI_DISP_ALBUM_VIEW_ALL:
            ai_ui_page_open(AI_UI_PAGE_ALBUM_ALL, NULL);
            break;

        case AI_UI_DISP_ALBUM_SELECT_IMG:
            ai_ui_page_open(AI_UI_PAGE_ALBUM_SELECT, NULL);
            break;

        case AI_UI_DISP_ALBUM_RELOAD:
            ai_ui_image_album_reload();
            break;

        case AI_UI_DISP_ALBUM_CLOSE:
            /* Explicit teardown before page_close so resources are freed
             * regardless of which album sub-page is currently active. */
            ai_ui_image_album_close();
            ai_ui_page_close();
            break;
#endif

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
        case AI_UI_DISP_PRINT_RESULT: {
            bool ok = (msg_data->len == sizeof(int32_t) &&
                       *(int32_t *)msg_data->data == OPRT_OK);
            ai_ui_image_album_show_print_result(ok);
            if (sg_chat_intfs.disp_print_result) {
                sg_chat_intfs.disp_print_result(ok);
            }
        } break;
#endif

        default:
            if (sg_ui_intfs.disp_other_msg) {
                sg_ui_intfs.disp_other_msg(msg_data->type, (uint8_t *)msg_data->data, msg_data->len);
            }
            break;
    }
}

/**
 * @brief AI chat UI task thread function.
 *
 * @param args Thread argument (unused).
 */
static void __ai_chat_ui_task(void *args)
{
    AI_UI_MSG_T msg_data = {0};

    (void)args;

    for (;;) {
        memset(&msg_data, 0, sizeof(AI_UI_MSG_T));
        tal_queue_fetch(sg_ui_queue_hdl, &msg_data, SEM_WAIT_FOREVER);

        __ui_disp_msg_handle(&msg_data);

        if (msg_data.sync_sem != NULL) {
            tal_semaphore_post(msg_data.sync_sem);
        }

        if (msg_data.data) {
            Free(msg_data.data);
        }
        msg_data.data = NULL;
    }
}

#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
/**
 * @brief Display stream text callback function.
 *
 * @param string Pointer to the text string to display, NULL indicates end of stream.
 */
static void __ai_chat_ui_stream_text_disp(char *string)
{
    if(NULL == string) {
        if(sg_chat_intfs.disp_ai_msg_stream_end) {
            sg_chat_intfs.disp_ai_msg_stream_end();
        }
    }else {
        if(sg_chat_intfs.disp_ai_msg_stream_data) {
            sg_chat_intfs.disp_ai_msg_stream_data(string);
        }
    }
}
#endif

/**
 * @brief Initialize AI UI module.
 *
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tal_queue_create_init(&sg_ui_queue_hdl, sizeof(AI_UI_MSG_T), 8));

    THREAD_CFG_T thrd_cfg;
    memset(&thrd_cfg, 0x00, sizeof(THREAD_CFG_T));
    thrd_cfg.thrdname = "ai_ui";
    thrd_cfg.priority = THREAD_PRIO_2;
    thrd_cfg.stackDepth = 1024 * 4;
    thrd_cfg.psram_mode = 1;

    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_ui_thrd_hdl, NULL, NULL, __ai_chat_ui_task, NULL, &thrd_cfg));

    TUYA_CALL_ERR_RETURN(__ai_ui_action_manage_init());

    __page_register_all();

#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
    TUYA_CALL_ERR_RETURN(ai_ui_stream_text_init(__ai_chat_ui_stream_text_disp));
#endif

    if (sg_ui_intfs.disp_init) {
        TUYA_CALL_ERR_RETURN(sg_ui_intfs.disp_init());
    }

    PR_DEBUG("ai chat ui init success");

    return OPRT_OK;
}

/**
 * @brief Display message on UI.
 *
 * @param tp Display type indicating the message category.
 * @param data Pointer to the message data.
 * @param len Length of the message data.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_disp_msg(AI_UI_DISP_TYPE_E tp, uint8_t *data, int len)
{
    AI_UI_MSG_T msg_data;

    memset(&msg_data, 0, sizeof(AI_UI_MSG_T));
    msg_data.type = tp;
    msg_data.len = len;
    if (len && data != NULL) {
        msg_data.data = (char *)Malloc(len + 1);
        if (NULL == msg_data.data) {
            return OPRT_MALLOC_FAILED;
        }
        memcpy(msg_data.data, data, len);
        msg_data.data[len] = 0; /* "\0" */
    } else {
        msg_data.data = NULL;
    }

    return tal_queue_post(sg_ui_queue_hdl, &msg_data, SEM_WAIT_FOREVER);
}

/**
 * @brief Display message on UI and block until the UI thread has finished dispatch.
 *
 * @param tp Display type indicating the message category.
 * @param data Pointer to the message data.
 * @param len Length of the message data.
 * @return OPERATE_RET Operation result code.
 * @note Uses a binary semaphore: the message is queued like ai_ui_disp_msg, then the
 *       caller waits until the UI worker posts after handling. Do not call from the
 *       UI worker thread (ai_ui) or deadlock will occur.
 */
OPERATE_RET ai_ui_disp_msg_sync(AI_UI_DISP_TYPE_E tp, uint8_t *data, int len)
{
    AI_UI_MSG_T msg_data;
    SEM_HANDLE  sync_sem = NULL;
    OPERATE_RET rt = OPRT_OK;

    memset(&msg_data, 0, sizeof(AI_UI_MSG_T));
    msg_data.type = tp;
    msg_data.len = len;
    if (len && data != NULL) {
        msg_data.data = (char *)Malloc(len + 1);
        if (NULL == msg_data.data) {
            return OPRT_MALLOC_FAILED;
        }
        memcpy(msg_data.data, data, len);
        msg_data.data[len] = 0; /* "\0" */
    }

    rt = tal_semaphore_create_init(&sync_sem, 0, 1);
    if (OPRT_OK != rt) {
        if (msg_data.data) {
            Free(msg_data.data);
        }
        return rt;
    }
    msg_data.sync_sem = sync_sem;

    rt = tal_queue_post(sg_ui_queue_hdl, &msg_data, SEM_WAIT_FOREVER);
    if (OPRT_OK != rt) {
        tal_semaphore_release(sync_sem);
        if (msg_data.data) {
            Free(msg_data.data);
        }
        return rt;
    }

    rt = tal_semaphore_wait_forever(sync_sem);
    tal_semaphore_release(sync_sem);

    return rt;
}

/**
 * @brief Register UI interface callbacks.
 *
 * @param intfs Pointer to the UI interface structure containing callback functions.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_register(AI_UI_INTFS_T *intfs)
{
    TUYA_CHECK_NULL_RETURN(intfs, OPRT_INVALID_PARM);

    memcpy(&sg_ui_intfs, intfs, sizeof(AI_UI_INTFS_T));

    return OPRT_OK;
}

/**
 * @brief Register chat display interface callbacks.
 *
 * @param intfs Pointer to the chat interface structure containing callback functions.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_chat_register(AI_UI_CHAT_INTFS_T *intfs)
{
    TUYA_CHECK_NULL_RETURN(intfs, OPRT_INVALID_PARM);

    memcpy(&sg_chat_intfs, intfs, sizeof(AI_UI_CHAT_INTFS_T));

    return OPRT_OK;
}

 /**
 * @brief Notify that an action menu item was touched (for internal use by UI implementation).
 *
 * @param action Action id: AI_UI_ACTION_TAKE_PHOTO, AI_UI_ACTION_IMAGE_RECOG, etc.
 */
 void ai_ui_notify_action(AI_UI_ACTION_E action, uint8_t *data, uint32_t len)
 {
    AI_UI_ACTION_MSG_T msg_data;

    msg_data.action = action;
    msg_data.len    = len;
    if (len && data != NULL) {
        msg_data.data = (uint8_t *)Malloc(len + 1);
        if (NULL == msg_data.data) {
            return;
        }
        memcpy(msg_data.data, data, len);
        msg_data.data[len] = '\0';
    } else {
        msg_data.data = NULL;
    }

    tal_queue_post(sg_action_queue_hdl, &msg_data, SEM_WAIT_FOREVER);
 }

 /**
 * @brief Notify that an action menu item was touched (for internal use by UI implementation).
 *
 * @param action Action id: AI_UI_ACTION_TAKE_PHOTO, AI_UI_ACTION_IMAGE_RECOG, etc.
 */
 void ai_ui_action_cb_register(AI_UI_ACTION_CB action_cb)
 {
    sg_action_cb = action_cb;
 }
