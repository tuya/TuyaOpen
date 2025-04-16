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
    // hardware config
    APP_CB_HW_CONFIG_S config;
    // work mode
    APP_WORK_MODE_E work_mode;

    uint8_t is_enable;

    MUTEX_HANDLE enable_mutex;
} APP_CHAT_BOT_S;

/***********************************************************
********************function declaration********************
***********************************************************/

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
    .work_mode = APP_CHAT_BOT_WORK_MODE_ONE_SHOT,
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
        PR_DEBUG("---> AI_MSG_TYPE_TEXT_ASR");
        if (msg->data_len > 0) {
            // send asr text to display
            tuya_display_send_msg(TY_DISPLAY_TP_HUMAN_CHAT, (char *)msg->data, msg->data_len);
        }
    } break;
    case AI_AGENT_MSG_TP_TEXT_NLG: {
        PR_DEBUG("---> AI_MSG_TYPE_TEXT_NLG");
        tuya_display_send_msg(TY_DISPLAY_TP_AI_CHAT, (char *)msg->data, msg->data_len);
    } break;
    case AI_AGENT_MSG_TP_AUDIO_START: {
        PR_DEBUG("---> AI_MSG_TYPE_AUDIO_START");
    } break;
    case AI_AGENT_MSG_TP_AUDIO_DATA: {
        PR_DEBUG("---> AI_MSG_TYPE_AUDIO_DATA, len: %d", msg->data_len);
    } break;
    case AI_AGENT_MSG_TP_AUDIO_STOP: {
        PR_DEBUG("---> AI_MSG_TYPE_AUDIO_STOP");
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
    return sg_chat_bot.work_mode;
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

    if (enable) {
        ai_audio_set_silent(false);
    } else {
        ai_audio_set_silent(true);
    }

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
    PR_DEBUG("work_mode: %s", sg_chat_bot.work_mode == APP_CHAT_BOT_WORK_MODE_HOLD ? "hold" : "one shot");
}

OPERATE_RET app_chat_bot_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_AUDIO_CONFIG_T ai_audio_cfg;

    app_chat_bot_config_dump();

    // enable mutex
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_chat_bot.enable_mutex));

    ai_audio_cfg.agent_msg_cb = __app_ai_msg_cb;
    if (sg_chat_bot.work_mode == APP_CHAT_BOT_WORK_MODE_HOLD) {
        ai_audio_cfg.is_enable_interrupt = 0;
        ai_audio_cfg.is_open_asr = 0;
        ai_audio_cfg.is_open_vad = 0;
        ai_audio_cfg.wakeup_timeout = 0;
    } else if (sg_chat_bot.work_mode == APP_CHAT_BOT_WORK_MODE_ONE_SHOT) {
        ai_audio_cfg.is_enable_interrupt = 0;
        ai_audio_cfg.is_open_asr = 0;
        ai_audio_cfg.is_open_vad = 1;
        ai_audio_cfg.wakeup_timeout = 0;
    } else if (sg_chat_bot.work_mode == APP_CHAT_BOT_WORK_MODE_WAKEUP) {
        ai_audio_cfg.is_enable_interrupt = 0;
        ai_audio_cfg.is_open_asr = 0;
        ai_audio_cfg.is_open_vad = 1;
        ai_audio_cfg.wakeup_timeout = AI_WAKEUP_TIMEOUT_MS;
    } else {
        ai_audio_cfg.is_enable_interrupt = 0;
        ai_audio_cfg.is_open_asr = 0;
        ai_audio_cfg.is_open_vad = 1;
        ai_audio_cfg.wakeup_timeout = AI_WAKEUP_TIMEOUT_MS;
    }
    TUYA_CALL_ERR_RETURN(ai_audio_init(&ai_audio_cfg));

    // button init
    TUYA_CALL_ERR_RETURN(app_button_init(sg_chat_bot.config.btn.pin, sg_chat_bot.config.btn.active_level));
    // led init
    TUYA_CALL_ERR_RETURN(app_led_init(sg_chat_bot.config.led.pin, sg_chat_bot.config.led.active_level));

    if (sg_chat_bot.work_mode == APP_CHAT_BOT_WORK_MODE_WAKEUP ||
        sg_chat_bot.work_mode == APP_CHAT_BOT_WORK_MODE_FREE) {
        TUYA_CALL_ERR_LOG(app_chat_bot_enable_set(1));
    } else {
        TUYA_CALL_ERR_LOG(app_chat_bot_enable_set(0));
    }

    return OPRT_OK;
}
