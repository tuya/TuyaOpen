/**
 * @file skill_cloudevent.h
 * @brief Cloud event skill module header.
 *
 * This header provides function declarations for parsing and processing
 * cloud events, including TTS playback commands.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __SKILL_CLOUDEVENT_H__
#define __SKILL_CLOUDEVENT_H__

#include "tuya_cloud_types.h"
#include "cJSON.h"
#include "skill_music_story.h"
#include "tuya_ai_protocol.h"

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
/**
 * @brief Parse and process cloud event from JSON.
 *
 * @param json JSON object containing cloud event data.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_parse_cloud_event(cJSON *json);

#ifdef __cplusplus
}
#endif

#endif /* __SKILL_CLOUDEVENT_H__ */
