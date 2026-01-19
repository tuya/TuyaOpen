/**
 * @file svc_ai_playlist.c
 * @brief 
 * @version 0.1
 * @date 2025-09-30
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

#include "tal_api.h"
#include "mix_method.h"
#include "ai_player.h"
#include "svc_ai_player.h"

#define INVALID_INDEX  0xFFFFffff

typedef struct {
    AI_PLAYER_SRC_E src;
    char *value;
    AI_AUDIO_CODEC_E codec;
} AI_PLAYITEM_T;

typedef struct {
    AI_PLAYER_HANDLE player;
    AI_PLAYLIST_CFG_T cfg;
    AI_PLAYITEM_T **playlist;
    MUTEX_HANDLE mutex;
    uint32_t count;
    uint32_t index;
    uint32_t target_index;
    bool playing;
    bool single;
} AI_PLAYLIST_CTX_T;

static void __player_status_cb(AI_PLAYLIST_HANDLE handle, AI_PLAYER_STATE_T state)
{
    AI_PLAYLIST_CTX_T *ctx = (AI_PLAYLIST_CTX_T *)handle;
    if(!ctx) {
        return ;
    }

    PR_DEBUG("playlist player state %d index %d count %d loop %d single %d", state, ctx->index, ctx->count, ctx->cfg.loop, ctx->single);

    tal_mutex_lock(ctx->mutex);
    if(AI_PLAYER_STOPPED == state) {
        if((ctx->target_index != INVALID_INDEX) && (ctx->target_index < ctx->count)) {
            ctx->index = ctx->target_index;
            ctx->target_index = INVALID_INDEX;
            tuya_ai_player_start(ctx->player, ctx->playlist[ctx->index]->src, ctx->playlist[ctx->index]->value, ctx->playlist[ctx->index]->codec);
        } else if(ctx->single) {
            tuya_ai_player_start(ctx->player, ctx->playlist[ctx->index]->src, ctx->playlist[ctx->index]->value, ctx->playlist[ctx->index]->codec);
        } else if(++ctx->index >= ctx->count) {
            ctx->index = 0;
            if(ctx->cfg.loop) {
                tuya_ai_player_start(ctx->player, ctx->playlist[ctx->index]->src, ctx->playlist[ctx->index]->value, ctx->playlist[ctx->index]->codec);
            } else {
                ctx->playing = FALSE;
            }
        } else {
            tuya_ai_player_start(ctx->player, ctx->playlist[ctx->index]->src, ctx->playlist[ctx->index]->value, ctx->playlist[ctx->index]->codec);
        }
    }
    tal_mutex_unlock(ctx->mutex);
}

/**
 * @brief Create a new playlist instance
 *
 * @param[in]  player   Player handle to bind with the playlist
 * @param[in]  cfg      Playlist configuration (loop, auto_play, etc.)
 * @param[out] handle   Returned playlist handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_create(AI_PLAYER_HANDLE player, AI_PLAYLIST_CFG_T *cfg, AI_PLAYLIST_HANDLE *handle)
{
    if(!player || !cfg || !handle) {
        return OPRT_INVALID_PARM;
    }

    AI_PLAYER_T *player_instance = (AI_PLAYER_T *)player;
    AI_PLAYLIST_CTX_T *ctx = (AI_PLAYLIST_CTX_T *)Malloc(SIZEOF(AI_PLAYLIST_CTX_T));
    if(!ctx) {
        return OPRT_MALLOC_FAILED;
    }

    memset(ctx, 0, SIZEOF(AI_PLAYLIST_CTX_T));
    tal_mutex_create_init(&ctx->mutex);
    memcpy(&ctx->cfg, cfg, SIZEOF(AI_PLAYLIST_CFG_T));
    ctx->playlist = Malloc(SIZEOF(AI_PLAYITEM_T*) * cfg->capacity);
    if(!ctx->playlist) {
        Free(ctx);
        return OPRT_MALLOC_FAILED;
    }
    ctx->player = player;
    ctx->single = FALSE;
    ctx->target_index = INVALID_INDEX;
    player_instance->playlist = ctx;
    player_instance->playlist_cb = __player_status_cb;

    *handle = ctx;
    return OPRT_OK;
}

/**
 * @brief Destroy a playlist instance and release resources
 *
 * @param[in] handle Playlist handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_destroy(AI_PLAYLIST_HANDLE handle)
{
    AI_PLAYLIST_CTX_T *ctx = (AI_PLAYLIST_CTX_T *)handle;
    if(!ctx) {
        return OPRT_INVALID_PARM;
    }

    uint32_t i = 0;

    tal_mutex_lock(ctx->mutex);
    for(i = 0; i < ctx->count; i++) {
        if(ctx->playlist[i]->value) {
            Free(ctx->playlist[i]->value);
        }
        Free(ctx->playlist[i]);
    }
    Free(ctx->playlist);
    tal_mutex_unlock(ctx->mutex);

    tal_mutex_release(ctx->mutex);
    Free(ctx);

    return OPRT_OK;
}

/**
 * @brief Add a new item to the playlist
 *
 * @param[in] handle Playlist handle
 * @param[in] src    Source type of the item (e.g. file, URL. NO memory)
 * @param[in] value  Source value (e.g. file path or URL string)
 * @param[in] codec  Audio codec type (e.g., MP3, WAV)
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_add(AI_PLAYLIST_HANDLE handle, AI_PLAYER_SRC_E src, char *value, AI_AUDIO_CODEC_E codec)
{
    AI_PLAYLIST_CTX_T *ctx = (AI_PLAYLIST_CTX_T *)handle;
    if(!ctx || !value || (src == AI_PLAYER_SRC_MEM)) {
        return OPRT_INVALID_PARM;
    }

    if(ctx->count >= ctx->cfg.capacity) {
        return OPRT_EXCEED_UPPER_LIMIT;
    }

    AI_PLAYITEM_T *item = (AI_PLAYITEM_T *)Malloc(SIZEOF(AI_PLAYITEM_T));
    if(!item) {
        return OPRT_MALLOC_FAILED;
    }

    item->src = src;
    item->codec = codec;
    item->value = mm_strdup(value);
    if(!item->value) {
        Free(item);
        return OPRT_MALLOC_FAILED;
    }

    tal_mutex_lock(ctx->mutex);
    ctx->playlist[ctx->count++] = item;
    tal_mutex_unlock(ctx->mutex);

    if((1 == ctx->count) && ctx->cfg.auto_play) {
        tuya_ai_player_start(ctx->player, src, value, codec);
        ctx->playing = TRUE;
    }

    return OPRT_OK;
}

/**
 * @brief Switch to the previous item in the playlist
 *
 * @param[in] handle Playlist handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_prev(AI_PLAYLIST_HANDLE handle)
{
    AI_PLAYLIST_CTX_T *ctx = (AI_PLAYLIST_CTX_T *)handle;
    if(!ctx) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ctx->mutex);
    if(ctx->index > 0) {
        ctx->index -= 1;
    } else if(ctx->cfg.loop) {
        ctx->index = ctx->count - 1;
    }
    tuya_ai_player_stop(ctx->player);
    tal_mutex_unlock(ctx->mutex);

    return OPRT_OK;
}

/**
 * @brief Switch to the next item in the playlist
 *
 * @param[in] handle Playlist handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_next(AI_PLAYLIST_HANDLE handle)
{
    AI_PLAYLIST_CTX_T *ctx = (AI_PLAYLIST_CTX_T *)handle;
    if(!ctx) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ctx->mutex);
    if(++ctx->index >= ctx->count) {
        if(ctx->cfg.loop) {
            ctx->index = 0;
        } else {
            ctx->index = (ctx->count >= 1) ? (ctx->count - 1) : 0;
        }
    }
    tuya_ai_player_stop(ctx->player);
    tal_mutex_unlock(ctx->mutex);

    return OPRT_OK;
}

/**
 * @brief Stop playback of the current playlist
 *
 * @param[in] handle Playlist handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_stop(AI_PLAYLIST_HANDLE handle)
{
    AI_PLAYLIST_CTX_T *ctx = (AI_PLAYLIST_CTX_T *)handle;
    if(!ctx) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ctx->mutex);
    tuya_ai_player_stop(ctx->player);
    ctx->playing = FALSE;
    tal_mutex_unlock(ctx->mutex);

    return OPRT_OK;
}

/**
 * @brief Restart playback of the playlist from the beginning
 *
 * This function resets the current index to the first item in the playlist
 * and starts playback from that item.
 *
 * @param[in] handle Playlist handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_restart(AI_PLAYLIST_HANDLE handle)
{
    AI_PLAYLIST_CTX_T *ctx = (AI_PLAYLIST_CTX_T *)handle;
    if(!ctx) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ctx->mutex);
    if(ctx->playing) {
        tuya_ai_player_stop(ctx->player);
    } else if(ctx->count > 0) {
        tuya_ai_player_start(ctx->player, ctx->playlist[0]->src, ctx->playlist[0]->value, ctx->playlist[0]->codec);
    }
    ctx->index = 0;
    ctx->playing = TRUE;
    tal_mutex_unlock(ctx->mutex);

    return OPRT_OK;
}

/**
 * @brief Clear all items in the playlist
 *
 * @param[in] handle Playlist handle
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_clear(AI_PLAYLIST_HANDLE handle)
{
    AI_PLAYLIST_CTX_T *ctx = (AI_PLAYLIST_CTX_T *)handle;
    if(!ctx) {
        return OPRT_INVALID_PARM;
    }

    uint32_t i = 0;

    tal_mutex_lock(ctx->mutex);
    if(ctx->playing) {
        tuya_ai_player_stop(ctx->player);
    }

    for(i = 0; i < ctx->count; i++) {
        if(ctx->playlist[i]->value) {
            Free(ctx->playlist[i]->value);
        }
        Free(ctx->playlist[i]);
    }
    ctx->count = 0;
    ctx->index = 0;
    ctx->playing = FALSE;
    tal_mutex_unlock(ctx->mutex);

    return OPRT_OK;
}

/**
 * @brief Play the specified playlist item by index.
 *
 * Stops the currently playing item (if any), updates the current index,
 * and starts playback of the target item.
 *
 * Behavior:
 * - If index is out of range (>= current item count) an error is returned.
 *
 * @param[in] handle Playlist handle.
 * @param[in] index  Zero-based item index to play.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_play(AI_PLAYLIST_HANDLE handle, uint32_t index)
{
    AI_PLAYLIST_CTX_T *ctx = (AI_PLAYLIST_CTX_T *)handle;
    if(!ctx) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ctx->mutex);
    if(index >= ctx->count) {
        tal_mutex_unlock(ctx->mutex);
        return OPRT_INDEX_OUT_OF_BOUND;
    }

    if(ctx->playing) {
        ctx->target_index = index;
        tuya_ai_player_stop(ctx->player);
    } else {
        ctx->index = index;
        tuya_ai_player_start(ctx->player, ctx->playlist[ctx->index]->src, ctx->playlist[ctx->index]->value, ctx->playlist[ctx->index]->codec);
    }
    ctx->playing = TRUE;
    tal_mutex_unlock(ctx->mutex);

    return OPRT_OK;
}

/**
 * @brief Configure playlist looping behavior.
 *
 * Sets whether the playlist loops after the last item, and optionally
 * whether it should loop a single track instead of the whole list.
 *
 * Logic:
 * - loop == FALSE: no looping; playback stops at the last item.
 * - loop == TRUE  && single == FALSE: loop all items (restart from first after last).
 * - loop == TRUE  && single == TRUE : repeat the current item indefinitely (single-track loop).
 *
 * If loop is FALSE the single parameter is ignored.
 *
 * @param[in] handle  Playlist handle.
 * @param[in] loop    TRUE enables looping behavior, FALSE disables it.
 * @param[in] single  TRUE enables single-track repeat when loop is TRUE.
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_set_loop(AI_PLAYLIST_HANDLE handle, bool loop, bool single)
{
    AI_PLAYLIST_CTX_T *ctx = (AI_PLAYLIST_CTX_T *)handle;
    if(!ctx) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ctx->mutex);
    ctx->cfg.loop = loop;
    ctx->single = loop ? single : FALSE;
    tal_mutex_unlock(ctx->mutex);

    return OPRT_OK;
}

/**
 * @brief Get playlist information
 *
 * @param[in]  handle Playlist handle
 * @param[out] info   Returned playlist information (count, current index)
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_ai_playlist_get_info(AI_PLAYLIST_HANDLE handle, AI_PLAYLIST_INFO_T *info)
{
    AI_PLAYLIST_CTX_T *ctx = (AI_PLAYLIST_CTX_T *)handle;
    if(!ctx) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(ctx->mutex);
    info->count = ctx->count;
    info->index = ctx->index;
    tal_mutex_unlock(ctx->mutex);

    return OPRT_OK;
}
