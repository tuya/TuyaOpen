/**
 * @file lv_demo_ai_pocket_pet.h
 * AI Pocket Pet Demo for LVGL
 */

#ifndef LV_DEMO_AI_POCKET_PET_H
#define LV_DEMO_AI_POCKET_PET_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../lvgl/lvgl.h"

// #define LVGL_SIMULATOR 1
#include "status_bar.h"
#include "toast.h"
/*********************
 *      DEFINES
 *********************/
#ifndef AI_PET_SCREEN_WIDTH
#define AI_PET_SCREEN_WIDTH  384
#endif
#ifndef AI_PET_SCREEN_HEIGHT
#define AI_PET_SCREEN_HEIGHT 168
#endif

// LVGL key codes
#define KEY_UP    17
#define KEY_LEFT  20
#define KEY_DOWN  18
#define KEY_RIGHT 19
#define KEY_ENTER 10
#define KEY_ESC   27
#define KEY_AI    105

/**********************
 *      TYPEDEFS
 **********************/

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
    AI_PET_MENU_SLEEP,
    AI_PET_MENU_SCAN,
} ai_pet_menu_t;

typedef struct {
    uint8_t health;    // 0-100
    uint8_t hungry;    // 0-100
    uint8_t happy;     // 0-100
    uint16_t age_days; // Age in days
    float weight_kg;   // Weight in kg (decimal)
    char name[16];     // Pet name
} ai_pet_stats_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize and start the AI Pocket Pet demo
 */
void lv_demo_ai_pocket_pet(void);

/**
 * Handle input events (joystick/keyboard simulation)
 * @param key The key pressed
 */
void lv_demo_ai_pocket_pet_handle_input(uint32_t key);

/**
 * Show a toast message overlay
 * @param message The text message to display
 * @param delay_ms How long to show the toast (in milliseconds, 0 for default)
 */
void lv_demo_ai_pocket_pet_show_toast(const char *message, uint32_t delay_ms);

/**
 * Hide the toast message immediately
 */
void lv_demo_ai_pocket_pet_hide_toast(void);

/**
 * Set WiFi signal strength
 * @param strength 0 = off, 1-3 = bars, 4 = find, 5 = add
 */
void lv_demo_ai_pocket_pet_set_wifi_strength(uint8_t strength);

/**
 * Set cellular signal strength and connection status
 * @param strength 0 = off, 1-3 = bars, 4 = no internet
 * @param connected Whether cellular is connected to internet
 */
void lv_demo_ai_pocket_pet_set_cellular_status(uint8_t strength, bool connected);

/**
 * Get current WiFi signal strength
 * @return Current WiFi signal strength (0-5)
 */
uint8_t lv_demo_ai_pocket_pet_get_wifi_strength(void);

/**
 * Get current cellular signal strength
 * @return Current cellular signal strength (0-4)
 */
uint8_t lv_demo_ai_pocket_pet_get_cellular_strength(void);

/**
 * Get current cellular connection status
 * @return Whether cellular is connected to internet
 */
bool lv_demo_ai_pocket_pet_get_cellular_connected(void);

/**
 * Set battery level and charging status
 * @param level Battery level (0-6, where 0 = empty, 5 = 5 bars, 6 = full)
 * @param charging Whether battery is charging
 */
void lv_demo_ai_pocket_pet_set_battery_status(uint8_t level, bool charging);

/**
 * Get current battery level
 * @return Current battery level (0-6)
 */
uint8_t lv_demo_ai_pocket_pet_get_battery_level(void);

/**
 * Get current battery charging status
 * @return Whether battery is charging
 */
bool lv_demo_ai_pocket_pet_get_battery_charging(void);

/**
 * Get the main screen object
 * @return Pointer to the main screen object
 */
lv_obj_t* lv_demo_ai_pocket_pet_get_main_screen(void);

void handle_ai_function(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_DEMO_AI_POCKET_PET_H */
