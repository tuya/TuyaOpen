/**
 * @file skill_cloudevent.c
 * @brief Cloud event skill implementation.
 *
 * This file provides functions for parsing and processing cloud events,
 * including TTS playback commands (playTts and alert).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
#include "svc_ai_player.h"
#include "ai_audio_player.h"
#endif

#include "skill_cloudevent.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/
static bool __json_is_string(const cJSON *item)
{
    return item && cJSON_IsString(item) && item->valuestring != NULL;
}

static const char *__json_get_string(const cJSON *item)
{
    return __json_is_string(item) ? item->valuestring : NULL;
}

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
static OPERATE_RET __ai_parse_playtts_free(AI_AUDIO_PLAY_TTS_T *playtts);

/**
 * @brief Parse audio codec type from format string.
 *
 * @param format Format string (e.g., "mp3", "wav", "speex", "opus", "oggopus").
 * @return AI_AUDIO_CODEC_E Audio codec type.
 */
static AI_AUDIO_CODEC_E __parse_get_codec_type(char *format)
{
    AI_AUDIO_CODEC_E fmt = AI_AUDIO_CODEC_MP3;

    if (format == NULL) {
        PR_ERR("decode type is NULL");
        return AI_AUDIO_CODEC_MAX;
    }

    if (strcmp(format, "mp3") == 0) {
        fmt = AI_AUDIO_CODEC_MP3;
    } else if (strcmp(format, "wav") == 0) {
        fmt = AI_AUDIO_CODEC_WAV;
    } else if (strcmp(format, "speex") == 0) {
        fmt = AI_AUDIO_CODEC_SPEEX;
    } else if (strcmp(format, "opus") == 0) {
        fmt = AI_AUDIO_CODEC_OPUS;
    } else if (strcmp(format, "oggopus") == 0) {
        fmt = AI_AUDIO_CODEC_OGGOPUS;
    } else {
        PR_ERR("decode type invald:%s", format);
        fmt = AI_AUDIO_CODEC_MAX;
    }

    return fmt;
}

/**
 * @brief Parse TTS playback data from JSON.
 *
 * @param action Action string ("playTts" or "alert").
 * @param json JSON object containing TTS data.
 * @param playtts Pointer to store parsed TTS structure.
 * @return OPERATE_RET Operation result code.
 */
static OPERATE_RET __ai_parse_playtts(CONST char *action, cJSON *json, AI_AUDIO_PLAY_TTS_T **playtts)
{
    OPERATE_RET rt = OPRT_OK;
    cJSON *node = NULL;
    AI_AUDIO_PLAY_TTS_T *playtts_ptr;

    TUYA_CHECK_NULL_RETURN(action, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(json, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(playtts, OPRT_INVALID_PARM);

    playtts_ptr = tal_malloc(sizeof(AI_AUDIO_PLAY_TTS_T));
    TUYA_CHECK_NULL_RETURN(playtts_ptr, OPRT_MALLOC_FAILED);

    memset(playtts_ptr, 0, sizeof(AI_AUDIO_PLAY_TTS_T));

    cJSON *tts = cJSON_GetObjectItem(json, "tts");
    if (tts != NULL && !cJSON_IsObject(tts)) {
        PR_ERR("tts field type invalid");
        rt = OPRT_CJSON_GET_ERR;
        goto __error;
    }

    if (tts) {
        playtts_ptr->tts.tts_type = AI_TTS_TYPE_NORMAL;
        if (strcmp(action, "alert") == 0) {
            playtts_ptr->tts.tts_type = AI_TTS_TYPE_ALERT;
        }
        node = cJSON_GetObjectItem(tts, "url");
        if (__json_is_string(node)) {
            playtts_ptr->tts.url = mm_strdup(node->valuestring);
            if (!playtts_ptr->tts.url) {
                PR_ERR("mm_strdup tts url fail");
                rt = OPRT_MALLOC_FAILED;
                goto __error;
            }
        } else if (node != NULL) {
            PR_ERR("tts url type invalid");
            rt = OPRT_CJSON_GET_ERR;
            goto __error;
        }

        node = cJSON_GetObjectItem(tts, "requestBody");
        if (__json_is_string(node)) {
            playtts_ptr->tts.req_body = mm_strdup(node->valuestring);
            if (!playtts_ptr->tts.req_body) {
                PR_ERR("mm_strdup tts req_body fail");
                rt = OPRT_MALLOC_FAILED;
                goto __error;
            }
        } else if (node != NULL) {
            PR_ERR("tts requestBody type invalid");
            rt = OPRT_CJSON_GET_ERR;
            goto __error;
        } else {
            playtts_ptr->tts.req_body = NULL;
        }

        playtts_ptr->tts.http_method = AI_HTTP_METHOD_GET;
        node = cJSON_GetObjectItem(tts, "requestType");
        if (__json_is_string(node)) {
            if (strcmp(node->valuestring, "post") == 0) {
                playtts_ptr->tts.http_method = AI_HTTP_METHOD_POST;
            }
        } else if (node != NULL) {
            PR_ERR("tts requestType type invalid");
            rt = OPRT_CJSON_GET_ERR;
            goto __error;
        }

        node = cJSON_GetObjectItem(tts, "format");
        if (__json_is_string(node)) {
            playtts_ptr->tts.format = __parse_get_codec_type(node->valuestring);
            if (playtts_ptr->tts.format == AI_AUDIO_CODEC_MAX) {
                rt = OPRT_CJSON_GET_ERR;
                goto __error;
            }
        } else if (node != NULL) {
            PR_ERR("tts format type invalid");
            rt = OPRT_CJSON_GET_ERR;
            goto __error;
        }
    }

    cJSON *bgmusic = cJSON_GetObjectItem(json, "bgMusic");
    if (bgmusic != NULL && !cJSON_IsObject(bgmusic)) {
        PR_ERR("bgMusic field type invalid");
        rt = OPRT_CJSON_GET_ERR;
        goto __error;
    }

    if (bgmusic) {
        node = cJSON_GetObjectItem(bgmusic, "url");
        if (__json_is_string(node)) {
            playtts_ptr->bg_music.url = mm_strdup(node->valuestring);
            if (!playtts_ptr->bg_music.url) {
                PR_ERR("mm_strdup bgmusic url fail");
                rt = OPRT_MALLOC_FAILED;
                goto __error;
            }
        } else if (node != NULL) {
            PR_ERR("bgMusic url type invalid");
            rt = OPRT_CJSON_GET_ERR;
            goto __error;
        }

        node = cJSON_GetObjectItem(bgmusic, "requestBody");
        if (__json_is_string(node)) {
            playtts_ptr->bg_music.req_body = mm_strdup(node->valuestring);
            if (!playtts_ptr->bg_music.req_body) {
                PR_ERR("mm_strdup bgmusic req_body fail");
                rt = OPRT_MALLOC_FAILED;
                goto __error;
            }
        } else if (node != NULL) {
            PR_ERR("bgMusic requestBody type invalid");
            rt = OPRT_CJSON_GET_ERR;
            goto __error;
        } else {
            playtts_ptr->bg_music.req_body = NULL;
        }
        playtts_ptr->bg_music.http_method = AI_HTTP_METHOD_GET;
        node = cJSON_GetObjectItem(bgmusic, "requestType");
        if (__json_is_string(node)) {
            if (strcmp(node->valuestring, "post") == 0) {
                playtts_ptr->bg_music.http_method = AI_HTTP_METHOD_POST;
            }
        } else if (node != NULL) {
            PR_ERR("bgMusic requestType type invalid");
            rt = OPRT_CJSON_GET_ERR;
            goto __error;
        }

        node = cJSON_GetObjectItem(bgmusic, "format");
        if (__json_is_string(node)) {
            playtts_ptr->bg_music.format = __parse_get_codec_type(node->valuestring);
            if (playtts_ptr->bg_music.format == AI_AUDIO_CODEC_MAX) {
                rt = OPRT_CJSON_GET_ERR;
                goto __error;
            }
        } else if (node != NULL) {
            PR_ERR("bgMusic format type invalid");
            rt = OPRT_CJSON_GET_ERR;
            goto __error;
        }

        node = cJSON_GetObjectItem(bgmusic, "duration");
        if (node && cJSON_IsNumber(node)) {
            playtts_ptr->bg_music.duration = cJSON_GetNumberValue(node);
        } else if (node != NULL) {
            PR_ERR("bgMusic duration type invalid");
            rt = OPRT_CJSON_GET_ERR;
            goto __error;
        }
    }

    *playtts = playtts_ptr;
    return OPRT_OK;

__error:
    __ai_parse_playtts_free(playtts_ptr);
    *playtts = NULL;
    return rt;
}

/**
 * @brief Free TTS playback structure and release allocated memory.
 *
 * @param playtts Pointer to TTS structure to free.
 * @return OPERATE_RET Operation result code.
 */
static OPERATE_RET __ai_parse_playtts_free(AI_AUDIO_PLAY_TTS_T *playtts)
{
    if (!playtts) {
        return OPRT_OK;
    }

    if (playtts->tts.url) {
        tal_free(playtts->tts.url);
    }
    if (playtts->tts.req_body) {
        tal_free(playtts->tts.req_body);
    }
    if (playtts->bg_music.url) {
        tal_free(playtts->bg_music.url);
    }
    if (playtts->bg_music.req_body) {
        tal_free(playtts->bg_music.req_body);
    }

    tal_free(playtts);
    return OPRT_OK;
}

#endif

/**
 * @brief Parse and process cloud event from JSON.
 *
 * @param json JSON object containing cloud event data.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_parse_cloud_event(cJSON *json)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(json, OPRT_INVALID_PARM);

    cJSON *action = cJSON_GetObjectItem(json, "action");
    const char *action_str = __json_get_string(action);

    if (action != NULL && action_str == NULL) {
        PR_ERR("cloud event action type invalid");
        return OPRT_CJSON_GET_ERR;
    }

    if (action_str && (strcmp(action_str, "playTts") == 0 ||
                       strcmp(action_str, "alert") == 0)) {

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
        cJSON *data = cJSON_GetObjectItem(json, "data");
        if (data == NULL || !cJSON_IsObject(data)) {
            PR_ERR("cloud event data invalid");
            return OPRT_CJSON_GET_ERR;
        }

        AI_AUDIO_PLAY_TTS_T *playtts = NULL;
        if ((rt = __ai_parse_playtts(action_str, data, &playtts)) == 0) {
            ai_audio_play_tts_url(playtts, FALSE);
            /* if (s_chat_cbc.tuya_ai_chat_playtts) { */
            /*     s_chat_cbc.tuya_ai_chat_playtts(playtts, s_chat_cbc.user_data); */
            /* } */
            __ai_parse_playtts_free(playtts);
        }
#endif

        return rt;
    }

    return rt;
}
