/**
 * @file menu_health_screen.h
 * @brief Header file for the health menu screen
 *
 * This file contains the declarations for the health menu screen which displays
 * health and medical options for the pet.
 *
 * The health menu includes:
 * - Health status indicators
 * - Medical care options
 * - Doctor visit functionality
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef MENU_HEALTH_SCREEN_H
#define MENU_HEALTH_SCREEN_H

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
 * @brief Health action types
 */
typedef enum {
    HEALTH_ACTION_SEE_DOCTOR,
    HEALTH_ACTION_TAKE_MEDICINE,
    HEALTH_ACTION_CHECK_SYMPTOMS,
    HEALTH_ACTION_EXERCISE,
} health_action_t;

/**
 * @brief Health event callback function type
 */
typedef void (*health_event_callback_t)(health_action_t action, void *user_data);

/**
 * @brief Health status structure
 */
typedef struct {
    uint8_t health_level;       /**< Overall health level (0-100) */
    uint8_t energy_level;       /**< Energy level (0-100) */
    bool is_sick;               /**< Whether pet is sick */
    bool needs_doctor;          /**< Whether pet needs medical attention */
    uint32_t last_checkup_time; /**< Last doctor visit timestamp */
    char symptoms[64];          /**< Current symptoms description */
} health_status_t;

/*********************
 * GLOBAL PROTOTYPES
 *********************/

/**
 * @brief External declaration of the health menu screen structure
 */
extern Screen_t menu_health_screen;

/**
 * @brief Initialize the health menu screen
 *
 * This function creates the health menu UI with health status
 * and medical care options.
 */
void menu_health_screen_init(void);

/**
 * @brief Deinitialize the health menu screen
 *
 * This function cleans up the health menu by removing event callbacks
 * and freeing resources.
 */
void menu_health_screen_deinit(void);

/**
 * @brief Set health status for display
 *
 * @param status Pointer to health status structure
 */
void menu_health_screen_set_health_status(health_status_t *status);

/**
 * @brief Get current health status
 *
 * @return Pointer to current health status
 */
health_status_t* menu_health_screen_get_health_status(void);

/*********************
 *      MACROS
 *********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MENU_HEALTH_SCREEN_H*/
