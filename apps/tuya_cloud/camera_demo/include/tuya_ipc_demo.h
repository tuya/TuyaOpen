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

/**
 * @brief Initialize demo video file
 * @return none
 */
VOID_T tuya_ipc_demo_start(VOID_T);

/**
 * @brief Clean up demo resources
 * @return none
 */
VOID_T tuya_ipc_demo_end(VOID_T);

/**
 * @brief Signal disconnect callback function
 * @return 0 on success
 */
INT_T demo_on_signal_disconnect_callback(VOID_T);

/**
 * @brief P2P live video start (align TuyaOS MEDIA_STREAM_LIVE_VIDEO_START)
 * @return 0 on success
 */
INT_T demo_on_live_video_start_callback(VOID_T);

/**
 * @brief P2P live video stop (align TuyaOS MEDIA_STREAM_LIVE_VIDEO_STOP)
 * @return 0 on success
 */
INT_T demo_on_live_video_stop_callback(VOID_T);

/**
 * @brief Get video frame callback function
 * @param[in,out] media_frame Media frame structure
 * @return 0 on success, -1 on failure
 */
INT_T demo_on_get_video_frame_callback(MEDIA_FRAME *media_frame);

/**
 * @brief Get audio frame callback function
 * @param[in,out] media_frame Media frame structure
 * @return 0 on success
 */
INT_T demo_on_get_audio_frame_callback(MEDIA_FRAME *media_frame);

/**
 * @brief P2P downlink intercom start (APP->device speaker), align MEDIA_STREAM_SPEAKER_START
 * @return 0 on success
 */
INT_T demo_on_live_audio_start_callback(VOID_T);

/**
 * @brief P2P downlink intercom stop, align MEDIA_STREAM_SPEAKER_STOP
 * @return 0 on success
 */
INT_T demo_on_live_audio_stop_callback(VOID_T);

/**
 * @brief P2P recv audio frame from APP (G.711U), decode+resample+play to speaker
 * @param[in] media_frame G.711 mu-law payload from APP
 * @return 0 on success
 */
INT_T demo_on_recv_audio_frame_callback(MEDIA_FRAME *media_frame);

/**
 * @brief Pause mic uplink (for PB send path — free P2P/UDP buffer)
 * @return none
 */
VOID_T demo_mic_uplink_pause(VOID_T);

#ifdef __cplusplus
}
#endif

#endif /*__TUYA_IPC_DEMO_H__*/
