/**
 * @file tuya_audio_debug.h
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

#ifndef __TUYA_AUDIO_DEBUG_H__
#define __TUYA_AUDIO_DEBUG_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TUYA_AUDIO_DEBUG 1

typedef struct {
    uint8_t *buf;
    uint32_t len;
} TY_AI_AUDIO_FRAME_T;

typedef enum {
    DEBUG_UPLOAD_STREAM_TYPE_RAW = 0,
    DEBUG_UPLOAD_STREAM_TYPE_MIC = 1,
    DEBUG_UPLOAD_STREAM_TYPE_REF = 2,
    DEBUG_UPLOAD_STREAM_TYPE_AEC = 3,
    DEBUG_UPLOAD_STREAM_TYPE_MAX = 4,
} DEBUG_UPLOAD_STREAM_TYPE;

/**
 * @brief Initialize the audio debug module.
 *
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET tuya_audio_debug_init(void);

/**
 * @brief Callback function to start audio debugging.
 *
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET tuya_audio_debug_start_cb(void);

/**
 * @brief Callback function to handle data for audio debugging.
 *
 * @param buf Pointer to the data buffer.
 * @param len The length of the data buffer.
 * @return OPERATE_RET - OPRT_OK on success, or an error code on failure.
 */
OPERATE_RET tuya_audio_debug_data_cb(char *buf, uint32_t len);

/**
 * @brief Callback function to stop audio debugging.
 *
 * @return OPERATE_RET - OPRT_OK on success.
 */
OPERATE_RET tuya_audio_debug_stop_cb(void);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_AUDIO_DEBUG_H__ */
