/**
 * @file ai_audio_proc.h
 * @brief Implements audio processing functionality for AI applications
 *
 * This source file provides the implementation of audio processing functionalities
 * required for AI applications. It includes functionality for audio frame handling,
 * voice activity detection, audio streaming, and interaction with AI processing
 * modules. The implementation supports audio frame processing, voice state management,
 * and integration with audio player modules. This file is essential for developers
 * working on AI-driven audio applications that require real-time audio processing
 * and interaction with AI services.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef _TTS_PLAYER_H_
#define _TTS_PLAYER_H_

#include "tuya_cloud_types.h"
#include "tkl_audio.h"
#include "tuya_audio_player.h"
#include "tuya_audio_recorder.h"

#define CHAT_BOT_WORK_MODE_HOLD     0
#define CHAT_BOT_WORK_MODE_ONE_SHOT 1

#define CHAT_BOT_WORK_MODE CHAT_BOT_WORK_MODE_ONE_SHOT

/**
 * @brief Initialize the AI audio processing module.
 *
 * This function initializes the AI audio processing module, including the audio recorder and player.
 *
 * @return OPERATE_RET - The return code of the operation.
 *         OPRT_OK on success, OPRT_COM_ERROR on communication error.
 */
OPERATE_RET tuya_ai_audio_init(void);

#endif
