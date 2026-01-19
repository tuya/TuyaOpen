/**
 * @file tdd_audio_no_codec.c
 * @brief tdd_audio_no_codec module is used to
 * @version 0.1
 * @date 2025-04-08
 */
#include "math.h"

#include "tdl_audio_driver.h"
#include "tdd_audio_no_codec.h"

#include "tal_memory.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tal_mutex.h"

#include "tkl_i2s.h"

#include "audio_afe.h"

/***********************************************************
************************macro define************************
***********************************************************/
// I2S read time default is 10ms
#define I2S_READ_TIME_MS (10)
#define SAMPLE_DATABITS  (16)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDD_AUDIO_NO_CODEC_T cfg;
    TDL_AUDIO_MIC_CB mic_cb;

    TUYA_I2S_NUM_E i2s_tx_id;
    TUYA_I2S_NUM_E i2s_rx_id;

    THREAD_HANDLE thrd_hdl;
    MUTEX_HANDLE mutex_play;

    uint8_t play_volume;

    // data buffer
    uint8_t *raw_data_buf;
    uint32_t raw_data_buf_len;

    uint8_t *data_buf;
    uint32_t data_buf_len;
} ESP_I2S_HANDLE_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

static void esp32_i2s_read_task(void *args)
{
    ESP_I2S_HANDLE_T *hdl = (ESP_I2S_HANDLE_T *)args;
    if (NULL == hdl) {
        PR_ERR("I2S read task args is NULL");
        return;
    }
    for (;;) {
        // Read data from I2S
        int bytes_read = tkl_i2s_recv(hdl->i2s_rx_id, hdl->raw_data_buf, hdl->raw_data_buf_len);
        if (bytes_read <= 0) {
            PR_ERR("I2S read failed");
            tal_system_sleep(I2S_READ_TIME_MS);
            continue;
        }

        uint32_t samples = bytes_read / sizeof(int32_t);

        // 32bit to 16bit
        int32_t *p_raw_data = (int32_t *)(hdl->raw_data_buf);
        int16_t *p_data = (int16_t *)(hdl->data_buf);
        for (int i = 0; i < samples; i++) {
            // Convert 32bit to 16bit
            int32_t tmp_value = p_raw_data[i] >> 14;
            p_data[i] = (tmp_value > INT16_MAX)    ? INT16_MAX
                        : (tmp_value < -INT16_MAX) ? -INT16_MAX
                                                   : (int16_t)tmp_value;
        }

        if (hdl->mic_cb) {
            // Call the callback function with the read data
            hdl->mic_cb(TDL_AUDIO_FRAME_FORMAT_PCM, TDL_AUDIO_STATUS_RECEIVING, hdl->data_buf,
                        samples * sizeof(int16_t));
        }

        auio_afe_processor_feed(hdl->data_buf, samples * sizeof(int16_t));

        tal_system_sleep(I2S_READ_TIME_MS);
    }
}

static OPERATE_RET __tdd_audio_no_codec_open(TDD_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb)
{
    OPERATE_RET rt = OPRT_OK;
    ESP_I2S_HANDLE_T *hdl = (ESP_I2S_HANDLE_T *)handle;

    if (NULL == hdl) {
        return OPRT_COM_ERROR;
    }

    hdl->mic_cb = mic_cb;

    TDD_AUDIO_NO_CODEC_T *tdd_i2s_cfg = &hdl->cfg;

    // Initialize I2S here

    hdl->i2s_rx_id = TUYA_I2S_NUM_0;
    hdl->i2s_tx_id = TUYA_I2S_NUM_1;

    TUYA_I2S_BASE_CFG_T i2s_rx_cfg = {0};
    i2s_rx_cfg.mode = TUYA_I2S_MODE_MASTER | TUYA_I2S_MODE_RX;
    i2s_rx_cfg.sample_rate = 16000;
    i2s_rx_cfg.bits_per_sample = TUYA_I2S_BITS_PER_SAMPLE_32BIT;
    tkl_i2s_init(hdl->i2s_rx_id, &i2s_rx_cfg);

    TUYA_I2S_BASE_CFG_T i2s_tx_cfg = {0};
    i2s_tx_cfg.mode = TUYA_I2S_MODE_MASTER | TUYA_I2S_MODE_TX;
    i2s_tx_cfg.sample_rate = 16000;
    i2s_tx_cfg.bits_per_sample = TUYA_I2S_BITS_PER_SAMPLE_32BIT;
    tkl_i2s_init(hdl->i2s_tx_id, &i2s_tx_cfg);

    PR_NOTICE("I2S channels created");

    // data buffer
    hdl->data_buf_len = I2S_READ_TIME_MS * tdd_i2s_cfg->mic_sample_rate / 1000;
    hdl->data_buf_len = hdl->data_buf_len * sizeof(int16_t);
    PR_DEBUG("I2S data buffer len: %d", hdl->data_buf_len);
    hdl->data_buf = (uint8_t *)tal_malloc(hdl->data_buf_len);
    TUYA_CHECK_NULL_RETURN(hdl->data_buf, OPRT_MALLOC_FAILED);
    memset(hdl->data_buf, 0, hdl->data_buf_len);

    // raw data buffer
    hdl->raw_data_buf_len = I2S_READ_TIME_MS * tdd_i2s_cfg->mic_sample_rate / 1000;
    hdl->raw_data_buf_len = hdl->raw_data_buf_len * sizeof(int32_t);
    PR_DEBUG("I2S raw data buffer len: %d", hdl->raw_data_buf_len);
    hdl->raw_data_buf = (uint8_t *)tal_malloc(hdl->raw_data_buf_len);
    TUYA_CHECK_NULL_RETURN(hdl->raw_data_buf, OPRT_MALLOC_FAILED);
    memset(hdl->raw_data_buf, 0, hdl->raw_data_buf_len);

    tal_mutex_create_init(&hdl->mutex_play);
    if (NULL == hdl->mutex_play) {
        PR_ERR("I2S mutex create failed");
        return OPRT_COM_ERROR;
    }

    rt = audio_afe_processor_init();
    if(rt != OPRT_OK) {
        PR_ERR("audio_afe_processor_init err:%d",  rt);
        return rt;
    }

    const THREAD_CFG_T thread_cfg = {
        .thrdname = "esp32_i2s_read",
        .stackDepth = 3 * 1024,
        .priority = THREAD_PRIO_1,
    };
    PR_DEBUG("I2S read task args: %p", hdl);
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&hdl->thrd_hdl, NULL, NULL,\
                                                 esp32_i2s_read_task, \
                                                 (void *)hdl, &thread_cfg));


    return rt;
}

static OPERATE_RET __tdd_audio_no_codec_play(TDD_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    ESP_I2S_HANDLE_T *hdl = (ESP_I2S_HANDLE_T *)handle;

    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);
    // TUYA_CHECK_NULL_RETURN(hdl->tx_hdl, OPRT_COM_ERROR);
    TUYA_CHECK_NULL_RETURN(hdl->mutex_play, OPRT_COM_ERROR);

    if (NULL == data || len == 0) {
        PR_ERR("I2S play data is NULL");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(hdl->mutex_play);

    int16_t *p_data = (int16_t *)data;
    uint32_t send_len = len / sizeof(int16_t);

    int32_t *p_send_data = tal_malloc(send_len * sizeof(int32_t));
    TUYA_CHECK_NULL_GOTO(p_send_data, __EXIT);

    // Convert 16bit to 32bit
    int32_t vol_factor = pow(((double)(hdl->play_volume) / 100.0), 2) * 65536;
    for (int i = 0; i < send_len; i++) {
        int64_t tmp = (int64_t)(p_data[i] * vol_factor);
        if (tmp > INT32_MAX) {
            tmp = INT32_MAX;
        } else if (tmp < INT32_MIN) {
            tmp = INT32_MIN;
        }
        p_send_data[i] = (int32_t)(tmp);
    }
    // size_t bytes_written;
    // esp_err_t esp_rt = i2s_channel_write(hdl->tx_hdl, p_send_data, send_len * sizeof(int32_t), &bytes_written,
    // portMAX_DELAY); if (esp_rt != ESP_OK) {
    //     PR_ERR("I2S write failed");
    //     rt = OPRT_COM_ERROR;
    //     goto __EXIT;
    // }

    TUYA_CALL_ERR_LOG(tkl_i2s_send(hdl->i2s_tx_id, p_send_data, send_len * sizeof(int32_t)));

__EXIT:
    if (p_send_data) {
        tal_free(p_send_data);
        p_send_data = NULL;
    }

    tal_mutex_unlock(hdl->mutex_play);

    return rt;
}

static OPERATE_RET __tdd_audio_no_codec_set_volume(TDD_AUDIO_HANDLE_T handle, uint8_t volume)
{
    OPERATE_RET rt = OPRT_OK;

    ESP_I2S_HANDLE_T *hdl = (ESP_I2S_HANDLE_T *)handle;

    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);

    if (volume > 100) {
        volume = 100;
    }

    hdl->play_volume = volume;

    return rt;
}

static OPERATE_RET __tdd_audio_no_codec_config(TDD_AUDIO_HANDLE_T handle, TDD_AUDIO_CMD_E cmd, void *args)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_COM_ERROR);

    switch (cmd) {
    case TDD_AUDIO_CMD_SET_VOLUME:
        // Set volume here
        TUYA_CHECK_NULL_GOTO(args, __EXIT);
        uint8_t volume = *(uint8_t *)args;
        TUYA_CALL_ERR_GOTO(__tdd_audio_no_codec_set_volume(handle, volume), __EXIT);
        break;
    default:
        rt = OPRT_INVALID_PARM;
        break;
    }

__EXIT:
    return rt;
}

static OPERATE_RET __tdd_audio_no_codec_close(TDD_AUDIO_HANDLE_T handle)
{
    OPERATE_RET rt = OPRT_OK;

    return rt;
}

OPERATE_RET tdd_audio_no_codec_register(char *name, TDD_AUDIO_NO_CODEC_T cfg)
{
    OPERATE_RET rt = OPRT_OK;
    ESP_I2S_HANDLE_T *_hdl = NULL;
    TDD_AUDIO_INTFS_T intfs = {0};
    TDD_AUDIO_INFO_T info = {0};

    _hdl = (ESP_I2S_HANDLE_T *)tal_malloc(sizeof(ESP_I2S_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(_hdl, OPRT_MALLOC_FAILED);
    memset(_hdl, 0, sizeof(ESP_I2S_HANDLE_T));

    // default play volume
    _hdl->play_volume = 80;

    info.sample_rate    = cfg.mic_sample_rate;
    info.sample_ch_num  = 1;
    info.sample_bits    = SAMPLE_DATABITS;
    info.sample_tm_ms   = I2S_READ_TIME_MS;

    memcpy(&_hdl->cfg, &cfg, sizeof(TDD_AUDIO_NO_CODEC_T));

    intfs.open = __tdd_audio_no_codec_open;
    intfs.play = __tdd_audio_no_codec_play;
    intfs.config = __tdd_audio_no_codec_config;
    intfs.close = __tdd_audio_no_codec_close;

    TUYA_CALL_ERR_GOTO(tdl_audio_driver_register(name, (TDD_AUDIO_HANDLE_T)_hdl,  &intfs, &info), __ERR);

    return rt;

__ERR:
    if (NULL == _hdl) {
        tal_free(_hdl);
        _hdl = NULL;
    }

    return rt;
}
