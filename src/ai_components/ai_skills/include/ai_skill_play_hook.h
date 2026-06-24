/**
 * @file ai_skill_play_hook.h
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 * @brief Platform hook for AI music/story skill playback (ops registration)
 *
 * ai_components calls ai_skill_play_hook() for music, story, and PlayControl.
 * Apps register AI_SKILL_PLAY_OPS_T at init; when unset, NOT_SUPPORTED falls
 * back to ai_audio_play_music / ai_user_event in skill_music_story.c.
 */
#ifndef __AI_SKILL_PLAY_HOOK_H__
#define __AI_SKILL_PLAY_HOOK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "ai_audio_player.h"

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    OPERATE_RET (*dispatch)(AI_AUDIO_MUSIC_T *music);
} AI_SKILL_PLAY_OPS_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Register platform skill play ops (call once from app init)
 * @param[in] ops Ops table; dispatch must be non-NULL
 * @return OPRT_OK on success, OPRT_INVALID_PARM if ops or dispatch is NULL
 */
OPERATE_RET ai_skill_play_ops_register(const AI_SKILL_PLAY_OPS_T *ops);

/**
 * @brief Unregister platform skill play ops (call from app deinit)
 * @return none
 */
void ai_skill_play_ops_unregister(void);

/**
 * @brief Dispatch music/story skill or play-control to registered ops
 * @param[in] music Parsed music or play-control payload
 * @return OPRT_OK if handled; OPRT_NOT_SUPPORTED if no ops registered
 */
OPERATE_RET ai_skill_play_hook(AI_AUDIO_MUSIC_T *music);

#ifdef __cplusplus
}
#endif

#endif /* __AI_SKILL_PLAY_HOOK_H__ */
