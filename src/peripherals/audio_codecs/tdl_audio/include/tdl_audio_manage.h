/**
 * @file tdl_audio_manage.h
 * @brief Tuya Driver Layer audio management interface.
 *
 * This file provides the high-level audio management interface for the Tuya
 * Driver Layer (TDL). It defines functions for discovering, opening, controlling,
 * and managing audio devices within the TDL framework. This interface allows
 * applications to interact with registered audio drivers in a unified manner,
 * regardless of the underlying hardware implementation.
 *
 * Key functionalities:
 * - Audio device discovery by name
 * - Opening audio devices with microphone callback support
 * - Audio playback control
 * - Volume adjustment
 * - Device lifecycle management
 *
 * The TDL audio management layer provides a simplified API that abstracts
 * the complexity of individual audio drivers while maintaining flexibility
 * for various audio use cases in Tuya IoT devices.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_AUDIO_MANAGE_H__
#define __TDL_AUDIO_MANAGE_H__

#include "tuya_cloud_types.h"

#include "tdl_audio_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef void *TDL_AUDIO_HANDLE_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint16_t sample_rate;
    uint16_t sample_ch_num;
    uint16_t sample_bits;
    uint16_t sample_tm_ms; 
    uint16_t frame_size;
}TDL_AUDIO_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tdl_audio_find(char *name, TDL_AUDIO_HANDLE_T *handle);

OPERATE_RET tdl_audio_open(TDL_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb);

OPERATE_RET tdl_audio_get_info(TDL_AUDIO_HANDLE_T handle, TDL_AUDIO_INFO_T *info);

OPERATE_RET tdl_audio_play(TDL_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len);

OPERATE_RET tdl_audio_play_stop(TDL_AUDIO_HANDLE_T handle);

OPERATE_RET tdl_audio_volume_set(TDL_AUDIO_HANDLE_T handle, uint8_t volume);

OPERATE_RET tdl_audio_close(TDL_AUDIO_HANDLE_T handle);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_AUDIO_MANAGE_H__ */
