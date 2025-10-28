/**
 * @file toast_screen.h
 * @brief Declaration of the toast screen for the application
 *
 * This file contains the declarations for the toast screen which displays
 * toast messages with customizable text and auto-hide functionality.
 *
 * The toast screen includes:
 * - Toast message container with styling
 * - Customizable message text
 * - Auto-hide timer functionality
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef TOAST_SCREEN_H
#define TOAST_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

extern Screen_t toast_screen;

/**
 * @brief Initialize the toast screen
 *
 * This function creates the toast screen UI with a toast container,
 * message label, and auto-hide functionality.
 */
void toast_screen_init(void);

/**
 * @brief Deinitialize the toast screen
 *
 * This function cleans up the toast screen by removing event callbacks
 * and freeing resources.
 */
void toast_screen_deinit(void);

/**
 * @brief Show toast message
 *
 * @param message The message text to display
 * @param delay_ms Auto-hide delay in milliseconds (0 for default delay)
 */
void toast_screen_show(const char *message, uint32_t delay_ms);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*TOAST_SCREEN_H*/
