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
#include "media_src_zh.h"
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
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
#if !defined(ENABLE_GUI_STREAM_AI_TEXT) || (ENABLE_GUI_STREAM_AI_TEXT != 1)
    static uint8_t *p_ai_text = NULL;
    static uint32_t ai_text_len = 0;
#endif
#endif

    switch (event) {
    case AI_AUDIO_EVT_HUMAN_ASR_TEXT: {
        if (len > 0 && data) {
// send asr text to display
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
            app_display_send_msg(TY_DISPLAY_TP_USER_MSG, data, len);
#else
            // Ubuntu console logging
            PR_NOTICE("USER: %.*s", (int)len, data);
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
#else
        // Ubuntu console logging - AI response start
        PR_NOTICE("AI: ", len);
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
#else
        PR_NOTICE("AI: %.*s", len, data);
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
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_INTERUPT: {
#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_INTERRUPT, NULL, 0);
#else
        PR_WARN("AI response interrupted");
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
        ai_audio_player_stop();
        ai_audio_player_play_alert(AI_AUDIO_ALERT_WAKEUP);

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
        app_display_send_msg(TY_DISPLAY_TP_EMOTION, (uint8_t *)EMOJI_NEUTRAL, strlen(EMOJI_NEUTRAL));
        app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)STANDBY, strlen(STANDBY));
#else
        PR_NOTICE("State: STANDBY (Ready for next conversation)");
#endif
        break;
    case AI_AUDIO_STATE_LISTEN:
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
        tdl_led_set_status(sg_led_hdl, TDL_LED_ON);
#endif

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)LISTENING, strlen(LISTENING));
#else
        PR_NOTICE("State: LISTENING (Recording audio...)");
#endif
        break;
    case AI_AUDIO_STATE_UPLOAD:
#if !defined(ENABLE_CHAT_DISPLAY) || (ENABLE_CHAT_DISPLAY != 1)
        PR_NOTICE("State: UPLOAD (Sending to cloud...)");
#endif
        break;
    case AI_AUDIO_STATE_AI_SPEAK:
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)SPEAKING, strlen(SPEAKING));
#else
        PR_NOTICE("State: AI_SPEAKING (Playing response...)");
#endif

        break;

    default:
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
            ai_audio_player_stop();
            ai_audio_player_play_alert(AI_AUDIO_ALERT_WAKEUP);
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

#if defined(ENABLE_KEYBOARD_INPUT) && (ENABLE_KEYBOARD_INPUT == 1)
#include "tuya_ai_client.h"

// State tracking for keyboard (simulates press/hold behavior)
static bool s_keyboard_listening = false;

/**
 * @brief Keyboard event handler
 *
 * Handles keyboard events from the board layer.
 * Maps keyboard keys to chatbot functionality:
 * - S: Start listening / Trigger wakeup
 * - X: Stop listening
 * - V: Volume up
 * - D: Volume down
 * - Q: Quit (handled in keyboard_input.c)
 */
void app_chat_bot_keyboard_event_handler(KEYBOARD_EVENT_E event)
{
    APP_CHAT_MODE_E work_mode = sg_chat_bot.work->mode;

    switch (event) {
    case KEYBOARD_EVENT_PRESS_S: {
        PR_DEBUG("Keyboard 'S' pressed, work_mode: %d", work_mode);

        // Check if AI client is ready
        if (!tuya_ai_client_is_ready()) {
            PR_WARN("AI client not ready, please wait for connection");
            if (!ai_audio_player_is_playing()) {
                ai_audio_player_play_alert(AI_AUDIO_ALERT_NOT_ACTIVE);
            }
            return;
        }

        // Handle based on work mode (same logic as button)
        if (work_mode == APP_CHAT_MODE_KEY_PRESS_HOLD_SINGLE) {
            if (ai_audio_player_is_playing()) {
                ai_audio_player_stop();
            }

            // Stop current chat/conversation
            ai_audio_manual_stop_single_talk();
            ai_audio_cloud_asr_stop();
            ai_audio_agent_upload_stop();

            ai_audio_player_play_alert(AI_AUDIO_ALERT_WAKEUP);
            ai_audio_manual_start_single_talk();

            // Start listening (simulates button press down)
            PR_NOTICE("Keyboard: Start listening");
            s_keyboard_listening = true;
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
            tdl_led_set_status(sg_led_hdl, TDL_LED_ON);
#endif
        }
        break;
    }

    case KEYBOARD_EVENT_PRESS_X: {
        PR_DEBUG("Keyboard 'X' pressed, work_mode: %d", work_mode);

        if (work_mode == APP_CHAT_MODE_KEY_PRESS_HOLD_SINGLE) {
            // Stop listening (simulates button press up)
            if (s_keyboard_listening) {
                PR_NOTICE("Keyboard: Stop listening");
                s_keyboard_listening = false;
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
                tdl_led_set_status(sg_led_hdl, TDL_LED_OFF);
#endif
                ai_audio_manual_stop_single_talk();
            } else {
                PR_WARN("Not currently listening, 'X' ignored");
            }
        }
        // For other modes, 'X' does nothing
        break;
    }

    case KEYBOARD_EVENT_PRESS_V: {
        // Volume up
        uint8_t volume = ai_audio_get_volume();
        if (volume < 100) {
            volume = (volume + 10 > 100) ? 100 : volume + 10;
            ai_audio_set_volume(volume);
            PR_NOTICE("Volume increased to %d%%", volume);
        }
        break;
    }

    case KEYBOARD_EVENT_PRESS_D: {
        // Volume down
        uint8_t volume = ai_audio_get_volume();
        if (volume > 0) {
            volume = (volume < 10) ? 0 : volume - 10;
            ai_audio_set_volume(volume);
            PR_NOTICE("Volume decreased to %d%%", volume);
        }
        break;
    }

    case KEYBOARD_EVENT_PRESS_Q:
        PR_NOTICE("Quit requested via keyboard");
        // Quit is handled in keyboard_input.c
        break;

    default:
        break;
    }
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

/**
 * @brief Plays an alert sound based on the specified alert type.
 *
 * @param type - The type of alert to play, defined by the APP_ALERT_TYPE_E enum.
 * @return OPERATE_RET - Returns OPRT_OK if the alert sound is successfully played, otherwise returns an error code.
 */
OPERATE_RET ai_audio_player_play_alert(AI_AUDIO_ALERT_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;
    char alert_id[64] = {0};

    snprintf(alert_id, sizeof(alert_id), "alert_%d", type);

    rt = ai_audio_player_start(alert_id);

    switch (type) {
    case AI_AUDIO_ALERT_POWER_ON: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_prologue_zh, sizeof(media_src_prologue_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NOT_ACTIVE: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_conn_zh,
                                        sizeof(media_src_network_conn_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_CFG: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_config_zh,
                                        sizeof(media_src_network_config_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_CONNECTED: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_conn_success_zh,
                                        sizeof(media_src_network_conn_success_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_FAIL: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_conn_failed_zh,
                                        sizeof(media_src_network_conn_failed_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_DISCONNECT: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_reconfigure_zh,
                                        sizeof(media_src_network_reconfigure_zh), 1);
    } break;
    case AI_AUDIO_ALERT_BATTERY_LOW: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_low_battery_zh, sizeof(media_src_low_battery_zh),
                                        1);
    } break;
    case AI_AUDIO_ALERT_PLEASE_AGAIN: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_please_again_zh,
                                        sizeof(media_src_please_again_zh), 1);
    } break;
    case AI_AUDIO_ALERT_WAKEUP: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_ai_zh, sizeof(media_src_ai_zh), 1);
    } break;
    case AI_AUDIO_ALERT_LONG_KEY_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_long_press_zh, sizeof(media_src_long_press_zh),
                                        1);
    } break;
    case AI_AUDIO_ALERT_KEY_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_press_talk_zh, sizeof(media_src_press_talk_zh),
                                        1);
    } break;
    case AI_AUDIO_ALERT_WAKEUP_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_wakeup_chat_zh, sizeof(media_src_wakeup_chat_zh),
                                        1);
    } break;
    case AI_AUDIO_ALERT_FREE_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_free_chat_zh, sizeof(media_src_free_chat_zh), 1);
    } break;

    default:
        break;
    }

    return rt;
}
