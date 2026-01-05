/**
 * @file level_indicator_screen.h
 * @brief Declaration of the level indicator screen for the application
 *
 * This file contains the declarations for the level indicator screen which provides
 * a digital spirit level with tilt detection, angle display, and calibration.
 *
 * The level indicator screen includes:
 * - Screen initialization and deinitialization functions
 * - Screen structure definition for the screen manager
 * - Tilt sensor integration and calibration
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef LEVEL_INDICATOR_SCREEN_H
#define LEVEL_INDICATOR_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

extern Screen_t level_indicator_screen;

void level_indicator_screen_init(void);
void level_indicator_screen_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LEVEL_INDICATOR_SCREEN_H*/
