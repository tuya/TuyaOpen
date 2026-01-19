/**
 * @file skill_emotion.h
 * @brief Emotion skill module header
 *
 * This header file defines the types and functions for parsing and playing
 * emotion expressions from AI skill responses.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __SKILL_EMOTION_H__
#define __SKILL_EMOTION_H__

#include "tuya_cloud_types.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define EMOJI_NEUTRAL      "NEUTRAL"
#define EMOJI_HAPPY        "HAPPY"
#define EMOJI_LAUGHING     "LAUGHING"
#define EMOJI_FUNNY        "FUNNY"
#define EMOJI_SAD          "SAD"
#define EMOJI_ANGRY        "ANGRY"
#define EMOJI_FEARFUL      "FEARFUL"
#define EMOJI_LOVING       "LOVING"
#define EMOJI_EMBARRASSED  "EMBARRASSED"
#define EMOJI_SURPRISE     "SURPRISE"
#define EMOJI_SHOCKED      "SHOCKED"
#define EMOJI_THINKING     "THINKING"
#define EMOJI_WINK         "WINK"
#define EMOJI_COOL         "COOL"
#define EMOJI_RELAXED      "RELAXED"
#define EMOJI_DELICIOUS    "DELICIOUS"
#define EMOJI_KISSY        "KISSY"
#define EMOJI_CONFIDENT    "CONFIDENT"
#define EMOJI_SLEEP        "SLEEP"
#define EMOJI_SILLY        "SILLY"
#define EMOJI_CONFUSED     "CONFUSED"
#define EMOJI_TOUCH        "TOUCH"
#define EMOJI_DISAPPOINTED "DISAPPOINTED"
#define EMOJI_ANNOYED      "ANNOYED"
#define EMOJI_WAKEUP       "WAKEUP"
#define EMOJI_LEFT         "LEFT"
#define EMOJI_RIGHT        "RIGHT"


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    const char  *emoji;
    const char  *name;
} AI_AGENT_EMO_T;


/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET ai_skill_emo_process(cJSON *json);

/**
 * @brief Convert Unicode code point string "U+XXXX" to UTF-8 bytes
 * @param[in] unicode_str Unicode string in format "U+XXXX" or "u+xxxx" (e.g., "U+1F636")
 * @param[out] utf8_buf Buffer to store UTF-8 bytes (at least 5 bytes)
 * @param[in] buf_size Size of the buffer
 * @return Number of bytes written, or -1 on error
 */
int ai_emoji_unicode_to_utf8(const char* unicode_str, char* utf8_buf, size_t buf_size);

const char* ai_agent_emoji_get_name(const char* emoji);

const char* ai_agent_emoji_get_by_name(const char* name);

/**
@brief Play emotion expression
@param emo Pointer to emotion structure
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_agent_play_emo(AI_AGENT_EMO_T *emo);


#ifdef __cplusplus
}
#endif

#endif /* __SKILL_EMOTION_H__ */
