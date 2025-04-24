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
#define GET_MIN_LEN(a, b) ((a) < (b) ? (a) : (b))

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
typedef enum {
    AI_CLOUD_ASR_UPLOAD_STATE_START,
    AI_CLOUD_ASR_UPLOAD_STATE_UPLOADING,
    AI_CLOUD_ASR_UPLOAD_STATE_STOP,
}AI_CLOUD_ASR_UPLOAD_STATE_E;

typedef struct {
    MUTEX_HANDLE                   mutex;
    THREAD_HANDLE                  thrd_hdl;
    AI_CLOUD_ASR_STATE_E           state;
    TIMER_ID                       asr_timer_id;

    AI_CLOUD_ASR_UPLOAD_STATE_E    upload_state;
    bool                           is_first_frame;
    TIMER_ID                       upload_timer_id;
    uint8_t                       *upload_buffer;
    uint32_t                       upload_buffer_len;
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
    PR_ERR("wait asr timeout");
    tal_mutex_lock(sg_ai_cloud_asr.mutex);
    sg_ai_cloud_asr.state = AI_CLOUD_ASR_STATE_IDLE;
    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    return;
}
static AI_CLOUD_ASR_STATE_E __ai_audio_cloud_asr_proc_upload(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_CLOUD_ASR_STATE_E cloud_state = AI_CLOUD_ASR_STATE_UPLOAD;

    switch(sg_ai_cloud_asr.upload_state) {
    case AI_CLOUD_ASR_UPLOAD_STATE_START:{
        rt = ai_audio_agent_upload_start(true);
        if (OPRT_OK == rt) {
            sg_ai_cloud_asr.upload_state = AI_CLOUD_ASR_UPLOAD_STATE_UPLOADING;
            sg_ai_cloud_asr.is_first_frame = true;
        } else {
            PR_NOTICE("upload start fail");
            cloud_state = AI_CLOUD_ASR_STATE_IDLE;
        }
    }
    break;
    case AI_CLOUD_ASR_UPLOAD_STATE_UPLOADING: {
        if (ai_audio_get_input_data_size() < sg_ai_cloud_asr.upload_buffer_len) {
            //wait receive data
            return true;
        }

        ai_audio_get_input_data(sg_ai_cloud_asr.upload_buffer, sg_ai_cloud_asr.upload_buffer_len);
        TUYA_CALL_ERR_LOG(ai_audio_agent_upload_data( sg_ai_cloud_asr.is_first_frame, \
                                                      sg_ai_cloud_asr.upload_buffer, \
                                                      sg_ai_cloud_asr.upload_buffer_len));
        sg_ai_cloud_asr.is_first_frame = false;
    }
    break;
    case AI_CLOUD_ASR_UPLOAD_STATE_STOP:{
        uint32_t upload_size = 0;
        uint32_t input_data_size = 0;

        input_data_size = ai_audio_get_input_data_size();

        while (input_data_size) {
            upload_size = GET_MIN_LEN(input_data_size, sg_ai_cloud_asr.upload_buffer_len);
            ai_audio_get_input_data(sg_ai_cloud_asr.upload_buffer, sg_ai_cloud_asr.upload_buffer_len);
            TUYA_CALL_ERR_LOG(ai_audio_agent_upload_data(0, sg_ai_cloud_asr.upload_buffer, upload_size));

            input_data_size -= upload_size;
        }

        ai_audio_agent_upload_stop();
        cloud_state = AI_CLOUD_ASR_STATE_WAIT_ASR;
    }
    break;
    }

    return cloud_state;
}

static void __ai_audio_cloud_asr_task(void *arg)
{
    OPERATE_RET rt = OPRT_OK;

    sg_ai_cloud_asr.state = AI_CLOUD_ASR_STATE_IDLE;

    for (;;) {
        tal_mutex_lock(sg_ai_cloud_asr.mutex);
        switch(sg_ai_cloud_asr.state) {
        case AI_CLOUD_ASR_STATE_IDLE:{
            uint32_t discard_size = 0;

            //Only retain the data within the time period of AI_AUDIO_VAD_ACITVE_TM_MS as VAD data, 
            //and send it together with the speech data to the cloud for ASR.
            if (ai_audio_get_input_data_size() > AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_VAD_ACITVE_TM_MS)) {
                discard_size = ai_audio_get_input_data_size() - AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_VAD_ACITVE_TM_MS);
                ai_audio_discard_input_data(discard_size);
            }

            if (tal_sw_timer_is_running(sg_ai_cloud_asr.asr_timer_id)) {
                tal_sw_timer_stop(sg_ai_cloud_asr.asr_timer_id);
            }
        }
        break;
        case AI_CLOUD_ASR_STATE_UPLOAD:{
            sg_ai_cloud_asr.state = __ai_audio_cloud_asr_proc_upload();
        }
        break;
        case AI_CLOUD_ASR_STATE_WAIT_ASR:
            if (false == tal_sw_timer_is_running(sg_ai_cloud_asr.asr_timer_id)) {
                tal_sw_timer_start(sg_ai_cloud_asr.asr_timer_id, AI_AUDIO_WAIT_ASR_TM_MS, TAL_TIMER_ONCE);
            }   
        break;    
        }
        tal_mutex_unlock(sg_ai_cloud_asr.mutex);

        tal_system_sleep(30);
    }
}

OPERATE_RET ai_audio_cloud_asr_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("%s start", __func__);

    memset(&sg_ai_cloud_asr, 0, sizeof(AI_AUDIO_CLOUD_ASR_T));

    // upload buffer init
    sg_ai_cloud_asr.upload_buffer_len = AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_UPLOAD_TIME_MS);
    sg_ai_cloud_asr.upload_buffer = (uint8_t *)tkl_system_psram_malloc(sg_ai_cloud_asr.upload_buffer_len);
    TUYA_CHECK_NULL_GOTO(sg_ai_cloud_asr.upload_buffer, __ERR);

    // wait asr timer init
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__ai_audio_wait_cloud_asr_tm_cb, \
                                           NULL, \
                                           &sg_ai_cloud_asr.asr_timer_id), __ERR);

    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_ai_cloud_asr.mutex), __ERR);
    TUYA_CALL_ERR_GOTO(tkl_thread_create_in_psram(&sg_ai_cloud_asr.thrd_hdl, \
                                                  "audio_cloud_asr", \
                                                  1024*4, \
                                                  THREAD_PRIO_2,
                                                  __ai_audio_cloud_asr_task, \
                                                  NULL), __ERR);

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

    PR_ERR("%s failed", __func__);

    return rt;
}


OPERATE_RET ai_audio_cloud_asr_update_vad_data(void)
{
    uint32_t discard_size = 0;

    tal_mutex_lock(sg_ai_cloud_asr.mutex);
    if (ai_audio_get_input_data_size() > AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_VAD_ACITVE_TM_MS)) {
        discard_size = ai_audio_get_input_data_size() - AI_AUDIO_VOICE_FRAME_LEN_GET(AI_AUDIO_VAD_ACITVE_TM_MS);
        ai_audio_discard_input_data(discard_size);
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
    tal_mutex_lock(sg_ai_cloud_asr.mutex);

    if (sg_ai_cloud_asr.state != AI_CLOUD_ASR_STATE_IDLE) {
        ai_audio_agent_upload_intrrupt();
    }

    if (tal_sw_timer_is_running(sg_ai_cloud_asr.asr_timer_id)) {
        tal_sw_timer_stop(sg_ai_cloud_asr.asr_timer_id);
    }

    sg_ai_cloud_asr.state = AI_CLOUD_ASR_STATE_UPLOAD;
    sg_ai_cloud_asr.upload_state = AI_CLOUD_ASR_UPLOAD_STATE_START;

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
    tal_mutex_lock(sg_ai_cloud_asr.mutex);

    if (sg_ai_cloud_asr.state != AI_CLOUD_ASR_STATE_UPLOAD) {
        PR_ERR("cloud_asr state:%d is not upload", sg_ai_cloud_asr.state);
        tal_mutex_unlock(sg_ai_cloud_asr.mutex);
        return OPRT_COM_ERROR;
    }

    sg_ai_cloud_asr.upload_state = AI_CLOUD_ASR_UPLOAD_STATE_STOP;

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
    tal_mutex_lock(sg_ai_cloud_asr.mutex);

    if (sg_ai_cloud_asr.state != AI_CLOUD_ASR_STATE_WAIT_ASR) {
        PR_NOTICE("the state is not wait cloud asr");
        tal_mutex_unlock(sg_ai_cloud_asr.mutex);
        return OPRT_COM_ERROR;
    }

    sg_ai_cloud_asr.state = AI_CLOUD_ASR_STATE_IDLE;

    tal_mutex_unlock(sg_ai_cloud_asr.mutex);

    return OPRT_OK;
}

/**
 * @brief Transitions audio cloud ASR process to the idle state, interrupting any ongoing uploads.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the operation is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_cloud_asr_set_idle(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_mutex_lock(sg_ai_cloud_asr.mutex);

    if (sg_ai_cloud_asr.state ==  AI_CLOUD_ASR_STATE_IDLE) {
        tal_mutex_unlock(sg_ai_cloud_asr.mutex);
        return;
    }

    ai_audio_agent_upload_intrrupt();

    sg_ai_cloud_asr.state = AI_CLOUD_ASR_STATE_IDLE;

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

OPERATE_RET ai_audio_cloud_is_wait_asr(void)
{
    if(AI_CLOUD_ASR_STATE_WAIT_ASR == sg_ai_cloud_asr.state) {
        return true;
    }else {
        return false;
    }
}