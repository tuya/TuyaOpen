/**
 * @file screen_manager.h
 * @brief Screen manager for handling screen navigation and stack operations
 *
 * This file contains the declarations for the screen manager system which
 * implements a stack-based screen navigation system. It provides functions
 * to push screens onto the stack, pop screens from the stack, and navigate
 * between screens with animation effects.
 *
 * The screen manager supports:
 * - Stack-based screen navigation (push/pop operations)
 * - Screen transition animations
 * - Home screen navigation
 * - Screen lifecycle management (init/deinit)
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include "../lvgl/lvgl.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define ENABLE_LVGL_HARDWARE
#ifdef ENABLE_LVGL_HARDWARE
#include "tal_log.h"
#include "tuya_cloud_types.h"
#include "tal_api.h"
#define printf PR_DEBUG
#endif

#define KEY_UP    17
#define KEY_LEFT  20
#define KEY_DOWN  18
#define KEY_RIGHT 19
#define KEY_ENTER 10
#define KEY_ESC   27
#define KEY_JOYCON  32
#define KEY_AI    105

#ifndef AI_PET_SCREEN_WIDTH
#define AI_PET_SCREEN_WIDTH  384
#endif
#ifndef AI_PET_SCREEN_HEIGHT
#define AI_PET_SCREEN_HEIGHT 168
#endif
/***********************************************************
***********************variable define**********************
***********************************************************/
/**
 * @brief Screen structure definition
 *
 * Defines a screen with initialization and deinitialization functions,
 * a pointer to the screen object, a name identifier, and state preservation.
 */
typedef struct {
    void (*init)(void);           /**< Screen initialization function */
    void (*deinit)(void);         /**< Screen deinitialization function */
    lv_obj_t **screen_obj;        /**< Pointer to the screen object */
    char *name;                   /**< Screen name identifier */
    void *state_data;             /**< Pointer to screen-specific state data */
} Screen_t;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Get the current screen (top of stack)
 * @return Pointer to the current screen, or NULL if the screen stack is empty
 */
Screen_t* screen_get_now_screen(void);

/**
 * @brief Go back to the previous screen
 *
 * This function unloads the current screen and loads the previous screen.
 * If there is no previous screen, it loads the startup screen.
 */
void screen_back(void);

/**
 * @brief Go back to the home screen (bottom of stack)
 *
 * This function unloads all screens except the home screen.
 */
void screen_back_bottom(void);

/**
 * @brief Load a new screen to the top of the stack
 *
 * @param newScreen Pointer to the new screen to be loaded
 *
 * This function pushes the current screen onto the stack and loads the specified new screen.
 * A slide-in-from-right animation effect is used when switching screens.
 */
void screen_load(Screen_t *newScreen);

/**
 * @brief Initialize the screen manager
 *
 * This function initializes the screen stack and loads the startup screen.
 */
void screens_init(void);

#endif // SCREEN_STACK_H
