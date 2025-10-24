/**
 * @file ai_pocket_pet_app.c
 * AI Pocket Pet Application - Main Entry Point
 *
 * This is the main application file that integrates all modular components
 * and provides the original public API interface.
 */

/*********************
 *      INCLUDES
 *********************/

#include "ai_pocket_pet_app.h"
#include "status_bar.h"
#include "pet_area.h"
#include "menu_system.h"
#include "keyboard.h"
#include "peripherals_scan.h"
#include "toast.h"
#include "startup_screen.h"
#include "dino_game.h"
#include "level_indicator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef LVGL_SIMULATOR
#include "tal_api.h"
#include "lv_port_indev.h"
#endif
/*********************
 *      DEFINES
 *********************/

// Testing key mappings:
// Battery: A=empty, S=1bar, D=2bar, F=3bar, G=4bar, H=5bar, J=full, C=charging
// Pet Animation: 1=normal, 2=sleep, 3=dance, 4=eat, 5=bath, 6=toilet, 7=sick, 8=happy, 9=angry, 0=cry

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *status_bar;
    lv_obj_t *pet_area;
    lv_obj_t *bottom_menu;
    lv_obj_t *sub_menu;
} ai_pet_app_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void init_app_data(void);
static void create_main_screen(void);
#if LVGL_SIMULATOR
static void keyboard_event_cb(lv_event_t *e);
#endif
static void handle_main_menu_navigation(uint32_t key);
static void handle_sub_menu_navigation(uint32_t key);
static void handle_menu_selection(void);
static void handle_sub_menu_selection(void);
// static void handle_ai_function(void);

/**********************
 *  STATIC VARIABLES
 **********************/
static ai_pet_app_t g_app_data;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Main demo initialization function
 */
void lv_demo_ai_pocket_pet(void)
{
    // Initialize random seed for natural movement
    srand(time(NULL));

    // Initialize app data
    init_app_data();

    // Initialize keyboard widget
    keyboard_init();

    // Create startup screen first
    startup_screen_create();

    // Create main screen (but don't show it yet)
    create_main_screen();

    // Create UI components using modular approach
    g_app_data.status_bar = status_bar_create(g_app_data.screen);
    g_app_data.pet_area = pet_area_create(g_app_data.screen);
    g_app_data.bottom_menu = menu_system_create_bottom_menu(g_app_data.screen);
    g_app_data.sub_menu = menu_system_create_sub_menu(g_app_data.screen);

    // Create toast message system
    toast_create(g_app_data.screen);

    // Initialize pet stats
    pet_stats_t stats;
    menu_system_init_pet_stats(&stats);

    // Start pet animation
    pet_area_start_animation();

    // Test network status icons - demonstrate different states
    printf("Initializing network status icons...\n");

    // Set initial WiFi to 3 bars and cellular to 2 bars with connection
    lv_demo_ai_pocket_pet_set_wifi_strength(3);
    lv_demo_ai_pocket_pet_set_cellular_status(2, true);

    // Start timer to transition to main screen after 1 second
    lv_timer_create(startup_screen_timer_cb, 1000, NULL);
}

void lv_demo_ai_pocket_pet_handle_input(uint32_t key)
{
    printf("=== Input Handler: Key %d pressed ===\n", key);

    // If a modal input UI (keyboard / scan) is active prefer routing to it first
    bool keyboard_active = keyboard_is_active();
    printf("keyboard_is_active(): %d\n", keyboard_active);
    if (keyboard_active) {
        keyboard_handle_input(key);
        return;
    }

    // Route to scan UI if active (scan behaves like keyboard widget)
    bool i2c_active = i2c_scan_is_active();
    printf("i2c_scan_is_active(): %d\n", i2c_active);
    if (i2c_active) {
        i2c_scan_handle_input(key);
        return;
    }

    // Route to wifi scan UI if active
    bool wifi_active = wifi_scan_is_active();
    printf("wifi_scan_is_active(): %d\n", wifi_active);
    if (wifi_active) {
        wifi_scan_handle_input(key);
        return;
    }

    // Route to level indicator if active
    bool level_active = level_indicator_is_active();
    printf("level_indicator_is_active(): %d\n", level_active);
    if (level_active) {
        printf("Routing key %d to level_indicator\n", key);
        level_indicator_key_input(key);
        return;
    }

    // Check if games are active and route input accordingly
    extern int dino_game_is_active(void);
    extern int snake_game_is_active(void);
    extern void dino_game_key_input(int key);
    extern void snake_game_key_input(int key);

    int dino_active = dino_game_is_active();
    int snake_active = snake_game_is_active();
    
    printf("dino_game_is_active(): %d, snake_game_is_active(): %d\n", dino_active, snake_active);
    
    if (dino_active) {
        printf("Routing key %d to dino_game\n", key);
        dino_game_key_input(key);
        return;  // Don't process menu input when game is active
    }
    
    if (snake_active) {
        printf("Routing key %d to snake_game\n", key);
        snake_game_key_input(key);
        return;  // Don't process menu input when game is active
    }

    printf("Key pressed: %d (UP:%d LEFT:%d DOWN:%d RIGHT:%d ENTER:%d ESC:%d I:%d)\n",
           key, KEY_UP, KEY_LEFT, KEY_DOWN, KEY_RIGHT, KEY_ENTER, KEY_ESC, KEY_AI);

    switch(key) {
        case KEY_UP:
            printf("UP key pressed - navigating up\n");
            if(menu_system_get_current_menu() == AI_PET_MENU_MAIN) {
                handle_main_menu_navigation(key);
            } else {
                handle_sub_menu_navigation(key);
            }
            break;

        case KEY_DOWN:
            printf("DOWN key pressed - navigating down\n");
            if(menu_system_get_current_menu() == AI_PET_MENU_MAIN) {
                handle_main_menu_navigation(key);
            } else {
                handle_sub_menu_navigation(key);
            }
            break;

        case KEY_LEFT:
            printf("LEFT/A key pressed - navigating left\n");
            handle_main_menu_navigation(key);
            break;

        case KEY_RIGHT:
            printf("RIGHT/D key pressed - navigating right\n");
            handle_main_menu_navigation(key);
            break;

        case KEY_ENTER:
            if(menu_system_get_current_menu() == AI_PET_MENU_MAIN) {
                handle_menu_selection();
            } else {
                handle_sub_menu_selection();
            }
            break;

        case KEY_ESC:
            if(menu_system_get_current_menu() != AI_PET_MENU_MAIN) {
                menu_system_hide_sub_menu();
            }
            break;

        /***********************test*************************/
        case KEY_AI:
            printf("I key pressed - AI function invoked\n");
            handle_ai_function();
            break;

        // Battery icon testing keys only
        case 97: // 'a' key - Battery 0 (empty)
            printf("A key pressed - Setting battery to empty\n");
            lv_demo_ai_pocket_pet_set_battery_status(0, false);
            lv_demo_ai_pocket_pet_show_toast("Battery: Empty", 1000);
            break;

        case 115: // 's' key - Battery 1
            printf("S key pressed - Setting battery to 1 bar\n");
            lv_demo_ai_pocket_pet_set_battery_status(1, false);
            lv_demo_ai_pocket_pet_show_toast("Battery: 1 bar", 1000);
            break;

        case 100: // 'd' key - Battery 2
            printf("D key pressed - Setting battery to 2 bars\n");
            lv_demo_ai_pocket_pet_set_battery_status(2, false);
            lv_demo_ai_pocket_pet_show_toast("Battery: 2 bars", 1000);
            break;

        case 102: // 'f' key - Battery 3
            printf("F key pressed - Setting battery to 3 bars\n");
            lv_demo_ai_pocket_pet_set_battery_status(3, false);
            lv_demo_ai_pocket_pet_show_toast("Battery: 3 bars", 1000);
            break;

        case 103: // 'g' key - Battery 4
            printf("G key pressed - Setting battery to 4 bars\n");
            lv_demo_ai_pocket_pet_set_battery_status(4, false);
            lv_demo_ai_pocket_pet_show_toast("Battery: 4 bars", 1000);
            break;

        case 104: // 'h' key - Battery 5 (5 bars)
            printf("H key pressed - Setting battery to 5 bars\n");
            lv_demo_ai_pocket_pet_set_battery_status(5, false);
            lv_demo_ai_pocket_pet_show_toast("Battery: 5 bars", 1000);
            break;

        case 106: // 'j' key - Battery 6 (full)
            printf("J key pressed - Setting battery to full\n");
            lv_demo_ai_pocket_pet_set_battery_status(6, false);
            lv_demo_ai_pocket_pet_show_toast("Battery: Full", 1000);
            break;

        case 99: // 'c' key - Battery charging
            printf("C key pressed - Setting battery to charging\n");
            lv_demo_ai_pocket_pet_set_battery_status(3, true);
            lv_demo_ai_pocket_pet_show_toast("Battery: Charging", 1000);
            break;

        // Pet animation testing keys
        case 49: // '1' key - Normal state
            printf("1 key pressed - Setting pet to normal state\n");
            pet_area_set_animation(AI_PET_STATE_NORMAL);
            lv_demo_ai_pocket_pet_show_toast("Pet: Normal", 1000);
            break;

        case 50: // '2' key - Sleep
            printf("2 key pressed - Setting pet to sleep\n");
            pet_area_set_animation(AI_PET_STATE_SLEEP);
            lv_demo_ai_pocket_pet_show_toast("Pet: Sleeping", 1000);
            break;

        case 51: // '3' key - Dance
            printf("3 key pressed - Setting pet to dance\n");
            pet_area_set_animation(AI_PET_STATE_DANCE);
            lv_demo_ai_pocket_pet_show_toast("Pet: Dancing", 1000);
            break;

        case 52: // '4' key - Eat
            printf("4 key pressed - Setting pet to eat\n");
            pet_area_set_animation(AI_PET_STATE_EAT);
            lv_demo_ai_pocket_pet_show_toast("Pet: Eating", 1000);
            break;

        case 53: // '5' key - Bath
            printf("5 key pressed - Setting pet to bath\n");
            pet_area_set_animation(AI_PET_STATE_BATH);
            lv_demo_ai_pocket_pet_show_toast("Pet: Bathing", 1000);
            break;

        case 54: // '6' key - Toilet
            printf("6 key pressed - Setting pet to toilet\n");
            pet_area_set_animation(AI_PET_STATE_TOILET);
            lv_demo_ai_pocket_pet_show_toast("Pet: Toilet", 1000);
            break;

        case 55: // '7' key - Sick
            printf("7 key pressed - Setting pet to sick\n");
            pet_area_set_animation(AI_PET_STATE_SICK);
            lv_demo_ai_pocket_pet_show_toast("Pet: Sick", 1000);
            break;

        case 56: // '8' key - Happy
            printf("8 key pressed - Setting pet to happy\n");
            pet_area_set_animation(AI_PET_STATE_HAPPY);
            lv_demo_ai_pocket_pet_show_toast("Pet: Happy", 1000);
            break;

        case 57: // '9' key - Angry
            printf("9 key pressed - Setting pet to angry\n");
            pet_area_set_animation(AI_PET_STATE_ANGRY);
            lv_demo_ai_pocket_pet_show_toast("Pet: Angry", 1000);
            break;

        case 48: // '0' key - Cry
            printf("0 key pressed - Setting pet to cry\n");
            pet_area_set_animation(AI_PET_STATE_CRY);
            lv_demo_ai_pocket_pet_show_toast("Pet: Crying", 1000);
            break;

        default:
            printf("Unhandled key: %d\n", key);
            if(key > 0) {
                printf("Key press detected but not handled: %d\n", key);
            }
            break;
    }
}

// Public API functions - delegating to appropriate modules

void lv_demo_ai_pocket_pet_show_toast(const char *message, uint32_t delay_ms)
{
    printf("Showing toast: '%s' for %d ms\n", message, delay_ms);
    toast_show(message, delay_ms);
}

void lv_demo_ai_pocket_pet_hide_toast(void)
{
    toast_hide();
}

void lv_demo_ai_pocket_pet_set_wifi_strength(uint8_t strength)
{
    status_bar_set_wifi_strength(strength);
}

void lv_demo_ai_pocket_pet_set_cellular_status(uint8_t strength, bool connected)
{
    status_bar_set_cellular_status(strength, connected);
}

uint8_t lv_demo_ai_pocket_pet_get_wifi_strength(void)
{
    return status_bar_get_wifi_strength();
}

uint8_t lv_demo_ai_pocket_pet_get_cellular_strength(void)
{
    return status_bar_get_cellular_strength();
}

bool lv_demo_ai_pocket_pet_get_cellular_connected(void)
{
    return status_bar_get_cellular_connected();
}

void lv_demo_ai_pocket_pet_set_battery_status(uint8_t level, bool charging)
{
    status_bar_set_battery_status(level, charging);
}

uint8_t lv_demo_ai_pocket_pet_get_battery_level(void)
{
    return status_bar_get_battery_level();
}

bool lv_demo_ai_pocket_pet_get_battery_charging(void)
{
    return status_bar_get_battery_charging();
}

lv_obj_t* lv_demo_ai_pocket_pet_get_main_screen(void)
{
    return g_app_data.screen;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Initializes the app data structure with default values
 */
static void init_app_data(void)
{
    memset(&g_app_data, 0, sizeof(ai_pet_app_t));
}

/**
 * Creates and configures the main screen
 */
static void create_main_screen(void)
{
    g_app_data.screen = lv_obj_create(NULL);
    lv_obj_set_size(g_app_data.screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(g_app_data.screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_app_data.screen, LV_OPA_COVER, 0);
    // Note: We don't load this screen immediately - it will be loaded by the timer

    // // Add keyboard event handler to the screen
#if LVGL_SIMULATOR
    lv_obj_add_event_cb(g_app_data.screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
#endif
    // Make sure the screen can receive keyboard focus
    lv_group_add_obj(lv_group_get_default(), g_app_data.screen);

    // Add horizontal line across the screen, 3px thick, positioned 1/3 from bottom
    lv_obj_t *horizontal_line = lv_obj_create(g_app_data.screen);
    lv_obj_set_size(horizontal_line, AI_PET_SCREEN_WIDTH, 2);
    lv_obj_align(horizontal_line, LV_ALIGN_TOP_LEFT, 0, 112); // 168 * (2/3) = 112 pixels from top
    lv_obj_set_style_bg_color(horizontal_line, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(horizontal_line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(horizontal_line, 0, 0);
    lv_obj_set_style_pad_all(horizontal_line, 0, 0);
}

/**
 * Keyboard event handler
 */
#if LVGL_SIMULATOR
static void keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);  // Get event code
    printf("Keyboard event received: code=%d\n", code);

    uint32_t key = lv_event_get_key(e);
    printf("Keyboard event received: key=%d\n", key);
    lv_demo_ai_pocket_pet_handle_input(key);
}
#endif
/**
 * Handles main menu navigation (up/down/left/right)
 */
static void handle_main_menu_navigation(uint32_t key)
{
    menu_system_handle_main_navigation(key);
}

/**
 * Handles sub menu navigation (up/down)
 */
static void handle_sub_menu_navigation(uint32_t key)
{
    menu_system_handle_sub_navigation(key);
}

/**
 * Handles main menu selection (ENTER key)
 */
static void handle_menu_selection(void)
{
    menu_system_handle_main_selection();
}

/**
 * Handles sub menu selection (ENTER key)
 */
static void handle_sub_menu_selection(void)
{
    menu_system_handle_sub_selection();
}

/**
 * Handles AI function (I key)
 */
void handle_ai_function(void)
{
    if(menu_system_get_current_menu() == AI_PET_MENU_MAIN) {
        // Show toast message to confirm AI action
        lv_demo_ai_pocket_pet_show_toast("Speech ASR AI", 2000);
    }
}
