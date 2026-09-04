/**
 * @file tdd_audio_pdm_i2s_spk.c
 * @brief PDM microphone + I2S STD speaker (MAX98357A) TDD audio driver for ESP32-S3.
 *
 * PDM RX and I2S STD TX use separate I2S ports so onboard PDM mic and an external
 * MAX98357A amp can run concurrently without sharing BCLK/WS lines.
 *
 * MAX98357A notes (same lessons as NICEMCU_T5_DEV):
 * - Amp stays unmuted when SD is tied to 3V3, so TX must never underrun with
 *   garbage DMA. Use auto_clear_after_cb and prime silence after enable.
 * - Philips I2S 16-bit stereo with L=R matches the T5 continuous-silence path.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdl_audio_driver.h"
#include "tdd_audio_pdm_i2s_spk.h"

#include "tal_memory.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tal_mutex.h"
#include "tkl_gpio.h"

#include "soc/soc_caps.h"

#if defined(SOC_I2S_SUPPORTS_PDM_RX) && SOC_I2S_SUPPORTS_PDM_RX

#include <limits.h>
#include <string.h>

#include "freertos/FreeRTOS.h"

#include "driver/i2s_common.h"
#include "driver/i2s_pdm.h"
#include "driver/i2s_std.h"

#include "audio_afe.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define I2S_READ_TIME_MS   (10)
#define SAMPLE_DATABITS    (16)
#define SPK_PRIME_MS       (20) /* silence written right after TX enable */
#define SPK_DMA_DESC_NUM   (6)
#define SPK_DMA_FRAME_NUM  (240)

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    TDD_AUDIO_PDM_I2S_SPK_T cfg;
    TDL_AUDIO_MIC_CB          mic_cb;

    i2s_chan_handle_t rx_hdl;
    i2s_chan_handle_t tx_hdl;

    THREAD_HANDLE thrd_hdl;
    MUTEX_HANDLE  mutex_play;

    uint8_t play_volume;

    uint8_t *data_buf;
    uint32_t data_buf_len;
} PDM_I2S_SPK_HANDLE_T;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Configure optional MAX98357 SD (shutdown) pin
 * @param[in] hdl driver handle
 * @param[in] enable true to unmute amp
 * @return none
 */
static void __spk_sd_set(PDM_I2S_SPK_HANDLE_T *hdl, bool enable)
{
    if (NULL == hdl) {
        return;
    }

    if (hdl->cfg.spk_sd_pin >= TUYA_GPIO_NUM_MAX) {
        return;
    }

    bool active = (hdl->cfg.spk_sd_polarity != 0);
    tkl_gpio_write(hdl->cfg.spk_sd_pin, (enable == active) ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW);
}

/**
 * @brief Init optional MAX98357 SD GPIO (start muted)
 * @param[in] hdl driver handle
 * @return OPRT_OK on success
 */
static OPERATE_RET __spk_sd_init(PDM_I2S_SPK_HANDLE_T *hdl)
{
    if (NULL == hdl) {
        return OPRT_INVALID_PARM;
    }

    if (hdl->cfg.spk_sd_pin >= TUYA_GPIO_NUM_MAX) {
        return OPRT_OK;
    }

    TUYA_GPIO_BASE_CFG_T cfg = {
        .mode   = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level  = TUYA_GPIO_LEVEL_LOW,
    };
    OPERATE_RET rt = tkl_gpio_init(hdl->cfg.spk_sd_pin, &cfg);
    if (rt != OPRT_OK) {
        return rt;
    }
    /* Keep muted until I2S TX is primed with silence (same as T5). */
    __spk_sd_set(hdl, false);
    return OPRT_OK;
}

/**
 * @brief Apply 0..100 volume to a PCM sample
 * @param[in] sample input sample
 * @param[in] volume 0..100
 * @return scaled sample
 */
static int16_t __apply_volume(int16_t sample, uint8_t volume)
{
    uint32_t v = volume;
    if (v > 100) {
        v = 100;
    }
    int32_t out = ((int32_t)sample * (int32_t)v) / 100;
    if (out > INT16_MAX) {
        out = INT16_MAX;
    } else if (out < INT16_MIN) {
        out = INT16_MIN;
    }
    return (int16_t)out;
}

/**
 * @brief Pack mono PCM into 16-bit stereo frames (L=R) for MAX98357A
 * @param[in] src mono PCM
 * @param[out] dst interleaved L/R int16 pairs
 * @param[in] samples mono sample count
 * @param[in] volume 0..100
 * @return none
 */
static void __pcm16_to_stereo16(const int16_t *src, int16_t *dst, uint32_t samples, uint8_t volume)
{
    for (uint32_t i = 0; i < samples; i++) {
        int16_t s = __apply_volume(src[i], volume);
        dst[2 * i] = s;
        dst[2 * i + 1] = s;
    }
}

/**
 * @brief Write silence to TX to fill DMA before unmuting amp
 * @param[in] hdl driver handle
 * @param[in] ms silence duration in milliseconds
 * @return OPRT_OK on success
 */
static OPERATE_RET __spk_write_silence(PDM_I2S_SPK_HANDLE_T *hdl, uint32_t ms)
{
    if (NULL == hdl || NULL == hdl->tx_hdl || ms == 0) {
        return OPRT_INVALID_PARM;
    }

    uint32_t samples = (hdl->cfg.spk_sample_rate * ms) / 1000;
    if (samples == 0) {
        samples = 1;
    }

    /* stereo: 2 int16 per sample */
    size_t bytes = samples * 2 * sizeof(int16_t);
    int16_t *zeros = (int16_t *)tal_malloc(bytes);
    if (NULL == zeros) {
        return OPRT_MALLOC_FAILED;
    }
    memset(zeros, 0, bytes);

    size_t bytes_written = 0;
    esp_err_t esp_rt = i2s_channel_write(hdl->tx_hdl, zeros, bytes, &bytes_written, portMAX_DELAY);
    tal_free(zeros);

    if (esp_rt != ESP_OK || bytes_written == 0) {
        PR_ERR("MAX98357 silence prime failed: %d", esp_rt);
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

/**
 * @brief Create I2S STD TX channel for MAX98357A
 * @param[in] hdl driver handle
 * @return OPRT_OK on success
 */
static OPERATE_RET __spk_tx_init(PDM_I2S_SPK_HANDLE_T *hdl)
{
    /*
     * auto_clear_after_cb MUST be true: when SD is tied high the amp is always
     * on, and default ESP-IDF config leaves underrun DMA filled with garbage
     * which MAX98357A amplifies as continuous noise (unlike T5's silence feeder).
     */
    i2s_chan_config_t chan_cfg = {
        .id                 = hdl->cfg.spk_i2s_id,
        .role               = I2S_ROLE_MASTER,
        .dma_desc_num       = SPK_DMA_DESC_NUM,
        .dma_frame_num      = SPK_DMA_FRAME_NUM,
        .auto_clear_after_cb  = true,
        .auto_clear_before_cb = false,
        .intr_priority      = 0,
    };
    if (i2s_new_channel(&chan_cfg, &hdl->tx_hdl, NULL) != ESP_OK) {
        PR_ERR("MAX98357 i2s_new_channel TX failed");
        return OPRT_COM_ERROR;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg =
            {
                .sample_rate_hz = hdl->cfg.spk_sample_rate,
                .clk_src        = I2S_CLK_SRC_DEFAULT,
                .mclk_multiple  = I2S_MCLK_MULTIPLE_256,
#ifdef I2S_HW_VERSION_2
                .ext_clk_freq_hz = 0,
#endif
            },
        .slot_cfg =
            {
                .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
                .slot_mode      = I2S_SLOT_MODE_STEREO,
                .slot_mask      = I2S_STD_SLOT_BOTH,
                .ws_width       = I2S_DATA_BIT_WIDTH_16BIT,
                .ws_pol         = false,
                .bit_shift      = true, /* Philips I2S */
#ifdef I2S_HW_VERSION_2
                .left_align    = false,
                .big_endian    = false,
                .bit_order_lsb = false,
#endif
            },
        .gpio_cfg =
            {
                .mclk         = I2S_GPIO_UNUSED,
                .bclk         = hdl->cfg.spk_bclk_io,
                .ws           = hdl->cfg.spk_ws_io,
                .dout         = hdl->cfg.spk_dout_io,
                .din          = I2S_GPIO_UNUSED,
                .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
            },
    };

    if (i2s_channel_init_std_mode(hdl->tx_hdl, &std_cfg) != ESP_OK) {
        PR_ERR("MAX98357 init_std_mode failed");
        return OPRT_COM_ERROR;
    }
    if (i2s_channel_enable(hdl->tx_hdl) != ESP_OK) {
        PR_ERR("MAX98357 i2s_channel_enable TX failed");
        return OPRT_COM_ERROR;
    }

    /* Fill DMA with zeros before enabling amp (or while SD is already high). */
    OPERATE_RET rt = __spk_write_silence(hdl, SPK_PRIME_MS);
    if (rt != OPRT_OK) {
        return rt;
    }
    __spk_sd_set(hdl, true);

    PR_NOTICE("MAX98357 TX started (bclk=%d ws=%d dout=%d, silence primed)", hdl->cfg.spk_bclk_io,
              hdl->cfg.spk_ws_io, hdl->cfg.spk_dout_io);
    return OPRT_OK;
}

/**
 * @brief Write 16-bit mono PCM to I2S TX as stereo L=R frames
 * @param[in] hdl driver handle
 * @param[in] data PCM samples
 * @param[in] samples sample count
 * @return number of mono samples written, or 0 on failure
 */
static int __spk_write(PDM_I2S_SPK_HANDLE_T *hdl, const int16_t *data, uint32_t samples)
{
    if (NULL == hdl || NULL == hdl->tx_hdl || NULL == data || samples == 0) {
        return 0;
    }

    int16_t *buffer = (int16_t *)tal_malloc(samples * 2 * sizeof(int16_t));
    if (NULL == buffer) {
        PR_ERR("MAX98357 write malloc failed");
        return 0;
    }

    __pcm16_to_stereo16(data, buffer, samples, hdl->play_volume);

    size_t bytes_written = 0;
    esp_err_t esp_rt =
        i2s_channel_write(hdl->tx_hdl, buffer, samples * 2 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    tal_free(buffer);

    if (esp_rt != ESP_OK || bytes_written == 0) {
        PR_ERR("MAX98357 I2S write failed: %d", esp_rt);
        return 0;
    }

    return (int)(bytes_written / (2 * sizeof(int16_t)));
}

static void __pdm_mic_read_task(void *args)
{
    PDM_I2S_SPK_HANDLE_T *hdl = (PDM_I2S_SPK_HANDLE_T *)args;
    if (NULL == hdl) {
        PR_ERR("PDM+I2S spk read task args is NULL");
        return;
    }

    for (;;) {
        size_t bytes_read = 0;
        esp_err_t esp_rt = i2s_channel_read(hdl->rx_hdl, hdl->data_buf, hdl->data_buf_len, &bytes_read,
                                            portMAX_DELAY);
        if (esp_rt != ESP_OK || bytes_read == 0) {
            PR_ERR("PDM mic read failed: %d", esp_rt);
            tal_system_sleep(I2S_READ_TIME_MS);
            continue;
        }

        if (hdl->mic_cb) {
            hdl->mic_cb(TDL_AUDIO_FRAME_FORMAT_PCM, TDL_AUDIO_STATUS_RECEIVING, hdl->data_buf, bytes_read);
        }

#if defined(ENABLE_AUDIO_AFE) && (ENABLE_AUDIO_AFE == 1)
        auio_afe_processor_feed(hdl->data_buf, bytes_read);
#endif
    }
}

static OPERATE_RET __tdd_audio_pdm_i2s_spk_open(TDD_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb)
{
    OPERATE_RET rt = OPRT_OK;
    PDM_I2S_SPK_HANDLE_T *hdl = (PDM_I2S_SPK_HANDLE_T *)handle;

    if (NULL == hdl) {
        return OPRT_COM_ERROR;
    }

    hdl->mic_cb = mic_cb;

    /* Speaker first: MAX98357 with SD tied high amplifies floating DIN until
     * I2S TX is enabled and primed with silence. */
    TUYA_CALL_ERR_RETURN(__spk_sd_init(hdl));
    TUYA_CALL_ERR_RETURN(__spk_tx_init(hdl));

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(hdl->cfg.mic_i2s_id, I2S_ROLE_MASTER);
    if (i2s_new_channel(&chan_cfg, NULL, &hdl->rx_hdl) != ESP_OK) {
        PR_ERR("PDM mic i2s_new_channel failed");
        return OPRT_COM_ERROR;
    }

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg  = I2S_PDM_RX_CLK_DEFAULT_CONFIG(hdl->cfg.mic_sample_rate),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg =
            {
                .clk = hdl->cfg.mic_clk_io,
                .din = hdl->cfg.mic_din_io,
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
    PR_NOTICE("PDM mic started (clk=%d din=%d)", hdl->cfg.mic_clk_io, hdl->cfg.mic_din_io);

    hdl->data_buf_len = (I2S_READ_TIME_MS * hdl->cfg.mic_sample_rate / 1000) * sizeof(int16_t);
    hdl->data_buf = (uint8_t *)tal_malloc(hdl->data_buf_len);
    TUYA_CHECK_NULL_RETURN(hdl->data_buf, OPRT_MALLOC_FAILED);
    memset(hdl->data_buf, 0, hdl->data_buf_len);

    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&hdl->mutex_play));

#if defined(ENABLE_AUDIO_AFE) && (ENABLE_AUDIO_AFE == 1)
    rt = audio_afe_processor_init();
    if (rt != OPRT_OK) {
        PR_ERR("audio_afe_processor_init err:%d", rt);
        rt = OPRT_OK;
    }
#endif

    const THREAD_CFG_T thread_cfg = {
        .thrdname   = "pdm_i2s_spk_rx",
        .stackDepth = 1024,
        .priority   = THREAD_PRIO_1,
    };
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&hdl->thrd_hdl, NULL, NULL, __pdm_mic_read_task, (void *)hdl,
                                                  &thread_cfg));

    return rt;
}

static OPERATE_RET __tdd_audio_pdm_i2s_spk_play(TDD_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len)
{
    PDM_I2S_SPK_HANDLE_T *hdl = (PDM_I2S_SPK_HANDLE_T *)handle;

    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);
    TUYA_CHECK_NULL_RETURN(hdl->mutex_play, OPRT_COM_ERROR);

    if (NULL == data || len == 0) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(hdl->mutex_play);
    uint32_t samples = len / sizeof(int16_t);
    int write_len = __spk_write(hdl, (const int16_t *)data, samples);
    tal_mutex_unlock(hdl->mutex_play);

    return (write_len > 0) ? OPRT_OK : OPRT_COM_ERROR;
}

static OPERATE_RET __tdd_audio_pdm_i2s_spk_set_volume(PDM_I2S_SPK_HANDLE_T *hdl, uint8_t volume)
{
    if (NULL == hdl) {
        return OPRT_INVALID_PARM;
    }
    if (volume > 100) {
        volume = 100;
    }
    hdl->play_volume = volume;
    return OPRT_OK;
}

static OPERATE_RET __tdd_audio_pdm_i2s_spk_config(TDD_AUDIO_HANDLE_T handle, TDD_AUDIO_CMD_E cmd, void *args)
{
    PDM_I2S_SPK_HANDLE_T *hdl = (PDM_I2S_SPK_HANDLE_T *)handle;

    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);

    switch (cmd) {
    case TDD_AUDIO_CMD_SET_VOLUME:
        TUYA_CHECK_NULL_RETURN(args, OPRT_INVALID_PARM);
        return __tdd_audio_pdm_i2s_spk_set_volume(hdl, *(uint8_t *)args);
    default:
        return OPRT_INVALID_PARM;
    }
}

static OPERATE_RET __tdd_audio_pdm_i2s_spk_close(TDD_AUDIO_HANDLE_T handle)
{
    PDM_I2S_SPK_HANDLE_T *hdl = (PDM_I2S_SPK_HANDLE_T *)handle;
    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);

    __spk_sd_set(hdl, false);

    if (hdl->tx_hdl) {
        i2s_channel_disable(hdl->tx_hdl);
        i2s_del_channel(hdl->tx_hdl);
        hdl->tx_hdl = NULL;
    }
    if (hdl->rx_hdl) {
        i2s_channel_disable(hdl->rx_hdl);
        i2s_del_channel(hdl->rx_hdl);
        hdl->rx_hdl = NULL;
    }

    return OPRT_OK;
}

OPERATE_RET tdd_audio_pdm_i2s_spk_register(char *name, TDD_AUDIO_PDM_I2S_SPK_T cfg)
{
    OPERATE_RET rt = OPRT_OK;
    PDM_I2S_SPK_HANDLE_T *_hdl = NULL;
    TDD_AUDIO_INTFS_T intfs = {0};
    TDD_AUDIO_INFO_T  info = {0};

    _hdl = (PDM_I2S_SPK_HANDLE_T *)tal_malloc(sizeof(PDM_I2S_SPK_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(_hdl, OPRT_MALLOC_FAILED);
    memset(_hdl, 0, sizeof(PDM_I2S_SPK_HANDLE_T));

    _hdl->play_volume = 70;
    memcpy(&_hdl->cfg, &cfg, sizeof(TDD_AUDIO_PDM_I2S_SPK_T));

    if (_hdl->cfg.mic_sample_rate == 0) {
        _hdl->cfg.mic_sample_rate = 16000;
    }
    if (_hdl->cfg.spk_sample_rate == 0) {
        _hdl->cfg.spk_sample_rate = _hdl->cfg.mic_sample_rate;
    }

    info.sample_rate   = (uint16_t)_hdl->cfg.mic_sample_rate;
    info.sample_ch_num = 1;
    info.sample_bits   = SAMPLE_DATABITS;
    info.sample_tm_ms  = I2S_READ_TIME_MS;

    intfs.open   = __tdd_audio_pdm_i2s_spk_open;
    intfs.play   = __tdd_audio_pdm_i2s_spk_play;
    intfs.config = __tdd_audio_pdm_i2s_spk_config;
    intfs.close  = __tdd_audio_pdm_i2s_spk_close;

    TUYA_CALL_ERR_GOTO(tdl_audio_driver_register(name, (TDD_AUDIO_HANDLE_T)_hdl, &intfs, &info), __ERR);

    return rt;

__ERR:
    if (_hdl) {
        tal_free(_hdl);
    }
    return rt;
}

#else /* !SOC_I2S_SUPPORTS_PDM_RX */

OPERATE_RET tdd_audio_pdm_i2s_spk_register(char *name, TDD_AUDIO_PDM_I2S_SPK_T cfg)
{
    (void)name;
    (void)cfg;
    PR_ERR("PDM RX not supported on this target");
    return OPRT_NOT_SUPPORTED;
}

#endif /* SOC_I2S_SUPPORTS_PDM_RX */
