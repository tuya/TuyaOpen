/**
 * @file ai_mode_speaker.h
 * @brief smart_speaker custom chat mode — single wakeword, multi-turn dialog.
 *
 * Registers an AI_MODE_HANDLE_T into the ai_mode framework (ai_components/ai_mode)
 * at AI_CHAT_MODE_SPEAKER (in the AI_CHAT_MODE_CUSTOM_START range). The product app
 * drives the framework (task thread + button/VAD/event routing) and switches to
 * this mode via ai_mode_switch(AI_CHAT_MODE_SPEAKER).
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#ifndef __AI_MODE_SPEAKER_H__
#define __AI_MODE_SPEAKER_H__

#include "tuya_cloud_types.h"
#include "ai_manage_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Custom chat-mode id for the smart_speaker mode. CUSTOM_START == 0x100. */
#define AI_CHAT_MODE_SPEAKER (AI_CHAT_MODE_CUSTOM_START + 0)

/* Talk mode within a single wake window. */
typedef enum {
    AI_SPEAKER_TALK_SINGLE = 0, /* one Q&A per wakeword, then back to await-wakeword */
    AI_SPEAKER_TALK_MULTI,      /* keep listening for follow-up turns (multi / free) */
} AI_SPEAKER_TALK_MODE_E;

/**
 * @brief Register the smart_speaker custom mode into the ai_mode framework.
 * @return OPRT_OK on success.
 */
OPERATE_RET ai_mode_speaker_register(void);

/**
 * @brief Select single-turn vs multi-turn behavior (from the ctrl_group DP).
 *        Affects what happens after an AI reply finishes.
 */
void ai_mode_speaker_set_talk_mode(AI_SPEAKER_TALK_MODE_E mode);

#ifdef __cplusplus
}
#endif

#endif /* __AI_MODE_SPEAKER_H__ */
