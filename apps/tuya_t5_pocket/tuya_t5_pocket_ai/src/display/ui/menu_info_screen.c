/**
 * @file menu_info_screen.c
 * @brief Implementation of the info menu screen for the application
 *
 * This file contains the implementation of the info menu screen which displays
 * pet information including name, statistics, and action buttons.
 *
 * The info menu includes:
 * - Pet name display with edit functionality
 * - Pet statistics with visual icon bars
 * - Action buttons for pet management
 * - Keyboard event handling for navigation
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "menu_info_screen.h"
#include "screen_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_info_menu_screen;
static lv_obj_t *info_menu_list;
static lv_timer_t *timer;
static pet_stats_t current_pet_stats;
static uint8_t selected_item = 0;

// UI Constants
#define STAT_CONTAINER_HEIGHT 30
#define STAT_CONTAINER_WIDTH 320
#define SEPARATOR_HEIGHT 2
#define MAX_STAT_VALUE 100

Screen_t menu_info_screen = {
    .init = menu_info_screen_init,
    .deinit = menu_info_screen_deinit,
    .screen_obj = &ui_info_menu_screen,
    .name = "menu_info_screen",
};

// External image declarations
LV_IMG_DECLARE(family_star);

/***********************************************************
********************function declaration********************
***********************************************************/

static void menu_info_screen_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);
static void create_pet_name_display(void);
static void create_pet_stats_displays(void);
static void create_separator(void);
static void create_actions_section(void);
static void create_stat_display_item(const char *label, const char *value);
static void create_stat_icon_bar(const char *label, int value);
static void update_selection(uint8_t old_selection, uint8_t new_selection);
static void handle_action_selection(void);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Timer callback for the info menu screen
 *
 * This function is called when the info menu timer expires.
 * It can be used for periodic updates or automatic transitions.
 *
 * @param timer The timer object
 */
static void menu_info_screen_timer_cb(lv_timer_t *timer)
{
    printf("[%s] info menu timer callback\n", menu_info_screen.name);
    // Add any periodic update logic here
}

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the info menu screen.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", menu_info_screen.name, key);

    uint32_t child_count = lv_obj_get_child_cnt(info_menu_list);
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
        case KEY_ENTER:
            handle_action_selection();
            break;
        case KEY_ESC:
            printf("ESC key pressed - returning to main menu\n");
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
 * @brief Create pet name display container
 */
static void create_pet_name_display(void)
{
    lv_obj_t *name_container = lv_obj_create(info_menu_list);
    lv_obj_set_size(name_container, STAT_CONTAINER_WIDTH, 40);
    lv_obj_set_style_bg_opa(name_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(name_container, 0, 0);
    lv_obj_set_style_pad_all(name_container, 2, 0);

    lv_obj_t *name_label = lv_label_create(name_container);
    lv_label_set_text_fmt(name_label, "Name: %s", current_pet_stats.name);
    lv_obj_align(name_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(name_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
}

/**
 * @brief Create pet statistics displays with icon bars
 */
static void create_pet_stats_displays(void)
{
    create_stat_icon_bar("Health:", current_pet_stats.health);
    create_stat_icon_bar("Hungry:", current_pet_stats.hungry);
    create_stat_icon_bar("Clean:", current_pet_stats.clean);
    create_stat_icon_bar("Happy:", current_pet_stats.happy);

    char value_str[16];
    snprintf(value_str, sizeof(value_str), "%d days", current_pet_stats.age_days);
    create_stat_display_item("Age:", value_str);

    snprintf(value_str, sizeof(value_str), "%.1f kg", current_pet_stats.weight_kg);
    create_stat_display_item("Weight:", value_str);
}

/**
 * @brief Create a visual separator
 */
static void create_separator(void)
{
    lv_obj_t *separator = lv_obj_create(info_menu_list);
    lv_obj_set_size(separator, STAT_CONTAINER_WIDTH, SEPARATOR_HEIGHT);
    lv_obj_set_style_bg_color(separator, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(separator, LV_OPA_50, 0);
}

/**
 * @brief Create actions section with buttons
 */
static void create_actions_section(void)
{
    // Add actions subtitle
    lv_obj_t *action_title = lv_label_create(info_menu_list);
    lv_label_set_text(action_title, "Actions:");
    lv_obj_align(action_title, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(action_title, lv_color_black(), 0);
    lv_obj_set_style_text_font(action_title, &lv_font_montserrat_14, 0);
    lv_obj_add_flag(action_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(action_title, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    // Add action buttons
    lv_list_add_btn(info_menu_list, LV_SYMBOL_EDIT, "Edit Pet Name");
    lv_list_add_btn(info_menu_list, LV_SYMBOL_SETTINGS, "View Statistics");
    lv_list_add_btn(info_menu_list, LV_SYMBOL_WIFI, "WIFI Settings");
    lv_list_add_btn(info_menu_list, LV_SYMBOL_REFRESH, "Randomize Pet Data");
}

/**
 * @brief Create a simple stat display item with label and value
 */
static void create_stat_display_item(const char *label, const char *value)
{
    lv_obj_t *container = lv_obj_create(info_menu_list);
    lv_obj_set_size(container, STAT_CONTAINER_WIDTH, STAT_CONTAINER_HEIGHT);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 2, 0);

    lv_obj_t *label_obj = lv_label_create(container);
    lv_label_set_text(label_obj, label);
    lv_obj_align(label_obj, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_text_color(label_obj, lv_color_black(), 0);

    lv_obj_t *value_obj = lv_label_create(container);
    lv_label_set_text(value_obj, value);
    lv_obj_align(value_obj, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_text_color(value_obj, lv_color_black(), 0);
}

/**
 * @brief Create a stat display with icon bar using family_star icons
 */
static void create_stat_icon_bar(const char *label, int value)
{
    lv_obj_t *container = lv_obj_create(info_menu_list);
    lv_obj_set_size(container, STAT_CONTAINER_WIDTH, STAT_CONTAINER_HEIGHT);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 2, 0);

    lv_obj_t *label_obj = lv_label_create(container);
    lv_label_set_text(label_obj, label);
    lv_obj_align(label_obj, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_text_color(label_obj, lv_color_black(), 0);

    // Quantize value to 0-5
    int filled = (value + 9) / 20; // 0-19:0, 20-39:1, ..., 100:5
    if (filled > 5) filled = 5;
    if (filled < 0) filled = 0;

    // Draw 5 icons using the family_star image
    for (int i = 0; i < 5; ++i) {
        if (i < filled) {
            lv_obj_t *icon = lv_img_create(container);
            lv_img_set_src(icon, &family_star);
            lv_obj_set_size(icon, 18, 18);
            lv_obj_set_style_img_recolor_opa(icon, LV_OPA_TRANSP, 0);
            lv_obj_align(icon, LV_ALIGN_LEFT_MID, 80 + i * 22, 0);
        }
    }

    // Add x/5 text after the icons
    char stat_text[8];
    snprintf(stat_text, sizeof(stat_text), "%d/5", filled);
    lv_obj_t *stat_label = lv_label_create(container);
    lv_label_set_text(stat_label, stat_text);
    lv_obj_align(stat_label, LV_ALIGN_LEFT_MID, 80 + 5 * 22 + 8, 0);
    lv_obj_set_style_text_color(stat_label, lv_color_black(), 0);
}

/**
 * @brief Update visual selection highlighting
 */
static void update_selection(uint8_t old_selection, uint8_t new_selection)
{
    uint32_t child_count = lv_obj_get_child_cnt(info_menu_list);

    if (old_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(info_menu_list, old_selection), lv_color_white(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(info_menu_list, old_selection), lv_color_black(), 0);
    }

    if (new_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(info_menu_list, new_selection), lv_color_black(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(info_menu_list, new_selection), lv_color_white(), 0);
        lv_obj_scroll_to_view(lv_obj_get_child(info_menu_list, new_selection), LV_ANIM_ON);
    }
}

/**
 * @brief Handle action selection based on current selected item
 */
static void handle_action_selection(void)
{
    uint32_t child_count = lv_obj_get_child_cnt(info_menu_list);

    // Find action items start (after stats and separator)
    uint32_t action_start = 0;
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(info_menu_list, i);
        if (lv_obj_check_type(child, &lv_label_class)) {
            action_start = i + 1; // First action button is after the "Actions:" label
            break;
        }
    }

    if (selected_item >= action_start) {
        uint32_t action_index = selected_item - action_start;

        switch (action_index) {
            case 0: // Edit Pet Name
                printf("Edit Pet Name action selected\n");
                break;
            case 1: // View Statistics
                printf("View Statistics action selected\n");
                break;
            case 2: // WIFI Settings
                printf("WIFI Settings action selected\n");
                break;
            case 3: // Randomize Pet Data
                printf("Randomize Pet Data action selected\n");
                // Randomize stats for demo
                current_pet_stats.health = rand() % 101;
                current_pet_stats.hungry = rand() % 101;
                current_pet_stats.clean = rand() % 101;
                current_pet_stats.happy = rand() % 101;

                // Refresh the display
                menu_info_screen_deinit();
                menu_info_screen_init();
                break;
            default:
                printf("Unknown action selected\n");
                break;
        }
    }
}

/**
 * @brief Initialize the info menu screen
 *
 * This function creates the info menu UI with pet information display,
 * statistics bars, and action buttons.
 */
void menu_info_screen_init(void)
{
    // Initialize pet stats if not already set
    if (strlen(current_pet_stats.name) == 0) {
        current_pet_stats.health = 85;
        current_pet_stats.hungry = 60;
        current_pet_stats.clean = 70;
        current_pet_stats.happy = 90;
        current_pet_stats.age_days = 15;
        current_pet_stats.weight_kg = 1.2f;
        strcpy(current_pet_stats.name, "Ducky");
    }

    ui_info_menu_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_info_menu_screen, 384, 168);
    lv_obj_set_style_bg_color(ui_info_menu_screen, lv_color_white(), 0);

    // Title at the top
    lv_obj_t *title = lv_label_create(ui_info_menu_screen);
    lv_label_set_text(title, "Pet Information");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);

    // List for info menu items
    info_menu_list = lv_list_create(ui_info_menu_screen);
    lv_obj_set_size(info_menu_list, 364, 128);
    lv_obj_align(info_menu_list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_add_flag(info_menu_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(info_menu_list, LV_DIR_VER);

    lv_obj_set_style_border_color(info_menu_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(info_menu_list, 2, 0);

    // Create all UI components
    create_pet_name_display();
    create_pet_stats_displays();
    create_separator();
    create_actions_section();

    // Highlight first item
    selected_item = 0;
    if (lv_obj_get_child_cnt(info_menu_list) > 0) {
        update_selection(0, 0);
    }

    timer = lv_timer_create(menu_info_screen_timer_cb, 1000, NULL);
    lv_obj_add_event_cb(ui_info_menu_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_info_menu_screen);
    lv_group_focus_obj(ui_info_menu_screen);
}

/**
 * @brief Deinitialize the info menu screen
 *
 * This function cleans up the info menu by removing event callbacks
 * and freeing resources.
 */
void menu_info_screen_deinit(void)
{
    if (ui_info_menu_screen) {
        printf("deinit info menu screen\n");
        lv_obj_remove_event_cb(ui_info_menu_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_info_menu_screen);
    }
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }
}

/**
 * @brief Set pet statistics for display
 *
 * @param stats Pointer to pet statistics structure
 */
void menu_info_screen_set_pet_stats(pet_stats_t *stats)
{
    if (stats) {
        current_pet_stats = *stats;
    }
}

/**
 * @brief Get current pet statistics
 *
 * @return Pointer to current pet statistics
 */
pet_stats_t* menu_info_screen_get_pet_stats(void)
{
    return &current_pet_stats;
}
