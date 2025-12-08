/**
 * @file camera_screen.h
 * @brief Camera screen with binary conversion control UI
 *
 * This file contains the declarations for the camera screen which displays
 * camera feed on the left side and binary conversion settings on the right side.
 *
 * The camera screen includes:
 * - Left side: Real-time camera feed display (monochrome)
 * - Right side: Binary conversion method and threshold display
 * - Joystick controls:
 *   - UP/DOWN: Adjust threshold (in fixed threshold mode)
 *   - LEFT/RIGHT: Switch binary conversion method
 *   - ENTER: Start/stop camera
 *   - ESC: Return to previous screen
 *
 * @copyright Copyright (c) 2025 Tuya Inc. All Rights Reserved.
 */

#ifndef CAMERA_SCREEN_H
#define CAMERA_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"
#include "yuv422_to_binary.h"
/**
 * @brief Camera screen lifecycle callback type
 * Called when screen is initialized or deinitialized
 *
 * @param is_init TRUE for init, FALSE for deinit
 */
typedef void (*camera_screen_lifecycle_cb_t)(BOOL_T is_init);

/**
 * @brief Camera photo print callback type
 * Called when ENTER key is pressed to print current photo from raw YUV422 data
 * @param params Conversion parameters for printing
 * @note The yuv422_data buffer will be freed after callback returns
 */
typedef void (*camera_photo_print_cb_t)(const YUV422_TO_BINARY_PARAMS_T *params);

extern Screen_t camera_screen;

void camera_screen_init(void);
void camera_screen_deinit(void);

/**
 * @brief Register lifecycle callback for camera screen
 * @param callback Callback function, NULL to unregister
 */
void camera_screen_register_lifecycle_cb(camera_screen_lifecycle_cb_t callback);

/**
 * @brief Register photo print callback for camera screen
 * Called when ENTER key is pressed with current photo data
 * @param callback Callback function, NULL to unregister
 */
void camera_screen_register_print_cb(camera_photo_print_cb_t callback);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* CAMERA_SCREEN_H */
