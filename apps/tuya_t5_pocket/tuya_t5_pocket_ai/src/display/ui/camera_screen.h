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

extern Screen_t camera_screen;

void camera_screen_init(void);
void camera_screen_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* CAMERA_SCREEN_H */
