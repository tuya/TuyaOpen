/**
 * @file ai_audio_player.h
 * @brief AI audio player module header
 *
 * This header file defines the types and functions for the audio player module,
 * which provides functions to initialize, start, stop, and control audio playback
 * including TTS, music, and alert sounds.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_AUDIO_PLAYER_H__
#define __AI_AUDIO_PLAYER_H__

#include "tuya_cloud_types.h"
#include "svc_ai_player.h"

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
    AI_AUDIO_ALERT_POWER_ON,             /* Power on notification */
    AI_AUDIO_ALERT_NOT_ACTIVE,           /* Not activated, please configure network first */
    AI_AUDIO_ALERT_NETWORK_CFG,          /* Enter network configuration state, start configuration */
    AI_AUDIO_ALERT_NETWORK_CONNECTED,    /* Network connected successfully */
    AI_AUDIO_ALERT_NETWORK_FAIL,         /* Network connection failed, retry */
    AI_AUDIO_ALERT_NETWORK_DISCONNECT,   /* Network disconnected */
    AI_AUDIO_ALERT_BATTERY_LOW,          /* Low battery */
    AI_AUDIO_ALERT_PLEASE_AGAIN,         /* Please say again */
    AI_AUDIO_ALERT_LONG_KEY_TALK,        /* Long key press talk */
    AI_AUDIO_ALERT_KEY_TALK,             /* Key press talk */
    AI_AUDIO_ALERT_WAKEUP_TALK,          /* Wake-up talk */
    AI_AUDIO_ALERT_RANDOM_TALK,          /* Random talk */
    AI_AUDIO_ALERT_WAKEUP,               /* Hello, I'm here */
    AI_AUDIO_ALERT_MAX,
} AI_AUDIO_ALERT_TYPE_E;

typedef enum {
    AI_AUDIO_PLAYER_STAT_IDLE = 0,
    AI_AUDIO_PLAYER_STAT_START,
    AI_AUDIO_PLAYER_STAT_PLAY,
    AI_AUDIO_PLAYER_STAT_FINISH,
    AI_AUDIO_PLAYER_STAT_PAUSE,
    AI_AUDIO_PLAYER_STAT_MAX,
} AI_AUDIO_PLAYER_STATE_E;

typedef enum {
    AI_AUDIO_PLAYER_TTS_START,
    AI_AUDIO_PLAYER_TTS_DATA,
    AI_AUDIO_PLAYER_TTS_STOP,
    AI_AUDIO_PLAYER_TTS_ABORT,
} AI_AUDIO_PLAYER_TTS_STATE_E;

typedef enum {
    AI_AUDIO_PLAYER_FG = 0,   // frontground player, used to play tts
    AI_AUDIO_PLAYER_BG = 1,   // background player, used to play music
    AI_AUDIO_PLAYER_ALL = 2,  // all player
}AI_AUDIO_PLAYER_TYPE_E;

typedef struct {
    uint32_t                      id;
    char                         *url;
    uint64_t                      length;
    uint64_t                      duration;
    AI_AUDIO_CODEC_E              format;
    char                         *artist;
    char                         *song_name;
    char                         *audio_id;
    char                         *img_url;
}AI_MUSIC_SRC_T;

typedef struct {
    char                      action[32];     /* play/next/prev/resume/ */
    bool                      has_tts;        /* Need to wait for TTS playback to finish before playing media */
    int                       src_cnt;
    AI_MUSIC_SRC_T           *src_array;
}AI_AUDIO_MUSIC_T;

typedef enum {
    AI_HTTP_METHOD_GET,
    AI_HTTP_METHOD_POST,
    AI_HTTP_METHOD_PUT,
    AI_HTTP_METHOD_INVALD
}AI_HTTP_METHOD_E;

typedef enum {
    AI_TTS_TYPE_NORMAL,
    AI_TTS_TYPE_ALERT, 
    AI_TTS_TYPE_CALL,  
}AI_TTS_TYPE_E;

typedef struct {
    char                          *url;
    char                          *req_body;
    AI_HTTP_METHOD_E               http_method;
    AI_AUDIO_CODEC_E               format;
    AI_TTS_TYPE_E                  tts_type;
    int                            duration;
} AI_AUDIO_TTS_T;

typedef struct {
    AI_AUDIO_TTS_T      tts;
    AI_AUDIO_TTS_T      bg_music;
}AI_AUDIO_PLAY_TTS_T;

typedef OPERATE_RET (*AI_PLAYER_ALERT_CUSTOM_CB)(AI_AUDIO_ALERT_TYPE_E type);

/***********************************************************
********************function declaration********************
***********************************************************/
/**
@brief Initialize the audio player module
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_init(void);

/**
@brief Deinitialize the audio player module
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_deinit(void);

/**
@brief Start the audio player with the specified identifier
@param id The identifier for the current playback session (can be NULL)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_start(char *id);

/**
@brief Play TTS from URL
@param playtts Pointer to TTS play structure
@param is_loop Loop flag (unused)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_play_tts_url(AI_AUDIO_PLAY_TTS_T *playtts, bool is_loop);

/**
@brief Play audio data from memory
@param format Audio codec format
@param data Pointer to audio data
@param len Audio data length
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_play_data(AI_AUDIO_CODEC_E format, uint8_t *data, uint32_t len);

/**
@brief Play TTS stream data
@param state TTS stream state (START, DATA, STOP, ABORT)
@param codec Audio codec format
@param data Pointer to TTS data
@param len TTS data length
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_play_tts_stream(AI_AUDIO_PLAYER_TTS_STATE_E state, AI_AUDIO_CODEC_E codec, char *data,  int len);

/**
@brief Play music from playlist
@param music Pointer to music structure containing playlist
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_play_music(AI_AUDIO_MUSIC_T *music);

/**
@brief Stop all audio players
@param type Player type to stop (foreground, background, or all)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_stop(AI_AUDIO_PLAYER_TYPE_E type);

/**
@brief Set music continuous play flag
@param is_music_continuous Continuous play flag
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_set_resume(bool is_music_continuous);

/**
@brief Set music replay flag
@param is_music_replay Replay flag
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_set_replay(bool is_music_replay);

/**
@brief Check if audio player is currently playing
@return uint8_t Returns TRUE if playing, FALSE otherwise
*/
uint8_t ai_audio_player_is_playing(void);

/**
@brief Play alert audio
@param type Alert type
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_alert(AI_AUDIO_ALERT_TYPE_E type);

/**
@brief Set audio player volume
@param vol Volume value (0-100)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_set_vol(int vol);

/**
@brief Get audio player volume
@param vol Pointer to store volume value
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_get_vol(int *vol);

#if defined(AI_PLAYER_ALERT_SOURCE_CUSTOM) && (AI_PLAYER_ALERT_SOURCE_CUSTOM == 1)

OPERATE_RET ai_audio_player_reg_alert_cb(AI_PLAYER_ALERT_CUSTOM_CB cb);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __AI_AUDIO_PLAYER_H__ */
