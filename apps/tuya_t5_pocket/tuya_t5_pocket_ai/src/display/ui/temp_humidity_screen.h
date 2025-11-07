/**
 * @file temp_humidity_screen.h
 * @brief Declaration of the temperature and humidity sensor screen
 *
 * This file contains the declarations for the temperature and humidity sensor screen
 * which displays real-time temperature and humidity readings from sensors.
 * Currently uses fixed values for demonstration purposes.
 *
 * The temperature humidity screen includes:
 * - Screen initialization and deinitialization functions
 * - Screen structure definition for the screen manager
 * - Real-time data display update timer
 * - Keyboard event handling for navigation
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef TEMP_HUMIDITY_SCREEN_H
#define TEMP_HUMIDITY_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

extern Screen_t temp_humidity_screen;

void temp_humidity_screen_init(void);
void temp_humidity_screen_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*TEMP_HUMIDITY_SCREEN_H*/