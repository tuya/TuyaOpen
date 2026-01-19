/**
 * @file ai_audio_input.c
 * @brief AI audio input module implementation
 *
 * This module implements audio input processing with VAD (Voice Activity Detection)
 * support. It handles audio frame collection, ring buffer management, and provides
 * both manual and automatic VAD modes.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tuya_ringbuf.h"
#include "tkl_vad.h"

#include "tal_api.h"

#include "tdl_audio_manage.h"
#include "stop_watch.h"
#include "ai_audio_input.h"
#include "ai_user_event.h"
/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    bool                     enable;
    bool                     wakeup_flag;
    
    AI_AUDIO_VAD_MODE_E      vad_mode;
    AI_AUDIO_VAD_STATE_E     vad_flag;
    uint16_t                 vad_size;
    THREAD_HANDLE            vad_task;
    TUYA_RINGBUFF_T          ringbuf;
    MUTEX_HANDLE             mutex;

    uint16_t                 slice_size;
    AI_AUDIO_OUTPUT          output_cb;
}AI_AUDIO_RECODER_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_AUDIO_RECODER_T *sg_recorder = NULL;
static TDL_AUDIO_HANDLE_T sg_audio_hdl = NULL;
/***********************************************************
***********************function define**********************
***********************************************************/
/**
@brief Check and send audio slice data if available
@param more_data Pointer to flag indicating if more data is available
@return OPERATE_RET Operation result
*/
static OPERATE_RET __audio_slice_check_and_send(bool *more_data)
{
    if(NULL == more_data) {
        return OPRT_INVALID_PARM;
    }

    *more_data = FALSE;

    if (sg_recorder->vad_flag == AI_AUDIO_VAD_START) {
        tal_mutex_lock(sg_recorder->mutex);
        uint32_t len = tuya_ring_buff_used_size_get(sg_recorder->ringbuf);
        /* If cache is larger than slice size, read all and send */
        if (len >= sg_recorder->slice_size) {
            
            uint8_t *cache_data = (uint8_t*)Malloc(sg_recorder->slice_size);
            uint32_t read_len = tuya_ring_buff_read(sg_recorder->ringbuf, cache_data, sg_recorder->slice_size);
        
            sg_recorder->output_cb(cache_data, read_len);
            Free(cache_data);
            *more_data = TRUE;
        }
        tal_mutex_unlock(sg_recorder->mutex);
    }

    return OPRT_OK;
}

/**
@brief Put audio frame data into ring buffer
@param type Audio frame format
@param status Audio status
@param data Pointer to audio data
@param data_len Audio data length
@return None
*/
static void __audio_frame_put(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data, uint32_t data_len)
{
    /* Microphone disabled? */
    if (sg_recorder == NULL || !sg_recorder->enable)
        return;
    
    if (sg_recorder->vad_mode == AI_AUDIO_VAD_MANUAL){
        /* In manual mode, if has VAD flag, send audio data to cache */
        /* Why we need cache the data? It is because the audio data is from the CP1, by IPC sync message */
        /* In IPC sync operation, we cannot do anything which can cause block */
        if (sg_recorder->vad_flag == AI_AUDIO_VAD_START) { 
            tal_mutex_lock(sg_recorder->mutex);
            tuya_ring_buff_write(sg_recorder->ringbuf, data, (uint16_t)data_len);
            tal_mutex_unlock(sg_recorder->mutex);            
        } else {
            /* No VAD flag, ignore */
        }
    } else {
        /* In auto mode, cache the data to ring buffer */
        tal_mutex_lock(sg_recorder->mutex);
        uint32_t len = tuya_ring_buff_used_size_get(sg_recorder->ringbuf);
        if (len >= sg_recorder->vad_size -1) {
            /* If ring buffer is full, drop pframe->buf_size */
            tuya_ring_buff_discard(sg_recorder->ringbuf, data_len);
        }
        tuya_ring_buff_write(sg_recorder->ringbuf, data, (uint16_t)data_len);
        tal_mutex_unlock(sg_recorder->mutex);
    }    

    AI_NOTIFY_MIC_DATA_T mic_data;
    mic_data.data = data;
    mic_data.data_len = data_len;
    ai_user_event_notify(AI_USER_EVT_MIC_DATA, (void*)&mic_data);

    return;
}

/**
@brief Update VAD flag and publish event if wake-up is active
@param flag VAD state flag
@return None
*/
static void __update_vad_flag(AI_AUDIO_VAD_STATE_E flag)
{
    PR_DEBUG("audio input -> vad stat change to flag %d", flag);
    if (sg_recorder->wakeup_flag) {
        tal_event_publish(EVENT_AUDIO_VAD, (void*)flag);
    }
}

/**
@brief Audio recording task function
@param arg Task argument (unused)
@return None
*/
static void __record_task(void *arg)
{
    bool more_data = false;

    while(sg_recorder->vad_task && tal_thread_get_state(sg_recorder->vad_task) == THREAD_STATE_RUNNING) {
        /* Microphone disabled or not wake-up, don't need to send VAD stat change */
        if (!sg_recorder->enable || !sg_recorder->wakeup_flag) {
           tal_system_sleep(10);
		   continue;
        }

        __audio_slice_check_and_send(&more_data);

        /* Manual mode don't need send VAD stat change */
        if (sg_recorder->vad_mode == AI_AUDIO_VAD_AUTO) {
            AI_AUDIO_VAD_STATE_E stat = (tkl_vad_get_status() == TKL_VAD_STATUS_SPEECH) ?\
                                        AI_AUDIO_VAD_START : AI_AUDIO_VAD_STOP;
            if (stat != sg_recorder->vad_flag) {
                PR_DEBUG("audio input -> wakup flag is %d, auto vad set from %d to %d!",\
                          sg_recorder->wakeup_flag, sg_recorder->vad_flag, stat);
                sg_recorder->vad_flag = stat;
                __update_vad_flag(sg_recorder->vad_flag);
            }
        }
    }
}

/**
@brief Destroy audio recorder and free resources
@return None
*/
static void __audio_recorder_destroy(void)
{
    if (sg_recorder) {
        if (sg_recorder->mutex) {
            tal_mutex_release(sg_recorder->mutex);
        }

        if (sg_recorder->ringbuf) {
            tuya_ring_buff_free(sg_recorder->ringbuf);
        }

        tal_free(sg_recorder);
        sg_recorder = NULL;
    }
}

/**
@brief Create and initialize audio recorder
@param cfg Audio input configuration
@param audio_info Audio information structure
@return Pointer to created recorder, NULL on failure
*/
static AI_AUDIO_RECODER_T *__audio_recorder_create(AI_AUDIO_INPUT_CFG_T *cfg, TDL_AUDIO_INFO_T *audio_info)
{
    TUYA_CHECK_NULL_RETURN(cfg, NULL);
    TUYA_CHECK_NULL_RETURN(audio_info, NULL);

    if (sg_recorder)
        return sg_recorder;

    OPERATE_RET rt = OPRT_OK;
    TUYA_CHECK_NULL_RETURN(sg_recorder = tal_calloc(1, sizeof(AI_AUDIO_RECODER_T)), NULL);
    memset(sg_recorder, 0, sizeof(AI_AUDIO_RECODER_T));
 
    sg_recorder->output_cb      = cfg->output_cb;
    sg_recorder->wakeup_flag    = FALSE;
    sg_recorder->vad_task       = NULL;
    sg_recorder->vad_mode       = cfg->vad_mode;

    uint32_t audio_1ms_size     = audio_info->sample_rate * audio_info->sample_bits * audio_info->sample_ch_num / 8 / 1000;
    sg_recorder->vad_size       = (cfg->vad_active_ms + 300) * audio_1ms_size + 1;
    sg_recorder->slice_size     = cfg->slice_ms * audio_1ms_size;

    uint32_t rb_size = sg_recorder->vad_size;
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(rb_size, OVERFLOW_PSRAM_STOP_TYPE, &sg_recorder->ringbuf), __error);
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_recorder->mutex), __error);
    PR_DEBUG("recorder vad mode %d", cfg->vad_mode);
    PR_DEBUG("recorder total ms %d, slice ms %d, vad active %d ms, vad off timeout %d", rb_size, cfg->slice_ms, cfg->vad_active_ms, cfg->vad_off_ms);

    return sg_recorder;

__error:
    __audio_recorder_destroy();

    return NULL;
}

/**
@brief Start audio input
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_input_start(void)
{
    PR_NOTICE("audio input -> start! mode is %d, task is %p", sg_recorder->vad_mode, sg_recorder->vad_task);
    sg_recorder->enable = true;

    if (!sg_recorder->vad_task) {
        THREAD_CFG_T thrd_cfg = {
            .priority = THREAD_PRIO_5,
            .stackDepth = 2 * 1024 + 512,  /* Support opus encode */
            .thrdname = "record_task",
            #if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
            .psram_mode = 1,
            #endif            
        };
        tal_thread_create_and_start(&sg_recorder->vad_task, NULL, NULL, __record_task, NULL, &thrd_cfg);
    }

    if (sg_recorder->vad_mode == AI_AUDIO_VAD_AUTO) {
        PR_DEBUG("need human voice detect, start __record_task");
        tkl_vad_start();
    }

    return OPRT_OK;    
}

/**
@brief Stop audio input
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_input_stop(void)
{
    PR_NOTICE("audio input -> stop! mode is %d, task is %p", sg_recorder->vad_mode, sg_recorder->vad_task);
    sg_recorder->enable = false;
    if (sg_recorder->vad_mode == AI_AUDIO_VAD_AUTO) {
        /* Stop VAD detect */
        tkl_vad_stop();
    }

    if (sg_recorder->vad_task) {
        tal_thread_delete(sg_recorder->vad_task);
        sg_recorder->vad_task = NULL;
    }

    return OPRT_OK;
}

/**
@brief Deinitialize the AI audio input module
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_input_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_CHECK_NULL_RETURN(sg_recorder, OPRT_OK);

    /* Stop */
    TUYA_CALL_ERR_LOG(ai_audio_input_stop());

    /* Stop mic, speaker, audio AFE (AEC, NS, VAD) */
    TUYA_CALL_ERR_LOG(tdl_audio_close(sg_audio_hdl));

    /* Release resource */
    __audio_recorder_destroy();

    return rt;
}

/**
@brief Initialize the AI audio input module
@param cfg Audio input configuration
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_input_init(AI_AUDIO_INPUT_CFG_T *cfg)
{    
    OPERATE_RET rt = OPRT_OK;
    TDL_AUDIO_INFO_T audio_info;

    TUYA_CHECK_NULL_RETURN(cfg, OPRT_INVALID_PARM);

    TUYA_CALL_ERR_RETURN(tdl_audio_find(AUDIO_CODEC_NAME, &sg_audio_hdl));
    TUYA_CALL_ERR_RETURN(tdl_audio_get_info(sg_audio_hdl, &audio_info));

    PR_DEBUG("sample %d, databits %d, channel %d", audio_info.sample_rate, audio_info.sample_bits, audio_info.sample_ch_num);

    TUYA_CHECK_NULL_RETURN(sg_recorder = __audio_recorder_create(cfg, &audio_info), OPRT_MALLOC_FAILED);

    TUYA_CALL_ERR_RETURN(tdl_audio_open(sg_audio_hdl, __audio_frame_put));

    TKL_VAD_CONFIG_T vad_cfg = {0};
    memset(&vad_cfg, 0, sizeof(TKL_VAD_CONFIG_T));
    vad_cfg.sample_rate     = audio_info.sample_rate;
    vad_cfg.channel_num     = audio_info.sample_ch_num;
    vad_cfg.speech_min_ms   = cfg->vad_active_ms;
    vad_cfg.noise_min_ms    = cfg->vad_off_ms;
    vad_cfg.frame_duration_ms = 10;
    vad_cfg.scale           = 1.0f;
    TUYA_CALL_ERR_RETURN(tkl_vad_init(&vad_cfg));

    TUYA_CALL_ERR_RETURN(ai_audio_input_start());

    return OPRT_OK;
}

/**
@brief Set wake-up mode (VAD mode)
@param mode VAD mode (manual or auto)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_input_wakeup_mode_set(AI_AUDIO_VAD_MODE_E mode)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(sg_recorder, OPRT_RESOURCE_NOT_READY);

    PR_NOTICE("audio input -> wakeup mode set from %d to %d!", sg_recorder->vad_mode, mode);
    if (mode != sg_recorder->vad_mode) {
        TUYA_CALL_ERR_LOG(ai_audio_input_stop());
        sg_recorder->vad_mode = mode;
        TUYA_CALL_ERR_LOG(ai_audio_input_start());
    }
    
    return rt;
}

/**
@brief Reset audio input ring buffer and VAD state
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_input_reset(void)
{
    PR_NOTICE("audio input -> reset ringbuf!");

    tal_mutex_lock(sg_recorder->mutex);
    tuya_ring_buff_reset(sg_recorder->ringbuf);
    tal_mutex_unlock(sg_recorder->mutex);
    //sg_recorder->vad_flag = AI_AUDIO_VAD_STOP;

    if (AI_AUDIO_VAD_AUTO == sg_recorder->vad_mode) {
        PR_NOTICE("audio input -> vad stop!");
        tkl_vad_stop();
        tkl_vad_start();
        PR_NOTICE("audio input -> vad start!");
    }

    return OPRT_OK;
}

/**
@brief Set wake-up state
@param is_wakeup Wake-up flag
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_input_wakeup_set(bool is_wakeup)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(sg_recorder, OPRT_RESOURCE_NOT_READY);

    /* Only manual mode support set VAD flag */
    /* Auto mode will update by audio VAD detect */
    if (sg_recorder->wakeup_flag != is_wakeup) {
        PR_NOTICE("audio input -> mode is %d, wakeup set to %d, vad flag is %d!", sg_recorder->vad_mode, is_wakeup, sg_recorder->vad_flag);
        sg_recorder->wakeup_flag = is_wakeup;
        if (sg_recorder->vad_mode == AI_AUDIO_VAD_MANUAL) {
            sg_recorder->vad_flag = is_wakeup ? AI_AUDIO_VAD_START : AI_AUDIO_VAD_STOP;
            __update_vad_flag(sg_recorder->vad_flag);            
        }
    }

    return rt;
}