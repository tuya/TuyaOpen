/**
 * @file decoder_opus.c
 * @brief Raw Opus decoder implementation
 * @version 0.1
 * @date 2025-11-28
 *
 * @copyright Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_AI_PLAYER_DECODER_OGGOPUS) && (ENABLE_AI_PLAYER_DECODER_OGGOPUS == 1)
#include "decoder_cfg.h"
#include "./opus/opus.h"
#include <string.h>

#ifndef AI_PLAYER_DECODER_OPUS_FRAME_SIZE
#define AI_PLAYER_DECODER_OPUS_FRAME_SIZE 80
#endif
#ifndef AI_PLAYER_DECODER_OPUS_KBPS
#define AI_PLAYER_DECODER_OPUS_KBPS 16
#endif
#define DEFAULT_SAMPLE_RATE 16000
#define DEFAULT_CHANNELS 1
#define DEFAULT_FRAME_SIZE ((DEFAULT_CHANNELS) * ((AI_PLAYER_DECODER_OPUS_KBPS) * 1000 * (AI_PLAYER_DECODER_OPUS_FRAME_SIZE) / 1000) / 8)
// calculation: frame_size_bytes = channels * (kbps * frame_size_ms / 1000) / 8
#define OPUS_PR_DEBUG PR_TRACE

static int s_opus_frame_size_bytes = DEFAULT_FRAME_SIZE;

typedef struct {
    OpusDecoder *decoder;
    int sample_rate;
    int channels;
    int frame_size_bytes; // CBR frame size
    BOOL_T initialized;
} DECODER_RAWOPUS_CTX_T;

static OPERATE_RET decoder_opus_start(void* *handle)
{
    DECODER_RAWOPUS_CTX_T *ctx = tal_malloc(sizeof(DECODER_RAWOPUS_CTX_T));
    if (!ctx) {
        return OPRT_MALLOC_FAILED;
    }

    memset(ctx, 0, sizeof(DECODER_RAWOPUS_CTX_T));

    ctx->sample_rate = DEFAULT_SAMPLE_RATE;
    ctx->channels = DEFAULT_CHANNELS;
    ctx->frame_size_bytes = s_opus_frame_size_bytes;

    // Initialize Opus decoder
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM==1)
    ctx->decoder = tal_psram_malloc(opus_decoder_get_size(ctx->channels));
#else
    ctx->decoder = tal_malloc(opus_decoder_get_size(ctx->channels));
#endif
    if (!ctx->decoder) {
        tal_free(ctx);
        return OPRT_MALLOC_FAILED;
    }

    if (opus_decoder_init(ctx->decoder, ctx->sample_rate, ctx->channels) != 0) {
        tal_free(ctx->decoder);
        tal_free(ctx);
        return OPRT_COM_ERROR;
    }

    ctx->initialized = true;
    *handle = ctx;

    OPUS_PR_DEBUG("Opus decoder started: %dHz, %dch, frame=%d",
             ctx->sample_rate, ctx->channels, ctx->frame_size_bytes);

    return OPRT_OK;
}

static OPERATE_RET decoder_opus_stop(void* handle)
{
    DECODER_RAWOPUS_CTX_T *ctx = (DECODER_RAWOPUS_CTX_T *)handle;
    if (!ctx) {
        return OPRT_INVALID_PARM;
    }

    if (ctx->decoder) {
        opus_decoder_destroy(ctx->decoder);
        ctx->decoder = NULL;
    }

    tal_free(ctx);
    return OPRT_OK;
}

static int decoder_opus_process(void* handle, uint8_t *in_buf, int in_len,
                                     uint8_t *out_buf, int out_size, DECODER_OUTPUT_T *output)
{
    DECODER_RAWOPUS_CTX_T *ctx = (DECODER_RAWOPUS_CTX_T *)handle;
    if (!ctx || !out_buf || out_size <= 0 || !output) {
        return -1;
    }

    memset(output, 0, sizeof(DECODER_OUTPUT_T));

    int total_decoded_samples = 0;
    int total_decoded_bytes = 0;
    int current_offset = 0;

    // Process frames directly from input buffer
    while (in_len - current_offset >= ctx->frame_size_bytes) {
        // Check output buffer space
        int samples_per_channel_available = (out_size - total_decoded_bytes) / (ctx->channels * sizeof(SHORT_T));

        // If no space left, return remaining input length
        if (samples_per_channel_available <= 0) {
            break;
        }

        int samples_decoded = opus_decode(ctx->decoder,
                                            in_buf + current_offset,
                                            ctx->frame_size_bytes,
                                            (SHORT_T *)(out_buf + total_decoded_bytes),
                                            samples_per_channel_available,
                                            0);
        if (samples_decoded < 0) {
            if (samples_decoded == OPUS_BUFFER_TOO_SMALL) {
                // Output buffer full, stop here
                break;
            }
            PR_ERR("opus_decode failed: %d, frame_size_bytes: %d", samples_decoded, ctx->frame_size_bytes);
            // Skip this frame on error to avoid infinite loop
            current_offset += ctx->frame_size_bytes;
            continue;
        }
        OPUS_PR_DEBUG("opus_decode decoded %d samples", samples_decoded);
        int decoded_bytes = samples_decoded * ctx->channels * sizeof(SHORT_T);
        total_decoded_samples += samples_decoded;
        total_decoded_bytes += decoded_bytes;
        current_offset += ctx->frame_size_bytes;

        // If we filled the output buffer, stop
        if (total_decoded_bytes >= out_size) {
            break;
        }
    }

    output->sample = (TKL_AUDIO_SAMPLE_E)ctx->sample_rate;
    output->datebits = TKL_AUDIO_DATABITS_16;
    output->channel = (ctx->channels == 1) ? TKL_AUDIO_CHANNEL_MONO : TKL_AUDIO_CHANNEL_STEREO;
    output->samples = total_decoded_samples;
    output->used_size = total_decoded_bytes;

    // Return remaining bytes in input buffer
    return in_len - current_offset;
}

// This function allows setting a custom frame size for Opus decoding globally
OPERATE_RET decoder_opus_set_frame_size(int frame_size_bytes)
{
    if (frame_size_bytes <= 0) {
        return OPRT_INVALID_PARM;
    }
    PR_DEBUG("Setting Opus decoder frame size to %d bytes", frame_size_bytes);
    s_opus_frame_size_bytes = frame_size_bytes;
    return OPRT_OK;
}

DECODER_T g_decoder_opus = {
    .start = decoder_opus_start,
    .stop = decoder_opus_stop,
    .process = decoder_opus_process,
};
#endif