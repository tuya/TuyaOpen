/**
 * @file ai_audio_input.c
 * @brief Implementation of audio input handling functions including initialization, 
 *        enabling/disabling detection, and setting wakeup types.
 *
 * This file contains the implementation of functions responsible for managing audio input operations
 * such as initializing the audio system,enabling and disabling audio detection, 
 * and setting the type of wakeup mechanism (e.g., VAD, ASR).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tkl_asr.h"
#include "tkl_audio.h"
#include "tkl_vad.h"
#include "tkl_memory.h"
#include "tkl_system.h"
#include "tkl_thread.h"
#include "tkl_mutex.h"

#include "tal_api.h"
#include "tuya_ringbuf.h"

#include "ai_audio_player.h"
#include "ai_audio_input.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define INPUT_RINGBUFF_LEN         (1024 * 4)
#define ASR_PROCE_UNIT_NUM          30
#define ASR_WAKEUP_TIMEOUT_MS      (20000)
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t  is_init;
    uint8_t  is_enable_workup_detect;
    uint8_t  is_enable_interrupt;

    AI_AUDIO_INPUT_STATE_E     state;
    AI_AUDIO_INPUT_WAKEUP_TP_E wakeup_tp;

    uint8_t         is_asr_wakeup_alive;
    TIMER_ID        asr_wakeup_timer_id;
    TUYA_RINGBUFF_T asr_ringbuff_hdl;
    MUTEX_HANDLE    asr_rb_mutex;

    TUYA_RINGBUFF_T ringbuff_hdl;
    MUTEX_HANDLE    rb_mutex;

} AI_AUDIO_INPUT_INFO_T;

/***********************************************************
***********************const declaration********************
***********************************************************/
const static TKL_ASR_WAKEUP_WORD_E cWAKEUP_KEYWORD_LIST[] = {
    TKL_ASR_WAKEUP_NIHAO_TUYA,
    TKL_ASR_WAKEUP_XIAOZHI_TONGXUE,
};

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_AUDIO_INOUT_INFORM_CB sg_audio_input_inform_cb = NULL;
static THREAD_HANDLE sg_ai_audio_input_thrd_hdl = NULL;
static AI_AUDIO_INPUT_INFO_T sg_audio_input;
/***********************************************************
***********************function define**********************
***********************************************************/
static void __ai_audio_asr_wakeup_timeout(TIMER_ID timer_id, void *arg)
{
    PR_NOTICE("asr wakeup timeout");
    AI_AUDIO_INPUT_EVENT_E event = (AI_AUDIO_INPUT_STATE_DETECTING == sg_audio_input.state)
                                       ? AI_AUDIO_INPUT_EVT_DETECTING
                                       : AI_AUDIO_INPUT_EVT_ENTER_DETECT;

    sg_audio_input.state = AI_AUDIO_INPUT_STATE_DETECTING;
    sg_audio_input.is_asr_wakeup_alive = false;
    if (sg_audio_input_inform_cb) {
        sg_audio_input_inform_cb(event, NULL, 0, NULL);
    }
}

static OPERATE_RET __ai_audio_open_vad(void)
{
    OPERATE_RET rt = OPRT_OK;

    TKL_VAD_CONFIG_T vad_config;
    vad_config.sample_rate = TKL_AUDIO_SAMPLE_16K;
    vad_config.channel_num = TKL_AUDIO_CHANNEL_MONO;
    vad_config.speech_min_ms = 300;
    vad_config.noise_min_ms = 500;
    vad_config.scale = 2.5;
    vad_config.frame_duration_ms = 10;

    TUYA_CALL_ERR_RETURN(tkl_vad_init(&vad_config));
    TUYA_CALL_ERR_RETURN(tkl_vad_start());

    return OPRT_OK;
}

static OPERATE_RET __ai_audio_close_vad(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_vad_deinit());

    return OPRT_OK;
}

static OPERATE_RET __ai_audio_open_asr(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_asr_init());
    TUYA_CALL_ERR_RETURN(tkl_asr_wakeup_word_config((TKL_ASR_WAKEUP_WORD_E *)cWAKEUP_KEYWORD_LIST, CNTSOF(cWAKEUP_KEYWORD_LIST)));
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__ai_audio_asr_wakeup_timeout, NULL, &sg_audio_input.asr_wakeup_timer_id));

    uint32_t asr_ringbuff_len = tkl_asr_get_process_uint_size() * ASR_PROCE_UNIT_NUM;
    TUYA_CALL_ERR_RETURN(tuya_ring_buff_create(asr_ringbuff_len, OVERFLOW_PSRAM_STOP_TYPE, \
                                               &sg_audio_input.asr_ringbuff_hdl));
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_audio_input.asr_rb_mutex));

    return OPRT_OK;
}

static OPERATE_RET __ai_audio_close_asr(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_asr_deinit());

    TUYA_CALL_ERR_RETURN(tal_sw_timer_delete(sg_audio_input.asr_wakeup_timer_id));

    tal_mutex_lock(sg_audio_input.asr_rb_mutex);
    TUYA_CALL_ERR_RETURN(tuya_ring_buff_free(sg_audio_input.asr_ringbuff_hdl));
    sg_audio_input.asr_ringbuff_hdl = NULL;
    tal_mutex_unlock(sg_audio_input.asr_rb_mutex);

    tal_mutex_release(sg_audio_input.asr_rb_mutex);
    sg_audio_input.asr_rb_mutex = NULL;

    return OPRT_OK;
}

static void __ai_audio_wakeup_asr_feed(void *data, uint32_t len)
{
    if (AI_AUDIO_INPUT_WAKEUP_ASR != sg_audio_input.wakeup_tp) {
        return;
    }

    tal_mutex_lock(sg_audio_input.asr_rb_mutex);
    if(TKL_VAD_STATUS_NONE == tkl_vad_get_status()) {
        uint32_t rb_used_size = tuya_ring_buff_used_size_get(sg_audio_input.asr_ringbuff_hdl);
        if (rb_used_size > AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_VAD_ACITVE_TM_MS)) {
            uint32_t discard_size = rb_used_size - AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_VAD_ACITVE_TM_MS);
            tuya_ring_buff_discard(sg_audio_input.asr_ringbuff_hdl, discard_size);
        }
    }else {
        tuya_ring_buff_write(sg_audio_input.asr_ringbuff_hdl, data, len);
    }

    if(0 == tuya_ring_buff_free_size_get(sg_audio_input.asr_ringbuff_hdl)) {
        PR_DEBUG("asr fuill");
    }

    tal_mutex_unlock(sg_audio_input.asr_rb_mutex);

    return;
}

static void __ai_audio_asr_buff_reset(void)
{
    tal_mutex_lock(sg_audio_input.asr_rb_mutex);
    tuya_ring_buff_reset(sg_audio_input.asr_ringbuff_hdl);
    tal_mutex_unlock(sg_audio_input.asr_rb_mutex);
}

static void __ai_audio_asr_wakeup_timer_start(void)
{
    if (AI_AUDIO_INPUT_WAKEUP_ASR != sg_audio_input.wakeup_tp) {
        return;
    }

    sg_audio_input.is_asr_wakeup_alive = true;

    tal_sw_timer_start(sg_audio_input.asr_wakeup_timer_id, ASR_WAKEUP_TIMEOUT_MS, TAL_TIMER_ONCE);
}

static TKL_ASR_WAKEUP_WORD_E __asr_recognize_wakeup_keyword(void)
{
    uint32_t i = 0, fc = 0;
    TKL_ASR_WAKEUP_WORD_E wakeup_word = TKL_ASR_WAKEUP_WORD_UNKNOWN;
    uint32_t uint_size = 0, rb_used_size = 0;
    uint8_t *p_buf = NULL;

    if (AI_AUDIO_INPUT_WAKEUP_ASR != sg_audio_input.wakeup_tp) {
        return TKL_ASR_WAKEUP_WORD_UNKNOWN;
    }

    uint_size = tkl_asr_get_process_uint_size();
    tal_mutex_lock(sg_audio_input.asr_rb_mutex);
    rb_used_size = tuya_ring_buff_used_size_get(sg_audio_input.asr_ringbuff_hdl);
    tal_mutex_unlock(sg_audio_input.asr_rb_mutex);
    if (rb_used_size < uint_size) {
        return TKL_ASR_WAKEUP_WORD_UNKNOWN;
    }

    p_buf = tkl_system_psram_malloc(uint_size);
    if (NULL == p_buf) {
        PR_ERR("malloc fail");
        return TKL_ASR_WAKEUP_WORD_UNKNOWN;
    }

    fc = rb_used_size / uint_size;
    for (i = 0; i < fc; i++) {
        tal_mutex_lock(sg_audio_input.asr_rb_mutex);
        tuya_ring_buff_read(sg_audio_input.asr_ringbuff_hdl, p_buf, uint_size);
        tal_mutex_unlock(sg_audio_input.asr_rb_mutex);

        wakeup_word = tkl_asr_recognize_wakeup_word(p_buf, uint_size);
        if (wakeup_word != TKL_ASR_WAKEUP_WORD_UNKNOWN) {
            break;
        }
    }

    tkl_system_psram_free(p_buf);

    return wakeup_word;
}

static void __ai_audio_wakeup_feed(uint8_t *data, uint32_t len)
{
    if (AI_AUDIO_INPUT_WAKEUP_VAD == sg_audio_input.wakeup_tp) {
        tkl_vad_feed(data, len);
    } else if (AI_AUDIO_INPUT_WAKEUP_ASR == sg_audio_input.wakeup_tp) {
        tkl_vad_feed(data, len);

        __ai_audio_wakeup_asr_feed(data, len);
    } else {
        ;
    }
}

static OPERATE_RET __ai_audio_input_rb_reset(void)
{
    tal_mutex_lock(sg_audio_input.rb_mutex);
    tuya_ring_buff_reset(sg_audio_input.ringbuff_hdl);
    tal_mutex_unlock(sg_audio_input.rb_mutex);

    return OPRT_OK;
}

TKL_ASR_WAKEUP_WORD_E __ai_audio_input_update_new_state(void)
{
    TKL_ASR_WAKEUP_WORD_E key = TKL_ASR_WAKEUP_WORD_UNKNOWN;

    switch (sg_audio_input.wakeup_tp) {
    case AI_AUDIO_INPUT_WAKEUP_MANUAL:
        if (false == sg_audio_input.is_enable_workup_detect) {
            sg_audio_input.state = AI_AUDIO_INPUT_STATE_DETECTING;
        }else {
            sg_audio_input.state = AI_AUDIO_INPUT_STATE_AWAKE;
        }
        break;
    case AI_AUDIO_INPUT_WAKEUP_VAD:
        if (false == sg_audio_input.is_enable_workup_detect) {
            sg_audio_input.state = AI_AUDIO_INPUT_STATE_IDLE;
            return key;
        }

        if (TKL_VAD_STATUS_SPEECH == tkl_vad_get_status()) {
            sg_audio_input.state = AI_AUDIO_INPUT_STATE_AWAKE;
        } else {
            sg_audio_input.state = AI_AUDIO_INPUT_STATE_DETECTING;
        }
        break;
    case AI_AUDIO_INPUT_WAKEUP_ASR: {
        if (false == sg_audio_input.is_enable_workup_detect) {
            sg_audio_input.state = AI_AUDIO_INPUT_STATE_IDLE;
            return key;
        }

        if (TKL_VAD_STATUS_SPEECH == tkl_vad_get_status()) {
            key = __asr_recognize_wakeup_keyword();
            if (TKL_ASR_WAKEUP_WORD_UNKNOWN != key) {
                PR_NOTICE("asr wakeup key: %d", key);
                sg_audio_input.state = AI_AUDIO_INPUT_STATE_DETECTED_WORD;
                __ai_audio_asr_wakeup_timer_start();
            } else {
                if(AI_AUDIO_INPUT_STATE_DETECTED_WORD == sg_audio_input.state) {
                    break;  //waiting the vad stop
                }

                if (true == sg_audio_input.is_asr_wakeup_alive) {
                    sg_audio_input.state = AI_AUDIO_INPUT_STATE_AWAKE;
                } else {
                    sg_audio_input.state = AI_AUDIO_INPUT_STATE_DETECTING;
                }
            }
        } else {
            sg_audio_input.state = AI_AUDIO_INPUT_STATE_DETECTING;
        }
    } break;
    default:
        PR_ERR("wakeup tp :%d not support", sg_audio_input.wakeup_tp);
        break;
    }

    return key;
}

AI_AUDIO_INPUT_EVENT_E __ai_audio_input_get_event(AI_AUDIO_INPUT_STATE_E curr_state, AI_AUDIO_INPUT_STATE_E last_state)
{
    AI_AUDIO_INPUT_EVENT_E event = AI_AUDIO_INPUT_EVT_IDLE;

    switch (curr_state) {
    case AI_AUDIO_INPUT_STATE_IDLE:
        event = AI_AUDIO_INPUT_EVT_IDLE;
        break;
    case AI_AUDIO_INPUT_STATE_DETECTING:
        if (AI_AUDIO_INPUT_STATE_DETECTING == last_state) {
            event = AI_AUDIO_INPUT_EVT_DETECTING;
        } else {
            event = AI_AUDIO_INPUT_EVT_ENTER_DETECT;
        }
        break;
    case AI_AUDIO_INPUT_STATE_AWAKE:
        if (AI_AUDIO_INPUT_STATE_AWAKE == last_state) {
            event = AI_AUDIO_INPUT_EVT_AWAKE;
        } else {
            event = AI_AUDIO_INPUT_EVT_WAKEUP;
        }
        break;
    case AI_AUDIO_INPUT_STATE_DETECTED_WORD:
        if(AI_AUDIO_INPUT_STATE_DETECTED_WORD == last_state) {
            event = AI_AUDIO_INPUT_EVT_NONE;  //waiting the vad stop
        }else {
            event = AI_AUDIO_INPUT_EVT_ASR_WORD;
        }
        break;
    }
    return event;
}

static int __ai_audio_get_input_frame(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    OPERATE_RET rt = OPRT_OK;

    if (false == sg_audio_input.is_enable_interrupt) {
        if (true == ai_audio_player_is_playing()) {
            return 0;
        }
    }

    __ai_audio_wakeup_feed((uint8_t *)pframe->pbuf, pframe->used_size);

    tal_mutex_lock(sg_audio_input.rb_mutex);
    tuya_ring_buff_write(sg_audio_input.ringbuff_hdl, pframe->pbuf, pframe->used_size);
    tal_mutex_unlock(sg_audio_input.rb_mutex);

    return rt;
}

static void __ai_audio_handle_frame_task(void *arg)
{
    uint32_t rb_used_sz = 0;
    uint8_t *p_buff = NULL;
    AI_AUDIO_INPUT_EVENT_E event = AI_AUDIO_INPUT_EVT_IDLE;
    TKL_ASR_WAKEUP_WORD_E wakeup_word = TKL_ASR_WAKEUP_WORD_UNKNOWN;
    AI_AUDIO_INPUT_STATE_E last_state = AI_AUDIO_INPUT_STATE_IDLE;

    while (1) {
        rb_used_sz = tuya_ring_buff_used_size_get(sg_audio_input.ringbuff_hdl);
        if (0 == rb_used_sz) {
            tkl_system_sleep(10);
            continue;
        }

        p_buff = tkl_system_psram_malloc(rb_used_sz);
        if (NULL == p_buff) {
            PR_ERR("malloc failed");
            tkl_system_sleep(100);
            continue;
        }

        tal_mutex_lock(sg_audio_input.rb_mutex);
        tuya_ring_buff_read(sg_audio_input.ringbuff_hdl, p_buff, rb_used_sz);
        tal_mutex_unlock(sg_audio_input.rb_mutex);

        last_state = sg_audio_input.state;
        wakeup_word = __ai_audio_input_update_new_state();
        event = __ai_audio_input_get_event(sg_audio_input.state, last_state);

        // PR_DEBUG("last state:%d state :%d event:%d, wakeup_word:%d \r\n",last_state, sg_audio_input.state, event, wakeup_word);

        if ((event != AI_AUDIO_INPUT_EVT_NONE) && sg_audio_input_inform_cb) {
            void *arg = (AI_AUDIO_INPUT_EVT_ASR_WORD == event) ? (void *)&wakeup_word : NULL;
            sg_audio_input_inform_cb(event, p_buff, rb_used_sz, arg);
        }

        tkl_system_psram_free(p_buff);
        p_buff = NULL;

        tal_system_sleep(5);
    }
}

static OPERATE_RET __ai_audio_input_hardware_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TKL_AUDIO_CONFIG_T config;
    memset(&config, 0, sizeof(TKL_AUDIO_CONFIG_T));

    config.ai_chn = TKL_AI_0;
    config.sample = TKL_AUDIO_SAMPLE_16K;
    config.datebits = TKL_AUDIO_DATABITS_16;
    config.channel = TKL_AUDIO_CHANNEL_MONO;
    config.codectype = TKL_CODEC_AUDIO_PCM;
    config.card = TKL_AUDIO_TYPE_BOARD;
    config.put_cb = __ai_audio_get_input_frame;
    config.enable = 0;

    config.spk_sample = TKL_AUDIO_SAMPLE_16K;
    config.spk_gpio = SPEAKER_EN_PIN;
    config.spk_gpio_polarity = TUYA_GPIO_LEVEL_LOW;

    TUYA_CALL_ERR_RETURN(tkl_ai_init(&config, 0));
    TUYA_CALL_ERR_RETURN(tkl_ai_start(0, 0));
    TUYA_CALL_ERR_RETURN(tkl_ai_set_vol(TKL_AUDIO_TYPE_BOARD, 0, 80));

    PR_DEBUG("__ai_audio_input_hardware_init success");

    return OPRT_OK;
}

static OPERATE_RET __ai_audio_input_set_wakeup_tp(AI_AUDIO_INPUT_WAKEUP_TP_E wakeup_tp)
{
    OPERATE_RET rt = OPRT_OK;

    if (AI_AUDIO_INPUT_WAKEUP_VAD == wakeup_tp) {
        TUYA_CALL_ERR_RETURN(__ai_audio_open_vad());
        sg_audio_input.is_enable_interrupt = true;

    } else if (AI_AUDIO_INPUT_WAKEUP_ASR == wakeup_tp) {
        TUYA_CALL_ERR_RETURN(__ai_audio_open_vad());
        TUYA_CALL_ERR_RETURN(__ai_audio_open_asr());
        sg_audio_input.is_enable_interrupt = true;
    }else {
        sg_audio_input.is_enable_interrupt = false;
    }

    sg_audio_input.wakeup_tp = wakeup_tp;

    return OPRT_OK;
}

/**
 * @brief Initializes the audio input system with the provided configuration and callback.
 * @param cfg Pointer to the configuration structure for audio input.
 * @param cb Callback function to be called for audio input events.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_init(AI_AUDIO_INPUT_CFG_T *cfg, AI_AUDIO_INOUT_INFORM_CB cb)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == cfg || NULL == cb) {
        return OPRT_INVALID_PARM;
    }

    if (true == sg_audio_input.is_init) {
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(tuya_ring_buff_create(INPUT_RINGBUFF_LEN, OVERFLOW_PSRAM_STOP_TYPE,\
                                               &sg_audio_input.ringbuff_hdl));
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_audio_input.rb_mutex));

    TUYA_CALL_ERR_RETURN(__ai_audio_input_hardware_init());

    __ai_audio_input_set_wakeup_tp(cfg->wakeup_tp);
    
    sg_audio_input_inform_cb = cb;

    TUYA_CALL_ERR_RETURN(tkl_thread_create_in_psram(&sg_ai_audio_input_thrd_hdl, "audio_input", 1024 * 4, THREAD_PRIO_1,
                                                    __ai_audio_handle_frame_task, NULL));

    return OPRT_OK;
}

/**
 * @brief Enables the audio input detection and wakeup functionality.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_enable_detect_wakeup(void)
{
    if(true == sg_audio_input.is_enable_workup_detect) {
        PR_NOTICE("input detect wakeup already enable");
        return OPRT_OK;
    }

    sg_audio_input.is_enable_workup_detect = true;
    if (AI_AUDIO_INPUT_WAKEUP_VAD == sg_audio_input.wakeup_tp || \
        AI_AUDIO_INPUT_WAKEUP_ASR == sg_audio_input.wakeup_tp ) {
        tkl_vad_start();
    } 

    PR_NOTICE("ai audio input open");

    return OPRT_OK;
}

/**
 * @brief Disables the audio input detection and wakeup functionality.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_disable_detect_wakeup(void)
{
    if(false == sg_audio_input.is_enable_workup_detect) {
        PR_NOTICE("input detect wakeup already disable");
        return OPRT_OK;
    }

    if (AI_AUDIO_INPUT_WAKEUP_ASR == sg_audio_input.wakeup_tp) {
        if (tal_sw_timer_is_running(sg_audio_input.asr_wakeup_timer_id)) {
            tal_sw_timer_stop(sg_audio_input.asr_wakeup_timer_id);
        }

        tkl_vad_stop();
        __ai_audio_asr_buff_reset();

    }else if(AI_AUDIO_INPUT_WAKEUP_VAD == sg_audio_input.wakeup_tp) {
        tkl_vad_stop();
    }

    sg_audio_input.is_enable_workup_detect = false;

    PR_NOTICE("ai audio input close");

    return OPRT_OK;
}
/**
 * @brief Restarts the ASR detection for wakeup words.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_restart_asr_detect_wakeup_word(void)
{
    AI_AUDIO_INPUT_EVENT_E event = 0;

    if (sg_audio_input.wakeup_tp != AI_AUDIO_INPUT_WAKEUP_ASR) {
        return OPRT_NOT_SUPPORTED;
    }

    if (false == sg_audio_input.is_enable_workup_detect) {
        PR_NOTICE("input detect wakeup is disable");
        return OPRT_COM_ERROR;
    }

    if (tal_sw_timer_is_running(sg_audio_input.asr_wakeup_timer_id)) {
        tal_sw_timer_stop(sg_audio_input.asr_wakeup_timer_id);
    }

    sg_audio_input.state = AI_AUDIO_INPUT_STATE_DETECTING;
    sg_audio_input.is_asr_wakeup_alive = false;
    if (sg_audio_input_inform_cb) {
        event = (AI_AUDIO_INPUT_STATE_DETECTING == sg_audio_input.state) ? AI_AUDIO_INPUT_EVT_DETECTING
                                                                         : AI_AUDIO_INPUT_EVT_ENTER_DETECT;
        sg_audio_input_inform_cb(event, NULL, 0, NULL);
    }

    PR_NOTICE("ai audio input restart asr detect wakeup word");

    return OPRT_OK;
}

/**
 * @brief Triggers the ASR wakeup process.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_trigger_asr_awake(void)
{
    if (sg_audio_input.wakeup_tp != AI_AUDIO_INPUT_WAKEUP_ASR) {
        return OPRT_NOT_SUPPORTED;
    }

    if (false == sg_audio_input.is_enable_workup_detect) {
        PR_NOTICE("input detect wakeup is disable");
        return OPRT_COM_ERROR;
    }

    __ai_audio_asr_wakeup_timer_start();

    return OPRT_OK;
}

/**
 * @brief Sets the wakeup type for audio input.
 * @param wakeup_tp The type of wakeup to set (e.g., VAD, ASR).
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_set_wakeup_tp(AI_AUDIO_INPUT_WAKEUP_TP_E wakeup_tp)
{
    if(wakeup_tp >= AI_AUDIO_INPUT_WAKEUP_MAX) {
        PR_ERR("wakeup_tp:%d over range", wakeup_tp);
        return OPRT_INVALID_PARM;
    }

    if(true == sg_audio_input.is_enable_workup_detect) {
        PR_NOTICE("input detect wakeup is enable please disable first");
        return OPRT_OK;
    }

    __ai_audio_input_rb_reset();

    if(AI_AUDIO_INPUT_WAKEUP_VAD == sg_audio_input.wakeup_tp) {
        __ai_audio_close_vad();
    }else if(AI_AUDIO_INPUT_WAKEUP_ASR == sg_audio_input.wakeup_tp) {
        __ai_audio_close_vad();
        __ai_audio_close_asr();
    }

    __ai_audio_input_set_wakeup_tp(wakeup_tp);

    return OPRT_OK;
}