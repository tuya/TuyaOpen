/**
 * @file ai_audio_input.c
 * @brief Implementation of audio input handling functions including initialization,
 *        enabling/disabling detection, and setting wakeup types.
 *
 * This file contains the implementation of functions responsible for managing audio input operations
 * such as initializing the audio system, enabling and disabling audio detection,
 * and setting the type of wakeup mechanism (e.g., VAD, ASR).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_asr.h"
#include "tkl_vad.h"
#include "tkl_memory.h"
#include "tkl_system.h"
#include "tkl_thread.h"
#include "tkl_mutex.h"

#include "app_board_api.h"
#include "tdl_audio_manage.h"

#include "tal_api.h"
#include "tuya_ringbuf.h"

#include "ai_audio.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define AI_AUDIO_INPUT_RB_TIME_MS (10 * 1000)

#define ASR_PROCE_UNIT_NUM    30
#define ASR_WAKEUP_TIMEOUT_MS (20000)
/***********************************************************
***********************typedef define***********************
***********************************************************/
// clang-format off
typedef struct {
    bool                is_wakeup;
    bool                is_need_inform_wakeup_stop;
    TIMER_ID            wakeup_timer_id;
    MUTEX_HANDLE        rb_mutex;
    TUYA_RINGBUFF_T     feed_ringbuff;
    uint32_t            buff_len;
}AI_AUDIO_INPUT_ASR_T;

typedef struct {
    bool                          is_init;
    bool                          is_enable_get_valid_data;

    AI_AUDIO_INPUT_STATE_E         state;
    AI_AUDIO_INPUT_VALID_METHOD_E  method;

    TUYA_RINGBUFF_T                ringbuff_hdl;
    MUTEX_HANDLE                   rb_mutex;

    AI_AUDIO_INPUT_ASR_T           asr;  

} AI_AUDIO_INPUT_INFO_T;
// clang-format on

/***********************************************************
***********************const declaration********************
***********************************************************/
const static TKL_ASR_WAKEUP_WORD_E cWAKEUP_KEYWORD_LIST[] = {
#if defined(ENABLE_WAKEUP_KEYWORD_NIHAO_TUYA) && (ENABLE_WAKEUP_KEYWORD_NIHAO_TUYA == 1)
    TKL_ASR_WAKEUP_NIHAO_TUYA,
#endif

#if defined(ENABLE_WAKEUP_KEYWORD_NIHAO_XIAOZHI) && (ENABLE_WAKEUP_KEYWORD_NIHAO_XIAOZHI == 1)
    TKL_ASR_WAKEUP_NIHAO_XIAOZHI,
#endif

#if defined(TKL_ASR_WAKEUP_XIAOZHI_TONGXUE) && (TKL_ASR_WAKEUP_XIAOZHI_TONGXUE == 1)
    TKL_ASR_WAKEUP_XIAOZHI_TONGXUE,
#endif

#if defined(TKL_ASR_WAKEUP_XIAOZHI_GUANJIA) && (TKL_ASR_WAKEUP_XIAOZHI_GUANJIA == 1)
    TKL_ASR_WAKEUP_XIAOZHI_GUANJIA,
#endif
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
    sg_audio_input.asr.is_wakeup = false;
    sg_audio_input.asr.is_need_inform_wakeup_stop = true;
}

static OPERATE_RET __ai_audio_asr_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_GOTO(tkl_asr_init(), __ASR_INIT_ERR);
    TUYA_CALL_ERR_GOTO(
        tkl_asr_wakeup_word_config((TKL_ASR_WAKEUP_WORD_E *)cWAKEUP_KEYWORD_LIST, CNTSOF(cWAKEUP_KEYWORD_LIST)),
        __ASR_INIT_ERR);
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__ai_audio_asr_wakeup_timeout, NULL, &sg_audio_input.asr.wakeup_timer_id),
                       __ASR_INIT_ERR);

    sg_audio_input.asr.buff_len = tkl_asr_get_process_uint_size() * ASR_PROCE_UNIT_NUM;
    PR_DEBUG("sg_audio_input.asr.buff_len:%d", sg_audio_input.asr.buff_len);
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(sg_audio_input.asr.buff_len + tkl_asr_get_process_uint_size(),
                                             OVERFLOW_PSRAM_STOP_TYPE, &sg_audio_input.asr.feed_ringbuff),
                       __ASR_INIT_ERR);
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_audio_input.asr.rb_mutex), __ASR_INIT_ERR);

    return OPRT_OK;

__ASR_INIT_ERR:
    tkl_asr_deinit();

    if (sg_audio_input.asr.wakeup_timer_id) {
        tal_sw_timer_delete(sg_audio_input.asr.wakeup_timer_id);
        sg_audio_input.asr.wakeup_timer_id = NULL;
    }

    if (sg_audio_input.asr.feed_ringbuff) {
        tuya_ring_buff_free(sg_audio_input.asr.feed_ringbuff);
        sg_audio_input.asr.feed_ringbuff = NULL;
    }

    if (sg_audio_input.asr.rb_mutex) {
        tal_mutex_release(sg_audio_input.asr.rb_mutex);
        sg_audio_input.asr.rb_mutex = NULL;
    }

    return rt;
}

static OPERATE_RET __ai_audio_asr_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_asr_deinit());

    TUYA_CALL_ERR_LOG(tal_sw_timer_delete(sg_audio_input.asr.wakeup_timer_id));

    TUYA_CALL_ERR_LOG(tal_mutex_lock(sg_audio_input.asr.rb_mutex));
    TUYA_CALL_ERR_LOG(tuya_ring_buff_free(sg_audio_input.asr.feed_ringbuff));
    sg_audio_input.asr.feed_ringbuff = NULL;
    TUYA_CALL_ERR_LOG(tal_mutex_unlock(sg_audio_input.asr.rb_mutex));

    TUYA_CALL_ERR_LOG(tal_mutex_release(sg_audio_input.asr.rb_mutex));
    sg_audio_input.asr.rb_mutex = NULL;

    return OPRT_OK;
}

static void __ai_audio_asr_feed(void *data, uint32_t len)
{
    tal_mutex_lock(sg_audio_input.asr.rb_mutex);
    if (TKL_VAD_STATUS_NONE == tkl_vad_get_status()) {
        uint32_t rb_used_size = tuya_ring_buff_used_size_get(sg_audio_input.asr.feed_ringbuff);
        if (rb_used_size > AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_VAD_ACITVE_TM_MS)) {
            uint32_t discard_size = rb_used_size - AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_VAD_ACITVE_TM_MS);
            tuya_ring_buff_discard(sg_audio_input.asr.feed_ringbuff, discard_size);
        }
#if defined(PLATFORM_ESP32) && (PLATFORM_ESP32 == 1)
        tuya_ring_buff_write(sg_audio_input.asr.feed_ringbuff, data, len);
#endif
    } else {
        tuya_ring_buff_write(sg_audio_input.asr.feed_ringbuff, data, len);
    }
    tal_mutex_unlock(sg_audio_input.asr.rb_mutex);

    return;
}

static TKL_ASR_WAKEUP_WORD_E __asr_recognize_wakeup_keyword(void)
{
    uint32_t i = 0, fc = 0;
    TKL_ASR_WAKEUP_WORD_E wakeup_word = TKL_ASR_WAKEUP_WORD_UNKNOWN;
    uint32_t uint_size = 0, feed_size = 0;

    uint_size = tkl_asr_get_process_uint_size();
    tal_mutex_lock(sg_audio_input.asr.rb_mutex);
    feed_size = tuya_ring_buff_used_size_get(sg_audio_input.asr.feed_ringbuff);
    tal_mutex_unlock(sg_audio_input.asr.rb_mutex);
    if (feed_size < uint_size) {
        return TKL_ASR_WAKEUP_WORD_UNKNOWN;
    }

#if 0
    tal_mutex_lock(sg_audio_input.asr.rb_mutex);
    tuya_ring_buff_read(sg_audio_input.asr.feed_ringbuff, sg_audio_input.asr.recognize_buff, sg_audio_input.asr.buff_len);
    tal_mutex_unlock(sg_audio_input.asr.rb_mutex);

    fc = sg_audio_input.asr.buff_len / uint_size;
    for (i = 0; i < fc; i++) {
        PR_DEBUG("i->:%d", i);
        wakeup_word = tkl_asr_recognize_wakeup_word(sg_audio_input.asr.recognize_buff[i*uint_size], uint_size);
        if (wakeup_word != TKL_ASR_WAKEUP_WORD_UNKNOWN) {
            break;
        }
        PR_DEBUG("i<-:%d", i);
    }
#else
    uint8_t *p_buf = tkl_system_psram_malloc(uint_size);
    if (NULL == p_buf) {
        PR_ERR("malloc fail");
        return TKL_ASR_WAKEUP_WORD_UNKNOWN;
    }

    fc = feed_size / uint_size;
    for (i = 0; i < fc; i++) {
        tal_mutex_lock(sg_audio_input.asr.rb_mutex);
        tuya_ring_buff_read(sg_audio_input.asr.feed_ringbuff, p_buf, uint_size);
        tal_mutex_unlock(sg_audio_input.asr.rb_mutex);

        wakeup_word = tkl_asr_recognize_wakeup_word(p_buf, uint_size);
        if (wakeup_word != TKL_ASR_WAKEUP_WORD_UNKNOWN) {
            break;
        }
    }

    tkl_system_psram_free(p_buf);

#endif

    return wakeup_word;
}

static void __ai_audio_asr_wakeup(void)
{
    sg_audio_input.asr.is_wakeup = true;
    sg_audio_input.asr.is_need_inform_wakeup_stop = false;
    tal_sw_timer_start(sg_audio_input.asr.wakeup_timer_id, ASR_WAKEUP_TIMEOUT_MS, TAL_TIMER_ONCE);
}

static OPERATE_RET __ai_audio_vad_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TKL_VAD_CONFIG_T vad_config;
    vad_config.sample_rate = 16000;
    vad_config.channel_num = 1;
    vad_config.speech_min_ms = 300;
    vad_config.noise_min_ms = 500;
    vad_config.scale = 2.5;
    vad_config.frame_duration_ms = 10;

    TUYA_CALL_ERR_RETURN(tkl_vad_init(&vad_config));

    return OPRT_OK;
}

static OPERATE_RET __ai_audio_vad_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_vad_deinit());

    return OPRT_OK;
}

static void __ai_audio_detect_valid_data_feed(AI_AUDIO_INPUT_VALID_METHOD_E method, uint8_t *data, uint32_t len)
{
    if (AI_AUDIO_INPUT_VALID_METHOD_VAD == method) {
        tkl_vad_feed(data, len);
    } else if (AI_AUDIO_INPUT_VALID_METHOD_ASR == method) {
        tkl_vad_feed(data, len);

        __ai_audio_asr_feed(data, len);
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

AI_AUDIO_INPUT_STATE_E __ai_audio_input_get_new_state(AI_AUDIO_INPUT_VALID_METHOD_E method)
{
    AI_AUDIO_INPUT_STATE_E state = AI_AUDIO_INPUT_STATE_IDLE;

    switch (method) {
    case AI_AUDIO_INPUT_VALID_METHOD_MANUAL:
        // the state is manually controlled from the outside.
        break;
    case AI_AUDIO_INPUT_VALID_METHOD_VAD:
        if (TKL_VAD_STATUS_SPEECH == tkl_vad_get_status()) {
            state = AI_AUDIO_INPUT_STATE_GET_VALID_DATA;
        } else {
            state = AI_AUDIO_INPUT_STATE_DETECTING;
        }
        break;
    case AI_AUDIO_INPUT_VALID_METHOD_ASR: {
        TKL_ASR_WAKEUP_WORD_E wakeup_word;

#if defined(PLATFORM_ESP32) && (PLATFORM_ESP32 == 1)
        wakeup_word = __asr_recognize_wakeup_keyword();
        if (TKL_ASR_WAKEUP_WORD_UNKNOWN != wakeup_word) {
            PR_NOTICE("asr wakeup key: %d", wakeup_word);
            state = AI_AUDIO_INPUT_STATE_ASR_WAKEUP_WORD;
            __ai_audio_asr_wakeup();
        } else {
            if (true == sg_audio_input.asr.is_wakeup) {
                if (tkl_vad_get_status() == TKL_VAD_STATUS_SPEECH) {
                    state = AI_AUDIO_INPUT_STATE_GET_VALID_DATA;
                } else {
                    state = AI_AUDIO_INPUT_STATE_DETECTING;
                }
            } else {
                state = AI_AUDIO_INPUT_STATE_DETECTING;
            }
        }
#else
        if (TKL_VAD_STATUS_SPEECH == tkl_vad_get_status()) {
            wakeup_word = __asr_recognize_wakeup_keyword();
            if (wakeup_word != TKL_ASR_WAKEUP_WORD_UNKNOWN) {
                state = AI_AUDIO_INPUT_STATE_ASR_WAKEUP_WORD;
                __ai_audio_asr_wakeup();
            } else {
                if (true == sg_audio_input.asr.is_wakeup) {
                    state = AI_AUDIO_INPUT_STATE_GET_VALID_DATA;
                } else {
                    state = AI_AUDIO_INPUT_STATE_DETECTING;
                }
            }
        } else {
            state = AI_AUDIO_INPUT_STATE_DETECTING;
        }
#endif
    } break;
    default:
        PR_ERR("get vaild voice method:%d not support", method);
        break;
    }

    return state;
}

AI_AUDIO_INPUT_EVENT_E __ai_audio_input_get_event(AI_AUDIO_INPUT_STATE_E curr_state, AI_AUDIO_INPUT_STATE_E last_state)
{
    AI_AUDIO_INPUT_EVENT_E event = AI_AUDIO_INPUT_EVT_NONE;

    switch (curr_state) {
    case AI_AUDIO_INPUT_STATE_IDLE:
        event = AI_AUDIO_INPUT_EVT_NONE;
        break;
    case AI_AUDIO_INPUT_STATE_DETECTING:
        if (AI_AUDIO_INPUT_STATE_GET_VALID_DATA == last_state) {
            event = AI_AUDIO_INPUT_EVT_GET_VALID_VOICE_STOP;
        } else {
            event = AI_AUDIO_INPUT_EVT_NONE;
        }
        break;
    case AI_AUDIO_INPUT_STATE_GET_VALID_DATA:
        if (AI_AUDIO_INPUT_STATE_GET_VALID_DATA == last_state) {
            event = AI_AUDIO_INPUT_EVT_NONE;
        } else {
            event = AI_AUDIO_INPUT_EVT_GET_VALID_VOICE_START;
        }
        break;
    case AI_AUDIO_INPUT_STATE_ASR_WAKEUP_WORD:
        if (AI_AUDIO_INPUT_STATE_ASR_WAKEUP_WORD == last_state) {
            event = AI_AUDIO_INPUT_EVT_NONE;
        } else {
            event = AI_AUDIO_INPUT_EVT_ASR_WAKEUP_WORD;
        }
        break;
    }
    return event;
}

static void __ai_audio_get_input_frame(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data,
                                       uint32_t len)
{
#if defined(ENABLE_AEC) && (ENABLE_AEC == 1)

#else
    if (true == ai_audio_player_is_playing()) {
        tkl_vad_stop();
        return;
    } else {
        tkl_vad_start();
    }
#endif

    if (true == sg_audio_input.is_enable_get_valid_data) {
        __ai_audio_detect_valid_data_feed(sg_audio_input.method, (uint8_t *)data, len);
    }

    tal_mutex_lock(sg_audio_input.rb_mutex);
    tuya_ring_buff_write(sg_audio_input.ringbuff_hdl, data, len);
    tal_mutex_unlock(sg_audio_input.rb_mutex);

    return;
}

static void __ai_audio_handle_frame_task(void *arg)
{
    uint32_t rb_used_sz = 0;
    AI_AUDIO_INPUT_EVENT_E event = AI_AUDIO_INPUT_EVT_NONE;
    AI_AUDIO_INPUT_STATE_E last_state = AI_AUDIO_INPUT_STATE_IDLE;

    while (1) {
        rb_used_sz = tuya_ring_buff_used_size_get(sg_audio_input.ringbuff_hdl);
        if (0 == rb_used_sz) {
            tkl_system_sleep(10);
            continue;
        }

        last_state = sg_audio_input.state;
        if (true == sg_audio_input.is_enable_get_valid_data) {
            sg_audio_input.state = __ai_audio_input_get_new_state(sg_audio_input.method);
        } else {
            sg_audio_input.state = AI_AUDIO_INPUT_STATE_DETECTING;
        }

        event = __ai_audio_input_get_event(sg_audio_input.state, last_state);

        // get asr wakeup stop event
        if (AI_AUDIO_INPUT_EVT_NONE == event && true == sg_audio_input.asr.is_need_inform_wakeup_stop) {
            event = AI_AUDIO_INPUT_EVT_ASR_WAKEUP_STOP;
            sg_audio_input.asr.is_need_inform_wakeup_stop = false;
        }

        if ((event != AI_AUDIO_INPUT_EVT_NONE) && sg_audio_input_inform_cb) {
            sg_audio_input_inform_cb(event, NULL);
        }

        tal_system_sleep(10);
    }
}

static OPERATE_RET __ai_audio_input_hardware_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TDL_AUDIO_HANDLE_T audio_hdl = NULL;

    app_audio_driver_init(AUDIO_DRIVER_NAME);
    TUYA_CALL_ERR_RETURN(tdl_audio_find(AUDIO_DRIVER_NAME, &audio_hdl));
    TUYA_CALL_ERR_RETURN(tdl_audio_open(audio_hdl, __ai_audio_get_input_frame));

    PR_DEBUG("__ai_audio_input_hardware_init success");

    return OPRT_OK;
}

static OPERATE_RET __ai_audio_input_set_method(AI_AUDIO_INPUT_VALID_METHOD_E method)
{
    switch (method) {
    case AI_AUDIO_INPUT_VALID_METHOD_VAD:
        __ai_audio_vad_init();
        break;
    case AI_AUDIO_INPUT_VALID_METHOD_ASR:
        __ai_audio_vad_init();
        __ai_audio_asr_init();
        break;
    case AI_AUDIO_INPUT_VALID_METHOD_MANUAL:
        // do nothing
        break;
    default:
        PR_ERR("ai audio input not support method:%d", method);
        return OPRT_NOT_SUPPORTED;
    }

    sg_audio_input.method = method;

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

    TUYA_CALL_ERR_RETURN(tuya_ring_buff_create(AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_INPUT_RB_TIME_MS),
                                               OVERFLOW_PSRAM_STOP_TYPE, &sg_audio_input.ringbuff_hdl));
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_audio_input.rb_mutex));

    TUYA_CALL_ERR_RETURN(__ai_audio_input_set_method(cfg->get_valid_data_method));

    TUYA_CALL_ERR_RETURN(__ai_audio_input_hardware_init());

    sg_audio_input_inform_cb = cb;

    TUYA_CALL_ERR_RETURN(tkl_thread_create_in_psram(&sg_ai_audio_input_thrd_hdl, "audio_input", 1024 * 4, THREAD_PRIO_1,
                                                    __ai_audio_handle_frame_task, NULL));

    return OPRT_OK;
}

/**
 * @brief Enables or disables the wakeup functionality of the audio input system.
 * @param is_enable Boolean flag indicating whether to enable (true) or disable (false) the wakeup functionality.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_enable_get_valid_data(bool is_enable)
{
    if (is_enable == sg_audio_input.is_enable_get_valid_data) {
        return OPRT_OK;
    }

    if (AI_AUDIO_INPUT_VALID_METHOD_VAD == sg_audio_input.method ||
        AI_AUDIO_INPUT_VALID_METHOD_ASR == sg_audio_input.method) {
        if (true == is_enable) {
            tkl_vad_start();
        } else {
            tkl_vad_stop();
            __ai_audio_input_rb_reset();
        }
    }

    sg_audio_input.is_enable_get_valid_data = is_enable;

    PR_NOTICE("input enable/disable :%d get valid audio data", is_enable);

    return OPRT_OK;
}

/**
 * @brief Manually triggers the wakeup functionality of the audio input system.
 * @param is_open Boolean flag indicating whether to manually trigger (true) or reset (false) the wakeup state.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_manual_open_get_valid_data(bool is_open)
{
    if (false == sg_audio_input.is_enable_get_valid_data) {
        PR_ERR("input is not allowed get valid data, please enable it first");
        return OPRT_COM_ERROR;
    }

    if (AI_AUDIO_INPUT_VALID_METHOD_MANUAL != sg_audio_input.method) {
        PR_ERR("get valid data method:%d is not support this api", sg_audio_input.method);
        return OPRT_NOT_SUPPORTED;
    }

    sg_audio_input.state = is_open ? AI_AUDIO_INPUT_STATE_GET_VALID_DATA : AI_AUDIO_INPUT_STATE_DETECTING;

    return OPRT_OK;
}

OPERATE_RET ai_audio_input_stop_asr_awake(void)
{
    AI_AUDIO_INPUT_EVENT_E event = 0;

    if (false == sg_audio_input.is_enable_get_valid_data) {
        PR_ERR("input is not allowed get valid data, please enable it first");
        return OPRT_COM_ERROR;
    }

    if (AI_AUDIO_INPUT_VALID_METHOD_ASR != sg_audio_input.method) {
        PR_ERR("get valid data method:%d is not support this api", sg_audio_input.method);
        return OPRT_NOT_SUPPORTED;
    }

    if (tal_sw_timer_is_running(sg_audio_input.asr.wakeup_timer_id)) {
        tal_sw_timer_stop(sg_audio_input.asr.wakeup_timer_id);
    }

    sg_audio_input.asr.is_wakeup = false;
    sg_audio_input.asr.is_need_inform_wakeup_stop = true;

    PR_NOTICE("ai audio needs to be awakened again by the wake-up word");

    return OPRT_OK;
}

OPERATE_RET ai_audio_input_restart_asr_awake_timer(void)
{
    if (false == sg_audio_input.is_enable_get_valid_data) {
        PR_ERR("input is not allowed get valid data, please enable it first");
        return OPRT_COM_ERROR;
    }

    if (AI_AUDIO_INPUT_VALID_METHOD_ASR != sg_audio_input.method) {
        PR_ERR("get valid data method:%d is not support this api", sg_audio_input.method);
        return OPRT_NOT_SUPPORTED;
    }

    if (false == sg_audio_input.asr.is_wakeup) {
        PR_ERR("asr wakeup is already timeout");
        return OPRT_COM_ERROR;
    }

    tal_sw_timer_start(sg_audio_input.asr.wakeup_timer_id, ASR_WAKEUP_TIMEOUT_MS, TAL_TIMER_ONCE);

    return OPRT_OK;
}

uint32_t ai_audio_get_input_data(uint8_t *buff, uint32_t buff_len)
{
    uint32_t rb_used_size = 0;
    uint32_t read_len = 0;

    if (NULL == buff || 0 == buff_len) {
        return 0;
    }

    tal_mutex_lock(sg_audio_input.rb_mutex);
    rb_used_size = tuya_ring_buff_used_size_get(sg_audio_input.ringbuff_hdl);
    read_len = (buff_len <= rb_used_size) ? buff_len : rb_used_size;
    tuya_ring_buff_read(sg_audio_input.ringbuff_hdl, buff, read_len);
    tal_mutex_unlock(sg_audio_input.rb_mutex);

    return read_len;
}

uint32_t ai_audio_get_input_data_size(void)
{
    uint32_t rb_used_size = 0;

    tal_mutex_lock(sg_audio_input.rb_mutex);
    rb_used_size = tuya_ring_buff_used_size_get(sg_audio_input.ringbuff_hdl);
    tal_mutex_unlock(sg_audio_input.rb_mutex);

    return rb_used_size;
}

void ai_audio_discard_input_data(uint32_t discard_size)
{
    tal_mutex_lock(sg_audio_input.rb_mutex);
    tuya_ring_buff_discard(sg_audio_input.ringbuff_hdl, discard_size);
    tal_mutex_unlock(sg_audio_input.rb_mutex);
}