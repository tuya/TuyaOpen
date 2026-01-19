/**
 * @file decoder_mp3.c
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
#include <stdint.h>
#define MINIMP3_IMPLEMENTATION

#include "tal_api.h"
#include "decoder_cfg.h"
#include "./minimp3/minimp3.h"

#define MP3_HEAD_SIZE (10)

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#define DECODER_MP3_MALLOC tal_psram_malloc
#define DECODER_MP3_FREE   tal_psram_free
#else
#define DECODER_MP3_MALLOC tal_malloc
#define DECODER_MP3_FREE   tal_free
#endif

typedef struct {
    bool is_first_frame;
    mp3dec_t decoder;
    uint32_t id3_size;
} DECODER_MP3_CTX_T;

static int __mp3_find_id3(uint8_t *buf)
{
    char tag_header[MP3_HEAD_SIZE];
    int tag_size = 0;

    // Check buffer validity and length
    if (!buf) {
        return 0;
    }

    memcpy(tag_header, buf, sizeof(tag_header));

    if (tag_header[0] == 'I' && tag_header[1] == 'D' && tag_header[2] == '3') {
        tag_size = ((tag_header[6] & 0x7F) << 21) | ((tag_header[7] & 0x7F) << 14) | ((tag_header[8] & 0x7F) << 7) | (tag_header[9] & 0x7F);
        PR_DEBUG("ID3 tag_size = %d", tag_size);
        return tag_size + sizeof(tag_header);
    } else {
        // tag_header ignored
        PR_DEBUG("ID3 tag_size n/a");
        return 0;
    }
}

OPERATE_RET decoder_mp3_start(void* *handle)
{
    DECODER_MP3_CTX_T *ctx = DECODER_MP3_MALLOC(sizeof(DECODER_MP3_CTX_T));
    if (!ctx) {
        return OPRT_MALLOC_FAILED;
    }

    memset(ctx, 0, sizeof(DECODER_MP3_CTX_T));
    ctx->is_first_frame = true;
    ctx->id3_size = 0;
    mp3dec_init(&ctx->decoder);
    *handle = ctx;
    return OPRT_OK;
}

OPERATE_RET decoder_mp3_stop(void* handle)
{
    DECODER_MP3_CTX_T *ctx = (DECODER_MP3_CTX_T *)handle;
    if (!ctx) {
        return OPRT_INVALID_PARM;
    }

    // mp3dec_t is a struct, no need to free
    // Just reset it if needed
    memset(&ctx->decoder, 0, sizeof(mp3dec_t));

    DECODER_MP3_FREE(ctx);

    return OPRT_OK;
}

int decoder_mp3_process(void* handle, uint8_t *in_buf, int in_len, uint8_t *out_buf, int out_size, DECODER_OUTPUT_T *output)
{
    DECODER_MP3_CTX_T *ctx = (DECODER_MP3_CTX_T *)handle;

    // Parameter validation
    if (!ctx) {
        return -1;
    }

    int samples = 0;
    int temp_len = (int)in_len;
    mp3dec_frame_info_t frame_info = {0};
    mp3d_sample_t *pcm_buf = (mp3d_sample_t *)out_buf;
    int pcm_buf_samples = out_size / sizeof(mp3d_sample_t);

    if(ctx->is_first_frame) {
        if(in_len < MP3_HEAD_SIZE) {
            return in_len;
        }

        if (0 == ctx->id3_size) {
            ctx->id3_size = __mp3_find_id3(in_buf);
        }

        // Check if id3_size is valid and doesn't exceed input length
        if (ctx->id3_size > in_len) {
            // ID3 tag spans multiple buffers, wait for more data
            ctx->id3_size -= in_len;
            return 0;
        }

        // Skip ID3 tag
        in_buf += ctx->id3_size;
        in_len -= ctx->id3_size;
        ctx->is_first_frame = false;
    }

DECODE_MORE:
    // Check buffer validity before decoding
    if (!in_buf || in_len <= 0) {
        return 0;
    }

    if (!pcm_buf || pcm_buf_samples <= 0) {
        return 0;
    }

    // mp3dec_decode_frame automatically finds sync word and decodes one frame
    samples = mp3dec_decode_frame(&ctx->decoder, in_buf, in_len, pcm_buf, &frame_info);
    if(samples <= 0) {
        // No frame decoded or error
        if(frame_info.frame_bytes == 0) {
            // Need more data - frame_bytes=0 means no valid frame found, need more input
            return temp_len;
        } else {
            PR_ERR("decoder_mp3_process: invalid frame_bytes=%d, in_len=%d", frame_info.frame_bytes, in_len);
            return 0;
        }
    }

    // Update input buffer position
    in_buf += frame_info.frame_bytes;
    in_len -= frame_info.frame_bytes;

    if(frame_info.channels > 0 && samples > 0) {
        // mp3dec_decode_frame returns samples per channel, not total samples
        // Total samples written to pcm buffer = samples * channels
        uint32_t total_samples = samples * frame_info.channels;
        uint32_t used_size = total_samples * sizeof(mp3d_sample_t);

        output->used_size += used_size;
        output->samples += samples;  // samples is already per channel
        output->sample = frame_info.hz;
        output->datebits = 16; // minimp3 outputs 16-bit samples by default
        output->channel = frame_info.channels;

        // pcm_buf needs to advance by total_samples (samples * channels)
        // because mp3dec_decode_frame writes samples * channels samples to the buffer
        pcm_buf += total_samples;
        pcm_buf_samples -= total_samples;
        out_size -= used_size;

        // Try to decode more frames if there's space and data
        if((out_size > used_size) && (in_len > 2)) { // at least 2 bytes for next frame
            temp_len = in_len;
            goto DECODE_MORE;
        }
    }

    return in_len;
}


DECODER_T g_decoder_mp3 = {
    .start = decoder_mp3_start,
    .stop = decoder_mp3_stop,
    .process = decoder_mp3_process
};
