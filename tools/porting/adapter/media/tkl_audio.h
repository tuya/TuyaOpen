/**
 * @file tkl_audio.h
 * @brief Common process - adapter the audio api
 * @version 0.1
 * @date 2021-11-04
 *
 * @copyright Copyright 2019-2021 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TKL_AUDIO_H__
#define __TKL_AUDIO_H__

#include "tuya_cloud_types.h"
#include "tkl_media.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TKL_AUDIO_DATABITS_8 = 8,
    TKL_AUDIO_DATABITS_16 = 16,
    TKL_AUDIO_DATABITS_MAX = 0xFF
} TKL_AUDIO_DATABITS_E;

typedef enum {
    TKL_AUDIO_CHANNEL_MONO = 1,
    TKL_AUDIO_CHANNEL_STEREO,
    TKL_AUDIO_CHANNEL_TDM3CHS,
    TKL_AUDIO_CHANNEL_TDM4CHS,
    TKL_AUDIO_CHANNEL_TDM5CHS,
    TKL_AUDIO_CHANNEL_TDM6CHS,
    TKL_AUDIO_CHANNEL_TDM7CHS,
    TKL_AUDIO_CHANNEL_TDM8CHS
} TKL_AUDIO_CHANNEL_E;

typedef enum {
    TKL_AUDIO_SAMPLE_8K = 8000,
    TKL_AUDIO_SAMPLE_11K = 11000,
    TKL_AUDIO_SAMPLE_12K = 12000,
    TKL_AUDIO_SAMPLE_16K = 16000,
    TKL_AUDIO_SAMPLE_22K = 22000,
    TKL_AUDIO_SAMPLE_24K = 24000,
    TKL_AUDIO_SAMPLE_32K = 32000,
    TKL_AUDIO_SAMPLE_44K = 44000,
    TKL_AUDIO_SAMPLE_48K = 48000,
    TKL_AUDIO_SAMPLE_MAX = 0xFFFFFFFF
} TKL_AUDIO_SAMPLE_E;

typedef enum {
    TKL_AI_0 = 0,
    TKL_AI_1,
    TKL_AI_2,
    TKL_AI_3,
    TKL_AI_MAX,
} TKL_AI_CHN_E; // audio input channel

typedef enum {
    TKL_AO_0 = 0,
    TKL_AO_1,
    TKL_AO_2,
    TKL_AO_3,
    TKL_AO_MAX,
} TKL_AO_CHN_E; // audio output channel

typedef struct {
    uint8_t platform_dai_type;         // 0--IIS类型的AUDIO 1--DAC类型的AUDIO
    uint8_t platform_dai_port;         // 选择哪个DAC ADC 或者 IIS
    uint8_t platform_dai_left_subport; // platform_dai_type == 1时，选择哪个DAC,ADC Chanel 作为左声道 0xff无效
    uint8_t platform_dai_right_subport; // platform_dai_type == 1时   选择哪个DAC,ADC Chanel 作为右声道 0xff无效
    uint8_t codec_i2c;                  // platform_dai_type == 0时 codec使用哪个I2C 0xff无效
    uint8_t codec_i2c_addr;             // platform_dai_type == 0时 codec的I2C地址       0xff无效
} TKL_AUDIO_HARDWARE_SOURCE;

typedef struct {
    TKL_MEDIA_FRAME_TYPE_E type;      // frame type
    char *pbuf;                       // buffer
    uint32_t buf_size;                // buffer size
    uint32_t used_size;               // used buffer
    uint64_t pts;                     // sdk pts
    uint64_t timestamp;               // system utc time，unit: ms
    TKL_MEDIA_CODEC_TYPE_E codectype; // codec type
    TKL_AUDIO_SAMPLE_E sample;        // sample
    TKL_AUDIO_DATABITS_E datebits;    // date bit
    TKL_AUDIO_CHANNEL_E channel;      // channel num
    uint32_t seq;                     // frame sequence number
} TKL_AUDIO_FRAME_INFO_T;             // audio frame

typedef int (*TKL_FRAME_PUT_CB)(TKL_AUDIO_FRAME_INFO_T *pframe);

typedef struct {
    uint32_t enable;                  // 1,enable,0,disable
    uint32_t card;                    // audio card num
    TKL_AI_CHN_E ai_chn;              // audio input channel
    TKL_AUDIO_SAMPLE_E sample;        // sample
    TKL_AUDIO_DATABITS_E datebits;    // datebit
    TKL_AUDIO_CHANNEL_E channel;      // channel num
    TKL_MEDIA_CODEC_TYPE_E codectype; // codec type
    int32_t is_softcodec;             // 1, soft encode，0, hardware encode
    uint32_t fps;                     // frame per second，suggest 25
    int32_t mic_volume;               // mic volume,[0,100]
    int32_t spk_volume;               // spk volume,[0,100]
    int32_t spk_volume_offset; // spk volume offset, for adapting different speakers,The default value is 0,[0,100]
    int32_t spk_gpio;          // spk amplifier pin number, <0, no amplifier
    int32_t spk_gpio_polarity; // pin polarity, 0 high enable, 1 low enable
    void *padta;
    TKL_FRAME_PUT_CB put_cb;
} TKL_AUDIO_CONFIG_T; // audio config

typedef struct {
    int32_t pcm_db;             // DB value
} TKL_AUDIO_DETECT_DB_RESULT_T; // DB result

typedef struct {
    int32_t val;
    char res[8];
} TKL_AUDIO_DETECT_RESULT_T; // DETECT result

typedef enum {
    TKL_AUDIO_VQE_AEC = 0,
    TKL_AUDIO_VQE_NR,
    TKL_AUDIO_VQE_HPF,
    TKL_AUDIO_VQE_AGC,
    TKL_AUDIO_VQE_ALC,
    TKL_AUDIO_VQE_HS,
    TKL_AUDIO_VQE_MAX,
} TKL_AUDIO_VQE_TYPE_E; // vqe type

typedef struct {
    uint32_t enable; // 1,enable.0,disable
} TKL_AUDIO_VQE_PARAM_T;

/**
 * @brief ai init
 *
 * @param[in] pconfig: audio config
 * @param[in] count: count of pconfig
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ai_init(TKL_AUDIO_CONFIG_T *pconfig, int32_t count);

/**
 * @brief ai start
 *
 * @param[in] card: card number
 * @param[in] chn: channel number
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ai_start(int32_t card, TKL_AI_CHN_E chn);

/**
 * @brief ai set mic volume
 *
 * @param[in] card: card number
 * @param[in] chn: channel number
 * @param[in] vol: mic volume,[0, 100]
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ai_set_vol(int32_t card, TKL_AI_CHN_E chn, int32_t vol);

/**
 * @brief ai get frame
 *
 * @param[in] card: card number
 * @param[in] chn: channel number
 * @param[out] pframe: audio frame, pframe->pbuf allocated by upper layer application
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ai_get_frame(int32_t card, TKL_AI_CHN_E chn, TKL_AUDIO_FRAME_INFO_T *pframe);

/**
 * @brief ai set vqe param
 *
 * @param[in] card: card number
 * @param[in] chn: channel number
 * @param[in] type: vqe type
 * @param[in] pparam: vqe param
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ai_set_vqe(int32_t card, TKL_AI_CHN_E chn, TKL_AUDIO_VQE_TYPE_E type, TKL_AUDIO_VQE_PARAM_T *pparam);

/**
 * @brief ai get vqe param
 *
 * @param[in] card: card number
 * @param[in] chn: channel number
 * @param[in] type: vqe type
 * @param[out] pparam: vqe param
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ai_get_vqe(int32_t card, TKL_AI_CHN_E chn, TKL_AUDIO_VQE_TYPE_E type, TKL_AUDIO_VQE_PARAM_T *pparam);

/**
 * @brief ai stop
 *
 * @param[in] card: card number
 * @param[in] chn: channel number
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ai_stop(int32_t card, TKL_AI_CHN_E chn);

/**
 * @brief ai uninit
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ai_uninit(void);

/**
 * @brief ao init
 *
 * @param[in] pconfig: audio config
 * @param[in] count: config count
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ao_init(TKL_AUDIO_CONFIG_T *pconfig, int32_t count, void **handle);

/**
 * @brief ao start
 *
 * @param[in] card: card number
 * @param[in] chn: channel number
 * @param[out] handle: handle of start
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ao_start(int32_t card, TKL_AO_CHN_E chn, void *handle);

/**
 * @brief ao set volume
 *
 * @param[in] card: card number
 * @param[in] chn: channel number
 * @param[in] vol: mic volume,[0, 100]
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ao_set_vol(int32_t card, TKL_AO_CHN_E chn, void *handle, int32_t vol);

/**
 * @brief ao get volume
 *
 * @param[in] card: card number
 * @param[in] chn: channel number
 * @param[in] vol: mic volume,[0, 100]
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ao_get_vol(int32_t card, TKL_AO_CHN_E chn, void *handle, int32_t *vol);

/**
 * @brief ao output frame
 *
 * @param[in] card: card number
 * @param[in] chn: channel number
 * @param[in] handle: handle of start
 * @param[in] pframe: output frame
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ao_put_frame(int32_t card, TKL_AO_CHN_E chn, void *handle, TKL_AUDIO_FRAME_INFO_T *pframe);

/**
 * @brief ao stop
 *
 * @param[in] card: card number
 * @param[in] chn: channel number
 * @param[in] handle: handle of start
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ao_stop(int32_t card, TKL_AO_CHN_E chn, void *handle);

/**
 * @brief ao uninit
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ao_uninit(void *handle);

/**
 * @brief audio input detect start
 *
 * @param[in] card: card number
 * @param[in] type: detect type
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ai_detect_start(int32_t card, TKL_MEDIA_DETECT_TYPE_E type);

/**
 * @brief audio input detect stop
 *
 * @param[in] card: card number
 * @param[in] type: detect type
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ai_detect_stop(int32_t card, TKL_MEDIA_DETECT_TYPE_E type);

/**
 * @brief audio detect get result
 *
 * @param[in] card: card number
 * @param[in] type: detect type
 * @param[out] presult: audio detect result
 *
 * @return OPRT_OK on success. Others on error, please refer to tkl_error_code.h
 */
OPERATE_RET tkl_ai_detect_get_result(int32_t card, TKL_MEDIA_DETECT_TYPE_E type, TKL_AUDIO_DETECT_RESULT_T *presult);

#ifdef __cplusplus
}
#endif

#endif
