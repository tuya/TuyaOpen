/**
 * @file app_ai_v1_0.c
 * @brief app_ai_v1_0 module is used to
 * @version 0.1
 * @date 2025-03-28
 */

#include "app_ai.h"

#if (ENABLE_TUYA_AI_V2_0 == 0)

#include "app_player.h"
#include "app_chat_bot.h"
#include "tuya_audio_debug.h"

#include "tuya_voice_protocol.h"
#include "tuya_voice_protocol_ws.h"
#include "speaker_upload.h"
#include "speex_encode.h"
#include "wav_encode.h"

#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#ifndef TUYA_WS_REQUEST_ID_MAX_LEN
#define TUYA_WS_REQUEST_ID_MAX_LEN (64)
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t is_online;
    uint8_t need_keep_session;
    MUTEX_HANDLE mutex;

    char session_id[TUYA_VOICE_MESSAGE_ID_MAX_LEN + 1];
    char request_id[TUYA_WS_REQUEST_ID_MAX_LEN];

    APP_AI_MSG_CB msg_cb;
} APP_AI_SESSION_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_AI_SESSION_T sg_ai = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

static void __tuya_voice_play_tts(TUYA_VOICE_TTS_S *tts)
{
    PR_DEBUG("%s", __func__);

    if (NULL == tts) {
        PR_ERR("tts is NULL");
        return;
    }

    tal_mutex_lock(sg_ai.mutex);

    sg_ai.need_keep_session = tts->keep_session;

    if (strlen(tts->session_id) > 0 && tts->keep_session) {
        strncpy(sg_ai.session_id, tts->session_id, sizeof(sg_ai.session_id));
    } else {
        memset(sg_ai.session_id, 0, sizeof(sg_ai.session_id));
    }

    tal_mutex_unlock(sg_ai.mutex);

    return;
}

static void _tuya_voice_custom(char *type, cJSON *json)
{
    APP_AI_SESSION_T *ctx = &sg_ai;

    PR_DEBUG("%s, %s", __func__, type);

    if (json == NULL) {
        PR_ERR("json is NULL");
        return;
    }

    if (ctx->msg_cb == NULL) {
        PR_ERR("msg_cb is NULL");
        return;
    }

    tal_mutex_lock(sg_ai.mutex);

    char *data = cJSON_PrintUnformatted(json);
    PR_DEBUG("json: %s", data);
    cJSON_free(data);

    if (strcmp(type, "response") == 0 && cJSON_GetArraySize(json) == 0) {
        // If response is empty, it indicates an empty voice, play a prompt tone.
        APP_AI_MSG_T ai_msg = {
            .type = APP_AI_MSG_TYPE_TEXT_ASR,
            .data_len = 0,
            .data = NULL,
        };
        sg_ai.msg_cb(&ai_msg);
        goto __EXIT;
    }

    if (strcmp(type, "syncDialogText") == 0 && cJSON_GetObjectItem(json, "id") == NULL) {
        cJSON *node = cJSON_GetObjectItem(json, "text");
        cJSON *text = cJSON_GetObjectItem(json, "text");
        if (text && text->valuestring && strlen(text->valuestring)) {
            APP_AI_MSG_T ai_msg = {
                .type = APP_AI_MSG_TYPE_TEXT_ASR,
                .data_len = strlen(text->valuestring),
                .data = text->valuestring,
            };
            sg_ai.msg_cb(&ai_msg);
        }
    } else if (strcmp(type, "playTts") == 0) {
        PR_DEBUG("playTts");
        cJSON *text = cJSON_GetObjectItem(json, "text");
        if (text && text->valuestring && strlen(text->valuestring)) {
            APP_AI_MSG_T ai_msg = {
                .type = APP_AI_MSG_TYPE_TEXT_NLG,
                .data_len = strlen(text->valuestring),
                .data = text->valuestring,
            };
            sg_ai.msg_cb(&ai_msg);
        }
    }

__EXIT:
    tal_mutex_unlock(sg_ai.mutex);

    return;
}

static void _tuya_voice_stream_player(TUYA_VOICE_STREAM_E type, uint8_t *data, int len)
{
    APP_AI_SESSION_T *ctx = &sg_ai;

    PR_DEBUG("%s, type: %d, len: %d", __func__, type, len);

    if (ctx->is_online == 0) {
        PR_ERR("ai is not online");
        return;
    }

    if (ctx->msg_cb == NULL) {
        PR_ERR("msg_cb is NULL");
        return;
    }

    tal_mutex_lock(sg_ai.mutex);

    switch (type) {
    case TUYA_VOICE_STREAM_START: {
        PR_DEBUG("tts start... requestid=%s", data);
        if (strcmp(ctx->request_id, data) != 0) {
            PR_DEBUG("tts start, request id is not match");
            break;
        }
        // SPK_STAT_CHANGE(APP_SPK_STATUS_START);

        APP_AI_MSG_T ai_msg = {
            .type = APP_AI_MSG_TYPE_AUDIO_START,
            .data_len = len,
            .data = data,
        };
        sg_ai.msg_cb(&ai_msg);

    } break;
    case TUYA_VOICE_STREAM_DATA: {
        APP_AI_MSG_T ai_msg = {
            .type = APP_AI_MSG_TYPE_AUDIO_DATA,
            .data_len = len,
            .data = data,
        };
        sg_ai.msg_cb(&ai_msg);
    } break;
    case TUYA_VOICE_STREAM_STOP: {
        APP_AI_MSG_T ai_msg = {
            .type = APP_AI_MSG_TYPE_AUDIO_STOP,
            .data_len = len,
            .data = data,
        };
        sg_ai.msg_cb(&ai_msg);
    } break;
    case TUYA_VOICE_STREAM_ABORT: {
        // abort
    } break;
    default:
        break;
    }

    tal_mutex_unlock(sg_ai.mutex);

    return;
}

void _tuya_voice_text_stream(TUYA_VOICE_STREAM_E type, uint8_t *data, int len)
{
    PR_DEBUG("%s", __func__);

    return;
}

static OPERATE_RET __app_ai_init(void *data)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("%s...", __func__);

#if defined(TUYA_AUDIO_DEBUG) && (TUYA_AUDIO_DEBUG == 1)
    tuya_audio_debug_init();
#endif

    TUYA_CALL_ERR_RETURN(tuya_voice_proto_start());

    sg_ai.is_online = TRUE;

    return rt;
}

OPERATE_RET app_ai_upload_start(uint8_t int_enable)
{
    OPERATE_RET rt = OPRT_OK;

    APP_AI_SESSION_T *ctx = &sg_ai;

#if defined(TUYA_AUDIO_DEBUG) && (TUYA_AUDIO_DEBUG == 1)
    tuya_audio_debug_start_cb();
#endif

    TUYA_VOICE_AUDIO_FORMAT_E format = TUYA_VOICE_AUDIO_FORMAT_SPEEX;
    SPEAKER_ENCODE_INFO_S param = {
        .encode_type = format,
        .info.channels = 1,
        .info.rate = 16000,
        .info.bits_per_sample = 16,
    };

    static uint8_t speaker_is_init = 0;
    if (speaker_is_init == 0) {
        SPEAKER_UPLOAD_CONFIG_S upload_config = SPEAKER_UPLOAD_CONFIG_FOR_SPEEX();
        memcpy(&upload_config.params, &param, sizeof(param));
        TUYA_CALL_ERR_RETURN(speaker_intf_upload_init(&upload_config));
        PR_NOTICE("speaker_intf_upload_init...ok");
        speaker_is_init = 1;
    }

    if (0 < strlen(ctx->session_id)) {
        strncpy(param.session_id, ctx->session_id, sizeof(param.session_id));
    }

    TUYA_CALL_ERR_RETURN(speaker_intf_upload_media_start(param.session_id));
    tuya_voice_get_current_request_id(ctx->request_id);
    PR_NOTICE("speaker_intf_upload_media_start...ok, request_id=%s", ctx->request_id);
    if (param.session_id == NULL) {
        TUYA_CALL_ERR_RETURN(speaker_intf_upload_media_get_message_id(param.session_id, TUYA_VOICE_MESSAGE_ID_MAX_LEN));
        PR_DEBUG("session_id: %s", param.session_id);
    }
    memcpy(ctx->session_id, param.session_id, sizeof(param.session_id));

    return rt;
}

OPERATE_RET app_ai_upload_data(uint8_t is_first, uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("speaker_intf_upload_media_send, len=%d", len);

#if defined(TUYA_AUDIO_DEBUG) && (TUYA_AUDIO_DEBUG == 1)
    tuya_audio_debug_data_cb(data, len);
#endif

    return speaker_intf_upload_media_send(data, len);
}

OPERATE_RET app_ai_upload_stop(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("speaker_intf_upload_media_stop");

#if defined(TUYA_AUDIO_DEBUG) && (TUYA_AUDIO_DEBUG == 1)
    tuya_audio_debug_stop_cb();
#endif

    return speaker_intf_upload_media_stop(FALSE);
}

OPERATE_RET app_ai_init(APP_AI_MSG_CB msg_cb)
{
    OPERATE_RET rt = OPRT_OK;

    memset(&sg_ai, 0, sizeof(APP_AI_SESSION_T));

    if (NULL == msg_cb) {
        PR_ERR("msg_cb is NULL");
        return OPRT_INVALID_PARM;
    }
    sg_ai.msg_cb = msg_cb;

    // create mutex
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_ai.mutex));

    TUYA_VOICE_CBS_S voice_cbc = {0};
    memset(&voice_cbc, 0, sizeof(voice_cbc));
    voice_cbc.tuya_voice_play_tts = __tuya_voice_play_tts;
    voice_cbc.tuya_voice_custom = _tuya_voice_custom;
    voice_cbc.tuya_voice_tts_stream = _tuya_voice_stream_player;
    voice_cbc.tuya_voice_text_stream = _tuya_voice_text_stream;
    TUYA_CALL_ERR_GOTO(tuya_voice_proto_init(&voice_cbc), __ERR);

    TUYA_CALL_ERR_GOTO(speaker_intf_encode_register(&global_tuya_speex_encoder), __ERR);
    TUYA_CALL_ERR_GOTO(speaker_intf_encode_register(&global_tuya_wav_encoder), __ERR);

    PR_DEBUG("ai session wait for mqtt connected...");

    tal_event_subscribe(EVENT_MQTT_CONNECTED, "app_ai_init", __app_ai_init, SUBSCRIBE_TYPE_ONETIME);

    return rt;

__ERR:
    if (sg_ai.mutex) {
        tal_mutex_release(sg_ai.mutex);
        sg_ai.mutex = NULL;
    }

    return rt;
}

#endif
