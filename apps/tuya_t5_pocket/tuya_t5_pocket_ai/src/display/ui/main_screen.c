/**
 * @file main_screen.c
 * @brief Implementation of the main screen for the application
 *
 * This file contains the implementation of the main screen which displays
 * the main AI Pocket Pet interface including status bar, pet area, and
 * menu system. This is the primary screen after the startup sequence.
 *
 * The main screen includes:
 * - A white background main screen
 * - Status bar with network and battery indicators
 * - Pet area with animated pet character
 * - Bottom menu system for navigation
 * - Sub-menu overlay system
 * - Toast notification system
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "main_screen.h"
#include "toast_screen.h"
#include "menu_bath_screen.h"
#include "menu_food_screen.h"
#include "menu_health_screen.h"
#include "menu_info_screen.h"
#include "menu_scan_screen.h"
#include "menu_sleep_screen.h"
#include "menu_video_screen.h"
#include "standby_screen.h"
#include "ebook_screen.h"
#include "rfid_scan_screen.h"
#include "camera_screen.h"
#include "game_pet.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#if defined(ENABLE_LVGL_HARDWARE)
#include "axp2101_driver.h"
#include "tal_system.h"
#include "tal_kv.h"
#include "netmgr.h"
#endif
/***********************************************************
***********************Type Definitions********************
***********************************************************/

/***********************************************************
***********************Image Declarations*****************
***********************************************************/

// External image declarations for menu icons
LV_IMG_DECLARE(info_icon);   // From icons/menu_info_icon.c
LV_IMG_DECLARE(eat_icon);    // From icons/menu_eat_icon.c
LV_IMG_DECLARE(toilet_icon); // From icons/menu_toilet_icon.c
LV_IMG_DECLARE(sick_icon);   // From icons/menu_sick_icon.c
LV_IMG_DECLARE(sleep_icon);  // From icons/menu_sleep_icon.c
LV_IMG_DECLARE(camera_icon); // From icons/menu_camera_icon.c
LV_IMG_DECLARE(scan_icon);   // From icons/menu_scan_icon.c

// Ducky animations
LV_IMG_DECLARE(ducky_walk); // From ducky/ducky_walk.c
LV_IMG_DECLARE(ducky_game); // From ducky/ducky_game.c
LV_IMG_DECLARE(ducky_walk_to_left);
LV_IMG_DECLARE(ducky_blink);
LV_IMG_DECLARE(ducky_stand_still);
LV_IMG_DECLARE(ducky_sleep);
LV_IMG_DECLARE(ducky_dance);
LV_IMG_DECLARE(ducky_eat);
LV_IMG_DECLARE(ducky_bath);
LV_IMG_DECLARE(ducky_toilet);
LV_IMG_DECLARE(ducky_sick);
LV_IMG_DECLARE(ducky_emotion_happy);
LV_IMG_DECLARE(ducky_emotion_angry);
LV_IMG_DECLARE(ducky_emotion_cry);

// Status bar icons declarations
LV_IMG_DECLARE(wifi_1_bar_icon);
LV_IMG_DECLARE(wifi_2_bar_icon);
LV_IMG_DECLARE(wifi_3_bar_icon);
LV_IMG_DECLARE(wifi_off_icon);
LV_IMG_DECLARE(wifi_find_icon);
LV_IMG_DECLARE(wifi_add_icon);
// LV_IMG_DECLARE(four_g_logo_icon);
// LV_IMG_DECLARE(cellular_1_bar_icon);
// LV_IMG_DECLARE(cellular_2_bar_icon);
// LV_IMG_DECLARE(cellular_3_bar_icon);
LV_IMG_DECLARE(four_g_icon);
LV_IMG_DECLARE(cellular_off_icon);
LV_IMG_DECLARE(cellular_connected_no_internet_icon);
LV_IMG_DECLARE(battery_0_icon);
LV_IMG_DECLARE(battery_1_icon);
LV_IMG_DECLARE(battery_2_icon);
LV_IMG_DECLARE(battery_3_icon);
LV_IMG_DECLARE(battery_4_icon);
LV_IMG_DECLARE(battery_5_icon);
LV_IMG_DECLARE(battery_full_icon);
LV_IMG_DECLARE(battery_charging_icon);

/***********************************************************
***********************defines****************************
***********************************************************/

#ifndef AI_PET_SCREEN_WIDTH
#define AI_PET_SCREEN_WIDTH 384
#endif
#ifndef AI_PET_SCREEN_HEIGHT
#define AI_PET_SCREEN_HEIGHT 168
#endif

// Pet animation constants - Optimized for better performance
#define PET_ANIMATION_INTERVAL        100
#define PET_MOVEMENT_INTERVAL         200
#define PET_MOVEMENT_STEP             2
#define PET_MOVEMENT_LIMIT            80
#define PET_WALK_DURATION_MIN         2000
#define PET_WALK_DURATION_MAX         8000
#define PET_IDLE_DURATION_MIN         3000
#define PET_IDLE_DURATION_MAX         10000
#define PET_IDLE_ANIMATION_SWITCH_MIN 4000
#define PET_IDLE_ANIMATION_SWITCH_MAX 12000

#define MENU_BUTTON_COUNT       7
#define BATTERY_UPDATE_INTERVAL 1000
#define UI_UPDATE_INTERVAL      100
#define STANDBY_TIME            30 // Seconds of inactivity before standby
#define PET_NAME_KV_KEY         "pet_name"

// Font definitions - easily customizable
#define SCREEN_TITLE_FONT   &lv_font_terminusTTF_Bold_18
#define SCREEN_CONTENT_FONT &lv_font_terminusTTF_Bold_16
#define SCREEN_INFO_FONT    &lv_font_terminusTTF_Bold_14

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_main_screen;

// Main screen UI components
static lv_obj_t *status_bar;
static lv_obj_t *pet_area;
static lv_obj_t *bottom_menu;
static lv_obj_t *horizontal_line;

// Menu system
static lv_obj_t *menu_buttons[MENU_BUTTON_COUNT];
static uint8_t current_selected_button = 0;

// Status bar components
static lv_obj_t *wifi_icon;
// static lv_obj_t *four_g_logo_obj;
static lv_obj_t *cellular_icon;
static lv_obj_t *battery_icon;
static lv_obj_t *battery_label; // Battery info label

// Status tracking
static uint8_t current_wifi_strength = 0;
static uint8_t current_cellular_strength = 2;
static uint8_t current_cellular_connected = true;
static uint8_t current_battery_level = 4;
static bool current_battery_charging = false;

// Pet area variables - Pre-loaded GIF approach (like pet_area.c)
static lv_obj_t *gif_container = NULL; // Container for GIF widgets

// Normal state animation objects (for walking behavior)
static lv_obj_t *pet_image_walk = NULL;
static lv_obj_t *pet_image_walk_left = NULL;
static lv_obj_t *pet_image_blink = NULL;
static lv_obj_t *pet_image_stand = NULL;
static lv_obj_t *current_normal_image = NULL; // Points to the currently active normal state image

// Special state animation objects (pre-loaded to prevent black screen)
static lv_obj_t *pet_image_sleep = NULL;
static lv_obj_t *pet_image_dance = NULL;
static lv_obj_t *pet_image_eat = NULL;
static lv_obj_t *pet_image_bath = NULL;
static lv_obj_t *pet_image_toilet = NULL;
static lv_obj_t *pet_image_sick = NULL;
static lv_obj_t *pet_image_happy = NULL;
static lv_obj_t *pet_image_angry = NULL;
static lv_obj_t *pet_image_cry = NULL;
static lv_obj_t *current_special_image = NULL; // Points to the currently active special state image

// Pet animation state
static ai_pet_state_t current_animation_state = AI_PET_STATE_NORMAL;
static lv_timer_t *pet_animation_timer = NULL;
static lv_timer_t *pet_movement_timer = NULL;
static int16_t pet_x_pos = 0;
static int8_t pet_direction = 1;
static uint32_t pet_state_timer = 0;
static uint32_t pet_state_duration = 0;
static bool pet_is_walking = false;
static uint8_t idle_animation_state = 1;

// UI update timer (unified update for all UI elements)
static lv_timer_t *ui_update_timer = NULL;

// Performance optimization
static int16_t last_pet_x_pos = 0;

// Idle animation timing
static uint32_t idle_animation_timer = 0;
static uint32_t idle_animation_duration = 0;

// Pet event callback system
static pet_event_callback_t pet_event_callback = NULL;
static void *pet_event_user_data = NULL;

// Pet stats
static pet_stats_t main_screen_pet_stats;

// Standby mode timer
static uint16_t standby_time = 0;

Screen_t main_screen = {
    .init = main_screen_init,
    .deinit = main_screen_deinit,
    .screen_obj = &ui_main_screen,
    .name = "Main",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void keyboard_event_cb(lv_event_t *e);
static void create_main_ui_components(void);

// Menu system functions
static void update_menu_button_selection(uint8_t old_selection, uint8_t new_selection);
static void handle_menu_navigation(uint32_t key);
static void handle_menu_selection(void);
static lv_obj_t *create_bottom_menu(lv_obj_t *parent);
static void handle_main_navigation(uint32_t key);
static uint8_t get_selected_button(void);

// UI component creation functions
static lv_obj_t *simple_status_bar_create(lv_obj_t *parent);
static lv_obj_t *simple_pet_area_create(lv_obj_t *parent);
static void simple_pet_area_start_animation(void);
static void simple_pet_area_stop_animation(void);

// Status bar icon helper functions (inline for performance)
static inline const lv_img_dsc_t *get_wifi_icon_by_strength(uint8_t strength);
static inline const lv_img_dsc_t *get_cellular_icon_by_strength(uint8_t strength, uint8_t connected);
static inline const lv_img_dsc_t *get_battery_icon_by_level(uint8_t level, bool charging);

// Pet animation functions
static const lv_img_dsc_t *get_gif_src_by_state(ai_pet_state_t state, bool is_walking, int8_t direction,
                                                uint8_t idle_state);
static void switch_pet_animation(lv_obj_t *target_image);
// static void switch_to_special_animation(lv_obj_t* target_special_image);
// static void switch_to_normal_animation(void);
static void pet_animation_cb(lv_timer_t *timer);
static void pet_movement_cb(lv_timer_t *timer);

// UI update timer callback
static void ui_update_timer_cb(lv_timer_t *timer);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the main screen.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    if (e == NULL) {
        return;
    }

    standby_time = 0; // Reset standby timer on any key event

    // lv_event_code_t code = lv_event_get_code(e);

    uint32_t key = lv_event_get_key(e);

    // Simple key handling with pet animation controls and menu navigation
    switch (key) {
    case KEY_UP:
        printf("[%s] Keyboard event: UP\n", main_screen.name);
        // UP key works like LEFT key - navigate to previous icon
        handle_menu_navigation(key);
        break;
    case KEY_DOWN:
        printf("[%s] Keyboard event: DOWN\n", main_screen.name);
        // DOWN key works like RIGHT key - navigate to next icon
        handle_menu_navigation(key);
        break;
    case KEY_LEFT:
        printf("[%s] Keyboard event: LEFT\n", main_screen.name);
        // Always handle menu navigation for LEFT/RIGHT keys
        handle_menu_navigation(key);
        break;
    case KEY_RIGHT:
        printf("[%s] Keyboard event: RIGHT\n", main_screen.name);
        // Always handle menu navigation for LEFT/RIGHT keys
        handle_menu_navigation(key);
        break;
    case KEY_ENTER:
        printf("[%s] Keyboard event: ENTER\n", main_screen.name);
        // ENTER always triggers menu selection
        handle_menu_selection();
        break;
    case KEY_ESC:
        printf("[%s] Keyboard event: ESC\n", main_screen.name);
        // ESC shows help message
        // toast_screen_show("Use LEFT/RIGHT to select, ENTER to confirm", 2000);
        break;
    case KEY_JOYCON: {
#if defined(ENABLE_LVGL_HARDWARE)
        // uint8_t chat_text[] = "Tell me today's weather and tell me a new story";
        // toast_screen_show("Tell you a new story", 1000);
        // ai_text_agent_upload(chat_text, sizeof(chat_text));
        screen_load(&camera_screen);
#else
        toast_screen_show("Unlock at Higher Level", 2000);
#endif
        break;
    }
#if !defined(ENABLE_LVGL_HARDWARE)
    // Pet event testing keys (demonstrate pet event callback system)
    case 116: // 't' key - Test pet event: eating
        printf("T key pressed - Testing pet event: eating\n");
        main_screen_handle_pet_event(PET_EVENT_FEED_HAMBURGER);
        break;
    case 121: // 'y' key - Test pet event: sleeping
        printf("Y key pressed - Testing pet event: sleeping\n");
        main_screen_handle_pet_event(PET_EVENT_SLEEP);
        break;
    case 117: // 'u' key - Test pet event: wake up
        printf("U key pressed - Testing pet event: wake up\n");
        main_screen_handle_pet_event(PET_EVENT_WAKE_UP);
        break;
    case 105: // 'i' key - Test pet event: bath
        printf("I key pressed - Testing pet event: bath\n");
        main_screen_handle_pet_event(PET_EVENT_TAKE_BATH);
        break;
    case 111: // 'o' key - Test pet event: toilet
        printf("O key pressed - Testing pet event: toilet\n");
        main_screen_handle_pet_event(PET_EVENT_TOILET);
        break;
    case 112: // 'p' key - Test pet event: randomize stats
        printf("P key pressed - Testing pet event: randomize stats\n");
        main_screen_handle_pet_event(PET_STAT_RANDOMIZE);
        break;

    // Pet animation testing keys
    case 49: // '1' key - Normal state
        printf("1 key pressed - Setting pet to normal state\n");
        main_screen_set_pet_animation_state(AI_PET_STATE_NORMAL);
        break;
    case 50: // '2' key - Sleep
        printf("2 key pressed - Setting pet to sleep\n");
        main_screen_set_pet_animation_state(AI_PET_STATE_SLEEP);
        break;
    case 51: // '3' key - Dance
        printf("3 key pressed - Setting pet to dance\n");
        main_screen_set_pet_animation_state(AI_PET_STATE_DANCE);
        break;
    case 52: // '4' key - Eat
        printf("4 key pressed - Setting pet to eat\n");
        main_screen_set_pet_animation_state(AI_PET_STATE_EAT);
        break;
    case 53: // '5' key - Bath
        printf("5 key pressed - Setting pet to bath\n");
        main_screen_set_pet_animation_state(AI_PET_STATE_BATH);
        break;
    case 54: // '6' key - Toilet
        printf("6 key pressed - Setting pet to toilet\n");
        main_screen_set_pet_animation_state(AI_PET_STATE_TOILET);
        break;
    case 55: // '7' key - Sick
        printf("7 key pressed - Setting pet to sick\n");
        main_screen_set_pet_animation_state(AI_PET_STATE_SICK);
        break;
    case 56: // '8' key - Happy
        printf("8 key pressed - Setting pet to happy\n");
        main_screen_set_pet_animation_state(AI_PET_STATE_HAPPY);
        break;
    case 57: // '9' key - Angry
        printf("9 key pressed - Setting pet to angry\n");
        main_screen_set_pet_animation_state(AI_PET_STATE_ANGRY);
        break;
    case 48: // '0' key - Cry
        printf("0 key pressed - Setting pet to cry\n");
        main_screen_set_pet_animation_state(AI_PET_STATE_CRY);
        break;

    // Battery testing keys
    case 97: // 'a' key - Battery 0 (empty)
        printf("A key pressed - Setting battery to empty\n");
        main_screen_set_battery_state(0, false);
        break;
    case 115: // 's' key - Battery 1
        printf("S key pressed - Setting battery to 1 bar\n");
        main_screen_set_battery_state(1, false);
        break;
    case 100: // 'd' key - Battery 2
        printf("D key pressed - Setting battery to 2 bars\n");
        main_screen_set_battery_state(2, false);
        break;
    case 102: // 'f' key - Battery 3
        printf("F key pressed - Setting battery to 3 bars\n");
        main_screen_set_battery_state(3, false);
        break;
    case 103: // 'g' key - Battery 4
        printf("G key pressed - Setting battery to 4 bars\n");
        main_screen_set_battery_state(4, false);
        break;
    case 104: // 'h' key - Battery 5
        printf("H key pressed - Setting battery to 5 bars\n");
        main_screen_set_battery_state(5, false);
        break;
    case 106: // 'j' key - Battery 6 (full)
        printf("J key pressed - Setting battery to full\n");
        main_screen_set_battery_state(6, false);
        break;
    case 99: // 'c' key - Battery charging
        printf("C key pressed - Setting battery to charging\n");
        // screen_load(&standby_screen);
        // screen_load(&ebook_screen);
        screen_load(&rfid_scan_screen);
        break;
#endif
    default:
        printf("[%s] Keyboard event: %d\n", main_screen.name, key);
        break;
    }
}

/**
 * @brief Create main UI components
 *
 * This function creates all the main UI components including status bar,
 * pet area, menu system, and toast notifications.
 */
static void create_main_ui_components(void)
{
    if (ui_main_screen == NULL) {
        printf("[%s] Error: Cannot create UI components - main screen is NULL\n", main_screen.name);
        return;
    }

    // Create UI components using simple inline implementations
    status_bar = simple_status_bar_create(ui_main_screen);
    if (status_bar == NULL) {
        printf("[%s] Warning: Failed to create status bar\n", main_screen.name);
    }

    // Create bottom menu using the real menu system FIRST (lowest layer)
    bottom_menu = create_bottom_menu(ui_main_screen);
    if (bottom_menu == NULL) {
        printf("[%s] Warning: Failed to create bottom menu\n", main_screen.name);
    }

    // Add horizontal line across the screen, 2px thick, positioned 1/3 from bottom
    horizontal_line = lv_obj_create(ui_main_screen);
    if (horizontal_line == NULL) {
        printf("[%s] Error: Failed to create horizontal line\n", main_screen.name);
    } else {
        lv_obj_set_size(horizontal_line, AI_PET_SCREEN_WIDTH, 2);
        lv_obj_align(horizontal_line, LV_ALIGN_TOP_LEFT, 0, 112); // 168 * (2/3) = 112 pixels from top
        lv_obj_set_style_bg_color(horizontal_line, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(horizontal_line, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(horizontal_line, 0, 0);
        lv_obj_set_style_pad_all(horizontal_line, 0, 0);
    }

    // Create pet area LAST so pet appears above everything else (highest layer)
    pet_area = simple_pet_area_create(ui_main_screen);
    if (pet_area == NULL) {
        printf("[%s] Warning: Failed to create pet area\n", main_screen.name);
    }

    // Start pet animation
    simple_pet_area_start_animation();
}

/**
 * @brief Initialize the main screen
 *
 * This function creates the main screen UI with a white background,
 * initializes all UI components, and sets up event handling.
 */
void main_screen_init(void)
{
    // Create main screen object
    ui_main_screen = lv_obj_create(NULL);
    if (ui_main_screen == NULL) {
        printf("[%s] Error: Failed to create main screen object\n", main_screen.name);
        return;
    }

    lv_obj_set_size(ui_main_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_main_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ui_main_screen, LV_OPA_COVER, 0);

    // Create all UI components
    create_main_ui_components();

    // Add keyboard event handler to the screen
    lv_obj_add_event_cb(ui_main_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);

    // Make sure the screen can receive keyboard focus
    lv_group_t *group = lv_group_get_default();
    if (group == NULL) {
        group = lv_group_create();
        lv_group_set_default(group);
    }
    lv_group_add_obj(group, ui_main_screen);
    lv_group_focus_obj(ui_main_screen);

    // Initialize pet stats
    main_screen_init_pet_stats(NULL);
}

/**
 * @brief Deinitialize the main screen
 *
 * This function cleans up the main screen by stopping animations,
 * deleting the UI object, and removing event callbacks.
 */
void main_screen_deinit(void)
{
    // Stop pet animation
    simple_pet_area_stop_animation();

    // Reset GIF container and image pointers (objects will be cleaned up with parent)
    gif_container = NULL;
    pet_image_walk = NULL;
    pet_image_walk_left = NULL;
    pet_image_blink = NULL;
    pet_image_stand = NULL;
    pet_image_sleep = NULL;
    pet_image_dance = NULL;
    pet_image_eat = NULL;
    pet_image_bath = NULL;
    pet_image_toilet = NULL;
    pet_image_sick = NULL;
    pet_image_happy = NULL;
    pet_image_angry = NULL;
    pet_image_cry = NULL;
    current_normal_image = NULL;
    current_special_image = NULL;

    // Remove event callback and delete the main screen object
    if (ui_main_screen) {
        // Remove from group before deleting
        lv_obj_remove_event_cb(ui_main_screen, keyboard_event_cb); // Remove event callback
        lv_group_remove_obj(ui_main_screen);                       // Remove from group
        printf("deinit main screen\n");
        // Delete the main screen object
        // lv_obj_del(ui_main_screen);
        // ui_main_screen = NULL;
    }

    // Reset component pointers
    status_bar = NULL;
    pet_area = NULL;
    bottom_menu = NULL;
    horizontal_line = NULL;

    // Reset status bar icon pointers
    wifi_icon = NULL;
    // four_g_logo_obj = NULL;
    cellular_icon = NULL;
    battery_icon = NULL;
    battery_label = NULL;

    // Reset menu system variables

    for (int i = 0; i < MENU_BUTTON_COUNT; i++) {
        menu_buttons[i] = NULL;
    }
}

/***********************************************************
******************UI Component Creation********************
***********************************************************/

static lv_obj_t *simple_status_bar_create(lv_obj_t *parent)
{
    if (parent == NULL) {
        printf("Error: Cannot create status bar - parent is NULL\n");
        return NULL;
    }

    lv_obj_t *status_bar = lv_obj_create(parent);
    if (status_bar == NULL) {
        printf("Error: Failed to create status bar object\n");
        return NULL;
    }

    lv_obj_set_size(status_bar, AI_PET_SCREEN_WIDTH, 24);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 2, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    // WiFi icon (image widget)
    wifi_icon = lv_img_create(status_bar);
    lv_obj_set_size(wifi_icon, 24, 24);
    lv_obj_align(wifi_icon, LV_ALIGN_LEFT_MID, 5, 0);

    // 4G logo icon (static, 24px)
    // four_g_logo_obj = lv_img_create(status_bar);
    // lv_obj_set_size(four_g_logo_obj, 24, 24);
    // lv_obj_align(four_g_logo_obj, LV_ALIGN_LEFT_MID, 35, 0);
    // lv_img_set_src(four_g_logo_obj, &four_g_icon);

    // Cellular signal icon
    cellular_icon = lv_img_create(status_bar);
    lv_obj_set_size(cellular_icon, 24, 24);
    lv_obj_align(cellular_icon, LV_ALIGN_LEFT_MID, 35, 0);

    // Battery info label (voltage and percentage)
    battery_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(battery_label, SCREEN_INFO_FONT, 0);
    lv_obj_set_style_text_color(battery_label, lv_color_black(), 0);
    lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, -35, 0);
    // lv_label_set_text(battery_label, "4.2V 100%");

    // Battery icon (image widget)
    battery_icon = lv_img_create(status_bar);
    lv_obj_set_size(battery_icon, 24, 24);
    lv_obj_align(battery_icon, LV_ALIGN_RIGHT_MID, -5, 0);

    return status_bar;
}

static lv_obj_t *simple_pet_area_create(lv_obj_t *parent)
{
    if (parent == NULL) {
        printf("Error: Cannot create pet area - parent is NULL\n");
        return NULL;
    }

    lv_obj_t *pet_area = lv_obj_create(parent);
    if (pet_area == NULL) {
        printf("Error: Failed to create pet area object\n");
        return NULL;
    }

    lv_obj_set_size(pet_area, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT - 24 - 26);
    lv_obj_align(pet_area, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_bg_opa(pet_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pet_area, 0, 0);
    lv_obj_set_style_pad_all(pet_area, 0, 0);
    lv_obj_clear_flag(pet_area, LV_OBJ_FLAG_SCROLLABLE);

    // Create a container for the GIF widgets (highest priority display)
    gif_container = lv_obj_create(pet_area);
    lv_obj_set_size(gif_container, 170 + 10, 170 + 10);
    lv_obj_align(gif_container, LV_ALIGN_CENTER, 0, -5);
    lv_obj_set_style_bg_opa(gif_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gif_container, 0, 0);
    lv_obj_set_style_pad_all(gif_container, 0, 0);
    lv_obj_clear_flag(gif_container, LV_OBJ_FLAG_SCROLLABLE);

    // Ensure GIF container has highest priority (always on top)
    lv_obj_move_foreground(gif_container);

    // Create all normal state GIF widgets (pre-loaded)
    // Walk right animation
    pet_image_walk = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_walk, &ducky_walk);
    lv_obj_align(pet_image_walk, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(pet_image_walk, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pet_image_walk, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_walk, LV_OPA_TRANSP, 0);

    // Walk left animation
    pet_image_walk_left = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_walk_left, &ducky_walk_to_left);
    lv_obj_align(pet_image_walk_left, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(pet_image_walk_left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pet_image_walk_left, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_walk_left, LV_OPA_TRANSP, 0);

    // Blink animation
    pet_image_blink = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_blink, &ducky_blink);
    lv_obj_align(pet_image_blink, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(pet_image_blink, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pet_image_blink, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_blink, LV_OPA_TRANSP, 0);

    // Stand animation
    pet_image_stand = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_stand, &ducky_stand_still);
    lv_obj_align(pet_image_stand, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(pet_image_stand, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pet_image_stand, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_stand, LV_OPA_TRANSP, 0);

    // Create special state animation objects (pre-loaded to prevent black screen)
    pet_image_sleep = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_sleep, &ducky_sleep);
    lv_obj_align(pet_image_sleep, LV_ALIGN_CENTER, 0, -5);
    lv_obj_clear_flag(pet_image_sleep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pet_image_sleep, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_sleep, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_sleep, LV_OBJ_FLAG_HIDDEN);

    pet_image_dance = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_dance, &ducky_dance);
    lv_obj_align(pet_image_dance, LV_ALIGN_CENTER, 0, -5);
    lv_obj_clear_flag(pet_image_dance, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pet_image_dance, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_dance, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_dance, LV_OBJ_FLAG_HIDDEN);

    pet_image_eat = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_eat, &ducky_eat);
    lv_obj_align(pet_image_eat, LV_ALIGN_CENTER, 0, -5);
    lv_obj_clear_flag(pet_image_eat, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pet_image_eat, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_eat, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_eat, LV_OBJ_FLAG_HIDDEN);

    pet_image_bath = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_bath, &ducky_bath);
    lv_obj_align(pet_image_bath, LV_ALIGN_CENTER, 0, -5);
    lv_obj_clear_flag(pet_image_bath, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pet_image_bath, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_bath, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_bath, LV_OBJ_FLAG_HIDDEN);

    pet_image_toilet = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_toilet, &ducky_toilet);
    lv_obj_align(pet_image_toilet, LV_ALIGN_CENTER, 0, -5);
    lv_obj_clear_flag(pet_image_toilet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pet_image_toilet, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_toilet, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_toilet, LV_OBJ_FLAG_HIDDEN);

    pet_image_sick = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_sick, &ducky_sick);
    lv_obj_align(pet_image_sick, LV_ALIGN_CENTER, 0, -5);
    lv_obj_clear_flag(pet_image_sick, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pet_image_sick, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_sick, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_sick, LV_OBJ_FLAG_HIDDEN);

    pet_image_happy = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_happy, &ducky_emotion_happy);
    lv_obj_align(pet_image_happy, LV_ALIGN_CENTER, 0, -5);
    lv_obj_clear_flag(pet_image_happy, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pet_image_happy, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_happy, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_happy, LV_OBJ_FLAG_HIDDEN);

    pet_image_angry = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_angry, &ducky_emotion_angry);
    lv_obj_align(pet_image_angry, LV_ALIGN_CENTER, 0, -5);
    lv_obj_clear_flag(pet_image_angry, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pet_image_angry, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_angry, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_angry, LV_OBJ_FLAG_HIDDEN);

    pet_image_cry = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_cry, &ducky_emotion_cry);
    lv_obj_align(pet_image_cry, LV_ALIGN_CENTER, 0, -5);
    lv_obj_clear_flag(pet_image_cry, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pet_image_cry, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_cry, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_cry, LV_OBJ_FLAG_HIDDEN);

    // Initialize current special image pointer
    current_special_image = NULL;

    // Set initial active image and hide others
    current_normal_image = pet_image_stand; // Default to standing
    lv_obj_add_flag(pet_image_blink, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_image_walk, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_image_walk_left, LV_OBJ_FLAG_HIDDEN);

    // Initialize pet state
    current_animation_state = AI_PET_STATE_NORMAL;
    pet_x_pos = 0;
    pet_direction = 1;
    pet_is_walking = false;
    idle_animation_state = 1;
    pet_state_timer = 0;
#if defined(ENABLE_LVGL_HARDWARE)
    pet_state_duration = PET_IDLE_DURATION_MIN + tal_system_get_random(PET_IDLE_DURATION_MAX - PET_IDLE_DURATION_MIN);
    idle_animation_timer = 0;
    idle_animation_duration = PET_IDLE_ANIMATION_SWITCH_MIN +
                              tal_system_get_random(PET_IDLE_ANIMATION_SWITCH_MAX - PET_IDLE_ANIMATION_SWITCH_MIN);
#else
    pet_state_duration = PET_IDLE_DURATION_MIN + (rand() % (PET_IDLE_DURATION_MAX - PET_IDLE_DURATION_MIN));
    idle_animation_timer = 0;
    idle_animation_duration =
        PET_IDLE_ANIMATION_SWITCH_MIN + (rand() % (PET_IDLE_ANIMATION_SWITCH_MAX - PET_IDLE_ANIMATION_SWITCH_MIN));
#endif

    return pet_area;
}

/***********************************************************
*****************Dynamic GIF Management Functions**********
***********************************************************/

/**
 * @brief Get GIF source based on pet state and animation parameters
 */
static const lv_img_dsc_t *get_gif_src_by_state(ai_pet_state_t state, bool is_walking, int8_t direction,
                                                uint8_t idle_state)
{
    switch (state) {
    case AI_PET_STATE_NORMAL:
        if (is_walking) {
            return (direction == 1) ? &ducky_walk : &ducky_walk_to_left;
        } else {
            return (idle_state == 0) ? &ducky_blink : &ducky_stand_still;
        }
    case AI_PET_STATE_SLEEP:
        return &ducky_sleep;
    case AI_PET_STATE_DANCE:
        return &ducky_dance;
    case AI_PET_STATE_EAT:
        return &ducky_eat;
    case AI_PET_STATE_BATH:
        return &ducky_bath;
    case AI_PET_STATE_TOILET:
        return &ducky_toilet;
    case AI_PET_STATE_SICK:
        return &ducky_sick;
    case AI_PET_STATE_HAPPY:
        return &ducky_emotion_happy;
    case AI_PET_STATE_ANGRY:
        return &ducky_emotion_angry;
    case AI_PET_STATE_CRY:
        return &ducky_emotion_cry;
    default:
        return &ducky_stand_still;
    }
}

/**
 * @brief Get the corresponding GIF image object based on GIF source
 * @param gif_src The GIF source descriptor
 * @return The corresponding GIF image object, or NULL if not found
 */
static lv_obj_t *get_gif_object_by_src(const lv_img_dsc_t *gif_src)
{
    if (gif_src == &ducky_walk)
        return pet_image_walk;
    if (gif_src == &ducky_walk_to_left)
        return pet_image_walk_left;
    if (gif_src == &ducky_blink)
        return pet_image_blink;
    if (gif_src == &ducky_stand_still)
        return pet_image_stand;
    if (gif_src == &ducky_sleep)
        return pet_image_sleep;
    if (gif_src == &ducky_dance)
        return pet_image_dance;
    if (gif_src == &ducky_eat)
        return pet_image_eat;
    if (gif_src == &ducky_bath)
        return pet_image_bath;
    if (gif_src == &ducky_toilet)
        return pet_image_toilet;
    if (gif_src == &ducky_sick)
        return pet_image_sick;
    if (gif_src == &ducky_emotion_happy)
        return pet_image_happy;
    if (gif_src == &ducky_emotion_angry)
        return pet_image_angry;
    if (gif_src == &ducky_emotion_cry)
        return pet_image_cry;
    return NULL;
}

/**
 * @brief Switch between normal state pet animations (walk, walk_left, blink, stand)
 * @param target_image The target normal animation image object to show
 */
static void switch_pet_animation(lv_obj_t *target_image)
{
    if (target_image == NULL) {
        return;
    }

    // Check if GIF objects are valid before operating on them
    if (pet_image_walk == NULL || pet_image_walk_left == NULL || pet_image_blink == NULL || pet_image_stand == NULL) {
        printf("[%s] Warning: GIF objects not initialized, cannot switch animation\n", main_screen.name);
        return;
    }

    // Hide all normal state animations
    lv_obj_add_flag(pet_image_walk, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_image_walk_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_image_blink, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_image_stand, LV_OBJ_FLAG_HIDDEN);

    // Show the target animation
    lv_obj_clear_flag(target_image, LV_OBJ_FLAG_HIDDEN);

    // Update current normal image pointer
    current_normal_image = target_image;

    // Play sound effect for normal state animation switch
    // game_pet_play_alert(PET_ALERT_SHORT_SELECT_TONE);
}

// Menu system integration functions
static void update_menu_button_selection(uint8_t old_selection, uint8_t new_selection)
{
    // Update visual selection in bottom menu - following menu_system.c exactly
    if (old_selection < MENU_BUTTON_COUNT && new_selection < MENU_BUTTON_COUNT) {
        lv_obj_t *old_btn = menu_buttons[old_selection];
        lv_obj_t *new_btn = menu_buttons[new_selection];

        // Reset old button style - following menu_system.c reference implementation
        if (old_btn) {
            lv_obj_set_style_bg_color(old_btn, lv_color_white(), 0);
            lv_obj_set_style_border_width(old_btn, 0, 0);
            lv_obj_set_style_shadow_width(old_btn, 0, 0);

            // Handle old button content styling (image inside button)
            lv_obj_t *old_child = lv_obj_get_child(old_btn, 0);
            if (old_child && lv_obj_check_type(old_child, &lv_image_class)) {
                // Reset image styling for unselected state
                lv_obj_set_style_img_recolor_opa(old_child, LV_OPA_TRANSP, 0);
                lv_obj_set_style_img_recolor(old_child, lv_color_black(), 0);
                lv_obj_set_style_img_opa(old_child, LV_OPA_COVER, 0);
            }
        }

        // Set new button style - following menu_system.c reference implementation
        if (new_btn) {
            lv_obj_set_style_bg_color(new_btn, lv_color_black(), 0);
            lv_obj_set_style_border_color(new_btn, lv_color_black(), 0);
            lv_obj_set_style_border_width(new_btn, 2, 0);
            lv_obj_set_style_shadow_width(new_btn, 0, 0);

            // Handle new button content styling (image inside button)
            lv_obj_t *new_child = lv_obj_get_child(new_btn, 0);
            if (new_child && lv_obj_check_type(new_child, &lv_image_class)) {
                // Invert the image colors for selected state - black becomes white, white becomes black
                lv_obj_set_style_img_recolor_opa(new_child, LV_OPA_COVER, 0);
                lv_obj_set_style_img_recolor(new_child, lv_color_white(), 0);
                lv_obj_set_style_img_opa(new_child, LV_OPA_COVER, 0);
            }
        }
    }
}

static void handle_menu_navigation(uint32_t key)
{
    // Forward navigation to the real menu system for visual feedback
    handle_main_navigation(key);
}

static void handle_menu_selection(void)
{
    // Get the currently selected button from menu system
    uint8_t selected_button = get_selected_button();

    printf("[%s] Menu selection: button %d\n", main_screen.name, selected_button);

    // Switch to corresponding screen using screen manager stack
    switch (selected_button) {
    case 0: // Info
        printf("Loading info screen\n");
        screen_load(&menu_info_screen);
        break;
    case 1: // Food
        printf("Loading food screen\n");
        screen_load(&menu_food_screen);
        break;
    case 2: // Bath
        printf("Loading bath screen\n");
        screen_load(&menu_bath_screen);
        break;
    case 3: // Health
        printf("Loading health screen\n");
        screen_load(&menu_health_screen);
        break;
    case 4: // Sleep
        printf("Loading sleep screen\n");
        screen_load(&menu_sleep_screen);
        break;
    case 5: // Video
        printf("Loading video screen\n");
        screen_load(&menu_video_screen);
        break;
    case 6: // Scan
        printf("Loading scan screen\n");
        screen_load(&menu_scan_screen);
        break;
    default:
        printf("Unknown menu selection: %d\n", selected_button);
        break;
    }
}

static void simple_pet_area_start_animation(void)
{
#if !defined(ENABLE_LVGL_HARDWARE)
    // Initialize random seed for movement (only needed for standard rand())
    srand(time(NULL));
#endif

    pet_animation_timer = lv_timer_create(pet_animation_cb, PET_ANIMATION_INTERVAL, NULL);
    pet_movement_timer = lv_timer_create(pet_movement_cb, PET_MOVEMENT_INTERVAL, NULL);

    // Create UI update timer for status bar and animation updates
    ui_update_timer = lv_timer_create(ui_update_timer_cb, UI_UPDATE_INTERVAL, NULL);
}

static void simple_pet_area_stop_animation(void)
{
    if (pet_animation_timer) {
        lv_timer_del(pet_animation_timer);
        pet_animation_timer = NULL;
    }

    if (pet_movement_timer) {
        lv_timer_del(pet_movement_timer);
        pet_movement_timer = NULL;
    }

    if (ui_update_timer) {
        lv_timer_del(ui_update_timer);
        ui_update_timer = NULL;
    }
}

/***********************************************************
***********State Setting Interface Functions***************
***********************************************************/

/**
 * @brief Set pet animation state (only updates state variable, actual GIF switching is handled by timer)
 * @param state Target animation state
 */
void main_screen_set_pet_animation_state(ai_pet_state_t state)
{
    // Skip if already in the target state
    if (current_animation_state == state) {
        return;
    }

    printf("[%s] Pet animation state changing: %d -> %d\n", main_screen.name, current_animation_state, state);

    // Play sound effect based on pet state
    switch (state) {
    case AI_PET_STATE_SLEEP:
        game_pet_play_alert(PET_ALERT_CANCEL_FAIL_TRI_TONE);
        break;
    case AI_PET_STATE_DANCE:
        game_pet_play_alert(PET_ALERT_CANCEL_FAIL_TRI_TONE);
        break;
    case AI_PET_STATE_EAT:
        game_pet_play_alert(PET_ALERT_SHORT_SELECT_TONE);
        break;
    case AI_PET_STATE_BATH:
        game_pet_play_alert(PET_ALERT_FAIL_CANCEL_BI_TONE);
        break;
    case AI_PET_STATE_TOILET:
        game_pet_play_alert(PET_ALERT_FAIL_CANCEL_BI_TONE);
        break;
    case AI_PET_STATE_SICK:
        game_pet_play_alert(PET_ALERT_LOADING_TONE);
        break;
    case AI_PET_STATE_HAPPY:
        game_pet_play_alert(PET_ALERT_SHORT_SELECT_TONE);
        break;
    case AI_PET_STATE_ANGRY:
        game_pet_play_alert(PET_ALERT_THREE_STAGE_UP_TONE);
        break;
    case AI_PET_STATE_CRY:
        game_pet_play_alert(PET_ALERT_THREE_STAGE_UP_TONE);
        break;
    default:
        // No sound for AI_PET_STATE_NORMAL
        break;
    }

    // Simply update the state variable - timer will handle GIF switching
    current_animation_state = state;
}

/**
 * @brief Set WiFi signal strength state (state will be updated in next timer cycle)
 * @param strength WiFi signal strength (0-5)
 */
void main_screen_set_wifi_state(uint8_t strength)
{
    current_wifi_strength = strength;
    printf("[%s] WiFi strength set to: %d\n", main_screen.name, strength);
}

/**
 * @brief Set battery state (state will be updated in next timer cycle)
 * @param level Battery level (0-6)
 * @param charging Battery charging status
 */
void main_screen_set_battery_state(uint8_t level, bool charging)
{
    current_battery_level = level;
    current_battery_charging = charging;
    printf("[%s] Battery state set to: level=%d, charging=%d\n", main_screen.name, level, charging);
}

/***********************************************************
******************Status Bar Icon Helpers******************
***********************************************************/
static inline const lv_img_dsc_t *get_wifi_icon_by_strength(uint8_t strength)
{
    switch (strength) {
    case 1:
        return &wifi_1_bar_icon;
    case 2:
        return &wifi_2_bar_icon;
    case 3:
        return &wifi_3_bar_icon;
    case 4:
        return &wifi_find_icon;
    case 5:
        return &wifi_add_icon;
    default:
        return &wifi_off_icon;
    }
}

static inline const lv_img_dsc_t *get_cellular_icon_by_strength(uint8_t strength, uint8_t connected)
{
    // if (strength == 0) return &cellular_off_icon;
    // if (strength == 4 || !connected) return &cellular_connected_no_internet_icon;

    // switch (strength) {
    //     case 1: return &cellular_1_bar_icon;
    //     case 2: return &cellular_2_bar_icon;
    //     case 3: return &cellular_3_bar_icon;
    //     default: return &cellular_off_icon;
    // }

    if (connected)
        return &four_g_icon;
    else
        return &cellular_off_icon;
}

static inline const lv_img_dsc_t *get_battery_icon_by_level(uint8_t level, bool charging)
{
    if (charging)
        return &battery_charging_icon;

    switch (level) {
    case 0:
        return &battery_0_icon;
    case 1:
        return &battery_1_icon;
    case 2:
        return &battery_2_icon;
    case 3:
        return &battery_3_icon;
    case 4:
        return &battery_4_icon;
    case 5:
        return &battery_5_icon;
    case 6:
        return &battery_full_icon;
    default:
        return &battery_full_icon;
    }
}

/***********************************************************
*****************UI Update Timer Callback******************
***********************************************************/

/**
 * @brief UI update timer callback - updates all UI elements based on current state
 */
static void ui_update_timer_cb(lv_timer_t *timer)
{
    if (standby_time++ > STANDBY_TIME * 1000 / UI_UPDATE_INTERVAL) {
        // Enter standby mode
        printf("[%s] Entering standby mode due to inactivity\n", main_screen.name);
        screen_load(&standby_screen);
        standby_time = 0;
    }

    // Update WiFi icon if changed
    if (wifi_icon) {
        lv_img_set_src(wifi_icon, get_wifi_icon_by_strength(current_wifi_strength));
    }

    // Update cellular icon if changed
    if (cellular_icon) {
#ifdef ENABLE_LVGL_HARDWARE
        netmgr_status_e status;
        OPERATE_RET result = netmgr_conn_get(NETCONN_CELLULAR, NETCONN_CMD_STATUS, &status);
        if (result == OPRT_OK) {
            current_cellular_connected = status;
        }
#endif
        lv_img_set_src(cellular_icon,
                       get_cellular_icon_by_strength(current_cellular_strength, current_cellular_connected));
    }

#if defined(ENABLE_LVGL_HARDWARE)
    // Read from hardware
    uint16_t voltage_mv = axp2101_getBattVoltage();
    uint8_t battery_percent = axp2101_getBatteryPercent();
    current_battery_charging = axp2101_isCharging();

    // Update state
    current_battery_level = (uint8_t)(battery_percent / 100.0f * 7);
    if (current_battery_level > 6)
        current_battery_level = 6;

    // Update label using snprintf
    if (battery_label) {
        char battery_text[16];
        snprintf(battery_text, sizeof(battery_text), "%.1fV %d%%", ((float)voltage_mv) / 1000.0f, battery_percent);
        lv_label_set_text(battery_label, battery_text);
    }
#else
    // PC simulator mode - update label based on current state
    if (battery_label) {
        char battery_text[20];
        int demo_percent = current_battery_level * 100 / 7;
        float demo_voltage = 3.0f + (current_battery_level * 1.2f / 6);
        if (current_battery_charging) {
            snprintf(battery_text, sizeof(battery_text), "%.1fV %d%% CHG", demo_voltage, demo_percent);
        } else {
            snprintf(battery_text, sizeof(battery_text), "%.1fV %d%%", demo_voltage, demo_percent);
        }
        lv_label_set_text(battery_label, battery_text);
    }
#endif
    if (battery_icon) {
        lv_img_set_src(battery_icon, get_battery_icon_by_level(current_battery_level, current_battery_charging));
    }
}

// Pet animation functions
static void pet_animation_cb(lv_timer_t *timer)
{
    // This timer is responsible for switching GIF display based on current_animation_state
    // Check if main screen is initialized and GIF objects are valid
    if (ui_main_screen == NULL || gif_container == NULL || pet_image_walk == NULL || pet_image_stand == NULL) {
        return;
    }

    // Determine which GIF should be displayed based on current state
    lv_obj_t *target_image = NULL;

    if (current_animation_state == AI_PET_STATE_NORMAL) {
        // Normal state - choose based on walking state and direction
        const lv_img_dsc_t *gif_src =
            get_gif_src_by_state(AI_PET_STATE_NORMAL, pet_is_walking, pet_direction, idle_animation_state);
        target_image = get_gif_object_by_src(gif_src);
    } else {
        // Special state - get the corresponding special animation
        const lv_img_dsc_t *gif_src = get_gif_src_by_state(current_animation_state, false, 1, 0);
        target_image = get_gif_object_by_src(gif_src);
    }

    // If target image is different from currently visible image, switch
    if (target_image != NULL) {
        bool need_switch = false;

        if (current_animation_state == AI_PET_STATE_NORMAL) {
            // Check if we need to switch normal animations
            if (current_normal_image != target_image) {
                need_switch = true;
            }
        } else {
            // Check if we need to switch to/from special animations
            if (current_special_image != target_image) {
                need_switch = true;
            }
        }

        if (need_switch) {
            // Hide all GIF objects
            if (pet_image_walk)
                lv_obj_add_flag(pet_image_walk, LV_OBJ_FLAG_HIDDEN);
            if (pet_image_walk_left)
                lv_obj_add_flag(pet_image_walk_left, LV_OBJ_FLAG_HIDDEN);
            if (pet_image_blink)
                lv_obj_add_flag(pet_image_blink, LV_OBJ_FLAG_HIDDEN);
            if (pet_image_stand)
                lv_obj_add_flag(pet_image_stand, LV_OBJ_FLAG_HIDDEN);
            if (pet_image_sleep)
                lv_obj_add_flag(pet_image_sleep, LV_OBJ_FLAG_HIDDEN);
            if (pet_image_dance)
                lv_obj_add_flag(pet_image_dance, LV_OBJ_FLAG_HIDDEN);
            if (pet_image_eat)
                lv_obj_add_flag(pet_image_eat, LV_OBJ_FLAG_HIDDEN);
            if (pet_image_bath)
                lv_obj_add_flag(pet_image_bath, LV_OBJ_FLAG_HIDDEN);
            if (pet_image_toilet)
                lv_obj_add_flag(pet_image_toilet, LV_OBJ_FLAG_HIDDEN);
            if (pet_image_sick)
                lv_obj_add_flag(pet_image_sick, LV_OBJ_FLAG_HIDDEN);
            if (pet_image_happy)
                lv_obj_add_flag(pet_image_happy, LV_OBJ_FLAG_HIDDEN);
            if (pet_image_angry)
                lv_obj_add_flag(pet_image_angry, LV_OBJ_FLAG_HIDDEN);
            if (pet_image_cry)
                lv_obj_add_flag(pet_image_cry, LV_OBJ_FLAG_HIDDEN);

            // Show the target image
            lv_obj_clear_flag(target_image, LV_OBJ_FLAG_HIDDEN);

            // Update current image pointers
            if (current_animation_state == AI_PET_STATE_NORMAL) {
                current_normal_image = target_image;
                current_special_image = NULL;
            } else {
                current_special_image = target_image;
                current_normal_image = NULL;
            }
        }
    }
}

static void pet_movement_cb(lv_timer_t *timer)
{
    // Only handle movement and state changes in normal state
    if (current_animation_state != AI_PET_STATE_NORMAL) {
        return;
    }

    // Update state timer
    pet_state_timer += PET_MOVEMENT_INTERVAL;

    // Handle idle animation switching when not walking
    if (!pet_is_walking) {
        idle_animation_timer += PET_MOVEMENT_INTERVAL;

        // Check if it's time to switch idle animations
        if (idle_animation_timer >= idle_animation_duration) {
            // Toggle between blink and stand animations
            idle_animation_state = 1 - idle_animation_state;

            const lv_img_dsc_t *new_gif_src =
                get_gif_src_by_state(AI_PET_STATE_NORMAL, false, pet_direction, idle_animation_state);
            lv_obj_t *target_image = get_gif_object_by_src(new_gif_src);
            if (target_image != NULL) {
                switch_pet_animation(target_image);
            }

            // Reset idle animation timer and set new duration
            idle_animation_timer = 0;
#if defined(ENABLE_LVGL_HARDWARE)
            idle_animation_duration =
                PET_IDLE_ANIMATION_SWITCH_MIN +
                tal_system_get_random(PET_IDLE_ANIMATION_SWITCH_MAX - PET_IDLE_ANIMATION_SWITCH_MIN);
#else
            idle_animation_duration = PET_IDLE_ANIMATION_SWITCH_MIN +
                                      (rand() % (PET_IDLE_ANIMATION_SWITCH_MAX - PET_IDLE_ANIMATION_SWITCH_MIN));
#endif
        }
    }

    // Check if it's time to change state (walking vs idle)
    if (pet_state_timer >= pet_state_duration) {
        // Switch between walking and idle
        pet_is_walking = !pet_is_walking;

        if (pet_is_walking) {
            // Start walking - choose random direction and duration
#if defined(ENABLE_LVGL_HARDWARE)
            pet_direction = tal_system_get_random(2) ? 1 : -1;
            pet_state_duration =
                PET_WALK_DURATION_MIN + tal_system_get_random(PET_WALK_DURATION_MAX - PET_WALK_DURATION_MIN);
#else
            pet_direction = (rand() % 2) ? 1 : -1;
            pet_state_duration = PET_WALK_DURATION_MIN + (rand() % (PET_WALK_DURATION_MAX - PET_WALK_DURATION_MIN));
#endif

            // Set appropriate animation based on direction
            const lv_img_dsc_t *new_gif_src =
                get_gif_src_by_state(AI_PET_STATE_NORMAL, true, pet_direction, idle_animation_state);
            lv_obj_t *target_image = get_gif_object_by_src(new_gif_src);
            if (target_image != NULL) {
                switch_pet_animation(target_image);
            }
        } else {
            // Start idle - choose random duration and return to current idle animation
#if defined(ENABLE_LVGL_HARDWARE)
            pet_state_duration =
                PET_IDLE_DURATION_MIN + tal_system_get_random(PET_IDLE_DURATION_MAX - PET_IDLE_DURATION_MIN);
#else
            pet_state_duration = PET_IDLE_DURATION_MIN + (rand() % (PET_IDLE_DURATION_MAX - PET_IDLE_DURATION_MIN));
#endif

            // Reset idle animation timers when starting new idle period
            idle_animation_timer = 0;
#if defined(ENABLE_LVGL_HARDWARE)
            idle_animation_duration =
                PET_IDLE_ANIMATION_SWITCH_MIN +
                tal_system_get_random(PET_IDLE_ANIMATION_SWITCH_MAX - PET_IDLE_ANIMATION_SWITCH_MIN);
#else
            idle_animation_duration = PET_IDLE_ANIMATION_SWITCH_MIN +
                                      (rand() % (PET_IDLE_ANIMATION_SWITCH_MAX - PET_IDLE_ANIMATION_SWITCH_MIN));
#endif

            // Start with current idle animation state
            const lv_img_dsc_t *new_gif_src =
                get_gif_src_by_state(AI_PET_STATE_NORMAL, false, pet_direction, idle_animation_state);
            lv_obj_t *target_image = get_gif_object_by_src(new_gif_src);
            if (target_image != NULL) {
                switch_pet_animation(target_image);
            }
        }

        pet_state_timer = 0;
    }

    // Move pet if walking
    if (pet_is_walking) {
        pet_x_pos += pet_direction * PET_MOVEMENT_STEP;

        // Bounce off boundaries
        if (pet_x_pos > PET_MOVEMENT_LIMIT) {
            pet_x_pos = PET_MOVEMENT_LIMIT;
            pet_direction = -1;
            const lv_img_dsc_t *new_gif_src =
                get_gif_src_by_state(AI_PET_STATE_NORMAL, true, pet_direction, idle_animation_state);
            lv_obj_t *target_image = get_gif_object_by_src(new_gif_src);
            if (target_image != NULL) {
                switch_pet_animation(target_image);
            }
        } else if (pet_x_pos < -PET_MOVEMENT_LIMIT) {
            pet_x_pos = -PET_MOVEMENT_LIMIT;
            pet_direction = 1;
            const lv_img_dsc_t *new_gif_src =
                get_gif_src_by_state(AI_PET_STATE_NORMAL, true, pet_direction, idle_animation_state);
            lv_obj_t *target_image = get_gif_object_by_src(new_gif_src);
            if (target_image != NULL) {
                switch_pet_animation(target_image);
            }
        }
    }

    // Update pet position - move the container
    // Optimization: Only update position if it actually changed
    if (pet_x_pos != last_pet_x_pos && gif_container) {
        lv_obj_set_x(gif_container, pet_x_pos);
        last_pet_x_pos = pet_x_pos;
    }
}

/***********************************************************
*****************Menu System Functions*********************
***********************************************************/

/**
 * @brief Create the bottom menu with navigation buttons
 */
static lv_obj_t *create_bottom_menu(lv_obj_t *parent)
{
// Define constants to match menu_system.c
#define BOTTOM_MENU_HEIGHT  26
#define MENU_BUTTON_SIZE    24
#define MENU_BUTTON_SPACING 30
#define MENU_BUTTON_START_X (AI_PET_SCREEN_WIDTH - 195)

    // Create bottom menu container - transparent like menu_system.c
    lv_obj_t *bottom_container = lv_obj_create(parent);
    lv_obj_set_size(bottom_container, AI_PET_SCREEN_WIDTH, BOTTOM_MENU_HEIGHT);
    lv_obj_align(bottom_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(bottom_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottom_container, 0, 0);
    lv_obj_set_style_pad_all(bottom_container, 2, 0);

    // Menu icons array - same order as menu_system.c
    const lv_img_dsc_t *menu_icons[] = {&info_icon,  &eat_icon,    &toilet_icon, &sick_icon,
                                        &sleep_icon, &camera_icon, &scan_icon};

    // Create menu buttons exactly like menu_system.c
    for (uint8_t i = 0; i < MENU_BUTTON_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(bottom_container);
        lv_obj_set_size(btn, MENU_BUTTON_SIZE, MENU_BUTTON_SIZE);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -(MENU_BUTTON_START_X - i * MENU_BUTTON_SPACING), 0);

        // Set default button style like menu_system.c
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 3, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, 0);

        // Create icon image inside button like menu_system.c
        lv_obj_t *img = lv_img_create(btn);
        lv_img_set_src(img, menu_icons[i]);
        lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

        // Store button reference for selection updates
        menu_buttons[i] = btn;
    }

    // Initialize first button as selected like menu_system.c
    update_menu_button_selection(0, current_selected_button);

    return bottom_container;
}

/**
 * @brief Handle main menu navigation
 */
static void handle_main_navigation(uint32_t key)
{
    uint8_t old_selection = current_selected_button;
    uint8_t new_selection = old_selection;

    switch (key) {
    case KEY_UP:
    case KEY_LEFT:
        // Move to previous icon with wrap around
        if (current_selected_button > 0) {
            new_selection = current_selected_button - 1;
        } else {
            new_selection = MENU_BUTTON_COUNT - 1; // Wrap to last icon
        }
        break;
    case KEY_DOWN:
    case KEY_RIGHT:
        // Move to next icon with wrap around
        if (current_selected_button < MENU_BUTTON_COUNT - 1) {
            new_selection = current_selected_button + 1;
        } else {
            new_selection = 0; // Wrap to first icon
        }
        break;
    }

    if (new_selection != old_selection) {
        update_menu_button_selection(old_selection, new_selection);
        current_selected_button = new_selection;
        printf("[%s] Menu navigation: %d -> %d\n", main_screen.name, old_selection, new_selection);
    }
}

/**
 * @brief Get current selected button
 */
static uint8_t get_selected_button(void)
{
    return current_selected_button;
}

/***********************************************************
***************Pet Event Callback Functions***************
***********************************************************/

void main_screen_register_pet_event_callback(pet_event_callback_t callback, void *user_data)
{
    pet_event_callback = callback;
    pet_event_user_data = user_data;
    printf("[%s] Pet event callback registered\n", main_screen.name);
}

pet_stats_t *main_screen_get_pet_stats(void)
{
    return &main_screen_pet_stats;
}

uint8_t main_screen_update_pet_stats(pet_stats_t *stats)
{
    if (NULL == stats) {
        return 1;
    }

    if (stats->health <= 100) {
        main_screen_pet_stats.health = stats->health;
    }
    if (stats->hungry <= 100) {
        main_screen_pet_stats.hungry = stats->hungry;
    }
    if (stats->clean <= 100) {
        main_screen_pet_stats.clean = stats->clean;
    }
    if (stats->happy <= 100) {
        main_screen_pet_stats.happy = stats->happy;
    }
    if (stats->age_days <= 999) {
        main_screen_pet_stats.age_days = stats->age_days;
    }
    if (stats->weight_kg <= 999.9) {
        main_screen_pet_stats.weight_kg = stats->weight_kg;
    }

    printf("[%s] Pet stats updated - Health: %d, Hungry: %d, Clean: %d, Happy: %d\n", main_screen.name, stats->health,
           stats->hungry, stats->clean, stats->happy);

    return 0;
}

void main_screen_init_pet_stats(pet_stats_t *stats)
{
    if (NULL == stats) {
        stats = &main_screen_pet_stats;
    }

    stats->health = 85;
    stats->hungry = 60;
    stats->clean = 70;
    stats->happy = 90;
    stats->age_days = 15;
    stats->weight_kg = 1.2f;
    strncpy(stats->name, "Ducky", sizeof(stats->name) - 1);
    stats->name[sizeof(stats->name) - 1] = '\0';

    // Also initialize internal pet stats
    main_screen_pet_stats = *stats;

    printf("[%s] Pet stats initialized - Name: %s, Health: %d, Hungry: %d, Clean: %d, Happy: %d\n", main_screen.name,
           stats->name, stats->health, stats->hungry, stats->clean, stats->happy);
}

/**
 * @brief Trigger a pet event through the callback system
 * @param event_type Type of pet event
 */
static void trigger_pet_event(pet_event_type_t event_type)
{
    if (pet_event_callback != NULL) {
        printf("[%s] Triggering pet event: %d\n", main_screen.name, event_type);
        pet_event_callback(event_type, pet_event_user_data);
    } else {
        printf("[%s] Pet event callback not registered, cannot trigger event %d\n", main_screen.name, event_type);
    }
}

/**
 * @brief Handle pet event and update animations accordingly
 * @param event_type Type of pet event
 */
void main_screen_handle_pet_event(pet_event_type_t event_type)
{
    // First trigger the callback if registered
    trigger_pet_event(event_type);

    // Then handle visual updates based on event type
    switch (event_type) {
    case PET_EVENT_FEED_HAMBURGER:
    case PET_EVENT_FEED_PIZZA:
    case PET_EVENT_FEED_APPLE:
    case PET_EVENT_FEED_FISH:
    case PET_EVENT_FEED_CARROT:
    case PET_EVENT_FEED_ICE_CREAM:
    case PET_EVENT_FEED_COOKIE:
        // Show eating animation
        main_screen_set_pet_animation_state(AI_PET_STATE_EAT);
        printf("[%s] Pet is eating\n", main_screen.name);
        break;

    case PET_EVENT_DRINK_WATER:
        // Show eating animation for drinking
        main_screen_set_pet_animation_state(AI_PET_STATE_EAT);
        printf("[%s] Pet is drinking water\n", main_screen.name);
        break;

    case PET_EVENT_TOILET:
        // Show toilet animation
        main_screen_set_pet_animation_state(AI_PET_STATE_TOILET);
        printf("[%s] Pet is using toilet\n", main_screen.name);
        break;

    case PET_EVENT_TAKE_BATH:
        // Show bath animation
        main_screen_set_pet_animation_state(AI_PET_STATE_BATH);
        printf("[%s] Pet is taking a bath\n", main_screen.name);
        break;

    case PET_EVENT_SEE_DOCTOR:
        // Show sick animation temporarily
        main_screen_set_pet_animation_state(AI_PET_STATE_SICK);
        printf("[%s] Pet is seeing the doctor\n", main_screen.name);
        break;

    case PET_EVENT_SLEEP:
        // Show sleep animation
        main_screen_set_pet_animation_state(AI_PET_STATE_SLEEP);
        printf("[%s] Pet is sleeping\n", main_screen.name);
        break;

    case PET_EVENT_WAKE_UP:
        // Return to normal animation
        main_screen_set_pet_animation_state(AI_PET_STATE_NORMAL);
        printf("[%s] Pet is waking up\n", main_screen.name);
        break;

    case PET_STAT_RANDOMIZE:
        // Show happy animation for stat randomization
        main_screen_set_pet_animation_state(AI_PET_STATE_HAPPY);
        printf("[%s] Pet stats randomized\n", main_screen.name);
        break;

    default:
        printf("[%s] Unknown pet event: %d\n", main_screen.name, event_type);
        break;
    }
}
