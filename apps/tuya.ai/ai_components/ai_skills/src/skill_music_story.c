/**
 * @file skill_music_story.c
 * @brief skill_music_story module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
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

    tal_free(music);
}

static AI_AUDIO_CODEC_E _parse_get_codec_type(char *format)
{
    AI_AUDIO_CODEC_E fmt = AI_AUDIO_CODEC_MAX;

    if (strcmp(format, "mp3") == 0) {
        fmt = AI_AUDIO_CODEC_MP3;
    } else {
        PR_ERR("decode type invald:%s", format);
    }

    return fmt;
}

static OPERATE_RET _parse_music_item(uint32_t id, cJSON *audio_item, AI_MUSIC_SRC_T *music_src)
{
    cJSON *item = NULL;

    if ((item = cJSON_GetObjectItem(audio_item, "id")) != NULL) {
        music_src->id = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(audio_item, "url")) != NULL) {
        music_src->url = mm_strdup(item->valuestring);
        if (music_src->url == NULL) {
            PR_ERR("no memory, strdup failed");
            return -1;
        }
    } else {
        PR_ERR("the data is error");
        return -1;
    }

    if ((item = cJSON_GetObjectItem(audio_item, "size")) != NULL) {
        music_src->length = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(audio_item, "duration")) != NULL) {
        music_src->duration = item->valueint;
    }

    if ((item = cJSON_GetObjectItem(audio_item, "format")) != NULL) {
        music_src->format = _parse_get_codec_type(item->valuestring);
        if (music_src->format == AI_AUDIO_CODEC_MAX) {
            PR_ERR("the format not support");
            return -1;
        }
    } else {
        PR_ERR("the data is error");
        return -1;
    }

    if ((item = cJSON_GetObjectItem(audio_item, "artist")) != NULL) {
        music_src->artist = mm_strdup(item->valuestring);
    }

    if ((item = cJSON_GetObjectItem(audio_item, "name")) != NULL) {
        music_src->song_name = mm_strdup(item->valuestring);
    }

    if ((item = cJSON_GetObjectItem(audio_item, "audioId")) != NULL) {
        music_src->audio_id = mm_strdup(item->valuestring);
    }

    if ((item = cJSON_GetObjectItem(audio_item, "imageUrl")) != NULL) {
        music_src->img_url = mm_strdup(item->valuestring);
    }

    return 0;
}

OPERATE_RET ai_skill_parse_music(cJSON *json, AI_AUDIO_MUSIC_T **music)
{
    int audio_num = 0;
    cJSON *skill_general = cJSON_GetObjectItem(json, "general");
    cJSON *skill_custom = cJSON_GetObjectItem(json, "custom");
    cJSON *action = NULL, *skill_data = NULL, *audios = NULL, *node = NULL;
    AI_MUSIC_SRC_T *music_src;
    AI_AUDIO_MUSIC_T *music_ptr;

    if (skill_custom && (action = cJSON_GetObjectItem(skill_custom, "action"))) {
        skill_data = cJSON_GetObjectItem(skill_custom, "data");
        if (skill_data) {
            if ((audios = cJSON_GetObjectItem(skill_data, "audios")) != NULL) {
                audio_num = cJSON_GetArraySize(audios);
            }
        }
    }

    if (action == NULL && skill_general) {
        action = cJSON_GetObjectItem(skill_general, "action");
        skill_data = cJSON_GetObjectItem(skill_general, "data");
        if (skill_data) {
            if ((audios = cJSON_GetObjectItem(skill_data, "audios")) != NULL) {
                audio_num = cJSON_GetArraySize(audios);
            }
        } else {
            PR_WARN("no skill data");
        }
    }

    if (action == NULL || (strcmp(action->valuestring, "play") == 0 && audio_num == 0)) {
        PR_WARN("the music list not exsit:%d", audio_num);
        return -1;
    }

    music_ptr = tal_malloc(sizeof(AI_AUDIO_MUSIC_T));
    if (music_ptr == NULL) {
        PR_ERR("malloc arr fail.");
        return OPRT_MALLOC_FAILED;
    }

    memset(music_ptr, 0, sizeof(AI_AUDIO_MUSIC_T));
    music_ptr->src_cnt = audio_num;
    if ((node = cJSON_GetObjectItem(skill_data, "preTtsFlag")) != NULL) {
        if (node->type == cJSON_True) {
            music_ptr->has_tts = TRUE;
        }
    }

    if (action && strlen(action->valuestring) > 0) {
        snprintf(music_ptr->action, sizeof(music_ptr->action), "%s", action->valuestring);
    } else {
        snprintf(music_ptr->action, sizeof(music_ptr->action), "play");
    }

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

    *music = music_ptr;

    return OPRT_OK;
}

void ai_skill_parse_music_free(AI_AUDIO_MUSIC_T *music)
{
    if (music == NULL) {
        return;
    }

    if (music->src_array) {
        _music_src_free(music->src_array);
    }

    tal_free(music);
}

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

static OPERATE_RET __parse_playcontrol_data(cJSON *data, cJSON *skill_data, AI_AUDIO_MUSIC_T **music)
{
    if (skill_data == NULL) {
        return -1;
    }

    cJSON *action = cJSON_GetObjectItem(skill_data, "action");
    if (action == NULL || strlen(action->valuestring) <= 0) {
        return -1;
    }

    if (strcmp(action->valuestring, "next") == 0 || 
        strcmp(action->valuestring, "prev") == 0 || 
        strcmp(action->valuestring, "play") == 0 ||
        strcmp(action->valuestring, "stop") == 0 ) {
        return ai_skill_parse_music(data, music);
    }

    AI_AUDIO_MUSIC_T *media = tal_malloc(sizeof(AI_AUDIO_MUSIC_T));
    if (media == NULL) {
        PR_ERR("malloc arr fail.");
        return OPRT_MALLOC_FAILED;
    }

    memset(media, 0, sizeof(AI_AUDIO_MUSIC_T));
    snprintf(media->action, sizeof(media->action), "%s", action->valuestring);

    *music = media;

    return 0;
}

OPERATE_RET ai_skill_parse_playcontrol(cJSON *json, AI_AUDIO_MUSIC_T **music)
{
    cJSON *skill_general = cJSON_GetObjectItem(json, "general");
    cJSON *skill_custom = cJSON_GetObjectItem(json, "custom");

    if (__parse_playcontrol_data(json, skill_custom, music) != 0) {
        return __parse_playcontrol_data(json, skill_general, music);
    }

    return 0;
}

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