/**
 * @file app_chat_bot.c
 * @brief app_chat_bot module is used to
 * @version 0.1
 * @date 2025-03-25
 */

#include "app_chat_bot.h"
#include "app_button.h"
#include "app_led.h"
#include "app_vad.h"
#include "app_player.h"
#include "app_recorder.h"
#include "app_ai.h"
#include "tuya_audio_debug.h"
#include "tuya_display.h"

#include "tal_api.h"
#include "tuya_ringbuf.h"

#include "tkl_video_in.h"
#include "tkl_audio.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define APP_SPK_VOLUME_KEY "spk_volume"

// vad cache size
#define VAD_CACHE_SIZE (30 * 320) // 300ms pcm data

#define RECORDER_DELAY_MS (200)

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
    APP_GPIO_CONFIG_S spk;
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
            .spk =
                {
                    .pin = SPEAKER_EN_PIN,
                    .active_level = TUYA_GPIO_LEVEL_LOW,
                },
        },
    .work_mode = APP_CHAT_BOT_WORK_MODE_ONE_SHOT,
};

/***********************************************************
***********************function define**********************
***********************************************************/

static int __app_chat_bot_mode_hold(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    uint8_t is_playing = app_player_is_playing();

    if (is_playing) {
        // if player is playing, discard the data
        app_recorder_rb_reset();
        return 0;
    }

    app_recorder_rb_write(pframe->pbuf, pframe->used_size);

    return 0;
}

static int __app_chat_bot_mode_one_shot(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    static uint8_t recv_cnt = 0;
    uint8_t is_playing = app_player_is_playing();

    if (is_playing) {
        app_vad_stop();
        recv_cnt = 0;
        return 0;
    }

    recv_cnt++;
    if (recv_cnt < RECORDER_DELAY_MS / RECORDER_FRAME_SIZE) {
        return 0;
    }
    recv_cnt = RECORDER_DELAY_MS / RECORDER_FRAME_SIZE; // Prevent overflow

    app_vad_start();
    app_vad_frame_put(pframe->pbuf, pframe->used_size);

    app_recorder_rb_write(pframe->pbuf, pframe->used_size);

    return 0;
}

static int __app_audio_frame_put(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    if (sg_chat_bot.is_enable == 0) {
        return 0;
    }

    if (APP_CHAT_BOT_WORK_MODE_HOLD == sg_chat_bot.work_mode) {
        __app_chat_bot_mode_hold(pframe);
    } else if (APP_CHAT_BOT_WORK_MODE_ONE_SHOT == sg_chat_bot.work_mode) {
        __app_chat_bot_mode_one_shot(pframe);
    } else {
        return 0;
    }

    return 0;
}

static void __app_ai_msg_cb(APP_AI_MSG_T *msg)
{
    if (NULL == msg) {
        PR_ERR("msg is NULL");
        return;
    }

    switch (msg->type) {
    case APP_AI_MSG_TYPE_TEXT_ASR: {
        PR_DEBUG("---> AI_MSG_TYPE_TEXT_ASR");
        if (msg->data_len > 0) {
            // send asr text to display
            tuya_display_send_msg(TY_DISPLAY_TP_HUMAN_CHAT, msg->data, msg->data_len);

            // Prepare to play mp3
            if (app_player_is_playing()) {
                PR_DEBUG("player is playing, stop it first");
                app_player_stop();
            }
            app_player_start();
        }
        TUYA_AUDIO_VOICE_STATE state = app_recorder_stat_get();
        if (state == VOICE_STATE_IN_WAIT_ASR) {
            PR_DEBUG("asr get, recorder set idle");
            app_recorder_stat_post(VOICE_STATE_IN_IDLE);
        }
    } break;
    case APP_AI_MSG_TYPE_TEXT_NLG: {
        PR_DEBUG("---> AI_MSG_TYPE_TEXT_NLG");
        tuya_display_send_msg(TY_DISPLAY_TP_AI_CHAT, msg->data, msg->data_len);
    } break;
    case APP_AI_MSG_TYPE_AUDIO_START: {
        PR_DEBUG("---> AI_MSG_TYPE_AUDIO_START");
    } break;
    case APP_AI_MSG_TYPE_AUDIO_DATA: {
        PR_DEBUG("---> AI_MSG_TYPE_AUDIO_DATA, len: %d", msg->data_len);
        app_player_data_write(msg->data, msg->data_len, 0);
    } break;
    case APP_AI_MSG_TYPE_AUDIO_STOP: {
        PR_DEBUG("---> AI_MSG_TYPE_AUDIO_STOP");
        app_player_data_write(msg->data, msg->data_len, 1);
    } break;
    case APP_AI_MSG_TYPE_EMOTION: {
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

static OPERATE_RET __app_audio_hardware_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    APP_GPIO_CONFIG_S *p_spk = &sg_chat_bot.config.spk;

    PR_DEBUG("tkl_audio_init...");

    TKL_AUDIO_CONFIG_T config;
    memset(&config, 0, sizeof(TKL_AUDIO_CONFIG_T));

    if (sg_chat_bot.work_mode == APP_CHAT_BOT_WORK_MODE_HOLD) {
        config.enable = 0;
    } else if (sg_chat_bot.work_mode == APP_CHAT_BOT_WORK_MODE_ONE_SHOT) {
        config.enable = 0;
    } else {
        return OPRT_INVALID_PARM;
    }

    config.ai_chn = TKL_AI_0;
    config.sample = TKL_AUDIO_SAMPLE_16K;
    config.datebits = TKL_AUDIO_DATABITS_16;
    config.channel = TKL_AUDIO_CHANNEL_MONO;
    config.codectype = TKL_CODEC_AUDIO_PCM;
    config.card = TKL_AUDIO_TYPE_BOARD;
    config.put_cb = __app_audio_frame_put;

    config.spk_sample = TKL_AUDIO_SAMPLE_16K;
    config.spk_gpio = p_spk->pin;
    config.spk_gpio_polarity = p_spk->active_level;

    TUYA_CALL_ERR_RETURN(tkl_ai_init(&config, 0));
    TUYA_CALL_ERR_RETURN(tkl_ai_start(0, 0));
    TUYA_CALL_ERR_RETURN(tkl_ai_set_vol(TKL_AUDIO_TYPE_BOARD, 0, 80));

    uint8_t volume = app_chat_bot_volume_get();
    TUYA_CALL_ERR_RETURN(tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, volume));

    PR_DEBUG("tkl_audio_init success");

    return rt;
}

OPERATE_RET app_chat_bot_volume_set(uint8_t volume)
{
    OPERATE_RET rt = OPRT_OK;

    // kv storage
    TUYA_CALL_ERR_LOG(tal_kv_set(APP_SPK_VOLUME_KEY, &volume, sizeof(volume)));
    TUYA_CALL_ERR_LOG(tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, volume));

    return rt;
}

uint8_t app_chat_bot_volume_get(void)
{
    OPERATE_RET rt = OPRT_OK;

    uint8_t volume = 0;
    uint8_t *value = NULL;
    size_t read_len = 0;

    // kv read
    TUYA_CALL_ERR_LOG(tal_kv_get(APP_SPK_VOLUME_KEY, &value, &read_len));
    if (OPRT_OK != rt || NULL == value) {
        PR_ERR("read volume failed");
        volume = 50;
    } else {
        volume = *value;
    }

    PR_DEBUG("get spk volume: %d", volume);

    if (value) {
        tal_kv_free(value);
        value = NULL;
    }

    return volume;
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

    // vad enable/disable
    if (sg_chat_bot.work_mode != APP_CHAT_BOT_WORK_MODE_HOLD) {
        if (enable) {
            app_vad_start();
        } else {
            app_vad_stop();
        }
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
    PR_DEBUG("spk: pin=%d, active_level=%d", sg_chat_bot.config.spk.pin, sg_chat_bot.config.spk.active_level);
    PR_DEBUG("work_mode: %s", sg_chat_bot.work_mode == APP_CHAT_BOT_WORK_MODE_HOLD ? "hold" : "one shot");
}

OPERATE_RET app_chat_bot_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    app_chat_bot_config_dump();

    // enable mutex
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_chat_bot.enable_mutex));

    // vad init
    if (sg_chat_bot.work_mode != APP_CHAT_BOT_WORK_MODE_HOLD) {
        TUYA_CALL_ERR_RETURN(app_vad_init(TKL_AUDIO_SAMPLE_16K, TKL_AUDIO_CHANNEL_MONO));
    }

    // recorder init
    TUYA_CALL_ERR_RETURN(app_recorder_init());
    // speaker init
    TUYA_CALL_ERR_RETURN(app_player_init());
    // ai init
    TUYA_CALL_ERR_RETURN(app_ai_init(__app_ai_msg_cb));

    // audio hardware init
    TUYA_CALL_ERR_RETURN(__app_audio_hardware_init());
    // button init
    TUYA_CALL_ERR_RETURN(app_button_init(sg_chat_bot.config.btn.pin, sg_chat_bot.config.btn.active_level));
    // led init
    TUYA_CALL_ERR_RETURN(app_led_init(sg_chat_bot.config.led.pin, sg_chat_bot.config.led.active_level));

    TUYA_CALL_ERR_LOG(app_chat_bot_enable_set(0));

    return OPRT_OK;
}
