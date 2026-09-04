/**
 * @file tuya_ipc_demo.h
 * @brief Header file for Tuya IPC demo functionality
 *
 * This header file provides the interface declarations for the Tuya IPC demo
 * functionality required for video streaming applications. It includes function
 * declarations for managing demo video files, handling video frame processing,
 * and providing callback functions for media streaming. The interface supports
 * integration with the Tuya IoT platform and ensures proper handling of video
 * streaming operations. This file is essential for developers working on IoT
 * camera applications that require robust video streaming mechanisms.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_IPC_DEMO_H__
#define __TUYA_IPC_DEMO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"
#include "tuya_ipc_p2p.h"

/* ---------------------------------------------------------------------------
 * What this build carries - both from menuconfig, both off by default.
 * --------------------------------------------------------------------------- */
#if defined(CAMERA_DEMO_AUDIO) && (CAMERA_DEMO_AUDIO == 1)
#define DEMO_ENABLE_AUDIO 1
#else
#define DEMO_ENABLE_AUDIO 0
#endif

/* Recording and playback need both: the option is on, and the library it
 * selects actually got built. */
#if defined(CAMERA_DEMO_LOCAL_STORE) && (CAMERA_DEMO_LOCAL_STORE == 1) &&                                          \
    defined(ENABLE_LOCAL_STORE) && (ENABLE_LOCAL_STORE == 1)
#define DEMO_HAS_LOCAL_STORE 1
#else
#define DEMO_HAS_LOCAL_STORE 0
#endif

/**
 * @brief Initialize demo video file
 * @return none
 */
void tuya_ipc_demo_start(void);

/**
 * @brief Clean up demo resources
 * @return none
 */
void tuya_ipc_demo_end(void);

/**
 * @brief Signal disconnect callback function
 * @return 0 on success
 */
int demo_on_signal_disconnect_callback(void);

/**
 * @brief P2P live video start (align TuyaOS MEDIA_STREAM_LIVE_VIDEO_START)
 * @return 0 on success
 */
int demo_on_live_video_start_callback(void);

/**
 * @brief P2P live video stop (align TuyaOS MEDIA_STREAM_LIVE_VIDEO_STOP)
 * @return 0 on success
 */
int demo_on_live_video_stop_callback(void);

/**
 * @brief Get video frame callback function
 * @param[in,out] media_frame Media frame structure
 * @return 0 on success, -1 on failure
 */
int demo_on_get_video_frame_callback(MEDIA_FRAME *media_frame);

/**
 * @brief Get audio frame callback function
 * @param[in,out] media_frame Media frame structure
 * @return 0 on success
 */
int demo_on_get_audio_frame_callback(MEDIA_FRAME *media_frame);

/**
 * @brief P2P downlink intercom start (APP->device speaker), align MEDIA_STREAM_SPEAKER_START
 * @return 0 on success
 */
int demo_on_live_audio_start_callback(void);

/**
 * @brief P2P downlink intercom stop, align MEDIA_STREAM_SPEAKER_STOP
 * @return 0 on success
 */
int demo_on_live_audio_stop_callback(void);

/**
 * @brief P2P recv audio frame from APP (G.711U), decode and play to speaker
 * @param[in] media_frame G.711 mu-law payload from APP
 * @return 0 on success
 */
int demo_on_recv_audio_frame_callback(MEDIA_FRAME *media_frame);

/**
 * @brief Ask the encoder for a key frame now
 * @return 0 when the encoder accepted the request
 */
int demo_on_request_i_frame_callback(void);

/**
 * @brief Move the encoder's target bitrate
 * @param[in] kbps requested bitrate
 * @return 0 when the encoder accepted the change
 */
int demo_on_set_video_bitrate_callback(uint32_t kbps);

/**
 * @brief Pause mic uplink (for PB send path — free P2P/UDP buffer)
 * @return none
 */
void demo_mic_uplink_pause(void);

#ifdef __cplusplus
}
#endif

#endif /*__TUYA_IPC_DEMO_H__*/
