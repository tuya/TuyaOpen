#ifndef __TUYA_AI_ENCODER_OPUS_IPC_CLIENT_H__
#define __TUYA_AI_ENCODER_OPUS_IPC_CLIENT_H__

#include "tuya_ai_types.h"

#include "tuya_cloud_types.h"
#include "tuya_ai_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IPC_AUDIO_ENC_OPS_NONE = 0,          // No operation
    IPC_AUDIO_ENC_OPS_CREATE,            // Create encoder
    IPC_AUDIO_ENC_OPS_ENCODE,            // Encode operation
    IPC_AUDIO_ENC_OPS_DESTROY,           // Destroy encoder
} IPC_AUDIO_ENC_OPS_E;

typedef enum {
    IPC_AUDIO_ENC_TYPE_UNKNOWN = 0,    // Unknown type
    IPC_AUDIO_ENC_TYPE_OPUS,
    IPC_AUDIO_ENC_TYPE_SPEEX,
} IPC_AUDIO_ENC_TYPE_E;

typedef struct {
    IPC_AUDIO_ENC_OPS_E ops;
    IPC_AUDIO_ENC_TYPE_E type;          // Encoder type
    TUYA_AI_ENCODER_INFO_T info;        // Encoder configuration
    AI_ENCODE_HANDLE_T handle;          // Encoder handle
    INT16_T *buf;                       // Input buffer
    uint32_t buf_len;                   // Input buffer length
    INT8_T  *out_buf;                   // Output buffer
    uint32_t out_buf_len;               // Output buffer length
    uint32_t out_len;                   // Output data length
} IPC_AUDIO_ENC_PARA_T;

OPERATE_RET tuya_ai_encoder_opus_server_handle(VOID *para);

#ifdef __cplusplus
}
#endif

#endif // __TUYA_AI_ENCODER_OPUS_IPC_CLIENT_H__
