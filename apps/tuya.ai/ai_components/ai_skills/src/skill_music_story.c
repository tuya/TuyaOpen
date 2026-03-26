/**
 * @file skill_music_story.c
 * @brief Music and story skill implementation.
 *
 * This file provides functions for parsing and playing music and story content
 * from AI skill responses, including playlist management and play control.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)

#include "mix_method.h"

#include "tal_api.h"
#include "svc_ai_player.h"

#include "ai_audio_player.h"

#include "ai_user_event.h"
#include "ai_skill.h"
#include "skill_music_story.h"

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
static bool _json_is_string(const cJSON *item)
{
    return item && cJSON_IsString(item) && item->valuestring != NULL;
}

static const char *_json_get_string(const cJSON *item)
{
    return _json_is_string(item) ? item->valuestring : NULL;
}

/**
 * @brief Free music source structure and release allocated memory.
 *
 * @param music Pointer to music source structure to free.
 */
static void _music_src_free(AI_MUSIC_SRC_T *music)
{
    if (!music)
        return;

    if (music->url) {
        tal_free(music->url);
    }

    if (music->artist) {
        tal_free(music->artist);
    }

    if (music->song_name) {
        tal_free(music->song_name);
    }

    if (music->audio_id) {
        tal_free(music->audio_id);
    }

    if (music->img_url) {
        tal_free(music->img_url);
    }
}

/**
 * @brief Parse audio codec type from format string.
 *
 * @param format Format string (e.g., "mp3").
 * @return AI_AUDIO_CODEC_E Audio codec type.
 */
static AI_AUDIO_CODEC_E _parse_get_codec_type(char *format)
{
    AI_AUDIO_CODEC_E fmt = AI_AUDIO_CODEC_MAX;

    if (format == NULL) {
        PR_ERR("decode type is NULL");
        return fmt;
    }

    if (strcmp(format, "mp3") == 0) {
        fmt = AI_AUDIO_CODEC_MP3;
    } else {
        PR_ERR("decode type invald:%s", format);
    }

    return fmt;
}

/**
 * @brief Parse a single music item from JSON.
 *
 * @param id Music item ID.
 * @param audio_item JSON object containing audio item data.
 * @param music_src Pointer to store parsed music source structure.
 * @return OPERATE_RET Operation result code.
 */
static OPERATE_RET _parse_music_item(uint32_t id, cJSON *audio_item, AI_MUSIC_SRC_T *music_src)
{
    cJSON *item = NULL;

    TUYA_CHECK_NULL_RETURN(audio_item, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(music_src, OPRT_INVALID_PARM);
    if (!cJSON_IsObject(audio_item)) {
        PR_ERR("audio item is not object");
        return OPRT_INVALID_PARM;
    }

    if ((item = cJSON_GetObjectItem(audio_item, "id")) != NULL && cJSON_IsNumber(item)) {
        music_src->id = item->valueint;
    } else {
        music_src->id = id;
    }

    if (_json_is_string(item = cJSON_GetObjectItem(audio_item, "url"))) {
        music_src->url = mm_strdup(item->valuestring);
        TUYA_CHECK_NULL_RETURN(music_src->url, OPRT_MALLOC_FAILED);
    } else {
        PR_ERR("invalid music url");
        return OPRT_CJSON_GET_ERR;
    }

    if ((item = cJSON_GetObjectItem(audio_item, "size")) != NULL && cJSON_IsNumber(item)) {
        music_src->length = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(audio_item, "duration")) != NULL && cJSON_IsNumber(item)) {
        music_src->duration = item->valueint;
    }

    if (_json_is_string(item = cJSON_GetObjectItem(audio_item, "format"))) {
        music_src->format = _parse_get_codec_type(item->valuestring);
        if (music_src->format == AI_AUDIO_CODEC_MAX) {
            PR_ERR("the format not support");
            return OPRT_CJSON_GET_ERR;
        }
    } else {
        PR_ERR("invalid music format");
        return OPRT_CJSON_GET_ERR;
    }

    item = cJSON_GetObjectItem(audio_item, "artist");
    if (_json_is_string(item)) {
        music_src->artist = mm_strdup(item->valuestring);
        TUYA_CHECK_NULL_RETURN(music_src->artist, OPRT_MALLOC_FAILED);
    } else if (item != NULL) {
        PR_WARN("invalid artist field type, ignore");
    }

    item = cJSON_GetObjectItem(audio_item, "name");
    if (_json_is_string(item)) {
        music_src->song_name = mm_strdup(item->valuestring);
        TUYA_CHECK_NULL_RETURN(music_src->song_name, OPRT_MALLOC_FAILED);
    } else if (item != NULL) {
        PR_WARN("invalid song name field type, ignore");
    }

    item = cJSON_GetObjectItem(audio_item, "audioId");
    if (_json_is_string(item)) {
        music_src->audio_id = mm_strdup(item->valuestring);
        TUYA_CHECK_NULL_RETURN(music_src->audio_id, OPRT_MALLOC_FAILED);
    } else if (item != NULL) {
        PR_WARN("invalid audioId field type, ignore");
    }

    item = cJSON_GetObjectItem(audio_item, "imageUrl");
    if (_json_is_string(item)) {
        music_src->img_url = mm_strdup(item->valuestring);
        TUYA_CHECK_NULL_RETURN(music_src->img_url, OPRT_MALLOC_FAILED);
    } else if (item != NULL) {
        PR_WARN("invalid imageUrl field type, ignore");
    }

    return OPRT_OK;
}

/**
 * @brief Parse music data from JSON.
 *
 * @param json JSON object containing music data.
 * @param music Pointer to store parsed music structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_skill_parse_music(cJSON *json, AI_AUDIO_MUSIC_T **music)
{
    TUYA_CHECK_NULL_RETURN(json, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(music, OPRT_INVALID_PARM);

    int audio_num = 0;
    cJSON *skill_general = cJSON_GetObjectItem(json, "general");
    cJSON *skill_custom = cJSON_GetObjectItem(json, "custom");
    cJSON *action = NULL, *skill_data = NULL, *audios = NULL, *node = NULL;
    const char *action_str = NULL;
    AI_MUSIC_SRC_T *music_src;
    AI_AUDIO_MUSIC_T *music_ptr;

    if (skill_custom && cJSON_IsObject(skill_custom) && (action = cJSON_GetObjectItem(skill_custom, "action"))) {
        skill_data = cJSON_GetObjectItem(skill_custom, "data");
        if (skill_data && cJSON_IsObject(skill_data)) {
            if ((audios = cJSON_GetObjectItem(skill_data, "audios")) != NULL && cJSON_IsArray(audios)) {
                audio_num = cJSON_GetArraySize(audios);
            }
        }
    }

    if (action == NULL && skill_general && cJSON_IsObject(skill_general)) {
        action = cJSON_GetObjectItem(skill_general, "action");
        skill_data = cJSON_GetObjectItem(skill_general, "data");
        if (skill_data && cJSON_IsObject(skill_data)) {
            if ((audios = cJSON_GetObjectItem(skill_data, "audios")) != NULL && cJSON_IsArray(audios)) {
                audio_num = cJSON_GetArraySize(audios);
            }
        } else {
            PR_WARN("no skill data");
        }
    }

    action_str = _json_get_string(action);
    if (action_str == NULL) {
        PR_WARN("invalid action");
        return OPRT_CJSON_GET_ERR;
    }

    if (strcmp(action_str, "play") == 0 && audio_num == 0) {
        PR_WARN("the music list not exsit:%d", audio_num);
        return OPRT_CJSON_GET_ERR;
    }

    music_ptr = tal_malloc(sizeof(AI_AUDIO_MUSIC_T));
    TUYA_CHECK_NULL_RETURN(music_ptr, OPRT_MALLOC_FAILED);

    memset(music_ptr, 0, sizeof(AI_AUDIO_MUSIC_T));
    music_ptr->src_cnt = audio_num;
    if (skill_data != NULL && (node = cJSON_GetObjectItem(skill_data, "preTtsFlag")) != NULL) {
        if (cJSON_IsTrue(node)) {
            music_ptr->has_tts = TRUE;
        }
    }

    if (action_str[0] != '\0') {
        snprintf(music_ptr->action, sizeof(music_ptr->action), "%s", action_str);
    } else {
        snprintf(music_ptr->action, sizeof(music_ptr->action), "play");
    }

    if (audio_num != 0) {
        music_ptr->src_array = tal_malloc(sizeof(AI_MUSIC_SRC_T) * audio_num);
        if (music_ptr->src_array == NULL) {
            PR_ERR("malloc arr fail.");
            ai_skill_parse_music_free(music_ptr);
            return OPRT_MALLOC_FAILED;
        }

        memset(music_ptr->src_array, 0, sizeof(AI_MUSIC_SRC_T) * audio_num);

        int i = 0;
        for (i = 0; i < music_ptr->src_cnt; i++) {
            music_src = &music_ptr->src_array[i];
            node = cJSON_GetArrayItem(audios, i);
            if (_parse_music_item(i, node, music_src) != OPRT_OK) {
                PR_ERR("parse audio %d fail.", i);
                ai_skill_parse_music_free(music_ptr);
                return OPRT_CJSON_PARSE_ERR;
            }
        }
    }

    *music = music_ptr;

    return OPRT_OK;
}

/**
 * @brief Free music structure memory.
 *
 * @param music Pointer to music structure to free.
 */
void ai_skill_parse_music_free(AI_AUDIO_MUSIC_T *music)
{
    if (music == NULL) {
        return;
    }

    if(music->src_array) {
        for(int i = 0; i < music->src_cnt; i++) {
            _music_src_free(&music->src_array[i]);
        }

        tal_free(music->src_array);
    }

    tal_free(music);
}

/**
 * @brief Dump music structure for debugging.
 *
 * @param music Pointer to music structure.
 */
void ai_skill_parse_music_dump(AI_AUDIO_MUSIC_T *music)
{
    if (music == NULL) {
        PR_WARN("can not dump media info");
        return;
    }

    int i = 0;
    AI_MUSIC_SRC_T *media_src = NULL;

    PR_INFO("media info: has tts:%d, action:%s, count:%d", 
        music->has_tts, music->action, music->src_cnt);

    for (i = 0; i < music->src_cnt; i++) {
        media_src = &music->src_array[i];
        PR_INFO("  id:%d", media_src->id);
        PR_INFO("  url:%s", media_src->url ? media_src->url : "NULL");
        PR_INFO("  length:%lld", media_src->length);
        PR_INFO("  duration:%lld", media_src->duration);
        PR_INFO("  format:%d", media_src->format);
        PR_INFO("  artist:%s", media_src->artist ? media_src->artist : "NULL");
        PR_INFO("  song_name:%s", media_src->song_name ? media_src->song_name : "NULL");
        PR_INFO("  audio_id:%s", media_src->audio_id  ? media_src->audio_id : "NULL");
        PR_INFO("  img_url:%s\n", media_src->img_url  ? media_src->img_url : "NULL");
    }
}

/**
 * @brief Parse play control data from JSON.
 *
 * @param data JSON object containing play control data.
 * @param skill_data JSON object containing skill data.
 * @param music Pointer to store parsed music structure.
 * @return OPERATE_RET Operation result code.
 */
static OPERATE_RET __parse_playcontrol_data(cJSON *data, cJSON *skill_data, AI_AUDIO_MUSIC_T **music)
{
    const char *action_str = NULL;

    TUYA_CHECK_NULL_RETURN(music, OPRT_INVALID_PARM);

    if (skill_data == NULL || !cJSON_IsObject(skill_data)) {
        return OPRT_CJSON_GET_ERR;
    }

    cJSON *action = cJSON_GetObjectItem(skill_data, "action");
    action_str = _json_get_string(action);
    if (action_str == NULL || action_str[0] == '\0') {
        return OPRT_CJSON_GET_ERR;
    }

    if (strcmp(action_str, "next") == 0 ||
        strcmp(action_str, "prev") == 0 ||
        strcmp(action_str, "play") == 0 ||
        strcmp(action_str, "stop") == 0 ) {
        return ai_skill_parse_music(data, music);
    }

    AI_AUDIO_MUSIC_T *media = tal_malloc(sizeof(AI_AUDIO_MUSIC_T));
    TUYA_CHECK_NULL_RETURN(media, OPRT_MALLOC_FAILED);

    memset(media, 0, sizeof(AI_AUDIO_MUSIC_T));
    snprintf(media->action, sizeof(media->action), "%s", action_str);

    *music = media;

    return OPRT_OK;
}

/**
 * @brief Parse play control data from JSON.
 *
 * @param json JSON object containing play control data.
 * @param music Pointer to store parsed music structure.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_skill_parse_playcontrol(cJSON *json, AI_AUDIO_MUSIC_T **music)
{
    cJSON *skill_general = cJSON_GetObjectItem(json, "general");
    cJSON *skill_custom = cJSON_GetObjectItem(json, "custom");

    if (__parse_playcontrol_data(json, skill_custom, music) != 0) {
        return __parse_playcontrol_data(json, skill_general, music);
    }

    return 0;
}

/**
 * @brief Execute play control command.
 *
 * @param music Pointer to music structure with play control information.
 */
void ai_skill_playcontrol_music(AI_AUDIO_MUSIC_T *music)
{
    if (strcmp(music->action, "play") == 0 && music->src_cnt > 0) {
        ai_audio_play_music(music);
    } else if (strcmp(music->action, "resume") == 0) {
        ai_user_event_notify(AI_USER_EVT_PLAY_CTL_RESUME, NULL);
    } else if (strcmp(music->action, "stop") == 0) {
        ai_user_event_notify(AI_USER_EVT_PLAY_CTL_PAUSE, NULL);
    } else if (strcmp(music->action, "replay") == 0) {
        ai_user_event_notify(AI_USER_EVT_PLAY_CTL_REPLAY, NULL);
    } else if (strcmp(music->action, "prev") == 0 || strcmp(music->action, "next") == 0) {
        if (music->src_cnt > 0) {
            ai_audio_play_music(music);
        } else {

        }
    } else if (strcmp(music->action, "single_loop") == 0) {
        ai_user_event_notify(AI_USER_EVT_PLAY_CTL_SINGLE_LOOP, NULL);
    } else if (strcmp(music->action, "sequential_loop") == 0) {
        ai_user_event_notify(AI_USER_EVT_PLAY_CTL_SEQUENTIAL_LOOP, NULL);
    } else if (strcmp(music->action, "no_loop") == 0) {
        ai_user_event_notify(AI_USER_EVT_PLAY_CTL_SEQUENTIAL, NULL);
    } else {
        PR_WARN("unknown action:%s", music->action);
    }

    return;
}

#endif
