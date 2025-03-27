/**
 * @file ai_audio_proc.c
 * @brief Implements audio processing functionality for AI applications
 *
 * This source file provides the implementation of audio processing functionalities
 * required for AI applications. It includes functionality for audio frame handling,
 * voice activity detection, audio streaming, and interaction with AI processing
 * modules. The implementation supports audio frame processing, voice state management,
 * and integration with audio player modules. This file is essential for developers
 * working on AI-driven audio applications that require real-time audio processing
 * and interaction with AI services.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tuya_iot.h"
#include "tuya_config.h"
#include "tuya_ringbuf.h"

#include "tal_api.h"
#include "ai_audio_proc.h"
#include "tuya_audio_player.h"
#include "tuya_audio_recorder.h"
#include "tdd_button_gpio.h"
#include "tdl_button_manage.h"

#include "tuya_audio_debug.h"
#include "tkl_video_in.h"
#include "tkl_gpio.h"

#include "tuya_display.h"
#include "ty_vad_app.h"

#define AUDIO_SAMPLE_RATE 16000
#define SPK_SAMPLE_RATE   16000
#define AUDIO_SAMPLE_BITS 16
#define AUDIO_CHANNEL     1

#define AUDIO_TTS_STREAM_BUFF_MAX_LEN (1024 * 64)
#define AUDIO_PCM_SLICE_BUFF_LEN      (320)
#define AUDIO_PCM_SLICE_TIME          (AUDIO_PCM_SLICE_BUFF_LEN / 2 / (AUDIO_SAMPLE_RATE / 1000))

#define VAD_ACTIVE_RB_SIZE (300 * 16000 * 16 * 1 / 8 / 1000) // 300ms

#define SPEAKER_ENABLE_PIN SPEAKER_EN_PIN

#define APP_BUTTON_NAME   "app_button"
#define AUDIO_TRIGGER_PIN CHAT_BUTTON_PIN
static TDL_BUTTON_HANDLE sg_button_hdl = NULL;

#define CHAT_LED_PIN CHAT_INDICATE_LED_PIN

static TUYA_AUDIO_RECORDER_HANDLE ty_ai_handle = NULL;

#define SILENCE_THRESHOLD_HOLD_MODE 200
#define ACTIVE_THRESHOLD_HOLD_MODE  200
#define WAIT_STOP_PLAY_THRESHOLD    200

static TUYA_AUDIO_RECORDER_CONFIG_T cfg = {
    .sample_rate = TKL_AUDIO_SAMPLE_16K,
    .sample_bits = TKL_AUDIO_DATABITS_16,
    .channel = TKL_AUDIO_CHANNEL_MONO,
    .upload_slice_duration = 100,
    .record_duration = 10000,
};

static TUYA_AUDIO_RECORDER_THRESHOLD_T recorder_threshold_cfg = {
    .silence_threshold = SILENCE_THRESHOLD_HOLD_MODE,
    .active_threshold = ACTIVE_THRESHOLD_HOLD_MODE,
    .wait_stop_play_threshold = WAIT_STOP_PLAY_THRESHOLD,
    .frame_duration_ms = 0,
};

static TUYA_RINGBUFF_T sg_vad_active_rb = NULL;

typedef struct {
    uint8_t is_enable;
} AI_CHAT_T;

static AI_CHAT_T sg_ai_chat = {
    .is_enable = 0,
};

static void app_chat_enable(uint8_t enable);
static uint8_t app_chat_is_enable(void);

static void app_proc_task(void *arg);

static BOOL_T audio_trigger_pin_is_pressed(void)
{
    TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_HIGH;
    tkl_gpio_read(AUDIO_TRIGGER_PIN, &level);
    return (level == TUYA_GPIO_LEVEL_LOW) ? TRUE : FALSE;
}

static void _vad_rb_discard(void)
{
    if (NULL == sg_vad_active_rb) {
        return;
    }

    // A frame containing 320 bytes of data is discarded
    uint32_t remain_len = tuya_ring_buff_free_size_get(sg_vad_active_rb);
    if (remain_len < 320) {
        tuya_ring_buff_discard(sg_vad_active_rb, 320);
    }
}

static int _audio_frame_put(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    static BOOL_T key_status_old = FALSE;
    static BOOL_T alert_flag = FALSE;
    BOOL_T is_press = FALSE;
    static TUYA_AUDIO_VOICE_STATE state = VOICE_STATE_IN_IDLE;
    OPERATE_RET ret;
    tuya_iot_client_t *client = tuya_iot_client_get();

#if (CHAT_BOT_WORK_MODE == CHAT_BOT_WORK_MODE_ONE_SHOT)
    if (client->status < TUYA_STATUS_MQTT_CONNECTED) {
        // not connected
        return pframe->buf_size;
    }

    static uint8_t is_first = 1;
    if (is_first) {
        is_first = 0;
        app_chat_enable(1);
    }

    if (tuya_audio_player_is_playing()) {
        return pframe->buf_size;
    }

    if (app_chat_is_enable() == FALSE) {
        return pframe->buf_size;
    }

    static uint8_t is_vad_init = 0;
    if (0 == is_vad_init) {
        is_vad_init = 1;
        ty_vad_app_start();
    }

    ty_vad_frame_put(pframe->pbuf, pframe->used_size);

    if (NULL != sg_vad_active_rb) {
        tuya_ring_buff_write(sg_vad_active_rb, pframe->pbuf, pframe->used_size);
    }

    static SYS_TICK_T last_vad_active_time = 0;

    uint8_t vad_state = ty_get_vad_flag();

    if (vad_state == 1) {
        last_vad_active_time = tal_system_get_millisecond();
        // vad start
        if (VOICE_STATE_IN_IDLE == state) {
            // first frame
            state = VOICE_STATE_IN_SILENCE;
            ty_ai_voice_stat_post(ty_ai_handle, state);
            PR_DEBUG("app ---> first frame");
            uint32_t use_len = tuya_ring_buff_used_size_get(sg_vad_active_rb);
            if (use_len > 0) {
                uint8_t *pbuf = (uint8_t *)tkl_system_psram_malloc(use_len);
                if (NULL != pbuf) {
                    tuya_ring_buff_read(sg_vad_active_rb, pbuf, use_len);
                    tuya_audio_recorder_stream_write(ty_ai_handle, pbuf, use_len);
                    tkl_system_psram_free(pbuf);
                }
            }
        } else if (VOICE_STATE_IN_SILENCE == state) {
            state = VOICE_STATE_IN_START;
            ty_ai_voice_stat_post(ty_ai_handle, state);
            tuya_audio_recorder_stream_write(ty_ai_handle, pframe->pbuf, pframe->buf_size);
        } else if (VOICE_STATE_IN_START == state) {
            state = VOICE_STATE_IN_VOICE;
            ty_ai_voice_stat_post(ty_ai_handle, state);
            tuya_audio_recorder_stream_write(ty_ai_handle, pframe->pbuf, pframe->buf_size);
        } else if (VOICE_STATE_IN_VOICE == state) {
            tuya_audio_recorder_stream_write(ty_ai_handle, pframe->pbuf, pframe->buf_size);
        }
    } else {
        // PR_DEBUG("vad none");
        if (last_vad_active_time - tal_system_get_millisecond() > 1000) {
            // 停止本次会话
            if (VOICE_STATE_IN_VOICE == state) {
                state = VOICE_STATE_IN_STOP;
                PR_DEBUG("app ---> stop frame");
                ty_ai_voice_stat_post(ty_ai_handle, state);
            } else if (VOICE_STATE_IN_STOP == state) {
                state = VOICE_STATE_IN_IDLE;
                ty_ai_voice_stat_post(ty_ai_handle, state);
            }
        }
    }

    _vad_rb_discard();

#else
    if (TRUE == audio_trigger_pin_is_pressed()) {
        is_press = TRUE;
    } else {
        is_press = FALSE;
    }

    if (is_press == TRUE && key_status_old != is_press) {
        key_status_old = is_press;
        PR_DEBUG("audio trigger pin is pressed");

        // status check
        PR_DEBUG("client status: %d", client->status);
        if (client->status < TUYA_STATUS_MQTT_CONNECTED) {
            PR_DEBUG("TUYA_STATUS_MQTT_CONNECTED");
            if (alert_flag == FALSE) {
                tuya_audio_player_play_alert(AUDIO_ALART_TYPE_NOT_ACTIVE, TRUE);
                alert_flag = TRUE;
            }
            return 0;
        }

        if (recorder_threshold_cfg.frame_duration_ms == 0) {
            PR_DEBUG("frame_duration_ms is 0, first frame");

            tuya_audio_recorder_stream_clear(ty_ai_handle);

            if (tuya_audio_player_is_playing()) {
                PR_DEBUG("tuya audio is playing, stop it...");
                tuya_audio_player_stop();
            }
            state = VOICE_STATE_IN_SILENCE;
        }

        recorder_threshold_cfg.frame_duration_ms += AUDIO_PCM_SLICE_TIME;

        tuya_audio_recorder_stream_write(ty_ai_handle, pframe->pbuf, pframe->buf_size);
        ret = ty_ai_voice_stat_post(ty_ai_handle, VOICE_STATE_IN_SILENCE);

    } else if (is_press == TRUE && key_status_old == is_press) {
        alert_flag = FALSE;
        if (state == VOICE_STATE_IN_IDLE)
            return 0;

        recorder_threshold_cfg.frame_duration_ms += AUDIO_PCM_SLICE_TIME;

        tuya_audio_recorder_stream_write(ty_ai_handle, pframe->pbuf, pframe->buf_size);

        if (recorder_threshold_cfg.frame_duration_ms >= recorder_threshold_cfg.active_threshold) {
            if (state == VOICE_STATE_IN_SILENCE) {
                ret = ty_ai_voice_stat_post(ty_ai_handle, VOICE_STATE_IN_START);
                if (ret != OPRT_OK) {
                    PR_ERR("record start failed %x", ret);
                }
                state = VOICE_STATE_IN_VOICE;
            } else if (state == VOICE_STATE_IN_VOICE) {
                ret = ty_ai_voice_stat_post(ty_ai_handle, VOICE_STATE_IN_VOICE);
                if (ret != OPRT_OK) {
                    PR_ERR("record post failed %x", ret);
                }
                state = VOICE_STATE_IN_STOP;
            }
        }
    } else if (is_press == FALSE && key_status_old != is_press) {
        alert_flag = FALSE;
        key_status_old = is_press;
        PR_DEBUG("audio trigger pin is released");
        if (state == VOICE_STATE_IN_IDLE)
            return;

        state = VOICE_STATE_IN_IDLE;

        recorder_threshold_cfg.frame_duration_ms = 0;

        ret = ty_ai_voice_stat_post(ty_ai_handle, VOICE_STATE_IN_STOP);
        if (ret != OPRT_OK) {
            PR_ERR("record stop failed %x", ret);
        }
    }
#endif
    return pframe->buf_size;
}

static void _vad_init()
{
    ty_vad_config_t vad_config;
    vad_config.start_threshold_ms = 300;
    vad_config.end_threshold_ms = 500;
    vad_config.silence_threshold_ms = 0;
    vad_config.sample_rate = AUDIO_SAMPLE_RATE;
    vad_config.channel = AUDIO_CHANNEL;
    vad_config.vad_frame_duration = 10;
    vad_config.scale = 1.0;
    ty_vad_app_init(&vad_config);

    PR_NOTICE("vad start");
}

static OPERATE_RET _audio_init(void)
{
    OPERATE_RET ret = 0;
    TKL_AUDIO_CONFIG_T config;

    memset(&config, 0, sizeof(TKL_AUDIO_CONFIG_T));
    config.enable = 0;
    config.ai_chn = 0;
    config.sample = AUDIO_SAMPLE_RATE;
    config.datebits = AUDIO_SAMPLE_BITS;
    config.channel = AUDIO_CHANNEL;
    config.codectype = TKL_CODEC_AUDIO_PCM;
    config.card = TKL_AUDIO_TYPE_BOARD;
    config.put_cb = _audio_frame_put;

    config.spk_sample = SPK_SAMPLE_RATE;
    config.spk_gpio = SPEAKER_ENABLE_PIN;
    config.spk_gpio_polarity = TUYA_GPIO_LEVEL_LOW;
    // open audio
    PR_NOTICE("tkl_ai_init...");

#if (CHAT_BOT_WORK_MODE == CHAT_BOT_WORK_MODE_ONE_SHOT)
    _vad_init();
    tuya_ring_buff_create(VAD_ACTIVE_RB_SIZE, OVERFLOW_PSRAM_STOP_TYPE, &sg_vad_active_rb);
#endif

    ret = tkl_ai_init(&config, 0);
    if (ret != OPRT_OK) {
        PR_ERR("tkl_ai_init fail");
        goto err;
    }

    PR_NOTICE("tkl_ai_start...");
    ret = tkl_ai_start(0, 0);
    if (ret != OPRT_OK) {
        PR_ERR("tkl_ai_start fail");
        goto err;
    }

    // set spk volume
    tuya_audio_player_set_volume(audio_volume_get());

    return OPRT_OK;
err:
    tkl_ai_stop(TKL_AUDIO_TYPE_BOARD, 0);
    tkl_ai_uninit();
    return ret;
}

static OPERATE_RET ty_audio_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;
    // deinit audio
    PR_DEBUG("tkl_ai_uninit...");
    TUYA_CALL_ERR_LOG(tkl_ai_uninit());

    return rt;
}

static void __button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    switch (event) {
    case TDL_BUTTON_PRESS_SINGLE_CLICK: {
        PR_NOTICE("%s: single click", name);
        uint8_t is_enable = app_chat_is_enable();
        app_chat_enable(!is_enable);
    } break;
    default:
        break;
    }
}

static OPERATE_RET ai_audio_trigger_pin_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    // button register
    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin = AUDIO_TRIGGER_PIN,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
        .level = TUYA_GPIO_LEVEL_LOW,
    };
    TUYA_CALL_ERR_RETURN(tdd_gpio_button_register(APP_BUTTON_NAME, &button_hw_cfg));

    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 50};
    TUYA_CALL_ERR_RETURN(tdl_button_create(APP_BUTTON_NAME, &button_cfg, &sg_button_hdl));

    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_SINGLE_CLICK, __button_function_cb);

    return OPRT_OK;
}

static OPERATE_RET ai_audio_led_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_GPIO_BASE_CFG_T out_pin_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL, .direct = TUYA_GPIO_OUTPUT, .level = TUYA_GPIO_LEVEL_HIGH};
    TUYA_CALL_ERR_RETURN(tkl_gpio_init(CHAT_LED_PIN, &out_pin_cfg));

    return OPRT_OK;
}

static void app_chat_enable(uint8_t enable)
{
    TUYA_GPIO_LEVEL_E level = enable ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW;
    sg_ai_chat.is_enable = enable;

    TY_DISPLAY_TYPE_E disp_tp;
    disp_tp = enable ? TY_DISPLAY_TP_STAT_LISTEN : TY_DISPLAY_TP_STAT_IDLE;

    tuya_display_send_msg(disp_tp, NULL, 0);

    tkl_gpio_write(CHAT_LED_PIN, level);

    return;
}

static uint8_t app_chat_is_enable(void)
{
    return sg_ai_chat.is_enable;
}

/**
 * @brief Initialize the AI audio processing module.
 *
 * This function initializes the AI audio processing module, including the audio recorder and player.
 *
 * @return OPERATE_RET - The return code of the operation.
 *         OPRT_OK on success, OPRT_COM_ERROR on communication error.
 */
OPERATE_RET tuya_ai_audio_init(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(TUYA_AUDIO_DEBUG) && (TUYA_AUDIO_DEBUG == 1)
    tuya_audio_debug_init();
#endif

    TUYA_CALL_ERR_RETURN(tuya_audio_recorder_init());
    TUYA_CALL_ERR_RETURN(tuya_audio_player_init());
    TUYA_CALL_ERR_RETURN(tuya_audio_recorder_start(&ty_ai_handle, &cfg));

    _audio_init();

    PR_DEBUG("ai_audio_trigger_pin_init");

    TUYA_CALL_ERR_RETURN(ai_audio_led_init());
    TUYA_CALL_ERR_RETURN(ai_audio_trigger_pin_init());

    return OPRT_OK;
}
