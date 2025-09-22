/**
 * @file startup_screen.h
 * Startup Screen for AI Pocket Pet Application
 *
 * This module handles the startup screen that displays the Tuya Open logo
 * and transitions to the main application after 1 second.
 */

#ifndef STARTUP_SCREEN_H
#define STARTUP_SCREEN_H

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

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create and display the startup screen
 * @return Pointer to the startup screen object
 */
lv_obj_t* startup_screen_create(void);

/**
 * Get the startup screen object
 * @return Pointer to the startup screen object
 */
lv_obj_t* startup_screen_get(void);

/**
 * Timer callback to transition from startup to main screen
 * @param timer The timer object
 */
void startup_screen_timer_cb(lv_timer_t *timer);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STARTUP_SCREEN_H */
