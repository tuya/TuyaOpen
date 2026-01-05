/**
 * @file menu_info_screen.h
 * @brief Header file for the info menu screen
 *
 * This file contains the declarations for the info menu screen which displays
 * pet information including name, stats, and actions.
 *
 * The info menu includes:
 * - Pet name display
 * - Pet statistics with icon bars
 * - Action buttons for pet management
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef MENU_INFO_SCREEN_H
#define MENU_INFO_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "main_screen.h"
/*********************
 *      DEFINES
 *********************/

/*********************
 *      TYPEDEFS
 *********************/

/**
 * @brief Pet statistics structure
 */

/*********************
 * GLOBAL PROTOTYPES
 *********************/

/**
 * @brief External declaration of the info menu screen structure
 */
extern Screen_t menu_info_screen;

/**
 * @brief Initialize the info menu screen
 *
 * This function creates the info menu UI with pet information display,
 * statistics bars, and action buttons.
 */
void menu_info_screen_init(void);

/**
 * @brief Deinitialize the info menu screen
 *
 * This function cleans up the info menu by removing event callbacks
 * and freeing resources.
 */
void menu_info_screen_deinit(void);

/**
 * @brief Set pet statistics for display
 *
 * @param stats Pointer to pet statistics structure
 */
void menu_info_screen_set_pet_stats(pet_stats_t *stats);

/**
 * @brief Get current pet statistics
 *
 * @return Pointer to current pet statistics
 */
pet_stats_t* menu_info_screen_get_pet_stats(void);

/*********************
 *      MACROS
 *********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MENU_INFO_SCREEN_H*/
