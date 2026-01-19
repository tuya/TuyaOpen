/**
 * @file decoder_wav.c
 * @brief 
 * @version 0.1
 * @date 2025-11-06
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

#define WAV_HEAD_SIZE (44)

#define WAV_RIFF_POS                0
#define WAV_FORMAT_POS              8
#define WAV_CHANNEL_POS             22
#define WAV_RATE_POS                24
#define WAV_BITS_PER_SAMPLE_POS     34

typedef struct {
    DECODER_OUTPUT_T output;
    BOOL_T is_first_frame;
} DECODER_WAV_CTX_T;

static int __wav_find_data_offset(uint8_t *buf, int in_len, DECODER_OUTPUT_T *output)
{
    if (!buf || in_len <= 0 || !output) {
        return -1;
    }

    if (!memcmp(&buf[WAV_RIFF_POS], "RIFF", 4) && !memcmp(&buf[WAV_FORMAT_POS], "WAVE", 4)) {
        output->channel = buf[WAV_CHANNEL_POS] | (buf[WAV_CHANNEL_POS + 1] << 8);

        output->sample = buf[WAV_RATE_POS] & 0x000000FF;
        output->sample |= (buf[WAV_RATE_POS + 1] & 0x000000FF) << 8;
        output->sample |= (buf[WAV_RATE_POS + 2] & 0x000000FF) << 16;
        output->sample |= (buf[WAV_RATE_POS + 3] & 0x000000FF) << 24;

        output->datebits = buf[WAV_BITS_PER_SAMPLE_POS] | (buf[WAV_BITS_PER_SAMPLE_POS + 1] << 8);
    } else {
        PR_ERR("invalid wav header");
        return -1;
    }

    // Check for "data" chunk
    int i = 0;
    for (i = 0; i < in_len - 8; i++) {
        if (buf[i] == 'd' && buf[i + 1] == 'a' && buf[i + 2] == 't' && buf[i + 3] == 'a') {
            return i + 8;  // Skip "data" and size fields
        }
    }

    return 0;
}

OPERATE_RET decoder_wav_start(void* *handle)
{
    DECODER_WAV_CTX_T *ctx = tal_malloc(sizeof(DECODER_WAV_CTX_T));
    if (!ctx) {
        return OPRT_MALLOC_FAILED;
    }

    memset(ctx, 0, sizeof(DECODER_WAV_CTX_T));
    ctx->is_first_frame = true;
    *handle = (void*)ctx;
    return OPRT_OK;
}

OPERATE_RET decoder_wav_stop(void* handle)
{
    DECODER_WAV_CTX_T *ctx = (DECODER_WAV_CTX_T *)handle;
    if (!ctx) {
        return OPRT_INVALID_PARM;
    }

    tal_free(ctx);
    return OPRT_OK;
}

int decoder_wav_process(void* handle, uint8_t *in_buf, int in_len, uint8_t *out_buf, int out_size, DECODER_OUTPUT_T *output)
{
    DECODER_WAV_CTX_T *ctx = (DECODER_WAV_CTX_T *)handle;
    if (!ctx) {
        return -1;
    }

    int offset = 0;
    if(ctx->is_first_frame) {
        if (in_len < WAV_HEAD_SIZE) {
            return in_len;
        }

        offset = __wav_find_data_offset((uint8_t *)in_buf, in_len, &ctx->output);
        if (offset < 0) {
            return -2;
        } else if(offset == 0) {
            return in_len;
        }

        in_len -= offset;
        ctx->is_first_frame = false;
    }

    memcpy(output, &ctx->output, sizeof(DECODER_OUTPUT_T));

    if(in_len >= out_size) {
        output->samples = (out_size * 8) / (output->channel * output->datebits);
    } else if(in_len > 0) {
        output->samples = (in_len * 8) / (output->channel * output->datebits);
    } else {
        output->samples = 0;
        return 0;
    }

    out_size = output->samples * output->channel * output->datebits / 8;
    memcpy(out_buf, in_buf + offset, out_size);
    output->used_size = out_size;
    in_len -= out_size;

    return in_len;
}


DECODER_T g_decoder_wav = {
    .start = decoder_wav_start,
    .stop = decoder_wav_stop,
    .process = decoder_wav_process
};
