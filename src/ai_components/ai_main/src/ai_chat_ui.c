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
#elif defined(ENABLE_AI_CHAT_GUI_GUIDED_WECHAT) && (ENABLE_AI_CHAT_GUI_GUIDED_WECHAT == 1)
#include "ai_ui_chat_guided_wechat.h"
#include "ai_ui_locale.h"
#endif

#include "ai_chat_main.h"

static AI_MODE_STATE_E sg_last_mode_state = AI_MODE_STATE_INIT;
static AI_CHAT_MODE_E  sg_last_chat_mode;
static bool            sg_last_mode_state_valid = false;
static bool            sg_last_chat_mode_valid = false;

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
    const char *status;

    sg_last_mode_state = state;
    sg_last_mode_state_valid = true;

    switch (state) {
    case AI_MODE_STATE_INIT:
    case AI_MODE_STATE_IDLE:
        ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)EMOJI_NEUTRAL, strlen(EMOJI_NEUTRAL));
#if defined(ENABLE_AI_CHAT_GUI_GUIDED_WECHAT) && (ENABLE_AI_CHAT_GUI_GUIDED_WECHAT == 1)
        status = ai_ui_locale_tr(STANDBY, "Standby");
#else
        status = STANDBY;
#endif
        ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)status, strlen(status));
        break;
    case AI_MODE_STATE_LISTEN:
#if defined(ENABLE_AI_CHAT_GUI_GUIDED_WECHAT) && (ENABLE_AI_CHAT_GUI_GUIDED_WECHAT == 1)
        status = ai_ui_locale_tr(LISTENING, "Listening");
#else
        status = LISTENING;
#endif
        ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)status, strlen(status));
        break;
    case AI_MODE_STATE_SPEAK:
#if defined(ENABLE_AI_CHAT_GUI_GUIDED_WECHAT) && (ENABLE_AI_CHAT_GUI_GUIDED_WECHAT == 1)
        status = ai_ui_locale_tr(SPEAKING, "Speaking");
#else
        status = SPEAKING;
#endif
        ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)status, strlen(status));
        break;
    case AI_MODE_STATE_UPLOAD:
#if defined(ENABLE_AI_CHAT_GUI_GUIDED_WECHAT) && (ENABLE_AI_CHAT_GUI_GUIDED_WECHAT == 1)
        status = ai_ui_locale_tr(UPLOADING, "Uploading");
#else
        status = UPLOADING;
#endif
        ai_ui_disp_msg(AI_UI_DISP_STATUS, (uint8_t *)status, strlen(status));
        break;
    default:
        break;
    }
}

void ai_chat_ui_handle_event(AI_NOTIFY_EVENT_T *event)
{
    AI_NOTIFY_TEXT_T *text = NULL;

    if (NULL == event) {
        return;
    }

    switch (event->type) {
    case AI_USER_EVT_ASR_OK: {
        text = (AI_NOTIFY_TEXT_T *)event->data;

        if (text && text->datalen > 0 && text->data) {
            ai_ui_disp_msg(AI_UI_DISP_USER_MSG, (uint8_t *)text->data, text->datalen);
        }
    } break;
    case AI_USER_EVT_TEXT_STREAM_START: {
        ai_ui_disp_msg(AI_UI_DISP_AI_MSG_STREAM_START, NULL, 0);

        text = (AI_NOTIFY_TEXT_T *)event->data;
        if (text && text->datalen > 0 && text->data) {
            ai_ui_disp_msg(AI_UI_DISP_AI_MSG_STREAM_DATA, (uint8_t *)text->data, text->datalen);
        }
    } break;
    case AI_USER_EVT_TEXT_STREAM_DATA: {
        text = (AI_NOTIFY_TEXT_T *)event->data;
        if (text && text->datalen > 0 && text->data) {
            ai_ui_disp_msg(AI_UI_DISP_AI_MSG_STREAM_DATA, (uint8_t *)text->data, text->datalen);
        }
    } break;
    case AI_USER_EVT_TEXT_STREAM_STOP: {
        text = (AI_NOTIFY_TEXT_T *)event->data;
        if (text && text->datalen > 0 && text->data) {
            ai_ui_disp_msg(AI_UI_DISP_AI_MSG_STREAM_DATA, (uint8_t *)text->data, text->datalen);
        }

        ai_ui_disp_msg(AI_UI_DISP_AI_MSG_STREAM_END, NULL, 0);
    } break;
    case AI_USER_EVT_CHAT_BREAK: {
        ai_ui_disp_msg(AI_UI_DISP_AI_MSG_STREAM_INTERRUPT, NULL, 0);
    } break;
    case AI_USER_EVT_LLM_EMOTION:
    case AI_USER_EVT_EMOTION: {
        AI_NOTIFY_EMO_T *emo = (AI_NOTIFY_EMO_T *)(event->data);

        if (emo) {
            PR_NOTICE("emoji: %s, name: %s", emo->emoji, emo->name);
            ai_ui_disp_msg(AI_UI_DISP_EMOTION, (uint8_t *)emo->name, strlen(emo->name));
        }
    } break;
    case AI_USER_EVT_MODE_STATE_UPDATE: {
        AI_MODE_STATE_E state = (AI_MODE_STATE_E)(event->data);
        __ai_chat_disp_mode_state(state);
    } break;
    case AI_USER_EVT_MODE_SWITCH: {
        AI_CHAT_MODE_E mode = (AI_CHAT_MODE_E)(event->data);
        char          *name = ai_get_mode_name_str(mode);
        const char    *localized_name = name;

        sg_last_chat_mode = mode;
        sg_last_chat_mode_valid = true;
        if (NULL == name) {
            PR_NOTICE("mode name str is null");
            break;
        }

#if defined(ENABLE_AI_CHAT_GUI_GUIDED_WECHAT) && (ENABLE_AI_CHAT_GUI_GUIDED_WECHAT == 1)
        if (ai_ui_locale_is_english()) {
            switch (mode) {
            case AI_CHAT_MODE_HOLD:
                localized_name = "Hold to talk";
                break;
            case AI_CHAT_MODE_ONE_SHOT:
                localized_name = "Push to talk";
                break;
            case AI_CHAT_MODE_WAKEUP:
                localized_name = "Wake word";
                break;
            case AI_CHAT_MODE_FREE:
                localized_name = "Free talk";
                break;
            default:
                break;
            }
        }
#endif
        ai_ui_disp_msg(AI_UI_DISP_CHAT_MODE, (uint8_t *)localized_name, strlen(localized_name));
    } break;

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
    case AI_USER_EVT_GENERATE_PICTURE: 
    case AI_USER_EVT_GET_PICTURE_FROM_APP: {
        ai_ui_disp_msg(AI_UI_DISP_AI_IMAGE_LINK, (uint8_t *)(event->data), strlen((char *)(event->data)));
    } break;

    case AI_USER_EVT_SEND_PICTURE_END: {
        ai_ui_disp_msg(AI_UI_DISP_CLEAR_CHAT_ATTACH, NULL, 0);
    } break;
#endif

    default:
        break;
    }
}

void ai_chat_ui_refresh_locale(void)
{
    if (sg_last_mode_state_valid) {
        __ai_chat_disp_mode_state(sg_last_mode_state);
    }

    if (sg_last_chat_mode_valid) {
        AI_NOTIFY_EVENT_T mode_event = {
            .type = AI_USER_EVT_MODE_SWITCH,
            .data = (void *)(uintptr_t)sg_last_chat_mode,
        };
        ai_chat_ui_handle_event(&mode_event);
    }
}

OPERATE_RET ai_chat_ui_init(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_AI_CHAT_CUSTOM_UI) && (ENABLE_AI_CHAT_CUSTOM_UI == 1)
    PR_NOTICE("use custom ai chat ui, need register ui by user");
#else

#if defined(ENABLE_AI_CHAT_GUI_GUIDED_WECHAT) && (ENABLE_AI_CHAT_GUI_GUIDED_WECHAT == 1)
    ai_ui_locale_load();
#endif

#if defined(ENABLE_AI_CHAT_GUI_WECHAT) && (ENABLE_AI_CHAT_GUI_WECHAT == 1)
    TUYA_CALL_ERR_RETURN(ai_ui_chat_wechat_register());
#elif defined(ENABLE_AI_CHAT_GUI_CHATBOT) && (ENABLE_AI_CHAT_GUI_CHATBOT == 1)
    TUYA_CALL_ERR_RETURN(ai_ui_chat_chatbot_register());
#elif defined(ENABLE_AI_CHAT_GUI_OLED) && (ENABLE_AI_CHAT_GUI_OLED == 1)
    TUYA_CALL_ERR_RETURN(ai_ui_chat_oled_register());
#elif defined(ENABLE_AI_CHAT_GUI_GUIDED_WECHAT) && (ENABLE_AI_CHAT_GUI_GUIDED_WECHAT == 1)
    TUYA_CALL_ERR_RETURN(ai_ui_chat_guided_wechat_register());
#else
#error "please select ai chat present ui"
#endif
#endif

    TUYA_CALL_ERR_RETURN(ai_ui_init());

    return rt;
}
#endif
