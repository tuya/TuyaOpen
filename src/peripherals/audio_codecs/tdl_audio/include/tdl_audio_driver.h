/**
 * @file tdl_audio_driver.h
 * @brief Tuya Driver Layer audio driver interface definitions.
 *
 * This file defines the driver interface for the Tuya Driver Layer (TDL) audio
 * subsystem. It provides the core structures, types, and function prototypes
 * that audio device drivers must implement to integrate with the TDL audio
 * management system. The interface supports various audio formats, status
 * notifications, and control commands.
 *
 * Key features:
 * - Support for multiple audio frame formats (PCM, SPEEX, OPUS, MP3)
 * - Audio status notifications for voice activity detection (VAD)
 * - Driver registration mechanism for different audio hardware
 * - Callback-based microphone data handling
 * - Playback and configuration control interfaces
 *
 * This layer acts as an abstraction between hardware-specific implementations
 * (TDD layer) and higher-level audio applications.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_AUDIO_DRIVER_H__
#define __TDL_AUDIO_DRIVER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
// Audio driver name max length
#define TDL_AUDIO_NAME_LEN_MAX (16)

// Audio frame format
typedef uint8_t TDL_AUDIO_FRAME_FORMAT_E;
#define TDL_AUDIO_FRAME_FORMAT_PCM   0
#define TDL_AUDIO_FRAME_FORMAT_SPEEX 1
#define TDL_AUDIO_FRAME_FORMAT_OPUS  2
#define TDL_AUDIO_FRAME_FORMAT_MP3   3

// Audio sampling status
typedef uint8_t TDL_AUDIO_STATUS_E;
#define TDL_AUDIO_STATUS_UNKNOWN     0
#define TDL_AUDIO_STATUS_VAD_START   1
#define TDL_AUDIO_STATUS_VAD_END     2
#define TDL_AUDIO_STATUS_RECEIVING   3
#define TDL_AUDIO_STATUS_RECV_FINISH 4

// Audio command
typedef uint8_t TDD_AUDIO_CMD_E;
#define TDD_AUDIO_CMD_SET_VOLUME 0
#define TDD_AUDIO_CMD_PLAY_STOP  1

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void *TDD_AUDIO_HANDLE_T;

typedef void (*TDL_AUDIO_MIC_CB)(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data, uint32_t len);

typedef struct {
    OPERATE_RET (*open)(TDD_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb);
    OPERATE_RET (*play)(TDD_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len);
    OPERATE_RET (*config)(TDD_AUDIO_HANDLE_T handle, TDD_AUDIO_CMD_E cmd, void *args);
    OPERATE_RET (*close)(TDD_AUDIO_HANDLE_T handle);
} TDD_AUDIO_INTFS_T;

typedef struct {
    uint16_t sample_rate;
    uint16_t sample_ch_num;
    uint16_t sample_bits;
    uint16_t sample_tm_ms; 
} TDD_AUDIO_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tdl_audio_driver_register(char *name, TDD_AUDIO_HANDLE_T tdd_hdl,\
                                      TDD_AUDIO_INTFS_T *intfs, TDD_AUDIO_INFO_T *info);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_AUDIO_DRIVER_H__ */
