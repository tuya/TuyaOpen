#include "tuya_ai_encoder_speex.h"
#include "tuya_error_code.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_system.h"
#include "speex/speex.h"

#define ENC_PR_D TAL_PR_DEBUG
// #define ENC_PR_D(format, ... ) do { bk_printf(format, ##__VA_ARGS__); bk_printf("\r\n"); } while (0)

#define OS_Malloc(req_size) tal_malloc(req_size)
#define OS_Calloc(req_count, req_size) tal_calloc(req_count, req_size)
#define OS_Free(ptr) tal_free(ptr)

// #define SPEEX_ENCODE_MAX_PACKET     (1500)
// #define SPEEX_ENCODE_BYTES_MAX      (1500)

#define SPEEX_MODEID_WB_RATE        16000
#define SPEEX_FRAME_SIZE            (320 * 1)                 //need use SPEEX_GET_FRAME_SIZE
#define SPEEX_FRAME_BYTE            (SPEEX_FRAME_SIZE * sizeof(short))  //16 bits/sample
#define SPEEX_MAX_FRAME_BYTES       (200 * 1)
#define SPEEX_VER_STRING_LEN        16
#define SPEEX_QUALITY_DEF           5                   //Set the quality to 5(16k rate: 8-->27.8kbps 5-->16.8kbps)
#define MODE_1_QUALITY_5_FRAME_SIZE 42
#define MODE_1_QUALITY_8_FRAME_SIZE 70
#define SPEEX_ENCODE_BUFFER_LEN     (MODE_1_QUALITY_5_FRAME_SIZE * 5)

typedef VOID *SpeexEncoder;

// Speex encoder context
typedef struct {
    SpeexEncoder *codec;            // speex encoder handle
    SpeexBits bits;                 // speex encode bits
    uint32_t frame_size;              // Frame size in samples
    uint32_t buf_offset;              // PCM data input buffer offset
    uint8_t *in_buf;                 // Input buffer
    uint32_t in_buf_size;             // Input buffer size
    uint8_t *out_buf;                // Encoded output data buffer
    uint32_t out_buf_size;            // Encoded output data buffer size
} TUYA_AI_SPEEX_CONTEXT_T;

STATIC OPERATE_RET _encoder_speex_create(AI_ENCODE_HANDLE_T *handle, TUYA_AI_ENCODER_INFO_T *info)
{
    OPERATE_RET rt = OPRT_OK;
    // spx_int32_t vbr_enabled = 0;
    spx_int32_t complexity = 3, quality = SPEEX_QUALITY_DEF;
    int channels = info->channels;
    if (channels != 1) {
        ENC_PR_D("current speex supports only 1 channel.");
        return OPRT_INVALID_PARM;
    }

    TUYA_AI_SPEEX_CONTEXT_T *speex = (TUYA_AI_SPEEX_CONTEXT_T *)OS_Malloc(sizeof(TUYA_AI_SPEEX_CONTEXT_T));
    if (NULL == speex) {
        ENC_PR_D("malloc speex failed.");
        rt = OPRT_MALLOC_FAILED;
        goto FAILURE_EXIT;
    }
    memset(speex, 0, sizeof(TUYA_AI_SPEEX_CONTEXT_T));
    ENC_PR_D("info->channels: %d, info->sample_rate: %d", info->channels, info->sample_rate);

    CONST uint32_t frame_size_ms = 20;     // Default frame size is 20ms, can be 10, 20, 40 or 60ms
    int frame_size = info->sample_rate / 1000 * frame_size_ms;

    const SpeexMode *mode = speex_lib_get_mode(SPEEX_MODEID_WB);
    SpeexEncoder *enc = speex_encoder_init(mode);
    if (enc == NULL) {
        ENC_PR_D("can't create encoder.");
        rt = OPRT_COM_ERROR;
        goto FAILURE_EXIT;
    }
    SpeexBits bits;
    speex_encoder_ctl(enc, SPEEX_SET_COMPLEXITY, &complexity);
    speex_encoder_ctl(enc, SPEEX_SET_SAMPLING_RATE, &info->sample_rate);
    speex_encoder_ctl(enc, SPEEX_SET_QUALITY, &quality);
    speex_bits_init(&bits);
    // speex_lib_ctl(SPEEX_LIB_GET_VERSION_STRING, (void *)&speex_version);
    // speex_encoder_ctl(enc, SPEEX_GET_BITRATE, &bitrate);
    // speex_encoder_ctl(enc, SPEEX_GET_FRAME_SIZE, &frame_size);
    // speex_encoder_ctl(enc, SPEEX_GET_VBR, &vbr_enabled);

    // /** FIXME: filling some data from external */
    // p_encoder->p_start_data     = (uint8_t *)p_speex->p_head;
    // p_encoder->start_data_len   = sizeof(*p_speex->p_head);

    speex->codec = enc;
    speex->bits = bits;
    speex->frame_size = frame_size;
    speex->in_buf_size = frame_size * channels * sizeof(INT16_T);
    speex->in_buf = (uint8_t *)OS_Malloc(speex->in_buf_size);
    if (NULL == speex->in_buf) {
        rt = OPRT_MALLOC_FAILED;
        goto FAILURE_EXIT;
    }
    speex->out_buf_size = SPEEX_MAX_FRAME_BYTES;
    speex->out_buf = (uint8_t *)OS_Malloc(speex->out_buf_size);
    if (NULL == speex->out_buf) {
        rt = OPRT_MALLOC_FAILED;
        goto FAILURE_EXIT;
    }
    *handle = (AI_ENCODE_HANDLE_T)speex;

    return OPRT_OK;
FAILURE_EXIT:
    if (speex && speex->in_buf) {
        OS_Free(speex->in_buf);
        speex->in_buf = NULL;
    }
    if (speex && speex->out_buf) {
        OS_Free(speex->out_buf);
        speex->out_buf = NULL;
    }
    if (speex) {
        if (speex->codec) {
            speex_bits_destroy(&speex->bits);
            speex_encoder_destroy(speex->codec);
            speex->codec = NULL;
        }
    }
    OS_Free(speex);
    return rt;
}

STATIC OPERATE_RET _encoder_speex_destroy(AI_ENCODE_HANDLE_T handle)
{
    TUYA_AI_SPEEX_CONTEXT_T *speex = (TUYA_AI_SPEEX_CONTEXT_T *)handle;
    if (speex == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (speex->in_buf) {
        OS_Free(speex->in_buf);
        speex->in_buf = NULL;
    }
    if (speex->out_buf) {
        OS_Free(speex->out_buf);
        speex->out_buf = NULL;
    }
    if (speex->codec) {
        speex_encoder_destroy(speex->codec);
        speex->codec = NULL;
    }
    OS_Free(speex);
    return OPRT_OK;
}

STATIC OPERATE_RET _encoder_speex_encode(AI_ENCODE_HANDLE_T handle, uint8_t *in_buf, uint32_t in_len, AI_ENCODER_DATA_OUT_CB cb, void *usr_data)
{
    TUYA_AI_SPEEX_CONTEXT_T *speex = (TUYA_AI_SPEEX_CONTEXT_T *)handle;

    if (speex == NULL || speex->codec == NULL || in_buf == NULL || cb == NULL) {
        return OPRT_INVALID_PARM;
    }
    int out_bytes = 0;

    while (in_len > 0) {
        uint32_t copy_size = speex->buf_offset + in_len > speex->in_buf_size ? speex->in_buf_size - speex->buf_offset : in_len;
        memcpy(speex->in_buf + speex->buf_offset, in_buf, copy_size);
        speex->buf_offset += copy_size;
        if (speex->buf_offset < speex->in_buf_size) {
            return OPRT_OK;
        }

        in_buf += copy_size;
        in_len -= copy_size;

        spx_int16_t *input = (spx_int16_t *)speex->in_buf;
        speex_bits_reset(&speex->bits);
#define ENCODER_TIMESTAMP_PR 0
#if ENCODER_TIMESTAMP_PR
        SYS_TIME_T start = tal_system_get_millisecond();
#endif
        int ret = speex_encode_int(speex->codec, input, &speex->bits);
#if ENCODER_TIMESTAMP_PR
        SYS_TIME_T end = tal_system_get_millisecond();
        SYS_TIME_T delta = end - start;
#endif
        if (ret != 1) {
            return OPRT_COM_ERROR;
        }
        out_bytes = speex_bits_write(&speex->bits, (char *)speex->out_buf, SPEEX_MAX_FRAME_BYTES);
#if ENCODER_TIMESTAMP_PR
        ENC_PR_D("_encoder_speex_encode: start: %llu, end: %llu, delta: %llu(ms), out_bytes=%d", start, end, delta, out_bytes);
#endif
        cb(AUDIO_CODEC_SPEEX, speex->out_buf, out_bytes, usr_data);
        speex->buf_offset = 0;
    }
    return OPRT_OK;
}

// Speex encoder
TUYA_AI_ENCODER_T g_tuya_ai_encoder_speex = {
    .handle = NULL,
    .name = "speex",
    .codec_type = AUDIO_CODEC_SPEEX,
    .create = _encoder_speex_create,
    .destroy = _encoder_speex_destroy,
    .encode = _encoder_speex_encode,
};
