#include "tuya_ai_encoder_opus.h"
#include "tuya_ai_encoder_opus_ipc_server.h"
#include "tuya_error_code.h"
#include "tal_system.h"

#if defined(ENABLE_TUYA_CODEC_OPUS) && (ENABLE_TUYA_CODEC_OPUS == 1)
#include "tuya_ai_encoder_opus.h"
STATIC TUYA_AI_ENCODER_T *enc_opus = &g_tuya_ai_encoder_opus;
#else
STATIC TUYA_AI_ENCODER_T *enc_opus = NULL;
#endif
#if defined(ENABLE_TUYA_CODEC_SPEEX) && (ENABLE_TUYA_CODEC_SPEEX == 1)
#include "tuya_ai_encoder_speex.h"
STATIC TUYA_AI_ENCODER_T *enc_speex = &g_tuya_ai_encoder_speex;
#else
STATIC TUYA_AI_ENCODER_T *enc_speex = NULL;
#endif

STATIC OPERATE_RET __output_cb(AI_AUDIO_CODEC_TYPE codec_type, uint8_t *data, uint32_t len, void *usr_data)
{
    if (data == NULL || len == 0 || usr_data == NULL) {
        return OPRT_INVALID_PARM; // Invalid parameters
    }

    IPC_AUDIO_ENC_PARA_T *enc_para = (IPC_AUDIO_ENC_PARA_T *)usr_data;
    if (enc_para->out_buf == NULL || enc_para->out_buf_len < len) {
        return OPRT_INVALID_PARM; // Output buffer is not sufficient
    }

    // Copy encoded data to output buffer
    memcpy(enc_para->out_buf + enc_para->out_len, data, len);
    enc_para->out_len += len; // Update the length of the output buffer

    return OPRT_OK; // Return success
}

OPERATE_RET tuya_ai_encoder_opus_server_handle(VOID *para)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_AI_ENCODER_T *enc = NULL;
    IPC_AUDIO_ENC_PARA_T *enc_para = (IPC_AUDIO_ENC_PARA_T *)para;
    if (enc_para == NULL) {
        return OPRT_INVALID_PARM; // Invalid parameters
    }

    if (enc_para->type == IPC_AUDIO_ENC_TYPE_OPUS) {
        enc = enc_opus;
    } else if (enc_para->type == IPC_AUDIO_ENC_TYPE_SPEEX) {
        enc = enc_speex;
    } else {
        return OPRT_INVALID_PARM; // Unsupported encoder type
    }
    if (enc == NULL) {
        return OPRT_COM_ERROR; // Encoder not registered
    }

    if (IPC_AUDIO_ENC_OPS_CREATE == enc_para->ops) {
        // Create Opus encoder
        rt = enc->create(&enc_para->handle, &enc_para->info);
        if (rt != OPRT_OK) {
            return rt; // Return error if creation failed
        }
        if (enc_para->handle == NULL) {
            return OPRT_COM_ERROR; // Handle should not be NULL after creation
        }
        return OPRT_OK; // Return success after creation
    } else if (IPC_AUDIO_ENC_OPS_ENCODE == enc_para->ops) {
        if (enc_para->handle == NULL || enc_para->buf == NULL || enc_para->buf_len == 0) {
            return OPRT_INVALID_PARM; // Invalid parameters
        }
        // Encode audio data
        rt = enc->encode(enc_para->handle, (uint8_t *)enc_para->buf, enc_para->buf_len, __output_cb, enc_para);
        return rt; // Return the result of encoding operation
    } else if (IPC_AUDIO_ENC_OPS_DESTROY == enc_para->ops) {
        // Destroy Opus encoder
        if (enc_para->handle != NULL) {
            return enc->destroy(enc_para->handle);
        }
        return OPRT_OK; // If handle is NULL, nothing to destroy
    } else {
        return OPRT_INVALID_PARM;
    }
}
