/**
 * @file standby_screen.h
 * @brief Declaration of the standby screen for the application
 *
 * This file contains the declarations for the standby screen which displays
 * "TuyaOpen" text with a 3D horizontal rotation effect in black and white.
 *
 * The standby screen includes:
 * - Screen initialization and deinitialization functions
 * - 3D rotation animation for text (monochrome version)
 * - Screen structure definition for the screen manager
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef STANDBY_SCREEN_H
#define STANDBY_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

extern Screen_t standby_screen;

void standby_screen_init(void);
void standby_screen_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*STANDBY_SCREEN_H*/
