#include "tuya_ai_encoder_opus.h"
#include "tuya_ai_encoder_opus_ipc_server.h"
#include "tuya_error_code.h"
#include "tal_system.h"

STATIC TUYA_AI_ENCODER_T *enc = &g_tuya_ai_encoder_opus;

STATIC OPERATE_RET __output_cb(AI_AUDIO_CODEC_TYPE codec_type, UCHAR_T *data, UINT_T len, void *usr_data)
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
    IPC_AUDIO_ENC_PARA_T *enc_para = (IPC_AUDIO_ENC_PARA_T *)para;
    if (enc_para == NULL || enc_para->type != IPC_AUDIO_ENC_TYPE_OPUS) {
        return OPRT_INVALID_PARM; // Invalid parameters
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
        rt = enc->encode(enc_para->handle, (UCHAR_T *)enc_para->buf, enc_para->buf_len, __output_cb, enc_para);
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
