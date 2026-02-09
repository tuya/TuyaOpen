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
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
/**
 * @brief Parse audio codec type from format string.
 *
 * @param format Format string (e.g., "mp3", "wav", "speex", "opus", "oggopus").
 * @return AI_AUDIO_CODEC_E Audio codec type.
 */
static AI_AUDIO_CODEC_E __parse_get_codec_type(char *format)
{
    AI_AUDIO_CODEC_E fmt = AI_AUDIO_CODEC_MP3;

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
    cJSON *node = NULL;
    AI_AUDIO_PLAY_TTS_T *playtts_ptr;

    playtts_ptr = tal_malloc(sizeof(AI_AUDIO_PLAY_TTS_T));
    if (playtts_ptr == NULL) {
        PR_ERR("malloc arr fail.");
        return OPRT_MALLOC_FAILED;
    }

    memset(playtts_ptr, 0, sizeof(AI_AUDIO_PLAY_TTS_T));

    cJSON *tts = cJSON_GetObjectItem(json, "tts");
    if (tts) {
        playtts_ptr->tts.tts_type = AI_TTS_TYPE_NORMAL;
        if (strcmp(action, "alert") == 0) {
            playtts_ptr->tts.tts_type = AI_TTS_TYPE_ALERT;
        }
        node = cJSON_GetObjectItem(tts, "url");
        if (node) {
            playtts_ptr->tts.url = mm_strdup(node->valuestring);
        }

        node = cJSON_GetObjectItem(tts, "requestBody");
        if (node) {
            playtts_ptr->tts.req_body = mm_strdup(node->valuestring);
        } else {
            playtts_ptr->tts.req_body = NULL;
        }

        playtts_ptr->tts.http_method = AI_HTTP_METHOD_GET;
        node = cJSON_GetObjectItem(tts, "requestType");
        if (node) {
            if (strcmp(node->valuestring, "post") == 0) {
                playtts_ptr->tts.http_method = AI_HTTP_METHOD_POST;
            }
        }

        node = cJSON_GetObjectItem(tts, "format");
        if (node) {
            playtts_ptr->tts.format = __parse_get_codec_type(node->valuestring);
        }
    }

    cJSON *bgmusic = cJSON_GetObjectItem(json, "bgMusic");
    if (bgmusic) {
        node = cJSON_GetObjectItem(bgmusic, "url");
        if (node) {
            playtts_ptr->bg_music.url = mm_strdup(node->valuestring);
        }

        node = cJSON_GetObjectItem(bgmusic, "requestBody");
        if (node) {
            playtts_ptr->bg_music.req_body = mm_strdup(node->valuestring);
        } else {
            playtts_ptr->bg_music.req_body = NULL;
        }
        playtts_ptr->bg_music.http_method = AI_HTTP_METHOD_GET;
        node = cJSON_GetObjectItem(bgmusic, "requestType");
        if (node) {
            if (strcmp(node->valuestring, "post") == 0) {
                playtts_ptr->bg_music.http_method = AI_HTTP_METHOD_POST;
            }
        }

        node = cJSON_GetObjectItem(bgmusic, "format");
        if (node) {
            playtts_ptr->bg_music.format = __parse_get_codec_type(node->valuestring);
        }

        node = cJSON_GetObjectItem(bgmusic, "duration");
        if (node) {
            playtts_ptr->bg_music.duration = cJSON_GetNumberValue(node);
        }
    }

    *playtts = playtts_ptr;
    return OPRT_OK;
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
    cJSON *action = cJSON_GetObjectItem(json, "action");
    if (action && (strcmp(action->valuestring, "playTts") == 0 ||
                   strcmp(action->valuestring, "alert") == 0)) {

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
        cJSON *data = cJSON_GetObjectItem(json, "data");

        AI_AUDIO_PLAY_TTS_T *playtts = NULL;
        if ((rt = __ai_parse_playtts(action->valuestring, data, &playtts)) == 0) {
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