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

#include "tuya_ipc_p2p.h"

/**
 * @brief Initialize demo video file
 */
void tuya_ipc_demo_start(void);

/**
 * @brief Clean up demo resources
 */
void tuya_ipc_demo_end(void);

/**
 * @brief Signal disconnect callback function
 * @return 0 on success
 */
int demo_on_signal_disconnect_callback(void);

/**
 * @brief Get video frame callback function
 * @param media_frame Media frame structure
 * @return 0 on success, -1 on failure
 */
int demo_on_get_video_frame_callback(MEDIA_FRAME *media_frame);

/**
 * @brief Get audio frame callback function
 * @param media_frame Media frame structure
 * @return 0 on success
 */
int demo_on_get_audio_frame_callback(MEDIA_FRAME *media_frame);

#ifdef __cplusplus
}
#endif

#endif /*__TUYA_IPC_DEMO_H__*/
