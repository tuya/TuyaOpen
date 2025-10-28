#include "tuya_cloud_types.h"
#include "tal_system.h"
#include "tal_network.h"
#include "tuya_ai_encoder_opus_ipc.h"
#include "tuya_ai_proc.h"
#include "tuya_speaker_service.h"
#include "tal_log.h"
#include "tuya_device_cfg.h"

//!  video
#define MAX_INPUT_RINGBUG_SIZE          (64*1024)
#define MAX_INPUT_BUF_SIZE              (16*1024)
#define LLM_IDLE_TIMEOUT                (30 * 1000)
#define LLM_TTS_TIMEOUT                 (15 * 1000)
#define LLM_ASR_TIMEOUT                 (15 * 1000)


STATIC VOID ai_proc_asr_timeout(TIMER_ID timer_id, VOID_T *arg)
{
    ty_ai_proc_t *llm = (ty_ai_proc_t *)arg;

    ai_proc_msg_t msg = {
        .data = NULL,
        .datalen = 0,
        .event = AI_PROC_ASR_TIMEOUT
    };

    TAL_PR_NOTICE("ai_proc_asr_timeout");

    llm->output_cb(&msg, llm->user_data);
}


STATIC VOID ai_proc_tts_timeout(TIMER_ID timer_id, VOID_T *arg)
{
    ty_ai_proc_t *llm = (ty_ai_proc_t *)arg;

    ai_proc_msg_t msg = {
        .data = NULL,
        .datalen = 0,
        .event = AI_PROC_TTS_TIMEOUT
    };

    TAL_PR_NOTICE("ai_proc_tts_timeout");

    llm->output_cb(&msg, llm->user_data);
}

int ty_ai_proc_video_write(ty_ai_proc_t *llm, uint8_t *data, uint16_t datalen)
{
    int rt = 0;

    tal_mutex_lock(llm->video_mutex);
    if (tuya_ring_buff_used_size_get(llm->video_ringbuf)) {
        tuya_ring_buff_reset(llm->video_ringbuf);
    }
    rt = tuya_ring_buff_write(llm->video_ringbuf, data, datalen);
    tal_mutex_unlock(llm->video_mutex);

    return rt;
}


int ty_ai_proc_video_push(ty_ai_proc_t *llm)
{
    int rt   = 0;
    int size = 0;

    tal_mutex_lock(llm->video_mutex);
    size = tuya_ring_buff_read(llm->video_ringbuf, llm->video_buf, MAX_INPUT_BUF_SIZE * 2);
    if (size > 0) {
        ai_proc_msg_t msg = {
            .event   = AI_PROC_VIDEO_EVENT,
            .data    = NULL,
            .datalen = 0,
        };
        msg.data =  tkl_system_psram_malloc(size + 1);
        if (NULL == msg.data) {
            TAL_PR_NOTICE("tkl_system_psram_malloc failed");
            tal_mutex_unlock(llm->video_mutex);
            return OPRT_MALLOC_FAILED;
        }
        msg.datalen = size;
        memcpy(msg.data, llm->video_buf, size);
        msg.data[size] = 0;
        rt = tal_queue_post(llm->event, &msg, 0);
        TAL_PR_DEBUG("AI_PROC_VIDEO_EVENT");
    }
    tal_mutex_unlock(llm->video_mutex);

    return rt;
}



int ty_ai_proc_event_send(ty_ai_proc_t *llm, int event, uint8_t *data, uint16_t datalen)
{
    int rt = 0;
    ai_proc_msg_t msg = {0};

    if (!ty_ai_chat_is_online()) {
        TAL_PR_DEBUG("ai is not online");
        return OPRT_NETWORK_ERROR;
    }

    msg.event   = event;
    msg.data    = NULL;
    msg.datalen = 0;

    if (data && datalen) {
        if (AI_PROC_VIDEO_EVENT == event) {
        #if ENABLE_VIDEO_ONE_FRAME
            return ty_ai_proc_video_write(llm, data, datalen);
        #else
            tal_mutex_lock(llm->video_mutex);
            tuya_ring_buff_write(llm->video_ringbuf, data, datalen);
            tal_mutex_unlock(llm->video_mutex);
        #endif
        } else {
            msg.data =  tkl_system_psram_malloc(datalen + 1);
            if (NULL == msg.data) {
                TAL_PR_NOTICE("tkl_system_psram_malloc failed");
                return OPRT_MALLOC_FAILED;
            }
            msg.datalen = datalen;
            memcpy(msg.data, data, datalen);
            msg.data[datalen] = 0;
        }
    }

    rt = tal_queue_post(llm->event, &msg, 0);
    if (OPRT_OK != rt && msg.data && msg.datalen) {
        TAL_PR_DEBUG("ai llm send faied %d", rt);
        tkl_system_psram_free(msg.data);
    }

    #if ENABLE_VIDEO_ONE_FRAME
    if (AI_PROC_AUDIO_EVENT == event) {
        ty_ai_proc_video_push(llm);
    }
    #endif

    return rt;
}

STATIC OPERATE_RET ai_proc_skill_process(ty_ai_proc_t *llm, AI_CHAT_SKILL_T *skill)
{
    int rt = OPRT_OK;

    if (skill == NULL || skill->json == NULL) {
        TAL_PR_ERR("skill is NULL");
        return OPRT_INVALID_PARM;
    }

    // demo code for custom skill info dump
    CHAR_T *tmp = ty_cJSON_PrintUnformatted(skill->json);
    TAL_PR_DEBUG("skill type: %s, data: %s", skill->type, tmp);
    ty_cJSON_FreeBuffer(tmp);

    ty_cJSON *json = skill->json;
    if (strcmp(skill->type, "CloudEvent") == 0) {
        ty_cJSON *action = ty_cJSON_GetObjectItem(json, "action");
        if (action && strcmp(action->valuestring, "TriggerAiChat") == 0) {
            ty_cJSON *node;
            ty_cJSON *data = ty_cJSON_GetObjectItem(json, "data");
            // parse content and requestId
            node = ty_cJSON_GetObjectItem(data, "content");
            CHAR_T *content = node ? node->valuestring : "";
            node = ty_cJSON_GetObjectItem(data, "requestId");
            CHAR_T *request_id = node ? node->valuestring : "";
            // send interrupt
            // ai_proc_trigger_t *trigger = 
            // trigger.request_id = request_id;
            rt  = ty_ai_proc_event_send(llm, AI_PROC_INTERRUPT_EVENT, NULL, 0);
            rt |= ty_ai_proc_event_send(llm, AI_PROC_SKILL_EVENT, request_id, strlen(request_id));
            rt |= ty_ai_proc_event_send(llm, AI_PROC_TEXT_EVENT, content, strlen(content));
            rt |= ty_ai_proc_event_send(llm, AI_PROC_FINSH_EVENT, NULL, 0);
            if (OPRT_OK != rt) {
                TAL_PR_ERR("ty_ai_proc_event_send failed");
                return OPRT_COM_ERROR;
            }
        } else {
            TAL_PR_DEBUG("unknown action: %s", action->valuestring);
            return OPRT_INVALID_PARM;
        }
    }

    return OPRT_NOT_SUPPORTED;
}


static int ai_proc_event_cb(TUYA_SPEAKER_SERVICE_EVENT_INFO_S *info, VOID *user_data)
{
    ty_ai_proc_t *llm = (ty_ai_proc_t *)user_data;
    ai_proc_msg_t msg = {0};

    // TAL_PR_DEBUG("ai_proc_event %d", info->event);
    msg.data     = info->data;
    msg.datalen  = info->dlen;

    if (info->event == AI_CHAT_EVENT_ASR_EMPTY) {
        tal_sw_timer_stop(llm->asr_timer);
        TAL_PR_DEBUG("asr result Empty");
        msg.event = AI_PROC_ASR_EMPTY;
        llm->output_cb(&msg, llm->user_data);
    } else if (info->event == AI_CHAT_EVENT_ASR_OK) {
        TAL_PR_DEBUG("asr result: %s", info->data);
        tal_sw_timer_stop(llm->asr_timer);
        msg.event = AI_PROC_ASR_OK;
        llm->output_cb(&msg, llm->user_data);
        TAL_PR_DEBUG("llm %p, llm->output_cb: %p", llm, llm->output_cb);        
        tal_sw_timer_start(llm->tts_timer, LLM_TTS_TIMEOUT, TAL_TIMER_ONCE);
    } else if (info->event == AI_CHAT_EVENT_TTS_START) {
        TAL_PR_DEBUG("tts start");
        tal_sw_timer_stop(llm->tts_timer);
        msg.event = AI_PROC_TTS_START;
        llm->output_cb(&msg, llm->user_data);
    } else if (info->event == AI_CHAT_EVENT_TTS_DATA) {
        // TAL_PR_DEBUG("tts mid");
        msg.event = AI_PROC_TTS_DATA;
        llm->output_cb(&msg, llm->user_data);
    } else if (info->event == AI_CHAT_EVENT_TTS_STOP) {
        TAL_PR_DEBUG("tts stop");
        msg.event = AI_PROC_TTS_STOP;
        llm->output_cb(&msg, llm->user_data);
    } else if (info->event == AI_CHAT_EVENT_TEXT_STREAM_START) {
        TAL_PR_DEBUG("text start");
        msg.event = AI_PROC_TEXT_START;
        llm->output_cb(&msg, llm->user_data);
    } else if (info->event == AI_CHAT_EVENT_TEXT_STREAM_DATA) {
        // TAL_PR_DEBUG("text mid");
        msg.event = AI_PROC_TEXT_DATA;
        llm->output_cb(&msg, llm->user_data);
    } else if (info->event == AI_CHAT_EVENT_TEXT_STREAM_STOP) {
        TAL_PR_DEBUG("text stop");
        msg.event = AI_PROC_TEXT_STOP;
        llm->output_cb(&msg, llm->user_data);
    } else if (info->event == AI_CHAT_EVENT_EMOTION) {
        msg.event = AI_PROC_ASR_EMOJI;
        llm->output_cb(&msg, llm->user_data);
        TAL_PR_DEBUG("emotion %s", info->data);
    } else if (info->event == AI_CHAT_EVENT_CHAT_BREAK) {
        tal_sw_timer_stop(llm->asr_timer);
        tal_sw_timer_stop(llm->tts_timer);
        msg.event = AI_PROC_CHAT_BREAK;
        llm->output_cb(&msg, llm->user_data);
        TAL_PR_DEBUG("AI_CHAT_EVENT_CHAT_BREAK");
    } else if (info->event == AI_CHAT_EVENT_SERVER_VAD) {
        msg.event = AI_PROC_CHAT_BREAK;
        llm->output_cb(&msg, llm->user_data);
        TAL_PR_DEBUG("AI_CHAT_EVENT_SERVER_VAD");
    } else if (info->event == AI_CHAT_EVENT_SKILL) {
        AI_CHAT_SKILL_T *skill = (AI_CHAT_SKILL_T *)msg.data;
        if (OPRT_OK == ai_proc_skill_process(llm, skill)) {
            // FIXME: only support ai trigger
            msg.event = AI_PROC_SKILL_TRIGGER;
            llm->output_cb(&msg, llm->user_data);
        }
    } else if (info->event == AI_CHAT_EVENT_PLAY_CTL_PLAY) {
        // 播放或继续播放
        msg.event = AI_PROC_PLAY_CTL_PLAY;
        llm->output_cb(&msg, llm->user_data);
        TAL_PR_DEBUG("AI_CHAT_EVENT_PLAY_CTL_PLAY");
    }  else if (info->event == AI_CHAT_EVENT_PLAY_CTL_RESUME) {
        // 播放或继续播放
        msg.event = AI_PROC_PLAY_CTL_RESUME;
        llm->output_cb(&msg, llm->user_data);
        TAL_PR_DEBUG("AI_CHAT_EVENT_PLAY_CTL_RESUME");
    } else if (info->event == AI_CHAT_EVENT_PLAY_CTL_PAUSE) {
        // 暂停或停止播放
        msg.event = AI_PROC_PLAY_CTL_PAUSE;
        llm->output_cb(&msg, llm->user_data);
        TAL_PR_DEBUG("AI_CHAT_EVENT_PLAY_CTL_PAUSE");
    } else if (info->event == AI_CHAT_EVENT_PLAY_CTL_REPLAY) {
        // 重新播放
        msg.event = AI_PROC_PLAY_CTL_REPLAY;
        llm->output_cb(&msg, llm->user_data);
        TAL_PR_DEBUG("AI_CHAT_EVENT_PLAY_CTL_REPLAY");
    } else if (info->event == AI_CHAT_EVENT_PLAY_CTL_NEXT ||
               info->event == AI_CHAT_EVENT_PLAY_CTL_SEQUENTIAL ||
               info->event == AI_CHAT_EVENT_PLAY_CTL_SEQUENTIAL_LOOP ||
               info->event == AI_CHAT_EVENT_PLAY_CTL_SINGLE_LOOP) {
        // 播放下一首或顺序播放或循环播放
        msg.event = AI_PROC_PLAY_CTL_NEXT;
        llm->output_cb(&msg, llm->user_data);
        TAL_PR_DEBUG("AI_CHAT_EVENT_PLAY_CTL_NEXT");
    }
    
    return 0;
}


static int ai_proc_audio_process(ty_ai_proc_t *llm,  ai_proc_msg_t *msg)
{
    int rt = OPRT_OK;
    STATIC BOOL_T is_init = FALSE;

    if (!is_init) {
        // init tuya ai chat
#if defined(ENABLE_APP_OPUS_ENCODER) && (ENABLE_APP_OPUS_ENCODER == 1)
        TY_AI_CHAT_UPLOAD_PARA_T upload_para = {
            .encoder = &g_tuya_ai_encoder_opus_ipc,
            .info = {
                .encode_type = AUDIO_CODEC_OPUS,
                .sample_rate = 16000,
                .channels = AUDIO_CHANNELS_MONO,
                .bits_per_sample = 16,
            },
            .tts_cfg = {
                .tts_overwrite = 1,
                .bit_rate = 64000, // 64kbps
                .sample_rate = 16000,
                .format = "mp3",
            },
        };
#else
        TY_AI_CHAT_UPLOAD_PARA_T upload_para = {
            .encoder = NULL,
            .info = {
                .encode_type = AUDIO_CODEC_PCM,
                .sample_rate = 16000,
                .channels = AUDIO_CHANNELS_MONO,
                .bits_per_sample = 16,
            },
            .tts_cfg = {
                .tts_overwrite = 1,
                .bit_rate = 64000, // 64kbps
                .sample_rate = 16000,
                .format = "mp3",
            },
        };
#endif
        TUYA_CALL_ERR_RETURN(tuya_ai_chat_proto_upload_init(&upload_para));
        is_init = TRUE;
    }

    //! 判断是否需要发送第一包
    if (!(llm->status & AI_PROC_AUDIO)) {
        llm->status |= AI_PROC_AUDIO;
        TAL_PR_NOTICE("AI_PROC_AUDIO START");
        TUYA_CALL_ERR_RETURN(tuya_ai_chat_proto_upload_start(true));
        TUYA_CALL_ERR_RETURN(tuya_ai_chat_proto_upload_data(AI_STREAM_START, NULL, 0));
    }

    if ((llm->status & AI_PROC_AUDIO) && msg->data && msg->datalen) {
        TUYA_CALL_ERR_RETURN(tuya_ai_chat_proto_upload_data(AI_STREAM_ING, msg->data, msg->datalen));
    }

    tal_sw_timer_start(llm->asr_timer, LLM_ASR_TIMEOUT, TAL_TIMER_ONCE);

    return OPRT_OK;
}


static int ai_proc_video_process(ty_ai_proc_t *llm,  ai_proc_msg_t *msg)
{
    int rt = OPRT_OK;
    uint32_t  read_size;

#if ENABLE_VIDEO_ONE_FRAME
    if (!(llm->status & AI_PROC_VIDEO)) {
        llm->status |= AI_PROC_VIDEO;
        TAL_PR_NOTICE("AI_PROC_VIDEO START");
        return tuya_ai_video_proto_upload_data(AI_STREAM_START, msg->data, msg->datalen);
    }

    if ((llm->status & AI_PROC_VIDEO)) {
        TAL_PR_NOTICE("AI_PROC_VIDEO DATA");
        TUYA_CALL_ERR_RETURN(tuya_ai_video_proto_upload_data(AI_STREAM_ING, msg->data, msg->datalen));
    }
#else
    tal_mutex_lock(llm->video_mutex);
    read_size = tuya_ring_buff_read(llm->video_ringbuf, llm->video_buf, MAX_INPUT_BUF_SIZE * 2);
    tal_mutex_unlock(llm->video_mutex);

    if (0 == read_size) {
        return OPRT_OK;
    }

    //! 判断是否需要发送第一包
    if (!(llm->status & AI_PROC_VIDEO)) {
        llm->status |= AI_PROC_VIDEO;
        TAL_PR_NOTICE("AI_PROC_VIDEO START");
        TUYA_CALL_ERR_RETURN(tuya_ai_video_proto_upload_data(AI_STREAM_START, llm->video_buf, read_size));
    }

    if ((llm->status & AI_PROC_VIDEO) && read_size) {
        TAL_PR_NOTICE("AI_PROC_VIDEO DATA");
        TUYA_CALL_ERR_RETURN(tuya_ai_video_proto_upload_data(AI_STREAM_ING, llm->video_buf, read_size));
    }
#endif

    return OPRT_OK;
}

static int ai_proc_text_process(ty_ai_proc_t *llm,  ai_proc_msg_t *msg)
{
    int rt = OPRT_OK;

    //! 判断是否需要发送第一包
    if (!(llm->status & (AI_PROC_TEXT | AI_PROC_SKILL))) {
        llm->status |= AI_PROC_TEXT;
        TUYA_CALL_ERR_RETURN(tuya_ai_chat_proto_upload_start_ext(true, msg->data));
        TUYA_CALL_ERR_RETURN(tuya_ai_text_proto_upload_data(AI_STREAM_START, NULL, 0));
        if (msg->event == AI_PROC_SKILL_EVENT) { //! msg->data = request_id
            return OPRT_OK;
        }
    }

    if ((llm->status & AI_PROC_TEXT) && msg->data && msg->datalen) {
        TUYA_CALL_ERR_RETURN(tuya_ai_text_proto_upload_data(AI_STREAM_ING, msg->data, msg->datalen));
    }

    return OPRT_OK;
}

static int ai_proc_finsh_process(ty_ai_proc_t *llm)
{
    int rt = OPRT_OK;

    if (llm->status & AI_PROC_AUDIO) {
        rt = tuya_ai_chat_proto_upload_stop();
        llm->status &= (~AI_PROC_AUDIO) ;
    }

    if (llm->status & AI_PROC_VIDEO) {
        rt |= tuya_ai_video_proto_stop();
        llm->status &= (~AI_PROC_VIDEO) ;
    }

    if (llm->status & AI_PROC_TEXT) {
        rt |= tuya_ai_text_proto_stop();
        llm->status &= (~AI_PROC_TEXT) ;
    }

    rt |= tuya_ai_chat_proto_end();

    tal_mutex_lock(llm->video_mutex);
    tuya_ring_buff_reset(llm->video_ringbuf);
    tal_mutex_unlock(llm->video_mutex);

    ai_proc_msg_t msg = {0};

    msg.event = (rt == OPRT_OK) ? AI_PROC_UPLOAD_DONE : AI_PROC_UPLOAD_FAIL;
    llm->output_cb(&msg, llm->user_data);

    return rt;
}



STATIC VOID ty_ai_proc_task(void* arg)
{
    int rt = OPRT_OK;
    ty_ai_proc_t *llm = (ty_ai_proc_t *)arg;
    ai_proc_msg_t     msg;

    UINT_T timeout = 0xFFFFFFFF;

    while(tal_thread_get_state(llm->task) == THREAD_STATE_RUNNING) {

        tal_queue_fetch(llm->event, &msg, &timeout);

        // TAL_PR_NOTICE("msg.event, %d", msg.event);
        switch (msg.event) {

        case AI_PROC_AUDIO_EVENT:
            // TAL_PR_NOTICE("AI_PROC_AUDIO_EVENT, %d", llm->status);
            ai_proc_audio_process(llm, &msg);
            break;
        
        case AI_PROC_VIDEO_EVENT:
            // TAL_PR_NOTICE("AI_PROC_VIDEO_EVENT, %d", llm->status);
            ai_proc_video_process(llm, &msg);
            break;
        
        case AI_PROC_TEXT_EVENT:
        case AI_PROC_SKILL_EVENT:
            // TAL_PR_NOTICE("AI_PROC_TEXT_EVENT, %d", llm->status);
            ai_proc_text_process(llm, &msg);
            break;

        case AI_PROC_FINSH_EVENT:
            TAL_PR_NOTICE("AI_PROC_FINSH_EVENT, %d", llm->status);
            ai_proc_finsh_process(llm);
            break;

        case AI_PROC_INTERRUPT_EVENT:
            TAL_PR_NOTICE("AI_PROC_INTERRUPT_EVENT, %d", llm->status);
            llm->status = 0;
            tal_sw_timer_stop(llm->asr_timer);
            tal_sw_timer_stop(llm->tts_timer);
            tuya_ai_chat_proto_interrupt();
            break;
        }

        if (msg.data && msg.datalen) {
            tkl_system_psram_free(msg.data);
        }
    }

    TAL_PR_NOTICE("ai llm exit...");
    tkl_thread_release(llm->task);
}


int ty_ai_proc_destroy(ty_ai_proc_t *llm)
{
    if (!llm) {
        return OPRT_OK;
    }

    if (llm->video_buf) {
        tkl_system_psram_free(llm->video_buf);
    }

    if (llm->video_mutex) {
        tal_mutex_release(llm->video_mutex);
    }

    if (llm->event) {
        tal_queue_free(llm->event);
    }

    if (llm->asr_timer) {
        tal_sw_timer_delete(llm->asr_timer);
    }

    if (llm->tts_timer) {
        tal_sw_timer_delete(llm->tts_timer);
    }

    if (llm->task) {
        tal_thread_delete(llm->task);
    }

    return OPRT_OK;
}


ty_ai_proc_t *ty_ai_proc_create(ty_ai_proc_cfg_t *cfg)
{

    int          rt = OPRT_OK;
    ty_ai_proc_t *llm;

    TUYA_CHECK_NULL_RETURN(llm = tal_calloc(1, sizeof(ty_ai_proc_t)), NULL);
    llm->output_cb = cfg->output_cb;
    llm->user_data = cfg->user_data;
    llm->video_buf = tkl_system_psram_malloc(MAX_INPUT_BUF_SIZE*2);
    TUYA_CALL_ERR_GOTO((NULL == llm->video_buf), __failed);
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(MAX_INPUT_RINGBUG_SIZE * 2, OVERFLOW_PSRAM_STOP_TYPE, &llm->video_ringbuf), __failed);
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&llm->video_mutex), __failed);
    TUYA_CALL_ERR_GOTO(tal_queue_create_init(&llm->event, sizeof(ai_proc_msg_t), 100), __failed);
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(ai_proc_asr_timeout, llm, &llm->asr_timer), __failed);
    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(ai_proc_tts_timeout, llm, &llm->tts_timer), __failed);

    TAL_PR_DEBUG("llm %p, llm->output_cb: %p", llm, llm->output_cb);

    return llm;

__failed:
    ty_ai_proc_destroy(llm);

    return NULL;
}

int ty_ai_proc_start(ty_ai_proc_t *llm)
{
    int rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tuya_speaker_service_event_register(DEF_STR_EVENT_AI_CHAT, ai_proc_event_cb, llm));

    //! start thread
    THREAD_CFG_T task_cfg = {1024 * 8, THREAD_PRIO_1, "ty_ai_proc"};
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&llm->task, 
                       NULL, 
                       NULL, 
                       ty_ai_proc_task, 
                       llm, 
                       &task_cfg));

    return rt;
}
