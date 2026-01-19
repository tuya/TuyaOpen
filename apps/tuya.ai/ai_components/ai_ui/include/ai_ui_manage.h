/**
 * @file ai_chat_ui_manage.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
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
// display network status
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
    AI_UI_DISP_SYS_MAX,
}AI_UI_DISP_TYPE_E;

typedef struct {
    OPERATE_RET (*disp_init)(void);
    void (*disp_user_msg)(char* string);
    void (*disp_ai_msg)(char* string);
    void (*disp_ai_msg_stream_start)(void);
    void (*disp_ai_msg_stream_data)(char *string);
    void (*disp_ai_msg_stream_end)(void);
    void (*disp_system_msg)(char *string);
    void (*disp_emotion)(char *emotion);
    void (*disp_ai_mode_state)(char *string);
    void (*disp_notification)(char *string);
    void (*disp_wifi_state)(AI_UI_WIFI_STATUS_E wifi_status);
    void (*disp_ai_chat_mode)(char *string);
    void (*disp_other_msg)(uint32_t type, uint8_t *data, int len );

    OPERATE_RET (*disp_camera_start)(uint16_t width, uint16_t height);
    OPERATE_RET (*disp_camera_flush)(uint8_t *data, uint16_t width, uint16_t height);
    OPERATE_RET (*disp_camera_end)(void);
}AI_UI_INTFS_T;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET ai_ui_register(AI_UI_INTFS_T *intfs);

OPERATE_RET ai_ui_init(void);

OPERATE_RET ai_ui_disp_msg(AI_UI_DISP_TYPE_E tp, uint8_t *data, int len);

OPERATE_RET ai_ui_camera_start(uint16_t width, uint16_t height);

OPERATE_RET ai_ui_camera_flush(uint8_t *data, uint16_t width, uint16_t height);

OPERATE_RET ai_ui_camera_end(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_CHAT_UI_MANAGE_H__ */
