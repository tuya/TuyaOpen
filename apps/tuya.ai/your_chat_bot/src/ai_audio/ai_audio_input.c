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

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t is_init;
    uint8_t is_enable;
    uint8_t is_enable_wakeup;

    AI_AUDIO_INPUT_STATE_E state;
    AI_AUDIO_INPUT_WAKEUP_TP_E wakeup_tp;

    TUYA_RINGBUFF_T ringbuff_hdl;
    MUTEX_HANDLE rb_mutex;

} AI_AUDIO_INPUT_INFO_T;

/***********************************************************
***********************const declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_AUDIO_INOUT_INFORM_CB sg_audio_input_inform_cb = NULL;
static THREAD_HANDLE sg_ai_audio_input_thrd_hdl = NULL;
static AI_AUDIO_INPUT_INFO_T sg_audio_input;
/***********************************************************
***********************function define**********************
***********************************************************/
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

static void __ai_audio_wakeup_feed(AI_AUDIO_INPUT_WAKEUP_TP_E wakeup_tp, uint8_t *data, uint32_t len)
{
    if (AI_AUDIO_INPUT_WAKEUP_VAD == wakeup_tp) {
        tkl_vad_feed(data, len);
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

AI_AUDIO_INPUT_STATE_E __ai_audio_input_get_new_state(bool is_enable_wakeup, AI_AUDIO_INPUT_WAKEUP_TP_E wakeup_tp)
{
    AI_AUDIO_INPUT_STATE_E state = AI_AUDIO_INPUT_STATE_IDLE;

    if (false == is_enable_wakeup) {
        return AI_AUDIO_INPUT_STATE_DETECTING;
    }

    switch (wakeup_tp) {
    case AI_AUDIO_INPUT_WAKEUP_MANUAL:
        // the state is manually controlled from the outside.
        break;
    case AI_AUDIO_INPUT_WAKEUP_VAD:
        if (TKL_VAD_STATUS_SPEECH == tkl_vad_get_status()) {
            state = AI_AUDIO_INPUT_STATE_AWAKE;
        } else {
            state = AI_AUDIO_INPUT_STATE_DETECTING;
        }
        break;
    default:
        PR_ERR("wakeup tp :%d not support", wakeup_tp);
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
        if (AI_AUDIO_INPUT_STATE_AWAKE == last_state) {
            event = AI_AUDIO_INPUT_EVT_AWAKE_STOP;
        } else {
            event = AI_AUDIO_INPUT_EVT_NONE;
        }
        break;
    case AI_AUDIO_INPUT_STATE_AWAKE:
        if (AI_AUDIO_INPUT_STATE_AWAKE == last_state) {
            event = AI_AUDIO_INPUT_EVT_NONE;
        } else {
            event = AI_AUDIO_INPUT_EVT_WAKEUP;
        }
        break;
    }
    return event;
}

static int __ai_audio_get_input_frame(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data,
                                      uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    if (false == sg_audio_input.is_enable) {
        return 0;
    }

#if defined(ENABLE_AEC) && (ENABLE_AEC == 1)

#else
    if (true == ai_audio_player_is_playing()) {
        tkl_vad_stop();
        return 0;
    } else {
        tkl_vad_start();
    }
#endif

    if (true == sg_audio_input.is_enable_wakeup) {
        __ai_audio_wakeup_feed(sg_audio_input.wakeup_tp, (uint8_t *)data, len);
    }

    tal_mutex_lock(sg_audio_input.rb_mutex);
    tuya_ring_buff_write(sg_audio_input.ringbuff_hdl, data, len);
    tal_mutex_unlock(sg_audio_input.rb_mutex);

    return len;
}

static void __ai_audio_handle_frame_task(void *arg)
{
    uint32_t rb_used_sz = 0;
    uint8_t *p_buff = NULL;
    AI_AUDIO_INPUT_EVENT_E event = AI_AUDIO_INPUT_EVT_NONE;
    AI_AUDIO_INPUT_STATE_E last_state = AI_AUDIO_INPUT_STATE_IDLE;

    while (1) {
        rb_used_sz = tuya_ring_buff_used_size_get(sg_audio_input.ringbuff_hdl);
        if (0 == rb_used_sz) {
            tkl_system_sleep(10);
            continue;
        }

        last_state = sg_audio_input.state;
        sg_audio_input.state =
            __ai_audio_input_get_new_state(sg_audio_input.is_enable_wakeup, sg_audio_input.wakeup_tp);
        event = __ai_audio_input_get_event(sg_audio_input.state, last_state);

        // PR_DEBUG("last state:%d state :%d event:%d, wakeup_word:%d \r\n",last_state, sg_audio_input.state, event,
        // wakeup_word);

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

    __ai_audio_vad_init();

    PR_DEBUG("__ai_audio_input_hardware_init success");

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

    sg_audio_input.is_enable = true;

    TUYA_CALL_ERR_RETURN(tuya_ring_buff_create(AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_INPUT_RB_TIME_MS),
                                               OVERFLOW_PSRAM_STOP_TYPE, &sg_audio_input.ringbuff_hdl));
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_audio_input.rb_mutex));

    TUYA_CALL_ERR_RETURN(__ai_audio_input_hardware_init());

    sg_audio_input.wakeup_tp = cfg->wakeup_tp;

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
OPERATE_RET ai_audio_input_enable_wakeup(bool is_enable)
{
    if (is_enable == sg_audio_input.is_enable_wakeup) {
        // PR_NOTICE("input already enable/disable :%d wakeup", is_enable);
        return OPRT_OK;
    }

    if (AI_AUDIO_INPUT_WAKEUP_VAD == sg_audio_input.wakeup_tp) {
        if (true == is_enable) {
            tkl_vad_start();
        } else {
            tkl_vad_stop();
            __ai_audio_input_rb_reset();
        }
    }

    sg_audio_input.is_enable_wakeup = is_enable;

    PR_NOTICE("input enable/disable :%d wakeup", is_enable);

    return OPRT_OK;
}

/**
 * @brief Manually triggers the wakeup functionality of the audio input system.
 * @param is_wakeup Boolean flag indicating whether to manually trigger (true) or reset (false) the wakeup state.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_input_manual_set_wakeup(bool is_wakeup)
{
    if (false == sg_audio_input.is_enable_wakeup) {
        PR_NOTICE("input is not allowed be woken up");
        return OPRT_COM_ERROR;
    }

    if (AI_AUDIO_INPUT_WAKEUP_MANUAL != sg_audio_input.wakeup_tp) {
        PR_NOTICE("wakeup type:%d is not support this api", sg_audio_input.wakeup_tp);
        return OPRT_COM_ERROR;
    }

    sg_audio_input.state = is_wakeup ? AI_AUDIO_INPUT_STATE_AWAKE : AI_AUDIO_INPUT_STATE_DETECTING;

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