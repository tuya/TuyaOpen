#ifndef __TUYA_AI_ENCODER_H__
#define __TUYA_AI_ENCODER_H__

#include "tuya_ai_types.h"

#include "tuya_cloud_types.h"
#include "tuya_ai_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// Encoder information structure
typedef struct {
    AI_AUDIO_CODEC_TYPE encode_type;
    uint32_t sample_rate;
    uint8_t channels;
    uint32_t bits_per_sample;
    uint16_t frame_size;
} TUYA_AI_ENCODER_INFO_T;

// Encoder handle type
typedef VOID *AI_ENCODE_HANDLE_T;

// Encoder data output callback function type
typedef OPERATE_RET (*AI_ENCODER_DATA_OUT_CB)(AI_AUDIO_CODEC_TYPE codec_type, uint8_t *data, uint32_t len, void *usr_data);

// Encoder interface structure
typedef struct {
    AI_ENCODE_HANDLE_T handle;
    char *name;
    AI_AUDIO_CODEC_TYPE codec_type;
    OPERATE_RET (*create)(AI_ENCODE_HANDLE_T *handle, TUYA_AI_ENCODER_INFO_T *info);
    OPERATE_RET (*destroy)(AI_ENCODE_HANDLE_T handle);
    OPERATE_RET (*encode)(AI_ENCODE_HANDLE_T handle, uint8_t *in_buf, uint32_t in_len, AI_ENCODER_DATA_OUT_CB cb, void *usr_data);
} TUYA_AI_ENCODER_T;

/**
 * @brief Register an audio encoder
 *
 * @param encoder Pointer to the encoder structure
 *
 * @return OPRT_OK on success, other error code on failure
 */
OPERATE_RET tuya_ai_register_encoder(TUYA_AI_ENCODER_T *encoder);

/**
 * @brief Unregister an audio encoder
 *
 * @param encoder Pointer to the encoder structure
 *
 * @return None
 */
OPERATE_RET tuya_ai_unregister_encoder(TUYA_AI_ENCODER_T *encoder);

/**
 * @brief Get an audio encoder by codec type
 *
 * @param codec_type The codec type
 *
 * @return Pointer to the encoder structure, or NULL if not found
 */
TUYA_AI_ENCODER_T *tuya_ai_get_encoder(AI_AUDIO_CODEC_TYPE codec_type);

#ifdef __cplusplus
}
#endif

#endif // __TUYA_AI_ENCODER_H__
