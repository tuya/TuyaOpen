
/**
 * @file ai_audio_player.h
 * @brief Header file for the audio player module, which provides functions to initialize, start, stop, and control
 * audio playback.
 *
 * @version 0.1
 * @date 2025-03-25
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_AUDIO_PLAYER_H__
#define __AI_AUDIO_PLAYER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    AI_AUDIO_PLAYER_STAT_IDLE = 0,
    AI_AUDIO_PLAYER_STAT_START,
    AI_AUDIO_PLAYER_STAT_PLAY,
    AI_AUDIO_PLAYER_STAT_STOP,
    AI_AUDIO_PLAYER_STAT_MAX,
} AI_AUDIO_PLAYER_STATE_E;

typedef enum {
    AI_AUDIO_ALERT_NORMAL = 0,
    AI_AUDIO_ALERT_POWER_ON,
    AI_AUDIO_ALERT_NOT_ACTIVE,
    AI_AUDIO_ALERT_NETWORK_CFG,
    AI_AUDIO_ALERT_NETWORK_CONNECTED,
    AI_AUDIO_ALERT_NETWORK_FAIL,
    AI_AUDIO_ALERT_NETWORK_DISCONNECT,
    AI_AUDIO_ALERT_BATTERY_LOW,
    AI_AUDIO_ALERT_PLEASE_AGAIN,
    AI_AUDIO_ALERT_WAKEUP,
    AI_AUDIO_ALERT_LONG_KEY_TALK,
    AI_AUDIO_ALERT_KEY_TALK,
    AI_AUDIO_ALERT_WAKEUP_TALK,
    AI_AUDIO_ALERT_FREE_TALK,
} AI_AUDIO_ALERT_TYPE_E;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Initializes the audio player module, setting up necessary resources
 *        such as mutexes, queues, timers, ring buffers, and threads.
 *
 * @param None
 * @return OPERATE_RET - Returns OPRT_OK if initialization is successful, otherwise returns an error code.
 */
OPERATE_RET ai_audio_player_init(void);

/**
 * @brief Starts the audio player by posting a start state to the player's state queue.
 *
 * @param None
 * @return OPERATE_RET - Returns OPRT_OK if the start state is successfully posted, otherwise returns an error code.
 */
OPERATE_RET ai_audio_player_start(void);

/**
 * @brief Writes audio data to the ring buffer and sets the end-of-file flag if necessary.
 *
 * @param data - Pointer to the audio data to be written.
 * @param len - Length of the audio data to be written.
 * @param is_eof - Flag indicating if this is the end of the audio data (1 for true, 0 for false).
 * @return OPERATE_RET - Returns OPRT_OK if the data is successfully written, otherwise returns an error code.
 */
OPERATE_RET ai_audio_player_data_write(uint8_t *data, uint32_t len, uint8_t is_eof);

/**
 * @brief Stops the audio player and clears the audio output buffer.
 *
 * @param None
 * @return OPERATE_RET - Returns OPRT_OK if the player is successfully stopped, otherwise returns an error code.
 */
OPERATE_RET ai_audio_player_stop(void);

/**
 * @brief Plays an alert sound based on the specified alert type.
 *
 * @param type - The type of alert to play, defined by the APP_ALERT_TYPE enum.
 * @return OPERATE_RET - Returns OPRT_OK if the alert sound is successfully played, otherwise returns an error code.
 */
OPERATE_RET ai_audio_player_play_alert(AI_AUDIO_ALERT_TYPE_E type);

/**
 * @brief Plays an alert sound synchronously based on the specified alert type.
 * @param type The type of alert to play, defined by the AI_AUDIO_ALERT_TYPE_E enum.
 * @return OPERATE_RET - OPRT_OK if the alert sound is successfully played, otherwise an error code.
 */
OPERATE_RET ai_audio_player_play_alert_syn(AI_AUDIO_ALERT_TYPE_E type);

/**
 * @brief Checks if the audio player is currently playing audio.
 *
 * @param None
 * @return uint8_t - Returns 1 if the player is playing, 0 otherwise.
 */
uint8_t ai_audio_player_is_playing(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_AUDIO_PLAYER_H__ */
