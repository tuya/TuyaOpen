/**
 * @file wifi_scan_screen.h
 * @brief Declaration of the WiFi scan screen for the application
 *
 * This file contains the declarations for the WiFi scan screen which provides
 * WiFi access point scanning functionality with hardware integration.
 *
 * The WiFi scan screen includes:
 * - Screen initialization and deinitialization functions
 * - Screen structure definition for the screen manager
 * - WiFi AP scanning and display
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef WIFI_SCAN_SCREEN_H
#define WIFI_SCAN_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

extern Screen_t wifi_scan_screen;

void wifi_scan_screen_init(void);
void wifi_scan_screen_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*WIFI_SCAN_SCREEN_H*/
