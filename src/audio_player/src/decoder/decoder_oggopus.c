/**
 * @file decoder_oggopus.c
 * @brief OggOpus decoder implementation
 * @version 0.1
 * @date 2025-11-25
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

#if defined(ENABLE_AI_PLAYER_DECODER_OGGOPUS) && (ENABLE_AI_PLAYER_DECODER_OGGOPUS == 1)
#include "decoder_cfg.h"
#include "tal_memory.h"
#include "./opus/opus.h"
#include "ogg/ogg.h"

#define OPUS_HEAD_MAGIC "OpusHead"
#define OPUS_TAGS_MAGIC "OpusTags"
#define OPUS_HEAD_SIZE  19
#define OPUS_DECODER_SAMPLE_RATE 16000  // Fixed output sample rate for decoder

#define OPUS_PR_DEBUG PR_TRACE

typedef enum {
    OPUS_STATE_NEED_HEADER,
    OPUS_STATE_NEED_TAGS,
    OPUS_STATE_DECODING
} OPUS_DECODE_STATE_E;

typedef struct {
    BOOL_T is_first_frame;
    OpusDecoder *decoder;
    ogg_sync_state sync_state;
    ogg_stream_state stream_state;
    BOOL_T stream_initialized;
    OPUS_DECODE_STATE_E state;
    int sample_rate;
    int channels;
    int pre_skip;
    int output_gain;
    BOOL_T need_more_data;
} DECODER_OGGOPUS_CTX_T;

/**
 * @brief Parse OpusHead packet to extract decoder parameters
 *
 * OpusHead structure:
 * - 8 bytes: "OpusHead" magic signature
 * - 1 byte:  Version (must be 1)
 * - 1 byte:  Channel count (1-255)
 * - 2 bytes: Pre-skip (little-endian uint16)
 * - 4 bytes: Input sample rate in Hz (little-endian uint32, informational)
 * - 2 bytes: Output gain in dB (little-endian int16, Q7.8 format)
 * - 1 byte:  Channel mapping family
 */
static int __parse_opus_head(uint8_t *data, int len, DECODER_OGGOPUS_CTX_T *ctx)
{
    if (!data || len < OPUS_HEAD_SIZE || !ctx) {
        PR_ERR("Invalid OpusHead packet: len=%d", len);
        return -1;
    }

    // Check magic signature
    if (memcmp(data, OPUS_HEAD_MAGIC, 8) != 0) {
        PR_ERR("Invalid OpusHead magic signature");
        return -1;
    }

    // Check version
    uint8_t version = data[8];
    if (version != 1) {
        PR_ERR("Unsupported Opus version: %d", version);
        return -1;
    }

    // Extract parameters
    ctx->channels = data[9];
    ctx->pre_skip = data[10] | (data[11] << 8);

    uint32_t input_sample_rate = data[12] | (data[13] << 8) | (data[14] << 16) | (data[15] << 24);
    ctx->output_gain = (SHORT_T)(data[16] | (data[17] << 8));

    uint8_t channel_mapping = data[18];

    // Validate parameters
    if (ctx->channels < 1 || ctx->channels > 2) {
        PR_ERR("Unsupported channel count: %d", ctx->channels);
        return -1;
    }

    if (channel_mapping != 0) {
        PR_WARN("Channel mapping family %d not fully supported", channel_mapping);
    }

    // fixed output sample rate
    ctx->sample_rate = OPUS_DECODER_SAMPLE_RATE;

    PR_DEBUG("OpusHead: channels=%d, pre_skip=%d, input_rate=%d, gain=%d",
             ctx->channels, ctx->pre_skip, input_sample_rate, ctx->output_gain);

    return 0;
}

static OPERATE_RET decoder_oggopus_start(void* *handle)
{
    DECODER_OGGOPUS_CTX_T *ctx = tal_malloc(sizeof(DECODER_OGGOPUS_CTX_T));
    if (!ctx) {
        return OPRT_MALLOC_FAILED;
    }

    memset(ctx, 0, sizeof(DECODER_OGGOPUS_CTX_T));

    // Initialize Ogg sync state
    if (ogg_sync_init(&ctx->sync_state) != 0) {
        tal_free(ctx);
        return OPRT_COM_ERROR;
    }

    ctx->is_first_frame = true;
    ctx->stream_initialized = false;
    ctx->state = OPUS_STATE_NEED_HEADER;
    ctx->need_more_data = false;

    *handle = ctx;
    return OPRT_OK;
}

static OPERATE_RET decoder_oggopus_stop(void* handle)
{
    DECODER_OGGOPUS_CTX_T *ctx = (DECODER_OGGOPUS_CTX_T *)handle;
    if (!ctx) {
        return OPRT_INVALID_PARM;
    }

    // Destroy Opus decoder
    if (ctx->decoder) {
        opus_decoder_destroy(ctx->decoder);
        ctx->decoder = NULL;
    }

    // Clear Ogg stream state
    if (ctx->stream_initialized) {
        ogg_stream_clear(&ctx->stream_state);
        ctx->stream_initialized = false;
    }

    // Clear Ogg sync state
    ogg_sync_clear(&ctx->sync_state);

    tal_free(ctx);
    return OPRT_OK;
}

static int decoder_oggopus_process(void* handle, uint8_t *in_buf, int in_len,
                                     uint8_t *out_buf, int out_size, DECODER_OUTPUT_T *output)
{
    OPERATE_RET rt = OPRT_OK;
    DECODER_OGGOPUS_CTX_T *ctx = (DECODER_OGGOPUS_CTX_T *)handle;

    if (!ctx || !out_buf || out_size <= 0 || !output) {
        return -1;
    }

    // Allow in_len = 0 when processing pending data from previous call
    // But in_buf should be valid if in_len > 0
    if (in_len > 0 && !in_buf) {
        return -1;
    }

    memset(output, 0, sizeof(DECODER_OUTPUT_T));

    // Feed data to Ogg sync layer only if we have new input data
    if (in_len > 0) {
        CHAR_T *buffer = ogg_sync_buffer(&ctx->sync_state, in_len);
        if (!buffer) {
            PR_ERR("ogg_sync_buffer failed");
            return -1;
        }
        memcpy(buffer, in_buf, in_len);

        if (ogg_sync_wrote(&ctx->sync_state, in_len) != 0) {
            PR_ERR("ogg_sync_wrote failed");
            return -1;
        }
        OPUS_PR_DEBUG("Fed %d bytes to Ogg sync buffer", in_len);
    }

    ogg_page page;
    ogg_packet packet;

    // Process Ogg pages first to ensure stream is initialized and packets are available
    // ogg_sync_pageout returns:
    //  1: Success, page returned
    //  0: Need more data
    // -1: Stream out of sync (we should continue to try to find the next page)
    while ((rt = ogg_sync_pageout(&ctx->sync_state, &page)) != 0) {
        if (rt == -1) {
            PR_WARN("Ogg sync lost, skipping to next page...");
            continue;
        }

        int serial = ogg_page_serialno(&page);

        // Initialize stream on first page
        if (!ctx->stream_initialized) {
            if (ogg_stream_init(&ctx->stream_state, serial) != 0) {
                PR_ERR("ogg_stream_init failed");
                return -1;
            }
            ctx->stream_initialized = true;
        } else {
            // Check for chained stream (serial number change)
            if (ctx->stream_state.serialno != serial) {
                PR_INFO("Stream serial number changed from %d to %d (chained stream), resetting...",
                        ctx->stream_state.serialno, serial);
                ogg_stream_reset(&ctx->stream_state);
                ogg_stream_clear(&ctx->stream_state); // Clear old stream
                if (ogg_stream_init(&ctx->stream_state, serial) != 0) {
                    PR_ERR("ogg_stream_init failed for chained stream");
                    return -1;
                }
                // Reset decoder state for new stream if needed, but usually Opus handles this via headers
                // For now we assume the new stream starts with headers too
                ctx->state = OPUS_STATE_NEED_HEADER;
                if (ctx->decoder) {
                    opus_decoder_destroy(ctx->decoder);
                    ctx->decoder = NULL;
                }
            }
        }

        // Submit page to stream (packets will be buffered for extraction)
        if (ogg_stream_pagein(&ctx->stream_state, &page) != 0) {
            PR_ERR("ogg_stream_pagein failed");
            continue;
        }
    }

    // Now extract and process all available packets from the stream
    // This handles both: 1) packets from newly submitted pages, 2) pending packets from previous call
    if (ctx->stream_initialized) {
        // ogg_stream_packetout returns:
        //  1: Success, packet returned
        //  0: Need more data
        // -1: Gap in data (lost packet), we should continue
        while ((rt = ogg_stream_packetout(&ctx->stream_state, &packet)) != 0) {
            if (rt == -1) {
                PR_WARN("Gap in Ogg stream (lost packet), continuing...");
                continue;
            }

            OPUS_PR_DEBUG("Extracted 1 Ogg packet (%d bytes)", packet.bytes);
            // Handle OpusHead header
            if (ctx->state == OPUS_STATE_NEED_HEADER) {
                if (__parse_opus_head(packet.packet, packet.bytes, ctx) != 0) {
                    PR_ERR("Failed to parse OpusHead");
                    return -1;
                }

                // Create Opus decoder
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM==1)
                ctx->decoder = tal_psram_malloc(opus_decoder_get_size(ctx->channels));
#else
                ctx->decoder = tal_malloc(opus_decoder_get_size(ctx->channels));
#endif
                if (!ctx->decoder) {
                    PR_ERR("Failed to allocate OpusDecoder (%d bytes)", opus_decoder_get_size(ctx->channels));
                    return -1;
                }
                rt = opus_decoder_init(ctx->decoder, ctx->sample_rate, ctx->channels);
                if (rt != OPRT_OK) {
                    tal_free(ctx->decoder);
                    ctx->decoder = NULL;
                    PR_ERR("opus_decoder_init failed: %d", rt);
                    return -1;
                }

                ctx->state = OPUS_STATE_NEED_TAGS;
                OPUS_PR_DEBUG("Opus decoder created: %dHz, %d channels", ctx->sample_rate, ctx->channels);
                continue;
            }

            // Handle OpusTags header
            if (ctx->state == OPUS_STATE_NEED_TAGS) {
                if (packet.bytes >= 8 && memcmp(packet.packet, OPUS_TAGS_MAGIC, 8) == 0) {
                    uint32_t tag_len = 0;
                    memcpy(&tag_len, packet.packet + 8, sizeof(uint32_t));
                    OPUS_PR_DEBUG("Skipping OpusTags packet");
                    ctx->state = OPUS_STATE_DECODING;
                    continue;
                } else {
                    ctx->state = OPUS_STATE_DECODING;
                }
            }

            // Decode audio packet
            if (ctx->state == OPUS_STATE_DECODING) {
                int samples_per_channel_available = out_size / (ctx->channels * sizeof(SHORT_T));
                int max_frame_samples = ctx->sample_rate * 60 / 1000;

                if (samples_per_channel_available < max_frame_samples) {
                    OPUS_PR_DEBUG("Buffer too small, need %d samples, have %d", max_frame_samples, samples_per_channel_available);
                    break;
                }

                int samples_decoded = opus_decode(ctx->decoder, packet.packet, packet.bytes,
                                                     (SHORT_T *)out_buf, samples_per_channel_available, 0);
                if (samples_decoded < 0) {
                    PR_ERR("opus_decode failed: %d", samples_decoded);
                    continue;
                }
                OPUS_PR_DEBUG("Decoded %d samples from Opus packet (%d bytes)", samples_decoded, packet.bytes);

                if (samples_decoded > 0) {
                    uint32_t decoded_bytes = samples_decoded * ctx->channels * sizeof(SHORT_T);
                    output->sample = (TKL_AUDIO_SAMPLE_E)ctx->sample_rate;
                    output->datebits = TKL_AUDIO_DATABITS_16;
                    output->channel = (ctx->channels == 1) ? TKL_AUDIO_CHANNEL_MONO : TKL_AUDIO_CHANNEL_STEREO;
                    output->samples += samples_decoded;
                    output->used_size += decoded_bytes;
                    out_buf += decoded_bytes;
                    out_size -= decoded_bytes;

                    // Check if we have enough space for another frame
                    if (samples_per_channel_available < max_frame_samples) {
                        OPUS_PR_DEBUG("Buffer too small, need %d samples, have %d", max_frame_samples, samples_per_channel_available);
                        break;
                    }
                }
            }
        }
    }

    // Check if stream has more packets available (without consuming them)
    if (ctx->stream_initialized) {
        ogg_packet peek_packet;
        if (ogg_stream_packetpeek(&ctx->stream_state, &peek_packet) == 1) {
            // Still have packets waiting, return BUFFER_NOT_ENOUGH to signal caller to continue
            OPUS_PR_DEBUG("Decode: output=%d samples (%d bytes), has pending packets",
                     output->samples, output->used_size);
            return OPRT_BUFFER_NOT_ENOUGH;
        }
    }

    OPUS_PR_DEBUG("Decode: output=%d samples (%d bytes)", output->samples, output->used_size);
    return 0;  // All data processed successfully
}

DECODER_T g_decoder_oggopus = {
    .start = decoder_oggopus_start,
    .stop = decoder_oggopus_stop,
    .process = decoder_oggopus_process
};
#endif