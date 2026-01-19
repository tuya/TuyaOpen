/**
 * @file ai_skill.h
 * @brief AI skill module header
 *
 * This header file defines the functions for processing AI skills such as
 * emotion, music, story, and play control skills.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_SKILL_H__
#define __AI_SKILL_H__

#include "tuya_cloud_types.h"
#include "cJSON.h"
#include "tuya_ai_output.h"
#include "skill_emotion.h"
#include "skill_cloudevent.h"

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
#include "skill_music_story.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET ai_text_process(AI_TEXT_TYPE_E type, cJSON *root, bool eof);

#ifdef __cplusplus
}
#endif

#endif /* __AI_SKILL_H__ */
