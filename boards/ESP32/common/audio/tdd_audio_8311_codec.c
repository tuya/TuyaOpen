#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_system.h"
#include "esp_check.h"

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

#include "tuya_cloud_types.h"
#include "tdl_audio_driver.h"
#include "tdd_audio_8311_codec.h"

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
    TDD_AUDIO_8311_CODEC_T cfg;
    TDL_AUDIO_MIC_CB mic_cb;

    TUYA_I2S_NUM_E i2s_id;

    THREAD_HANDLE thrd_hdl;
    MUTEX_HANDLE mutex_play;

    uint8_t play_volume;

    // data buffer
    uint8_t *data_buf;
    uint32_t data_buf_len;
} ESP_I2S_8311_HANDLE_T;

static const char *TAG = "tdd_audio_8311_codec";

static i2s_chan_handle_t tx_handle_ = NULL;
static i2s_chan_handle_t rx_handle_ = NULL;
static int input_sample_rate_ = 0;
static int output_sample_rate_ = 0;
static int output_volume_ = 0;
static gpio_num_t pa_pin_ = 0;
static i2c_master_bus_handle_t codec_i2c_bus_ = NULL;
static const audio_codec_data_if_t *data_if_ = NULL;
static const audio_codec_ctrl_if_t *ctrl_if_ = NULL;
static const audio_codec_gpio_if_t *gpio_if_ = NULL;
static const audio_codec_if_t *codec_if_ = NULL;
static esp_codec_dev_handle_t output_dev_ = NULL;
static esp_codec_dev_handle_t input_dev_ = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static i2c_master_bus_handle_t __i2c_init(int i2c_num, int scl_io, int sda_io)
{
    i2c_master_bus_handle_t i2c_bus = NULL;
    esp_err_t esp_rt = ESP_OK;

    // retrieve i2c bus handle
    esp_rt = i2c_master_get_bus_handle(i2c_num, &i2c_bus);
    if (esp_rt == ESP_OK && i2c_bus) {
        ESP_LOGI(TAG, "I2C bus handle retrieved successfully");
        return i2c_bus;
    }

    // initialize i2c bus
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = i2c_num,
        .sda_io_num = sda_io,
        .scl_io_num = scl_io,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = 1,
            },
    };
    esp_rt = i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus);
    if (esp_rt != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(esp_rt));
        return NULL;
    }

    ESP_LOGI(TAG, "I2C bus initialized successfully");

    return i2c_bus;
}

static void EnableInput(bool enable)
{
    if (enable) {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = SAMPLE_DATABITS,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = (uint32_t)input_sample_rate_,
            .mclk_multiple = 0,
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(input_dev_, 40.0));
    } else {
        ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    }
}

static void SetOutputVolume(int volume)
{
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
}

static void EnableOutput(bool enable)
{
    if (enable) {
        // Play 16bit 1 channel
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = SAMPLE_DATABITS,
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
    } else {
        ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
        if (pa_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_pin_, 0);
        }
    }
}

static void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
                                 uint32_t dma_desc_num, uint32_t dma_frame_num)
{
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = dma_desc_num,
        .dma_frame_num = dma_frame_num,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

    i2s_std_config_t std_cfg = {.clk_cfg =
                                    {
                                        .sample_rate_hz = (uint32_t)output_sample_rate_,
                                        .clk_src = I2S_CLK_SRC_DEFAULT,
                                        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
#ifdef I2S_HW_VERSION_2
                                        .ext_clk_freq_hz = 0,
#endif
                                    },
                                .slot_cfg = {.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
                                             .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                                             .slot_mode = I2S_SLOT_MODE_STEREO,
                                             .slot_mask = I2S_STD_SLOT_BOTH,
                                             .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
                                             .ws_pol = false,
                                             .bit_shift = true,
#ifdef I2S_HW_VERSION_2
                                             .left_align = true,
                                             .big_endian = false,
                                             .bit_order_lsb = false
#endif
                                },
                                .gpio_cfg = {.mclk = mclk,
                                             .bclk = bclk,
                                             .ws = ws,
                                             .dout = dout,
                                             .din = din,
                                             .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false}}};

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
    ESP_LOGI(TAG, "Duplex channels created");
}

OPERATE_RET codec_8311_init(TUYA_I2S_NUM_E i2s_num, const TDD_AUDIO_8311_CODEC_T *i2s_config)
{
    OPERATE_RET rt = OPRT_OK;
    void *i2c_master_handle = NULL;
    i2c_port_t i2c_port = (i2c_port_t)(i2s_config->i2c_id);
    uint8_t es8311_addr = i2s_config->es8311_addr;
    pa_pin_ = i2s_config->gpio_output_pa;
    input_sample_rate_ = i2s_config->mic_sample_rate;
    output_sample_rate_ = i2s_config->spk_sample_rate;
    output_volume_ = i2s_config->default_volume;

    // ESP_LOGI(TAG, ">>>>>>>>mclk=%d, bclk=%d, ws=%d, dout=%d, din=%d",
    //          i2s_config->i2s_mck_io, i2s_config->i2s_bck_io, i2s_config->i2s_ws_io,
    //          i2s_config->i2s_do_io, i2s_config->i2s_di_io);
    // ESP_LOGI(TAG, ">>>>>>>>i2c_port=%d, sda=%d, scl=%d, es8311_addr=%d, pa_pin_=%d",
    //          i2c_port, i2s_config->i2c_sda_io, i2s_config->i2c_scl_io, es8311_addr, pa_pin_);
    // ESP_LOGI(TAG, ">>>>>>>>input_sample_rate=%d, output_sample_rate_=%d, volume=%d",
    //          input_sample_rate_, output_sample_rate_, output_volume_);
    // ESP_LOGI(TAG, ">>>>>>>>dma_desc_num=%ld, dma_frame_num=%ld",
    //          i2s_config->dma_desc_num, i2s_config->dma_frame_num);

    codec_i2c_bus_ = __i2c_init(i2s_config->i2c_id, i2s_config->i2c_scl_io, i2s_config->i2c_sda_io);
    i2c_master_handle = (void *)codec_i2c_bus_;
    CreateDuplexChannels(i2s_config->i2s_mck_io, i2s_config->i2s_bck_io, i2s_config->i2s_ws_io, i2s_config->i2s_do_io,
                         i2s_config->i2s_di_io, i2s_config->dma_desc_num, i2s_config->dma_frame_num);

    // Do initialize of related interface: data_if, ctrl_if and gpio_if
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = i2s_config->i2s_id,
        .rx_handle = rx_handle_,
        .tx_handle = tx_handle_,
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if_ != NULL);

    // Output
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = i2c_port,
        .addr = es8311_addr,
        .bus_handle = i2c_master_handle,
    };
    ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(ctrl_if_ != NULL);

    gpio_if_ = audio_codec_new_gpio();
    assert(gpio_if_ != NULL);

    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = ctrl_if_;
    es8311_cfg.gpio_if = gpio_if_;
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    es8311_cfg.pa_pin = pa_pin_;
    es8311_cfg.use_mclk = ((i2s_config->i2s_mck_io != -1) ? true : false);
    es8311_cfg.hw_gain.pa_voltage = 5.0;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
    codec_if_ = es8311_codec_new(&es8311_cfg);
    assert(codec_if_ != NULL);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if_,
        .data_if = data_if_,
    };
    output_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(output_dev_ != NULL);
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    input_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(input_dev_ != NULL);
    esp_codec_set_disable_when_closed(output_dev_, false);
    esp_codec_set_disable_when_closed(input_dev_, false);
    ESP_LOGI(TAG, "Es8311AudioCodec initialized");
    EnableInput(true);
    EnableOutput(true);

    return rt;
}

static OPERATE_RET tkl_i2s_8311_send(TUYA_I2S_NUM_E i2s_num, void *buff, uint32_t len)
{
    // len -> data len
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_write(output_dev_, (void *)buff, len * sizeof(int16_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s write failed. %d", ret);
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

static int tkl_i2s_8311_recv(TUYA_I2S_NUM_E i2s_num, void *buff, uint32_t len)
{
    // len -> data len
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_read(input_dev_, (void *)buff, len * sizeof(int16_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s read failed. %d", ret);
        return 0;
    }

    return (int)len;
}

static void esp32_i2s_8311_read_task(void *args)
{
    ESP_I2S_8311_HANDLE_T *hdl = (ESP_I2S_8311_HANDLE_T *)args;
    if (NULL == hdl) {
        PR_ERR("I2S 8311 read task args is NULL");
        return;
    }
    for (;;) {
        // Read data from I2S 8311
        uint32_t recv_len = hdl->data_buf_len / sizeof(int16_t);
        int data_len = tkl_i2s_8311_recv(hdl->i2s_id, hdl->data_buf, recv_len);
        if (data_len <= 0) {
            PR_ERR("I2S 8311 read failed");
            tal_system_sleep(I2S_READ_TIME_MS);
            continue;
        }

        int bytes_read = 0;

        if (hdl->mic_cb) {
            // Call the callback function with the read data
            bytes_read = data_len * sizeof(int16_t);
            hdl->mic_cb(TDL_AUDIO_FRAME_FORMAT_PCM, TDL_AUDIO_STATUS_RECEIVING, hdl->data_buf, bytes_read);
        }

        auio_afe_processor_feed(hdl->data_buf, bytes_read);

        tal_system_sleep(I2S_READ_TIME_MS);
    }
}

static OPERATE_RET __tdd_audio_esp_i2s_8311_open(TDD_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb)
{
    OPERATE_RET rt = OPRT_OK;
    ESP_I2S_8311_HANDLE_T *hdl = (ESP_I2S_8311_HANDLE_T *)handle;

    if (NULL == hdl) {
        return OPRT_COM_ERROR;
    }

    hdl->mic_cb = mic_cb;

    TDD_AUDIO_8311_CODEC_T *tdd_i2s_cfg = &hdl->cfg;

    hdl->i2s_id = TUYA_I2S_NUM_0;

    codec_8311_init(hdl->i2s_id, tdd_i2s_cfg);

    PR_NOTICE("I2S 8311 channels created");

    // data buffer
    hdl->data_buf_len = I2S_READ_TIME_MS * tdd_i2s_cfg->mic_sample_rate / 1000;
    hdl->data_buf_len = hdl->data_buf_len * sizeof(int16_t);
    PR_DEBUG("I2S 8311 data buffer len: %d", hdl->data_buf_len);
    hdl->data_buf = (uint8_t *)tal_malloc(hdl->data_buf_len);
    TUYA_CHECK_NULL_RETURN(hdl->data_buf, OPRT_MALLOC_FAILED);
    memset(hdl->data_buf, 0, hdl->data_buf_len);

    tal_mutex_create_init(&hdl->mutex_play);
    if (NULL == hdl->mutex_play) {
        PR_ERR("I2S 8311 mutex create failed");
        return OPRT_COM_ERROR;
    }

    rt = audio_afe_processor_init();
    if(rt != OPRT_OK) {
        PR_ERR("audio_afe_processor_init err:%d",  rt);
        return rt;
    }

    const THREAD_CFG_T thread_cfg = {
        .thrdname = "esp32_i2s_8311_read",
        .stackDepth = 3 * 1024,
        .priority = THREAD_PRIO_1,
    };
    PR_DEBUG("I2S 8311 read task args: %p", hdl);
    TUYA_CALL_ERR_LOG(
        tal_thread_create_and_start(&hdl->thrd_hdl, NULL, NULL, esp32_i2s_8311_read_task, (void *)hdl, &thread_cfg));

    return rt;
}

static OPERATE_RET __tdd_audio_esp_i2s_8311_play(TDD_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    ESP_I2S_8311_HANDLE_T *hdl = (ESP_I2S_8311_HANDLE_T *)handle;

    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);
    TUYA_CHECK_NULL_RETURN(hdl->mutex_play, OPRT_COM_ERROR);

    if (NULL == data || len == 0) {
        PR_ERR("I2S 8311 play data is NULL");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(hdl->mutex_play);

    uint32_t send_len = len / sizeof(int16_t);

    TUYA_CALL_ERR_LOG(tkl_i2s_8311_send(hdl->i2s_id, data, send_len));

    tal_mutex_unlock(hdl->mutex_play);

    return rt;
}

static OPERATE_RET __tdd_audio_esp_i2s_8311_config(TDD_AUDIO_HANDLE_T handle, TDD_AUDIO_CMD_E cmd, void *args)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_COM_ERROR);
    ESP_I2S_8311_HANDLE_T *hdl = (ESP_I2S_8311_HANDLE_T *)handle;

    switch (cmd) {
    case TDD_AUDIO_CMD_SET_VOLUME:
        // Set volume here
        TUYA_CHECK_NULL_GOTO(args, __EXIT);
        uint8_t volume = *(uint8_t *)args;
        if (100 < volume) {
            volume = 100;
        }

        hdl->play_volume = volume;
        SetOutputVolume(volume);
        break;
    default:
        rt = OPRT_INVALID_PARM;
        break;
    }

__EXIT:
    return rt;
}

static OPERATE_RET __tdd_audio_esp_i2s_8311_close(TDD_AUDIO_HANDLE_T handle)
{
    OPERATE_RET rt = OPRT_OK;

    return rt;
}

OPERATE_RET tdd_audio_8311_codec_register(char *name, TDD_AUDIO_8311_CODEC_T cfg)
{
    OPERATE_RET rt = OPRT_OK;
    ESP_I2S_8311_HANDLE_T *_hdl = NULL;
    TDD_AUDIO_INTFS_T intfs = {0};
    TDD_AUDIO_INFO_T info = {0};

    _hdl = (ESP_I2S_8311_HANDLE_T *)tal_malloc(sizeof(ESP_I2S_8311_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(_hdl, OPRT_MALLOC_FAILED);
    memset(_hdl, 0, sizeof(ESP_I2S_8311_HANDLE_T));

    // default play volume
    _hdl->play_volume = 80;

    info.sample_rate    = cfg.mic_sample_rate;
    info.sample_ch_num  = 1;
    info.sample_bits    = SAMPLE_DATABITS;
    info.sample_tm_ms   = I2S_READ_TIME_MS;


    memcpy(&_hdl->cfg, &cfg, sizeof(TDD_AUDIO_8311_CODEC_T));

    intfs.open = __tdd_audio_esp_i2s_8311_open;
    intfs.play = __tdd_audio_esp_i2s_8311_play;
    intfs.config = __tdd_audio_esp_i2s_8311_config;
    intfs.close = __tdd_audio_esp_i2s_8311_close;

    TUYA_CALL_ERR_GOTO(tdl_audio_driver_register(name, (TDD_AUDIO_HANDLE_T)_hdl,  &intfs, &info), __ERR);

    return rt;

__ERR:
    if (NULL == _hdl) {
        tal_free(_hdl);
        _hdl = NULL;
    }

    return rt;
}
