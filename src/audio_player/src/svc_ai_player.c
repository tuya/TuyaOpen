/**
 * @file svc_ai_player.c
 * @brief 
 * @version 0.1
 * @date 2025-09-19
 * 
 * @copyright Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 * 
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying 
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 */

#include "uni_log.h"
#include "base_event.h"
#include "mix_method.h"
#include "tal_queue.h"
#include "tal_system.h"
#include "tal_memory.h"
#include "tal_thread.h"
#include "svc_ai_player.h"
#include "ai_player.h"
#include "./mixer/mixer_fixed.h"

static char *s_player_cmd_str[PLAYER_CMD_MAX] = {
    "START",
    "STOP",
    "PAUSE",
    "RESUME",
    "EXIT",
    "DEINIT"
};

static char *s_player_mode_str[AI_PLAYER_MODE_MAX] = {
    "FG",
    "BG"
};

typedef struct {
    QUEUE_HANDLE queue;
    THREAD_HANDLE thread;
    bool is_playing;
    uint32_t msg_timeout;
    AI_PLAYER_T *player[AI_PLAYER_MODE_MAX];
    AI_PLAYER_T *active_player;
    AI_PLAYER_CONSUMER_T consumer;
    PLAYER_CONSUMER_HANDLE consumer_handle;
    AI_PLAYER_CFG_T cfg;
    int volume;
    bool mute;
    bool mixer_mode;
    bool decoder_mode;
    uint8_t *mixer_buf; // AI_PLAYER_DECODEBUF_SIZE
    uint32_t mixer_offset;
    uint32_t mixer_flag; // 0-null, 1-fg, 2-bg
} AI_PLAYER_CTX_T;

static AI_PLAYER_CTX_T s_ai_player_ctx = {0};

static void __switch_player_mode(void)
{
    AI_PLAYER_T *fg_player = s_ai_player_ctx.player[AI_PLAYER_MODE_FOREGROUND];
    AI_PLAYER_T *bg_player = s_ai_player_ctx.player[AI_PLAYER_MODE_BACKGROUND];
    AI_PLAYER_T *active_player = NULL;

    if(fg_player != NULL && fg_player->state == AI_PLAYER_PLAYING) {
        active_player = fg_player;
    } else if(bg_player != NULL && bg_player->state == AI_PLAYER_PLAYING) {
        active_player = bg_player;
    }

    if(active_player) {
        if(s_ai_player_ctx.active_player != active_player) {
            if(!s_ai_player_ctx.active_player && s_ai_player_ctx.decoder_mode && s_ai_player_ctx.consumer.start) {
                s_ai_player_ctx.consumer.start(s_ai_player_ctx.consumer_handle);
            }
            s_ai_player_ctx.active_player = active_player;
            s_ai_player_ctx.is_playing = true;
        }
    } else {
        if(s_ai_player_ctx.active_player && s_ai_player_ctx.decoder_mode && s_ai_player_ctx.consumer.stop) {
            s_ai_player_ctx.consumer.stop(s_ai_player_ctx.consumer_handle);
        }
        s_ai_player_ctx.active_player = NULL;
        s_ai_player_ctx.is_playing = false;
    }

    if(s_ai_player_ctx.is_playing) {
        s_ai_player_ctx.msg_timeout = PLAYER_BUSY_TIMEOUT_MS;
    } else {
        s_ai_player_ctx.msg_timeout = PLAYER_IDLE_TIMEOUT_MS;
    }
}

static OPERATE_RET __cmd_player_start(AI_PLAYER_T *player, AI_PLAYER_MSG_T *msg)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("start player %s value=%s", s_player_mode_str[player->mode], msg->param.cmd_start.value ? msg->param.cmd_start.value : "null");

    TUYA_CALL_ERR_RETURN(ai_player_datasink_start(player->sink, msg->param.cmd_start.src, msg->param.cmd_start.value));
    TUYA_CALL_ERR_RETURN(ai_player_decoder_start(player->decoder, msg->param.cmd_start.codec));

    player->state = AI_PLAYER_PLAYING;
    player->offset = 0;
    player->has_pending_output = FALSE;
    if(msg->param.cmd_start.value) {
        Free(msg->param.cmd_start.value);
    }

    if(!s_ai_player_ctx.decoder_mode) {
        if(s_ai_player_ctx.consumer.start) {
            s_ai_player_ctx.consumer.start(s_ai_player_ctx.consumer_handle);
        }
    }

    AI_PLAYER_EVT_T evt = {
        .handle = player,
        .state = player->state
    };
    ty_publish_event(EVENT_AI_PLAYER_STATE, &evt);
    return OPRT_OK;
}

static OPERATE_RET __cmd_player_stop(AI_PLAYER_T *player)
{
    PR_DEBUG("stop player %s", s_player_mode_str[player->mode]);

    ai_player_datasink_stop(player->sink);
    ai_player_decoder_stop(player->decoder);
    player->state = AI_PLAYER_STOPPED;
    player->offset = 0;
    player->has_pending_output = FALSE;
    if(player->playlist && player->playlist_cb) {
        player->playlist_cb(player->playlist, player->state);
    }

    if(!s_ai_player_ctx.decoder_mode) {
        if(s_ai_player_ctx.consumer.stop) {
            s_ai_player_ctx.consumer.stop(s_ai_player_ctx.consumer_handle);
        }
    }

    AI_PLAYER_EVT_T evt = {
        .handle = player,
        .state = player->state
    };
    ty_publish_event(EVENT_AI_PLAYER_STATE, &evt);
    return OPRT_OK;
}

static OPERATE_RET __cmd_player_pause(AI_PLAYER_T *player)
{
    player->state = AI_PLAYER_PAUSED;

    AI_PLAYER_EVT_T evt = {
        .handle = player,
        .state = player->state
    };
    ty_publish_event(EVENT_AI_PLAYER_STATE, &evt);
    return OPRT_OK;
}

static OPERATE_RET __cmd_player_resume(AI_PLAYER_T *player)
{
    player->state = AI_PLAYER_PLAYING;

    AI_PLAYER_EVT_T evt = {
        .handle = player,
        .state = player->state
    };
    ty_publish_event(EVENT_AI_PLAYER_STATE, &evt);
    return OPRT_OK;
}

static OPERATE_RET __cmd_player_exit(AI_PLAYER_T *player)
{
    ai_player_datasink_deinit(player->sink);
    ai_player_decoder_deinit(player->decoder);
    s_ai_player_ctx.player[player->mode] = NULL;

    Free(player->framebuf);
    Free(player->decode_buf);
    Free(player);
    return OPRT_OK;
}

static OPERATE_RET __cmd_player_service_deinit(void)
{
    uint32_t i = 0;

    for(i = 0; i < AI_PLAYER_MODE_MAX; i++) {
        if(s_ai_player_ctx.player[i]) {
            __cmd_player_exit(s_ai_player_ctx.player[i]);
        }
    }

    tal_queue_free(s_ai_player_ctx.queue);
    ai_player_resample_deinit();

    if(s_ai_player_ctx.mixer_buf) {
        Free(s_ai_player_ctx.mixer_buf);
        s_ai_player_ctx.mixer_buf = NULL;
    }

    if(s_ai_player_ctx.consumer.close) {
        s_ai_player_ctx.consumer.close(s_ai_player_ctx.consumer_handle);
    }

    memset(&s_ai_player_ctx, 0, SIZEOF(AI_PLAYER_CTX_T));
    return OPRT_OK;
}


static OPERATE_RET __handle_player_streaming_source(AI_PLAYER_T *player)
{
    OPERATE_RET rt = OPRT_OK;
    bool is_eof = false;

    player->decode_size = 0;

    // 1. Read data from datasink (skip if decoder has pending output)
    if (!player->has_pending_output) {
        uint32_t out_len = 0;
        rt = ai_player_datasink_read(player->sink, player->framebuf + player->offset, AI_PLAYER_FRAMEBUF_SIZE - player->offset, &out_len);
        if(OPRT_OK == rt) {
            player->offset += out_len;
        } else if(OPRT_NOT_FOUND == rt) {
            is_eof = true;
        } else {
            PR_ERR("ai player %s datasink read error: %d", s_player_mode_str[player->mode], rt);
            return rt;
        }
    }

    if(0 == player->offset && !player->has_pending_output) {
        if(is_eof) {
            PR_NOTICE("ai player %s eof", s_player_mode_str[player->mode]);

            __cmd_player_stop(player);
            __switch_player_mode();
        }
        tal_system_sleep(10);
        return OPRT_OK;
    }

    if(!s_ai_player_ctx.decoder_mode) {
        player->decode_size = (player->offset > AI_PLAYER_DECODEBUF_SIZE) ? AI_PLAYER_DECODEBUF_SIZE : player->offset;
        memcpy(player->decode_buf, player->framebuf, player->decode_size);
        player->offset -= player->decode_size;
        return OPRT_OK;
    }

    // 2. Decode data
    DECODER_OUTPUT_T output = {0};
    memset(&output, 0, SIZEOF(DECODER_OUTPUT_T));
    rt = ai_player_decoder_process(player->decoder, player->framebuf, player->offset, player->decode_buf, AI_PLAYER_DECODEBUF_SIZE, &output);

    // Update player's pending output flag based on decoder return value
    player->has_pending_output = (rt == OPRT_BUFFER_NOT_ENOUGH);

    if(rt > 0) { // buf is not completely consumed
        if((rt == player->offset) && is_eof) {
            player->offset = 0; // all data consumed and eof
        } else {
            memmove(player->framebuf, player->framebuf + (player->offset - rt), rt);
            player->offset = rt;
        }
    } else if (rt == 0) {
        player->offset = 0;
    } else {
        player->offset = 0;
        return OPRT_OK;
    }

    if(output.sample == 0) {
        return OPRT_OK; //id3 tag?
    }

#if defined(AI_PLAYER_SUPPORT_DIGITAL_VOLUME) && (AI_PLAYER_SUPPORT_DIGITAL_VOLUME == 1)
    // 3. Adjust volume
    if(player->volume != PLAYER_MAX_VOLUME) {
        ai_player_volume_process(player->decode_buf, output.used_size, player->volume, PLAYER_MAX_VOLUME, output.datebits);
    }
#endif

    // 4. Resample data if needed
    player->decode_size = output.used_size;
    if(output.channel == s_ai_player_ctx.cfg.channel && output.sample  == s_ai_player_ctx.cfg.sample && output.datebits == s_ai_player_ctx.cfg.datebits) {
        // No need to resample
    } else {
#if defined(AI_PLAYER_SUPPORT_RESAMPLE) && (AI_PLAYER_SUPPORT_RESAMPLE == 1)
        rt = ai_player_resample_process(player->decode_buf, &output, player->decode_buf, (int *)&player->decode_size);
        if(OPRT_OK != rt) {
            PR_ERR("ai player %s resample error: %d", s_player_mode_str[player->mode], rt);
            return rt;
        }
#else
        PR_ERR("ai player %s resample not support!", s_player_mode_str[player->mode]);
        return OPRT_NOT_SUPPORTED;
#endif
    }

    return OPRT_OK;
}

#if defined(AI_PLAYER_SUPPORT_MIX_MODE) && (AI_PLAYER_SUPPORT_MIX_MODE == 1)
static OPERATE_RET __handle_player_streaming_mix(AI_PLAYER_T *fg_player, AI_PLAYER_T *bg_player)
{
    uint32_t samples = 0;

    if(fg_player && bg_player) {
        if(fg_player->decode_size >= bg_player->decode_size) {
            samples = bg_player->decode_size/2;
            ai_player_mixer_process(bg_player->decode_buf, fg_player->decode_buf, samples);
            if(s_ai_player_ctx.consumer.write) {
                s_ai_player_ctx.consumer.write(s_ai_player_ctx.consumer_handle, fg_player->decode_buf, samples*2);
            }
            s_ai_player_ctx.mixer_offset = fg_player->decode_size - bg_player->decode_size;
            if(s_ai_player_ctx.mixer_offset) {
                memmove(s_ai_player_ctx.mixer_buf, fg_player->decode_buf + bg_player->decode_size, s_ai_player_ctx.mixer_offset);
                s_ai_player_ctx.mixer_flag = 1;
            } else {
                s_ai_player_ctx.mixer_flag = 0;
            }
        } else {
            samples = fg_player->decode_size/2;
            ai_player_mixer_process(bg_player->decode_buf, fg_player->decode_buf, samples);
            if(s_ai_player_ctx.consumer.write) {
                s_ai_player_ctx.consumer.write(s_ai_player_ctx.consumer_handle, fg_player->decode_buf, samples*2);
            }
            s_ai_player_ctx.mixer_offset = bg_player->decode_size - fg_player->decode_size;
            if(s_ai_player_ctx.mixer_offset) {
                memmove(s_ai_player_ctx.mixer_buf, bg_player->decode_buf + fg_player->decode_size, s_ai_player_ctx.mixer_offset);
                s_ai_player_ctx.mixer_flag = 2;
            } else {
                s_ai_player_ctx.mixer_flag = 0;
            }
        }
    } else if(fg_player) {
        if(fg_player->decode_size >= s_ai_player_ctx.mixer_offset) {
            samples = s_ai_player_ctx.mixer_offset/2;
            ai_player_mixer_process(s_ai_player_ctx.mixer_buf, fg_player->decode_buf, samples);
            if(s_ai_player_ctx.consumer.write) {
                s_ai_player_ctx.consumer.write(s_ai_player_ctx.consumer_handle, fg_player->decode_buf, samples*2);
            }
            s_ai_player_ctx.mixer_offset = fg_player->decode_size - s_ai_player_ctx.mixer_offset;
            if(s_ai_player_ctx.mixer_offset) {
                memmove(s_ai_player_ctx.mixer_buf, fg_player->decode_buf + samples*2, s_ai_player_ctx.mixer_offset);
                s_ai_player_ctx.mixer_flag = 1;
            } else {
                s_ai_player_ctx.mixer_flag = 0;
            }
        } else {
            samples = fg_player->decode_size/2;
            ai_player_mixer_process(s_ai_player_ctx.mixer_buf, fg_player->decode_buf, samples);
            if(s_ai_player_ctx.consumer.write) {
                s_ai_player_ctx.consumer.write(s_ai_player_ctx.consumer_handle, fg_player->decode_buf, samples*2);
            }
            s_ai_player_ctx.mixer_offset = s_ai_player_ctx.mixer_offset - fg_player->decode_size;
            if(s_ai_player_ctx.mixer_offset) {
                memmove(s_ai_player_ctx.mixer_buf, s_ai_player_ctx.mixer_buf + samples*2, s_ai_player_ctx.mixer_offset);
                s_ai_player_ctx.mixer_flag = 2;
            } else {
                s_ai_player_ctx.mixer_flag = 0;
            }
        }
    } else if(bg_player) {
        if(bg_player->decode_size >= s_ai_player_ctx.mixer_offset) {
            samples = s_ai_player_ctx.mixer_offset/2;
            ai_player_mixer_process(s_ai_player_ctx.mixer_buf, bg_player->decode_buf, samples);
            if(s_ai_player_ctx.consumer.write) {
                s_ai_player_ctx.consumer.write(s_ai_player_ctx.consumer_handle, bg_player->decode_buf, samples*2);
            }
            s_ai_player_ctx.mixer_offset = bg_player->decode_size - s_ai_player_ctx.mixer_offset;
            if(s_ai_player_ctx.mixer_offset) {
                memmove(s_ai_player_ctx.mixer_buf, bg_player->decode_buf + samples*2, s_ai_player_ctx.mixer_offset);
                s_ai_player_ctx.mixer_flag = 2;
            } else {
                s_ai_player_ctx.mixer_flag = 0;
            }
        } else {
            samples = bg_player->decode_size/2;
            ai_player_mixer_process(s_ai_player_ctx.mixer_buf, bg_player->decode_buf, samples);
            if(s_ai_player_ctx.consumer.write) {
                s_ai_player_ctx.consumer.write(s_ai_player_ctx.consumer_handle, bg_player->decode_buf, samples*2);
            }
            s_ai_player_ctx.mixer_offset = s_ai_player_ctx.mixer_offset - bg_player->decode_size;
            if(s_ai_player_ctx.mixer_offset) {
                memmove(s_ai_player_ctx.mixer_buf, s_ai_player_ctx.mixer_buf + samples*2, s_ai_player_ctx.mixer_offset);
                s_ai_player_ctx.mixer_flag = 1;
            } else {
                s_ai_player_ctx.mixer_flag = 0;
            }
        }
    }

    return OPRT_OK;
}
#endif
static OPERATE_RET __handle_player_streaming(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_PLAYER_T *active_player = s_ai_player_ctx.active_player;

#if defined(AI_PLAYER_SUPPORT_MIX_MODE) && (AI_PLAYER_SUPPORT_MIX_MODE == 1)
    // Handle mixing if needed
    AI_PLAYER_T *bg_player = s_ai_player_ctx.player[AI_PLAYER_MODE_BACKGROUND];
    if(s_ai_player_ctx.mixer_mode && (active_player->mode == AI_PLAYER_MODE_FOREGROUND)) {
        if(bg_player && (bg_player->state == AI_PLAYER_PLAYING)) {
            if(s_ai_player_ctx.mixer_flag == 0) {
                rt = __handle_player_streaming_source(bg_player);
                rt = __handle_player_streaming_source(active_player);
                if(!s_ai_player_ctx.decoder_mode) {
                    if(s_ai_player_ctx.consumer.write) {
                        if(bg_player->decode_size) {
                            s_ai_player_ctx.consumer.write(s_ai_player_ctx.consumer_handle, bg_player->decode_buf, bg_player->decode_size);
                            bg_player->decode_size = 0;
                        }
                        if(active_player->decode_size) {
                            s_ai_player_ctx.consumer.write(s_ai_player_ctx.consumer_handle, active_player->decode_buf, active_player->decode_size);
                            active_player->decode_size = 0;
                        }
                    }
                } else {
                    rt = __handle_player_streaming_mix(active_player, bg_player);
                }
            } else if(s_ai_player_ctx.mixer_flag == 1) {
                rt = __handle_player_streaming_source(bg_player);
                if(!s_ai_player_ctx.decoder_mode) {
                    if((OPRT_OK == rt) && bg_player->decode_size && s_ai_player_ctx.consumer.write) {
                        s_ai_player_ctx.consumer.write(s_ai_player_ctx.consumer_handle, bg_player->decode_buf, bg_player->decode_size);
                        bg_player->decode_size = 0;
                    }
                } else {
                    rt = __handle_player_streaming_mix(NULL, bg_player);
                }
            } else {
                rt = __handle_player_streaming_source(active_player);
                if(!s_ai_player_ctx.decoder_mode) {
                    if((OPRT_OK == rt) && active_player->decode_size && s_ai_player_ctx.consumer.write) {
                        s_ai_player_ctx.consumer.write(s_ai_player_ctx.consumer_handle, active_player->decode_buf, active_player->decode_size);
                        active_player->decode_size = 0;
                    }
                } else {
                    rt = __handle_player_streaming_mix(active_player, NULL);
                }
            }

            return OPRT_OK; // mixed
        }
    } else if(s_ai_player_ctx.mixer_offset) {
        s_ai_player_ctx.mixer_flag = 0;
        s_ai_player_ctx.mixer_offset = 0;
    }
#endif

    // No mixing
    rt = __handle_player_streaming_source(active_player);
    if((OPRT_OK == rt) && active_player->decode_size && s_ai_player_ctx.consumer.write) {
        s_ai_player_ctx.consumer.write(s_ai_player_ctx.consumer_handle, active_player->decode_buf, active_player->decode_size);
    }
    active_player->decode_size = 0;

    return rt;
}

static void __ai_player_thread_cb(void* args)
{
    OPERATE_RET rt = OPRT_OK;
    AI_PLAYER_MSG_T msg = {0};
    s_ai_player_ctx.msg_timeout = PLAYER_IDLE_TIMEOUT_MS;

    while(tal_thread_get_state(s_ai_player_ctx.thread) == THREAD_STATE_RUNNING) {
        if (tal_queue_fetch(s_ai_player_ctx.queue, &msg, s_ai_player_ctx.msg_timeout) != OPRT_OK) {
            if(s_ai_player_ctx.is_playing) {
                rt = __handle_player_streaming();
                if(OPRT_OK != rt) {
                    __cmd_player_stop(s_ai_player_ctx.active_player);
                    __switch_player_mode();
                }
            }
            continue;
        }

        PR_DEBUG("ai player %s cmd %s", s_player_mode_str[msg.mode], s_player_cmd_str[msg.cmd]);

        AI_PLAYER_T *player = s_ai_player_ctx.player[msg.mode];
        if (!player && (msg.cmd != PLAYER_CMD_SERVICE_DEINIT)) {
            continue;
        }

        switch (msg.cmd) {
            case PLAYER_CMD_START:
                rt = __cmd_player_start(player, &msg);
                break;
            case PLAYER_CMD_STOP:
                rt = __cmd_player_stop(player);
                break;
            case PLAYER_CMD_PAUSE:
                rt = __cmd_player_pause(player);
                break;
            case PLAYER_CMD_RESUME:
                rt = __cmd_player_resume(player);
                break;
            case PLAYER_CMD_EXIT:
                rt = __cmd_player_exit(player);
                break;
            case PLAYER_CMD_SERVICE_DEINIT:
                tal_thread_delete(s_ai_player_ctx.thread);
                break;
            default:
                break;
        }

        if((OPRT_OK != rt) && player) {
            PR_ERR("ai player cmd error: %d", rt);
            __cmd_player_stop(player);
        }

        __switch_player_mode();
    }

    __cmd_player_service_deinit();
}

static bool __is_player_valid(AI_PLAYER_T *player)
{
    if (!player) {
        return false;
    }

    if (player->mode >= AI_PLAYER_MODE_MAX || s_ai_player_ctx.player[player->mode] != player) {
        return false;
    }

    return true;
}

/**
 * @brief Initialize the AI player service.
 *
 * This function must be called once before creating or using any AI player instance.
 * It sets up global resources such as:
 * - Audio configuration (sample rate, bit depth, channels)
 * - Internal tasks and message queues
 * - Audio driver or output backends
 *
 * @param[in] cfg  Pointer to the player configuration (must not be NULL).
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_service_init(AI_PLAYER_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tal_queue_create_init(&s_ai_player_ctx.queue, SIZEOF(AI_PLAYER_MSG_T), 2));

    THREAD_CFG_T thread_cfg = {0};
    thread_cfg.stackDepth = AI_PLAYER_STACK_SIZE;
    thread_cfg.priority   = THREAD_PRIO_1;
    thread_cfg.thrdname   = "ai_player";
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&s_ai_player_ctx.thread, NULL, NULL, __ai_player_thread_cb, NULL, &thread_cfg));

    memcpy(&s_ai_player_ctx.cfg, cfg, SIZEOF(AI_PLAYER_CFG_T));
    memcpy(&s_ai_player_ctx.consumer, &g_consumer_speaker, SIZEOF(AI_PLAYER_CONSUMER_T));
    g_consumer_speaker.open(&s_ai_player_ctx.consumer_handle);
    ai_player_resample_init(cfg->sample, cfg->datebits, cfg->channel);
    s_ai_player_ctx.decoder_mode = true;
    s_ai_player_ctx.volume = PLAYER_MAX_VOLUME;
    s_ai_player_ctx.mute = false;

    return rt;
}

/**
 * @brief Deinitialize the AI player service.
 *
 * This function releases all resources allocated by
 * tuya_ai_player_service_init(). After calling this function,
 * no player instance can be created or used until
 * tuya_ai_player_service_init() is called again.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_service_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    AI_PLAYER_MSG_T msg = {0};
    msg.mode = AI_PLAYER_MODE_FOREGROUND;
    msg.cmd  = PLAYER_CMD_SERVICE_DEINIT;
    TUYA_CALL_ERR_RETURN(tal_queue_post(s_ai_player_ctx.queue, &msg, 0));
    return OPRT_OK;
}

/**
 * @brief Create an audio player instance.
 *
 * @param[in]  mode    Player working mode.
 * @param[out] handle  Pointer to the player handle returned on success.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_create(AI_PLAYER_MODE_E mode, AI_PLAYER_HANDLE *handle)
{
    if (!handle || (mode >= AI_PLAYER_MODE_MAX) || (s_ai_player_ctx.player[mode] != NULL)) {
        return OPRT_INVALID_PARM;
    }

    AI_PLAYER_T *player = Malloc(SIZEOF(AI_PLAYER_T));
    if (!player) {
        return OPRT_MALLOC_FAILED;
    }
    memset(player, 0, SIZEOF(AI_PLAYER_T));

    player->framebuf = Malloc(AI_PLAYER_FRAMEBUF_SIZE);
    if (!player->framebuf) {
        Free(player);
        return OPRT_MALLOC_FAILED;
    }

    player->decode_buf = Malloc(AI_PLAYER_DECODEBUF_SIZE);
    if (!player->decode_buf) {
        Free(player->framebuf);
        Free(player);
        return OPRT_MALLOC_FAILED;
    }

    player->state = AI_PLAYER_STOPPED;
    player->mode = mode;
    player->mute = false;
    player->volume = PLAYER_MAX_VOLUME;
    player->has_pending_output = FALSE;
    ai_player_datasink_init(&player->sink);
    ai_player_decoder_init(&player->decoder);
    s_ai_player_ctx.player[mode] = player;

    *handle = (AI_PLAYER_HANDLE)player;

    return OPRT_OK;
}

/**
 * @brief Destroy an audio player instance and release all resources.
 *
 * @param[in] handle Player handle to be destroyed.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_destroy(AI_PLAYER_HANDLE handle)
{
    OPERATE_RET rt = OPRT_OK;
    AI_PLAYER_T *player = (AI_PLAYER_T *)handle;

    if (!__is_player_valid(player)) {
        return OPRT_INVALID_PARM;
    }

    AI_PLAYER_MSG_T msg = {0};
    msg.mode = player->mode;
    msg.cmd  = PLAYER_CMD_EXIT;
    TUYA_CALL_ERR_RETURN(tal_queue_post(s_ai_player_ctx.queue, &msg, 0));
    return OPRT_OK;
}

/**
 * @brief Start playback from a specific data source.
 *
 * @param[in] handle Player handle.
 * @param[in] src    Data source type (memory, file, or URL).
 * @param[in] value  Source value:
 *                   - For memory: reserved or NULL (use feed API instead).
 *                   - For file:   file path string.
 *                   - For URL:    URL string.
 * @param[in] codec  Audio codec type of the source data.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_start(AI_PLAYER_HANDLE handle, AI_PLAYER_SRC_E src, char *value, AI_AUDIO_CODEC_E codec)
{
    OPERATE_RET rt = OPRT_OK;
    AI_PLAYER_T *player = (AI_PLAYER_T *)handle;

    if (!__is_player_valid(player) || (player->state != AI_PLAYER_STOPPED)) {
        return OPRT_INVALID_PARM;
    }

    if((src != AI_PLAYER_SRC_MEM) && (value == NULL || strlen(value) == 0)) {
        return OPRT_INVALID_PARM;
    }

    AI_PLAYER_MSG_T msg = {0};
    msg.mode = player->mode;
    msg.cmd  = PLAYER_CMD_START;
    msg.param.cmd_start.src = src;
    msg.param.cmd_start.codec = codec;
    msg.param.cmd_start.value = value ? mm_strdup(value) : NULL;
    TUYA_CALL_ERR_RETURN(tal_queue_post(s_ai_player_ctx.queue, &msg, 0));
    if(src == AI_PLAYER_SRC_MEM) {
        uint32_t timeout = 0;
        while(player->state != AI_PLAYER_PLAYING) {
            tal_system_sleep(10);
            if(timeout++ > 1000) { // wait max 10s
                return OPRT_TIMEOUT;
            }
        }
    }
    return OPRT_OK;
}

/**
 * @brief Feed raw audio data to the player (used in memory mode).
 *
 * This function pushes audio data into the player for playback when using 
 * memory-based input. The data buffer is not copied internally, so the caller 
 * should ensure its validity until the function returns.
 *
 * Special case:
 * - If @p data == NULL and @p len == 0, it indicates the end of the stream.
 *
 * @param[in] handle Player handle.
 * @param[in] data   Pointer to audio data buffer.
 * @param[in] len    Length of the data buffer in bytes.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_feed(AI_PLAYER_HANDLE handle, uint8_t *data, uint32_t len)
{
    AI_PLAYER_T *player = (AI_PLAYER_T *)handle;

    if (!__is_player_valid(player) || (player->state == AI_PLAYER_STOPPED)) {
        return OPRT_INVALID_PARM;
    }

    return ai_player_datasink_feed(player->sink, data, len);
}

/**
 * @brief Stop playback immediately.
 *
 * @param[in] handle Player handle.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_stop(AI_PLAYER_HANDLE handle)
{
    OPERATE_RET rt = OPRT_OK;
    AI_PLAYER_T *player = (AI_PLAYER_T *)handle;

    if (!__is_player_valid(player)) {
        return OPRT_INVALID_PARM;
    }

    if(player->state == AI_PLAYER_STOPPED) {
        return OPRT_OK;
    }

    AI_PLAYER_MSG_T msg = {0};
    msg.mode = player->mode;
    msg.cmd  = PLAYER_CMD_STOP;
    TUYA_CALL_ERR_RETURN(tal_queue_post(s_ai_player_ctx.queue, &msg, 0));
    while(player->state != AI_PLAYER_STOPPED) {
        tal_system_sleep(10);
    }
    return OPRT_OK;
}

/**
 * @brief Pause playback.
 *
 * @param[in] handle Player handle.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_pause(AI_PLAYER_HANDLE handle)
{
    OPERATE_RET rt = OPRT_OK;
    AI_PLAYER_T *player = (AI_PLAYER_T *)handle;

    if (!__is_player_valid(player) || (player->state != AI_PLAYER_PLAYING)) {
        return OPRT_INVALID_PARM;
    }

    AI_PLAYER_MSG_T msg = {0};
    msg.mode = player->mode;
    msg.cmd  = PLAYER_CMD_PAUSE;
    TUYA_CALL_ERR_RETURN(tal_queue_post(s_ai_player_ctx.queue, &msg, 0));
    return OPRT_OK;
}

/**
 * @brief Resume playback after being paused.
 *
 * @param[in] handle Player handle.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_resume(AI_PLAYER_HANDLE handle)
{
    OPERATE_RET rt = OPRT_OK;
    AI_PLAYER_T *player = (AI_PLAYER_T *)handle;

    if (!__is_player_valid(player) || (player->state != AI_PLAYER_PAUSED)) {
        return OPRT_INVALID_PARM;
    }

    AI_PLAYER_MSG_T msg = {0};
    msg.mode = player->mode;
    msg.cmd  = PLAYER_CMD_RESUME;
    TUYA_CALL_ERR_RETURN(tal_queue_post(s_ai_player_ctx.queue, &msg, 0));
    return OPRT_OK;
}

/**
 * @brief Get the current playback state of the player
 *
 * @param[in] handle  Handle of the player instance
 *
 * @return Current player state (see @ref AI_PLAYER_STATE_T)
 */
AI_PLAYER_STATE_T tuya_ai_player_get_state(AI_PLAYER_HANDLE handle)
{
    AI_PLAYER_T *player = (AI_PLAYER_T *)handle;

    if (!__is_player_valid(player)) {
        return AI_PLAYER_STOPPED;
    }

    return player->state;
}

/**
 * @brief Get the current audio codec of the player
 *
 * @param[in] handle  Handle of the player instance
 *
 * @return Current audio codec (see @ref AI_AUDIO_CODEC_E)
 */
AI_AUDIO_CODEC_E tuya_ai_player_get_codec(AI_PLAYER_HANDLE handle)
{
    AI_PLAYER_T *player = (AI_PLAYER_T *)handle;

    if (!__is_player_valid(player)) {
        return AI_AUDIO_CODEC_MP3;
    }

    return ai_player_decoder_get_codec(player->decoder);
}

/**
 * @brief Set the consumer for the audio player (default is tkl_audio).
 *
 * This function binds a user-implemented consumer (AI_PLAYER_CONSUMER_T)
 * to the player service. The player service will invoke the consumer’s
 * callbacks to handle decoded audio output.
 *
 * @param[in] consumer  Pointer to user-implemented consumer interface
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_set_consumer(AI_PLAYER_CONSUMER_T *consumer)
{
    if(s_ai_player_ctx.consumer.close) {
        s_ai_player_ctx.consumer.close(s_ai_player_ctx.consumer_handle);
    }

    if(!consumer) {
        memcpy(&s_ai_player_ctx.consumer, &g_consumer_speaker, SIZEOF(AI_PLAYER_CONSUMER_T));
    } else {
        memcpy(&s_ai_player_ctx.consumer, consumer, SIZEOF(AI_PLAYER_CONSUMER_T));
    }

    if(s_ai_player_ctx.consumer.open) {
        s_ai_player_ctx.consumer.open(&s_ai_player_ctx.consumer_handle);
    }

    return OPRT_OK;
}

/**
 * @brief Set playback volume.
 *
 * This function adjusts the playback volume of the AI player.
 * - If @p handle is NULL, it adjusts the global analog (hardware) volume.
 * - If @p handle is not NULL, it adjusts the digital volume for the specified player instance.
 *
 * The valid range of @p volume is 0–100, where a higher value indicates a higher volume.
 *
 * @param[in] handle Player handle. NULL for global analog volume control.
 * @param[in] volume Volume level (0–100, higher means louder).
 *
 * @return OPRT_OK on success. Other error codes are defined in tuya_error_code.h.
 */
OPERATE_RET tuya_ai_player_set_volume(AI_PLAYER_HANDLE handle, int volume)
{
    if(!handle) {
        s_ai_player_ctx.volume = volume;

        if(s_ai_player_ctx.consumer.set_volume) {
            s_ai_player_ctx.consumer.set_volume(s_ai_player_ctx.consumer_handle, s_ai_player_ctx.volume);
        }
    } else {
        AI_PLAYER_T *player = (AI_PLAYER_T *)handle;
        player->volume = volume;
    }

    return OPRT_OK;
}

/**
 * @brief Get the current playback volume.
 *
 * @param[in] handle Player handle. Null indicates global.
 * @param[out] volume Pointer to store the current volume level.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_get_volume(AI_PLAYER_HANDLE handle, int *volume)
{
    if(!handle) {
        *volume = s_ai_player_ctx.volume;
    } else {
        AI_PLAYER_T *player = (AI_PLAYER_T *)handle;
        *volume = player->volume;
    }

    return OPRT_OK;
}

/**
 * @brief Enable or disable mute.
 *
 * @param[in] handle Player handle. Null indicates global.
 * @param[in] mute   true to mute, false to unmute.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_set_mute(AI_PLAYER_HANDLE handle, bool mute)
{
    if(!handle) {
        s_ai_player_ctx.mute = mute;

        if(s_ai_player_ctx.consumer.set_volume) {
            if(mute) {
                s_ai_player_ctx.consumer.set_volume(s_ai_player_ctx.consumer_handle, 0);
            } else {
                s_ai_player_ctx.consumer.set_volume(s_ai_player_ctx.consumer_handle, s_ai_player_ctx.volume);
            }
        }
    } else {
        AI_PLAYER_T *player = (AI_PLAYER_T *)handle;
        player->mute = mute;
    }

    return OPRT_OK;
}

/**
 * @brief Get the mute status.
 *
 * @param[in] handle Player handle. Null indicates global.
 * @param[out] mute   Pointer to store mute status (true if muted).
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_player_get_mute(AI_PLAYER_HANDLE handle, bool *mute)
{
    if(!handle) {
        *mute = s_ai_player_ctx.mute;
    } else {
        AI_PLAYER_T *player = (AI_PLAYER_T *)handle;
        *mute = player->mute;
    }

    return OPRT_OK;
}

/**
 * @brief Set the player mixing mode
 *
 * Configures whether the player operates in preemptive mode or mixing mode
 * when handling foreground and background playback.
 *
 * In **preemptive mode** (default), foreground playback takes priority:
 * - When foreground audio starts, background playback is automatically paused.
 * - When foreground playback ends, background playback resumes.
 *
 * In **mixing mode**, both foreground and background audio can be played
 * simultaneously:
 * - The background volume is automatically reduced (e.g., to 50%) while
 *   foreground audio is active.
 * - Once foreground playback ends, the background volume returns to normal.
 *
 * @param[in] enable  true to enable mixing mode, false to use preemptive mode.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h.
 *
 * @note This setting affects the behavior of both foreground and background
 *       players globally. It should be configured before playback starts.
 */
OPERATE_RET tuya_ai_player_set_mix_mode(bool enable)
{
    if(enable && !s_ai_player_ctx.mixer_buf) {
        s_ai_player_ctx.mixer_buf = Malloc(AI_PLAYER_DECODEBUF_SIZE);
        if(!s_ai_player_ctx.mixer_buf) {
            return OPRT_MALLOC_FAILED;
        }
    }
    s_ai_player_ctx.mixer_offset = 0;
    s_ai_player_ctx.mixer_flag = 0;
    s_ai_player_ctx.mixer_mode = enable;
    return OPRT_OK;
}

/**
 * @brief Enable or disable the decoder for PCM output.
 *
 * This function controls whether the AI player decodes the input audio stream.
 * When enabled (default), the player decodes the input data into PCM format before output.
 * When disabled, the player bypasses decoding and outputs the raw audio data directly.
 *
 * @param[in] enable  true to enable decoding (output PCM), false to disable decoding (output raw data).
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h.
 */
OPERATE_RET tuya_ai_player_set_decoder_mode(bool enable)
{
    s_ai_player_ctx.decoder_mode = enable;
    return OPRT_OK;
}
