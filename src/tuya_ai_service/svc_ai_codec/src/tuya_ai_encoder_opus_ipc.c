#include "tuya_ai_encoder_opus_ipc_server.h"
#include "tuya_ai_encoder_opus_ipc.h"
#include "tuya_error_code.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_system.h"
#include "tkl_ipc.h"
#include "tkl_memory.h"

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#define OS_Malloc(req_size) tal_psram_malloc(req_size)
#define OS_Calloc(req_count, req_size) tal_psram_calloc(req_count, req_size)
#define OS_Free(ptr) tal_psram_free(ptr)
#else
#define OS_Malloc(req_size) tal_malloc(req_size)
#define OS_Calloc(req_count, req_size) tal_calloc(req_count, req_size)
#define OS_Free(ptr) tal_free(ptr)
#endif

extern OPERATE_RET tkl_audio_enc_request(VOID *enc_para);

STATIC IPC_AUDIO_ENC_PARA_T *__ipc_para_alloc(IPC_AUDIO_ENC_OPS_E ops, TUYA_AI_ENCODER_INFO_T *info, int out_buf_len)
{
    IPC_AUDIO_ENC_PARA_T *enc_para = (IPC_AUDIO_ENC_PARA_T *)OS_Malloc(sizeof(IPC_AUDIO_ENC_PARA_T));
    if (enc_para == NULL) {
        return NULL;
    }
    memset(enc_para, 0, sizeof(IPC_AUDIO_ENC_PARA_T));
    enc_para->ops = ops;
    enc_para->type = IPC_AUDIO_ENC_TYPE_OPUS;
    if (info) {
        memcpy(&enc_para->info, info, sizeof(TUYA_AI_ENCODER_INFO_T));
    }
    if (out_buf_len > 0) {
        enc_para->out_buf_len = out_buf_len;
        enc_para->out_buf = (INT8_T *)OS_Malloc(out_buf_len);
        if (enc_para->out_buf == NULL) {
            OS_Free(enc_para);
            return NULL;
        }
    }
    return enc_para;
}

STATIC VOID __ipc_para_free(IPC_AUDIO_ENC_PARA_T *enc_para)
{
    if (enc_para == NULL) {
        return;
    }
    if (enc_para->out_buf) {
        OS_Free(enc_para->out_buf);
        enc_para->out_buf = NULL;
    }
    OS_Free(enc_para);
}

STATIC OPERATE_RET _encoder_opus_ipc_create(AI_ENCODE_HANDLE_T *handle, TUYA_AI_ENCODER_INFO_T *info)
{
    OPERATE_RET rt = OPRT_OK;
    IPC_AUDIO_ENC_PARA_T *enc_para = __ipc_para_alloc(IPC_AUDIO_ENC_OPS_CREATE, info, 0);
    if (enc_para == NULL) {
        return OPRT_MALLOC_FAILED;
    }

    rt = tkl_audio_enc_request(enc_para);
    if (rt != OPRT_OK || enc_para->handle == NULL) {
        __ipc_para_free(enc_para);
        return rt;
    }
    *handle = enc_para->handle; // Get encoder handle
    __ipc_para_free(enc_para);
    return OPRT_OK;
}

STATIC OPERATE_RET _encoder_opus_ipc_destroy(AI_ENCODE_HANDLE_T handle)
{
    OPERATE_RET rt = OPRT_OK;
    IPC_AUDIO_ENC_PARA_T *enc_para = __ipc_para_alloc(IPC_AUDIO_ENC_OPS_DESTROY, NULL, 0);
    if (enc_para == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    enc_para->handle = handle; // Set the encoder handle to be destroyed
    rt = tkl_audio_enc_request(enc_para);
    if (rt != OPRT_OK) {
        __ipc_para_free(enc_para);
        return rt;
    }
    __ipc_para_free(enc_para);
    return OPRT_OK;
}

STATIC OPERATE_RET _encoder_opus_ipc_encode(AI_ENCODE_HANDLE_T handle, uint8_t *in_buf, uint32_t in_len, AI_ENCODER_DATA_OUT_CB cb, void *usr_data)
{
    OPERATE_RET rt = OPRT_OK;
    IPC_AUDIO_ENC_PARA_T *enc_para = __ipc_para_alloc(IPC_AUDIO_ENC_OPS_ENCODE, NULL, in_len / 6);
    if (enc_para == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    enc_para->handle = handle;          // Set encoder handle
    enc_para->buf = (INT16_T *)in_buf;  // Set input buffer
    enc_para->buf_len = in_len;         // Set input buffer length

#define ENCODER_TIMESTAMP_PR 0
#if ENCODER_TIMESTAMP_PR
    SYS_TIME_T start = tal_system_get_millisecond();
#endif
    rt = tkl_audio_enc_request(enc_para);
#if ENCODER_TIMESTAMP_PR
    SYS_TIME_T end = tal_system_get_millisecond();
    SYS_TIME_T delta = end - start;
    TAL_PR_DEBUG("_encoder_opus_ipc_encode: start: %llu, end: %llu, delta: %llu(ms)", start, end, delta);
#endif
    // handle the output data callback
    if (rt != OPRT_OK) {
        __ipc_para_free(enc_para);
        return rt;
    }
    if (cb && enc_para->out_buf && enc_para->out_len > 0) {
        rt = cb(AUDIO_CODEC_OPUS, (uint8_t *)enc_para->out_buf, enc_para->out_len, usr_data);
        if (rt != OPRT_OK) {
            __ipc_para_free(enc_para);
            return rt;
        }
    }
    __ipc_para_free(enc_para);
    return OPRT_OK;
}

// Opus encoder throuth ipc
TUYA_AI_ENCODER_T g_tuya_ai_encoder_opus_ipc = {
    .handle = NULL,
    .name = "opus_ipc",
    .codec_type = AUDIO_CODEC_OPUS,
    .create = _encoder_opus_ipc_create,
    .destroy = _encoder_opus_ipc_destroy,
    .encode = _encoder_opus_ipc_encode,
};
