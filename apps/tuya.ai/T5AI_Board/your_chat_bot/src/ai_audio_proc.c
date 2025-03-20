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

#include "tal_api.h"
#include "ai_audio_proc.h"
#include "tuya_audio_player.h"
#include "tuya_audio_recorder.h"

#include "tuya_audio_debug.h"
#include "tkl_video_in.h"
#include "tkl_gpio.h"

#define AUDIO_SAMPLE_RATE 16000
#define SPK_SAMPLE_RATE   16000
#define AUDIO_SAMPLE_BITS 16
#define AUDIO_CHANNEL     1

#define AUDIO_TTS_STREAM_BUFF_MAX_LEN (1024 * 64)
#define AUDIO_PCM_SLICE_BUFF_LEN      (320)
#define AUDIO_PCM_SLICE_TIME          (AUDIO_PCM_SLICE_BUFF_LEN / 2 / (AUDIO_SAMPLE_RATE / 1000))

#define SPEAKER_ENABLE_PIN TUYA_GPIO_NUM_28
#define AUDIO_TRIGGER_PIN  TUYA_GPIO_NUM_12

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

static void app_proc_task(void *arg);

static void _vi_init(void)
{
    TKL_VI_CONFIG_T vi_config;
    TKL_VI_EXT_CONFIG_T ext_conf;

    ext_conf.type = TKL_VI_EXT_CONF_CAMERA;
    ext_conf.camera.camera_type = TKL_VI_CAMERA_TYPE_UVC;
    ext_conf.camera.fmt = TKL_CODEC_VIDEO_MJPEG;
    ext_conf.camera.power_pin = TUYA_GPIO_NUM_56;
    ext_conf.camera.active_level = TUYA_GPIO_LEVEL_HIGH;

    vi_config.isp.width = 864;
    vi_config.isp.height = 480;
    vi_config.isp.fps = 15;
    vi_config.pdata = &ext_conf;

    tkl_vi_init(&vi_config, 0);
}

static void _vi_deinit(void)
{
    tkl_vi_uninit(TKL_VI_CAMERA_TYPE_UVC);
}

static BOOL_T audio_trigger_pin_is_pressed(void)
{
    TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_HIGH;
    tkl_gpio_read(AUDIO_TRIGGER_PIN, &level);
    return (level == TUYA_GPIO_LEVEL_LOW) ? TRUE : FALSE;
}

static int _audio_frame_put(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    static BOOL_T key_status_old = FALSE;
    static BOOL_T alert_flag = FALSE;
    BOOL_T is_press = FALSE;
    static TUYA_AUDIO_VOICE_STATE state = VOICE_STATE_IN_IDLE;
    OPERATE_RET ret;
    tuya_iot_client_t *client = tuya_iot_client_get();

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

    return pframe->buf_size;
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

    _vi_init(); // FIXME: open uvc to avoid system suspend

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

    // set mic volume
    tkl_ai_set_vol(TKL_AUDIO_TYPE_BOARD, 0, 50);

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

    _vi_deinit();
    return rt;
}

static OPERATE_RET ai_audio_trigger_pin_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    // init gpio
    TUYA_GPIO_BASE_CFG_T key_cfg = {.mode = TUYA_GPIO_PULLUP, .direct = TUYA_GPIO_INPUT, .level = TUYA_GPIO_LEVEL_HIGH};
    TUYA_CALL_ERR_LOG(tkl_gpio_init(AUDIO_TRIGGER_PIN, &key_cfg));

    return OPRT_OK;
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
    OPERATE_RET ret = OPRT_OK;

#if defined(TUYA_AUDIO_DEBUG) && (TUYA_AUDIO_DEBUG == 1)
    tuya_audio_debug_init();
#endif

    if (tuya_audio_recorder_init()) {
        PR_ERR("tuya_audio_recorder_init failed");
        return OPRT_COM_ERROR;
    }

    if (tuya_audio_player_init()) {
        PR_ERR("tuya_audio_player_init failed");
        return OPRT_COM_ERROR;
    }

    if (tuya_audio_recorder_start(&ty_ai_handle, &cfg)) {
        return OPRT_COM_ERROR;
    }

    _audio_init();

    PR_DEBUG("ai_audio_trigger_pin_init");
    ret = ai_audio_trigger_pin_init();
    if (ret != OPRT_OK) {
        PR_ERR("ai_audio_trigger_pin_init failed");
        return ret;
    }

    return OPRT_OK;
}
