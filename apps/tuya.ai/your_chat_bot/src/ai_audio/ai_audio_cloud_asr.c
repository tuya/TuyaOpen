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
#include "tkl_queue.h"
#include "tal_api.h"
#include "tuya_ringbuf.h"

#include "ai_audio.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define AI_AUDIO_UPLOAD_VAD_TM_MS (300 + 300)

#define AI_AUDIO_RB_TIME_MS          (10 * 1000)
#define AI_AUDIO_UPLOAD_MIN_TIME_MS  (100)
#define AI_AUDIO_UPLOAD_BUFF_TIME_MS (100)
#define AI_AUDIO_WAIT_ASR_TM_MS      (10 * 1000)

#define AI_CLOUD_ASR_EVENT(event)                                                                                      \
    do {                                                                                                               \
        PR_DEBUG("ai cloud asr event: %d", event);                                                                     \
    } while (0)

#define AI_CLOUD_ASR_STAT_CHANGE(last_stat, new_stat)                                                                  \
    do {                                                                                                               \
        if (last_stat != new_stat) {                                                                                   \
            PR_DEBUG("ai cloud asr stat changed: %d->%d", last_stat, new_stat);                                        \
        }                                                                                                              \
    } while (0)

/***********************************************************
***********************typedef define***********************
***********************************************************/
// clang-format off
typedef enum {
    AI_CLOUD_ASR_UPLOAD_STATE_START,
    AI_CLOUD_ASR_UPLOAD_STATE_UPLOADING,
    AI_CLOUD_ASR_UPLOAD_STATE_STOP,
} AI_CLOUD_ASR_UPLOAD_STATE_E;

typedef enum {
    AI_CLOUD_ASR_EVT_ENTER_IDLE,
    AI_CLOUD_ASR_EVT_UPDATE_VAD,
    AI_CLOUD_ASR_EVT_START,
    AI_CLOUD_ASR_EVT_UPLOADING,
    AI_CLOUD_ASR_EVT_STOP,
} AI_CLOUD_ASR_EVENT_E;

typedef struct {
    AI_CLOUD_ASR_EVENT_E event;
    bool                 is_force_interrupt;
}AI_CLOUD_ASR_MSG_T;

typedef struct {
    bool                        is_uploading;
    MUTEX_HANDLE                mutex;
    THREAD_HANDLE               thrd_hdl;
    QUEUE_HANDLE                queue;
    AI_CLOUD_ASR_STATE_E        state;
    TIMER_ID                    asr_timer_id;

    AI_CLOUD_ASR_UPLOAD_STATE_E upload_state;
    TIMER_ID                    upload_timer_id;
    uint8_t                    *upload_buffer;
    uint32_t                    upload_buffer_len;

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
// wait cloud asr response timeout
static void __ai_audio_wait_cloud_asr_tm_cb(TIMER_ID timer_id, void *arg)
{
    AI_CLOUD_ASR_MSG_T send_msg;
    PR_ERR("wait asr timeout");

    tal_mutex_lock(sg_ai_cloud_asr.mutex);

    send_msg.event = AI_CLOUD_ASR_EVT_ENTER_IDLE;
    send_msg.is_force_interrupt = false;
    tal_queue_post(sg_ai_cloud_asr.queue, &send_msg, 0);

    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    return;
}

static void __ai_audio_cloud_asr_task(void *arg)
{
    static AI_CLOUD_ASR_STATE_E last_state;
    AI_CLOUD_ASR_MSG_T msg;
    AI_CLOUD_ASR_MSG_T send_msg;
    OPERATE_RET rt = OPRT_OK;

    sg_ai_cloud_asr.state = AI_CLOUD_ASR_STATE_IDLE;

    for (;;) {
        rt = tal_queue_fetch(sg_ai_cloud_asr.queue, &msg, 20);
        if (OPRT_OK != rt) {
            // wait event timeout
            if (true == sg_ai_cloud_asr.is_uploading) {
                msg.event = AI_CLOUD_ASR_EVT_UPLOADING;
                msg.is_force_interrupt = false;
            } else {
                msg.event = AI_CLOUD_ASR_EVT_UPDATE_VAD;
                msg.is_force_interrupt = false;
            }
        } else {
            AI_CLOUD_ASR_EVENT(msg.event);
        }

        if (true == msg.is_force_interrupt) {
            ai_audio_agent_chat_intrrupt();
        }

        switch (msg.event) {
        case AI_CLOUD_ASR_EVT_ENTER_IDLE: {

            if (tal_sw_timer_is_running(sg_ai_cloud_asr.asr_timer_id)) {
                tal_sw_timer_stop(sg_ai_cloud_asr.asr_timer_id);
            }

            sg_ai_cloud_asr.state = AI_CLOUD_ASR_STATE_IDLE;

            send_msg.event = AI_CLOUD_ASR_EVT_UPDATE_VAD;
            send_msg.is_force_interrupt = false;
            tal_queue_post(sg_ai_cloud_asr.queue, &send_msg, 0);
        } break;
        case AI_CLOUD_ASR_EVT_UPDATE_VAD: {
            uint32_t discard_size = 0;

            // Only retain the data within the time period of AI_AUDIO_VAD_ACITVE_TM_MS as VAD data,
            // and send it together with the speech data to the cloud for ASR.
            if (ai_audio_get_input_data_size() > AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_UPLOAD_VAD_TM_MS)) {
                discard_size = ai_audio_get_input_data_size() - AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_UPLOAD_VAD_TM_MS);
                ai_audio_discard_input_data(discard_size);
            }
        } break;
        case AI_CLOUD_ASR_EVT_START: {
            OPERATE_RET rt = OPRT_OK;
            AI_CLOUD_ASR_MSG_T send_msg;

            if (tal_sw_timer_is_running(sg_ai_cloud_asr.asr_timer_id)) {
                tal_sw_timer_stop(sg_ai_cloud_asr.asr_timer_id);
            }

            rt = ai_audio_agent_upload_start(true);
            if (OPRT_OK == rt) {
                sg_ai_cloud_asr.state = AI_CLOUD_ASR_STATE_UPLOAD;
                send_msg.event = AI_CLOUD_ASR_EVT_UPLOADING;
                send_msg.is_force_interrupt = false;
                tal_queue_post(sg_ai_cloud_asr.queue, &send_msg, 0);
            } else {
                PR_NOTICE("upload start fail");
                send_msg.event = AI_CLOUD_ASR_EVT_ENTER_IDLE;
                send_msg.is_force_interrupt = false;
                tal_queue_post(sg_ai_cloud_asr.queue, &send_msg, 0);
            }
        } break;
        case AI_CLOUD_ASR_EVT_UPLOADING: {
            uint32_t upload_len = 0;
            uint32_t input_data_size = ai_audio_get_input_data_size();

            if (false == sg_ai_cloud_asr.is_uploading) {
                break;
            }

            if (input_data_size < AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_UPLOAD_MIN_TIME_MS)) {
                // wait receive data
                break;
            }

            upload_len = ai_audio_get_input_data(sg_ai_cloud_asr.upload_buffer, sg_ai_cloud_asr.upload_buffer_len);
            TUYA_CALL_ERR_LOG(ai_audio_agent_upload_data(sg_ai_cloud_asr.upload_buffer, upload_len));
        } break;
        case AI_CLOUD_ASR_EVT_STOP: {
            uint32_t upload_len = 0;
            uint32_t input_data_size = 0;

            if (false == sg_ai_cloud_asr.is_uploading) {
                break;
            }

            input_data_size = ai_audio_get_input_data_size();

            PR_NOTICE("AI_CLOUD_ASR_UPLOAD_STATE_STOP size:%d", input_data_size);

            while (input_data_size) {
                if (false == sg_ai_cloud_asr.is_uploading) {
                    break;
                }

                upload_len = ai_audio_get_input_data(sg_ai_cloud_asr.upload_buffer, sg_ai_cloud_asr.upload_buffer_len);
                if (0 == upload_len) {
                    break;
                }

                TUYA_CALL_ERR_LOG(ai_audio_agent_upload_data(sg_ai_cloud_asr.upload_buffer, upload_len));
                if (input_data_size <= upload_len) {
                    break;
                }

                input_data_size -= upload_len;
            }

            ai_audio_agent_upload_stop();

            tal_sw_timer_start(sg_ai_cloud_asr.asr_timer_id, AI_AUDIO_WAIT_ASR_TM_MS, TAL_TIMER_ONCE);
            sg_ai_cloud_asr.state = AI_CLOUD_ASR_STATE_WAIT_ASR;
            sg_ai_cloud_asr.is_uploading = false;
        } break;

            AI_CLOUD_ASR_STAT_CHANGE(last_state, sg_ai_cloud_asr.state);

            last_state = sg_ai_cloud_asr.state;
        }
    }
}

OPERATE_RET ai_audio_cloud_asr_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("%s start", __func__);

    memset(&sg_ai_cloud_asr, 0, sizeof(AI_AUDIO_CLOUD_ASR_T));

    // upload buffer init
    sg_ai_cloud_asr.upload_buffer_len = AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_UPLOAD_BUFF_TIME_MS);
    sg_ai_cloud_asr.upload_buffer = (uint8_t *)tkl_system_psram_malloc(sg_ai_cloud_asr.upload_buffer_len);
    TUYA_CHECK_NULL_GOTO(sg_ai_cloud_asr.upload_buffer, __ERR);

    TUYA_CALL_ERR_GOTO(tal_queue_create_init(&sg_ai_cloud_asr.queue, sizeof(AI_CLOUD_ASR_MSG_T), 8), __ERR);

    // wait asr timer init
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__ai_audio_wait_cloud_asr_tm_cb, NULL, &sg_ai_cloud_asr.asr_timer_id),
                       __ERR);

    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_ai_cloud_asr.mutex), __ERR);
    TUYA_CALL_ERR_GOTO(tkl_thread_create_in_psram(&sg_ai_cloud_asr.thrd_hdl, "audio_cloud_asr", 1024 * 4, THREAD_PRIO_1,
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

    if (sg_ai_cloud_asr.queue) {
        tal_queue_free(sg_ai_cloud_asr.queue);
        sg_ai_cloud_asr.queue = NULL;
    }

    PR_ERR("%s failed", __func__);

    return rt;
}

/**
 * @brief Starts the audio cloud ASR process.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the start operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_start(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_CLOUD_ASR_MSG_T send_msg;

    tal_mutex_lock(sg_ai_cloud_asr.mutex);

    if (true == sg_ai_cloud_asr.is_uploading) {
        PR_ERR("cloud_asr is uploading");
        tal_mutex_unlock(sg_ai_cloud_asr.mutex);
        return OPRT_COM_ERROR;
    }

    send_msg.is_force_interrupt = false;
    send_msg.event = AI_CLOUD_ASR_EVT_START;
    TUYA_CALL_ERR_LOG(tal_queue_post(sg_ai_cloud_asr.queue, &send_msg, 0));

    sg_ai_cloud_asr.is_uploading = true;

    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    PR_NOTICE("ai audio cloud asr start");

    return OPRT_OK;
}

/**
 * @brief Stops the audio cloud ASR process.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the stop operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_stop(void)
{
    AI_CLOUD_ASR_MSG_T send_msg;

    tal_mutex_lock(sg_ai_cloud_asr.mutex);

    if (false == sg_ai_cloud_asr.is_uploading) {
        PR_ERR("cloud_asr is not upload");
        tal_mutex_unlock(sg_ai_cloud_asr.mutex);
        return OPRT_COM_ERROR;
    }

    send_msg.event = AI_CLOUD_ASR_EVT_STOP;
    send_msg.is_force_interrupt = false;
    tal_queue_post(sg_ai_cloud_asr.queue, &send_msg, 0);

    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    PR_NOTICE("ai audio cloud asr stop");

    return OPRT_OK;
}

/**
 * @brief Stops waiting for the cloud ASR response and transitions the state to idle.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_stop_wait_asr(void)
{
    AI_CLOUD_ASR_MSG_T send_msg;

    tal_mutex_lock(sg_ai_cloud_asr.mutex);

    if (sg_ai_cloud_asr.state != AI_CLOUD_ASR_STATE_WAIT_ASR) {
        PR_NOTICE("the state is not wait cloud asr");
        tal_mutex_unlock(sg_ai_cloud_asr.mutex);
        return OPRT_COM_ERROR;
    }

    send_msg.event = AI_CLOUD_ASR_EVT_ENTER_IDLE;
    send_msg.is_force_interrupt = false;
    tal_queue_post(sg_ai_cloud_asr.queue, &send_msg, 0);

    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    return OPRT_OK;
}

/**
 * @brief Transitions audio cloud ASR process to the idle state, interrupting any ongoing uploads.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_set_idle(bool is_force)
{
    OPERATE_RET rt = OPRT_OK;
    AI_CLOUD_ASR_MSG_T send_msg;

    tal_mutex_lock(sg_ai_cloud_asr.mutex);

    if (true == is_force || sg_ai_cloud_asr.state != AI_CLOUD_ASR_STATE_IDLE) {
        send_msg.is_force_interrupt = true;
    } else {
        send_msg.is_force_interrupt = false;
    }

    send_msg.event = AI_CLOUD_ASR_EVT_ENTER_IDLE;
    tal_queue_post(sg_ai_cloud_asr.queue, &send_msg, 0);

    sg_ai_cloud_asr.is_uploading = false;

    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    PR_NOTICE("ai audio cloud asr set IDLE");

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

OPERATE_RET ai_audio_cloud_is_wait_asr(void)
{
    if (AI_CLOUD_ASR_STATE_WAIT_ASR == sg_ai_cloud_asr.state) {
        return true;
    } else {
        return false;
    }
}