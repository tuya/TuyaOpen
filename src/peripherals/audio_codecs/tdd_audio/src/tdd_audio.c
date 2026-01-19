/**
 * @file tdd_audio.c
 * @brief Implementation of Tuya Device Driver layer audio interface for T5AI platform.
 *
 * This file implements the device driver interface for audio functionality on the T5AI
 * platform. It provides the implementation for audio device initialization, configuration,
 * volume control, and audio data handling. The driver supports both audio input (microphone)
 * and output (speaker) operations with configurable parameters such as sample rates,
 * data bits, and channels.
 *
 * Key functionalities include:
 * - Audio device registration and initialization
 * - Microphone data capture with callback mechanism
 * - Speaker playback with volume control
 * - Support for acoustic echo cancellation (AEC)
 * - Frame-based audio data processing
 *
 * This implementation bridges the TKL (Tuya Kernel Layer) audio APIs with the higher-level
 * TDL (Tuya Driver Layer) audio management system.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_log.h"
#include "tal_memory.h"

#include "tdd_audio.h"

#include "tkl_audio.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AUDIO_PCM_FRAME_MS       20

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDD_AUDIO_T5AI_T cfg;
    TDL_AUDIO_MIC_CB mic_cb;

    uint8_t play_volume;
} TDD_AUDIO_DATA_HANDLE_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDD_AUDIO_DATA_HANDLE_T *g_tdd_audio_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

static int __tkl_audio_frame_put(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    if (NULL == g_tdd_audio_hdl) {
        return 0;
    }

    if (g_tdd_audio_hdl->mic_cb) {
        g_tdd_audio_hdl->mic_cb(TDL_AUDIO_FRAME_FORMAT_PCM, TDL_AUDIO_STATUS_RECEIVING, (uint8_t *)pframe->pbuf,
                                pframe->used_size);
    }

    return 0;
}

static OPERATE_RET __tdd_audio_open(TDD_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_AUDIO_DATA_HANDLE_T *hdl = (TDD_AUDIO_DATA_HANDLE_T *)handle;

    if (NULL == hdl) {
        return OPRT_COM_ERROR;
    }

    hdl->mic_cb = mic_cb;

    TDD_AUDIO_T5AI_T *tdd_audio_cfg = &hdl->cfg;

    // Initialize audio here
    TKL_AUDIO_CONFIG_T config;
    memset(&config, 0, sizeof(TKL_AUDIO_CONFIG_T));

    config.enable = tdd_audio_cfg->aec_enable;
    config.ai_chn = tdd_audio_cfg->ai_chn;
    config.sample = tdd_audio_cfg->sample_rate;
    config.datebits = tdd_audio_cfg->data_bits;
    config.channel = tdd_audio_cfg->channel;
    config.codectype = TKL_CODEC_AUDIO_PCM;
    config.card = TKL_AUDIO_TYPE_BOARD;
    config.put_cb = __tkl_audio_frame_put;

    config.spk_sample = tdd_audio_cfg->spk_sample_rate;
    config.spk_gpio = tdd_audio_cfg->spk_pin;
    config.spk_gpio_polarity = tdd_audio_cfg->spk_pin_polarity;

    TUYA_CALL_ERR_RETURN(tkl_ai_init(&config, 0));
    TUYA_CALL_ERR_RETURN(tkl_ai_start(0, 0));
    // TUYA_CALL_ERR_RETURN(tkl_ai_set_vol(TKL_AUDIO_TYPE_BOARD, 0, 80));

    uint8_t volume = hdl->play_volume;
    if(volume) {
        TUYA_CALL_ERR_RETURN(tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, volume));
    }

    return rt;
}

static OPERATE_RET __tdd_audio_play(TDD_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    TDD_AUDIO_DATA_HANDLE_T *hdl = (TDD_AUDIO_DATA_HANDLE_T *)handle;

    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);

    if (NULL == data || len == 0) {
        PR_ERR("Play data is NULL");
        return OPRT_COM_ERROR;
    }

    TKL_AUDIO_FRAME_INFO_T frame;

    frame.type = TKL_AUDIO_FRAME;
    frame.codectype = TKL_CODEC_AUDIO_PCM;
    frame.sample = hdl->cfg.sample_rate;
    frame.datebits = hdl->cfg.data_bits;
    frame.channel = hdl->cfg.channel;
    frame.pbuf = (char *)data;
    frame.used_size = len;
   
    TUYA_CALL_ERR_RETURN(tkl_ao_put_frame(0, 0, NULL, &frame));

    return rt;
}

static OPERATE_RET __tdd_audio_set_volume(TDD_AUDIO_HANDLE_T handle, uint8_t volume)
{
    OPERATE_RET rt = OPRT_OK;

    TDD_AUDIO_DATA_HANDLE_T *hdl = (TDD_AUDIO_DATA_HANDLE_T *)handle;

    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);

    if (volume > 100) {
        volume = 100;
    }

    hdl->play_volume = volume;

    TUYA_CALL_ERR_RETURN(tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, volume));

    return rt;
}

static OPERATE_RET __tdd_audio_config(TDD_AUDIO_HANDLE_T handle, TDD_AUDIO_CMD_E cmd, void *args)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_COM_ERROR);

    switch (cmd) {
    case TDD_AUDIO_CMD_SET_VOLUME: {
        // Set volume here
        TUYA_CHECK_NULL_GOTO(args, __EXIT);
        uint8_t volume = *(uint8_t *)args;
        TUYA_CALL_ERR_GOTO(__tdd_audio_set_volume(handle, volume), __EXIT);
    } break;
    case TDD_AUDIO_CMD_PLAY_STOP: {
        // Stop play here
        TUYA_CALL_ERR_GOTO(tkl_ao_clear_buffer(TKL_AUDIO_TYPE_BOARD, 0), __EXIT);
    } break;
    default:
        rt = OPRT_INVALID_PARM;
        break;
    }

__EXIT:
    return rt;
}

static OPERATE_RET __tdd_audio_close(TDD_AUDIO_HANDLE_T handle)
{
    OPERATE_RET rt = OPRT_OK;

    return rt;
}

OPERATE_RET tdd_audio_register(char *name, TDD_AUDIO_T5AI_T cfg)
{
    OPERATE_RET rt = OPRT_OK;
    TDD_AUDIO_DATA_HANDLE_T *_hdl = NULL;
    TDD_AUDIO_INTFS_T intfs = {0};
    TDD_AUDIO_INFO_T info = {0};

    _hdl = (TDD_AUDIO_DATA_HANDLE_T *)tal_malloc(sizeof(TDD_AUDIO_DATA_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(_hdl, OPRT_MALLOC_FAILED);
    memset(_hdl, 0, sizeof(TDD_AUDIO_DATA_HANDLE_T));
    g_tdd_audio_hdl = _hdl;

    // default play volume
    _hdl->play_volume = 80;

    memcpy(&_hdl->cfg, &cfg, sizeof(TDD_AUDIO_T5AI_T));

    info.sample_rate   = cfg.sample_rate;
    info.sample_ch_num = cfg.channel;
    info.sample_bits   = cfg.data_bits;
    info.sample_tm_ms  = AUDIO_PCM_FRAME_MS;

    intfs.open = __tdd_audio_open;
    intfs.play = __tdd_audio_play;
    intfs.config = __tdd_audio_config;
    intfs.close = __tdd_audio_close;

    TUYA_CALL_ERR_GOTO(tdl_audio_driver_register(name, (TDD_AUDIO_HANDLE_T)_hdl, &intfs, &info), __ERR);

    return rt;

__ERR:
    if (NULL == _hdl) {
        tal_free(_hdl);
        _hdl = NULL;
    }

    return rt;
}
