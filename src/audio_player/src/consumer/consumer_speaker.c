/**
 * @file consumer_speaker.c
 * @brief 
 * @version 0.1
 * @date 2025-09-24
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

#include "tal_log.h"
#include "svc_ai_player.h"

#if defined(ENABLE_AUDIO_CODECS) && (ENABLE_AUDIO_CODECS == 1)
#include "tdl_audio_manage.h"
#endif

#if defined(ENABLE_AUDIO_CODECS) && (ENABLE_AUDIO_CODECS == 1)
static TDL_AUDIO_HANDLE_T s_speaker_ctx = NULL;
static TDL_AUDIO_INFO_T s_speaker_info;
#endif

OPERATE_RET consumer_speaker_open(PLAYER_CONSUMER_HANDLE *handle)
{
#if defined(ENABLE_AUDIO_CODECS) && (ENABLE_AUDIO_CODECS == 1)
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tdl_audio_find(AUDIO_CODEC_NAME, &s_speaker_ctx));
    TUYA_CALL_ERR_RETURN(tdl_audio_get_info(s_speaker_ctx, &s_speaker_info));

    *handle = (PLAYER_CONSUMER_HANDLE)s_speaker_ctx;

    return rt;
#else
    return OPRT_OK;
#endif
}

OPERATE_RET consumer_speaker_get_caps(PLAYER_CONSUMER_HANDLE handle,
                                      AI_PLAYER_CONSUMER_CAPS_T *caps)
{
    if (caps == NULL) {
        return OPRT_INVALID_PARM;
    }

    memset(caps, 0, sizeof(*caps));
#if defined(ENABLE_AUDIO_CODECS) && (ENABLE_AUDIO_CODECS == 1)
    (void)handle;
    switch (s_speaker_info.playback.format) {
    case TDL_AUDIO_FRAME_FORMAT_MP3:
        caps->passthrough_codec_mask |= AI_PLAYER_CODEC_MASK(AI_AUDIO_CODEC_MP3);
        break;
    case TDL_AUDIO_FRAME_FORMAT_SPEEX:
        caps->passthrough_codec_mask |= AI_PLAYER_CODEC_MASK(AI_AUDIO_CODEC_SPEEX);
        break;
    case TDL_AUDIO_FRAME_FORMAT_OPUS:
        caps->passthrough_codec_mask |= AI_PLAYER_CODEC_MASK(AI_AUDIO_CODEC_OPUS);
        break;
    default:
        break;
    }
#else
    (void)handle;
#endif
    return OPRT_OK;
}

OPERATE_RET consumer_speaker_close(PLAYER_CONSUMER_HANDLE handle)
{
#if defined(ENABLE_AUDIO_CODECS) && (ENABLE_AUDIO_CODECS == 1)
    return OPRT_OK;
#else
    return OPRT_OK;
#endif
}

OPERATE_RET consumer_speaker_start(PLAYER_CONSUMER_HANDLE handle)
{
#if defined(ENABLE_AUDIO_CODECS) && (ENABLE_AUDIO_CODECS == 1)
    return OPRT_OK;
#else
    return OPRT_OK;
#endif
}

OPERATE_RET consumer_speaker_write(PLAYER_CONSUMER_HANDLE handle, const void *buf, uint32_t len)
{
#if defined(ENABLE_AUDIO_CODECS) && (ENABLE_AUDIO_CODECS == 1)
    return tdl_audio_play(s_speaker_ctx, (uint8_t *)buf, len);
#else
    return OPRT_OK;
#endif
}

OPERATE_RET consumer_speaker_stop(PLAYER_CONSUMER_HANDLE handle)
{
#if defined(ENABLE_AUDIO_CODECS) && (ENABLE_AUDIO_CODECS == 1)
    return tdl_audio_play_stop(s_speaker_ctx);
#else
    return OPRT_OK;
#endif
}

OPERATE_RET consumer_speaker_set_volume(PLAYER_CONSUMER_HANDLE handle, uint32_t volume)
{
#if defined(ENABLE_AUDIO_CODECS) && (ENABLE_AUDIO_CODECS == 1)
    return tdl_audio_volume_set(s_speaker_ctx, volume);
#else
    return OPRT_OK;
#endif
}

AI_PLAYER_CONSUMER_T g_consumer_speaker = {
    .open = consumer_speaker_open,
    .close = consumer_speaker_close,
    .get_caps = consumer_speaker_get_caps,
    .start = consumer_speaker_start,
    .write = consumer_speaker_write,
    .stop = consumer_speaker_stop,
    .set_volume = consumer_speaker_set_volume
};
