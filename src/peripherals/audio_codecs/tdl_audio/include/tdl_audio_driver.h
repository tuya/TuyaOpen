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

/**
 * @brief Audio frame format used by the microphone callback.
 */
typedef uint8_t TDL_AUDIO_FRAME_FORMAT_E;
#define TDL_AUDIO_FRAME_FORMAT_NONE  0xFF /**< Direction is not available. */
#define TDL_AUDIO_FRAME_FORMAT_PCM   0 /**< Raw PCM samples. */
#define TDL_AUDIO_FRAME_FORMAT_SPEEX 1 /**< SPEEX encoded frame. */
#define TDL_AUDIO_FRAME_FORMAT_OPUS  2 /**< OPUS encoded frame. */
#define TDL_AUDIO_FRAME_FORMAT_MP3   3 /**< MP3 encoded frame. */

/**
 * @brief Audio capture or VAD status.
 */
typedef uint8_t TDL_AUDIO_STATUS_E;
#define TDL_AUDIO_STATUS_UNKNOWN     0 /**< Status is not available. */
#define TDL_AUDIO_STATUS_VAD_START   1 /**< Voice activity starts. */
#define TDL_AUDIO_STATUS_VAD_END     2 /**< Voice activity ends. */
#define TDL_AUDIO_STATUS_RECEIVING   3 /**< Audio payload is being received. */
#define TDL_AUDIO_STATUS_RECV_FINISH 4 /**< Capture has finished. */

/**
 * @brief Commands sent from TDL to a TDD audio driver.
 */
typedef uint8_t TDD_AUDIO_CMD_E;
#define TDD_AUDIO_CMD_SET_VOLUME 0 /**< args points to a uint8_t volume (0..100). */
#define TDD_AUDIO_CMD_PLAY_STOP  1 /**< Stop playback; args is not used. */

/***********************************************************
***********************typedef define***********************
***********************************************************/
/**
 * @brief Opaque handle owned by a TDD audio driver.
 */
typedef void *TDD_AUDIO_HANDLE_T;

/**
 * @brief Microphone data and status callback.
 *
 * The callback is invoked by the TDD driver from its capture context. The data
 * pointer is only valid during the callback and must not be retained.
 *
 * @param[in] type    Frame format of @p data.
 * @param[in] status  Capture or VAD status.
 * @param[in] data    Audio payload; may be NULL for VAD status events.
 * @param[in] len     Payload length in bytes.
 */
typedef void (*TDL_AUDIO_MIC_CB)(TDL_AUDIO_FRAME_FORMAT_E type,
                                 TDL_AUDIO_STATUS_E status,
                                 uint8_t *data, uint32_t len);

/** Audio-side events that are not PCM payloads. */
typedef uint8_t TDL_AUDIO_EVENT_E;
#define TDL_AUDIO_EVENT_WAKEUP      0U
#define TDL_AUDIO_EVENT_SLEEP       1U
#define TDL_AUDIO_EVENT_ASR_RESULT  2U

/**
 * @brief Voice module event callback.
 *
 * @param event     TDL_AUDIO_EVENT_E value.
 * @param param     Wake/semantic ID, or event-specific value.
 * @param score     ASR confidence for ASR_RESULT, otherwise 0.
 * @param user_data User pointer supplied at registration.
 */
typedef void (*TDL_AUDIO_EVENT_CB)(TDL_AUDIO_EVENT_E event, uint32_t param,
                                   uint8_t score, void *user_data);

/**
 * @brief TDD audio driver operation table.
 *
 * A TDD driver fills this table and passes it to
 * tdl_audio_driver_register(). Function pointers that are not supported must
 * be left NULL.
 */
typedef struct {
    /** Open the device and start microphone capture. */
    OPERATE_RET (*open)(TDD_AUDIO_HANDLE_T handle, TDL_AUDIO_MIC_CB mic_cb);
    /** Queue PCM or encoded playback data. */
    OPERATE_RET (*play)(TDD_AUDIO_HANDLE_T handle, uint8_t *data, uint32_t len);
    /** Apply a TDL command from TDD_AUDIO_CMD_E. */
    OPERATE_RET (*config)(TDD_AUDIO_HANDLE_T handle, TDD_AUDIO_CMD_E cmd, void *args);
    /** Stop capture and release driver resources. */
    OPERATE_RET (*close)(TDD_AUDIO_HANDLE_T handle);
    /** Register non-PCM voice events (wake/sleep/ASR result). */
    OPERATE_RET (*event_register)(TDD_AUDIO_HANDLE_T handle,
                                  TDL_AUDIO_EVENT_CB cb, void *user_data);
} TDD_AUDIO_INTFS_T;

/** @brief Audio capabilities for one direction (capture or playback). */
typedef struct {
    TDL_AUDIO_FRAME_FORMAT_E format; /**< Payload format in this direction. */
    uint16_t sample_rate;             /**< PCM sample rate or decode target rate in Hz. */
    uint16_t sample_ch_num;           /**< PCM channel count. */
    uint16_t sample_bits;             /**< PCM sample width in bits. */
    uint16_t sample_tm_ms;            /**< Nominal PCM frame duration in milliseconds. */
    uint16_t frame_size;              /**< Computed bytes per nominal PCM frame. */
    uint32_t max_payload_size;        /**< Maximum encoded or PCM payload in bytes. */
} TDL_AUDIO_STREAM_INFO_T;

/** @brief Audio capabilities advertised by a TDD driver. */
typedef struct {
    TDL_AUDIO_STREAM_INFO_T capture;  /**< Device-to-host audio path. */
    TDL_AUDIO_STREAM_INFO_T playback; /**< Host-to-device audio path. */
} TDL_AUDIO_INFO_T;

/** @brief Backward-compatible TDD name for the shared audio capability type. */
typedef TDL_AUDIO_INFO_T TDD_AUDIO_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Register a TDD audio driver with the TDL audio manager.
 *
 * This function is called by TDD drivers during board or component
 * initialization. Application code must use the tdl_audio_* API instead of
 * this function.
 *
 * @param[in] name     Unique audio device name; maximum TDL_AUDIO_NAME_LEN_MAX.
 * @param[in] tdd_hdl  Driver-private handle passed back to every TDD callback.
 * @param[in] intfs    Driver operation table.
 * @param[in] info     Advertised audio format. For PCM playback, zero-valued
 *                     rate/channel/bit-width/frame-duration fields inherit the
 *                     corresponding capture value.
 *
 * @return OPRT_OK on success, otherwise an error code.
 */
OPERATE_RET tdl_audio_driver_register(char *name, TDD_AUDIO_HANDLE_T tdd_hdl,
                                      TDD_AUDIO_INTFS_T *intfs, TDD_AUDIO_INFO_T *info);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_AUDIO_DRIVER_H__ */
