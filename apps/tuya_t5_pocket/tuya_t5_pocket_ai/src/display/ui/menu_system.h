/**
 * @file menu_system.h
 * Menu System Component for AI Pocket Pet
 */

#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../lvgl/lvgl.h"
#include "ai_pocket_pet_app.h"
#include "keyboard.h"

/**********************
 *      TYPEDEFS
 **********************/
// Pet event types for menu actions
typedef enum {
    PET_EVENT_FEED_HAMBURGER,
    PET_EVENT_DRINK_WATER,
    PET_EVENT_FEED_PIZZA,
    PET_EVENT_FEED_APPLE,
    PET_EVENT_FEED_FISH,
    PET_EVENT_FEED_CARROT,
    PET_EVENT_FEED_ICE_CREAM,
    PET_EVENT_FEED_COOKIE,
    PET_EVENT_TOILET,
    PET_EVENT_TAKE_BATH,
    PET_EVENT_SEE_DOCTOR,
    PET_EVENT_SLEEP,
    PET_EVENT_WAKE_UP,
    PET_EVENT_WIFI_SCAN,
    PET_EVENT_I2C_SCAN,
    PET_STAT_RANDOMIZE,
    PET_EVENT_MAX
} pet_event_type_t;

// Pet event callback function type
typedef void (*pet_event_callback_t)(pet_event_type_t event_type, void *user_data);

typedef struct {
    uint8_t health;    // 0-100
    uint8_t hungry;    // 0-100
    uint8_t clean;    // 0-100
    uint8_t happy;     // 0-100
    uint16_t age_days; // Age in days
    float weight_kg;   // Weight in kg (decimal)
    char name[16];     // Pet name
} pet_stats_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create the bottom menu with navigation buttons
 * @param parent Parent object to attach the menu to
 * @return Pointer to the created bottom menu object
 */
lv_obj_t* menu_system_create_bottom_menu(lv_obj_t *parent);

/**
 * Create the sub menu container
 * @param parent Parent object to attach the sub menu to
 * @return Pointer to the created sub menu object
 */
lv_obj_t* menu_system_create_sub_menu(lv_obj_t *parent);

/**
 * Handle main menu navigation (up/down/left/right)
 * @param key The key pressed
 */
void menu_system_handle_main_navigation(uint32_t key);

/**
 * Handle sub menu navigation (up/down)
 * @param key The key pressed
 */
void menu_system_handle_sub_navigation(uint32_t key);

/**
 * Handle main menu selection (ENTER key)
 */
void menu_system_handle_main_selection(void);

/**
 * Handle sub menu selection (ENTER key)
 */
void menu_system_handle_sub_selection(void);

/**
 * Hide the sub menu and return to main menu
 */
void menu_system_hide_sub_menu(void);

/**
 * Get current menu type
 * @return Current menu type
 */
ai_pet_menu_t menu_system_get_current_menu(void);

/**
 * Get current selected button
 * @return Current selected button index
 */
uint8_t menu_system_get_selected_button(void);

/**
 * Initialize pet statistics with default values
 * @param stats Pointer to pet stats structure
 */
void menu_system_init_pet_stats(pet_stats_t *stats);

/**
 * Update pet statistics
 * @param stats Pointer to pet stats structure
 */
uint8_t menu_system_update_pet_stats(pet_stats_t *stats);

/**
 * Get pet statistics
 * @return Pointer to pet stats structure
 */
pet_stats_t* menu_system_get_pet_stats(void);

/**
 * Update pet stats for testing
 */
void menu_system_update_pet_stats_for_testing(void);

/**
 * Register callback function for pet events
 * @param callback Function to call when pet events occur
 * @param user_data User data to pass to the callback
 */
void menu_system_register_pet_event_callback(pet_event_callback_t callback, void *user_data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MENU_SYSTEM_H */
