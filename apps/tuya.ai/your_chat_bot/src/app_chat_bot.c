/**
 * @file app_chat_bot.c
 * @brief app_chat_bot module is used to
 * @version 0.1
 * @date 2025-03-25
 */
#include "netmgr.h"

#include "tkl_gpio.h"
#include "tal_api.h"
#include "tuya_ringbuf.h"

#include "tdd_button_gpio.h"
#include "tdl_button_manage.h"

#include "ai_audio.h"
#include "tuya_display.h"
#include "app_chat_bot.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define APP_BUTTON_NAME "app_button"

typedef uint8_t APP_WORK_MODE_E;
// Press and hold button to talk.
#define APP_CHAT_BOT_WORK_MODE_HOLD     0  
// Press the button once to start or stop the free conversation.
#define APP_CHAT_BOT_WORK_MODE_ONE_SHOT 1 
#define APP_CHAT_BOT_WORK_MODE_MAX      2
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    APP_WORK_MODE_E        mode;
    AI_AUDIO_WORK_MODE_E   auido_mode;
    bool                   is_open;
}CHAT_WORK_MODE_INFO_T;

typedef struct {
    TUYA_GPIO_NUM_E        led_pin;
    TUYA_GPIO_LEVEL_E      active_level;
    uint8_t                 status;
}INDICATE_LED_T;

typedef struct {
      uint8_t                       is_enable;
      const CHAT_WORK_MODE_INFO_T  *work;
      INDICATE_LED_T                led;
} APP_CHAT_BOT_S;



/***********************************************************
***********************const declaration********************
***********************************************************/
const CHAT_WORK_MODE_INFO_T cAPP_WORK_HOLD ={
    .mode = APP_CHAT_BOT_WORK_MODE_HOLD,
    .auido_mode = AI_AUDIO_MODE_MANUAL_SINGLE_TALK,
    .is_open = true, 
};

const CHAT_WORK_MODE_INFO_T cAPP_WORK_ONE_SHOT ={
    .mode = APP_CHAT_BOT_WORK_MODE_ONE_SHOT,
    .auido_mode = AI_AUDIO_WORK_MANUAL_FREE_TALK,
    .is_open = false,
};

const CHAT_WORK_MODE_INFO_T *cWORK_MODE_INFO_LIST[] = {
    &cAPP_WORK_HOLD,
    &cAPP_WORK_ONE_SHOT,
};

/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_CHAT_BOT_S sg_chat_bot = {

    .work = &cAPP_WORK_ONE_SHOT,
};

static TDL_BUTTON_HANDLE sg_button_hdl = NULL;
/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __app_led_set_state(uint8_t is_on)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_GPIO_LEVEL_E level = is_on ? sg_chat_bot.led.active_level : !sg_chat_bot.led.active_level;
    sg_chat_bot.led.status = is_on;
    TUYA_CALL_ERR_LOG(tkl_gpio_write(sg_chat_bot.led.led_pin, level));

    return rt;
}

static OPERATE_RET __app_led_init(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E active_level)
{
    OPERATE_RET rt = OPRT_OK;

    if (pin >= TUYA_GPIO_NUM_MAX) {
        return OPRT_INVALID_PARM;
    }

    sg_chat_bot.led.led_pin = pin;
    sg_chat_bot.led.active_level = active_level;

    TUYA_GPIO_BASE_CFG_T out_pin_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT, 
        .level = TUYA_GPIO_LEVEL_HIGH
    };
    TUYA_CALL_ERR_RETURN(tkl_gpio_init(sg_chat_bot.led.led_pin, &out_pin_cfg));

    TUYA_CALL_ERR_RETURN(__app_led_set_state(0));

    return rt;
}

static void __app_ai_audio_inform_cb(AI_AUDIO_EVENT_E event,  uint8_t *data, uint32_t len, void *arg)
{
    switch (event) {
    case AI_AUDIO_EVT_HUMAN_ASR_TEXT: {
        if (len > 0 && data) {
            // send asr text to display
            tuya_display_send_msg(TY_DISPLAY_TP_HUMAN_CHAT, (char *)data, len);
        }
    } 
    break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT: {
        tuya_display_send_msg(TY_DISPLAY_TP_AI_CHAT, (char *)data, len);
    } 
    break;
    case AI_AUDIO_EVT_AI_REPLIES_EMO: {
        AI_AUDIO_EMOTION_T *emo;
        PR_DEBUG("---> AI_MSG_TYPE_EMOTION");
        emo = (AI_AUDIO_EMOTION_T *)data;
        if(emo) {
            if(emo->name){
                PR_DEBUG("emotion:%s", emo->name);
            }

            if(emo->text){
                PR_DEBUG("emotion text:%s", emo->text);
            }
        }
    } 
    break;
    case AI_AUDIO_EVT_WAKEUP:{
        PR_DEBUG("wakeup");
    }
    break;
    default:
        break;
    }

    return;
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

static void __app_button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    APP_WORK_MODE_E work_mode = sg_chat_bot.work->mode;
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
        if (work_mode == APP_CHAT_BOT_WORK_MODE_HOLD) {
            PR_DEBUG("button press down, chat bot enable");
            __app_led_set_state(1);
            ai_audio_manual_start_single_talk();
        }
    } break;
    case TDL_BUTTON_PRESS_UP: {
        if (work_mode == APP_CHAT_BOT_WORK_MODE_HOLD) {
            PR_DEBUG("button press up, chat bot disable");
            __app_led_set_state(0);
            ai_audio_manual_stop_single_talk();
        }
    } break;
    case TDL_BUTTON_PRESS_SINGLE_CLICK: {
        if (work_mode == APP_CHAT_BOT_WORK_MODE_ONE_SHOT) {
            if(sg_chat_bot.is_enable) {
                __app_chat_bot_enable(false);
                __app_led_set_state(0);
                tuya_display_send_msg(TY_DISPLAY_TP_STAT_IDLE, NULL, 0);
            }else {
                __app_chat_bot_enable(true);
                __app_led_set_state(1);
                tuya_display_send_msg(TY_DISPLAY_TP_STAT_LISTEN, NULL, 0);
            }
            PR_DEBUG("button single click, chat bot %s", sg_chat_bot.is_enable ? "enable" : "disable");
        }
    } break;
    default:
        break;
    }
}

static OPERATE_RET __app_button_init(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E active_level)
{
    OPERATE_RET rt = OPRT_OK;

    if (pin >= TUYA_GPIO_NUM_MAX) {
        return OPRT_INVALID_PARM;
    }

    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin = pin,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
        .level = active_level,
    };

    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(APP_BUTTON_NAME, &button_hw_cfg));

    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 500};
    TUYA_CALL_ERR_RETURN(tdl_button_create(APP_BUTTON_NAME, &button_cfg, &sg_button_hdl));

    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_DOWN, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_UP, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_SINGLE_CLICK, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_DOUBLE_CLICK, __app_button_function_cb);

    return rt;
}

static void __app_chat_bot_config_dump(void)
{
    PR_DEBUG("chat bot config:");
    PR_DEBUG("btn: pin=%d, active_level=%d", CHAT_BUTTON_PIN, TUYA_GPIO_LEVEL_LOW);
    PR_DEBUG("led: pin=%d, active_level=%d", CHAT_INDICATE_LED_PIN, TUYA_GPIO_LEVEL_HIGH);
}

OPERATE_RET app_chat_bot_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_AUDIO_CONFIG_T ai_audio_cfg;

    __app_chat_bot_config_dump();

    ai_audio_cfg.work_mode = sg_chat_bot.work->auido_mode;
    ai_audio_cfg.inform_cb = __app_ai_audio_inform_cb;

    TUYA_CALL_ERR_RETURN(ai_audio_init(&ai_audio_cfg));

    // button init
    TUYA_CALL_ERR_RETURN(__app_button_init(CHAT_BUTTON_PIN, TUYA_GPIO_LEVEL_LOW));
    // led init
    TUYA_CALL_ERR_RETURN(__app_led_init(CHAT_INDICATE_LED_PIN, TUYA_GPIO_LEVEL_HIGH));

    tuya_display_send_msg(TY_DISPLAY_TP_STAT_IDLE, NULL, 0);

    __app_chat_bot_enable(sg_chat_bot.work->is_open);

    return OPRT_OK;
}
