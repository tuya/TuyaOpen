/**
 * @file menu_bath_screen.h
 * @brief Header file for the bath menu screen
 *
 * This file contains the declarations for the bath menu screen which displays
 * cleaning and hygiene options for the pet.
 *
 * The bath menu includes:
 * - Toilet and bathroom facilities
 * - Bath and cleaning options
 * - Hygiene status indicators
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef MENU_BATH_SCREEN_H
#define MENU_BATH_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "screen_manager.h"

/*********************
 *      DEFINES
 *********************/

/*********************
 *      TYPEDEFS
 *********************/

/**
 * @brief Bath action types
 */
typedef enum {
    BATH_ACTION_TOILET,
    BATH_ACTION_TAKE_BATH,
    BATH_ACTION_BRUSH_TEETH,
    BATH_ACTION_WASH_HANDS,
} bath_action_t;

/**
 * @brief Bath event callback function type
 */
typedef void (*bath_event_callback_t)(bath_action_t action, void *user_data);

/**
 * @brief Hygiene status structure
 */
typedef struct {
    uint8_t cleanliness;        /**< Cleanliness level (0-100) */
    uint8_t toilet_need;        /**< Toilet need level (0-100) */
    bool needs_bath;            /**< Whether pet needs a bath */
    uint32_t last_bath_time;    /**< Last bath timestamp */
} hygiene_status_t;

/*********************
 * GLOBAL PROTOTYPES
 *********************/

/**
 * @brief External declaration of the bath menu screen structure
 */
extern Screen_t menu_bath_screen;

/**
 * @brief Initialize the bath menu screen
 *
 * This function creates the bath menu UI with cleaning options
 * and hygiene status display.
 */
void menu_bath_screen_init(void);

/**
 * @brief Deinitialize the bath menu screen
 *
 * This function cleans up the bath menu by removing event callbacks
 * and freeing resources.
 */
void menu_bath_screen_deinit(void);

/**
 * @brief Set hygiene status for display
 *
 * @param status Pointer to hygiene status structure
 */
void menu_bath_screen_set_hygiene_status(hygiene_status_t *status);

/**
 * @brief Get current hygiene status
 *
 * @return Pointer to current hygiene status
 */
hygiene_status_t* menu_bath_screen_get_hygiene_status(void);

/*********************
 *      MACROS
 *********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MENU_BATH_SCREEN_H*/
