#include "tdl_audio_driver.h"
#include "tdd_audio_no_codec.h"

#include <stdint.h>

#include "tal_memory.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tal_mutex.h"
#include "tal_api.h"

#include "tkl_i2s.h"

/***********************************************************
************************macro define************************
***********************************************************/

#define I2S_READ_INTERVAL_MS   10
#define I2S_RX_BUFF_FRAME      320000 /* 20s buffer data sample rate 16Khz */
#define I2S_TX_BUFF_FRAME      1059840 /* 128s buffer data sample rate 8Khz */
#define I2S_RX_SAMPLE_RATE     16000
#define I2S_TX_SAMPLE_RATE     8000
#define I2S_PLAYER_WAIT        300
#define I2S_TX_POLL_MS         1    /* poll TX complete; do NOT post sem from ISR */
#define I2S_TX_IDLE_WAIT_MS    20   /* wait new pcm while TX underrun */
#define I2S_TX_IDLE_TIMEOUT_MS 2000 /* no pcm -> switch back to RX */
#define I2S_MODE_RX            0
#define I2S_MODE_TX            1

#define VOLUME_Q16_SHIFT       16
#define VOLUME_Q16_SCALE       (1u << VOLUME_Q16_SHIFT)
#define VOLUME_MAX_PERCENT     100

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    int16_t buf[I2S_RX_BUFF_FRAME + 1] __attribute__((aligned(4)));
    volatile uint32_t read_pos;
    volatile uint32_t write_pos;
} rx_buffer_t;

typedef struct {
    uint8_t buf[I2S_TX_BUFF_FRAME + 1] __attribute__((aligned(4)));
    volatile uint32_t read_pos;
    volatile uint32_t write_pos;
} tx_buffer_t;

typedef struct {
    TDD_AUDIO_NO_CODEC_T cfg;
    TDL_AUDIO_MIC_CB mic_cb;
    TUYA_I2S_NUM_E i2s_id;
    THREAD_HANDLE thrd_hdl;
    MUTEX_HANDLE mutex_play;
    SEM_HANDLE tx_sem;
    uint8_t play_volume;
    volatile uint8_t i2s_mode;

    rx_buffer_t rx_buff;
    tx_buffer_t tx_buff;

    TIMER_ID player_timer_id;
} SI91X_I2S_HANDLE_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Wake TX task from thread/timer context
 * @param[in] hdl audio handle
 * @return none
 * @note Must NOT be called from I2S ISR (osSemaphoreRelease is not ISR-safe here)
 */
static void si91x_i2s_tx_wake(SI91X_I2S_HANDLE_T *hdl)
{
    if ((hdl != NULL) && (hdl->tx_sem != NULL)) {
        (void)tal_semaphore_post(hdl->tx_sem);
    }
}

/**
 * @brief Start TX mode after prebuffer wait
 * @param[in] timer_id software timer id
 * @param[in] args audio handle
 * @return none
 */
static void si91x_i2s_audio_player_start(TIMER_ID timer_id, void *args)
{
    SI91X_I2S_HANDLE_T *hdl = (SI91X_I2S_HANDLE_T *)args;

    if (NULL == hdl) {
        return;
    }

    hdl->i2s_mode = I2S_MODE_TX;
    PR_NOTICE("I2S mode tx");
    si91x_i2s_tx_wake(hdl);
}

/**
 * @brief RX streaming buffer ready callback
 * @param[in] args audio handle
 * @param[in] buffer unused
 * @param[in] n_frames frames written
 * @return none
 */
static void si91x_i2s_buffer_ready_callback(void *args, void *buffer, uint32_t n_frames)
{
    SI91X_I2S_HANDLE_T *hdl = (SI91X_I2S_HANDLE_T *)args;

    hdl->rx_buff.write_pos = (hdl->rx_buff.write_pos + n_frames) % I2S_RX_BUFF_FRAME;
}

/**
 * @brief Get available TX bytes
 * @param[in] hdl audio handle
 * @return available bytes in TX ring
 */
static uint32_t si91x_i2s_tx_available(SI91X_I2S_HANDLE_T *hdl)
{
    if (hdl->tx_buff.write_pos >= hdl->tx_buff.read_pos) {
        return hdl->tx_buff.write_pos - hdl->tx_buff.read_pos;
    }
    return (I2S_TX_BUFF_FRAME - hdl->tx_buff.read_pos) + hdl->tx_buff.write_pos;
}

/**
 * @brief Switch back to RX streaming
 * @param[in] hdl audio handle
 * @return none
 */
static void si91x_i2s_switch_to_rx(SI91X_I2S_HANDLE_T *hdl)
{
    tal_mutex_lock(hdl->mutex_play);
    hdl->i2s_mode = I2S_MODE_RX;
    hdl->tx_buff.read_pos = hdl->tx_buff.write_pos = 0;
    tal_mutex_unlock(hdl->mutex_play);

    tkl_i2s_send_stop(hdl->i2s_id);
    tkl_i2s_recv_stop(hdl->i2s_id);
    tkl_i2s_recv_streaming(hdl->i2s_id, si91x_i2s_buffer_ready_callback, hdl);
    PR_NOTICE("I2S mode rx");
}

/**
 * @brief I2S RX/TX service task
 * @param[in] args audio handle
 * @return none
 */
static void si91x_i2s_read_task(void *args)
{
    SI91X_I2S_HANDLE_T *hdl = (SI91X_I2S_HANDLE_T *)args;
    int new_data_length;
    int length_to_end;
    int chunk_length;
    uint32_t idle_ms = 0;
    uint8_t *buff;
    uint32_t length;
    uint32_t available;

    if (NULL == hdl) {
        PR_ERR("I2S read task args is NULL");
        return;
    }

    while (1) {
        if (hdl->i2s_mode == I2S_MODE_TX) {
            if (tkl_i2s_send_inprogress(hdl->i2s_id)) {
                /* Short poll only: SEND_COMPLETE runs in ISR, cannot post semaphore */
                tal_system_sleep(I2S_TX_POLL_MS);
                continue;
            }

            tal_mutex_lock(hdl->mutex_play);
            available = si91x_i2s_tx_available(hdl);

            if (available == 0) {
                tal_mutex_unlock(hdl->mutex_play);
                if (tal_semaphore_wait(hdl->tx_sem, I2S_TX_IDLE_WAIT_MS) != OPRT_OK) {
                    idle_ms += I2S_TX_IDLE_WAIT_MS;
                } else {
                    idle_ms = 0;
                }

                if (idle_ms >= I2S_TX_IDLE_TIMEOUT_MS) {
                    si91x_i2s_switch_to_rx(hdl);
                    idle_ms = 0;
                }
                continue;
            }

            length_to_end = I2S_TX_BUFF_FRAME - hdl->tx_buff.read_pos;
            if (available <= (uint32_t)length_to_end) {
                length = available;
                buff = &hdl->tx_buff.buf[hdl->tx_buff.read_pos];
                hdl->tx_buff.read_pos = (hdl->tx_buff.read_pos + length) % I2S_TX_BUFF_FRAME;
            } else {
                length = (uint32_t)length_to_end;
                buff = &hdl->tx_buff.buf[hdl->tx_buff.read_pos];
                hdl->tx_buff.read_pos = 0;
            }
            tal_mutex_unlock(hdl->mutex_play);

            if (length > 0) {
                idle_ms = 0;
                if (tkl_i2s_send(hdl->i2s_id, buff, length) != OPRT_OK) {
                    PR_ERR("tkl_i2s_send failed len:%u", length);
                    tal_system_sleep(I2S_TX_POLL_MS);
                }
            }
        } else {
            if (hdl->rx_buff.read_pos == hdl->rx_buff.write_pos) {
                tal_system_sleep(I2S_READ_INTERVAL_MS);
                continue;
            }

            new_data_length = (hdl->rx_buff.read_pos < hdl->rx_buff.write_pos)
                                  ? (hdl->rx_buff.write_pos - hdl->rx_buff.read_pos)
                                  : ((I2S_RX_BUFF_FRAME - hdl->rx_buff.read_pos) + hdl->rx_buff.write_pos);

            if (new_data_length > 0) {
                length_to_end = I2S_RX_BUFF_FRAME - hdl->rx_buff.read_pos;
                chunk_length = (new_data_length < length_to_end) ? new_data_length : length_to_end;

                if (hdl->mic_cb && chunk_length > 0) {
                    hdl->mic_cb(TDL_AUDIO_FRAME_FORMAT_PCM, TDL_AUDIO_STATUS_RECEIVING,
                                (uint8_t *)&hdl->rx_buff.buf[hdl->rx_buff.read_pos], chunk_length * sizeof(int16_t));
                }
                hdl->rx_buff.read_pos = (hdl->rx_buff.read_pos + chunk_length) % (I2S_RX_BUFF_FRAME);
                new_data_length -= chunk_length;

                if (new_data_length > 0) {
                    if (hdl->mic_cb) {
                        hdl->mic_cb(TDL_AUDIO_FRAME_FORMAT_PCM, TDL_AUDIO_STATUS_RECEIVING,
                                    (uint8_t *)&hdl->rx_buff.buf[0], new_data_length * sizeof(int16_t));
                    }
                    hdl->rx_buff.read_pos = new_data_length;
                }
            }
            tal_system_sleep(I2S_READ_INTERVAL_MS);
        }
    }
}

/**
 * @brief Open no-codec audio device
 * @param[in] handle audio handle
 * @param[in] mic_cb mic frame callback
 * @return OPRT_OK on success
 */
static OPERATE_RET __tdd_audio_no_codec_open(TDD_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb)
{
    OPERATE_RET rt = OPRT_OK;
    SI91X_I2S_HANDLE_T *hdl = (SI91X_I2S_HANDLE_T *)handle;

    if (NULL == hdl) {
        return OPRT_COM_ERROR;
    }

    hdl->mic_cb = mic_cb;
    hdl->i2s_id = TUYA_I2S_NUM_0;

    TUYA_I2S_BASE_CFG_T i2s_cfg = {0};
    i2s_cfg.mode = TUYA_I2S_MODE_MASTER | TUYA_I2S_MODE_RX | TUYA_I2S_MODE_TX;
    i2s_cfg.sample_rate = I2S_RX_SAMPLE_RATE;
    i2s_cfg.channel_format = TUYA_I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_cfg.bits_per_sample = TUYA_I2S_BITS_PER_SAMPLE_16BIT;
    tkl_i2s_init(hdl->i2s_id, &i2s_cfg);
    PR_NOTICE("I2S channels created");

    tal_mutex_create_init(&hdl->mutex_play);
    if (NULL == hdl->mutex_play) {
        PR_ERR("I2S mutex create failed");
        return OPRT_COM_ERROR;
    }

    /* Binary semaphore: wake TX task when new pcm arrives (thread context only) */
    if (tal_semaphore_create_init(&hdl->tx_sem, 0, 1) != OPRT_OK) {
        PR_ERR("I2S tx sem create failed");
        return OPRT_COM_ERROR;
    }

    memset(hdl->rx_buff.buf, 0, sizeof(hdl->rx_buff.buf));
    tkl_i2s_set_streaming_config(hdl->i2s_id, hdl->rx_buff.buf, I2S_RX_BUFF_FRAME / 2);
    tkl_i2s_recv_streaming(hdl->i2s_id, si91x_i2s_buffer_ready_callback, hdl);

    TUYA_CALL_ERR_LOG(tal_sw_timer_create(si91x_i2s_audio_player_start, (void *)hdl, &hdl->player_timer_id));

    const THREAD_CFG_T thread_cfg = {
        .thrdname = "i2s_read",
        .stackDepth = 3 * 1024,
        .priority = THREAD_PRIO_0,
    };

    PR_DEBUG("I2S read task args: %p", hdl);
    TUYA_CALL_ERR_LOG(
        tal_thread_create_and_start(&hdl->thrd_hdl, NULL, NULL, si91x_i2s_read_task, (void *)hdl, &thread_cfg));

    return rt;
}

/**
 * @brief Apply square-law digital gain to PCM16 samples (in-place)
 * @param[in,out] pcm sample buffer
 * @param[in] samples number of int16 samples
 * @param[in] volume 0-100
 * @return none
 * @note Same curve as ESP32 tdd_audio_no_codec: (vol/100)^2 in Q16.
 */
static void __tdd_audio_no_codec_apply_volume(int16_t *pcm, uint32_t samples, uint8_t volume)
{
    uint32_t i;
    uint32_t vol_q16;
    int32_t tmp;

    if (pcm == NULL || samples == 0 || volume >= VOLUME_MAX_PERCENT) {
        return;
    }
    if (volume == 0) {
        memset(pcm, 0, samples * sizeof(int16_t));
        return;
    }

    vol_q16 = (uint32_t)volume * (uint32_t)volume * VOLUME_Q16_SCALE /
              ((uint32_t)VOLUME_MAX_PERCENT * (uint32_t)VOLUME_MAX_PERCENT);
    for (i = 0; i < samples; i++) {
        tmp = ((int32_t)pcm[i] * (int32_t)vol_q16) >> VOLUME_Q16_SHIFT;
        if (tmp > INT16_MAX) {
            tmp = INT16_MAX;
        } else if (tmp < INT16_MIN) {
            tmp = INT16_MIN;
        }
        pcm[i] = (int16_t)tmp;
    }
}

/**
 * @brief Write PCM data into TX ring buffer
 * @param[in] handle audio handle
 * @param[in] data pcm bytes
 * @param[in] len byte length
 * @return OPRT_OK on success
 */
static OPERATE_RET __tdd_audio_no_codec_play(TDD_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;
    SI91X_I2S_HANDLE_T *hdl = (SI91X_I2S_HANDLE_T *)handle;
    uint8_t *play_data = data;
    uint8_t *gain_buf = NULL;

    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);

    if (NULL == data || len == 0) {
        PR_ERR("I2S play data is NULL");
        return OPRT_COM_ERROR;
    }

    /* Apply digital volume before enqueue when not at full scale. */
    if (hdl->play_volume < VOLUME_MAX_PERCENT && (len >= sizeof(int16_t)) && ((len & 1u) == 0u)) {
        gain_buf = (uint8_t *)tal_malloc(len);
        if (gain_buf != NULL) {
            memcpy(gain_buf, data, len);
            __tdd_audio_no_codec_apply_volume((int16_t *)gain_buf, len / sizeof(int16_t), hdl->play_volume);
            play_data = gain_buf;
        }
    }

    tal_mutex_lock(hdl->mutex_play);
    int length_to_end = I2S_TX_BUFF_FRAME - hdl->tx_buff.write_pos;
    int chunk_size = MIN((int)len, length_to_end);
    int remaining_size = (int)len - chunk_size;
    memcpy(&hdl->tx_buff.buf[hdl->tx_buff.write_pos], play_data, chunk_size);
    hdl->tx_buff.write_pos += chunk_size;
    if (hdl->tx_buff.write_pos >= I2S_TX_BUFF_FRAME) {
        hdl->tx_buff.write_pos = 0;
    }
    if (remaining_size > 0) {
        memcpy(&hdl->tx_buff.buf[0], play_data + chunk_size, remaining_size);
        hdl->tx_buff.write_pos = remaining_size;
    }

    tal_mutex_unlock(hdl->mutex_play);
    si91x_i2s_tx_wake(hdl);

    if (hdl->i2s_mode != I2S_MODE_TX && !tal_sw_timer_is_running(hdl->player_timer_id)) {
        tal_sw_timer_start(hdl->player_timer_id, I2S_PLAYER_WAIT, TAL_TIMER_ONCE);
    }

    if (gain_buf != NULL) {
        tal_free(gain_buf);
        gain_buf = NULL;
    }

    return rt;
}

/**
 * @brief Set play volume
 * @param[in] handle audio handle
 * @param[in] volume 0-100
 * @return OPRT_OK on success
 */
static OPERATE_RET __tdd_audio_no_codec_set_volume(TDD_AUDIO_HANDLE_T handle, uint8_t volume)
{
    OPERATE_RET rt = OPRT_OK;

    SI91X_I2S_HANDLE_T *hdl = (SI91X_I2S_HANDLE_T *)handle;

    TUYA_CHECK_NULL_RETURN(hdl, OPRT_COM_ERROR);

    if (volume > VOLUME_MAX_PERCENT) {
        volume = VOLUME_MAX_PERCENT;
    }

    hdl->play_volume = volume;

    return rt;
}

/**
 * @brief Config no-codec audio device
 * @param[in] handle audio handle
 * @param[in] cmd config command
 * @param[in] args command args
 * @return OPRT_OK on success
 */
static OPERATE_RET __tdd_audio_no_codec_config(TDD_AUDIO_HANDLE_T handle, TDD_AUDIO_CMD_E cmd, void *args)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_COM_ERROR);

    switch (cmd) {
    case TDD_AUDIO_CMD_SET_VOLUME:
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

/**
 * @brief Close no-codec audio device
 * @param[in] handle audio handle
 * @return OPRT_OK on success
 */
static OPERATE_RET __tdd_audio_no_codec_close(TDD_AUDIO_HANDLE_T handle)
{
    OPERATE_RET rt = OPRT_OK;

    return rt;
}

/**
 * @brief Register no-codec audio driver
 * @param[in] name driver name
 * @param[in] cfg board audio config
 * @return OPRT_OK on success
 */
OPERATE_RET tdd_audio_no_codec_register(char *name, TDD_AUDIO_NO_CODEC_T cfg)
{
    OPERATE_RET rt = OPRT_OK;
    SI91X_I2S_HANDLE_T *_hdl = NULL;

    TDD_AUDIO_INTFS_T intfs = {0};
    TDD_AUDIO_INFO_T info = {0};


    _hdl = (SI91X_I2S_HANDLE_T *)tal_malloc(sizeof(SI91X_I2S_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(_hdl, OPRT_MALLOC_FAILED);
    memset(_hdl, 0, sizeof(SI91X_I2S_HANDLE_T));

    // default play volume
    _hdl->play_volume = 80;

    info.sample_rate = I2S_RX_SAMPLE_RATE,
    info.sample_ch_num = 1,  // mono
    info.sample_bits = 16,   // 16-bit
    info.sample_tm_ms = I2S_READ_INTERVAL_MS;

    memcpy(&_hdl->cfg, &cfg, sizeof(TDD_AUDIO_NO_CODEC_T));

    intfs.open = __tdd_audio_no_codec_open;
    intfs.play = __tdd_audio_no_codec_play;
    intfs.config = __tdd_audio_no_codec_config;
    intfs.close = __tdd_audio_no_codec_close;

    TUYA_CALL_ERR_GOTO(tdl_audio_driver_register(name, (TDD_AUDIO_HANDLE_T)_hdl, &intfs, &info), __ERR);
    return rt;

__ERR:
    if (NULL == _hdl) {
        tal_free(_hdl);
        _hdl = NULL;
    }

    return rt;
}
