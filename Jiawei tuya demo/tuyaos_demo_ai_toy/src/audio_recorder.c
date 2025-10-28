/**
* Copyright (C) by Tuya Inc                                                  
* All rights reserved                                                        
*
* @file audio_recorder.h
* @brief audio input and output processing
* @version 1.0
* @author linch
* @date 2025-05-13
*
*/

/*============================ INCLUDES ======================================*/
#include "audio_recorder.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_thread.h"
#include "tuya_ringbuf.h"
#include "tuya_devos_utils.h"
#include "tal_queue.h"
#include "tal_system.h"
#include "tal_mutex.h"
#include "ty_vad_app.h"
#include "tuya_wakeupword.h"
#include "tkl_asr.h"

/*============================ MACROS ========================================*/


/*============================ TYPES =========================================*/
typedef enum {
    AUDIO_RECODER_WAKEUP_EVENT  = 1,
    AUDIO_RECODER_MODE_EVENT,
    AUDIO_RECODER_VAD_EVENT,
    AUDIO_RECODER_START_EVENT,
    AUDIO_RECODER_STOP_EVENT,
} audio_recorder_event_id_t;

typedef enum {
    AUDIO_RECODER_INIT,
    AUDIO_RECODER_CHECK_START,
    AUDIO_RECODER_CHECK_END,
} audio_recorder_task_stat_t;

typedef enum {
    AUDIO_RECODER_VAD_AUTO,
    AUDIO_RECODER_VAD_MANIAL,
} audio_recorder_vad_t;

typedef struct {
    uint8_t                      state;
    BOOL_T                       enable;
    BOOL_T                       vad_enable;
    audio_recorder_vad_t         vad_mode;
    BOOL_T                       vad_flag;
    uint16_t                     vad_size;

    audio_recorder_mode_t        mode;

    TUYA_RINGBUFF_T              ringbuf;
    MUTEX_HANDLE                 mutex;
    
    QUEUE_HANDLE                 event;
    THREAD_HANDLE                task;

    uint8_t                     *stream;
    uint16_t                     stream_size;

    recorder_output_cb           output_cb;
    void                        *user_data;
} audio_recorder_t;


typedef struct {
    int         id;
    uint32_t    value;
} audio_recorder_event_t;


/*============================ PROTOTYPES ====================================*/
static void audio_recorder_task(void *args);

/*============================ LOCAL VARIABLES ===============================*/
static audio_recorder_t *audio_recorder = NULL;

/*============================ IMPLEMENTATION ================================*/
int audio_recorder_write(audio_recorder_t *recorder, uint8_t *data, uint16_t datalen)
{
    int rt = 0;

    if (!recorder) {
        return 0;
    }

    tal_mutex_lock(recorder->mutex);
    rt = tuya_ring_buff_write(recorder->ringbuf, data, datalen);
    tal_mutex_unlock(recorder->mutex);

    return rt;
}

int audio_recoder_read(audio_recorder_t *recorder, uint8_t *data, uint16_t datalen)
{
    int rt = 0;

    if (!recorder) {
        return 0;
    }

    tal_mutex_lock(recorder->mutex);
    rt = tuya_ring_buff_read(recorder->ringbuf, data, datalen);
    tal_mutex_unlock(recorder->mutex);

    return rt;
}


static void audio_recorderer_clear(audio_recorder_t *recorder)
{
    if (NULL == recorder) {
        return;
    }

    if (recorder->mutex) {
        tal_mutex_release(recorder->mutex);
    }

    if (recorder->event) {
        tal_queue_free(recorder->event);
    }

    if (recorder->ringbuf) {
        tuya_ring_buff_free(recorder->ringbuf);
    }

    if (recorder->stream) {
        tal_free(recorder->stream);
    }

    tal_free(recorder);
}


audio_recorder_t *audio_recorderer_create(audio_recorder_cfg_t *cfg)
{
    int rt = OPRT_OK;

    audio_recorder_t *recorder = NULL;

    TUYA_CHECK_NULL_RETURN(cfg, NULL);
    TUYA_CHECK_NULL_RETURN(cfg->output_cb, NULL);

    TUYA_CHECK_NULL_RETURN(recorder = tal_calloc(1, sizeof(audio_recorder_t)), NULL);

    recorder->mode           = cfg->mode;
    recorder->user_data      = cfg->user_data;
    recorder->output_cb      = cfg->output_cb;
    recorder->vad_mode       = AUDIO_RECODER_VAD_AUTO;
    uint32_t audio_1ms_size  = cfg->sample_rate * cfg->sample_bits * cfg->channel / 8 / 1000;
    recorder->stream_size    = cfg->slice_ms * audio_1ms_size;
    recorder->vad_size       = (cfg->vad_active_ms + cfg->vad_off_ms) * audio_1ms_size;
    TUYA_CHECK_NULL_GOTO(recorder->stream = tal_calloc(1, recorder->stream_size), __failed);
    TAL_PR_DEBUG("recorder mode %d, spk io %d, spk io level %d", cfg->mode, cfg->spk_io, cfg->spk_io_level);

    uint32_t rb_size = cfg->total_ms * audio_1ms_size + 1;
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(rb_size, OVERFLOW_PSRAM_COVERAGE_TYPE, &recorder->ringbuf), __failed);
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&recorder->mutex), __failed);
    TUYA_CALL_ERR_GOTO(tal_queue_create_init(&recorder->event, sizeof(audio_recorder_event_t), 20), __failed);

    TAL_PR_DEBUG("recorder total ms %d, slice ms %d, vad_case ms %d", rb_size, cfg->vad_active_ms + cfg->vad_off_ms);

    return recorder;

__failed:
    audio_recorderer_clear(recorder);
    return NULL;
}

static INT_T audio_recorder_frame_put(TKL_AUDIO_FRAME_INFO_T *pframe)
{
    audio_recorder_t *recorder = audio_recorder;

    if (!recorder || !recorder->enable) {
        return 1;
    }

    int  written_size = audio_recorder_write(recorder, pframe->pbuf, (uint16_t)pframe->buf_size);
    if (written_size != pframe->buf_size) {
        TAL_PR_ERR("audio_recoder overflow %d", pframe->buf_size - written_size);
        return 1;
    }


    return 0;
}



int audio_recorder_event_post(audio_recorder_t *recorder, int id, uint32_t value)
{
    audio_recorder_event_t event;

    event.id    = id;
    event.value = value;

    return tal_queue_post(recorder->event, &event, 0);
}


VOID_T audio_wakeup_word_event(VOID *buf, UINT_T len, VOID *args)
{
    audio_recorder_t *recorder = (audio_recorder_t *)args;

    UINT_T event = *(uint8_t *)buf;

    TAL_PR_DEBUG("audio_wakeup_word_event  %d", event);
    if (event) {
        audio_recorder_event_post(recorder, AUDIO_RECODER_WAKEUP_EVENT, 1);
    }
}



int audio_recorder_init(audio_recorder_cfg_t *cfg)
{
    int rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(audio_recorder = audio_recorderer_create(cfg), OPRT_MALLOC_FAILED);

    TKL_AUDIO_CONFIG_T audio_config;
    memset(&audio_config, 0, sizeof(TKL_AUDIO_CONFIG_T));
    audio_config.enable                 = 1;      //! enable aec
    audio_config.ai_chn                 = 0;
    audio_config.sample                 = cfg->sample_rate;
    audio_config.spk_sample             = cfg->sample_rate;
    audio_config.datebits               = cfg->sample_bits;
    audio_config.channel                = cfg->channel;
    audio_config.codectype              = TKL_CODEC_AUDIO_PCM;
    audio_config.card                   = TKL_AUDIO_TYPE_BOARD;
    audio_config.spk_gpio               = cfg->spk_io;
    audio_config.spk_gpio_polarity      = cfg->spk_io_level;
    audio_config.put_cb                 = audio_recorder_frame_put;

    TUYA_CALL_ERR_GOTO(tkl_ai_init(&audio_config, 0), __failed);
    TUYA_CALL_ERR_GOTO(tkl_ai_start(0, 0), __failed);

    ty_vad_config_t vad_config;
    memset(&vad_config, 0, sizeof(ty_vad_config_t));
    vad_config.silence_threshold_ms     = 0;
    vad_config.scale                    = 1.0;
    vad_config.sample_rate              = cfg->sample_rate;
    vad_config.channel                  = cfg->channel;
    vad_config.start_threshold_ms       = cfg->vad_active_ms;
    vad_config.end_threshold_ms         = cfg->vad_silence_ms;
    vad_config.vad_frame_duration       = 20;
    TUYA_CALL_ERR_GOTO(ty_vad_app_init(&vad_config), __failed);

    tkl_asr_init(audio_wakeup_word_event, audio_recorder);

    // init recorder key mode
    if (AUDIO_RECODER_MODE_KEY_HOLD == audio_recorder->mode) {
        audio_recorder->vad_mode = AUDIO_RECODER_VAD_MANIAL;
        tuya_wakeupword_disable();
    } else {
        audio_recorder->vad_mode = AUDIO_RECODER_VAD_AUTO;
    }

    if (audio_recorder->vad_mode == AUDIO_RECODER_VAD_AUTO) {
        ty_vad_app_start();
        audio_recorder_enable();
        tuya_wakeupword_enable();
    }

    //! start thread
    THREAD_CFG_T task_cfg = {6 * 1024, THREAD_PRIO_2, "recorder"};
    TUYA_CALL_ERR_GOTO(tal_thread_create_and_start(&audio_recorder->task, 
                       NULL, 
                       NULL, 
                       audio_recorder_task, 
                       audio_recorder, 
                       &task_cfg),
                       __failed);

    return OPRT_OK;


__failed:
    tkl_ai_uninit();
    ty_vad_app_stop();
    audio_recorderer_clear(audio_recorder);
    audio_recorder = NULL;

    return rt;
}


int audio_recorder_enable(void)
{
    if (audio_recorder) {
        audio_recorder->enable = TRUE;
        return OPRT_OK;
    }

    return OPRT_RESOURCE_NOT_READY;
}


int audio_recorder_start(void)
{
    if (!audio_recorder) {
        return OPRT_RESOURCE_NOT_READY;
    }

    return audio_recorder_event_post(audio_recorder, AUDIO_RECODER_START_EVENT, 1);
}

int audio_recorder_stop(void)
{
    if (!audio_recorder) {
        return OPRT_RESOURCE_NOT_READY;
    }

    return audio_recorder_event_post(audio_recorder, AUDIO_RECODER_STOP_EVENT, 0);
}


int audio_recorder_disable(void)
{
    if (audio_recorder) {
        audio_recorder->enable = FALSE;
        return OPRT_OK;
    }

    return OPRT_RESOURCE_NOT_READY;
}



int audio_recorder_key_vad_set(int vad_flag)
{
    if (!audio_recorder) {
        return OPRT_RESOURCE_NOT_READY;
    }

    if (audio_recorder->mode != AUDIO_RECODER_MODE_KEY_HOLD) {
        return OPRT_NOT_SUPPORTED;
    }

    if (audio_recorder->vad_flag == vad_flag) {
        return OPRT_OK;
    }

    return audio_recorder_event_post(audio_recorder, AUDIO_RECODER_VAD_EVENT, vad_flag);
}


int audio_recorder_mode_set(audio_recorder_mode_t mode)
{
    if (!audio_recorder) {
        return OPRT_RESOURCE_NOT_READY;
    }

    return audio_recorder_event_post(audio_recorder, AUDIO_RECODER_MODE_EVENT, mode);
}


audio_recorder_mode_t audio_recorder_mode_get(void)
{
    if (!audio_recorder) {
        return AUDIO_RECODER_MODE_MAX;
    }

    return  audio_recorder->mode;
}


static int audio_vad_check(audio_recorder_t *recorder)
{
    int stat = 0;

    //！ FIXME:
    if (recorder->vad_mode == AUDIO_RECODER_VAD_AUTO) {
        stat = ty_get_vad_flag();
        return 1 == stat ? true : false;
    }

    return recorder->vad_flag ? true : false;
}


void audio_recoder_clear(audio_recorder_t *recorder)
{
    tal_mutex_lock(recorder->mutex);
    tuya_ring_buff_reset(recorder->ringbuf);
    tal_mutex_unlock(recorder->mutex);
}


uint32_t audio_recoder_get_size(audio_recorder_t *recorder)
{
    uint32_t rt = 0;

    tal_mutex_lock(recorder->mutex);
    rt = tuya_ring_buff_used_size_get(recorder->ringbuf);
    tal_mutex_unlock(recorder->mutex);

    return rt;
}

static int audio_recoder_drop(audio_recorder_t *recorder)
{
    //！长按模式无VAD缓存
    if (AUDIO_RECODER_VAD_MANIAL == recorder->vad_mode) {
        return OPRT_OK;
    }

    //! vad检测需要300ms时间, 每20ms时间检测一次
    //！保留vad_active + vad_off
    uint32_t recoder_size = audio_recoder_get_size(recorder);
    if (recoder_size > recorder->vad_size) {
        recoder_size = recoder_size - recorder->vad_size;
        
        int count  = 0;
        count = recoder_size / recorder->stream_size;

        while (count) {
            audio_recoder_read(recorder, recorder->stream, recorder->stream_size);
            recoder_size -= recorder->stream_size;
            count--;
        }
        if (recoder_size) {
            audio_recoder_read(recorder, recorder->stream, recoder_size);
        }
    }
}


static const char *recorder_statstr[] = {
    "AUDIO_RECODER_INIT",
    "AUDIO_RECODER_CHECK_START",
    "AUDIO_RECODER_CHECK_END",
};

#define RECODER_STAT_CHANGE(new_stat) do { \
        if (recorder->state != new_stat) {  \
            TAL_PR_DEBUG("recoder stat change: %s -> %s", recorder_statstr[recorder->state], recorder_statstr[new_stat]); \
            recorder->state = new_stat; \
        }   \
    } while(0)


static void audio_recorder_callback(audio_recorder_t *recorder, audio_recorder_stat_t stat)
{
    //! USER CALL
    audio_recorder_msg_t msg = {
        .state   = stat,
        .mode    = recorder->mode,
        .data    = NULL,
        .datalen = 0,
    };

    recorder->output_cb(&msg,  recorder->user_data);
}


static int audio_recorder_output(audio_recorder_t *recorder, audio_recorder_stat_t stat, uint8_t *stream, uint16_t stream_size)
{
    if (stream && stream_size) {
        audio_recoder_read(recorder, stream, stream_size);

    }

    //! USER CALL
    audio_recorder_msg_t msg = {
        .state   = stat,
        .mode    = recorder->mode,
        .data    = stream,
        .datalen = stream_size,
    };

    recorder->output_cb(&msg,  recorder->user_data);

    return OPRT_OK;
}


static void audio_recorder_task(void *args)
{
    int           rt         = 0;
    UINT_T      timeout      = 20;
    uint32_t    recoder_size = 0;
    int         count        = 0;

    audio_recorder_event_t event;

    SYS_TICK_T  start, end;

    audio_recorder_t *recorder = (audio_recorder_t *)(args);

    while(tal_thread_get_state(recorder->task) == THREAD_STATE_RUNNING) {

        rt = tal_queue_fetch(recorder->event, &event, timeout);
        if (OPRT_OK == rt) {
            TAL_PR_DEBUG("recorder event %d", event.id);

            if (AUDIO_RECODER_VAD_AUTO == recorder->vad_mode) {
                audio_recorder_disable();
                ty_vad_app_stop();
                audio_recoder_clear(recorder);
            }
            
            if (AUDIO_RECODER_START_EVENT == event.id) {
                audio_recorder_callback(recorder, AUDIO_RECODER_START);
            } else if (AUDIO_RECODER_WAKEUP_EVENT == event.id) {
                audio_recorder_callback(recorder, AUDIO_RECODER_WAKEUP);
            } else if (AUDIO_RECODER_VAD_EVENT == event.id) {   //! vad set by extern
                recorder->vad_flag = event.value;
                if (recorder->vad_flag) {
                    audio_recorder_enable();
                    RECODER_STAT_CHANGE(AUDIO_RECODER_INIT);
                } else {
                    timeout = 100;                              //！ 按键释放太快，导致漏字，进行补偿100ms
                    continue;
                }
                TAL_PR_DEBUG("vad value %d", event.value);
            } else if (AUDIO_RECODER_MODE_EVENT == event.id) {
                recorder->mode = event.value;
                audio_recorder_output(recorder, AUDIO_RECODER_MODE_UPDATE, NULL, 0);
                if (AUDIO_RECODER_MODE_KEY_HOLD == recorder->mode) {
                    recorder->vad_mode = AUDIO_RECODER_VAD_MANIAL;
                    tuya_wakeupword_disable();
                } else  {
                    recorder->vad_mode = AUDIO_RECODER_VAD_AUTO;
                    tuya_wakeupword_enable();
                }
            }  else if (AUDIO_RECODER_STOP_EVENT == event.id) {
                audio_recorder_output(recorder, AUDIO_RECODER_STOP, NULL, 0);
            }
            
            if (AUDIO_RECODER_VAD_AUTO == recorder->vad_mode) {
                ty_vad_app_start();
                audio_recorder_enable();
                RECODER_STAT_CHANGE(AUDIO_RECODER_INIT);
                continue;
            }
            
        } else {
            timeout = 20;
        }
        // start = tal_time_get_posix_ms();
        switch (recorder->state) {

        case AUDIO_RECODER_INIT:
            if (!recorder->enable) {
                break;
            }
            recorder->state = AUDIO_RECODER_CHECK_START;

        case AUDIO_RECODER_CHECK_START:
            if (audio_vad_check(recorder)) {
                if (audio_recoder_get_size(recorder) > recorder->stream_size) {
                    audio_recorder_output(recorder,  AUDIO_RECODER_VAD_START,  recorder->stream, recorder->stream_size);
                } else {
                    TAL_PR_DEBUG("recoder size get %d", audio_recoder_get_size(recorder));
                    audio_recorder_output(recorder,  AUDIO_RECODER_VAD_START, NULL, 0);
                }
                RECODER_STAT_CHANGE(AUDIO_RECODER_CHECK_END);
            } else {
                audio_recoder_drop(recorder);
            }
            break;

        case AUDIO_RECODER_CHECK_END:
            if (!audio_vad_check(recorder)) {
                recoder_size = audio_recoder_get_size(recorder);
                count = recoder_size / recorder->stream_size;
                TAL_PR_DEBUG("recoder stream count %d", count);
                while (count) {
                    audio_recorder_output(recorder, AUDIO_RECODER_VAD_SPEAK,  recorder->stream,  recorder->stream_size);
                    count--;
                    recoder_size -= recorder->stream_size;
                }
                audio_recorder_output(recorder, AUDIO_RECODER_VAD_END, recorder->stream, recoder_size);
                //！模式处理
                if (recorder->mode == AUDIO_RECODER_MODE_KEY_HOLD) {
                    audio_recorder_disable();
                    audio_recoder_clear(recorder);
                } 
                RECODER_STAT_CHANGE(AUDIO_RECODER_CHECK_START);
            } else {
                count = audio_recoder_get_size(recorder) / recorder->stream_size;
                while (count) {
                    audio_recorder_output(recorder,  AUDIO_RECODER_VAD_SPEAK,  recorder->stream,  recorder->stream_size);
                    count--;
                }
            }
            break;
        }
        //! mem usage
        static uint16_t mem_usage = 0;
        if (mem_usage++ >= 3000) {
            mem_usage = 0;
            TAL_PR_NOTICE("cpu0 mem free sram: %d, psram: %d\n",
                    tkl_system_get_free_heap_size(),
                    tkl_system_psram_get_free_heap_size());
        }

        //! 时间补偿
        // end = tal_time_get_posix_ms();
        // if (end - start > 20) {
        // }
    }
}
