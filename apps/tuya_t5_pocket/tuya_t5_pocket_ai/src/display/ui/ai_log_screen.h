/**
 * @file ai_log_screen.h
 * @brief Declaration of the AI log screen for the application
 *
 * This file contains the declarations for the AI log screen which displays
 * analysis logs in a text area with scrolling support.
 *
 * The AI log screen includes:
 * - Title display
 * - Scrollable text area for log content
 * - API to update log content dynamically
 * - Keyboard navigation support
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef AI_LOG_SCREEN_H
#define AI_LOG_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

/**
 * @brief AI log screen lifecycle callback type
 * Called when screen is initialized or deinitialized
 *
 * @param is_init TRUE for init, FALSE for deinit
 */
typedef void (*ai_log_screen_lifecycle_cb_t)(BOOL_T is_init);

extern Screen_t ai_log_screen;

void ai_log_screen_init(void);
void ai_log_screen_deinit(void);

/**
 * @brief Register lifecycle callback for AI log screen
 * This allows external modules to be notified when the screen is shown/hidden
 *
 * @param callback Callback function, NULL to unregister
 */
void ai_log_screen_register_lifecycle_cb(ai_log_screen_lifecycle_cb_t callback);

/**
 * @brief Update AI log content
 * @param log_text Pointer to log text string
 * @param length Length of the log text
 */
void ai_log_screen_update_log(const char *log_text, size_t length);

/**
 * @brief Append text to existing log content
 * @param log_text Pointer to log text string to append
 * @param length Length of the log text
 */
void ai_log_screen_append_log(const char *log_text, size_t length);

/**
 * @brief Clear all log content
 */
void ai_log_screen_clear_log(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*AI_LOG_SCREEN_H*/