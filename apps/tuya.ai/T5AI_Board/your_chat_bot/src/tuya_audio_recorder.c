/**
 * @file tuya_audio_recorder.c
 * @brief Implements audio recorder functionality for handling audio streams
 *
 * This source file provides the implementation of an audio recorder that handles
 * audio stream recording, processing, and uploading. It includes functionality for
 * audio stream management, voice state handling, and integration with audio player
 * and voice protocol modules. The implementation supports audio stream writing,
 * reading, and uploading, as well as session management for multi-round conversations.
 * This file is essential for developers working on IoT applications that require
 * audio recording and processing capabilities.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_gpio.h"
#include "tuya_ringbuf.h"
#include "tkl_thread.h"
#include "tkl_memory.h"

#include "tuya_audio_recorder.h"
#include "tuya_audio_player.h"
#include "tuya_audio_debug.h"

#include "tuya_voice_protocol.h"
#include "tuya_voice_protocol_ws.h"
#include "speaker_upload.h"
#include "speex_encode.h"
#include "wav_encode.h"

#include "tuya_display.h"

typedef struct {
    TUYA_AUDIO_RECORDER_CONFIG_T config;
    BOOL_T is_running;
    BOOL_T is_stop;
    TUYA_TTS_STATE tts_state;
    TUYA_RINGBUFF_T stream_ringbuf;
    MUTEX_HANDLE ringbuf_mutex;
    QUEUE_HANDLE msg_queue;
    BOOL_T is_empty;
    char *curr_session_id;
    BOOL_T curr_is_need_keep; // Used for multi-round conversations, to distinguish skills in multi-round conversations
    uint32_t stream_buf_size; // Audio stream buffer size, in bytes
    uint32_t upload_buf_size; // Slice upload buffer size, in bytes, default is 100ms
    uint8_t *read_buf;
    THREAD_HANDLE task_handle;
} TUYA_AUDIO_RECORDER_CONTEXT;

#ifndef AP_ASSERT_CHECK
#define AP_ASSERT_CHECK(EXPR, MSG, ACTION)                                                                             \
    if (!(EXPR)) {                                                                                                     \
        PR_ERR("ASSERT(%s) has assert failed at %s:%d %s:%s\n", #EXPR, __FILE__, __LINE__, __FUNCTION__, MSG);         \
        ACTION;                                                                                                        \
    }
#endif /* #ifndef AP_ASSERT_CHECK */

#define PCM_STREAM_BUFF_MAX_LEN
#ifndef TUYA_WS_REQUEST_ID_MAX_LEN
#define TUYA_WS_REQUEST_ID_MAX_LEN (64)
#endif

static char s_tts_request_id[TUYA_WS_REQUEST_ID_MAX_LEN] = {0};
static MUTEX_HANDLE s_mutex;
static BOOL_T s_is_stop = TRUE;
TUYA_AUDIO_RECORDER_CONTEXT *s_ctx = NULL;

static void _ai_proc_task(void *arg);
static OPERATE_RET ty_ai_session_id_set(TUYA_AUDIO_RECORDER_HANDLE handle, char *session_id);

static void _request_id_update(char *request_id)
{
    snprintf(s_tts_request_id, TUYA_WS_REQUEST_ID_MAX_LEN, "%s", request_id);
}

static void _request_id_reset(void)
{
    memset(s_tts_request_id, 0, TUYA_WS_REQUEST_ID_MAX_LEN);
}

static char *_get_request_id(void)
{
    return s_tts_request_id;
}

static OPERATE_RET _voice_tts_interrupt(void)
{
    PR_NOTICE("voice upload interrupt...");
    if (_get_request_id()[0] == '\0') {
        PR_NOTICE("no request id, ignore");
        return OPRT_OK;
    }
    return tuya_voice_proto_interrupt();
}

static void __tuya_voice_play_tts(TUYA_VOICE_TTS_S *tts)
{
    /* add play info to playlist for tts */
    AP_ASSERT_CHECK(NULL != tts, "invalid parameter", return;);

    tal_mutex_lock(s_mutex);
    if (s_is_stop) {
        PR_DEBUG("tuya voice play tts, but is stopped");
        tal_mutex_unlock(s_mutex);
        return;
    }

    s_ctx->curr_is_need_keep = tts->keep_session;

    /* current session process */
    if (strlen(tts->session_id) > 0 && tts->keep_session) {
        ty_ai_session_id_set(s_ctx, tts->session_id);
    } else {
        ty_ai_session_id_set(s_ctx, NULL);
    }

    tal_mutex_unlock(s_mutex);

    return;
}

static void _tuya_voice_custom(char *type, cJSON *json)
{
    PR_DEBUG("type: %s", type);

    tal_mutex_lock(s_mutex);
    if (s_is_stop) {
        PR_DEBUG("tuya voice custom, but is stopped");
        tal_mutex_unlock(s_mutex);
        return;
    }

    if (json) {
        char *data = cJSON_PrintUnformatted(json);
        PR_DEBUG("json: %s", data);
        cJSON_free(data);
        if (strcmp(type, "response") == 0 && cJSON_GetArraySize(json) == 0) {
            // If response is empty, it indicates an empty voice, play a prompt tone.
            tuya_audio_player_play_alert(AUDIO_ALART_TYPE_PLEASE_AGAIN, TRUE);
        }

         // syncDialogText without id
        if (strcmp(type, "syncDialogText") == 0 && cJSON_GetObjectItem(json, "id") == NULL) {
            cJSON *node = cJSON_GetObjectItem(json, "text");
            cJSON *text = cJSON_GetObjectItem(json, "text");
            if (text && text->valuestring && strlen(text->valuestring)) {
                tuya_display_send_msg(TY_DISPLAY_TP_HUMAN_CHAT, text->valuestring, strlen(text->valuestring));
            }
        } else if (strcmp(type, "playTts") == 0) {
            PR_DEBUG("playTts");
            cJSON *text = cJSON_GetObjectItem(json, "text");
            if (text && text->valuestring && strlen(text->valuestring)) {
                tuya_display_send_msg(TY_DISPLAY_TP_AI_CHAT, text->valuestring, strlen(text->valuestring));
            }
        }

    }
    tal_mutex_unlock(s_mutex);
    return;
}

static void _tuya_voice_stream_player(TUYA_VOICE_STREAM_E type, uint8_t *data, int len)
{
    int ret = 0;
    const char *cur_request_id = NULL;

    tal_mutex_lock(s_mutex);
    if (s_is_stop) {
        // PR_DEBUG("tuya voice stream player, but is stopped");
        tal_mutex_unlock(s_mutex);
        return;
    }
    cur_request_id = _get_request_id();
    // send data to socket
    switch (type) {
    case TUYA_VOICE_STREAM_START:
        PR_DEBUG("tts start... requestid=%s", (char *)data);
        if (strcmp(cur_request_id, data) != 0) {
            PR_DEBUG("tts start, request id is not match");
            break;
        }

        s_ctx->tts_state = TTS_STATE_STREAM_START;
        if (tuya_audio_player_is_playing()) {
            PR_DEBUG("tts start, player is playing, stop it first");
            tuya_audio_player_stop();
        }

        tuya_audio_player_start();
        break;

    case TUYA_VOICE_STREAM_DATA:
        if (s_ctx->tts_state < TTS_STATE_STREAM_START) {
            PR_DEBUG("tts data, streaming flag is not set");
            break;
        }
        s_ctx->tts_state = TTS_STATE_STREAM_DATA;

        PR_DEBUG("tts data... len=%d, used size=%d", len, tuya_audio_player_stream_get_size());
        while (tuya_audio_player_is_playing() && len > 0) {
            ret = tuya_audio_player_stream_write((char *)data, len);
            if (ret < 0) {
                PR_ERR("tkl_player_start_stream_write failed, ret=%d", ret);
                break;
            }
            if (ret == 0) {
                tal_system_sleep(10);
            } else {
                len -= ret;
                data += ret;
            }
        }

        if (ret < 0) {
            // stream buffer error, drop all pending data
            s_ctx->tts_state = TTS_STATE_STREAM_IDLE;
            PR_DEBUG("ret < 0, tts_state: %d", s_ctx->tts_state);
        }
        break;

    case TUYA_VOICE_STREAM_STOP: {
        if (s_ctx->tts_state < TTS_STATE_STREAM_DATA) {
            PR_DEBUG("tts stop, streaming flag is not set");
            break;
        }

        PR_DEBUG("tts stop...");

        tuya_audio_player_stream_write(NULL, 0);
        s_ctx->tts_state = TTS_STATE_STREAM_IDLE;
        break;
    }

    case TUYA_VOICE_STREAM_ABORT: {
        if (s_ctx->tts_state < TTS_STATE_STREAM_DATA) {
            PR_DEBUG("tts stop, streaming flag is not set");
            break;
        }
        PR_DEBUG("tts abort... ");

        tuya_audio_player_stop();
        _request_id_reset();
        s_ctx->tts_state = TTS_STATE_STREAM_IDLE;
        break;
    }

    default:
        break;
    }
    tal_mutex_unlock(s_mutex);
    return;
}

void _tuya_voice_text_stream(TUYA_VOICE_STREAM_E type, uint8_t *data, int len)
{


    switch (type) {
    case TUYA_VOICE_STREAM_START: {
    } break;

    case TUYA_VOICE_STREAM_DATA: {
        PR_DEBUG("voice stream data... %s", data);
    } break;

    case TUYA_VOICE_STREAM_STOP: {
    } break;
    }
    return;
}

static OPERATE_RET _tuya_voice_register_extra_mqt_cb(void *data)
{
    OPERATE_RET rt = OPRT_OK;
    static BOOL_T registed = FALSE;

    if (registed) {
        return OPRT_OK;
    }

    tuya_audio_player_play_alert(AUDIO_ALART_TYPE_NETWORK_CONNECTED, TRUE);

    rt = tuya_voice_proto_start();
    if (rt != OPRT_OK) {
        PR_ERR("tuya_voice_proto_start failed");
        return rt;
    }

    registed = TRUE;

    return OPRT_OK;
}

static OPERATE_RET _tuya_voice_register_extra_reset_cb(void *data)
{
    tuya_voice_proto_del_domain_name();
    return OPRT_OK;
}

static OPERATE_RET _tuya_player_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_VOICE_CBS_S voice_cbc = {0};
    memset(&voice_cbc, 0, sizeof(voice_cbc));
    voice_cbc.tuya_voice_play_tts = __tuya_voice_play_tts;
    voice_cbc.tuya_voice_custom = _tuya_voice_custom;
    voice_cbc.tuya_voice_tts_stream = _tuya_voice_stream_player;
    voice_cbc.tuya_voice_text_stream = _tuya_voice_text_stream;
    rt = tuya_voice_proto_init(&voice_cbc);
    if (rt != OPRT_OK) {
        PR_ERR("tuya_voice_proto_init failed");
        return rt;
    }

    // Subscribe to events
    TUYA_CALL_ERR_RETURN(tal_event_subscribe(EVENT_MQTT_CONNECTED, "tts_player", _tuya_voice_register_extra_mqt_cb,
                                             SUBSCRIBE_TYPE_NORMAL));
    TUYA_CALL_ERR_RETURN(
        tal_event_subscribe(EVENT_RESET, "tts_player", _tuya_voice_register_extra_reset_cb, SUBSCRIBE_TYPE_NORMAL));

    PR_DEBUG("_tuya_player_init end");
    return rt;
}

static OPERATE_RET _tuya_player_uninit(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("_tuya_player_uninit...");
    tal_event_unsubscribe(EVENT_MQTT_CONNECTED, "tts_player", NULL);
    tal_event_unsubscribe(EVENT_RESET, "tts_player", NULL);

    PR_DEBUG("tuya_voice_proto_stop...");
    TUYA_CALL_ERR_LOG(tuya_voice_proto_stop());
    PR_DEBUG("tuya_voice_proto_stop done");
    PR_DEBUG("tuya_voice_proto_deinit...");
    TUYA_CALL_ERR_LOG(tuya_voice_proto_deinit());
    PR_DEBUG("tuya_voice_proto_deinit done");

    return rt;
}

/**
 * @brief Initializes the Tuya Audio Recorder.
 *
 * This function initializes the audio recorder by creating a mutex, initializing the player,
 * registering the audio encoders, and setting up the internal context.
 *
 * @return OPERATE_RET - The return code of the initialization function.
 *         OPRT_OK - Success.
 *         OPRT_ERROR - An error occurred during initialization.
 */
OPERATE_RET tuya_audio_recorder_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    static BOOL_T is_init = FALSE;

    if (is_init) {
        return OPRT_OK;
    }

    PR_NOTICE("tuya_audio_recorder init...");

    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&s_mutex));
    TUYA_CALL_ERR_GOTO(_tuya_player_init(), error);
    TUYA_CALL_ERR_GOTO(speaker_intf_encode_register(&global_tuya_speex_encoder), error);
    TUYA_CALL_ERR_GOTO(speaker_intf_encode_register(&global_tuya_wav_encoder), error);

    is_init = TRUE;
    return OPRT_OK;

error:
    if (s_mutex) {
        tal_mutex_release(s_mutex);
    }
    _tuya_player_uninit();
    return rt;
}

/**
 * @brief Starts the Tuya Audio Recorder with the specified configuration.
 *
 * This function starts the audio recorder, allocating necessary buffers and starting a task
 * to process the audio stream. It returns a handle to the recorder context.
 *
 * @param handle - A pointer to the Tuya Audio Recorder handle.
 * @param cfg - A pointer to the Tuya Audio Recorder configuration structure.
 * @return OPERATE_RET - The return code of the start function.
 *         OPRT_OK - Success.
 *         OPRT_INVALID_PARM - Invalid parameter.
 *         OPRT_ERROR - An error occurred during start.
 */
OPERATE_RET tuya_audio_recorder_start(TUYA_AUDIO_RECORDER_HANDLE *handle, const TUYA_AUDIO_RECORDER_CONFIG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_AUDIO_RECORDER_CONTEXT *ctx = NULL;

    PR_NOTICE("tuya_audio_recorder start...");
    if (cfg == NULL) {
        PR_ERR("invalid param");
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(s_mutex);
    ctx = tkl_system_psram_malloc(sizeof(TUYA_AUDIO_RECORDER_CONTEXT));
    if (ctx == NULL) {
        PR_ERR("malloc failed");
        goto error;
    }
    memset(ctx, 0, sizeof(TUYA_AUDIO_RECORDER_CONTEXT));
    ctx->config = *cfg;

    // calc buffer size
    ctx->upload_buf_size = cfg->upload_slice_duration * cfg->sample_rate * cfg->sample_bits * cfg->channel / 8 / 1000;
    PR_DEBUG("upload buf size: %d", ctx->upload_buf_size);
    ctx->read_buf = tkl_system_psram_malloc(ctx->upload_buf_size);
    if (ctx->read_buf == NULL) {
        PR_ERR("malloc failed");
        goto error;
    }

    ctx->stream_buf_size = cfg->record_duration * cfg->sample_rate * cfg->sample_bits * cfg->channel / 8 / 1000;
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(ctx->stream_buf_size, OVERFLOW_PSRAM_STOP_TYPE, &ctx->stream_ringbuf),
                       error);
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&ctx->ringbuf_mutex), error);
    TUYA_CALL_ERR_GOTO(tal_queue_create_init(&ctx->msg_queue, sizeof(int), 8), error);

    ctx->is_running = TRUE;
    TUYA_CALL_ERR_GOTO(
        tkl_thread_create_in_psram(&ctx->task_handle, "ai_proc_task", 1024 * 4 * 4, THREAD_PRIO_2, _ai_proc_task, ctx),
        error);
    PR_NOTICE("ai_proc_task create success");
    s_ctx = *handle = ctx;
    s_is_stop = FALSE;
    tal_mutex_unlock(s_mutex);

    return OPRT_OK;

error:
    tuya_audio_recorder_stop(ctx);
    tal_mutex_unlock(s_mutex);
    return rt;
}

/**
 * @brief Stops the Tuya Audio Recorder.
 *
 * This function stops the audio recorder, freeing allocated resources and stopping the processing
 * task.
 *
 * @param handle - A pointer to the Tuya Audio Recorder handle.
 */
void tuya_audio_recorder_stop(TUYA_AUDIO_RECORDER_HANDLE handle)
{
    PR_NOTICE("tuya_audio_recorder stop...");
    tal_mutex_lock(s_mutex);
    _voice_tts_interrupt();

    TUYA_AUDIO_RECORDER_CONTEXT *ctx = (TUYA_AUDIO_RECORDER_CONTEXT *)handle;
    if (ctx == NULL) {
        tal_mutex_unlock(s_mutex);
        return;
    }

    if (ctx->task_handle) {
        ctx->is_running = FALSE;
        while (!ctx->is_stop) {
            tal_system_sleep(10);
        }
    }

    if (ctx->ringbuf_mutex) {
        tal_mutex_release(ctx->ringbuf_mutex);
    }
    if (ctx->msg_queue) {
        tal_queue_free(ctx->msg_queue);
    }
    if (ctx->stream_ringbuf) {
        tuya_ring_buff_free(ctx->stream_ringbuf);
    }
    if (ctx->read_buf) {
        tkl_system_psram_free(ctx->read_buf);
    }
    tkl_system_psram_free(ctx);
    s_ctx = NULL;
    s_is_stop = TRUE;
    tal_mutex_unlock(s_mutex);
    PR_NOTICE("tuya_audio_recorder stop success");
    return;
}

/**
 * @brief Writes audio data to the audio stream.
 *
 * This function writes audio data to the internal buffer of the audio recorder.
 *
 * @param handle - A pointer to the Tuya Audio Recorder handle.
 * @param buf - A pointer to the buffer containing the audio data.
 * @param len - The length of the audio data in bytes.
 * @return int - The number of bytes written to the audio stream.
 *         OPRT_COM_ERROR - An error occurred during writing.
 */
int tuya_audio_recorder_stream_write(TUYA_AUDIO_RECORDER_HANDLE handle, const char *buf, uint32_t len)
{
    int ret = 0, write_size = 0;
    TUYA_AUDIO_RECORDER_CONTEXT *ctx = (TUYA_AUDIO_RECORDER_CONTEXT *)handle;
    tal_mutex_lock(ctx->ringbuf_mutex);
    ret = tuya_ring_buff_write(ctx->stream_ringbuf, buf, len);
    tal_mutex_unlock(ctx->ringbuf_mutex);
    if (ret != len) {
        PR_ERR("tuya_ring_buff_write failed, ret=%d, used_size=%d, free_size=%d", ret,
               tuya_ring_buff_used_size_get(ctx->stream_ringbuf), tuya_ring_buff_free_size_get(ctx->stream_ringbuf));
        return OPRT_COM_ERROR;
    }

    write_size = ret;
    return write_size;
}

/**
 * @brief Reads audio data from the audio stream.
 *
 * This function reads audio data from the internal buffer of the audio recorder.
 *
 * @param handle - A pointer to the Tuya Audio Recorder handle.
 * @param buf - A pointer to the buffer to store the audio data.
 * @param len - The length of the audio data buffer.
 * @return int - The number of bytes read from the audio stream.
 *         OPRT_COM_ERROR - An error occurred during reading.
 */
int tuya_audio_recorder_stream_read(TUYA_AUDIO_RECORDER_HANDLE handle, char *buf, uint32_t len)
{
    int ret = 0, read_size = 0;
    TUYA_AUDIO_RECORDER_CONTEXT *ctx = (TUYA_AUDIO_RECORDER_CONTEXT *)handle;
    tal_mutex_lock(ctx->ringbuf_mutex);
    ret = tuya_ring_buff_read(ctx->stream_ringbuf, buf, len);
    tal_mutex_unlock(ctx->ringbuf_mutex);
    if (ret < 0) {
        PR_ERR("tuya_ring_buff_read failed, ret=%d", ret);
        return OPRT_COM_ERROR;
    }

    read_size = ret;
    return read_size;
}

/**
 * @brief Clears the audio stream buffer.
 *
 * This function resets the internal buffer of the audio recorder.
 *
 * @param handle - A pointer to the Tuya Audio Recorder handle.
 * @return OPERATE_RET - The return code of the clear function.
 *         OPRT_OK - Success.
 */
OPERATE_RET tuya_audio_recorder_stream_clear(TUYA_AUDIO_RECORDER_HANDLE handle)
{
    OPERATE_RET ret;
    TUYA_AUDIO_RECORDER_CONTEXT *ctx = (TUYA_AUDIO_RECORDER_CONTEXT *)handle;
    tal_mutex_lock(ctx->ringbuf_mutex);
    ret = tuya_ring_buff_reset(ctx->stream_ringbuf);
    tal_mutex_unlock(ctx->ringbuf_mutex);
    return ret;
}

/**
 * @brief Gets the current size of the audio stream buffer.
 *
 * This function returns the number of bytes used in the internal buffer of the audio recorder.
 *
 * @param handle - A pointer to the Tuya Audio Recorder handle.
 * @return int - The size of the audio stream buffer in bytes.
 */
int tuya_audio_recorder_stream_get_size(TUYA_AUDIO_RECORDER_HANDLE handle)
{
    int size = 0;
    TUYA_AUDIO_RECORDER_CONTEXT *ctx = (TUYA_AUDIO_RECORDER_CONTEXT *)handle;
    tal_mutex_lock(ctx->ringbuf_mutex);
    size = tuya_ring_buff_used_size_get(ctx->stream_ringbuf);
    tal_mutex_unlock(ctx->ringbuf_mutex);
    return size;
}

/**
 * @brief Posts a voice status message.
 *
 * This function posts a voice status message to the internal message queue.
 *
 * @param handle - A pointer to the Tuya Audio Recorder handle.
 * @param stat - The voice status to be posted.
 * @return OPERATE_RET - The return code of the post function.
 *         OPRT_OK - Success.
 *         OPRT_COM_ERROR - An error occurred during posting.
 */
OPERATE_RET ty_ai_voice_stat_post(TUYA_AUDIO_RECORDER_HANDLE handle, TUYA_AUDIO_VOICE_STATE stat)
{
    OPERATE_RET ret = OPRT_OK;
    tal_mutex_lock(s_mutex);
    TUYA_AUDIO_RECORDER_CONTEXT *ctx = (TUYA_AUDIO_RECORDER_CONTEXT *)handle;
    if (ctx == NULL) {
        tal_mutex_unlock(s_mutex);
        return OPRT_COM_ERROR;
    }
    int msg = stat;
    ret = tal_queue_post(ctx->msg_queue, &msg, 0);
    tal_mutex_unlock(s_mutex);
    return ret;
}
static OPERATE_RET ty_ai_voice_stat_fetch(TUYA_AUDIO_RECORDER_HANDLE handle, TUYA_AUDIO_VOICE_STATE *stat,
                                          uint32_t timeout)
{
    OPERATE_RET ret = OPRT_OK;
    TUYA_AUDIO_RECORDER_CONTEXT *ctx = (TUYA_AUDIO_RECORDER_CONTEXT *)handle;
    if (ctx == NULL) {
        return OPRT_COM_ERROR;
    }
    int msg;
    ret = tal_queue_fetch(ctx->msg_queue, &msg, timeout);
    if (ret != OPRT_OK) {
        return ret;
    }
    *stat = msg;
    return OPRT_OK;
}
static BOOL_T ty_ai_voice_need_keep_session(TUYA_AUDIO_RECORDER_HANDLE handle)
{
    TUYA_AUDIO_RECORDER_CONTEXT *ctx = (TUYA_AUDIO_RECORDER_CONTEXT *)handle;
    return ctx->curr_is_need_keep;
}
static char *ty_ai_get_session_id_get(TUYA_AUDIO_RECORDER_HANDLE handle)
{
    TUYA_AUDIO_RECORDER_CONTEXT *ctx = (TUYA_AUDIO_RECORDER_CONTEXT *)handle;
    if (ctx == NULL)
        return NULL;

    PR_DEBUG("%s: current session id [%s]", __func__, ctx->curr_session_id);
    return ctx->curr_session_id;
}

static OPERATE_RET ty_ai_session_id_set(TUYA_AUDIO_RECORDER_HANDLE handle, char *session_id)
{
    TUYA_AUDIO_RECORDER_CONTEXT *ctx = (TUYA_AUDIO_RECORDER_CONTEXT *)handle;
    if (ctx == NULL) {
        return OPRT_COM_ERROR;
    }
    if (ctx->curr_session_id) {
        tal_free(ctx->curr_session_id);
        ctx->curr_session_id = NULL;
    }

    if (session_id) {
        ctx->curr_session_id = tal_malloc(strlen(session_id) + 1);
        memset(ctx->curr_session_id, 0, strlen(session_id) + 1);
        if (ctx->curr_session_id == NULL)
            return OPRT_MALLOC_FAILED;
        strncpy(ctx->curr_session_id, session_id, strlen(session_id));
    }
    PR_DEBUG("%s: current session id [%s]", __func__, ctx->curr_session_id != NULL ? ctx->curr_session_id : "null");
    return OPRT_OK;
}
static OPERATE_RET _upload_start(TUYA_AUDIO_RECORDER_CONTEXT *ctx)
{
    OPERATE_RET rt = OPRT_OK;

    char *session_id = NULL;
    static BOOL_T is_init = FALSE;

#if defined(TUYA_AUDIO_DEBUG) && (TUYA_AUDIO_DEBUG == 1)
    tuya_audio_debug_start_cb();
#endif

    BOOL_T is_region_ay = TRUE;

    TUYA_VOICE_AUDIO_FORMAT_E format = is_region_ay ? TUYA_VOICE_AUDIO_FORMAT_SPEEX : TUYA_VOICE_AUDIO_FORMAT_WAV;
    SPEAKER_ENCODE_INFO_S param = {
        .encode_type = format,
        .info.channels = 1,
        .info.rate = ctx->config.sample_rate,
        .info.bits_per_sample = ctx->config.sample_bits,
    };

    if (!is_init) {
        SPEAKER_UPLOAD_CONFIG_S upload_config = SPEAKER_UPLOAD_CONFIG_FOR_SPEEX();
        if (!is_region_ay) {
            memset(&upload_config, 0, sizeof(upload_config)); // WAV
        }

        memcpy(&upload_config.params, &param, sizeof(param));
        TUYA_CALL_ERR_RETURN(speaker_intf_upload_init(&upload_config));
        PR_NOTICE("tuya_voice_upload_init...ok");
        is_init = TRUE;
    }

    session_id = ty_ai_get_session_id_get(ctx);
    if (session_id)
        strncpy(param.session_id, session_id, TUYA_WS_REQUEST_ID_MAX_LEN);

    TUYA_CALL_ERR_RETURN(speaker_intf_upload_media_start(param.session_id));

    char request_id[TUYA_WS_REQUEST_ID_MAX_LEN] = {0};
    rt = tuya_voice_get_current_request_id(request_id);
    if (rt != OPRT_OK)
        PR_ERR("tuya_voice_get_current_request_id failed, ret=%d", rt);

    PR_NOTICE("tuya_voice_upload_start...ok, request_id=%s", request_id);

    // update request id
    _request_id_update(request_id);
    if (param.session_id == NULL) {
        TUYA_CALL_ERR_RETURN(speaker_intf_upload_media_get_message_id(param.session_id, TUYA_VOICE_MESSAGE_ID_MAX_LEN));
        PR_DEBUG("session_id: %s", param.session_id);
    }
    ty_ai_session_id_set(ctx, NULL);

    return OPRT_OK;
}

static OPERATE_RET _upload_proc(TUYA_AUDIO_RECORDER_CONTEXT *ctx, BOOL_T need_force_upload)
{
    OPERATE_RET rt = OPRT_OK;
    int stream_size = 0;
    int read_size = 0;

    stream_size = tuya_audio_recorder_stream_get_size(ctx);
    if (stream_size < ctx->upload_buf_size && !need_force_upload) {
        return OPRT_OK; // no enough data
    }

    read_size = tuya_audio_recorder_stream_read(ctx, (char *)ctx->read_buf, ctx->upload_buf_size);
    if (read_size < 0) {
        PR_ERR("tuya_audio_recorder_stream_read failed, ret=%d", read_size);
    }

#if defined(TUYA_AUDIO_DEBUG) && (TUYA_AUDIO_DEBUG == 1)
    tuya_audio_debug_data_cb(ctx->read_buf, read_size);
#endif

    // upload data
    PR_NOTICE("speaker_intf_upload_media_send, len=%d", read_size);
    TUYA_CALL_ERR_RETURN(speaker_intf_upload_media_send(ctx->read_buf, read_size));
    return OPRT_OK;
}

static OPERATE_RET _upload_stop(TUYA_AUDIO_RECORDER_CONTEXT *ctx, BOOL_T force_stop)
{
    OPERATE_RET ret = OPRT_OK;
    if (!force_stop) {
        // upload remain data
        while (tuya_audio_recorder_stream_get_size(ctx) > 0) {
            ret = _upload_proc(ctx, TRUE);
            if (ret != OPRT_OK) {
                PR_ERR("_upload_proc failed, ret=%d", ret);
                break;
            }
        }
    }
    // stop upload
    ret = speaker_intf_upload_media_stop(force_stop);
    if (ret != OPRT_OK) {
        PR_ERR("speaker_intf_upload_media_stop failed, ret=%d", ret);
    }

#if defined(TUYA_AUDIO_DEBUG) && (TUYA_AUDIO_DEBUG == 1)
    tuya_audio_debug_stop_cb();
#endif

    return ret;
}

static void _ai_proc_task(void *arg)
{
    OPERATE_RET ret = OPRT_OK;
    TUYA_AUDIO_RECORDER_CONTEXT *ctx = (TUYA_AUDIO_RECORDER_CONTEXT *)arg;

    TUYA_AUDIO_VOICE_STATE stat = VOICE_STATE_IN_SILENCE;
    PR_NOTICE("ai_proc start...");
    PR_NOTICE("ctx = %p", ctx);

    while (ctx->is_running) {
        // fetch voice state
        TUYA_AUDIO_VOICE_STATE cur_stat = stat;
        ret = ty_ai_voice_stat_fetch(ctx, &cur_stat, stat == VOICE_STATE_IN_VOICE ? 30 : 100);
        if (ret != OPRT_OK && stat != VOICE_STATE_IN_VOICE) {
            continue;
        }
        if (ret == OPRT_OK && cur_stat != stat) {
            PR_NOTICE("stat changed: %d->%d", stat, cur_stat);
            stat = cur_stat;
        }
        switch (stat) {
        case VOICE_STATE_IN_SILENCE:
            PR_NOTICE("voice silence...");
            _voice_tts_interrupt(); // stop current tts
            _request_id_reset();
            break;

        case VOICE_STATE_IN_START:
            PR_NOTICE("voice start...");
            s_ctx->tts_state = TTS_STATE_STREAM_IDLE;
            ret = _upload_start(ctx);
            break;

        case VOICE_STATE_IN_VOICE:
            ret = _upload_proc(ctx, FALSE);
            break;

        case VOICE_STATE_IN_STOP:
            PR_NOTICE("voice stop...");
            ret = _upload_stop(ctx, FALSE);
            break;

        case VOICE_STATE_IN_RESUME:
            PR_NOTICE("voice resume...");
            break;
        default:
            break;
        }
    }

    PR_NOTICE("ai_proc exit...");
    ctx->is_stop = TRUE;
    tkl_thread_release(ctx->task_handle);
}
