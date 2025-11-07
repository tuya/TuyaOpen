/**
 * @file menu_scan_screen.c
 * @brief Implementation of the scan menu screen for the application
 *
 * This file contains the implementation of the scan menu screen which provides
 * various scanning and game features including WiFi scan, I2C scan, and games.
 *
 * The scan menu includes:
 * - WiFi scanning functionality
 * - I2C device scanning
 * - Mini games (Dino, Snake)
 * - Level indicator
 * - Automatic transition timer
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "menu_scan_screen.h"
#include "screen_manager.h"
#include "wifi_scan_screen.h"
#include "i2c_scan_screen.h"
#include "dino_game_screen.h"
#include "snake_game_screen.h"
#include "level_indicator_screen.h"
#include "ebook_screen.h"
#include "temp_humidity_screen.h"
#include <stdio.h>

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_menu_scan_screen;
static lv_obj_t *scan_menu_list;
static lv_timer_t *timer;
static uint8_t selected_item = 0;
static uint8_t last_selected_item = 0;

Screen_t menu_scan_screen = {
    .init = menu_scan_screen_init,
    .deinit = menu_scan_screen_deinit,
    .screen_obj = &ui_menu_scan_screen,
    .name = "menu_scan_screen",
    .state_data = NULL,
};

/***********************************************************
********************function declaration********************
***********************************************************/

// External screen declarations
extern Screen_t wifi_scan_screen;
extern Screen_t i2c_scan_screen;
extern Screen_t dino_game_screen;
extern Screen_t snake_game_screen;
extern Screen_t level_indicator_screen;
extern Screen_t temp_humidity_screen;

static void menu_scan_screen_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);
static void update_selection(uint8_t old_selection, uint8_t new_selection);
static void handle_scan_selection(void);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Timer callback for the scan menu screen
 *
 * This function is called when the scan menu screen timer expires.
 * It can be used for periodic updates.
 *
 * @param timer The timer object
 */
static void menu_scan_screen_timer_cb(lv_timer_t *timer)
{
    printf("[%s] scan menu timer callback\n", menu_scan_screen.name);
    // Add any periodic update logic here
}

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the scan menu screen.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", menu_scan_screen.name, key);

    uint32_t child_count = lv_obj_get_child_cnt(scan_menu_list);
    if (child_count == 0) return;

    uint8_t old_selection = selected_item;
    uint8_t new_selection = old_selection;

    switch (key) {
        case KEY_UP:
            if (selected_item > 0) {
                new_selection = selected_item - 1;
            }
            break;
        case KEY_DOWN:
            if (selected_item < child_count - 1) {
                new_selection = selected_item + 1;
            }
            break;
        case KEY_LEFT:
            printf("LEFT key pressed\n");
            break;
        case KEY_RIGHT:
            printf("RIGHT key pressed\n");
            break;
        case KEY_ENTER:
            handle_scan_selection();
            break;
        case KEY_ESC:
            printf("ESC key pressed - returning to main menu\n");
            last_selected_item = 0;
            screen_back();
            break;
        default:
            printf("Key %d pressed\n", key);
            break;
    }

    if (new_selection != old_selection) {
        update_selection(old_selection, new_selection);
        selected_item = new_selection;
    }
}

/**
 * @brief Update visual selection highlighting
 */
static void update_selection(uint8_t old_selection, uint8_t new_selection)
{
    uint32_t child_count = lv_obj_get_child_cnt(scan_menu_list);

    if (old_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(scan_menu_list, old_selection), lv_color_white(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(scan_menu_list, old_selection), lv_color_black(), 0);
    }

    if (new_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(scan_menu_list, new_selection), lv_color_black(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(scan_menu_list, new_selection), lv_color_white(), 0);
        lv_obj_scroll_to_view(lv_obj_get_child(scan_menu_list, new_selection), LV_ANIM_ON);
    }
}

/**
 * @brief Handle scan menu selection based on current selected item
 */
static void handle_scan_selection(void)
{
    last_selected_item = selected_item;

    switch (selected_item) {
        case 0: // WiFi scan demo
            printf("WiFi scan demo selected\n");
            screen_load(&wifi_scan_screen);
            break;
        case 1: // I2C device scan demo
            printf("I2C device scan demo selected\n");
            screen_load(&i2c_scan_screen);
            break;
        case 2: // Dino Game
            printf("Dino Game selected\n");
            screen_load(&dino_game_screen);
            break;
        case 3: // Snake Game
            printf("Snake Game selected\n");
            screen_load(&snake_game_screen);
            break;
        case 4: // Level Indicator
            printf("Level Indicator selected\n");
            screen_load(&level_indicator_screen);
            break;
        case 5: // E-book Reader
            printf("E-book Reader action selected\n");
            screen_load(&ebook_screen);
            break;
        case 6: // Temperature & Humidity
            printf("Temperature & Humidity selected\n");
            screen_load(&temp_humidity_screen);
            break;
        default:
            printf("Unknown scan option selected\n");
            break;
    }
}

/**
 * @brief Initialize the scan menu screen
 *
 * This function creates the scan menu UI with scanning options and games.
 */
void menu_scan_screen_init(void)
{
    ui_menu_scan_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_menu_scan_screen, 384, 168);
    lv_obj_set_style_bg_color(ui_menu_scan_screen, lv_color_white(), 0);

    // Title at the top
    lv_obj_t *title = lv_label_create(ui_menu_scan_screen);
    lv_label_set_text(title, "Device Scan & Games");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);

    // List for scan menu items
    scan_menu_list = lv_list_create(ui_menu_scan_screen);
    lv_obj_set_size(scan_menu_list, 364, 128);
    lv_obj_align(scan_menu_list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_add_flag(scan_menu_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scan_menu_list, LV_DIR_VER);

    lv_obj_set_style_border_color(scan_menu_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(scan_menu_list, 2, 0);

    // Add scan menu items
    lv_list_add_btn(scan_menu_list, LV_SYMBOL_WIFI, "WiFi scan demo");
    lv_list_add_btn(scan_menu_list, LV_SYMBOL_SETTINGS, "I2C device scan demo");
    lv_list_add_btn(scan_menu_list, LV_SYMBOL_PLAY, "Dino Game");
    lv_list_add_btn(scan_menu_list, LV_SYMBOL_SHUFFLE, "Snake Game");
    lv_list_add_btn(scan_menu_list, LV_SYMBOL_EYE_OPEN, "Level Indicator");
    lv_list_add_btn(scan_menu_list, LV_SYMBOL_FILE, "E-book Reader");
    lv_list_add_btn(scan_menu_list, LV_SYMBOL_WARNING, "Temperature & Humidity");

    selected_item = last_selected_item;
    uint32_t child_count = lv_obj_get_child_cnt(scan_menu_list);

    if (selected_item >= child_count) {
        selected_item = 0;
        last_selected_item = 0;
    }

    if (child_count > 0) {
        update_selection(0, selected_item);
        printf("[%s] Restored selection to item %d\n", menu_scan_screen.name, selected_item);
    }

    timer = lv_timer_create(menu_scan_screen_timer_cb, 1000, NULL);
    lv_obj_add_event_cb(ui_menu_scan_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_menu_scan_screen);
    lv_group_focus_obj(ui_menu_scan_screen);
}

/**
 * @brief Deinitialize the scan menu screen
 *
 * This function cleans up the scan menu by deleting the UI object
 * and timer, and removing the event callback.
 */
void menu_scan_screen_deinit(void)
{
    if (ui_menu_scan_screen) {
        printf("deinit scan menu screen\n");
        lv_obj_remove_event_cb(ui_menu_scan_screen, NULL);
        lv_group_remove_obj(ui_menu_scan_screen);
    }
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }
}

/**
 * @brief Create the scan menu screen (alias for init)
 */
void menu_scan_screen_create(void)
{
    menu_scan_screen_init();
}
