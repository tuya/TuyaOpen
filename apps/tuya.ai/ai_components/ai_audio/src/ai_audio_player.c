/**
 * @file ai_audio_player.c
 * @brief This file contains the implementation of the audio player module, which is responsible for playing audio
 * streams.
 *
 * @version 0.1
 * @date 2025-03-25
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#define MINIMP3_IMPLEMENTATION

#include "tkl_system.h"
#include "tkl_memory.h"

#include "tal_api.h"
#include "tuya_ringbuf.h"
#include "svc_ai_player.h"

#include "tdl_audio_manage.h"

#include "media_src.h"
#include "ai_user_event.h"
#include "ai_agent.h"
#include "ai_audio_player.h"
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static bool __s_music_continuous = false;
static bool __s_music_replay = false;
static bool __s_tts_play_flag = false;
static AI_PLAYER_HANDLE __s_tone_player = NULL;
static AI_PLAYLIST_HANDLE __s_tone_playlist = NULL;
static AI_PLAYER_HANDLE __s_music_player = NULL;
static AI_PLAYLIST_HANDLE __s_music_playlist = NULL;
static AI_PLAYER_ALERT_CUSTOM_CB __s_alert_custom_cb = NULL;
/***********************************************************
***********************function define**********************
***********************************************************/
#if defined(AI_PLAYER_ALERT_SOURCE_LOCAL) && (AI_PLAYER_ALERT_SOURCE_LOCAL == 1)

OPERATE_RET __player_local_alert(AI_AUDIO_ALERT_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t *audio_data = NULL;
    uint32_t audio_size = 0;

    switch(type) {
    case AI_AUDIO_ALERT_POWER_ON:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_POWER_ON;
        audio_size = sizeof(LOCAL_ALERT_SRC_POWER_ON);
    break;
    case AI_AUDIO_ALERT_NOT_ACTIVE:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_NOT_ACTIVE;
        audio_size = sizeof(LOCAL_ALERT_SRC_NOT_ACTIVE);
    break;
    case AI_AUDIO_ALERT_NETWORK_CFG:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_NET_CFG;
        audio_size = sizeof(LOCAL_ALERT_SRC_NET_CFG);
    break;
    case AI_AUDIO_ALERT_NETWORK_FAIL:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_NET_FAILED;
        audio_size = sizeof(LOCAL_ALERT_SRC_NET_FAILED);
    break;
    case AI_AUDIO_ALERT_NETWORK_DISCONNECT:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_NET_DISCONNECT;
        audio_size = sizeof(LOCAL_ALERT_SRC_NET_DISCONNECT);
    break;
    case AI_AUDIO_ALERT_NETWORK_CONNECTED:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_NET_CONNECTED;
        audio_size = sizeof(LOCAL_ALERT_SRC_NET_CONNECTED);
    break;
    case AI_AUDIO_ALERT_BATTERY_LOW:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_LOW_BATTERY;
        audio_size = sizeof(LOCAL_ALERT_SRC_LOW_BATTERY);
    break;
    case AI_AUDIO_ALERT_PLEASE_AGAIN:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_PLEASE_AGAIN;
        audio_size = sizeof(LOCAL_ALERT_SRC_PLEASE_AGAIN);
    break;
    case AI_AUDIO_ALERT_LONG_KEY_TALK:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_LONG_KEY_TALK;
        audio_size = sizeof(LOCAL_ALERT_SRC_LONG_KEY_TALK);
    break;
    case AI_AUDIO_ALERT_KEY_TALK:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_KEY_TALK;
        audio_size = sizeof(LOCAL_ALERT_SRC_KEY_TALK);
    break;
    case AI_AUDIO_ALERT_WAKEUP_TALK:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_WAKEUP_TALK;
        audio_size = sizeof(LOCAL_ALERT_SRC_WAKEUP_TALK);
    break;
    case AI_AUDIO_ALERT_RANDOM_TALK:
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_FREE_TALK;
        audio_size = sizeof(LOCAL_ALERT_SRC_FREE_TALK);
    break;
    case AI_AUDIO_ALERT_WAKEUP: 
        audio_data = (uint8_t*)LOCAL_ALERT_SRC_WAKEUP;
        audio_size = sizeof(LOCAL_ALERT_SRC_WAKEUP);
    break;
    default:
        PR_NOTICE("audio player -> local alert type: %d not support", type);
        break;
    }

    if(audio_data && audio_size) {
        TUYA_CALL_ERR_LOG(ai_audio_play_data(AI_AUDIO_CODEC_MP3, audio_data, audio_size));
    }

    return rt;
}

#endif

#if defined(AI_AGENT_ENABLE_CLOUD_ALERT) && (AI_AGENT_ENABLE_CLOUD_ALERT == 1)
OPERATE_RET __player_cloud_alert(AI_AUDIO_ALERT_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;

    rt = ai_agent_cloud_alert(type);
    if(rt != OPRT_OK) {
        TUYA_CALL_ERR_LOG(ai_audio_play_data(AI_AUDIO_CODEC_MP3, \
                        (uint8_t*)media_src_dingdong, \
                        sizeof(media_src_dingdong)));
    }

    return rt;
}

#endif


/**
@brief Player event callback function
@param data Pointer to player event data
@return OPERATE_RET Operation result
*/
static OPERATE_RET __player_event(void *data)
{
    TUYA_CHECK_NULL_RETURN(data, OPRT_OK);    

    AI_PLAYER_EVT_T *event = (AI_PLAYER_EVT_T*)data;
    PR_DEBUG("audio player -> player %s event: %d", (event->handle == __s_tone_player) ? "tts" : "music", event->state);

    OPERATE_RET rt = OPRT_OK;
    int player_vol = 0;
    tuya_ai_player_get_volume(NULL, &player_vol);
    /* Player finished or failed */
    if (event->state == AI_PLAYER_STOPPED) {
        PR_DEBUG("audio player -> stop event");
        if (!ai_audio_player_is_playing()) {
            ai_user_event_notify(AI_USER_EVT_PLAY_END, NULL);
        }else {
			PR_DEBUG("audio player -> playing stop event, music vol change to %d", player_vol);
			tuya_ai_player_set_volume(__s_music_player, player_vol);	
		}

        /* If TTS play stop, reset play flag */
        if(event->handle == __s_tone_player && __s_tts_play_flag) {
            __s_tts_play_flag = FALSE;
        }
    }
    else if (event->state == AI_PLAYER_PLAYING) {
        PR_DEBUG("audio player -> playing start event");
        if (event->handle == __s_tone_player && AI_PLAYER_PLAYING == tuya_ai_player_get_state(__s_music_player)) {
            PR_DEBUG("audio player -> playing start event, music vol change to %d", player_vol/2);
            tuya_ai_player_set_volume(__s_music_player, player_vol/2);
        }
        ai_user_event_notify(AI_USER_EVT_PLAY_CTL_PLAY, NULL);
    } else if (event->state == AI_PLAYER_PAUSED) {
        PR_DEBUG("audio player -> pause event");
        ai_user_event_notify(AI_USER_EVT_PLAY_CTL_PAUSE, NULL);
    }
    
    return rt;
}

/**
@brief Initialize the audio player module
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    
    /* Player init */
    AI_PLAYER_CFG_T cfg = {.sample = 16000, .datebits = 16, .channel = 1};
    TUYA_CALL_ERR_GOTO(tuya_ai_player_service_init(&cfg), __error);

    /* TTS player */
    TUYA_CALL_ERR_GOTO(tuya_ai_player_create(AI_PLAYER_MODE_FOREGROUND, &__s_tone_player), __error);

    AI_PLAYLIST_CFG_T ton_cfg = {.auto_play = true,.capacity = 2};
    TUYA_CALL_ERR_GOTO(tuya_ai_playlist_create(__s_tone_player, &ton_cfg, &__s_tone_playlist), __error);

    /* Music player */
    TUYA_CALL_ERR_GOTO(tuya_ai_player_create(AI_PLAYER_MODE_BACKGROUND, &__s_music_player), __error);

    AI_PLAYLIST_CFG_T misc_cfg = {.auto_play = true,.capacity = 32};
    TUYA_CALL_ERR_GOTO(tuya_ai_playlist_create(__s_music_player, &misc_cfg, &__s_music_playlist), __error);

    /* Player state */
    TUYA_CALL_ERR_GOTO(tal_event_subscribe(EVENT_AI_PLAYER_STATE, "ai_player", __player_event, SUBSCRIBE_TYPE_NORMAL), __error);

    return rt;

__error:
    ai_audio_player_deinit();

    return rt;
}

/**
@brief Deinitialize the audio player module
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (__s_tone_player) {
        TUYA_CALL_ERR_LOG(tuya_ai_player_destroy(__s_tone_player));
        __s_tone_player = NULL;
    }
    
    if (__s_tone_playlist) {
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_destroy(__s_tone_playlist));
        __s_tone_playlist = NULL;
    }

    if (__s_music_player) {
        TUYA_CALL_ERR_LOG(tuya_ai_player_destroy(__s_music_player));
        __s_music_player = NULL;
    }
    
    if (__s_music_playlist) {
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_destroy(__s_music_playlist));
        __s_music_playlist = NULL;
    }

    /* Player deinit */
    TUYA_CALL_ERR_RETURN(tuya_ai_player_service_deinit());
    
    return rt;
}

/**
@brief Set music continuous play flag
@param is_music_continuous Continuous play flag
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_set_resume(bool is_music_continuous)
{
    __s_music_continuous = is_music_continuous;
    return OPRT_OK;
}

/**
@brief Set music replay flag
@param is_music_replay Replay flag
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_set_replay(bool is_music_replay)
{
    __s_music_replay = is_music_replay;
    return OPRT_OK;
}

/**
@brief Check if audio player is currently playing
@return uint8_t Returns TRUE if playing, FALSE otherwise
*/
uint8_t ai_audio_player_is_playing(void)
{
    if (tuya_ai_player_get_state(__s_tone_player) == AI_PLAYER_PLAYING ||
        tuya_ai_player_get_state(__s_music_player) == AI_PLAYER_PLAYING) {
            return TRUE;
    } 

    return FALSE;
}

/**
@brief Play music from playlist
@param music Pointer to music structure containing playlist
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_play_music(AI_AUDIO_MUSIC_T *music)
{
    OPERATE_RET rt = OPRT_OK;
    if (music->src_cnt <= 0) {
        PR_ERR("music src cnt is 0");
        return rt;
    }

    TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_music_playlist));
    for (int i = 0; i < music->src_cnt; i++) {

        PR_DEBUG("audio player -> player music url %s", music->src_array[i].url);
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_add(__s_music_playlist, AI_PLAYER_SRC_URL,\
                                               music->src_array[i].url, music->src_array[i].format));
    }

    return rt;
}

/**
@brief Play TTS from URL
@param playtts Pointer to TTS play structure
@param is_loop Loop flag (unused)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_play_tts_url(AI_AUDIO_PLAY_TTS_T *playtts, bool is_loop)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(playtts, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(playtts->tts.url, OPRT_INVALID_PARM);

    PR_DEBUG("audio player -> player tts url %s", playtts->tts.url);
    TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_tone_playlist));
    TUYA_CALL_ERR_LOG(tuya_ai_playlist_add(__s_tone_playlist, AI_PLAYER_SRC_URL, \
                                           playtts->tts.url, playtts->tts.format));

    return rt;
}

/**
@brief Play audio data from memory
@param format Audio codec format
@param data Pointer to audio data
@param len Audio data length
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_play_data(AI_AUDIO_CODEC_E format, uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_LOG(tuya_ai_playlist_stop(__s_tone_playlist));

    if (data && len > 0) {
        PR_NOTICE("audio player -> player tts mem data len %d", len);
        TUYA_CALL_ERR_LOG(tuya_ai_player_start(__s_tone_player, AI_PLAYER_SRC_MEM, NULL, format));
        TUYA_CALL_ERR_LOG(tuya_ai_player_feed(__s_tone_player, (uint8_t *)data, len));
        TUYA_CALL_ERR_LOG(tuya_ai_player_feed(__s_tone_player, NULL, 0));
    } 

    return rt;
}

/**
@brief Play TTS stream data
@param state TTS stream state (START, DATA, STOP, ABORT)
@param codec Audio codec format
@param data Pointer to TTS data
@param len TTS data length
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_play_tts_stream(AI_AUDIO_PLAYER_TTS_STATE_E state, AI_AUDIO_CODEC_E codec, char *data,  int len)
{
    OPERATE_RET rt = OPRT_OK;
    
    switch(state) {
    case AI_AUDIO_PLAYER_TTS_START:
        PR_DEBUG("audio player -> tts stream start");
        __s_tts_play_flag = TRUE;
        ai_user_event_notify(AI_USER_EVT_TTS_PRE, NULL);
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_tone_playlist));
        TUYA_CALL_ERR_LOG(tuya_ai_player_start(__s_tone_player, AI_PLAYER_SRC_MEM, NULL, codec));
        ai_user_event_notify(AI_USER_EVT_TTS_START, NULL);
        break;
    case AI_AUDIO_PLAYER_TTS_DATA:
        // PR_DEBUG("audio player -> tts stream data %d", len);        
        if (data && len > 0 && __s_tts_play_flag) {
            TUYA_CALL_ERR_LOG(tuya_ai_player_feed(__s_tone_player, (uint8_t *)data, len));
        }
        ai_user_event_notify(AI_USER_EVT_TTS_DATA, NULL);
        break;
    case AI_AUDIO_PLAYER_TTS_STOP:
        PR_DEBUG("audio player -> tts stream stop");
        TUYA_CALL_ERR_LOG(tuya_ai_player_feed(__s_tone_player, NULL, 0));
        ai_user_event_notify(AI_USER_EVT_TTS_STOP, NULL);
        break;
    case AI_AUDIO_PLAYER_TTS_ABORT:
        PR_DEBUG("audio player -> tts stream abort");
        TUYA_CALL_ERR_LOG(tuya_ai_player_feed(__s_tone_player, NULL, 0));
        ai_user_event_notify(AI_USER_EVT_TTS_ABORT, NULL);
        break;
    default:
        break;
    }

    return rt;
}

/**
@brief Play local audio file
@param url Audio file URL
@param song_name Song name (unused)
@param artist Artist name (unused)
@param format Audio format
@param size File size (unused)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_play_local(char *url, char *song_name, char *artist, int format, int size)
{
    OPERATE_RET rt = OPRT_OK;
	PR_DEBUG("audio player -> play local url: %s", url);
    AI_PLAYER_SRC_E src = (strstr(url, "http://") == url || strstr(url, "https://") == url) ?
                           AI_PLAYER_SRC_URL : AI_PLAYER_SRC_FILE;
    TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_music_playlist));
    TUYA_CALL_ERR_LOG(tuya_ai_playlist_add(__s_music_playlist, src, url, format));

    return rt;
}

/**
@brief Stop all audio players
@param type Player type to stop (foreground, background, or all)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_stop(AI_AUDIO_PLAYER_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("audio player -> stop all player");

    switch (type)
    {
    case AI_AUDIO_PLAYER_FG:
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_tone_playlist));
        TUYA_CALL_ERR_LOG(tuya_ai_player_stop(__s_tone_player));
        break;
    case AI_AUDIO_PLAYER_BG:
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_music_playlist));
        TUYA_CALL_ERR_LOG(tuya_ai_player_stop(__s_music_player));
        break;
    case AI_AUDIO_PLAYER_ALL:
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_tone_playlist));
        TUYA_CALL_ERR_LOG(tuya_ai_player_stop(__s_tone_player));
        TUYA_CALL_ERR_LOG(tuya_ai_playlist_clear(__s_music_playlist));
        TUYA_CALL_ERR_LOG(tuya_ai_player_stop(__s_music_player));
        break;
    default:
        break;
    }

    return rt;
}

/**
@brief Play alert audio
@param type Alert type
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_alert(AI_AUDIO_ALERT_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;

    PR_NOTICE("audio player -> play alert type=%d", type);

#if defined(AI_PLAYER_ALERT_SOURCE_LOCAL) && (AI_PLAYER_ALERT_SOURCE_LOCAL == 1)
    __player_local_alert(type);

#elif defined(AI_AGENT_ENABLE_CLOUD_ALERT) && (AI_AGENT_ENABLE_CLOUD_ALERT == 1)
    __player_cloud_alert(type);

#elif defined(AI_PLAYER_ALERT_SOURCE_CUSTOM) && (AI_PLAYER_ALERT_SOURCE_CUSTOM == 1)
    if(__s_alert_custom_cb) {
       rt = __s_alert_custom_cb(type);
    }else {
        PR_ERR("audio player -> alert custom cb is NULL");
        rt = OPRT_NOT_SUPPORTED;
    }

#endif

    return rt;
}

/**
@brief Set audio player volume
@param vol Volume value (0-100)
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_set_vol(int vol)
{
    PR_DEBUG("audio player -> set volume %d", vol);

    return tuya_ai_player_set_volume(__s_tone_player, vol);
}

/**
@brief Get audio player volume
@param vol Pointer to store volume value
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_audio_player_get_vol(int *vol)
{
    return tuya_ai_player_get_volume(__s_tone_player, vol);
}

OPERATE_RET ai_audio_player_reg_alert_cb(AI_PLAYER_ALERT_CUSTOM_CB cb)
{
    __s_alert_custom_cb = cb;

    return OPRT_OK;
}
