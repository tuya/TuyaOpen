/**
 * @file skill_music_story.h
 * @brief Music and story skill module header
 *
 * This header file defines the functions for parsing and playing music
 * and story content from AI skill responses.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __SKILL_MUSIC_STORY_H__
#define __SKILL_MUSIC_STORY_H__

#include "tuya_cloud_types.h"
#include "cJSON.h"

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
#include "ai_audio_player.h"
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
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
/**
@brief Parse music data from JSON
@param json JSON object containing music data
@param music Pointer to store parsed music structure
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_skill_parse_music(cJSON *json, AI_AUDIO_MUSIC_T **music);

/**
@brief Free music structure memory
@param music Pointer to music structure to free
@return None
*/
void ai_skill_parse_music_free(AI_AUDIO_MUSIC_T *music);

/**
@brief Dump music structure for debugging
@param music Pointer to music structure
@return None
*/
void ai_skill_parse_music_dump(AI_AUDIO_MUSIC_T *music);

/**
@brief Parse play control data from JSON
@param json JSON object containing play control data
@param music Pointer to store parsed music structure
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_skill_parse_playcontrol(cJSON *json, AI_AUDIO_MUSIC_T **music);

/**
@brief Execute play control command
@param music Pointer to music structure with play control information
@return None
*/
void ai_skill_playcontrol_music(AI_AUDIO_MUSIC_T *music);

#endif

#ifdef __cplusplus
}
#endif

#endif /* __SKILL_MUSIC_STORY_H__ */
