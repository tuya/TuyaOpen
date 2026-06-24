/**
 * @file ai_skill_play_hook.c
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 * @brief Dispatcher for platform AI skill music/story playback ops
 */
#include "ai_skill_play_hook.h"

#include "tal_log.h"

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static const AI_SKILL_PLAY_OPS_T *s_play_ops = NULL;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Register platform skill play ops (call once from app init)
 * @param[in] ops Ops table; dispatch must be non-NULL
 * @return OPRT_OK on success, OPRT_INVALID_PARM if ops or dispatch is NULL
 */
OPERATE_RET ai_skill_play_ops_register(const AI_SKILL_PLAY_OPS_T *ops)
{
    if (ops == NULL || ops->dispatch == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (s_play_ops != NULL && s_play_ops != ops) {
        PR_ERR("ai_skill_play_ops_register: second owner is forbidden");
        return OPRT_RESOURCE_NOT_READY;
    }
    s_play_ops = ops;
    return OPRT_OK;
}

/**
 * @brief Unregister platform skill play ops (call from app deinit)
 * @return none
 */
void ai_skill_play_ops_unregister(void)
{
    s_play_ops = NULL;
}

/**
 * @brief Dispatch music/story skill or play-control to registered ops
 * @param[in] music Parsed music or play-control payload
 * @return OPRT_OK if handled; OPRT_NOT_SUPPORTED if no ops registered
 */
OPERATE_RET ai_skill_play_hook(AI_AUDIO_MUSIC_T *music)
{
    if (music == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (s_play_ops == NULL || s_play_ops->dispatch == NULL) {
        return OPRT_NOT_SUPPORTED;
    }
    return s_play_ops->dispatch(music);
}
