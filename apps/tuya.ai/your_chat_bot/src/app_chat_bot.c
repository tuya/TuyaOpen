/**
 * @file app_chat_bot.c
 * @brief app_chat_bot module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#include "app_chat_bot.h"
#include "app_button.h"
#include "app_led.h"
#include "tuya_display.h"

#include "tal_api.h"
#include "tuya_ringbuf.h"

#include "ai_audio.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_WAKEUP_TIMEOUT_MS (20000)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_GPIO_NUM_E pin;
    TUYA_GPIO_LEVEL_E active_level;
} APP_GPIO_CONFIG_S;

typedef struct {
    APP_GPIO_CONFIG_S btn;
    APP_GPIO_CONFIG_S led;
} APP_CB_HW_CONFIG_S;

typedef struct {
    APP_WORK_MODE_E        mode;
    AI_AUDIO_WORK_MODE_E   auido_mode;
    AI_AUDIO_ALERT_TYPE_E  mode_alert;
    bool                   is_open;
}CHAT_WORK_MODE_INFO_T;

typedef struct {
      uint8_t                       is_enable;

      APP_CB_HW_CONFIG_S            config;
      const CHAT_WORK_MODE_INFO_T  *work;

      MUTEX_HANDLE                 enable_mutex;
} APP_CHAT_BOT_S;


/***********************************************************
***********************const declaration********************
***********************************************************/
const CHAT_WORK_MODE_INFO_T cAPP_WORK_HOLD ={
    .mode = APP_CHAT_BOT_WORK_MODE_HOLD,
    .auido_mode = AI_AUDIO_WORK_MODE_HOLD,
    .mode_alert = AI_AUDIO_ALERT_LONG_KEY_TALK,
    .is_open = false, 
};

const CHAT_WORK_MODE_INFO_T cAPP_WORK_ONE_SHOT ={
    .mode = APP_CHAT_BOT_WORK_MODE_ONE_SHOT,
    .auido_mode = AI_AUDIO_WORK_MODE_TRIGGER,
    .mode_alert = AI_AUDIO_ALERT_KEY_TALK,
    .is_open = false,
};

const CHAT_WORK_MODE_INFO_T cAPP_WORK_WAKEUP ={
    .mode = APP_CHAT_BOT_WORK_MODE_WAKEUP,
    .auido_mode = AI_AUDIO_WORK_MODE_WAKEUP,
    .mode_alert = AI_AUDIO_ALERT_WAKEUP_TALK,
    .is_open = true,
};

const CHAT_WORK_MODE_INFO_T cAPP_WORK_FREE ={
    .mode = APP_CHAT_BOT_WORK_MODE_FREE,
    .auido_mode = AI_AUDIO_WORK_MODE_FREE,
    .mode_alert = AI_AUDIO_ALERT_FREE_TALK,
    .is_open = true,
};

const CHAT_WORK_MODE_INFO_T *cWORK_MODE_INFO_LIST[] = {
    &cAPP_WORK_HOLD,
    &cAPP_WORK_ONE_SHOT,
    &cAPP_WORK_WAKEUP,
    &cAPP_WORK_FREE,
};

/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_CHAT_BOT_S sg_chat_bot = {
    .config =
        {
            .btn =
                {
                    .pin = CHAT_BUTTON_PIN,
                    .active_level = TUYA_GPIO_LEVEL_LOW,
                },
            .led =
                {
                    .pin = CHAT_INDICATE_LED_PIN,
                    .active_level = TUYA_GPIO_LEVEL_HIGH,
                },
        },
    .work = &cAPP_WORK_HOLD,
};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __app_ai_msg_cb(AI_AGENT_MSG_T *msg)
{
    if (NULL == msg) {
        PR_ERR("msg is NULL");
        return;
    }

    switch (msg->type) {
    case AI_AGENT_MSG_TP_TEXT_ASR: {
        if (msg->data_len > 0) {
            // send asr text to display
            tuya_display_send_msg(TY_DISPLAY_TP_HUMAN_CHAT, (char *)msg->data, msg->data_len);
        }
    } break;
    case AI_AGENT_MSG_TP_TEXT_NLG: {
        tuya_display_send_msg(TY_DISPLAY_TP_AI_CHAT, (char *)msg->data, msg->data_len);
    } break;
    case AI_AGENT_MSG_TP_AUDIO_START: {
    } break;
    case AI_AGENT_MSG_TP_AUDIO_DATA: {
    } break;
    case AI_AGENT_MSG_TP_AUDIO_STOP: {
    } break;
    case AI_AGENT_MSG_TP_EMOTION: {
        PR_DEBUG("---> AI_MSG_TYPE_EMOTION");
        PR_DEBUG("emotion len: %d", msg->data_len);
        if (msg->data_len > 0) {
            // send emotion text to display
            PR_DEBUG("emotion data: %s", msg->data);
            for (int i = 0; i < msg->data_len; i++) {
                PR_DEBUG("emotion data[%d]: 0x%x", i, msg->data[i]);
            }
        }
    } break;
    default:
        break;
    }

    return;
}

APP_WORK_MODE_E app_chat_bot_get_work_mode(void)
{
    return sg_chat_bot.work->mode;
}

OPERATE_RET app_chat_bot_enable_set(uint8_t enable)
{
    if (NULL == sg_chat_bot.enable_mutex) {
        PR_ERR("chat bot enable mutex is NULL");
        return OPRT_COM_ERROR;
    }

    if (sg_chat_bot.is_enable == enable) {
        PR_DEBUG("chat bot enable is already %s", enable ? "enable" : "disable");
        return OPRT_OK;
    }

    PR_DEBUG("chat bot enable set %s", enable ? "enable" : "disable");

    tal_mutex_lock(sg_chat_bot.enable_mutex);
    sg_chat_bot.is_enable = enable;
    tal_mutex_unlock(sg_chat_bot.enable_mutex);

    ai_audio_set_open(enable);

    // set led, display, etc.
    app_led_set(enable);

    TY_DISPLAY_TYPE_E disp_tp;
    disp_tp = enable ? TY_DISPLAY_TP_STAT_LISTEN : TY_DISPLAY_TP_STAT_IDLE;
    tuya_display_send_msg(disp_tp, NULL, 0);

    return OPRT_OK;
}

uint8_t app_chat_bot_is_enable(void)
{
    return sg_chat_bot.is_enable;
}

void app_chat_bot_config_dump(void)
{
    PR_DEBUG("chat bot config:");
    PR_DEBUG("btn: pin=%d, active_level=%d", sg_chat_bot.config.btn.pin, sg_chat_bot.config.btn.active_level);
    PR_DEBUG("led: pin=%d, active_level=%d", sg_chat_bot.config.led.pin, sg_chat_bot.config.led.active_level);
}

OPERATE_RET app_chat_bot_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_AUDIO_CONFIG_T ai_audio_cfg;

    app_chat_bot_config_dump();

    // enable mutex
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_chat_bot.enable_mutex));

    ai_audio_cfg.work_mode = sg_chat_bot.work->auido_mode;
    ai_audio_cfg.agent_msg_cb = __app_ai_msg_cb;

    TUYA_CALL_ERR_RETURN(ai_audio_init(&ai_audio_cfg));

    // button init
    TUYA_CALL_ERR_RETURN(app_button_init(sg_chat_bot.config.btn.pin, sg_chat_bot.config.btn.active_level));
    // led init
    TUYA_CALL_ERR_RETURN(app_led_init(sg_chat_bot.config.led.pin, sg_chat_bot.config.led.active_level));

    app_chat_bot_enable_set(sg_chat_bot.work->is_open);

    return OPRT_OK;
}

OPERATE_RET app_chat_bot_set_work_mode(APP_WORK_MODE_E work_mode)
{ 
    const CHAT_WORK_MODE_INFO_T *p_work = NULL;

    if (work_mode >= APP_CHAT_BOT_WORK_MODE_MAX) {
        PR_ERR("work mode:%d is invalid", work_mode);
        return OPRT_INVALID_PARM;
    }

    p_work = cWORK_MODE_INFO_LIST[work_mode];

    app_chat_bot_enable_set(0);
    ai_audio_set_work_mode(p_work->auido_mode);
    ai_audio_player_play_alert(p_work->mode_alert);
    app_chat_bot_enable_set(p_work->is_open);

    sg_chat_bot.work = p_work;

    return OPRT_OK;
}

OPERATE_RET app_chat_bot_set_next_mode(void)
{
    uint8_t mode = sg_chat_bot.work->mode;

    mode = (mode + 1) % APP_CHAT_BOT_WORK_MODE_MAX;

    app_chat_bot_set_work_mode(mode);

    return OPRT_OK;
}
