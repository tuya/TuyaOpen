/**
 * @file skill_emotion.c
 * @brief skill_emotion module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"
#include "mix_method.h"

#include "tal_api.h"
#include "cJSON.h"

#include "ai_user_event.h"
#include "skill_emotion.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
AI_AGENT_EMO_T cAI_AGENT_EMO[] = {
    {"U+1F636", EMOJI_NEUTRAL},      // 😶
    {"U+1F642", EMOJI_HAPPY},        // 🙂
    {"U+1F606", EMOJI_LAUGHING},     // 😆
    {"U+1F602", EMOJI_FUNNY},        // 😂
    {"U+1F614", EMOJI_SAD},          // 😔
    {"U+1F620", EMOJI_ANGRY},        // 😠
    {"U+1F62D", EMOJI_FEARFUL},      // 😭
    {"U+1F60D", EMOJI_LOVING},       // 😍
    {"U+1F633", EMOJI_EMBARRASSED},  // 😳
    {"U+1F62F", EMOJI_SURPRISE},     // 😯
    {"U+1F631", EMOJI_SHOCKED},      // 😱
    {"U+1F914", EMOJI_THINKING},     // 🤔
    {"U+1F609", EMOJI_WINK},         // 😉
    {"U+1F60E", EMOJI_COOL},         // 😎
    {"U+1F60C", EMOJI_RELAXED},      // 😌
    {"U+1F924", EMOJI_DELICIOUS},    // 🤤
    {"U+1F618", EMOJI_KISSY},        // 😘
    {"U+1F60F", EMOJI_CONFIDENT},    // 😏
    {"U+1F634", EMOJI_SLEEP},        // 😴
    {"U+1F61C", EMOJI_SILLY},        // 😜
    {"U+1F644", EMOJI_CONFUSED}      // 🙄
};

/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/
OPERATE_RET ai_skill_emo_process(cJSON *json)
{
    cJSON *emotion, *text;
    AI_AGENT_EMO_T emo;
    int emo_cnt = 0;

    json = cJSON_GetObjectItem(json, "skillContent");
    if (!json) {
        PR_ERR("skill content is NULL");
        return OPRT_OK;
    }

    emotion = cJSON_GetObjectItem(json, "emotion");
    text = cJSON_GetObjectItem(json, "text");
    emo_cnt = cJSON_GetArraySize(emotion);
    if (emo_cnt == 0) {
        PR_ERR("emo array is empty");
        return OPRT_CJSON_GET_ERR;
    }

    for (int i = 0; i < emo_cnt; i++) {
        emo.emoji = cJSON_GetStringValue(cJSON_GetArrayItem(text, i));
        emo.name = cJSON_GetStringValue(cJSON_GetArrayItem(emotion, i));
        ai_agent_play_emo(&emo);
    }

    return OPRT_OK;
}


/**
 * @brief Convert Unicode code point string "U+XXXX" to UTF-8 bytes
 * @param[in] unicode_str Unicode string in format "U+XXXX" or "u+xxxx" (e.g., "U+1F636")
 * @param[out] utf8_buf Buffer to store UTF-8 bytes (at least 5 bytes)
 * @param[in] buf_size Size of the buffer
 * @return Number of bytes written, or -1 on error
 */
int ai_emoji_unicode_to_utf8(const char* unicode_str, char* utf8_buf, size_t buf_size)
{
    TUYA_CHECK_NULL_RETURN(unicode_str, -1);
    TUYA_CHECK_NULL_RETURN(utf8_buf, -1);
    
    if (buf_size < 5 || (unicode_str[0] != 'U' && unicode_str[0] != 'u') || unicode_str[1] != '+') {
        return -1;
    }
    
    // Parse hex string after "U+"
    uint32_t codepoint = 0;
    for (const char* p = unicode_str + 2; *p != '\0'; p++) {
        char c = *p;
        if (c >= '0' && c <= '9') {
            codepoint = (codepoint << 4) | (c - '0');
        } else if (c >= 'A' && c <= 'F') {
            codepoint = (codepoint << 4) | (c - 'A' + 10);
        } else if (c >= 'a' && c <= 'f') {
            codepoint = (codepoint << 4) | (c - 'a' + 10);
        } else {
            break;
        }
    }
    
    if (codepoint > 0x10FFFF) {
        return -1;
    }
    
    // Convert to UTF-8
    int len = 0;
    if (codepoint <= 0x7F) {
        utf8_buf[0] = (char)codepoint;
        len = 1;
    } else if (codepoint <= 0x7FF) {
        utf8_buf[0] = (char)(0xC0 | (codepoint >> 6));
        utf8_buf[1] = (char)(0x80 | (codepoint & 0x3F));
        len = 2;
    } else if (codepoint <= 0xFFFF) {
        utf8_buf[0] = (char)(0xE0 | (codepoint >> 12));
        utf8_buf[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        utf8_buf[2] = (char)(0x80 | (codepoint & 0x3F));
        len = 3;
    } else {
        utf8_buf[0] = (char)(0xF0 | (codepoint >> 18));
        utf8_buf[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        utf8_buf[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        utf8_buf[3] = (char)(0x80 | (codepoint & 0x3F));
        len = 4;
    }
    
    utf8_buf[len] = '\0';

    return len;
}

const char* ai_agent_emoji_get_name(const char* emoji) 
{
    TUYA_CHECK_NULL_RETURN(emoji, NULL);
    
    for (size_t i = 0; i < CNTSOF(cAI_AGENT_EMO); i++) {
        if (strcmp(cAI_AGENT_EMO[i].emoji, emoji) == 0) {
            return cAI_AGENT_EMO[i].name;
        }
    }

    PR_NOTICE("not found emoji: %s return %s as default", emoji, cAI_AGENT_EMO[0].name);
    
    return cAI_AGENT_EMO[0].name; // 未找到匹配的emoji, use neutral as default
}

const char* ai_agent_emoji_get_by_name(const char* name) 
{
    TUYA_CHECK_NULL_RETURN(name, NULL);
    
    for (size_t i = 0; i < CNTSOF(cAI_AGENT_EMO); i++) {
        if (strcmp(cAI_AGENT_EMO[i].name, name) == 0) {
            return cAI_AGENT_EMO[i].emoji;
        }
    }
    
    return cAI_AGENT_EMO[0].emoji; // 未找到匹配的emoji, use neutral as default
}


OPERATE_RET ai_agent_play_emo(AI_AGENT_EMO_T *emo)
{
    // send data to register cb
    ai_user_event_notify(AI_USER_EVT_EMOTION, emo);

    return OPRT_OK;    
}