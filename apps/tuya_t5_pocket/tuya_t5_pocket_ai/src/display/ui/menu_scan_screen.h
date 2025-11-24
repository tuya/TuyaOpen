/**
 * @file menu_scan_screen.h
 * @brief Declaration of the scan menu screen for the application
 *
 * This file contains the declarations for the scan menu screen which provides
 * various scanning and game features including WiFi scan, I2C scan, and games.
 *
 * The scan menu includes:
 * - WiFi scanning functionality
 * - I2C device scanning
 * - Mini games (Dino, Snake)
 * - Level indicator
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef MENU_SCAN_SCREEN_H
#define MENU_SCAN_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

extern Screen_t menu_scan_screen;

void menu_scan_screen_init(void);
void menu_scan_screen_deinit(void);


#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MENU_SCAN_SCREEN_H*/
