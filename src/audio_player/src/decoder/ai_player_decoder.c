/**
 * @file ai_player_decoder.c
 * @brief 
 * @version 0.1
 * @date 2025-09-25
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
#include "decoder_cfg.h"
#include "ai_player_decoder.h"

#if defined(AI_PLAYER_DECODER) && (AI_PLAYER_DECODER & AI_PLAYER_DECODER_MP3)
extern DECODER_T g_decoder_mp3;
#endif
#if defined(AI_PLAYER_DECODER) && (AI_PLAYER_DECODER & AI_PLAYER_DECODER_WAV)
extern DECODER_T g_decoder_wav;
#endif
#if defined(AI_PLAYER_DECODER) && (AI_PLAYER_DECODER & AI_PLAYER_DECODER_OPUS)
extern DECODER_T g_decoder_opus;
#endif
#if defined(AI_PLAYER_DECODER) && (AI_PLAYER_DECODER & AI_PLAYER_DECODER_OGGOPUS)
extern DECODER_T g_decoder_oggopus;
#endif

typedef struct {
    DECODER_T *decoder;
    void* priv;
} DECODER_CTX_T;

OPERATE_RET ai_player_decoder_init(PLAYER_DECODER *handle)
{
    if (handle == NULL) {
        return OPRT_INVALID_PARM;
    }

    DECODER_CTX_T *ctx = (DECODER_CTX_T *)Malloc(sizeof(DECODER_CTX_T));
    if (ctx == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    memset(ctx, 0, sizeof(DECODER_CTX_T));
    *handle = (PLAYER_DECODER)ctx;
    return OPRT_OK;
}

OPERATE_RET ai_player_decoder_deinit(PLAYER_DECODER handle)
{
    DECODER_CTX_T *ctx = (DECODER_CTX_T *)handle;

    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }

    DECODER_T *decoder = ctx->decoder;
    if(decoder && decoder->stop) {
        decoder->stop(ctx->priv);
    }

    Free(ctx);
    return OPRT_OK;
}

OPERATE_RET ai_player_decoder_start(PLAYER_DECODER handle, AI_AUDIO_CODEC_E codec)
{
    DECODER_CTX_T *ctx = (DECODER_CTX_T *)handle;
    DECODER_T *decoder = NULL;

    switch(codec) {
#if defined(AI_PLAYER_DECODER) && (AI_PLAYER_DECODER & AI_PLAYER_DECODER_MP3)
        case AI_AUDIO_CODEC_MP3:
            decoder = &g_decoder_mp3;
            break;
#endif
#if defined(AI_PLAYER_DECODER) && (AI_PLAYER_DECODER & AI_PLAYER_DECODER_WAV)
        case AI_AUDIO_CODEC_WAV:
            decoder = &g_decoder_wav;
            break;
#endif
#if defined(AI_PLAYER_DECODER) && (AI_PLAYER_DECODER & AI_PLAYER_DECODER_OPUS)
        case AI_AUDIO_CODEC_OPUS:
            decoder = &g_decoder_opus;
            break;
#endif
#if defined(AI_PLAYER_DECODER) && (AI_PLAYER_DECODER & AI_PLAYER_DECODER_OGGOPUS)
        case AI_AUDIO_CODEC_OGGOPUS:
            decoder = &g_decoder_oggopus;
            break;
#endif
        default:
            PR_ERR("unsupported codec %d", codec);
            return OPRT_INVALID_PARM;
    }

    if (decoder == NULL || decoder->start == NULL || decoder->stop == NULL || decoder->process == NULL) {
        return OPRT_INVALID_PARM;
    }

    if(ctx->decoder) {
        ctx->decoder->stop(ctx->priv);
        ctx->priv = NULL;
    }

    ctx->decoder = decoder;
    return decoder->start(&ctx->priv);
}

OPERATE_RET ai_player_decoder_stop(PLAYER_DECODER handle)
{
    DECODER_CTX_T *ctx = (DECODER_CTX_T *)handle;

    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }

    if(ctx->decoder) {
        ctx->decoder->stop(ctx->priv);
    }
    ctx->priv = NULL;

    return OPRT_OK;
}

int ai_player_decoder_process(PLAYER_DECODER handle, uint8_t *in_buf, int in_len, uint8_t *out_buf, int out_size, DECODER_OUTPUT_T *output)
{
    DECODER_CTX_T *ctx = (DECODER_CTX_T *)handle;

    if (!ctx || !in_buf || in_len < 0 || !out_buf || out_size <= 0 || !output) {
        return -1;
    }

    return ctx->decoder->process(ctx->priv, in_buf, in_len, out_buf, out_size, output);
}

AI_AUDIO_CODEC_E ai_player_decoder_get_codec(PLAYER_DECODER handle)
{
    DECODER_CTX_T *ctx = (DECODER_CTX_T *)handle;

    if (!ctx || !ctx->decoder) {
        return AI_AUDIO_CODEC_MP3;
    }

#if defined(AI_PLAYER_DECODER) && (AI_PLAYER_DECODER & AI_PLAYER_DECODER_MP3)
    if (ctx->decoder == &g_decoder_mp3) {
        return AI_AUDIO_CODEC_MP3;
    }
#endif

#if defined(AI_PLAYER_DECODER) && (AI_PLAYER_DECODER & AI_PLAYER_DECODER_WAV)
    if (ctx->decoder == &g_decoder_wav) {
        return AI_AUDIO_CODEC_WAV;
    }
#endif

#if defined(AI_PLAYER_DECODER) && (AI_PLAYER_DECODER & AI_PLAYER_DECODER_OPUS)
    if (ctx->decoder == &g_decoder_opus) {
        return AI_AUDIO_CODEC_OPUS;
    }
#endif

#if defined(AI_PLAYER_DECODER) && (AI_PLAYER_DECODER & AI_PLAYER_DECODER_OGGOPUS)
    if (ctx->decoder == &g_decoder_oggopus) {
        return AI_AUDIO_CODEC_OGGOPUS;
    }
#endif

    return AI_AUDIO_CODEC_MP3;
}
