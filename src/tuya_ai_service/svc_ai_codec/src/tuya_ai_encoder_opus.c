#include "tuya_ai_encoder_opus.h"
#include "tuya_error_code.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_system.h"
#include "opus/opus.h"

#define ENC_PR_D PR_DEBUG

#define OS_Malloc(req_size) tal_malloc(req_size)
#define OS_Calloc(req_count, req_size) tal_calloc(req_count, req_size)
#define OS_Free(ptr) tal_free(ptr)

#define OPUS_ENCODE_MAX_PACKET     (1500)
#define OPUS_ENCODE_BYTES_MAX      (1500)

// Opus encoder context
typedef struct {
    OpusEncoder *codec;             // Opus encoder handle
    uint32_t frame_size;              // Frame size in samples
    uint32_t buf_offset;              // PCM data input buffer offset
    uint8_t *in_buf;                 // Input buffer
    uint32_t in_buf_size;             // Input buffer size
    uint8_t *out_buf;                // Encoded output data buffer
    uint32_t out_buf_size;            // Encoded output data buffer size
} TUYA_AI_OPUS_CONTEXT_T;

STATIC OPERATE_RET _encoder_opus_create(AI_ENCODE_HANDLE_T *handle, TUYA_AI_ENCODER_INFO_T *info)
{
    OPERATE_RET rt = OPRT_OK;
    const int application = OPUS_APPLICATION_VOIP;
    opus_int32 sample_rate = (opus_int32)info->sample_rate;
    int variable_duration = OPUS_FRAMESIZE_ARG;
    int channels = info->channels;
    if (channels != 1) {
        ENC_PR_D("current opus supports only 1 channel.");
        return OPRT_INVALID_PARM;
    }
    if (sample_rate != 8000 && sample_rate != 12000
        && sample_rate != 16000 && sample_rate != 24000
        && sample_rate != 48000) {
        ENC_PR_D("opus supported sample rates are 8000,12000,16000,24000 and 48000.");
        return OPRT_INVALID_PARM;
    }
    // Default frame size is 20ms, can be 10, 20, 40 or 60ms
    if (info->frame_size == 0) {
        info->frame_size = (UINT16_T)(sample_rate / 1000 * 40); // 40ms
    }
    uint32_t frame_size_ms = info->frame_size / (sample_rate / 1000);
    if (frame_size_ms == 10) {
        variable_duration = OPUS_FRAMESIZE_10_MS;
    } else if (frame_size_ms == 20) {
        variable_duration = OPUS_FRAMESIZE_20_MS;
    } else if (frame_size_ms == 40) {
        variable_duration = OPUS_FRAMESIZE_40_MS;
    } else if (frame_size_ms == 60) {
        variable_duration = OPUS_FRAMESIZE_60_MS;
    } else {
        ENC_PR_D("opus supported frame size is 10,20,40 and 60 ms.");
        return OPRT_INVALID_PARM;
    }
    int frame_size = sample_rate / 1000 * frame_size_ms;

    TUYA_AI_OPUS_CONTEXT_T *opus = (TUYA_AI_OPUS_CONTEXT_T *)OS_Malloc(sizeof(TUYA_AI_OPUS_CONTEXT_T));
    if (NULL == opus) {
        ENC_PR_D("malloc opus failed.");
        rt = OPRT_MALLOC_FAILED;
        goto FAILURE_EXIT;
    }
    memset(opus, 0, sizeof(TUYA_AI_OPUS_CONTEXT_T));
    int err = 0;
    OpusEncoder *enc = opus_encoder_create(sample_rate, channels, application, &err);
    if (err != OPUS_OK) {
        ENC_PR_D("can't create encoder: %s", opus_strerror(err));
        rt = OPRT_COM_ERROR;
        goto FAILURE_EXIT;
    }
    opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(enc, OPUS_SET_BITRATE(16000));
    opus_encoder_ctl(enc, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_MEDIUMBAND));
    opus_encoder_ctl(enc, OPUS_SET_VBR(0));
    opus_encoder_ctl(enc, OPUS_SET_VBR_CONSTRAINT(0));
    opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(0));
    opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(0));
    opus_encoder_ctl(enc, OPUS_SET_FORCE_CHANNELS(1));
    opus_encoder_ctl(enc, OPUS_SET_DTX(0));
    opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(0));
    opus_encoder_ctl(enc, OPUS_SET_LSB_DEPTH(16));
    opus_encoder_ctl(enc, OPUS_SET_PREDICTION_DISABLED(1));
    opus_encoder_ctl(enc, OPUS_SET_EXPERT_FRAME_DURATION(variable_duration));

    opus->codec = enc;
    opus->frame_size = frame_size;
    opus->in_buf_size = frame_size * channels * sizeof(opus_int16);
    opus->in_buf = (uint8_t *)OS_Malloc(opus->in_buf_size);
    if (NULL == opus->in_buf) {
        rt = OPRT_MALLOC_FAILED;
        goto FAILURE_EXIT;
    }
    opus->out_buf_size = OPUS_ENCODE_MAX_PACKET;
    opus->out_buf = (uint8_t *)OS_Malloc(opus->out_buf_size);
    if (NULL == opus->out_buf) {
        rt = OPRT_MALLOC_FAILED;
        goto FAILURE_EXIT;
    }
    *handle = (AI_ENCODE_HANDLE_T)opus;

    return OPRT_OK;
FAILURE_EXIT:
    if (opus && opus->in_buf) {
        OS_Free(opus->in_buf);
        opus->in_buf = NULL;
    }
    if (opus && opus->out_buf) {
        OS_Free(opus->out_buf);
        opus->out_buf = NULL;
    }
    if (opus && opus->codec) {
        opus_encoder_destroy(opus->codec);
        opus->codec = NULL;
    }
    OS_Free(opus);
    return rt;
}

STATIC OPERATE_RET _encoder_opus_destroy(AI_ENCODE_HANDLE_T handle)
{
    TUYA_AI_OPUS_CONTEXT_T *opus = (TUYA_AI_OPUS_CONTEXT_T *)handle;
    if (opus == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (opus->in_buf) {
        OS_Free(opus->in_buf);
        opus->in_buf = NULL;
    }
    if (opus->out_buf) {
        OS_Free(opus->out_buf);
        opus->out_buf = NULL;
    }
    if (opus->codec) {
        opus_encoder_destroy(opus->codec);
        opus->codec = NULL;
    }
    OS_Free(opus);
    return OPRT_OK;
}

STATIC OPERATE_RET _encoder_opus_encode(AI_ENCODE_HANDLE_T handle, uint8_t *in_buf, uint32_t in_len, AI_ENCODER_DATA_OUT_CB cb, void *usr_data)
{
    TUYA_AI_OPUS_CONTEXT_T *opus = (TUYA_AI_OPUS_CONTEXT_T *)handle;

    if (opus == NULL || opus->codec == NULL || in_buf == NULL || cb == NULL) {
        return OPRT_INVALID_PARM;
    }
    OpusEncoder *enc = opus->codec;
    uint32_t frame_size = opus->frame_size;

    while (in_len > 0) {
        uint32_t copy_size = opus->buf_offset + in_len > opus->in_buf_size ? opus->in_buf_size - opus->buf_offset : in_len;
        memcpy(opus->in_buf + opus->buf_offset, in_buf, copy_size);
        opus->buf_offset += copy_size;
        if (opus->buf_offset < opus->in_buf_size) {
            return OPRT_OK;
        }

        in_buf += copy_size;
        in_len -= copy_size;

        opus_int32 out_len = OPUS_ENCODE_BYTES_MAX;
        opus_int16 *input = (opus_int16 *)opus->in_buf;
#define ENCODER_TIMESTAMP_PR 0
#if ENCODER_TIMESTAMP_PR
        SYS_TIME_T start = tal_system_get_millisecond();
#endif
        opus_int32 len = opus_encode(enc, input, frame_size, opus->out_buf, out_len);
#if ENCODER_TIMESTAMP_PR
        SYS_TIME_T end = tal_system_get_millisecond();
        SYS_TIME_T delta = end - start;
        ENC_PR_D("_encoder_opus_encode: start: %llu, end: %llu, delta: %llu(ms), out_bytes=%d", start, end, delta, len);
#endif
        if (len < 0) {
            return OPRT_COM_ERROR;
        }
        cb(AUDIO_CODEC_OPUS, opus->out_buf, len, usr_data);
        opus->buf_offset = 0;
    }
    return OPRT_OK;
}

// Opus encoder
TUYA_AI_ENCODER_T g_tuya_ai_encoder_opus = {
    .handle = NULL,
    .name = "opus",
    .codec_type = AUDIO_CODEC_OPUS,
    .create = _encoder_opus_create,
    .destroy = _encoder_opus_destroy,
    .encode = _encoder_opus_encode,
};
