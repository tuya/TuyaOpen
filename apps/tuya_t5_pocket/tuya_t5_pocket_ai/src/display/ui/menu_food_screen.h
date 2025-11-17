/**
 * @file menu_food_screen.h
 * @brief Header file for the food menu screen
 *
 * This file contains the declarations for the food menu screen which displays
 * food and nutrition options for the pet.
 *
 * The food menu includes:
 * - Food item selection with icons
 * - Nutritional information display
 * - Level-based food unlocking
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef MENU_FOOD_SCREEN_H
#define MENU_FOOD_SCREEN_H

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
 * @brief Food item structure
 */
typedef struct {
    const char *name;           /**< Food name */
    const char *icon;           /**< Food icon symbol */
    uint8_t required_level;     /**< Required pet level to unlock */
    uint8_t hunger_restore;     /**< Hunger points restored */
    uint8_t happiness_bonus;    /**< Happiness bonus points */
    bool available;             /**< Whether food is available */
} food_item_t;

/**
 * @brief Food event types
 */
typedef enum {
    FOOD_EVENT_FEED_HAMBURGER,
    FOOD_EVENT_DRINK_WATER,
    FOOD_EVENT_FEED_PIZZA,
    FOOD_EVENT_FEED_APPLE,
    FOOD_EVENT_FEED_FISH,
    FOOD_EVENT_FEED_CARROT,
    FOOD_EVENT_FEED_ICE_CREAM,
    FOOD_EVENT_FEED_COOKIE,
} food_event_t;

/**
 * @brief Food event callback function type
 */
typedef void (*food_event_callback_t)(food_event_t event, void *user_data);

/*********************
 * GLOBAL PROTOTYPES
 *********************/

/**
 * @brief External declaration of the food menu screen structure
 */
extern Screen_t menu_food_screen;

/**
 * @brief Initialize the food menu screen
 *
 * This function creates the food menu UI with food item selection
 * and nutritional information display.
 */
void menu_food_screen_init(void);

/**
 * @brief Deinitialize the food menu screen
 *
 * This function cleans up the food menu by removing event callbacks
 * and freeing resources.
 */
void menu_food_screen_deinit(void);

/**
 * @brief Set pet level for food unlocking
 *
 * @param level Current pet level
 */
void menu_food_screen_set_pet_level(uint8_t level);

/*********************
 *      MACROS
 *********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MENU_FOOD_SCREEN_H*/
