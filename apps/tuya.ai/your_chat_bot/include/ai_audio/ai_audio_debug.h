/**
 * @file ai_audio_debug.h
 * @brief Implements audio debugging functionality for network communication
 *
 * This source file provides the implementation of audio debugging functionalities
 * required for network communication. It includes functionality for audio stream
 * handling, TCP connection management, data transmission, and ring buffer operations.
 * The implementation supports multiple audio stream types, TCP connections, and
 * data upload mechanisms. This file is essential for developers working on IoT
 * applications that require real-time audio debugging and network communication.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_AUDIO_DEBUG_H__
#define __AI_AUDIO_DEBUG_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AI_AUDIO_DEBUG 1

typedef struct {
    uint8_t *buf;
    uint32_t len;
} AI_AUDIO_DEBUG_FRAME_T;

typedef enum {
    DEBUG_UPLOAD_STREAM_TYPE_RAW = 0,
    DEBUG_UPLOAD_STREAM_TYPE_MIC = 1,
    DEBUG_UPLOAD_STREAM_TYPE_REF = 2,
    DEBUG_UPLOAD_STREAM_TYPE_AEC = 3,
    DEBUG_UPLOAD_STREAM_TYPE_MAX = 4,
} DEBUG_UPLOAD_STREAM_TYPE;

/**
 * @brief Initializes the audio debug module.
 * @param None
 * @return OPERATE_RET - OPRT_OK if initialization is successful, otherwise an error code.
 */
OPERATE_RET ai_audio_debug_init(void);

/**
 * @brief Starts audio debugging by establishing TCP connections.
 * @param None
 * @return OPERATE_RET - OPRT_OK if the connections are successfully established, otherwise an error code.
 */

OPERATE_RET ai_audio_debug_start(void);

/**
 * @brief Handles and uploads audio data for debugging.
 * @param buf Pointer to the data buffer containing audio data.
 * @param len The length of the data buffer in bytes.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET ai_audio_debug_data(char *buf, uint32_t len);

/**
 * @brief Stops audio debugging by closing all TCP connections.
 * @param None
 * @return OPERATE_RET - OPRT_OK on success.
 */
OPERATE_RET ai_audio_debug_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_AUDIO_DEBUG_H__ */
