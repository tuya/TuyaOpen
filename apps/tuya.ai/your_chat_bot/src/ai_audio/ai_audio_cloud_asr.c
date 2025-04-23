/**
 * @file ai_audio_cloud_asr.c
 * @brief Implementation of the audio cloud ASR module, which handles audio recording, buffering, and uploading.
 *
 * This module initializes and manages the audio recording process, including setting up buffers, timers, and threads.
 * It also provides functions to write audio data, reset the buffer, post new states, and retrieve the current state.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tkl_thread.h"
#include "tkl_memory.h"
#include "tkl_audio.h"

#include "tal_api.h"
#include "tuya_ringbuf.h"

#include "ai_audio.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define AI_AUDIO_RB_TIME_MS (10 * 1000)
#define AI_AUDIO_UPLOAD_TIME_MS (100)
#define AI_AUDIO_WAIT_ASR_TM_MS (10 * 1000)

#define AI_CLOUD_ASR_STAT_CHANGE(new_stat)                                                                             \
    do {                                                                                                               \
        PR_DEBUG("ai cloud asr stat changed: %d->%d", sg_ai_cloud_asr.state, new_stat);                                \
        sg_ai_cloud_asr.state = new_stat;                                                                              \
    } while (0)

/***********************************************************
***********************typedef define***********************
***********************************************************/
// clang-format off
typedef struct {
    TUYA_RINGBUFF_T      rb_hdl;
    MUTEX_HANDLE         mutex;
    THREAD_HANDLE        thrd_hdl;
    TIMER_ID             asr_timer_id;
    bool                 is_enable_interrupt;

    AI_CLOUD_ASR_STATE_E state;
    QUEUE_HANDLE         stat_queue;

    uint8_t              *upload_buffer;
} AI_AUDIO_CLOUD_ASR_T;
// clang-format on
/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_AUDIO_CLOUD_ASR_T sg_ai_cloud_asr = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __ai_audio_cloud_asr_post_state(AI_CLOUD_ASR_STATE_E stat)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_ai_cloud_asr.stat_queue) {
        return OPRT_COM_ERROR;
    }

    rt = tal_queue_post(sg_ai_cloud_asr.stat_queue, &stat, 0);

    return rt;
}

// wait cloud asr response timeout
static void __ai_audio_wait_cloud_asr_tm_cb(TIMER_ID timer_id, void *arg)
{
    PR_ERR("wait asr timeout");
    tal_mutex_lock(sg_ai_cloud_asr.mutex);
    __ai_audio_cloud_asr_post_state(AI_CLOUD_ASR_STATE_IDLE);
    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    return;
}

static void __ai_audio_cloud_asr_task(void *arg)
{
    OPERATE_RET rt = OPRT_OK;
    AI_CLOUD_ASR_STATE_E stat = AI_CLOUD_ASR_STATE_IDLE;
    uint32_t next_timeout = 100;
    static uint8_t is_upload_first_frame = 0;

    sg_ai_cloud_asr.state = AI_CLOUD_ASR_STATE_IDLE;

    for (;;) {
        rt = tal_queue_fetch(sg_ai_cloud_asr.stat_queue, &stat, next_timeout);
        if (sg_ai_cloud_asr.state != stat && OPRT_OK == rt) {
            AI_CLOUD_ASR_STAT_CHANGE(stat);
        }
        next_timeout = (sg_ai_cloud_asr.state == AI_CLOUD_ASR_STATE_UPLOADING) ? 30 : 100;

        tal_mutex_lock(sg_ai_cloud_asr.mutex);

        switch (sg_ai_cloud_asr.state) {
        case AI_CLOUD_ASR_STATE_IDLE:
            if (tal_sw_timer_is_running(sg_ai_cloud_asr.asr_timer_id)) {
                tal_sw_timer_stop(sg_ai_cloud_asr.asr_timer_id);
            }
            break;
        case AI_CLOUD_ASR_STATE_UPLOAD_START: {
            TUYA_CALL_ERR_LOG(ai_audio_agent_upload_start(sg_ai_cloud_asr.is_enable_interrupt));
            if (OPRT_OK == rt) {
                is_upload_first_frame = 1;
                __ai_audio_cloud_asr_post_state(AI_CLOUD_ASR_STATE_UPLOADING);
            } else {
                PR_NOTICE("upload start fail");
                __ai_audio_cloud_asr_post_state(AI_CLOUD_ASR_STATE_IDLE);
            }
        } break;
        case AI_CLOUD_ASR_STATE_UPLOADING: {
            uint32_t upload_size = AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_UPLOAD_TIME_MS);
            uint32_t rb_used_size = tuya_ring_buff_used_size_get(sg_ai_cloud_asr.rb_hdl);
            if (upload_size > rb_used_size) {
                break;
            }

            PR_DEBUG("upload_size: %d, rb_used_size: %d", upload_size, rb_used_size);
            tuya_ring_buff_read(sg_ai_cloud_asr.rb_hdl, sg_ai_cloud_asr.upload_buffer, upload_size);
            TUYA_CALL_ERR_LOG(
                ai_audio_agent_upload_data(is_upload_first_frame, sg_ai_cloud_asr.upload_buffer, upload_size));
            is_upload_first_frame = 0;
        } break;
        case AI_CLOUD_ASR_STATE_UPLOAD_STOP: {
            // upload all data
            uint32_t rb_used_size = tuya_ring_buff_used_size_get(sg_ai_cloud_asr.rb_hdl);
            while (rb_used_size) {
                uint32_t upload_size = AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_UPLOAD_TIME_MS);
                if (upload_size > rb_used_size) {
                    upload_size = rb_used_size;
                }

                tuya_ring_buff_read(sg_ai_cloud_asr.rb_hdl, sg_ai_cloud_asr.upload_buffer, upload_size);
                TUYA_CALL_ERR_LOG(ai_audio_agent_upload_data(0, sg_ai_cloud_asr.upload_buffer, upload_size));

                rb_used_size = tuya_ring_buff_used_size_get(sg_ai_cloud_asr.rb_hdl);
            }

            ai_audio_agent_upload_stop();
            tal_sw_timer_start(sg_ai_cloud_asr.asr_timer_id, AI_AUDIO_WAIT_ASR_TM_MS, TAL_TIMER_ONCE);
            __ai_audio_cloud_asr_post_state(AI_CLOUD_ASR_STATE_WAIT_ASR);
        } break;
        case AI_CLOUD_ASR_STATE_WAIT_ASR: {
            // wait cloud asr response, nothing to do
        } break;
        case AI_CLOUD_ASR_STATE_UPLOAD_INTERUPT: {
            PR_ERR("upload interrupt");
            ai_audio_agent_upload_intrrupt();
            if (tal_sw_timer_is_running(sg_ai_cloud_asr.asr_timer_id)) {
                tal_sw_timer_stop(sg_ai_cloud_asr.asr_timer_id);
            }
            AI_CLOUD_ASR_STAT_CHANGE(AI_CLOUD_ASR_STATE_IDLE);
        } break;
        default:
            break;
        }

        tal_mutex_unlock(sg_ai_cloud_asr.mutex);
    }
}

/**
 * @brief Initializes the audio cloud ASR module.
 * @param is_enable_interrupt Boolean flag indicating whether interrupts are enabled.
 * @return OPERATE_RET - OPRT_OK if initialization is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_init(bool is_enable_interrupt)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("%s start", __func__);

    memset(&sg_ai_cloud_asr, 0, sizeof(AI_AUDIO_CLOUD_ASR_T));

    sg_ai_cloud_asr.is_enable_interrupt = is_enable_interrupt;

    // upload buffer init
    sg_ai_cloud_asr.upload_buffer =
        (uint8_t *)tkl_system_psram_malloc(AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_UPLOAD_TIME_MS));
    TUYA_CHECK_NULL_GOTO(sg_ai_cloud_asr.upload_buffer, __ERR);

    // asr timer init
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__ai_audio_wait_cloud_asr_tm_cb, NULL, &sg_ai_cloud_asr.asr_timer_id),
                       __ERR);

    // create ring buffer
    uint32_t recorder_rb_size = AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_RB_TIME_MS);
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(recorder_rb_size, OVERFLOW_PSRAM_STOP_TYPE, &sg_ai_cloud_asr.rb_hdl),
                       __ERR);

    // create stat queue
    TUYA_CALL_ERR_GOTO(tal_queue_create_init(&sg_ai_cloud_asr.stat_queue, sizeof(AI_CLOUD_ASR_STATE_E), 10), __ERR);

    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_ai_cloud_asr.mutex), __ERR);
    TUYA_CALL_ERR_GOTO(tkl_thread_create_in_psram(&sg_ai_cloud_asr.thrd_hdl, "audio_cloud_asr", 1024*8, THREAD_PRIO_2,
                                                  __ai_audio_cloud_asr_task, NULL),
                       __ERR);

    PR_DEBUG("%s success", __func__);

    return OPRT_OK;

__ERR:
    if (sg_ai_cloud_asr.asr_timer_id) {
        tal_sw_timer_delete(sg_ai_cloud_asr.asr_timer_id);
        sg_ai_cloud_asr.asr_timer_id = NULL;
    }

    if (sg_ai_cloud_asr.upload_buffer) {
        tkl_system_psram_free(sg_ai_cloud_asr.upload_buffer);
        sg_ai_cloud_asr.upload_buffer = NULL;
    }

    if (sg_ai_cloud_asr.mutex) {
        tal_mutex_release(sg_ai_cloud_asr.mutex);
        sg_ai_cloud_asr.mutex = NULL;
    }

    if (sg_ai_cloud_asr.rb_hdl) {
        tuya_ring_buff_free(sg_ai_cloud_asr.rb_hdl);
        sg_ai_cloud_asr.rb_hdl = NULL;
    }

    PR_ERR("%s failed", __func__);

    return rt;
}

/**
 * @brief Writes data to the audio recorder's ring buffer.
 * @param data Pointer to the data to be written.
 * @param len Length of the data to be written.
 * @return OPERATE_RET - OPRT_OK if the write operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_input(const void *data, uint32_t len)
{
    OPERATE_RET rt;

    if (NULL == sg_ai_cloud_asr.rb_hdl || NULL == sg_ai_cloud_asr.mutex) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_ai_cloud_asr.mutex);
    tuya_ring_buff_write(sg_ai_cloud_asr.rb_hdl, data, len);
    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    return OPRT_OK;
}

/**
 * @brief Writes VAD (Voice Activity Detection) data to the audio recorder's ring buffer and discards excess data if necessary.
 * @param data Pointer to the VAD data to be written.
 * @param len Length of the VAD data to be written.
 * @return OPERATE_RET - OPRT_OK if the write operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_vad_input(const void *data, uint32_t len)
{
    uint32_t rb_used_size = tuya_ring_buff_used_size_get(sg_ai_cloud_asr.rb_hdl);

    // if (sg_ai_cloud_asr.state != AI_CLOUD_ASR_STATE_IDLE ||\
    //     sg_ai_cloud_asr.state != AI_CLOUD_ASR_STATE_WAIT_ASR) {
    //     // PR_DEBUG("state:%d is not idle please not use this api", sg_ai_cloud_asr.state);
    //     return OPRT_OK;
    // }

    tal_mutex_lock(sg_ai_cloud_asr.mutex);

    tuya_ring_buff_write(sg_ai_cloud_asr.rb_hdl, data, len);

    if (rb_used_size > AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_VAD_ACITVE_TM_MS)) {
        uint32_t discard_size = rb_used_size - AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_VAD_ACITVE_TM_MS);
        tuya_ring_buff_discard(sg_ai_cloud_asr.rb_hdl, discard_size);
    }

    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    return OPRT_OK;
}

/**
 * @brief Starts the audio cloud ASR process.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the start operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_start(void)
{
    if (NULL == sg_ai_cloud_asr.rb_hdl || NULL == sg_ai_cloud_asr.mutex) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_ai_cloud_asr.mutex);

    if (sg_ai_cloud_asr.state != AI_CLOUD_ASR_STATE_IDLE) {
        __ai_audio_cloud_asr_post_state(AI_CLOUD_ASR_STATE_UPLOAD_INTERUPT);
    }

    if (sg_ai_cloud_asr.state == AI_CLOUD_ASR_STATE_IDLE) {
        __ai_audio_cloud_asr_post_state(AI_CLOUD_ASR_STATE_UPLOAD_START);
    }

    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    return OPRT_OK;
}

/**
 * @brief Stops the audio cloud ASR process.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the stop operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_stop(void)
{
    if (NULL == sg_ai_cloud_asr.rb_hdl || NULL == sg_ai_cloud_asr.mutex) {
        return OPRT_COM_ERROR;
    }

    if (sg_ai_cloud_asr.state != AI_CLOUD_ASR_STATE_UPLOADING &&
        sg_ai_cloud_asr.state != AI_CLOUD_ASR_STATE_UPLOAD_START) {
        return OPRT_OK;
    }

    tal_mutex_lock(sg_ai_cloud_asr.mutex);

    __ai_audio_cloud_asr_post_state(AI_CLOUD_ASR_STATE_UPLOAD_STOP);

    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    return OPRT_OK;
}

/**
 * @brief Stops waiting for the cloud ASR response and transitions the state to idle.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_stop_wait_asr(void)
{
    if (NULL == sg_ai_cloud_asr.rb_hdl || NULL == sg_ai_cloud_asr.mutex) {
        return OPRT_COM_ERROR;
    }

    if (sg_ai_cloud_asr.state != AI_CLOUD_ASR_STATE_WAIT_ASR) {
        PR_NOTICE("the state is not wait cloud asr");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_ai_cloud_asr.mutex);
    __ai_audio_cloud_asr_post_state(AI_CLOUD_ASR_STATE_IDLE);
    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    return OPRT_OK;
}

/**
 * @brief Resets the audio recorder's ring buffer if it is not empty.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the reset operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_rb_reset(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t rb_used_size = 0;

    if (NULL == sg_ai_cloud_asr.rb_hdl || NULL == sg_ai_cloud_asr.mutex) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_ai_cloud_asr.mutex);
    rb_used_size = tuya_ring_buff_used_size_get(sg_ai_cloud_asr.rb_hdl);
    if (rb_used_size) {
        tuya_ring_buff_reset(sg_ai_cloud_asr.rb_hdl);
    }
    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    return rt;
}

/**
 * @brief Transitions audio cloud ASR process to the idle state, interrupting any ongoing uploads.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_idle(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_ai_cloud_asr.rb_hdl || NULL == sg_ai_cloud_asr.mutex) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_ai_cloud_asr.mutex);
    if (sg_ai_cloud_asr.state == AI_CLOUD_ASR_STATE_UPLOADING ||
        sg_ai_cloud_asr.state == AI_CLOUD_ASR_STATE_UPLOAD_START) {
        __ai_audio_cloud_asr_post_state(AI_CLOUD_ASR_STATE_UPLOAD_INTERUPT);
    }

    if (sg_ai_cloud_asr.state != AI_CLOUD_ASR_STATE_IDLE) {
        __ai_audio_cloud_asr_post_state(AI_CLOUD_ASR_STATE_IDLE);
    }
    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    return rt;
}

/**
 * @brief Get the current state of the audio could asr process.
 * @param None
 * @return AI_CLOUD_ASR_STATE_E - The current state of the audio cloud asr process.
 */
AI_CLOUD_ASR_STATE_E ai_audio_cloud_asr_get_state(void)
{
    return sg_ai_cloud_asr.state;
}

/**
 * @brief Enables or disables interrupts for the audio cloud ASR module.
 * @param is_enable Boolean value indicating whether to enable (true) or disable (false) interrupts.
 * @return OPERATE_RET - OPRT_OK if the operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_enable_intrrupt(bool is_enable)
{
    sg_ai_cloud_asr.is_enable_interrupt = is_enable;

    return OPRT_OK;
}

OPERATE_RET ai_audio_cloud_is_wait_asr(void)
{
    if(AI_CLOUD_ASR_STATE_WAIT_ASR == sg_ai_cloud_asr.state) {
        return true;
    }else {
        return false;
    }
}