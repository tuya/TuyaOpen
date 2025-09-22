/**
 * @file tdd_audio_no_codec.h
 * @brief tdd_audio_no_codec module is used to
 * @version 0.1
 * @date 2025-04-08
 */

#ifndef __TDD_AUDIO_NO_CODEC_H__
#define __TDD_AUDIO_NO_CODEC_H__

#include "tuya_cloud_types.h"

#include "tdl_audio_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef struct {
    uint8_t i2s_id;
    uint32_t mic_sample_rate;
    uint32_t spk_sample_rate;
} TDD_AUDIO_NO_CODEC_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tdd_audio_no_codec_register(char *name, TDD_AUDIO_NO_CODEC_T cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_AUDIO_NO_CODEC_H__ */
