/**
 * @file speex_encode.c
 * @brief Defines the Speex audio encoding interface for Tuya's audio service.
 *
 * This source file provides the implementation of Speex audio encoding
 * functionality within the Tuya audio service framework. It includes the
 * initialization, configuration, and processing functions for Speex encoding,
 * which handles the compression of audio data using the Speex codec. The
 * implementation supports various encoding parameters including quality settings,
 * bit rates, and frame sizes. This file is essential for developers working on
 * IoT applications that require efficient voice compression and transmission
 * while maintaining acceptable audio quality.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <speex/speex.h>
#include "speex_encode.h"
#include "speaker_encode_types.h"
#include "speaker_upload.h"
#include "tuya_cloud_types.h"
#include "tal_log.h"

#define SPEEX_MODEID_WB_RATE        16000
#define SPEEX_FRAME_SIZE            320                                // need use SPEEX_GET_FRAME_SIZE
#define SPEEX_FRAME_BYTE            (SPEEX_FRAME_SIZE * sizeof(short)) // 16 bits/sample
#define SPEEX_MAX_FRAME_BYTES       200
#define SPEEX_VER_STRING_LEN        16
#define SPEEX_QUALITY_DEF           5 // Set the quality to 5(16k rate: 8-->27.8kbps 5-->16.8kbps)
#define MODE_1_QUALITY_5_FRAME_SIZE 42
#define MODE_1_QUALITY_8_FRAME_SIZE 70
#define SPEEX_ENCODE_BUFFER_LEN     (MODE_1_QUALITY_5_FRAME_SIZE * 5)

/**
 * TY_MEDIA_HEAD_S for speex encode
 *
 * NOTE: The size of a frame after encode depends on mode and quality
 * mode(0) quality(8) --> 38
 * mode(1) quality(5) --> 42
 * mode(1) quality(8) --> 70
 */

typedef struct {
    void *state;                    ///< speex encode state
    SpeexBits bits;                 ///< speex encode bits
    short buffer[SPEEX_FRAME_SIZE]; ///< speex encode buffer
    uint32_t buffer_offset;         ///< speex encode buffer offset

#if defined(ENABLE_VOICE_PROTOCOL_STREAM_GW)
    TUYA_VOICE_WS_START_PARAMS_S *p_head; ///< speex encode head
#else
    struct {
        uint8_t ver_id;
        char ver_string[SPEEX_VER_STRING_LEN];
        uint8_t mode;
        uint8_t mode_bit_stream_ver;
        uint32_t rate;
        uint8_t channels;
        uint32_t bit_rate;
        uint32_t frame_size;
        uint8_t vbr;
        uint8_t encode_frame_size;
    } PACKED SPEEX_HEAD_S, *p_head; ///< speex encode head
#endif /** ENABLE_VOICE_PROTOCOL_STREAM_GW */
} SPEEX_ENCODE_S;

static OPERATE_RET speex_encode_free(SPEAKER_MEDIA_ENCODER_S *p_encoder)
{
    if (NULL == p_encoder || NULL == p_encoder->p_encode_info) {
        PR_ERR("invalid parm");
        return OPRT_INVALID_PARM;
    }

    SPEEX_ENCODE_S *p_speex = (SPEEX_ENCODE_S *)p_encoder->p_encode_info;
    speex_encoder_destroy(p_speex->state);
    speex_bits_destroy(&p_speex->bits);
    if (NULL != p_speex->p_head)
        free(p_speex->p_head);

    free(p_encoder->p_encode_info);
    p_encoder->p_encode_info = NULL;

    if (p_encoder->p_buffer) {
        free(p_encoder->p_buffer);
        p_encoder->p_buffer = NULL;
    }

    PR_DEBUG("encode free\n");
    return OPRT_OK;
}

static unsigned int speex_data_encode(SPEAKER_MEDIA_ENCODER_S *p_encoder, void *private_data,
                                      const unsigned char *buffer, unsigned int size)
{
    SPEEX_ENCODE_S *p_speex = NULL;
    OPERATE_RET ret = OPRT_OK;
    unsigned int encode_len = 0, cp_len = 0;
    float input[SPEEX_FRAME_SIZE] = {0x0};
    char cbits[SPEEX_MAX_FRAME_BYTES] = {0x0};
    int nbBytes = 0, i = 0;

    if (NULL == p_encoder || NULL == buffer || NULL == p_encoder->p_encode_info ||
        NULL == p_encoder->encoder_data_callback) {
        PR_ERR("%p %p %p", p_encoder, buffer, p_encoder->p_encode_info);
        return OPRT_INVALID_PARM;
    }

    p_speex = (SPEEX_ENCODE_S *)p_encoder->p_encode_info;
    while (1) {
        cp_len = (p_speex->buffer_offset + (size - encode_len) < SPEEX_FRAME_BYTE)
                     ? (size - encode_len)
                     : (SPEEX_FRAME_BYTE - p_speex->buffer_offset);

        memcpy((uint8_t *)p_speex->buffer + p_speex->buffer_offset, buffer + encode_len, cp_len);
        p_speex->buffer_offset += cp_len;
        encode_len += cp_len;

        if (SPEEX_FRAME_BYTE != p_speex->buffer_offset) {
            return encode_len;

        } else {
            for (i = 0; i < SPEEX_FRAME_SIZE; i++) {
                input[i] = p_speex->buffer[i];
            }

            speex_bits_reset(&p_speex->bits);
            speex_encode(p_speex->state, input, &p_speex->bits); // dump
            nbBytes = speex_bits_write(&p_speex->bits, cbits, SPEEX_MAX_FRAME_BYTES);

            /** FIXME: do write data to upload, but impl inside! */
            ret = p_encoder->encoder_data_callback(p_encoder, private_data, (unsigned char *)cbits, nbBytes);
            p_speex->buffer_offset = 0;
            if (OPRT_OK != ret) {
                return ret;
            }
        }

        if (encode_len >= size) {
            break;
        }

        PR_TRACE("buffer_offset:%d, size:%d, encode_len:%d", p_speex->buffer_offset, size, encode_len);
    }

    return encode_len;
}

static void *__speex_encoder_init(SPEAKER_MEDIA_ENCODER_S *encoder)
{
    int bitrate = 0;
    const char *speex_version = NULL;
    spx_int32_t frame_size = 0, vbr_enabled = 0;
    spx_int32_t complexity = 3, quality = SPEEX_QUALITY_DEF;
    SPEAKER_MEDIA_ENCODER_S *p_encoder = (SPEAKER_MEDIA_ENCODER_S *)encoder;

    if (NULL == p_encoder) {
        PR_ERR("input params");
        return NULL;
    }

    if (p_encoder->param.info.rate != SPEEX_MODEID_WB_RATE) {
        PR_ERR("just support rate: %d", SPEEX_MODEID_WB_RATE);
        return NULL;
    }

    SPEEX_ENCODE_S *p_speex = (SPEEX_ENCODE_S *)calloc(1, sizeof(SPEEX_ENCODE_S));
    if (NULL == p_speex) {
        PR_ERR("malloc p_speex error");
        return NULL;
    }

    p_speex->p_head = calloc(1, sizeof(*p_speex->p_head));
    if (NULL == p_speex->p_head) {
        PR_ERR("malloc p_speex->p_head error");
        free(p_speex);
        return NULL;
    }

    if (SPEEX_ENCODE_BUFFER_LEN) {
        p_encoder->p_buffer = (uint8_t *)calloc(sizeof(uint8_t), SPEEX_ENCODE_BUFFER_LEN);
        if (NULL == p_encoder->p_buffer) {
            PR_ERR("malloc p_buffer error");
            free(p_speex->p_head);
            free(p_speex);
            return NULL;
        }
    }

    const SpeexMode *mode = speex_lib_get_mode(SPEEX_MODEID_WB);
    p_speex->state = speex_encoder_init(mode);

    speex_encoder_ctl(p_speex->state, SPEEX_SET_COMPLEXITY, &complexity);
    speex_encoder_ctl(p_speex->state, SPEEX_SET_SAMPLING_RATE, &p_encoder->param.info.rate);
    speex_encoder_ctl(p_speex->state, SPEEX_SET_QUALITY, &quality);
    speex_bits_init(&p_speex->bits);

    speex_lib_ctl(SPEEX_LIB_GET_VERSION_STRING, (void *)&speex_version);
    speex_encoder_ctl(p_speex->state, SPEEX_GET_BITRATE, &bitrate);
    speex_encoder_ctl(p_speex->state, SPEEX_GET_FRAME_SIZE, &frame_size);
    speex_encoder_ctl(p_speex->state, SPEEX_GET_VBR, &vbr_enabled);

    /** FIXME: filling some data from external */
    p_encoder->p_start_data = (uint8_t *)p_speex->p_head;
    p_encoder->start_data_len = sizeof(*p_speex->p_head);

#if defined(ENABLE_VOICE_PROTOCOL_STREAM_GW)
    strcpy(p_speex->p_head->ver_string, speex_version);
    p_speex->p_head->ver_id = 1;
    p_speex->p_head->mode = SPEEX_MODEID_WB;
    p_speex->p_head->mode_bit_stream_ver = mode->bitstream_version;
    p_speex->p_head->rate = p_encoder->param.info.rate;
    p_speex->p_head->channels = p_encoder->param.info.channels;
    p_speex->p_head->bit_rate = bitrate;
    p_speex->p_head->frame_size = frame_size;
    p_speex->p_head->vbr = vbr_enabled;
    p_speex->p_head->encode_frame_size = MODE_1_QUALITY_5_FRAME_SIZE;
#else
    strcpy(p_speex->p_head->ver_string, speex_version);
    p_speex->p_head->ver_id = 1;
    p_speex->p_head->mode = SPEEX_MODEID_WB;
    p_speex->p_head->mode_bit_stream_ver = mode->bitstream_version;
    p_speex->p_head->rate = UNI_HTONL(p_encoder->param.info.rate);
    p_speex->p_head->channels = p_encoder->param.info.channels;
    p_speex->p_head->bit_rate = UNI_HTONL(bitrate);
    p_speex->p_head->frame_size = UNI_HTONL(frame_size);
    p_speex->p_head->vbr = vbr_enabled;
    p_speex->p_head->encode_frame_size = MODE_1_QUALITY_5_FRAME_SIZE;
#endif /** ENABLE_VOICE_PROTOCOL_STREAM_GW */

    PR_DEBUG("speex encode init successful. sample rate:%d bitrate:%d frame_size:%d encode_frame_size:%d",
             p_encoder->param.info.rate, bitrate, frame_size, MODE_1_QUALITY_5_FRAME_SIZE);

    return (void *)p_speex;
}

static OPERATE_RET __speex_encoder_deinit(SPEAKER_MEDIA_ENCODER_S *encoder)
{
    return speex_encode_free(encoder);
}

static uint32_t __speex_encoder_encode(SPEAKER_MEDIA_ENCODER_S *encoder, void *private_data, const uint8_t *buffer,
                                       uint32_t size)
{
    return speex_data_encode(encoder, private_data, buffer, size);
}

static OPERATE_RET __speex_encoder_free(SPEAKER_MEDIA_ENCODER_S *encoder)
{
    return speex_encode_free(encoder);
}

static OPERATE_RET __speex_encoder_data_callback(SPEAKER_MEDIA_ENCODER_S *encoder, void *private_data,
                                                 const uint8_t *buffer, uint32_t size)
{
    /** FIXME: don't worry, impl inside! */
    return OPRT_OK;
}

SPEAKER_MEDIA_ENCODER_S global_tuya_speex_encoder = {
    .handle = &global_tuya_speex_encoder,
    .name = "global_tuya_speex_encoder",

    .encode_buffer_max = SPEEX_ENCODE_BUFFER_LEN,
    .param =
        {
            .encode_type = TUYA_VOICE_AUDIO_FORMAT_SPEEX,
            .info =
                {
                    .channels = 1,
                    .rate = 16000,
                    .bits_per_sample = 16,
                },
        },

    .encoder_init = __speex_encoder_init,
    .encoder_deinit = __speex_encoder_deinit,
    .encoder_encode = __speex_encoder_encode,
    .encoder_free = __speex_encoder_free,
    .encoder_data_callback = __speex_encoder_data_callback,
};
