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

#include "tuya_iot.h"

#include "tdl_button_manage.h"

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
#include "tdl_led_manage.h"
#endif

#include "app_display.h"
#include "ai_audio.h"
#include "app_pocket.h"
#include "game_pet.h"
#include "media_src_en.h"
#include "uart_expand.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define TRIG_BUTTON_NAME "btn_trig"

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

typedef struct {
    char *emoj_name;
    POCKET_DISP_TP_E disp_tp;
} AI_EMOJ_DISP_MAP_T;

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

const AI_EMOJ_DISP_MAP_T cAI_EMOJI_DISP_LIST[] = {
    {"HAPPY", POCKET_DISP_TP_EMOJ_HAPPY},
    {"ANGRY", POCKET_DISP_TP_EMOJ_ANGRY},
    {"FEARFUL", POCKET_DISP_TP_EMOJ_CRY},
    {"SAD", POCKET_DISP_TP_EMOJ_CRY},
};
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

static TDL_BUTTON_HANDLE sg_button_hdl = NULL;
/***********************************************************
***********************function define**********************
***********************************************************/
static volatile BOOL_T sg_text_stream_active = FALSE;  /* Whether text stream is active */

/**
 * @brief Get text stream status
 * @return TRUE=text stream active, FALSE=text stream ended
 */
BOOL_T app_get_text_stream_status(void)
{
    return sg_text_stream_active;
}

static void __app_ai_audio_evt_inform_cb(AI_AUDIO_EVENT_E event, uint8_t *data, uint32_t len, void *arg)
{

    switch (event) {
    case AI_AUDIO_EVT_HUMAN_ASR_TEXT: {
        if (len > 0 && data) {
        }
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_START: {
        sg_text_stream_active = TRUE;
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_DATA: {
        // Write UTF8 data to ring buffer
        uart_print_write(data, len);
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_END: {
        sg_text_stream_active = FALSE;
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_EMO: {
        AI_AUDIO_EMOTION_T *emo;
        PR_DEBUG("---> AI_MSG_TYPE_EMOTION");
        emo = (AI_AUDIO_EMOTION_T *)data;
        if (emo) {
            if (emo->name) {
                PR_DEBUG("emotion name:%s", emo->name);
            }

            for (int i = 0; i < CNTSOF(cAI_EMOJI_DISP_LIST); i++) {
                if (strcmp(emo->name, cAI_EMOJI_DISP_LIST[i].emoj_name) == 0) {
                    app_display_send_msg(cAI_EMOJI_DISP_LIST[i].disp_tp, NULL, 0);
                    break;
                }
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
        break;
    case AI_AUDIO_STATE_LISTEN:
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
        tdl_led_set_status(sg_led_hdl, TDL_LED_ON);
#endif
        break;
    case AI_AUDIO_STATE_UPLOAD:

        break;
    case AI_AUDIO_STATE_AI_SPEAK:
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
            ai_audio_player_stop();
            if (AI_AUDIO_STATE_UPLOAD == ai_audio_get_state() || AI_AUDIO_STATE_AI_SPEAK == ai_audio_get_state()) {
                ai_audio_cloud_asr_set_idle(true);
            }
            PR_DEBUG("button press down, listen start");
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
            tdl_led_set_status(sg_led_hdl, TDL_LED_ON);
#endif
            ai_audio_manual_start_single_talk();
            app_display_send_msg(POCKET_DISP_TP_AI, NULL, 0);
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
    TUYA_CALL_ERR_RETURN(tdl_button_create(TRIG_BUTTON_NAME, &button_cfg, &sg_button_hdl));

    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_DOWN, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_UP, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_SINGLE_CLICK, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_DOUBLE_CLICK, __app_button_function_cb);

    return rt;
}

OPERATE_RET app_pocket_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_AUDIO_CONFIG_T ai_audio_cfg;

    app_display_init();

    ai_audio_cfg.work_mode = sg_chat_bot.work->auido_mode;
    ai_audio_cfg.evt_inform_cb = __app_ai_audio_evt_inform_cb;
    ai_audio_cfg.state_inform_cb = __app_ai_audio_state_inform_cb;

    TUYA_CALL_ERR_RETURN(ai_audio_init(&ai_audio_cfg));

    TUYA_CALL_ERR_RETURN(__app_open_button());

    TUYA_CALL_ERR_RETURN(uart_expand_init());

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    sg_led_hdl = tdl_led_find_dev(LED_NAME);
    TUYA_CALL_ERR_RETURN(tdl_led_open(sg_led_hdl));
#endif

    __app_chat_bot_enable(sg_chat_bot.work->is_open);

    PR_NOTICE("work:%s", sg_chat_bot.work->display_text);

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

    ai_audio_player_start(alert_id);

    switch (type) {
    case AI_AUDIO_ALERT_POWER_ON: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_prologue_en, sizeof(media_src_prologue_en), 1);
    } break;
    case AI_AUDIO_ALERT_NOT_ACTIVE: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_conn_en, sizeof(media_src_network_conn_en), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_CFG: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_config_en, sizeof(media_src_network_config_en), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_CONNECTED: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_conn_success_en,
                                        sizeof(media_src_network_conn_success_en), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_FAIL: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_conn_failed_en, sizeof(media_src_network_conn_failed_en), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_DISCONNECT: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_reconfigure_en,
                                        sizeof(media_src_network_reconfigure_en), 1);
    } break;
    case AI_AUDIO_ALERT_BATTERY_LOW: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_low_battery_en, sizeof(media_src_low_battery_en), 1);
    } break;
    case AI_AUDIO_ALERT_PLEASE_AGAIN: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_please_again_en, sizeof(media_src_please_again_en), 1);
    } break;
    case AI_AUDIO_ALERT_WAKEUP: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_ai_en, sizeof(media_src_ai_en), 1);
    } break;
    case AI_AUDIO_ALERT_LONG_KEY_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_long_press_en,
                                        sizeof(media_src_long_press_en), 1);
    } break;
    case AI_AUDIO_ALERT_KEY_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_press_talk_en, sizeof(media_src_press_talk_en), 1);
    } break;
    case AI_AUDIO_ALERT_WAKEUP_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_wakeup_chat_en, sizeof(media_src_wakeup_chat_en),
                                        1);
    } break;
    case AI_AUDIO_ALERT_FREE_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_free_chat_en, sizeof(media_src_free_chat_en),
                                        1);
    } break;

    default:
        break;
    }

    return rt;
}