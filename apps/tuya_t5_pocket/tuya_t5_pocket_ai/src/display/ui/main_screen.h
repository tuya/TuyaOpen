/**
 * @file main_screen.h
 * @brief Declaration of the main screen for the application
 *
 * This file contains the declarations for the main screen which displays
 * the main AI Pocket Pet interface including status bar, pet area, and
 * menu system. This is the primary screen after the startup sequence.
 *
 * The main screen includes:
 * - Screen initialization and deinitialization functions
 * - Screen structure definition for the screen manager
 * - Main UI components initialization
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef MAIN_SCREEN_H
#define MAIN_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

/***********************************************************
***********************Type Definitions********************
***********************************************************/
typedef enum {
    AI_PET_STATE_NORMAL,        // Normal state (walk, blink, stand)
    AI_PET_STATE_SLEEP,         // Sleeping animation
    AI_PET_STATE_DANCE,         // Dancing animation
    AI_PET_STATE_EAT,           // Eating animation
    AI_PET_STATE_BATH,          // Bathing animation
    AI_PET_STATE_TOILET,        // Toilet animation
    AI_PET_STATE_SICK,          // Sick animation
    AI_PET_STATE_HAPPY,         // Happy emotion
    AI_PET_STATE_ANGRY,         // Angry emotion
    AI_PET_STATE_CRY,           // Crying emotion
    // Legacy states for backward compatibility
    AI_PET_STATE_IDLE = AI_PET_STATE_NORMAL,
    AI_PET_STATE_WALKING = AI_PET_STATE_NORMAL,
    AI_PET_STATE_BLINKING = AI_PET_STATE_NORMAL,
    AI_PET_STATE_EATING = AI_PET_STATE_EAT,
    AI_PET_STATE_SLEEPING = AI_PET_STATE_SLEEP,
    AI_PET_STATE_PLAYING = AI_PET_STATE_DANCE
} ai_pet_state_t;

typedef enum {
    AI_PET_MENU_MAIN,
    AI_PET_MENU_INFO,
    AI_PET_MENU_FOOD,
    AI_PET_MENU_BATH,
    AI_PET_MENU_HEALTH,
    AI_PET_MENU_SLEEP
} ai_pet_menu_t;

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
    uint8_t clean;     // 0-100
    uint8_t happy;     // 0-100
    uint16_t age_days; // Age in days
    float weight_kg;   // Weight in kg (decimal)
    char name[16];     // Pet name
} pet_stats_t;

/**********************************************************/
extern Screen_t main_screen;

void main_screen_init(void);
void main_screen_deinit(void);

/**
 * State setting interface functions - Only change state, UI update in timer
 */

/**
 * Set pet animation state (state machine only, UI update in timer)
 * @param state Target animation state
 */
void main_screen_set_pet_animation_state(ai_pet_state_t state);

/**
 * Set WiFi signal strength state (state machine only, UI update in timer)
 * @param strength WiFi signal strength (0-5: 0=off, 1-3=bars, 4=find, 5=add)
 */
void main_screen_set_wifi_state(uint8_t strength);

/**
 * Set battery level and charging state (state machine only, UI update in timer)
 * @param level Battery level (0-6: 0=empty, 6=full)
 * @param charging Battery charging status (true=charging, false=discharging)
 */
void main_screen_set_battery_state(uint8_t level, bool charging);

/**
 * Register callback function for pet events
 * @param callback Function to call when pet events occur
 * @param user_data User data to pass to the callback
 */
void main_screen_register_pet_event_callback(pet_event_callback_t callback, void *user_data);

/**
 * Get pet statistics from main screen
 * @return Pointer to pet stats structure
 */
pet_stats_t* main_screen_get_pet_stats(void);

/**
 * Update pet statistics in main screen
 * @param stats Pointer to pet stats structure
 * @return 0 on success, 1 on error
 */
uint8_t main_screen_update_pet_stats(pet_stats_t *stats);

/**
 * Initialize pet statistics with default values
 * @param stats Pointer to pet stats structure
 */
void main_screen_init_pet_stats(pet_stats_t *stats);

/**
 * Handle pet event and update animations accordingly
 * @param event_type Type of pet event
 */
void main_screen_handle_pet_event(pet_event_type_t event_type);
#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MAIN_SCREEN_H*/
