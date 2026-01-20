/**
 * @file ai_chat_ui.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
#include "ai_ui_manage.h"
#include "lang_config.h"

#if defined(ENABLE_AI_CHAT_GUI_WECHAT) && (ENABLE_AI_CHAT_GUI_WECHAT == 1)
#include "ai_ui_chat_wechat.h"
#elif defined(ENABLE_AI_CHAT_GUI_CHATBOT) && (ENABLE_AI_CHAT_GUI_CHATBOT == 1)
#include "ai_ui_chat_chatbot.h"
#elif defined(ENABLE_AI_CHAT_GUI_OLED) && (ENABLE_AI_CHAT_GUI_OLED == 1)
#include "ai_ui_chat_oled.h"
#endif

#include "ai_chat_main.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/
static void __ai_chat_disp_mode_state(AI_MODE_STATE_E state)
{
    switch (state) {
    case AI_MODE_STATE_INIT:
    case AI_MODE_STATE_IDLE:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_NEUTRAL, strlen(EMOJI_NEUTRAL));
        ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)STANDBY, strlen(STANDBY));
        break;
    case AI_MODE_STATE_LISTEN:
        ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)LISTENING, strlen(LISTENING));
        break;
    case AI_MODE_STATE_SPEAK:
        ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)SPEAKING, strlen(SPEAKING));
        break;
    default:
        break;
    }
}

void ai_chat_ui_handle_event(AI_NOTIFY_EVENT_T *event)
{
    AI_NOTIFY_TEXT_T *text = NULL;

    if(NULL == event) {
        return;
    }

    switch(event->type) {
        case AI_USER_EVT_ASR_OK:{
        text = (AI_NOTIFY_TEXT_T *)event->data;

        if(text && text->datalen > 0 && text->data) {
            ai_ui_disp_msg(AI_UI_DISP_USER_MSG, (uint8_t *)text->data, text->datalen);
        }
    }
    break;
    case AI_USER_EVT_TEXT_STREAM_START:{
        ai_ui_disp_msg(AI_UI_DISP_AI_MSG_STREAM_START, NULL, 0);

        text = (AI_NOTIFY_TEXT_T *)event->data;
        if(text && text->datalen > 0 && text->data) {
            ai_ui_disp_msg(AI_UI_DISP_AI_MSG_STREAM_DATA, (uint8_t *)text->data, text->datalen);
        }
    }
    break;
    case AI_USER_EVT_TEXT_STREAM_DATA:{
        text = (AI_NOTIFY_TEXT_T *)event->data;
        if(text && text->datalen > 0 && text->data) {
            ai_ui_disp_msg(AI_UI_DISP_AI_MSG_STREAM_DATA, (uint8_t *)text->data, text->datalen);
        }
    }
    break;
    case AI_USER_EVT_TEXT_STREAM_STOP:{
        text = (AI_NOTIFY_TEXT_T *)event->data;
        if(text && text->datalen > 0 && text->data) {
            ai_ui_disp_msg(AI_UI_DISP_AI_MSG_STREAM_DATA, (uint8_t *)text->data, text->datalen);
        }

        ai_ui_disp_msg(AI_UI_DISP_AI_MSG_STREAM_END, NULL, 0);
    }
    break;
    case AI_USER_EVT_CHAT_BREAK:{
        ai_ui_disp_msg(AI_UI_DISP_AI_MSG_STREAM_INTERRUPT, NULL, 0);
    }
    break;
    case AI_USER_EVT_LLM_EMOTION:
    case AI_USER_EVT_EMOTION:{
        AI_NOTIFY_EMO_T *emo = (AI_NOTIFY_EMO_T *)(event->data);

        if(emo) {
            PR_NOTICE("emoji: %s, name: %s", emo->emoji, emo->name);
            ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)emo->name, strlen(emo->name));
        }
    }
    break;
    case AI_USER_EVT_MODE_STATE_UPDATE:{
        AI_MODE_STATE_E state = (AI_MODE_STATE_E )(event->data);
        __ai_chat_disp_mode_state(state);
    }
    break;
    case AI_USER_EVT_MODE_SWITCH:{
        AI_CHAT_MODE_E mode = (AI_CHAT_MODE_E)(event->data);
        char *name  = ai_get_mode_name_str(mode);
        if(NULL == name) {
            PR_NOTICE("mode name str is null");
            break;
        }

        ai_ui_disp_msg(AI_UI_DISP_CHAT_MODE, (uint8_t *)name, strlen(name));
    }
    break;
    case AI_USER_EVT_VEDIO_DISPLAY_START:{
        AI_NOTIFY_VIDEO_START_T *video_start = (AI_NOTIFY_VIDEO_START_T *)(event->data);
        if(NULL == video_start) {
            PR_ERR("video start param is null");
            break;
        }

        ai_ui_camera_start(video_start->camera_width, video_start->camera_height);
    }
    break;
    case AI_USER_EVT_VEDIO_DISPLAY_END:
        ai_ui_camera_end();
    break;
    default:
        break;
    }

}

OPERATE_RET ai_chat_ui_init(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_AI_CHAT_CUSTOM_UI) && (ENABLE_AI_CHAT_CUSTOM_UI == 1)
    PR_NOTICE("use custom ai chat ui, need register ui by user");
#else
    #if defined(ENABLE_AI_CHAT_GUI_WECHAT) && (ENABLE_AI_CHAT_GUI_WECHAT == 1)
        TUYA_CALL_ERR_RETURN(ai_ui_chat_wechat_register());
    #elif defined(ENABLE_AI_CHAT_GUI_CHATBOT) && (ENABLE_AI_CHAT_GUI_CHATBOT == 1)
        TUYA_CALL_ERR_RETURN(ai_ui_chat_chatbot_register());
    #elif defined(ENABLE_AI_CHAT_GUI_OLED) && (ENABLE_AI_CHAT_GUI_OLED == 1)
        TUYA_CALL_ERR_RETURN(ai_ui_chat_oled_register());
    #else
        #error "please select ai chat present ui"
    #endif
#endif

    TUYA_CALL_ERR_RETURN(ai_ui_init());

    return rt;
}
#endif