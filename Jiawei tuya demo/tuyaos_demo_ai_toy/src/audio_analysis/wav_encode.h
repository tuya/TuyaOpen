/**
 * @file wav_encode.h
 * @brief wav_encode module is used to
 * @version 0.1
 * @date 2025-02-21
 */

#ifndef __WAV_ENCODE_H__
#define __WAV_ENCODE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define WAV_HEAD_LEN 44

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Generates the WAV file header.
 *
 * This function creates the header for a WAV file based on the provided parameters.
 *
 * @param pcm_len The length of the PCM data in bytes.
 * @param cd_format The codec format of the WAV file, 1 for PCM.
 * @param sample_rate The sample rate of the audio data.
 * @param bit_depth The bit depth of the audio data.
 * @param channel The number of audio channels.
 * @param head Pointer to the buffer where the generated WAV header will be stored.
 *
 * @return OPERATE_RET Returns the operation result.
 */
OPERATE_RET app_get_wav_head(uint32_t pcm_len, uint8_t cd_format, uint32_t sample_rate, uint16_t bit_depth,
                             uint16_t channel, uint8_t head[]);

#ifdef __cplusplus
}
#endif

#endif /* __WAV_ENCODE_H__ */
