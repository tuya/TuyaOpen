/**
 * @file ai_skill.c
 * @brief AI skill module implementation
 *
 * This module implements AI skill processing, including emotion skills,
 * music/story skills, and play control skills.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "cJSON.h"

#include "ai_user_event.h"
#include "skill_emotion.h"

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
#include "ai_audio_player.h"
#include "skill_music_story.h"
#endif

#include "ai_skill.h"

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
/**
@brief Process AI skill data from JSON
@param root JSON root object containing skill data
@param eof End of file flag
@return OPERATE_RET Operation result
*/
static OPERATE_RET __ai_skills_process(cJSON *root, bool eof)
{
    OPERATE_RET rt = OPRT_OK;
    const cJSON *node = NULL;
    const char *code = NULL;

    /* Root is data:{}, parse code */
    node = cJSON_GetObjectItem(root, "code");
    code = cJSON_GetStringValue(node);
    if (!code) 
        return OPRT_OK;

    PR_NOTICE("text -> skill code: %s", code);
    if (strcmp(code, "emo") == 0 || strcmp(code, "llm_emo") == 0) {
        ai_skill_emo_process(root);
    }
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
    else if (strcmp(code, "music") == 0 || strcmp(code, "story") == 0) {
        AI_AUDIO_MUSIC_T *music = NULL;
        if (ai_skill_parse_music(root, &music) == OPRT_OK) {
            ai_skill_parse_music_dump(music);
            ai_audio_play_music(music);
            ai_skill_parse_music_free(music);
        }
    } else if (strcmp(code, "PlayControl") == 0) {
        AI_AUDIO_MUSIC_T *music = NULL;
        if ((rt = ai_skill_parse_playcontrol(root, &music)) == 0) {
            ai_skill_parse_music_dump(music);
            ai_skill_playcontrol_music(music);
            ai_skill_parse_music_free(music);
        }
    } 
#endif
    else {
        PR_NOTICE("skill %s not handled", code);
        /* PR_NOTICE("skill content %s ", cJSON_PrintUnformatted(root)); */

        ai_user_event_notify(AI_USER_EVT_SKILL, root);
    }

    return rt; 
}

/**
@brief Process ASR (Automatic Speech Recognition) text stream
@param root JSON root object containing ASR data
@param eof End of file flag
@return OPERATE_RET Operation result
*/
static OPERATE_RET __ai_asr_process(cJSON *root, bool eof)
{
    char *content = cJSON_GetStringValue(root);
    PR_NOTICE("text -> ASR result: %s", content);
    if (!content) {
        content = "";
    }

    AI_NOTIFY_TEXT_T text;
    text.data      = content;
    text.datalen   = strlen(content);
    text.timeindex = 0;
    ai_user_event_notify((0 == strlen(content))?AI_USER_EVT_ASR_EMPTY:AI_USER_EVT_ASR_OK, &text);

    return OPRT_OK;
}

/**
@brief Process NLG (Natural Language Generation) text stream
@param root JSON root object containing NLG data
@param eof End of file flag
@return OPERATE_RET Operation result
*/
static OPERATE_RET __ai_nlg_process(cJSON *root, bool eof)
{
    char *json_str = cJSON_PrintUnformatted(root);
    PR_NOTICE("json-str %s", json_str);
    cJSON_free(json_str);

    char *content = cJSON_GetStringValue(cJSON_GetObjectItem(root, "content"));
    if (!content) {
        content = "";
    }

    AI_NOTIFY_TEXT_T text;
    text.data      = content;
    text.datalen   = strlen(content);
    PR_NOTICE("text -> NLG eof: %d, content: %s, time: %d", eof, content, text.timeindex);

    // send data to register cb
    static AI_USER_EVT_TYPE_E event_type = AI_USER_EVT_TEXT_STREAM_STOP;
    if(event_type == AI_USER_EVT_TEXT_STREAM_STOP) {
        if(eof) {
            if(strlen(content) > 0) {
                ai_user_event_notify(AI_USER_EVT_TEXT_STREAM_START, &text);
                text.data = NULL;
                text.datalen = 0;
                ai_user_event_notify(AI_USER_EVT_TEXT_STREAM_STOP, &text);
                event_type = AI_USER_EVT_TEXT_STREAM_STOP;
            }
        }else {
            ai_user_event_notify(AI_USER_EVT_TEXT_STREAM_START, &text); 
            event_type = AI_USER_EVT_TEXT_STREAM_DATA;
        }
    } else {
        if (event_type == AI_USER_EVT_TEXT_STREAM_DATA) {
            ai_user_event_notify(eof?AI_USER_EVT_TEXT_STREAM_STOP:AI_USER_EVT_TEXT_STREAM_DATA, &text);
            event_type = eof?AI_USER_EVT_TEXT_STREAM_STOP:AI_USER_EVT_TEXT_STREAM_DATA;
        }
    }

    AI_AGENT_EMO_T emo;
    cJSON *tags_array = cJSON_GetObjectItem(root, "tags");
    if (tags_array && cJSON_IsArray(tags_array) && cJSON_GetArraySize(tags_array) > 0) {
        char *emoji = cJSON_GetStringValue(cJSON_GetArrayItem(tags_array, 0));
        if (emoji && strlen(emoji)) {
            emo.emoji = emoji;
            emo.name = ai_agent_emoji_get_name(emoji);
            ai_agent_play_emo(&emo);
        }
    }  


    return OPRT_OK;
}

OPERATE_RET ai_text_process(AI_TEXT_TYPE_E type, cJSON *root, bool eof)
{    
    TUYA_CHECK_NULL_RETURN(root, OPRT_INVALID_PARM);

    switch(type) {
    case AI_TEXT_ASR:
        __ai_asr_process(root, eof);
    break;
    case AI_TEXT_NLG:
        __ai_nlg_process(root, eof);
    break;
    case AI_TEXT_SKILL:
        __ai_skills_process(root, eof);
    break;
    case AI_TEXT_CLOUD_EVENT:
        ai_parse_cloud_event(root);
    break;
    default:
        // PR_NOTICE("ai agent -> unknown text type: %d", type);
        // char *content = cJSON_PrintUnformatted(root);
        // PR_NOTICE("text content: %s", content);
        // cJSON_free(content);
    break;     
    }

    return OPRT_OK;
}