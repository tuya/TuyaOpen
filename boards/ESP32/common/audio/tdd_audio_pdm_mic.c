/**
 * @file tdd_audio_pdm_mic.c
 * @brief PDM digital microphone (mic-only) TDD audio driver for ESP32/ESP32-S3.
 *
 * Bridges an ESP-IDF I2S PDM RX channel to the TDL audio layer. Capture only:
 * there is no codec and no speaker, so play() returns OPRT_NOT_SUPPORTED.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdl_audio_driver.h"
#include "tdd_audio_pdm_mic.h"

#include "tal_memory.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_thread.h"

/* PDM RX only exists on some targets (ESP32, ESP32-S3). This file is GLOB-compiled
 * for every ESP32-family audio board, so guard the whole body and fall back to a
 * stub register() on chips without PDM RX (e.g. ESP32-C3/C6). */
#include "soc/soc_caps.h"

#if defined(SOC_I2S_SUPPORTS_PDM_RX) && SOC_I2S_SUPPORTS_PDM_RX

#include "freertos/FreeRTOS.h"

#include "driver/i2s_common.h"
#include "driver/i2s_pdm.h"

#include "audio_afe.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define I2S_READ_TIME_MS (10)
#define SAMPLE_DATABITS  (16)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDD_AUDIO_PDM_MIC_T cfg;
    TDL_AUDIO_MIC_CB    mic_cb;

    i2s_chan_handle_t rx_hdl;

    THREAD_HANDLE thrd_hdl;

    uint8_t *data_buf;      /* 16-bit PCM read buffer */
    uint32_t data_buf_len;  /* in bytes */
} PDM_MIC_HANDLE_T;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __pdm_mic_read_task(void *args)
{
    PDM_MIC_HANDLE_T *hdl = (PDM_MIC_HANDLE_T *)args;
    if (NULL == hdl) {
        PR_ERR("PDM mic read task args is NULL");
        return;
    }

    for (;;) {
        size_t bytes_read = 0;
        esp_err_t esp_rt = i2s_channel_read(hdl->rx_hdl, hdl->data_buf, hdl->data_buf_len,
                                            &bytes_read, portMAX_DELAY);
        if (esp_rt != ESP_OK || bytes_read == 0) {
            PR_ERR("PDM mic read failed: %d", esp_rt);
            tal_system_sleep(I2S_READ_TIME_MS);
            continue;
        }

        if (hdl->mic_cb) {
            hdl->mic_cb(TDL_AUDIO_FRAME_FORMAT_PCM, TDL_AUDIO_STATUS_RECEIVING, hdl->data_buf, bytes_read);
        }

        auio_afe_processor_feed(hdl->data_buf, bytes_read);
    }
}

static OPERATE_RET __tdd_audio_pdm_mic_open(TDD_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb)
{
    OPERATE_RET rt = OPRT_OK;
    PDM_MIC_HANDLE_T *hdl = (PDM_MIC_HANDLE_T *)handle;

    if (NULL == hdl) {
        return OPRT_COM_ERROR;
    }

    hdl->mic_cb = mic_cb;

    /* Allocate an RX-only I2S channel and bring it up in PDM RX mode. */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(hdl->cfg.i2s_id, I2S_ROLE_MASTER);
    if (i2s_new_channel(&chan_cfg, NULL, &hdl->rx_hdl) != ESP_OK) {
        PR_ERR("PDM mic i2s_new_channel failed");
        return OPRT_COM_ERROR;
    }

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg  = I2S_PDM_RX_CLK_DEFAULT_CONFIG(hdl->cfg.mic_sample_rate),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg =
            {
                .clk = hdl->cfg.clk_io,
                .din = hdl->cfg.din_io,
                .invert_flags = {.clk_inv = false},
            },
    };
    if (i2s_channel_init_pdm_rx_mode(hdl->rx_hdl, &pdm_rx_cfg) != ESP_OK) {
        PR_ERR("PDM mic init_pdm_rx_mode failed");
        return OPRT_COM_ERROR;
    }
    if (i2s_channel_enable(hdl->rx_hdl) != ESP_OK) {
        PR_ERR("PDM mic i2s_channel_enable failed");
        return OPRT_COM_ERROR;
    }
    PR_NOTICE("PDM mic channel started (clk=%d din=%d)", hdl->cfg.clk_io, hdl->cfg.din_io);

    /* 16-bit PCM read buffer sized for one I2S_READ_TIME_MS chunk. */
    hdl->data_buf_len = (I2S_READ_TIME_MS * hdl->cfg.mic_sample_rate / 1000) * sizeof(int16_t);
    hdl->data_buf = (uint8_t *)tal_malloc(hdl->data_buf_len);
    TUYA_CHECK_NULL_RETURN(hdl->data_buf, OPRT_MALLOC_FAILED);
    memset(hdl->data_buf, 0, hdl->data_buf_len);

    rt = audio_afe_processor_init();
    if (rt != OPRT_OK) {
        PR_ERR("audio_afe_processor_init err:%d", rt);
        return rt;
    }

    const THREAD_CFG_T thread_cfg = {
        .thrdname   = "pdm_mic_read",
        .stackDepth = 1024,
        .priority   = THREAD_PRIO_1,
    };
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&hdl->thrd_hdl, NULL, NULL, __pdm_mic_read_task,
                                                  (void *)hdl, &thread_cfg));

    return rt;
}

static OPERATE_RET __tdd_audio_pdm_mic_play(TDD_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len)
{
    /* Mic-only board: no speaker / output path. */
    (void)handle;
    (void)data;
    (void)len;
    return OPRT_NOT_SUPPORTED;
}

static OPERATE_RET __tdd_audio_pdm_mic_config(TDD_AUDIO_HANDLE_T handle, TDD_AUDIO_CMD_E cmd, void *args)
{
    /* No volume/output control on a mic-only device; accept and ignore. */
    (void)handle;
    (void)cmd;
    (void)args;
    return OPRT_OK;
}

static OPERATE_RET __tdd_audio_pdm_mic_close(TDD_AUDIO_HANDLE_T handle)
{
    PDM_MIC_HANDLE_T *hdl = (PDM_MIC_HANDLE_T *)handle;
    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);

    if (hdl->rx_hdl) {
        i2s_channel_disable(hdl->rx_hdl);
        i2s_del_channel(hdl->rx_hdl);
        hdl->rx_hdl = NULL;
    }

    return OPRT_OK;
}

OPERATE_RET tdd_audio_pdm_mic_register(char *name, TDD_AUDIO_PDM_MIC_T cfg)
{
    OPERATE_RET rt = OPRT_OK;
    PDM_MIC_HANDLE_T *_hdl = NULL;
    TDD_AUDIO_INTFS_T intfs = {0};
    TDD_AUDIO_INFO_T  info = {0};

    _hdl = (PDM_MIC_HANDLE_T *)tal_malloc(sizeof(PDM_MIC_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(_hdl, OPRT_MALLOC_FAILED);
    memset(_hdl, 0, sizeof(PDM_MIC_HANDLE_T));

    memcpy(&_hdl->cfg, &cfg, sizeof(TDD_AUDIO_PDM_MIC_T));

    info.sample_rate   = cfg.mic_sample_rate;
    info.sample_ch_num = 1;
    info.sample_bits   = SAMPLE_DATABITS;
    info.sample_tm_ms  = I2S_READ_TIME_MS;

    intfs.open   = __tdd_audio_pdm_mic_open;
    intfs.play   = __tdd_audio_pdm_mic_play;
    intfs.config = __tdd_audio_pdm_mic_config;
    intfs.close  = __tdd_audio_pdm_mic_close;

    TUYA_CALL_ERR_GOTO(tdl_audio_driver_register(name, (TDD_AUDIO_HANDLE_T)_hdl, &intfs, &info), __ERR);

    return rt;

__ERR:
    if (_hdl) {
        tal_free(_hdl);
        _hdl = NULL;
    }

    return rt;
}

#else /* !SOC_I2S_SUPPORTS_PDM_RX */

OPERATE_RET tdd_audio_pdm_mic_register(char *name, TDD_AUDIO_PDM_MIC_T cfg)
{
    (void)name;
    (void)cfg;
    PR_ERR("PDM RX not supported on this target");
    return OPRT_NOT_SUPPORTED;
}

#endif /* SOC_I2S_SUPPORTS_PDM_RX */
