/**
 * @file tdl_audio_manage.h
 * @brief Tuya Driver Layer audio management interface.
 *
 * This file provides the high-level audio management interface for the Tuya
 * Driver Layer (TDL). It defines functions for discovering, opening, controlling,
 * and managing audio devices within the TDL framework. This interface allows
 * applications to interact with registered audio drivers in a unified manner,
 * regardless of the underlying hardware implementation.
 *
 * Key functionalities:
 * - Audio device discovery by name
 * - Opening audio devices with microphone callback support
 * - Audio playback control
 * - Volume adjustment
 * - Device lifecycle management
 *
 * The TDL audio management layer provides a simplified API that abstracts
 * the complexity of individual audio drivers while maintaining flexibility
 * for various audio use cases in Tuya IoT devices.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_AUDIO_MANAGE_H__
#define __TDL_AUDIO_MANAGE_H__

#include "tuya_cloud_types.h"

#include "tdl_audio_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef void *TDL_AUDIO_HANDLE_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/
/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Find a registered audio device by name.
 *
 * @param[in]  name   Audio device name, normally AUDIO_CODEC_NAME.
 * @param[out] handle Receives the TDL audio handle.
 *
 * @return OPRT_OK on success, otherwise an error code.
 */
OPERATE_RET tdl_audio_find(char *name, TDL_AUDIO_HANDLE_T *handle);

/**
 * @brief Open an audio device and start capture/playback.
 *
 * The microphone callback may be NULL when only playback is required.
 *
 * @param[in] handle Audio handle returned by tdl_audio_find().
 * @param[in] mic_cb Microphone data callback, or NULL for playback-only use.
 *
 * @return OPRT_OK on success, otherwise an error code.
 */
OPERATE_RET tdl_audio_open(TDL_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb);

/**
 * @brief Get the audio format advertised by a device.
 *
 * @param[in]  handle Audio handle.
 * @param[out] info   Receives independent capture and playback capabilities.
 *
 * @return OPRT_OK on success, otherwise an error code.
 */
OPERATE_RET tdl_audio_get_info(TDL_AUDIO_HANDLE_T handle, TDL_AUDIO_INFO_T *info);

/**
 * @brief Send a playback buffer to an audio device.
 *
 * The buffer format must match info.playback.format. The call may block while
 * the TDD driver submits or drains the buffer.
 *
 * @param[in] handle Audio handle.
 * @param[in] data   Playback buffer.
 * @param[in] len    Buffer length in bytes.
 *
 * @return OPRT_OK on success, otherwise an error code.
 */
OPERATE_RET tdl_audio_play(TDL_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len);

/**
 * @brief Stop playback and discard pending playback data.
 *
 * @param[in] handle Audio handle.
 *
 * @return OPRT_OK on success, otherwise an error code.
 */
OPERATE_RET tdl_audio_play_stop(TDL_AUDIO_HANDLE_T handle);

/**
 * @brief Set playback volume.
 *
 * @param[in] handle Audio handle.
 * @param[in] volume Volume in the range 0..100.
 *
 * @return OPRT_OK on success, otherwise an error code.
 */
OPERATE_RET tdl_audio_volume_set(TDL_AUDIO_HANDLE_T handle, uint8_t volume);

/**
 * @brief Register a callback for wake/sleep/ASR events.
 *
 * @param[in] handle    Audio handle.
 * @param[in] cb        Event callback, or NULL to clear it.
 * @param[in] user_data User pointer passed to the callback.
 *
 * @return OPRT_OK on success, otherwise an error code.
 */
OPERATE_RET tdl_audio_event_register(TDL_AUDIO_HANDLE_T handle,
                                     TDL_AUDIO_EVENT_CB cb, void *user_data);

/**
 * @brief Close an audio device and stop capture/playback.
 *
 * @param[in] handle Audio handle.
 *
 * @return OPRT_OK on success, otherwise an error code.
 */
OPERATE_RET tdl_audio_close(TDL_AUDIO_HANDLE_T handle);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_AUDIO_MANAGE_H__ */
