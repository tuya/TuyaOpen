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
 * @brief Show toast message as an overlay on the current screen
 *
 * @param message The message text to display
 * @param delay_ms Auto-hide delay in milliseconds (0 for default delay)
 * 
 * This function creates a floating toast overlay on top of the current active screen.
 * The toast does not replace the current screen, it simply appears on top as a popup.
 */
void toast_screen_show(const char *message, uint32_t delay_ms);

/**
 * @brief Manually hide the toast overlay
 *
 * This function can be called to immediately dismiss the toast overlay.
 */
void toast_screen_hide(void);


#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*TOAST_SCREEN_H*/
