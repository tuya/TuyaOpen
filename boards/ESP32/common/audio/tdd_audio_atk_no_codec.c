#include <stdio.h>
#include <string.h>
#include <math.h>
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
#include "tdd_audio_atk_no_codec.h"

#include "tal_memory.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tal_mutex.h"

/***********************************************************
************************macro define************************
***********************************************************/
// I2S read time default is 10ms
#define I2S_READ_TIME_MS (10)

typedef struct {
    TDD_AUDIO_ATK_NO_CODEC_T cfg;
    TDL_AUDIO_MIC_CB mic_cb;

    TUYA_I2S_NUM_E i2s_id;

    THREAD_HANDLE thrd_hdl;
    MUTEX_HANDLE mutex_play;

    uint8_t play_volume;

    // data buffer
    uint8_t *data_buf;
    uint32_t data_buf_len;
} ATK_NO_CODEC_HANDLE_T;

static const char *TAG = "tdd_audio_atk_no_codec";

static i2s_chan_handle_t tx_handle_ = NULL;
static i2s_chan_handle_t rx_handle_ = NULL;
static int input_sample_rate_ = 0;
static int output_sample_rate_ = 0;
static int output_volume_ = 0;
static i2c_master_bus_handle_t codec_i2c_bus_ = NULL;
static i2c_master_dev_handle_t i2c_device_ = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static void InitializeCodecI2c(const TDD_AUDIO_ATK_NO_CODEC_T *i2s_config)
{
    // Initialize I2C peripheral
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = i2s_config->i2c_id,
        .sda_io_num = i2s_config->i2c_sda_io,
        .scl_io_num = i2s_config->i2c_scl_io,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = 1,
            },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
}

static void __WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t buffer[2] = {reg, value};
    ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_, buffer, 2, 100));
}

static uint8_t __ReadReg(uint8_t reg)
{
    uint8_t buffer[1];
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_device_, &reg, 1, buffer, 1, 100));
    return buffer[0];
}

static void __SetOutputState(uint8_t bit, uint8_t level)
{
    uint16_t data;
    if (bit < 8) {
        data = __ReadReg(0x02);
    } else {
        data = __ReadReg(0x03);
        bit -= 8;
    }

    data = (data & ~(1 << bit)) | (level << bit);

    if (bit < 8) {
        __WriteReg(0x02, data);
    } else {
        __WriteReg(0x03, data);
    }
}

static void xl9555_in_setup(i2c_master_bus_handle_t i2c_bus, uint8_t addr)
{
    i2c_device_config_t i2c_device_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400 * 1000,
        .scl_wait_us = 0,
        .flags =
            {
                .disable_ack_check = 0,
            },
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &i2c_device_cfg, &i2c_device_));
    assert(i2c_device_ != NULL);

    __WriteReg(0x06, 0x3B);
    __WriteReg(0x07, 0xFE);

    __WriteReg(0x06, 0x1B);
    __WriteReg(0x07, 0xFE);

    __SetOutputState(5, 1);
    __SetOutputState(7, 1);
}

static void SetOutputVolume(int volume)
{
    output_volume_ = volume;
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

    esp_err_t esp_rt = i2s_channel_init_std_mode(tx_handle_, &std_cfg);
    if ((esp_rt != ESP_OK) || (NULL == tx_handle_)) {
        ESP_LOGE(TAG, "Init tx handle failed.");
        return;
    }

    esp_rt = i2s_channel_init_std_mode(rx_handle_, &std_cfg);
    if ((esp_rt != ESP_OK) || (NULL == rx_handle_)) {
        ESP_LOGE(TAG, "Init rx handle failed.");
        return;
    }

    ESP_LOGI(TAG, "Duplex channels created");
}

OPERATE_RET atk_no_codec_init(TUYA_I2S_NUM_E i2s_num, const TDD_AUDIO_ATK_NO_CODEC_T *i2s_config)
{
    OPERATE_RET rt = OPRT_OK;
    input_sample_rate_ = i2s_config->mic_sample_rate;
    output_sample_rate_ = i2s_config->spk_sample_rate;
    output_volume_ = i2s_config->default_volume;

    // ESP_LOGI(TAG, ">>>>>>>>mclk=%d, bclk=%d, ws=%d, dout=%d, din=%d",
    //          i2s_config->i2s_mck_io, i2s_config->i2s_bck_io, i2s_config->i2s_ws_io,
    //          i2s_config->i2s_do_io, i2s_config->i2s_di_io);
    // ESP_LOGI(TAG, ">>>>>>>>input_sample_rate=%d, output_sample_rate_=%d, volume=%d",
    //          input_sample_rate_, output_sample_rate_, output_volume_);
    // ESP_LOGI(TAG, ">>>>>>>>dma_desc_num=%ld, dma_frame_num=%ld",
    //          i2s_config->dma_desc_num, i2s_config->dma_frame_num);

    InitializeCodecI2c(i2s_config);
    xl9555_in_setup(codec_i2c_bus_, 0x20);
    CreateDuplexChannels(i2s_config->i2s_mck_io, i2s_config->i2s_bck_io, i2s_config->i2s_ws_io, i2s_config->i2s_do_io,
                         i2s_config->i2s_di_io, i2s_config->dma_desc_num, i2s_config->dma_frame_num);

    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
    return rt;
}

static int atk_no_codec_write(const int16_t *data, int samples)
{
    if (NULL == tx_handle_) {
        PR_ERR("atk_no_codec has not been initialized yet.");
        return 0;
    }
    int32_t *buffer = tal_malloc(samples * sizeof(int32_t));
    if (NULL == buffer) {
        PR_ERR("atk_no_codec_write malloc failed.");
        return 0;
    }

    // Convert 16bit to 32bit
    int32_t volume_factor = pow(((double)(output_volume_) / 100.0), 2) * 65536;
    for (int i = 0; i < samples; i++) {
        int64_t temp = (int64_t)(data[i]) * volume_factor;
        if (temp > INT32_MAX) {
            buffer[i] = INT32_MAX;
        } else if (temp < INT32_MIN) {
            buffer[i] = INT32_MIN;
        } else {
            buffer[i] = (int32_t)(temp);
        }
    }

    size_t bytes_written;
    esp_err_t esp_rt = i2s_channel_write(tx_handle_, buffer, samples * sizeof(int32_t), &bytes_written, portMAX_DELAY);
    if ((esp_rt != ESP_OK) || (0 >= bytes_written)) {
        PR_ERR("I2S write failed");
        tal_free(buffer);
        return 0;
    }

    tal_free(buffer);
    return bytes_written / sizeof(int32_t);
}

static int atk_no_codec_read(int16_t *dest, int samples)
{
    if (NULL == rx_handle_) {
        PR_ERR("atk_no_codec has not been initialized yet.");
        return 0;
    }
    int32_t *bit32_buffer = tal_malloc(samples * sizeof(int32_t));
    if (NULL == bit32_buffer) {
        PR_ERR("atk_no_codec_read malloc failed.");
        return 0;
    }
    size_t bytes_read;

    if (i2s_channel_read(rx_handle_, bit32_buffer, samples * sizeof(int32_t), &bytes_read, portMAX_DELAY) != ESP_OK) {
        ESP_LOGE(TAG, "Read Failed!");
        tal_free(bit32_buffer);
        return 0;
    }

    samples = bytes_read / sizeof(int32_t);
    for (int i = 0; i < samples; i++) {
        int32_t value = bit32_buffer[i] >> 12;
        dest[i] = (value > INT16_MAX) ? INT16_MAX : (value < -INT16_MAX) ? -INT16_MAX : (int16_t)value;
    }

    tal_free(bit32_buffer);
    return samples;
}

static void atk_no_codec_read_task(void *args)
{
    ATK_NO_CODEC_HANDLE_T *hdl = (ATK_NO_CODEC_HANDLE_T *)args;
    if (NULL == hdl) {
        PR_ERR("I2S read task args is NULL");
        return;
    }
    for (;;) {
        // Read data from I2S
        uint32_t recv_len = hdl->data_buf_len / sizeof(int16_t);
        int data_len = atk_no_codec_read((int16_t *)(hdl->data_buf), recv_len);
        if (data_len <= 0) {
            PR_ERR("I2S read failed");
            tal_system_sleep(I2S_READ_TIME_MS);
            continue;
        }

        if (hdl->mic_cb) {
            // Call the callback function with the read data
            int bytes_read = data_len * sizeof(int16_t);
            hdl->mic_cb(TDL_AUDIO_FRAME_FORMAT_PCM, TDL_AUDIO_STATUS_RECEIVING, hdl->data_buf, bytes_read);
        }

        tal_system_sleep(I2S_READ_TIME_MS);
    }
}

static OPERATE_RET __tdd_atk_no_codec_open(TDD_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb)
{
    OPERATE_RET rt = OPRT_OK;
    ATK_NO_CODEC_HANDLE_T *hdl = (ATK_NO_CODEC_HANDLE_T *)handle;

    if (NULL == hdl) {
        return OPRT_COM_ERROR;
    }

    hdl->mic_cb = mic_cb;

    TDD_AUDIO_ATK_NO_CODEC_T *tdd_i2s_cfg = &hdl->cfg;

    hdl->i2s_id = TUYA_I2S_NUM_0;

    atk_no_codec_init(hdl->i2s_id, tdd_i2s_cfg);

    PR_NOTICE("I2S channels created");

    // data buffer
    hdl->data_buf_len = I2S_READ_TIME_MS * tdd_i2s_cfg->mic_sample_rate / 1000;
    hdl->data_buf_len = hdl->data_buf_len * sizeof(int16_t);
    PR_DEBUG("I2S data buffer len: %d", hdl->data_buf_len);
    hdl->data_buf = (uint8_t *)tal_malloc(hdl->data_buf_len);
    TUYA_CHECK_NULL_RETURN(hdl->data_buf, OPRT_MALLOC_FAILED);
    memset(hdl->data_buf, 0, hdl->data_buf_len);

    tal_mutex_create_init(&hdl->mutex_play);
    if (NULL == hdl->mutex_play) {
        PR_ERR("I2S mutex create failed");
        return OPRT_COM_ERROR;
    }

    const THREAD_CFG_T thread_cfg = {
        .thrdname = "atk_no_codec_read",
        .stackDepth = 3 * 1024,
        .priority = THREAD_PRIO_1,
    };
    PR_DEBUG("I2S read task args: %p", hdl);
    TUYA_CALL_ERR_LOG(
        tal_thread_create_and_start(&hdl->thrd_hdl, NULL, NULL, atk_no_codec_read_task, (void *)hdl, &thread_cfg));

    return rt;
}

static OPERATE_RET __tdd_atk_no_codec_play(TDD_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    ATK_NO_CODEC_HANDLE_T *hdl = (ATK_NO_CODEC_HANDLE_T *)handle;

    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);
    TUYA_CHECK_NULL_RETURN(hdl->mutex_play, OPRT_COM_ERROR);

    if (NULL == data || len == 0) {
        PR_ERR("I2S play data is NULL");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(hdl->mutex_play);

    uint32_t send_len = len / sizeof(int16_t);

    int write_len = atk_no_codec_write((const int16_t *)data, send_len);

    tal_mutex_unlock(hdl->mutex_play);

    if (0 == write_len) {
        return OPRT_COM_ERROR;
    }

    return rt;
}

static OPERATE_RET __tdd_atk_no_codec_config(TDD_AUDIO_HANDLE_T handle, TDD_AUDIO_CMD_E cmd, void *args)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_COM_ERROR);
    ATK_NO_CODEC_HANDLE_T *hdl = (ATK_NO_CODEC_HANDLE_T *)handle;

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

static OPERATE_RET __tdd_atk_no_codec_close(TDD_AUDIO_HANDLE_T handle)
{
    OPERATE_RET rt = OPRT_OK;

    return rt;
}

OPERATE_RET tdd_audio_atk_no_codec_register(char *name, TDD_AUDIO_ATK_NO_CODEC_T cfg)
{
    OPERATE_RET rt = OPRT_OK;
    ATK_NO_CODEC_HANDLE_T *_hdl = NULL;

    TDD_AUDIO_INTFS_T intfs = {0};

    _hdl = (ATK_NO_CODEC_HANDLE_T *)tal_malloc(sizeof(ATK_NO_CODEC_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(_hdl, OPRT_MALLOC_FAILED);
    memset(_hdl, 0, sizeof(ATK_NO_CODEC_HANDLE_T));

    // default play volume
    _hdl->play_volume = 80;

    memcpy(&_hdl->cfg, &cfg, sizeof(TDD_AUDIO_ATK_NO_CODEC_T));

    intfs.open = __tdd_atk_no_codec_open;
    intfs.play = __tdd_atk_no_codec_play;
    intfs.config = __tdd_atk_no_codec_config;
    intfs.close = __tdd_atk_no_codec_close;

    TUYA_CALL_ERR_GOTO(tdl_audio_driver_register(name, &intfs, (TDD_AUDIO_HANDLE_T)_hdl), __ERR);

    return rt;

__ERR:
    if (NULL == _hdl) {
        tal_free(_hdl);
        _hdl = NULL;
    }

    return rt;
}
