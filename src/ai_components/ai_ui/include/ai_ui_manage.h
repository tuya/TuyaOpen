/**
 * @file ai_ui_manage.h
 * @brief AI UI management interface definitions.
 *
 * This header provides function declarations and type definitions for managing
 * AI user interface, including display interfaces for messages, emotions, status,
 * camera, and pictures.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_CHAT_UI_MANAGE_H__
#define __AI_CHAT_UI_MANAGE_H__

#include "tuya_cloud_types.h"
#include "ai_user_event.h"
#include "lang_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
/* Display network status */
typedef uint8_t AI_UI_WIFI_STATUS_E;
#define AI_UI_WIFI_STATUS_DISCONNECTED 0
#define AI_UI_WIFI_STATUS_GOOD         1
#define AI_UI_WIFI_STATUS_FAIR         2
#define AI_UI_WIFI_STATUS_WEAK         3

typedef enum {
    AI_UI_DISP_USER_MSG,
    AI_UI_DISP_AI_MSG,
    AI_UI_DISP_AI_MSG_STREAM_START,
    AI_UI_DISP_AI_MSG_STREAM_DATA,
    AI_UI_DISP_AI_MSG_STREAM_END,
    AI_UI_DISP_AI_MSG_STREAM_INTERRUPT,

    AI_UI_DISP_SYSTEM_MSG,
    AI_UI_DISP_EMOTION,
    AI_UI_DISP_STATUS,
    AI_UI_DISP_NOTIFICATION,
    AI_UI_DISP_NETWORK,
    AI_UI_DISP_CHAT_MODE,

    AI_UI_DISP_CAMERA_OPEN,
    AI_UI_DISP_CAMERA_FLUSH,
    AI_UI_DISP_CAMERA_THUMB,
    AI_UI_DISP_CAMERA_CLOSE,

    AI_UI_DISP_USER_IMAGE_LINK,
    AI_UI_DISP_AI_IMAGE_LINK,
    AI_UI_DISP_ADD_CHAT_ATTACH_IMG,
    AI_UI_DISP_CLEAR_CHAT_ATTACH,

    AI_UI_DISP_ALBUM_OPEN,
    AI_UI_DISP_ALBUM_VIEW_NEXT,
    AI_UI_DISP_ALBUM_VIEW_PREV,
    AI_UI_DISP_ALBUM_VIEW_ALL,
    AI_UI_DISP_ALBUM_SELECT_IMG,
    AI_UI_DISP_ALBUM_RELOAD,
    AI_UI_DISP_ALBUM_CLOSE,

    AI_UI_DISP_PRINT_RESULT,

    AI_UI_DISP_SYS_MAX,
}AI_UI_DISP_TYPE_E;

typedef enum {
    AI_UI_ACT_OPEN_CAMERA,
    AI_UI_ACT_TAKE_PHOTO,
    AI_UI_ACT_CLOSE_CAMER,
    AI_UI_ACT_CAMERA_AI_ON,
    AI_UI_ACT_CAMERA_AI_OFF,

    AI_UI_ACT_OPEN_ALBUM,
    AI_UI_ACT_VIEW_PREV_IMG,
    AI_UI_ACT_VIEW_NEXT_IMG,
    AI_UI_ACT_VIEW_ALL_IMG,
    AI_UI_ACT_DELETE_IMG,
    AI_UI_ACT_BATCH_DELETE_IMG,
    AI_UI_ACT_CLOSE_ALBUM,
    AI_UI_ACT_OPEN_IMG_ATTACH_LIST,
    AI_UI_ACT_ADD_IMG_ATTACH,
    AI_UI_ACT_DEL_IMG_ATTACH,
    
    AI_UI_ACT_PRINT_IMG,

    AI_UI_ACT_MAX
} AI_UI_ACTION_E;

#define AI_UI_BATCH_DELETE_NAME_LEN  64   /* matches ALBUM_FILENAME_MAX_LEN */
#define AI_UI_BATCH_DELETE_MAX       8

typedef struct {
    uint32_t count;
    char     names[AI_UI_BATCH_DELETE_MAX][AI_UI_BATCH_DELETE_NAME_LEN + 1];
} AI_UI_BATCH_DELETE_T;

typedef struct {
    char    *name;
    uint16_t width;
    uint16_t height;
    uint8_t *data;
    uint32_t len;
} AI_UI_IMG_T;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t *yuv422;
    uint32_t len;
} AI_UI_VIDEO_T;

typedef struct {
    void (*disp_open)(void);
    void (*disp_image)(AI_UI_IMG_T *img);
    void (*disp_all_img_thumb_list)(AI_UI_IMG_T *item_arr, uint32_t arr_cnt);
    void (*disp_select_img_thumb_list)(AI_UI_IMG_T *item_arr, uint32_t arr_cnt, uint8_t select_num_max);
    void (*disp_close)(void);
#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    void (*disp_print_result)(bool ok);
#endif
} AI_UI_ALBUM_INTFS_T;

typedef struct {
    void (*disp_open)(void);
    void (*disp_yuv_flush)(AI_UI_VIDEO_T *video);
    void (*disp_set_thumbnail_jpeg)(uint8_t *jpeg, uint32_t len);
    void (*disp_close)(void);
} AI_UI_CAMERA_INTFS_T;

typedef void (*AI_UI_CHAT_LINK_CB)(void *arg);

typedef struct {
    void (*disp_open)(void);
    void (*disp_close)(void);
    void (*disp_user_msg)(char *string);
    void (*disp_ai_msg)(char *string);
    void (*disp_ai_msg_stream_start)(void);
    void (*disp_ai_msg_stream_data)(char *string);
    void (*disp_ai_msg_stream_end)(void);
    void (*disp_system_msg)(char *string);
    void (*disp_image)(AI_UI_IMG_T *img);
    void (*disp_link)(bool is_ai, char *text, AI_UI_CHAT_LINK_CB cb, void *cb_arg, uint32_t len);
    void (*disp_add_chat_attch_img)(AI_UI_IMG_T *img);
    void (*disp_clear_chat_attach)(void);
#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    void (*disp_print_result)(bool ok);
#endif
} AI_UI_CHAT_INTFS_T;

typedef struct {
    OPERATE_RET (*disp_init)(void);
    void (*disp_emotion)(char *emotion);
    void (*disp_ai_mode_state)(char *string);
    void (*disp_notification)(char *string);
    void (*disp_wifi_state)(AI_UI_WIFI_STATUS_E wifi_status);
    void (*disp_ai_chat_mode)(char *string);
    void (*disp_other_msg)(uint32_t type, uint8_t *data, int len);
}AI_UI_INTFS_T;

typedef void (*AI_UI_ACTION_CB)(AI_UI_ACTION_E action, uint8_t *data, uint32_t len);

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Register UI interface callbacks.
 *
 * @param intfs Pointer to the UI interface structure containing callback functions.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_register(AI_UI_INTFS_T *intfs);

OPERATE_RET ai_ui_chat_register(AI_UI_CHAT_INTFS_T *intfs);

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
OPERATE_RET ai_ui_image_album_register(AI_UI_ALBUM_INTFS_T *intfs);
#endif

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
OPERATE_RET ai_ui_camera_register(AI_UI_CAMERA_INTFS_T *intfs);
#endif

/**
 * @brief Initialize AI UI module.
 *
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_init(void);

/**
 * @brief Display message on UI.
 *
 * @param tp Display type indicating the message category.
 * @param data Pointer to the message data.
 * @param len Length of the message data.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_disp_msg(AI_UI_DISP_TYPE_E tp, uint8_t *data, int len);

/**
 * @brief Display message on UI and block until the UI thread has finished dispatch.
 *
 * @param tp Display type indicating the message category.
 * @param data Pointer to the message data.
 * @param len Length of the message data.
 * @return OPERATE_RET Operation result code.
 * @note Synchronizes via a semaphore after queueing; registered callbacks run on the UI
 *       worker thread. Do not call from that same thread or deadlock will occur.
 */
OPERATE_RET ai_ui_disp_msg_sync(AI_UI_DISP_TYPE_E tp, uint8_t *data, int len);

 /**
 * @brief Notify that an action menu item was touched (for internal use by UI implementation).
 *
 * @param action Action id: AI_UI_ACTION_TAKE_PHOTO, AI_UI_ACTION_IMAGE_RECOG, etc.
 */
 void ai_ui_notify_action(AI_UI_ACTION_E action, uint8_t *data, uint32_t len);

/**
 * @brief Notify that an action menu item was touched (for internal use by UI implementation).
 *
 * @param action Action id: AI_UI_ACTION_TAKE_PHOTO, AI_UI_ACTION_IMAGE_RECOG, etc.
 */
 void ai_ui_action_cb_register(AI_UI_ACTION_CB action_cb);


#ifdef __cplusplus
}
#endif

#endif /* __AI_CHAT_UI_MANAGE_H__ */
