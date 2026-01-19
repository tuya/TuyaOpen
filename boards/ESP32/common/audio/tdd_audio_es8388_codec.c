#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_check.h"

#include "driver/gpio.h"
#include "es8388_codec.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

#include "tuya_cloud_types.h"
#include "tdl_audio_driver.h"
#include "tdd_audio_es8388_codec.h"

#include "tal_memory.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tal_mutex.h"

#include "audio_afe.h"

/***********************************************************
************************macro define************************
***********************************************************/
// I2S read time default is 10ms
#define I2S_READ_TIME_MS (10)
#define SAMPLE_DATABITS  (16)

typedef struct {
    TDD_AUDIO_ES8388_CODEC_T cfg;
    TDL_AUDIO_MIC_CB mic_cb;

    TUYA_I2S_NUM_E i2s_id;

    THREAD_HANDLE thrd_hdl;
    MUTEX_HANDLE mutex_play;

    uint8_t play_volume;

    // data buffer
    uint8_t *data_buf;
    uint32_t data_buf_len;
} ESP_I2S_ES8388_HANDLE_T;

static int input_sample_rate_ = 0;
static int output_sample_rate_ = 0;
static int output_volume_ = 0;
static gpio_num_t pa_pin_ = 0;

static const audio_codec_gpio_if_t *gpio_if_ = NULL;
static const audio_codec_ctrl_if_t *ctrl_if_ = NULL;
static const audio_codec_data_if_t *data_if_ = NULL;
static esp_codec_dev_handle_t output_dev_ = NULL;
static esp_codec_dev_handle_t input_dev_ = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

static void enable_input_device(bool enable)
{
    if (enable) {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = (uint32_t)input_sample_rate_,
            .mclk_multiple = 0,
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(input_dev_, 24.0)); // 24dB is the max gain
    } else {
        ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    }
}

static void set_output_volume(int volume)
{
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
}

static void enable_output_device(bool enable)
{
    if (enable) {
        // Play 16bit 1 channel
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = (uint32_t)output_sample_rate_,
            .mclk_multiple = 0,
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, output_volume_));
        if (pa_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_pin_, 1);
        }
        // Set analog output volume to 0dB, default is -45dB
        uint8_t reg_val = 30;              // 0dB
        uint8_t regs[] = {46, 47, 48, 49}; // HP_LVOL, HP_RVOL, SPK_LVOL, SPK_RVOL
        for (int i = 0; i < sizeof(regs); i++) {
            ctrl_if_->write_reg(ctrl_if_, regs[i], 1, &reg_val, 1);
        }
    } else {
        ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
        if (pa_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_pin_, 0);
        }
    }
}

OPERATE_RET codec_es8388_init(TDD_AUDIO_ES8388_CODEC_T *cfg)
{
    pa_pin_ = cfg->pa_pin;
    input_sample_rate_ = cfg->mic_sample_rate;
    output_sample_rate_ = cfg->spk_sample_rate;
    output_volume_ = cfg->default_volume;

    if (cfg->i2c_handle == NULL || cfg->i2s_tx_handle == NULL || cfg->i2s_rx_handle == NULL) {
        PR_ERR("i2c_handle/i2s_tx_handle/i2s_rx_handle is NULL");
        return OPRT_COM_ERROR;
    }

    // Initialize data_if and ctrl_if
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = cfg->i2s_id,
        .rx_handle = cfg->i2s_rx_handle,
        .tx_handle = cfg->i2s_tx_handle,
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if_ != NULL);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = cfg->i2c_id,
        .addr = cfg->es8388_addr,
        .bus_handle = cfg->i2c_handle,
    };
    ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(ctrl_if_ != NULL);

    if (cfg->pa_pin != GPIO_NUM_NC) {
        gpio_if_ = audio_codec_new_gpio();
        assert(gpio_if_ != NULL);
    }
    es8388_codec_cfg_t es8388_cfg = {};
    es8388_cfg.ctrl_if = ctrl_if_;
    es8388_cfg.gpio_if = gpio_if_;
    es8388_cfg.pa_pin = cfg->pa_pin;
    es8388_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    es8388_cfg.hw_gain.pa_voltage = 5.0;
    es8388_cfg.hw_gain.codec_dac_voltage = 3.3;
    const audio_codec_if_t *codec_if = es8388_codec_new(&es8388_cfg);
    assert(codec_if != NULL);

    // Output device
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if,
        .data_if = data_if_,
    };
    output_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(output_dev_ != NULL);

    // Input device
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    input_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(input_dev_ != NULL);

    esp_codec_set_disable_when_closed(output_dev_, false);
    esp_codec_set_disable_when_closed(input_dev_, false);

    PR_INFO("Input and Output channels created");

    enable_input_device(true);
    enable_output_device(true);

    return OPRT_OK;
}

static OPERATE_RET tkl_i2s_es8388_send(void *buff, uint32_t len)
{
    // len -> data len
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_write(output_dev_, (void *)buff, len);
    if (ret != ESP_OK) {
        PR_ERR("i2s write failed: %d", ret);
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

static int tkl_i2s_es8388_recv(void *buff, uint32_t len)
{
    // len -> data len
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_read(input_dev_, (void *)buff, len);
    if (ret != ESP_OK) {
        PR_ERR("i2s read failed: %d", ret);
        return 0;
    }

    return (int)len;
}

static void esp32_i2s_es8388_read_task(void *args)
{
    ESP_I2S_ES8388_HANDLE_T *hdl = (ESP_I2S_ES8388_HANDLE_T *)args;
    if (NULL == hdl) {
        PR_ERR("I2S es8388 read task args is NULL");
        return;
    }
    for (;;) {
        // Read data from I2S es8388
        int data_len = tkl_i2s_es8388_recv(hdl->data_buf, hdl->data_buf_len);
        if (data_len <= 0) {
            PR_ERR("I2S es8388 read failed");
            tal_system_sleep(I2S_READ_TIME_MS);
            continue;
        }

        if (hdl->mic_cb) {
            // Call the callback function with the read data
            hdl->mic_cb(TDL_AUDIO_FRAME_FORMAT_PCM, TDL_AUDIO_STATUS_RECEIVING, hdl->data_buf, data_len);
        }

        auio_afe_processor_feed(hdl->data_buf, (uint32_t)data_len);

        tal_system_sleep(I2S_READ_TIME_MS);
    }
}

static OPERATE_RET __tdd_audio_esp_i2s_es8388_open(TDD_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb)
{
    OPERATE_RET rt = OPRT_OK;
    ESP_I2S_ES8388_HANDLE_T *hdl = (ESP_I2S_ES8388_HANDLE_T *)handle;

    if (NULL == hdl) {
        return OPRT_COM_ERROR;
    }

    hdl->mic_cb = mic_cb;

    TDD_AUDIO_ES8388_CODEC_T *cfg = &hdl->cfg;
    if (NULL == cfg) {
        PR_ERR("I2S es8388 cfg is NULL");
        return OPRT_COM_ERROR;
    }

    TUYA_CALL_ERR_RETURN(codec_es8388_init(cfg));

    // data buffer
    hdl->data_buf_len = I2S_READ_TIME_MS * cfg->mic_sample_rate / 1000;
    hdl->data_buf_len = hdl->data_buf_len * sizeof(int16_t);
    PR_DEBUG("I2S es8388 recv buffer len: %d", hdl->data_buf_len);
    hdl->data_buf = (uint8_t *)tal_malloc(hdl->data_buf_len);
    TUYA_CHECK_NULL_RETURN(hdl->data_buf, OPRT_MALLOC_FAILED);
    memset(hdl->data_buf, 0, hdl->data_buf_len);

    tal_mutex_create_init(&hdl->mutex_play);
    if (NULL == hdl->mutex_play) {
        PR_ERR("I2S es8388 mutex create failed");
        return OPRT_COM_ERROR;
    }

    rt = audio_afe_processor_init();
    if(rt != OPRT_OK) {
        PR_ERR("audio_afe_processor_init err:%d",  rt);
        return rt;
    }

    const THREAD_CFG_T thread_cfg = {
        .thrdname = "esp32_i2s_es8388_read",
        .stackDepth = 3 * 1024,
        .priority = THREAD_PRIO_1,
    };
    PR_DEBUG("I2S es8388 read task args: %p", hdl);
    TUYA_CALL_ERR_LOG(
        tal_thread_create_and_start(&hdl->thrd_hdl, NULL, NULL, esp32_i2s_es8388_read_task, (void *)hdl, &thread_cfg));

    return rt;
}

static OPERATE_RET __tdd_audio_esp_i2s_es8388_play(TDD_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    ESP_I2S_ES8388_HANDLE_T *hdl = (ESP_I2S_ES8388_HANDLE_T *)handle;

    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);
    TUYA_CHECK_NULL_RETURN(hdl->mutex_play, OPRT_COM_ERROR);

    if (NULL == data || len == 0) {
        PR_ERR("I2S es8388 play data is NULL");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(hdl->mutex_play);

    TUYA_CALL_ERR_LOG(tkl_i2s_es8388_send(data, len));

    tal_mutex_unlock(hdl->mutex_play);

    return rt;
}

static OPERATE_RET __tdd_audio_esp_i2s_es8388_config(TDD_AUDIO_HANDLE_T handle, TDD_AUDIO_CMD_E cmd, void *args)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_COM_ERROR);
    ESP_I2S_ES8388_HANDLE_T *hdl = (ESP_I2S_ES8388_HANDLE_T *)handle;

    switch (cmd) {
    case TDD_AUDIO_CMD_SET_VOLUME:
        // Set volume here
        TUYA_CHECK_NULL_GOTO(args, __EXIT);
        uint8_t volume = *(uint8_t *)args;
        if (100 < volume) {
            volume = 100;
        }

        hdl->play_volume = volume;
        set_output_volume(volume);
        break;
    default:
        rt = OPRT_INVALID_PARM;
        break;
    }

__EXIT:
    return rt;
}

static OPERATE_RET __tdd_audio_esp_i2s_es8388_close(TDD_AUDIO_HANDLE_T handle)
{
    OPERATE_RET rt = OPRT_OK;

    return rt;
}

OPERATE_RET tdd_audio_es8388_codec_register(char *name, TDD_AUDIO_ES8388_CODEC_T cfg)
{
    OPERATE_RET rt = OPRT_OK;
    ESP_I2S_ES8388_HANDLE_T *_hdl = NULL;
    TDD_AUDIO_INTFS_T intfs = {0};
    TDD_AUDIO_INFO_T info = {0};

    _hdl = (ESP_I2S_ES8388_HANDLE_T *)tal_malloc(sizeof(ESP_I2S_ES8388_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(_hdl, OPRT_MALLOC_FAILED);
    memset(_hdl, 0, sizeof(ESP_I2S_ES8388_HANDLE_T));

    // default play volume
    _hdl->play_volume = 80;

    info.sample_rate    = cfg.mic_sample_rate;
    info.sample_ch_num  = 1;
    info.sample_bits    = SAMPLE_DATABITS;
    info.sample_tm_ms   = I2S_READ_TIME_MS;

    memcpy(&_hdl->cfg, &cfg, sizeof(TDD_AUDIO_ES8388_CODEC_T));

    intfs.open = __tdd_audio_esp_i2s_es8388_open;
    intfs.play = __tdd_audio_esp_i2s_es8388_play;
    intfs.config = __tdd_audio_esp_i2s_es8388_config;
    intfs.close = __tdd_audio_esp_i2s_es8388_close;

    TUYA_CALL_ERR_GOTO(tdl_audio_driver_register(name, (TDD_AUDIO_HANDLE_T)_hdl,  &intfs, &info), __ERR);

    return rt;

__ERR:
    if (NULL == _hdl) {
        tal_free(_hdl);
        _hdl = NULL;
    }

    return rt;
}