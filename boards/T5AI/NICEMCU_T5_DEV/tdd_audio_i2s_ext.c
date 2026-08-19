/**
 * @file tdd_audio_i2s_ext.c
 * @brief T5 external I2S audio: INMP441 + MAX98357 on one I2S1 port (duplex)
 * @version 0.11
 * @date 2026-08-12
 * @copyright Copyright (c) Tuya Inc. All Rights Reserved.
 *
 * Why single-port duplex on I2S1 (P40..P43):
 * - MAX98357 TX on I2S1 is proven working (clean alerts, continuous silence).
 * - Separate I2S0 RX (P6/P7/P8) never produced DMA data (fill always 0).
 * - Beken duplex uses TX+RX on the SAME i2s_id with shared BCLK/WS.
 *
 * Wiring (MUST share clocks):
 *   INMP441  SCK/WS/SD -> P40 / P41 / P42, L/R -> GND
 *   MAX98357 BCLK/LRC/DIN -> P40 / P41 / P43, SD -> 3V3 or cfg.sd_pin
 *
 * Format (v0.11): 16 kHz, 32-bit LRLR stereo slots.
 * - TX: L=R=sample<<16.
 * - RX: right slot + moderate gain + spike filter (DMA glitches pin VAD).
 * - Soft-mute mic while speaker plays (no hardware AEC).
 * - Oneshot/free also need ai_audio_input VAD re-arm on wakeup (see ai_audio_input.c).
 */
#include "tdd_audio_i2s_ext.h"

#include "tdl_audio_driver.h"

#include "tal_memory.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tal_mutex.h"
#include "tal_semaphore.h"

#include "tkl_gpio.h"

#include <driver/i2s.h>
#include <driver/i2s_types.h>
#include <driver/audio_ring_buff.h>

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define I2S_FRAME_MS           20
#define I2S_SAMPLE_RATE_HZ     16000
#define SAMPLE_BITS_PCM        16
#define I2S_FRAME_SAMPLES      (I2S_SAMPLE_RATE_HZ * I2S_FRAME_MS / 1000) /* 320 mono */
#define I2S_SLOTS_PER_SAMPLE   2                                          /* L + R */
#define I2S_WORD_BYTES         4
#define I2S_FRAME_WORDS        (I2S_FRAME_SAMPLES * I2S_SLOTS_PER_SAMPLE) /* 640 */
#define I2S_FRAME_BYTES        (I2S_FRAME_WORDS * I2S_WORD_BYTES)         /* 2560 */
#define I2S_RB_BYTES           (I2S_FRAME_BYTES * 4)
#define I2S_PORT_ID            I2S_GPIO_GROUP_1
#define PLAY_Q_DEPTH           8
/* 24-bit LJ -> int16 normally >>8; >>5 ≈ 8x gain (v0.9 used >>3≈32x and clipped VAD). */
#define MIC_S24_SHIFT_TO_PCM16 5
/* After last audible TX frame, keep muting mic this many 20ms frames (acoustic echo). */
#define MIC_MUTE_AFTER_SPK_FRAMES 15
/* Reject DMA glitches (seen as 0x28xxxxxx / peak=32768) that pin VAD in SPEECH. */
#define MIC_SPIKE_ABS_MAX      20000

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    TDD_AUDIO_I2S_EXT_T cfg;
    TDL_AUDIO_MIC_CB mic_cb;

    THREAD_HANDLE thrd_rx;
    THREAD_HANDLE thrd_tx;
    MUTEX_HANDLE mutex_play;
    SEM_HANDLE sem_tx;
    SEM_HANDLE sem_rx;
    SEM_HANDLE sem_play_space;

    RingBufferContext *tx_rb;
    RingBufferContext *rx_rb;

    uint8_t play_volume;
    uint8_t opened;
    volatile uint8_t tx_running;
    volatile uint8_t rx_running;

    uint8_t *rx_raw_buf;
    uint8_t *rx_pcm_buf;
    uint8_t *tx_pending_buf;
    uint8_t *tx_out_buf;
    uint8_t *tx_silence_buf;
    uint8_t *tx_frame_buf;

    uint8_t *play_q[PLAY_Q_DEPTH];
    uint8_t play_q_r;
    uint8_t play_q_w;
    uint8_t play_q_n;
    uint32_t tx_pending_samples;
    volatile uint16_t mic_mute_frames; /* >0: feed silence to upper layer */
} T5_I2S_EXT_HDL_T;

static T5_I2S_EXT_HDL_T *s_i2s_ext_hdl = NULL;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Absolute value of int16 sample
 * @param[in] s sample
 * @return absolute value
 */
static int32_t __abs16(int16_t s)
{
    return (s >= 0) ? (int32_t)s : -(int32_t)s;
}

/**
 * @brief Enable or disable MAX98357 via SD pin
 * @param[in] hdl driver handle
 * @param[in] enable true to unmute
 * @return none
 */
static void __spk_sd_set(T5_I2S_EXT_HDL_T *hdl, bool enable)
{
    if (hdl->cfg.sd_pin >= TUYA_GPIO_NUM_MAX) {
        return;
    }

    TUYA_GPIO_LEVEL_E level;
    if (enable) {
        level = (hdl->cfg.sd_pin_polarity == TUYA_GPIO_LEVEL_HIGH) ? TUYA_GPIO_LEVEL_HIGH
                                                                   : TUYA_GPIO_LEVEL_LOW;
    } else {
        level = (hdl->cfg.sd_pin_polarity == TUYA_GPIO_LEVEL_HIGH) ? TUYA_GPIO_LEVEL_LOW
                                                                   : TUYA_GPIO_LEVEL_HIGH;
    }
    tkl_gpio_write(hdl->cfg.sd_pin, level);
}

/**
 * @brief Init optional MAX98357 SD GPIO
 * @param[in] hdl driver handle
 * @return OPRT_OK on success
 */
static OPERATE_RET __spk_sd_init(T5_I2S_EXT_HDL_T *hdl)
{
    OPERATE_RET rt = OPRT_OK;

    if (hdl->cfg.sd_pin >= TUYA_GPIO_NUM_MAX) {
        return OPRT_OK;
    }

    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL,
        .direct = TUYA_GPIO_OUTPUT,
        .level = TUYA_GPIO_LEVEL_LOW,
    };
    TUYA_CALL_ERR_RETURN(tkl_gpio_init(hdl->cfg.sd_pin, &gpio_cfg));
    __spk_sd_set(hdl, false);
    return rt;
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
 * @brief Pack mono PCM into stereo 32-bit LRLR words (L=R=sample<<16)
 * @param[in] src mono PCM
 * @param[out] dst stereo words (2 words per sample)
 * @param[in] samples mono sample count
 * @param[in] volume 0..100
 * @return none
 */
static void __pcm16_to_i2s32_stereo(const int16_t *src, uint32_t *dst, uint32_t samples, uint8_t volume)
{
    for (uint32_t i = 0; i < samples; i++) {
        int16_t s = __apply_volume(src[i], volume);
        uint32_t w = ((uint32_t)(uint16_t)s) << 16;
        dst[2 * i] = w;
        dst[2 * i + 1] = w;
    }
}

/**
 * @brief Convert one 32-bit left-justified I2S word to PCM16 with gain
 * @param[in] word raw I2S word
 * @return scaled PCM16 sample
 */
static int16_t __i2s32_word_to_pcm16(uint32_t word)
{
    /* INMP441: 24-bit left-justified in 32-bit slot → arithmetic >> 8 */
    int32_t s24 = ((int32_t)word) >> 8;
    int32_t g = s24 >> MIC_S24_SHIFT_TO_PCM16;
    if (g > INT16_MAX) {
        g = INT16_MAX;
    } else if (g < INT16_MIN) {
        g = INT16_MIN;
    }
    return (int16_t)g;
}

/**
 * @brief Convert stereo 32-bit LRLR words to mono PCM16
 * @param[in] src packed words (2 per sample)
 * @param[out] dst mono PCM
 * @param[in] samples mono sample count
 * @return none
 * @note Right slot only. Holds last good sample when DMA word looks corrupt.
 */
static void __i2s32_stereo_to_pcm16(const uint32_t *src, int16_t *dst, uint32_t samples)
{
    int16_t prev = 0;

    for (uint32_t i = 0; i < samples; i++) {
        int16_t s = __i2s32_word_to_pcm16(src[2 * i + 1]);
        /* Hold last good sample on DMA glitches (peak=32768 pins VAD SPEECH). */
        if (__abs16(s) >= MIC_SPIKE_ABS_MAX) {
            s = prev;
        } else {
            prev = s;
        }
        dst[i] = s;
    }
}

/**
 * @brief TX DMA callback
 * @param[in] size bytes consumed
 * @return size
 */
static int __tx_dma_cb(uint32_t size)
{
    if (s_i2s_ext_hdl && s_i2s_ext_hdl->sem_tx) {
        tal_semaphore_post(s_i2s_ext_hdl->sem_tx);
    }
    return (int)size;
}

/**
 * @brief RX DMA callback
 * @param[in] size bytes produced
 * @return size
 */
static int __rx_dma_cb(uint32_t size)
{
    if (s_i2s_ext_hdl && s_i2s_ext_hdl->sem_rx) {
        tal_semaphore_post(s_i2s_ext_hdl->sem_rx);
    }
    return (int)size;
}

/**
 * @brief Pop one play frame if available
 * @param[in] hdl driver handle
 * @param[out] dst frame buffer
 * @return true if popped
 */
static bool __play_q_pop(T5_I2S_EXT_HDL_T *hdl, uint8_t *dst)
{
    bool ok = false;
    tal_mutex_lock(hdl->mutex_play);
    if (hdl->play_q_n > 0 && hdl->play_q[hdl->play_q_r]) {
        memcpy(dst, hdl->play_q[hdl->play_q_r], I2S_FRAME_BYTES);
        hdl->play_q_r = (uint8_t)((hdl->play_q_r + 1) % PLAY_Q_DEPTH);
        hdl->play_q_n--;
        ok = true;
    }
    tal_mutex_unlock(hdl->mutex_play);
    if (ok && hdl->sem_play_space) {
        tal_semaphore_post(hdl->sem_play_space);
    }
    return ok;
}

/**
 * @brief Push one play frame, blocking until queue has space
 * @param[in] hdl driver handle
 * @param[in] src frame buffer
 * @return OPRT_OK on success
 */
static OPERATE_RET __play_q_push_block(T5_I2S_EXT_HDL_T *hdl, const uint8_t *src)
{
    while (hdl->tx_running) {
        tal_mutex_lock(hdl->mutex_play);
        if (hdl->play_q_n < PLAY_Q_DEPTH && hdl->play_q[hdl->play_q_w]) {
            memcpy(hdl->play_q[hdl->play_q_w], src, I2S_FRAME_BYTES);
            hdl->play_q_w = (uint8_t)((hdl->play_q_w + 1) % PLAY_Q_DEPTH);
            hdl->play_q_n++;
            tal_mutex_unlock(hdl->mutex_play);
            if (hdl->sem_tx) {
                tal_semaphore_post(hdl->sem_tx);
            }
            return OPRT_OK;
        }
        tal_mutex_unlock(hdl->mutex_play);
        if (hdl->sem_tx) {
            tal_semaphore_post(hdl->sem_tx);
        }
        tal_semaphore_wait(hdl->sem_play_space, 20);
    }
    return OPRT_COM_ERROR;
}

/**
 * @brief Write one frame to TX ringbuffer
 * @param[in] hdl driver handle
 * @param[in] frame frame bytes
 * @return none
 */
static void __tx_write_frame(T5_I2S_EXT_HDL_T *hdl, const uint8_t *frame)
{
    while (hdl->tx_running) {
        if (hdl->tx_rb && ring_buffer_get_free_size(hdl->tx_rb) >= I2S_FRAME_BYTES) {
            ring_buffer_write(hdl->tx_rb, (uint8_t *)frame, I2S_FRAME_BYTES);
            return;
        }
        tal_semaphore_wait(hdl->sem_tx, 20);
    }
}

/**
 * @brief TX feeder task
 * @param[in] args driver handle
 * @return none
 */
static void __i2s_tx_task(void *args)
{
    T5_I2S_EXT_HDL_T *hdl = (T5_I2S_EXT_HDL_T *)args;
    if (NULL == hdl) {
        return;
    }

    PR_NOTICE("I2S spk task start (continuous silence, stereo32)");

    while (hdl->tx_running) {
        const uint8_t *frame = hdl->tx_silence_buf;
        if (__play_q_pop(hdl, hdl->tx_out_buf)) {
            frame = hdl->tx_out_buf;
            /* Playing alert/TTS: mute mic uplink (no hardware AEC on this board). */
            hdl->mic_mute_frames = MIC_MUTE_AFTER_SPK_FRAMES;
        } else if (hdl->mic_mute_frames > 0) {
            hdl->mic_mute_frames--;
        }
        __tx_write_frame(hdl, frame);
    }
}

/**
 * @brief Mic capture task
 * @param[in] args driver handle
 * @return none
 */
static void __i2s_rx_task(void *args)
{
    T5_I2S_EXT_HDL_T *hdl = (T5_I2S_EXT_HDL_T *)args;
    uint32_t log_div = 0;
    uint32_t wait_div = 0;

    if (NULL == hdl) {
        return;
    }

    PR_NOTICE("I2S mic task start, frame=%u words=%u", (unsigned)I2S_FRAME_BYTES, (unsigned)I2S_FRAME_WORDS);

    while (hdl->rx_running) {
        uint32_t fill = hdl->rx_rb ? ring_buffer_get_fill_size(hdl->rx_rb) : 0;
        if (fill < I2S_FRAME_BYTES) {
            if ((++wait_div % 40) == 0) {
                PR_NOTICE("I2S mic waiting fill=%u (need %u)", (unsigned)fill, (unsigned)I2S_FRAME_BYTES);
            }
            tal_semaphore_wait(hdl->sem_rx, 50);
            continue;
        }

        /* Drop overflow to avoid DMA wrap corruption / reset */
        while (hdl->rx_rb && ring_buffer_get_fill_size(hdl->rx_rb) >= (I2S_FRAME_BYTES * 3)) {
            ring_buffer_read(hdl->rx_rb, hdl->rx_raw_buf, I2S_FRAME_BYTES);
        }

        uint32_t got = ring_buffer_read(hdl->rx_rb, hdl->rx_raw_buf, I2S_FRAME_BYTES);
        if (got < I2S_FRAME_BYTES) {
            continue;
        }

        const uint32_t *raw = (const uint32_t *)hdl->rx_raw_buf;
        __i2s32_stereo_to_pcm16(raw, (int16_t *)hdl->rx_pcm_buf, I2S_FRAME_SAMPLES);

        /* Soft mute while / just after speaker output — prevents VAD stuck on echo */
        if (hdl->mic_mute_frames > 0) {
            memset(hdl->rx_pcm_buf, 0, I2S_FRAME_SAMPLES * sizeof(int16_t));
        }

        if ((++log_div % 25) == 0) {
            const int16_t *pcm = (const int16_t *)hdl->rx_pcm_buf;
            int32_t peak = 0;
            for (uint32_t i = 0; i < I2S_FRAME_SAMPLES; i++) {
                int32_t a = __abs16(pcm[i]);
                if (a > peak) {
                    peak = a;
                }
            }
            PR_NOTICE("I2S mic peak=%d mute=%u raw1=0x%08x", (int)peak, (unsigned)hdl->mic_mute_frames,
                      (unsigned)raw[1]);
        }

        if (hdl->mic_cb) {
            hdl->mic_cb(TDL_AUDIO_FRAME_FORMAT_PCM, TDL_AUDIO_STATUS_RECEIVING, hdl->rx_pcm_buf,
                        I2S_FRAME_SAMPLES * sizeof(int16_t));
        }
    }
}

/**
 * @brief Fill 16 kHz / 32-bit LRLR master config
 * @param[out] cfg i2s config
 * @return none
 */
static void __fill_i2s_cfg(i2s_config_t *cfg)
{
    *cfg = (i2s_config_t)DEFAULT_I2S_CONFIG();
    cfg->role = I2S_ROLE_MASTER;
    cfg->work_mode = I2S_WORK_MODE_I2S;
    cfg->store_mode = I2S_LRCOM_STORE_LRLR;
    cfg->samp_rate = I2S_SAMP_RATE_16000;
    cfg->data_length = 32;
    cfg->pcm_chl_num = 1;
}

/**
 * @brief Allocate play queue frame buffers
 * @param[in] hdl driver handle
 * @return OPRT_OK on success
 */
static OPERATE_RET __alloc_play_q(T5_I2S_EXT_HDL_T *hdl)
{
    for (uint32_t i = 0; i < PLAY_Q_DEPTH; i++) {
        hdl->play_q[i] = (uint8_t *)tal_malloc(I2S_FRAME_BYTES);
        if (!hdl->play_q[i]) {
            return OPRT_MALLOC_FAILED;
        }
        memset(hdl->play_q[i], 0, I2S_FRAME_BYTES);
    }
    return OPRT_OK;
}

/**
 * @brief Open duplex I2S1 and start feeder / mic tasks
 * @param[in] handle driver handle
 * @param[in] mic_cb microphone PCM callback
 * @return OPRT_OK on success
 */
static OPERATE_RET __tdd_audio_i2s_ext_open(TDD_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb)
{
    OPERATE_RET rt = OPRT_OK;
    T5_I2S_EXT_HDL_T *hdl = (T5_I2S_EXT_HDL_T *)handle;
    bk_err_t bk_rt;

    if (NULL == hdl) {
        return OPRT_COM_ERROR;
    }
    if (hdl->opened) {
        return OPRT_OK;
    }

    hdl->mic_cb = mic_cb;
    s_i2s_ext_hdl = hdl;

    TUYA_CALL_ERR_RETURN(__spk_sd_init(hdl));
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&hdl->mutex_play));
    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&hdl->sem_tx, 0, 8));
    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&hdl->sem_rx, 0, 8));
    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&hdl->sem_play_space, PLAY_Q_DEPTH, PLAY_Q_DEPTH));
    TUYA_CALL_ERR_RETURN(__alloc_play_q(hdl));

    hdl->rx_raw_buf = (uint8_t *)tal_malloc(I2S_FRAME_BYTES);
    hdl->rx_pcm_buf = (uint8_t *)tal_malloc(I2S_FRAME_SAMPLES * sizeof(int16_t));
    hdl->tx_pending_buf = (uint8_t *)tal_malloc(I2S_FRAME_BYTES);
    hdl->tx_out_buf = (uint8_t *)tal_malloc(I2S_FRAME_BYTES);
    hdl->tx_silence_buf = (uint8_t *)tal_malloc(I2S_FRAME_BYTES);
    hdl->tx_frame_buf = (uint8_t *)tal_malloc(I2S_FRAME_BYTES);
    if (!hdl->rx_raw_buf || !hdl->rx_pcm_buf || !hdl->tx_pending_buf || !hdl->tx_out_buf || !hdl->tx_silence_buf ||
        !hdl->tx_frame_buf) {
        return OPRT_MALLOC_FAILED;
    }
    memset(hdl->rx_raw_buf, 0, I2S_FRAME_BYTES);
    memset(hdl->rx_pcm_buf, 0, I2S_FRAME_SAMPLES * sizeof(int16_t));
    memset(hdl->tx_pending_buf, 0, I2S_FRAME_BYTES);
    memset(hdl->tx_out_buf, 0, I2S_FRAME_BYTES);
    memset(hdl->tx_silence_buf, 0, I2S_FRAME_BYTES);
    memset(hdl->tx_frame_buf, 0, I2S_FRAME_BYTES);

    bk_rt = bk_i2s_multi_driver_init();
    if (bk_rt != BK_OK) {
        PR_ERR("bk_i2s_multi_driver_init failed: %d", (int)bk_rt);
        return OPRT_COM_ERROR;
    }

    i2s_config_t i2s_cfg;
    __fill_i2s_cfg(&i2s_cfg);
    bk_rt = bk_i2s_init_by_id(I2S_PORT_ID, &i2s_cfg);
    if (bk_rt != BK_OK) {
        PR_ERR("bk_i2s_init failed: %d", (int)bk_rt);
        return OPRT_COM_ERROR;
    }

    bk_rt = bk_i2s_chl_init_by_id(I2S_PORT_ID, I2S_CHANNEL_1, I2S_TXRX_TYPE_TX, I2S_RB_BYTES, __tx_dma_cb,
                                  &hdl->tx_rb);
    if (bk_rt != BK_OK || hdl->tx_rb == NULL) {
        PR_ERR("bk_i2s_chl_init TX failed: %d", (int)bk_rt);
        return OPRT_COM_ERROR;
    }

    bk_rt = bk_i2s_chl_init_by_id(I2S_PORT_ID, I2S_CHANNEL_1, I2S_TXRX_TYPE_RX, I2S_RB_BYTES, __rx_dma_cb,
                                  &hdl->rx_rb);
    if (bk_rt != BK_OK || hdl->rx_rb == NULL) {
        PR_ERR("bk_i2s_chl_init RX failed: %d", (int)bk_rt);
        return OPRT_COM_ERROR;
    }

    while (ring_buffer_get_free_size(hdl->tx_rb) >= I2S_FRAME_BYTES) {
        ring_buffer_write(hdl->tx_rb, hdl->tx_silence_buf, I2S_FRAME_BYTES);
    }
    __spk_sd_set(hdl, true);
    bk_i2s_start_by_id(I2S_PORT_ID);

    hdl->tx_running = 1;
    hdl->rx_running = 1;
    hdl->play_q_r = 0;
    hdl->play_q_w = 0;
    hdl->play_q_n = 0;
    hdl->tx_pending_samples = 0;

    THREAD_CFG_T tx_cfg_th = {
        .thrdname = "t5_i2s_tx",
        .stackDepth = 1024 * 4,
        .priority = THREAD_PRIO_1,
    };
    THREAD_CFG_T rx_cfg_th = {
        .thrdname = "t5_i2s_rx",
        .stackDepth = 1024 * 4,
        .priority = THREAD_PRIO_1,
    };
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&hdl->thrd_tx, NULL, NULL, __i2s_tx_task, hdl, &tx_cfg_th));
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&hdl->thrd_rx, NULL, NULL, __i2s_rx_task, hdl, &rx_cfg_th));

    hdl->opened = 1;
    PR_NOTICE("I2S audio bk: duplex 32bit LRLR @I2S1 (no AEC; soft-mute while spk)");
    return rt;
}

/**
 * @brief Play PCM (blocking queue into TX feeder)
 * @param[in] handle driver handle
 * @param[in] data 16-bit mono PCM
 * @param[in] len byte length
 * @return OPRT_OK on success
 */
static OPERATE_RET __tdd_audio_i2s_ext_play(TDD_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    T5_I2S_EXT_HDL_T *hdl = (T5_I2S_EXT_HDL_T *)handle;

    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);
    TUYA_CHECK_NULL_RETURN(hdl->mutex_play, OPRT_COM_ERROR);
    TUYA_CHECK_NULL_RETURN(hdl->tx_pending_buf, OPRT_COM_ERROR);
    TUYA_CHECK_NULL_RETURN(hdl->tx_frame_buf, OPRT_COM_ERROR);

    if (NULL == data || len == 0) {
        return OPRT_INVALID_PARM;
    }

    const int16_t *pcm = (const int16_t *)data;
    uint32_t samples = len / sizeof(int16_t);
    uint32_t idx = 0;
    uint8_t *out_frame = hdl->tx_frame_buf;
    uint32_t *out_words = (uint32_t *)out_frame;

    tal_mutex_lock(hdl->mutex_play);
    uint32_t pending = hdl->tx_pending_samples;
    if (pending > 0 && pending < I2S_FRAME_SAMPLES) {
        memcpy(out_frame, hdl->tx_pending_buf, pending * I2S_SLOTS_PER_SAMPLE * I2S_WORD_BYTES);
    } else {
        pending = 0;
        memset(out_frame, 0, I2S_FRAME_BYTES);
    }
    tal_mutex_unlock(hdl->mutex_play);

    while (idx < samples) {
        uint32_t space = I2S_FRAME_SAMPLES - pending;
        uint32_t take = samples - idx;
        if (take > space) {
            take = space;
        }

        __pcm16_to_i2s32_stereo(pcm + idx, out_words + pending * I2S_SLOTS_PER_SAMPLE, take, hdl->play_volume);
        pending += take;
        idx += take;

        if (pending >= I2S_FRAME_SAMPLES) {
            rt = __play_q_push_block(hdl, out_frame);
            if (rt != OPRT_OK) {
                break;
            }
            pending = 0;
            memset(out_frame, 0, I2S_FRAME_BYTES);
        }
    }

    tal_mutex_lock(hdl->mutex_play);
    if (pending > 0) {
        memcpy(hdl->tx_pending_buf, out_frame, pending * I2S_SLOTS_PER_SAMPLE * I2S_WORD_BYTES);
    } else {
        memset(hdl->tx_pending_buf, 0, I2S_FRAME_BYTES);
    }
    hdl->tx_pending_samples = pending;
    tal_mutex_unlock(hdl->mutex_play);

    return rt;
}

/**
 * @brief Set software playback volume
 * @param[in] handle driver handle
 * @param[in] volume 0..100
 * @return OPRT_OK on success
 */
static OPERATE_RET __tdd_audio_i2s_ext_set_volume(TDD_AUDIO_HANDLE_T handle, uint8_t volume)
{
    T5_I2S_EXT_HDL_T *hdl = (T5_I2S_EXT_HDL_T *)handle;
    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);

    if (volume > 100) {
        volume = 100;
    }
    hdl->play_volume = volume;
    return OPRT_OK;
}

/**
 * @brief Audio config command handler
 * @param[in] handle driver handle
 * @param[in] cmd command
 * @param[in] args command args
 * @return OPRT_OK on success
 */
static OPERATE_RET __tdd_audio_i2s_ext_config(TDD_AUDIO_HANDLE_T handle, TDD_AUDIO_CMD_E cmd, void *args)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_CHECK_NULL_RETURN(handle, OPRT_COM_ERROR);

    switch (cmd) {
    case TDD_AUDIO_CMD_SET_VOLUME:
        TUYA_CHECK_NULL_RETURN(args, OPRT_INVALID_PARM);
        rt = __tdd_audio_i2s_ext_set_volume(handle, *(uint8_t *)args);
        break;
    case TDD_AUDIO_CMD_PLAY_STOP: {
        T5_I2S_EXT_HDL_T *hdl = (T5_I2S_EXT_HDL_T *)handle;
        tal_mutex_lock(hdl->mutex_play);
        hdl->play_q_r = 0;
        hdl->play_q_w = 0;
        hdl->play_q_n = 0;
        hdl->tx_pending_samples = 0;
        memset(hdl->tx_pending_buf, 0, I2S_FRAME_BYTES);
        tal_mutex_unlock(hdl->mutex_play);
        if (hdl->sem_play_space) {
            for (uint32_t i = 0; i < PLAY_Q_DEPTH; i++) {
                tal_semaphore_post(hdl->sem_play_space);
            }
        }
    } break;
    default:
        rt = OPRT_INVALID_PARM;
        break;
    }

    return rt;
}

/**
 * @brief Close I2S audio
 * @param[in] handle driver handle
 * @return OPRT_OK
 */
static OPERATE_RET __tdd_audio_i2s_ext_close(TDD_AUDIO_HANDLE_T handle)
{
    T5_I2S_EXT_HDL_T *hdl = (T5_I2S_EXT_HDL_T *)handle;
    if (NULL == hdl) {
        return OPRT_OK;
    }

    hdl->tx_running = 0;
    hdl->rx_running = 0;
    if (hdl->sem_tx) {
        tal_semaphore_post(hdl->sem_tx);
    }
    if (hdl->sem_rx) {
        tal_semaphore_post(hdl->sem_rx);
    }
    if (hdl->sem_play_space) {
        tal_semaphore_post(hdl->sem_play_space);
    }

    __spk_sd_set(hdl, false);
    bk_i2s_stop_by_id(I2S_PORT_ID);
    hdl->opened = 0;
    return OPRT_OK;
}

/**
 * @brief Register INMP441 + MAX98357 I2S audio driver
 * @param[in] name codec name
 * @param[in] cfg board config
 * @return OPRT_OK on success
 */
OPERATE_RET tdd_audio_i2s_ext_register(char *name, TDD_AUDIO_I2S_EXT_T cfg)
{
    OPERATE_RET rt = OPRT_OK;
    T5_I2S_EXT_HDL_T *_hdl = NULL;
    TDD_AUDIO_INTFS_T intfs = {0};
    TDD_AUDIO_INFO_T info = {0};

    TUYA_CHECK_NULL_RETURN(name, OPRT_INVALID_PARM);

    _hdl = (T5_I2S_EXT_HDL_T *)tal_malloc(sizeof(T5_I2S_EXT_HDL_T));
    TUYA_CHECK_NULL_RETURN(_hdl, OPRT_MALLOC_FAILED);
    memset(_hdl, 0, sizeof(T5_I2S_EXT_HDL_T));

    _hdl->play_volume = 70;
    memcpy(&_hdl->cfg, &cfg, sizeof(TDD_AUDIO_I2S_EXT_T));
    if (_hdl->cfg.mic_sample_rate == 0) {
        _hdl->cfg.mic_sample_rate = I2S_SAMPLE_RATE_HZ;
    }
    if (_hdl->cfg.spk_sample_rate == 0) {
        _hdl->cfg.spk_sample_rate = I2S_SAMPLE_RATE_HZ;
    }

    info.sample_rate = (uint16_t)_hdl->cfg.mic_sample_rate;
    info.sample_ch_num = 1;
    info.sample_bits = SAMPLE_BITS_PCM;
    info.sample_tm_ms = I2S_FRAME_MS;

    intfs.open = __tdd_audio_i2s_ext_open;
    intfs.play = __tdd_audio_i2s_ext_play;
    intfs.config = __tdd_audio_i2s_ext_config;
    intfs.close = __tdd_audio_i2s_ext_close;

    TUYA_CALL_ERR_GOTO(tdl_audio_driver_register(name, (TDD_AUDIO_HANDLE_T)_hdl, &intfs, &info), __ERR);
    return rt;

__ERR:
    if (_hdl) {
        tal_free(_hdl);
    }
    return rt;
}
