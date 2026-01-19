/**
 * @file skill_cloudevent.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
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
OPERATE_RET ai_parse_cloud_event(cJSON *json);

#ifdef __cplusplus
}
#endif

#endif /* __SKILL_CLOUDEVENT_H__ */
