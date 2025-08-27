/**
 * @file keyboard.h
 * Custom Keyboard Widget for AI Pocket Pet
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../lvgl/lvgl.h"

/*********************
 *      DEFINES
 *********************/
#ifndef AI_PET_SCREEN_WIDTH
#define AI_PET_SCREEN_WIDTH  384
#endif

#ifndef AI_PET_SCREEN_HEIGHT
#define AI_PET_SCREEN_HEIGHT 168
#endif

/**********************
 *      TYPEDEFS
 **********************/

typedef enum {
    KEYBOARD_RESULT_OK,
    KEYBOARD_RESULT_CANCEL,
    KEYBOARD_RESULT_MENU
} keyboard_result_t;

typedef void (*keyboard_callback_t)(keyboard_result_t result, const char *text, void *user_data);

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize the keyboard widget
 */
void keyboard_init(void);

/**
 * Show the keyboard widget
 * @param initial_text Initial text to display (can be NULL)
 * @param callback Callback function to handle keyboard results
 * @param user_data User data passed to callback
 */
void keyboard_show(const char *initial_text, keyboard_callback_t callback, void *user_data);

/**
 * Hide the keyboard widget
 */
void keyboard_hide(void);

/**
 * Handle input events for the keyboard widget
 * @param key The key pressed
 */
void keyboard_handle_input(uint32_t key);

/**
 * Check if keyboard widget is currently active
 * @return true if keyboard is active, false otherwise
 */
bool keyboard_is_active(void);

/**
 * Cleanup keyboard after screen has been restored
 * This should be called after the main screen is loaded
 */
void keyboard_cleanup(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* KEYBOARD_H */
