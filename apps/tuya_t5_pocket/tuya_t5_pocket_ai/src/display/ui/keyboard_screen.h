/**
 * @file keyboard_screen.h
 * @brief Declaration of the keyboard screen for the application
 *
 * This file contains the declarations for the keyboard screen which provides
 * a virtual keyboard for text input with support for navigation and callbacks.
 *
 * The keyboard screen includes:
 * - Screen initialization and deinitialization functions
 * - Screen structure definition for the screen manager
 * - Text input and callback management
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef KEYBOARD_SCREEN_H
#define KEYBOARD_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

typedef void (*keyboard_callback_t)(const char *text, void *user_data);

extern Screen_t keyboard_screen;

void keyboard_screen_init(void);
void keyboard_screen_deinit(void);
void keyboard_screen_show_with_callback(const char *initial_text, keyboard_callback_t callback, void *user_data);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*KEYBOARD_SCREEN_H*/
