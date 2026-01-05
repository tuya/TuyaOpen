/**
 * @file i2c_scan_screen.h
 * @brief Declaration of the I2C scan screen for the application
 *
 * This file contains the declarations for the I2C scan screen which provides
 * I2C device scanning functionality across multiple ports with hardware integration.
 *
 * The I2C scan screen includes:
 * - Screen initialization and deinitialization functions
 * - Screen structure definition for the screen manager
 * - I2C port scanning and device detection
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef I2C_SCAN_SCREEN_H
#define I2C_SCAN_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

extern Screen_t i2c_scan_screen;

void i2c_scan_screen_init(void);
void i2c_scan_screen_deinit(void);
void i2c_scan_screen_show_port(uint8_t port);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*I2C_SCAN_SCREEN_H*/
