/**
 * @file app_chat_bot.c
 * @brief app_chat_bot module is used to
 * @version 0.1
 * @date 2025-03-25
 */
#include "netmgr.h"

#include "tkl_wifi.h"
#include "tkl_gpio.h"
#include "tkl_memory.h"
#include "tal_api.h"
#include "tuya_ringbuf.h"

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
#include "tdl_button_manage.h"
#endif

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
#include "tdl_led_manage.h"
#endif

#include "app_display.h"
#include "ai_audio.h"
#include "app_chat_bot.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_AUDIO_TEXT_BUFF_LEN (1024)
#define AI_AUDIO_TEXT_SHOW_LEN (60 * 3)

typedef uint8_t APP_CHAT_MODE_E;
/*Press and hold button to start a single conversation.*/
#define APP_CHAT_MODE_KEY_PRESS_HOLD_SINGLE 0
/*Press the button once to start or stop the free conversation.*/
#define APP_CHAT_MODE_KEY_TRIG_VAD_FREE 1
/*Say the wake-up word to start a single conversation, similar to a smart speaker.
 *If no conversation is detected within 20 seconds, you need to say the wake-up word again*/
#define APP_CHAT_MODE_ASR_WAKEUP_SINGLE 2
/*Saying the wake-up word, you can have a free conversation.
 *If no conversation is detected within 20 seconds, you need to say the wake-up word again*/
#define APP_CHAT_MODE_ASR_WAKEUP_FREE 3

#define APP_CHAT_MODE_MAX 4
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    APP_CHAT_MODE_E mode;
    AI_AUDIO_WORK_MODE_E auido_mode;
    AI_AUDIO_ALERT_TYPE_E mode_alert;
    char *display_text;
    bool is_open;
} CHAT_WORK_MODE_INFO_T;

typedef struct {
    uint8_t is_enable;
    const CHAT_WORK_MODE_INFO_T *work;
} APP_CHAT_BOT_S;

/***********************************************************
***********************const declaration********************
***********************************************************/
const CHAT_WORK_MODE_INFO_T cAPP_WORK_HOLD = {
    .mode = APP_CHAT_MODE_KEY_PRESS_HOLD_SINGLE,
    .auido_mode = AI_AUDIO_MODE_MANUAL_SINGLE_TALK,
    .mode_alert = AI_AUDIO_ALERT_LONG_KEY_TALK,
    .display_text = HOLD_TALK,
    .is_open = true,
};

const CHAT_WORK_MODE_INFO_T cAPP_WORK_TRIG_VAD = {
    .mode = APP_CHAT_MODE_KEY_TRIG_VAD_FREE,
    .auido_mode = AI_AUDIO_WORK_VAD_FREE_TALK,
    .mode_alert = AI_AUDIO_ALERT_KEY_TALK,
    .display_text = TRIG_TALK,
    .is_open = false,
};

const CHAT_WORK_MODE_INFO_T cAPP_WORK_WAKEUP_SINGLE = {
    .mode = APP_CHAT_MODE_ASR_WAKEUP_SINGLE,
    .auido_mode = AI_AUDIO_WORK_ASR_WAKEUP_SINGLE_TALK,
    .mode_alert = AI_AUDIO_ALERT_WAKEUP_TALK,
    .display_text = WAKEUP_TALK,
    .is_open = true,
};

const CHAT_WORK_MODE_INFO_T cAPP_WORK_WAKEUP_FREE = {
    .mode = APP_CHAT_MODE_ASR_WAKEUP_FREE,
    .auido_mode = AI_AUDIO_WORK_ASR_WAKEUP_FREE_TALK,
    .mode_alert = AI_AUDIO_ALERT_FREE_TALK,
    .display_text = FREE_TALK,
    .is_open = true,
};

#if 0
const CHAT_WORK_MODE_INFO_T *cWORK_MODE_INFO_LIST[] = {
    &cAPP_WORK_HOLD,
    &cAPP_WORK_TRIG_VAD,
    &cAPP_WORK_WAKEUP_SINGLE,
    &cAPP_WORK_WAKEUP_FREE,
};
#endif
/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_CHAT_BOT_S sg_chat_bot = {
#if defined(ENABLE_CHAT_MODE_KEY_PRESS_HOLD_SINGEL) && (ENABLE_CHAT_MODE_KEY_PRESS_HOLD_SINGEL == 1)
    .work = &cAPP_WORK_HOLD,
#endif

#if defined(ENABLE_CHAT_MODE_KEY_TRIG_VAD_FREE) && (ENABLE_CHAT_MODE_KEY_TRIG_VAD_FREE == 1)
    .work = &cAPP_WORK_TRIG_VAD,
#endif

#if defined(ENABLE_CHAT_MODE_ASR_WAKEUP_SINGEL) && (ENABLE_CHAT_MODE_ASR_WAKEUP_SINGEL == 1)
    .work = &cAPP_WORK_WAKEUP_SINGLE,
#endif

#if defined(ENABLE_CHAT_MODE_ASR_WAKEUP_FREE) && (ENABLE_CHAT_MODE_ASR_WAKEUP_FREE == 1)
    .work = &cAPP_WORK_WAKEUP_FREE,
#endif

};

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
static TDL_LED_HANDLE_T sg_led_hdl = NULL;
#endif

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
static TDL_BUTTON_HANDLE sg_button_hdl = NULL;
#endif

/***********************************************************
***********************function define**********************
***********************************************************/
static void __app_ai_audio_evt_inform_cb(AI_AUDIO_EVENT_E event, uint8_t *data, uint32_t len, void *arg)
{
    static uint8_t *p_ai_text = NULL;
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
    static uint32_t ai_text_len = 0;
#endif

    switch (event) {
    case AI_AUDIO_EVT_HUMAN_ASR_TEXT: {
        if (len > 0 && data) {
// send asr text to display
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
            app_display_send_msg(TY_DISPLAY_TP_USER_MSG, data, len);
#endif
        }
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_START: {
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_START, data, len);
#else
        if (NULL == p_ai_text) {
            p_ai_text = tkl_system_psram_malloc(AI_AUDIO_TEXT_BUFF_LEN);
            if (NULL == p_ai_text) {
                return;
            }
        }

        ai_text_len = 0;
#endif
#endif
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_DATA: {
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_DATA, data, len);
#else
        memcpy(p_ai_text + ai_text_len, data, len);

        ai_text_len += len;
        if (ai_text_len >= AI_AUDIO_TEXT_SHOW_LEN) {
            app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG, p_ai_text, ai_text_len);
            ai_text_len = 0;
        }
#endif
#endif
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_END: {
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_END, data, len);
#else
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG, p_ai_text, ai_text_len);
        ai_text_len = 0;
#endif
#endif
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_EMO: {
        AI_AUDIO_EMOTION_T *emo;
        PR_DEBUG("---> AI_MSG_TYPE_EMOTION");
        emo = (AI_AUDIO_EMOTION_T *)data;
        if (emo) {
            if (emo->name) {
                PR_DEBUG("emotion name:%s", emo->name);
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
                app_display_send_msg(TY_DISPLAY_TP_EMOTION, (uint8_t *)emo->name, strlen(emo->name));
#endif
            }

            if (emo->text) {
                PR_DEBUG("emotion text:%s", emo->text);
            }
        }
    } break;
    case AI_AUDIO_EVT_ASR_WAKEUP: {
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
        TDL_LED_BLINK_CFG_T blink_cfg = {
            .cnt = 2,
            .start_stat = TDL_LED_ON,
            .end_stat = TDL_LED_OFF,
            .first_half_cycle_time = 100,
            .latter_half_cycle_time = 100,
        };

        tdl_led_blink(sg_led_hdl, &blink_cfg);
#endif

#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_END, data, len);
#endif
    } break;

    default:
        break;
    }

    return;
}

static void __app_ai_audio_state_inform_cb(AI_AUDIO_STATE_E state)
{

    PR_DEBUG("ai audio state: %d", state);

    switch (state) {
    case AI_AUDIO_STATE_STANDBY:

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
        tdl_led_set_status(sg_led_hdl, TDL_LED_OFF);
#endif

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        app_display_send_msg(TY_DISPLAY_TP_EMOTION, (uint8_t *)"NATURAL", strlen("NATURAL"));
        app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)STANDBY, strlen(STANDBY));
#endif
        break;
    case AI_AUDIO_STATE_LISTEN:
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
        tdl_led_set_status(sg_led_hdl, TDL_LED_ON);
#endif

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)LISTENING, strlen(LISTENING));
#endif
    case AI_AUDIO_STATE_UPLOAD:

        break;
    case AI_AUDIO_STATE_AI_SPEAK:
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)SPEAKING, strlen(SPEAKING));
#endif

        break;
    }
}

static OPERATE_RET __app_chat_bot_enable(uint8_t enable)
{
    if (sg_chat_bot.is_enable == enable) {
        PR_DEBUG("chat bot enable is already %s", enable ? "enable" : "disable");
        return OPRT_OK;
    }

    PR_DEBUG("chat bot enable set %s", enable ? "enable" : "disable");

    ai_audio_set_open(enable);

    sg_chat_bot.is_enable = enable;

    return OPRT_OK;
}

uint8_t app_chat_bot_get_enable(void)
{
    return sg_chat_bot.is_enable;
}

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
static void __app_button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    APP_CHAT_MODE_E work_mode = sg_chat_bot.work->mode;
    PR_DEBUG("app button function cb, work mode: %d", work_mode);

    // network status
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    if (status == NETMGR_LINK_DOWN) {
        PR_DEBUG("network is down, ignore button event");
        if (ai_audio_player_is_playing()) {
            return;
        }
        ai_audio_player_play_alert(AI_AUDIO_ALERT_NOT_ACTIVE);
        return;
    }

    switch (event) {
    case TDL_BUTTON_PRESS_DOWN: {
        if (work_mode == APP_CHAT_MODE_KEY_PRESS_HOLD_SINGLE) {
            PR_DEBUG("button press down, listen start");
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
            tdl_led_set_status(sg_led_hdl, TDL_LED_ON);
#endif
            ai_audio_manual_start_single_talk();
        }
    } break;
    case TDL_BUTTON_PRESS_UP: {
        if (work_mode == APP_CHAT_MODE_KEY_PRESS_HOLD_SINGLE) {
            PR_DEBUG("button press up, listen end");
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
            tdl_led_set_status(sg_led_hdl, TDL_LED_OFF);
#endif
            ai_audio_manual_stop_single_talk();
        }
    } break;
    case TDL_BUTTON_PRESS_SINGLE_CLICK: {
        if (work_mode == APP_CHAT_MODE_KEY_PRESS_HOLD_SINGLE) {
            break;
        }

        if (sg_chat_bot.is_enable) {
            ai_audio_set_wakeup();
        } else {
            __app_chat_bot_enable(true);
        }
        PR_DEBUG("button single click");
    } break;
    default:
        break;
    }
}

static OPERATE_RET __app_open_button(void)
{
    OPERATE_RET rt = OPRT_OK;

    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 500};
    TUYA_CALL_ERR_RETURN(tdl_button_create(BUTTON_NAME, &button_cfg, &sg_button_hdl));

    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_DOWN, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_UP, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_SINGLE_CLICK, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_DOUBLE_CLICK, __app_button_function_cb);

    return rt;
}
#endif

OPERATE_RET app_chat_bot_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_AUDIO_CONFIG_T ai_audio_cfg;

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
    app_display_init();
#endif

    ai_audio_cfg.work_mode = sg_chat_bot.work->auido_mode;
    ai_audio_cfg.evt_inform_cb = __app_ai_audio_evt_inform_cb;
    ai_audio_cfg.state_inform_cb = __app_ai_audio_state_inform_cb;

    TUYA_CALL_ERR_RETURN(ai_audio_init(&ai_audio_cfg));

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
    TUYA_CALL_ERR_RETURN(__app_open_button());
#endif

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    sg_led_hdl = tdl_led_find_dev(LED_NAME);
    TUYA_CALL_ERR_RETURN(tdl_led_open(sg_led_hdl));
#endif

    __app_chat_bot_enable(sg_chat_bot.work->is_open);

    PR_NOTICE("work:%s", sg_chat_bot.work->display_text);

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
    app_display_send_msg(TY_DISPLAY_TP_CHAT_MODE, (uint8_t *)sg_chat_bot.work->display_text,
                         strlen(sg_chat_bot.work->display_text));
#endif
    return OPRT_OK;
}
