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
#include "tkl_thread.h"

#include "tal_api.h"
#include "tuya_ringbuf.h"

#include "tdl_audio_manage.h"

#include "ai_media_alert.h"
#include "minimp3_ex.h"
#include "ai_audio.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define MP3_STREAM_BUFF_MAX_LEN (1024 * 64 * 2)

#define MAINBUF_SIZE 1940

#define MAX_NGRAN 2   /* max granules */
#define MAX_NCHAN 2   /* max channels */
#define MAX_NSAMP 576 /* max samples per channel, per granule */

#define MP3_PCM_SIZE_MAX           (MAX_NSAMP * MAX_NCHAN * MAX_NGRAN * 2)
#define PLAYING_NO_DATA_TIMEOUT_MS (5 * 1000)

#define AI_AUDIO_PLAYER_STAT_CHANGE(last_stat, new_stat)                              \
    do {                                                                              \
        if(last_stat != new_stat) {                                                   \
            PR_DEBUG("ai audio player stat changed: %d->%d", last_stat, new_stat);    \
        }                                                                             \
    } while (0)                                                                       \

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    bool                    is_playing;
    bool                    is_writing;
    AI_AUDIO_PLAYER_STATE_E stat;

    TDL_AUDIO_HANDLE_T      audio_hdl;
    MUTEX_HANDLE            mutex;
    THREAD_HANDLE           thrd_hdl;

    char                   *id;
    TUYA_RINGBUFF_T         rb_hdl;
    MUTEX_HANDLE            spk_rb_mutex;
    uint8_t                 is_eof;
    TIMER_ID                tm_id;

    mp3dec_t               *mp3_dec;
    mp3dec_frame_info_t     mp3_frame_info;
    uint8_t                *mp3_raw;
    uint8_t                *mp3_raw_head;
    uint32_t                mp3_raw_used_len;
    uint8_t                *mp3_pcm; // mp3 decode to pcm buffer

} APP_PLAYER_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_PLAYER_T sg_player;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __ai_audio_player_mp3_start(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_player.mp3_dec) {
        sg_player.mp3_dec = (mp3dec_t *)tkl_system_psram_malloc(sizeof(mp3dec_t));
        if (NULL == sg_player.mp3_dec) {
            PR_ERR("malloc mp3dec_t failed");
            return OPRT_MALLOC_FAILED;
        }

        mp3dec_init(sg_player.mp3_dec);
    }

    sg_player.mp3_raw_used_len = 0;

    return rt;
}

static OPERATE_RET __ai_audio_player_mp3_playing(void)
{
    OPERATE_RET rt = OPRT_OK;
    APP_PLAYER_T *ctx = &sg_player;

    if (NULL == ctx->mp3_dec) {
        PR_ERR("mp3 decoder is NULL");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_player.spk_rb_mutex);
    uint32_t rb_used_len = tuya_ring_buff_used_size_get(ctx->rb_hdl);
    tal_mutex_unlock(sg_player.spk_rb_mutex);
    if (0 == rb_used_len && 0 == ctx->mp3_raw_used_len) {
        // PR_DEBUG("mp3 data is empty");
        rt = OPRT_RECV_DA_NOT_ENOUGH;
        goto __EXIT;
    }

    if (NULL != ctx->mp3_raw_head && ctx->mp3_raw_used_len > 0 && ctx->mp3_raw_head != ctx->mp3_raw) {
        // PR_DEBUG("move data, offset=%d, used_len=%d", ctx->mp3_raw_head - ctx->mp3_raw, ctx->mp3_raw_used_len);
        memmove(ctx->mp3_raw, ctx->mp3_raw_head, ctx->mp3_raw_used_len);
    }
    ctx->mp3_raw_head = ctx->mp3_raw;

    // read new data
    if (rb_used_len > 0 && ctx->mp3_raw_used_len < MAINBUF_SIZE) {
        uint32_t read_len = ((MAINBUF_SIZE - ctx->mp3_raw_used_len) > rb_used_len) ?\
                             rb_used_len : (MAINBUF_SIZE - ctx->mp3_raw_used_len);
          
        tal_mutex_lock(sg_player.spk_rb_mutex);                     
        uint32_t rt_len = tuya_ring_buff_read(ctx->rb_hdl, ctx->mp3_raw + ctx->mp3_raw_used_len, read_len);
        tal_mutex_unlock(sg_player.spk_rb_mutex);  
        //  PR_DEBUG("read_len=%d rt_len: %d", read_len, rt_len);

        ctx->mp3_raw_used_len += rt_len;
    }

    int samples = mp3dec_decode_frame(ctx->mp3_dec, ctx->mp3_raw_head, ctx->mp3_raw_used_len,
                                      (mp3d_sample_t *)ctx->mp3_pcm, &ctx->mp3_frame_info);
    if (samples == 0) {
        ctx->mp3_raw_used_len = 0;
        ctx->mp3_raw_head = ctx->mp3_raw;
        rt = OPRT_COM_ERROR;
        goto __EXIT;
    }

    ctx->mp3_raw_used_len -= ctx->mp3_frame_info.frame_bytes;
    ctx->mp3_raw_head += ctx->mp3_frame_info.frame_bytes;

    tdl_audio_play(ctx->audio_hdl, ctx->mp3_pcm, samples * 2);

__EXIT:
    return rt;
}

static OPERATE_RET __ai_audio_player_mp3_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("app player mp3 init...");

    sg_player.mp3_raw = (uint8_t *)tkl_system_psram_malloc(MAINBUF_SIZE);
    TUYA_CHECK_NULL_GOTO(sg_player.mp3_raw, __ERR);

    sg_player.mp3_pcm = (uint8_t *)tkl_system_psram_malloc(MP3_PCM_SIZE_MAX);
    TUYA_CHECK_NULL_GOTO(sg_player.mp3_pcm, __ERR);

    return rt;

__ERR:
    if (sg_player.mp3_pcm) {
        tkl_system_psram_free(sg_player.mp3_pcm);
        sg_player.mp3_pcm = NULL;
    }

    if (sg_player.mp3_raw) {
        tkl_system_psram_free(sg_player.mp3_raw);
        sg_player.mp3_raw = NULL;
    }

    return OPRT_COM_ERROR;
}

static void __ai_audio_player_task(void *arg)
{
    OPERATE_RET rt = OPRT_OK;
    APP_PLAYER_T *ctx = &sg_player;
    static AI_AUDIO_PLAYER_STATE_E last_state = 0xFF;

    ctx->stat = AI_AUDIO_PLAYER_STAT_IDLE;

    for (;;) {
        tal_mutex_lock(sg_player.mutex);

        AI_AUDIO_PLAYER_STAT_CHANGE(last_state, ctx->stat);
        last_state = ctx->stat;

        switch (ctx->stat) {
        case AI_AUDIO_PLAYER_STAT_IDLE: {
            if (tal_sw_timer_is_running(ctx->tm_id)) {
                tal_sw_timer_stop(ctx->tm_id);
            }
            ctx->is_eof     = 0;
        } break;
        case AI_AUDIO_PLAYER_STAT_START: {
            rt = __ai_audio_player_mp3_start();
            if (rt != OPRT_OK) {
                ctx->stat = AI_AUDIO_PLAYER_STAT_IDLE;
            } else {
                ctx->stat = AI_AUDIO_PLAYER_STAT_PLAY;
            }
        } break;
        case AI_AUDIO_PLAYER_STAT_PLAY: {
            rt = __ai_audio_player_mp3_playing();
            if (OPRT_RECV_DA_NOT_ENOUGH == rt) {
                tal_sw_timer_start(ctx->tm_id, PLAYING_NO_DATA_TIMEOUT_MS, TAL_TIMER_ONCE);
            } else if (OPRT_OK == rt) {
                if (tal_sw_timer_is_running(ctx->tm_id)) {
                    tal_sw_timer_stop(ctx->tm_id);
                }
            }
            tal_mutex_lock(ctx->spk_rb_mutex);
            uint32_t rb_used_len = tuya_ring_buff_used_size_get(ctx->rb_hdl);
            tal_mutex_unlock(ctx->spk_rb_mutex);
            if (rb_used_len == 0 && 0 == ctx->mp3_raw_used_len && ctx->is_eof) {
                PR_DEBUG("app player end");
                ctx->stat = AI_AUDIO_PLAYER_STAT_FINISH;
            }
        } break;
        case AI_AUDIO_PLAYER_STAT_FINISH: {
            tal_sw_timer_stop(ctx->tm_id);

            ctx->is_playing = false;
            ctx->stat       = AI_AUDIO_PLAYER_STAT_IDLE;
            ctx->is_eof     = 0;
        } break;
        case AI_AUDIO_PLAYER_STAT_PAUSE:
            // do nothing
        break;
        default:
            break;
        }

        tal_mutex_unlock(sg_player.mutex);

        tal_system_sleep(10);
    }
}

static void __app_playing_tm_cb(TIMER_ID timer_id, void *arg)
{
    PR_DEBUG("app player timeout cb, stop playing");
    tal_mutex_lock(sg_player.mutex);
    sg_player.stat = AI_AUDIO_PLAYER_STAT_FINISH;
    tal_mutex_unlock(sg_player.mutex);
    return;
}

static bool __app_player_compare_id(char *id_1, char *id_2)
{

    if(NULL == id_1 && NULL == id_1) {
        return true;
    }

    if(id_1 && id_2) {
        if(0 == strcmp(id_1, id_2)) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Initializes the audio player module, setting up necessary resources
 *        such as mutexes, queues, timers, ring buffers, and threads.
 *
 * @param None
 * @return OPERATE_RET - Returns OPRT_OK if initialization is successful, otherwise returns an error code.
 */
OPERATE_RET ai_audio_player_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    memset(&sg_player, 0, sizeof(APP_PLAYER_T));

    PR_DEBUG("app player init...");

    TUYA_CALL_ERR_GOTO(tdl_audio_find(AUDIO_CODEC_NAME, &sg_player.audio_hdl), __ERR);

    // create mutex
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_player.mutex), __ERR);

    TUYA_CALL_ERR_GOTO(tal_sw_timer_create(__app_playing_tm_cb, NULL, &sg_player.tm_id), __ERR);

    TUYA_CALL_ERR_GOTO(__ai_audio_player_mp3_init(), __ERR);
    // ring buffer init
    TUYA_CALL_ERR_GOTO(tuya_ring_buff_create(MP3_STREAM_BUFF_MAX_LEN, \
                                             OVERFLOW_PSRAM_STOP_TYPE,\
                                             &sg_player.rb_hdl), __ERR);
    //ring buffer mutex init
    TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_player.spk_rb_mutex), __ERR);

    // thread init
    TUYA_CALL_ERR_GOTO(tkl_thread_create_in_psram(&sg_player.thrd_hdl, \
                                                  "ai_player", \
                                                   1024 * 4,\
                                                   THREAD_PRIO_2,
                                                   __ai_audio_player_task, NULL), __ERR);

    PR_DEBUG("app player init success");

    return rt;

__ERR:
    if (sg_player.mutex) {
        tal_mutex_release(sg_player.mutex);
        sg_player.mutex = NULL;
    }

    if (sg_player.spk_rb_mutex) {
        tal_mutex_release(sg_player.spk_rb_mutex);
        sg_player.spk_rb_mutex = NULL;
    }

    if (sg_player.rb_hdl) {
        tuya_ring_buff_free(sg_player.rb_hdl);
        sg_player.rb_hdl = NULL;
    }

    return rt;
}

/**
 * @brief Starts the audio player with the specified identifier.
 *
 *
 * @param id        The identifier for the current playback session. 
 *                  If NULL, no specific ID is set.
 * 
 * @return          Returns OPRT_OK if the player is successfully started.
 */
OPERATE_RET ai_audio_player_start(char *id)
{
    tal_mutex_lock(sg_player.mutex);
 
    if(true == sg_player.is_playing) {
        PR_NOTICE("player is already start");
        tal_mutex_unlock(sg_player.mutex);
        return OPRT_OK;
    }

    if(sg_player.id) {
        tkl_system_free(sg_player.id);
        sg_player.id = NULL;
    }

    if(id) {
        sg_player.id = tkl_system_malloc(strlen(id) + 1);
        if(sg_player.id) {
            strcpy(sg_player.id, id);
            sg_player.id[strlen(id)] = '\0';
        }
    }

    sg_player.is_playing = true;
    sg_player.stat = AI_AUDIO_PLAYER_STAT_START;

    tal_mutex_unlock(sg_player.mutex);

    PR_NOTICE("ai audio player start");

    return OPRT_OK;
}

/**
 * @brief Writes audio data to the ring buffer and sets the end-of-file flag if necessary.
 * 
 * @param id        The identifier to validate against the current player's ID.
 * @param data      Pointer to the audio data to be written into the buffer.
 * @param len       Length of the audio data to be written.
 * @param is_eof    Flag indicating whether this block of data is the end of the stream (1 for true, 0 for false).
 * 
 * @return          Returns OPRT_OK if the data was successfully written to the buffer, otherwise returns an error code.
 */
OPERATE_RET ai_audio_player_data_write(char *id, uint8_t *data, uint32_t len, uint8_t is_eof)
{
    uint32_t write_len = 0, alreay_write_len = 0;

    tal_mutex_lock(sg_player.mutex);

    if (AI_AUDIO_PLAYER_STAT_PLAY != sg_player.stat &&\
        AI_AUDIO_PLAYER_STAT_START != sg_player.stat){
        tal_mutex_unlock(sg_player.mutex);
        return OPRT_COM_ERROR;
    }

    if(false == __app_player_compare_id(id, sg_player.id)) {
        PR_NOTICE("the id:%s is not match... curr id:%s", id, sg_player.id);
        tal_mutex_unlock(sg_player.mutex);
        return OPRT_INVALID_PARM; 
    }


    if (NULL != data && len > 0) {    
        while((alreay_write_len < len) && \
              (AI_AUDIO_PLAYER_STAT_PLAY == sg_player.stat ||\
               AI_AUDIO_PLAYER_STAT_START == sg_player.stat)) {

            sg_player.is_writing = true;
            tal_mutex_lock(sg_player.spk_rb_mutex);
            uint32_t rb_free_len = tuya_ring_buff_free_size_get(sg_player.rb_hdl);
            tal_mutex_unlock(sg_player.spk_rb_mutex);
            if(0 == rb_free_len) {
                //need unlock mutex before sleep
                tal_mutex_unlock(sg_player.mutex);
                tal_system_sleep(3);
                tal_mutex_lock(sg_player.mutex);
                continue;
            }
    
            write_len = GET_MIN_LEN(rb_free_len, (len - alreay_write_len));
    
            tal_mutex_lock(sg_player.spk_rb_mutex);
            tuya_ring_buff_write(sg_player.rb_hdl, data + alreay_write_len, write_len);
            tal_mutex_unlock(sg_player.spk_rb_mutex);
    
            alreay_write_len += write_len;
        };
        sg_player.is_writing = false;
    }

    sg_player.is_eof = is_eof;
    tal_mutex_unlock(sg_player.mutex);

    return OPRT_OK;
}

/**
 * @brief Stops the audio player and clears the audio output buffer.
 *
 * @param None
 * @return OPERATE_RET - Returns OPRT_OK if the player is successfully stopped, otherwise returns an error code.
 */
OPERATE_RET ai_audio_player_stop(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_mutex_lock(sg_player.mutex);

    if (false == sg_player.is_playing) {
        tal_mutex_unlock(sg_player.mutex);
        return OPRT_OK;
    }

    sg_player.stat = AI_AUDIO_PLAYER_STAT_PAUSE;

    if(sg_player.id) {
        tkl_system_free(sg_player.id);
        sg_player.id = NULL;
    }

    while(sg_player.is_writing) {
        tal_mutex_unlock(sg_player.mutex);
        tal_system_sleep(3);
        tal_mutex_lock(sg_player.mutex);
    }

    tal_mutex_lock(sg_player.spk_rb_mutex);
    tuya_ring_buff_reset(sg_player.rb_hdl);
    tal_mutex_unlock(sg_player.spk_rb_mutex);

    tdl_audio_play_stop(sg_player.audio_hdl);

    sg_player.is_playing = false;
    sg_player.stat = AI_AUDIO_PLAYER_STAT_IDLE;

    tal_mutex_unlock(sg_player.mutex);

    PR_NOTICE("ai audio player stop");

    return rt;
}

/**
 * @brief Plays an alert sound based on the specified alert type.
 *
 * @param type - The type of alert to play, defined by the APP_ALERT_TYPE_E enum.
 * @return OPERATE_RET - Returns OPRT_OK if the alert sound is successfully played, otherwise returns an error code.
 */
OPERATE_RET ai_audio_player_play_alert(AI_AUDIO_ALERT_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;
    char alert_id[64] = {0};

    snprintf(alert_id, sizeof(alert_id), "alert_%d", type);

    ai_audio_player_start(alert_id);

    switch (type) {
    case AI_AUDIO_ALERT_POWER_ON: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_power_on,\
                                        sizeof(media_src_power_on), 1);
    } break;
    case AI_AUDIO_ALERT_NOT_ACTIVE: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_not_active,\
                                        sizeof(media_src_not_active), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_CFG: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_netcfg_mode,\
                                        sizeof(media_src_netcfg_mode), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_CONNECTED: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_conencted,\
                                        sizeof(media_src_network_conencted), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_FAIL: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_fail,\
                                        sizeof(media_src_network_fail), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_DISCONNECT: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_disconnect,\
                                        sizeof(media_src_network_disconnect), 1);
    } break;
    case AI_AUDIO_ALERT_BATTERY_LOW: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_battery_low,\
                                        sizeof(media_src_battery_low), 1);
    } break;
    case AI_AUDIO_ALERT_PLEASE_AGAIN: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_please_again,\
                                        sizeof(media_src_please_again), 1);
    } break;
    case AI_AUDIO_ALERT_WAKEUP: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_wakeup, \
                                        sizeof(media_src_wakeup), 1);
    } break;
    case AI_AUDIO_ALERT_LONG_KEY_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_long_press_dialogue,\
                                        sizeof(media_src_long_press_dialogue), 1);
    } break;
    case AI_AUDIO_ALERT_KEY_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_key_dialogue,\
                                        sizeof(media_src_key_dialogue), 1);
    } break;
    case AI_AUDIO_ALERT_WAKEUP_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_wake_dialogue, \
                                        sizeof(media_src_wake_dialogue), 1);
    } break;
    case AI_AUDIO_ALERT_FREE_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_free_dialogue, \
                                        sizeof(media_src_free_dialogue), 1);
    } break;
    default:
        break;
    }

    return rt;
}

/**
 * @brief Plays an alert sound synchronously based on the specified alert type.
 * @param type The type of alert to play, defined by the AI_AUDIO_ALERT_TYPE_E enum.
 * @return OPERATE_RET - OPRT_OK if the alert sound is successfully played, otherwise an error code.
 */
OPERATE_RET ai_audio_player_play_alert_syn(AI_AUDIO_ALERT_TYPE_E type)
{
    ai_audio_player_play_alert(type);

    while (ai_audio_player_is_playing()) {
        tal_system_sleep(10);
    }

    return OPRT_OK;
}

/**
 * @brief Checks if the audio player is currently playing audio.
 *
 * @param None
 * @return uint8_t - Returns 1 if the player is playing, 0 otherwise.
 */
uint8_t ai_audio_player_is_playing(void)
{
    return sg_player.is_playing;
}
