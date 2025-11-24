/**
 * @file startup_screen.h
 * @brief Declaration of the startup screen for the application
 *
 * This file contains the declarations for the startup screen which is displayed
 * when the application starts. It shows a splash screen with "TuyaOpen" and
 * "AI Pocket Pet Demo" text, and automatically transitions after a timeout.
 *
 * The startup screen includes:
 * - Screen initialization and deinitialization functions
 * - Screen structure definition for the screen manager
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef STARTUP_SCREEN_H
#define STARTUP_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

extern Screen_t startup_screen;

void startup_screen_init(void);
void startup_screen_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*STARTUP_SCREEN_H*/
