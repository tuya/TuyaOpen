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
#include "keyboard_screen.h"
#include "toast_screen.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(ENABLE_LVGL_HARDWARE)
#include "tal_kv.h"
#endif
/***********************************************************
***********************variable define**********************
***********************************************************/

#define MENU_INFO_FONT       &lv_font_terminusTTF_Bold_18
#define MENU_INFO_TITLE_FONT &lv_font_terminusTTF_Bold_16

static lv_obj_t *ui_info_menu_screen;
static lv_obj_t *info_menu_list;
static lv_timer_t *timer;
static pet_stats_t current_pet_stats = {
    .health = 85, .hungry = 60, .clean = 70, .happy = 90, .age_days = 15, .weight_kg = 1.5f, .name = "Ducky"};
static uint8_t selected_item = 0;
static uint8_t last_selected_item = 0;

// KV storage key for pet name
#define PET_NAME_KV_KEY "pet_name"

// UI Constants
#define STAT_CONTAINER_HEIGHT 30
#define STAT_CONTAINER_WIDTH  320
#define SEPARATOR_HEIGHT      2
#define MAX_STAT_VALUE        100

Screen_t menu_info_screen = {
    .init = menu_info_screen_init,
    .deinit = menu_info_screen_deinit,
    .screen_obj = &ui_info_menu_screen,
    .name = "menu_info_screen",
    .state_data = NULL,
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
static void refresh_info_screen_timer_cb(lv_timer_t *timer);

static void keyboard_callback(const char *text, void *user_data);
static void show_keyboard_for_pet_name(void);

/*
 * Helper: determine whether a child object should be selectable/focusable
 * We treat objects with the CLICK_FOCUSABLE flag as selectable (e.g. list buttons).
 * Section titles and decorative labels typically clear CLICK_FOCUSABLE and will be skipped.
 */
static bool is_child_selectable(lv_obj_t *child)
{
    if (child == NULL)
        return false;
    return lv_obj_has_flag(child, LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

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
    if (child_count == 0)
        return;

    uint8_t old_selection = selected_item;
    uint8_t new_selection = old_selection;
    switch (key) {
    case KEY_UP:
        // move to previous selectable child
        for (int i = (int)selected_item - 1; i >= 0; --i) {
            lv_obj_t *ch = lv_obj_get_child(info_menu_list, i);
            if (is_child_selectable(ch)) {
                new_selection = (uint8_t)i;
                break;
            }
        }
        break;
    case KEY_DOWN:
        // move to next selectable child
        for (uint32_t i = selected_item + 1; i < child_count; ++i) {
            lv_obj_t *ch = lv_obj_get_child(info_menu_list, i);
            if (is_child_selectable(ch)) {
                new_selection = (uint8_t)i;
                break;
            }
        }
        break;
    case KEY_ENTER:
        handle_action_selection();
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
    lv_obj_set_style_text_font(name_label, MENU_INFO_FONT, 0);
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
    lv_obj_set_style_text_font(action_title, MENU_INFO_FONT, 0);
    lv_obj_add_flag(action_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(action_title, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    // Add action buttons
    lv_obj_t *btn1 = lv_list_add_btn(info_menu_list, LV_SYMBOL_EDIT, "Edit Pet Name");
    // Get the label from the button and set font only for the text label
    lv_obj_t *label1 = lv_obj_get_child(btn1, 1); // Second child is the text label
    if (label1)
        lv_obj_set_style_text_font(label1, MENU_INFO_FONT, 0);

    lv_obj_t *btn2 = lv_list_add_btn(info_menu_list, LV_SYMBOL_REFRESH, "Randomize Pet Data");
    lv_obj_t *label2 = lv_obj_get_child(btn2, 1);
    if (label2)
        lv_obj_set_style_text_font(label2, MENU_INFO_FONT, 0);
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
    lv_obj_set_style_text_font(label_obj, MENU_INFO_FONT, 0);

    lv_obj_t *value_obj = lv_label_create(container);
    lv_label_set_text(value_obj, value);
    lv_obj_align(value_obj, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_text_color(value_obj, lv_color_black(), 0);
    lv_obj_set_style_text_font(value_obj, MENU_INFO_FONT, 0);
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
    lv_obj_set_style_text_font(label_obj, MENU_INFO_FONT, 0);

    // Quantize value to 0-5
    int filled = (value + 9) / 20; // 0-19:0, 20-39:1, ..., 100:5
    if (filled > 5)
        filled = 5;
    if (filled < 0)
        filled = 0;

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
    lv_obj_set_style_text_font(stat_label, MENU_INFO_FONT, 0);
}

/**
 * @brief Update visual selection highlighting
 */
static void update_selection(uint8_t old_selection, uint8_t new_selection)
{
    uint32_t child_count = lv_obj_get_child_cnt(info_menu_list);
    // Find nearest selectable old child to un-highlight
    if (old_selection < child_count) {
        for (int i = old_selection; i >= 0; --i) {
            lv_obj_t *ch = lv_obj_get_child(info_menu_list, i);
            if (is_child_selectable(ch)) {
                lv_obj_set_style_bg_color(ch, lv_color_white(), 0);
                lv_obj_set_style_text_color(ch, lv_color_black(), 0);
                break;
            }
        }
    }

    // Find nearest selectable new child to highlight
    if (new_selection < child_count) {
        for (uint32_t i = new_selection; i < child_count; ++i) {
            lv_obj_t *ch = lv_obj_get_child(info_menu_list, i);
            if (is_child_selectable(ch)) {
                lv_obj_set_style_bg_color(ch, lv_color_black(), 0);
                lv_obj_set_style_text_color(ch, lv_color_white(), 0);
                lv_obj_scroll_to_view(ch, LV_ANIM_ON);
                break;
            }
        }
    }
}

/**
 * @brief Handle action selection based on current selected item
 */
static void handle_action_selection(void)
{
    uint32_t child_count = lv_obj_get_child_cnt(info_menu_list);

    // Save current selection
    last_selected_item = selected_item;

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
            show_keyboard_for_pet_name();
            break;
        case 1: // Randomize Pet Data
            printf("Randomize Pet Data action selected\n");
            toast_screen_show("Unlock at Higher Level", 2000);
            break;
        default:
            printf("Unknown action selected\n");
            toast_screen_show("Unlock at Higher Level", 2000);
            break;
        }
    }
}

/**
 * @brief Timer callback to refresh info screen after name change
 */
static void refresh_info_screen_timer_cb(lv_timer_t *timer)
{
    printf("Refreshing info screen after name change\n");
    printf("Current pet name: '%s'\n", current_pet_stats.name);

    // Try to directly update the name display instead of reinitializing
    if (info_menu_list && lv_obj_is_valid(info_menu_list)) {
        uint32_t child_count = lv_obj_get_child_cnt(info_menu_list);
        printf("Info menu list has %d children\n", child_count);

        if (child_count > 0) {
            lv_obj_t *name_container = lv_obj_get_child(info_menu_list, 0); // First child should be name container
            if (name_container && lv_obj_is_valid(name_container)) {
                printf("Found name container\n");
                lv_obj_t *name_label = lv_obj_get_child(name_container, 0); // First child should be name label
                if (name_label && lv_obj_check_type(name_label, &lv_label_class)) {
                    printf("Updating name label from direct access\n");
                    lv_label_set_text_fmt(name_label, "Name: %s", current_pet_stats.name);
                    // Force a redraw
                    lv_obj_invalidate(name_label);
                    lv_obj_invalidate(name_container);
                    printf("Name label updated successfully to: %s\n", current_pet_stats.name);
                } else {
                    printf("Name label not found or invalid type\n");
                }
            } else {
                printf("Name container not found or invalid\n");
            }
        } else {
            printf("No children in info menu list\n");
        }
    } else {
        printf("Info menu list is not valid, falling back to full reinit\n");
        // Fallback: reinitialize the entire screen
        if (ui_info_menu_screen && lv_obj_is_valid(ui_info_menu_screen)) {
            menu_info_screen_deinit();
            menu_info_screen_init();
        }
    }

    lv_timer_del(timer);
}

/**
 * @brief Keyboard callback function
 */
static void keyboard_callback(const char *text, void *user_data)
{
    (void)user_data; // Unused parameter

    printf("Keyboard callback called with text: '%s'\n", text ? text : "NULL");

    if (text && strlen(text) > 0) {
        // Update pet name
        strncpy(current_pet_stats.name, text, sizeof(current_pet_stats.name) - 1);
        current_pet_stats.name[sizeof(current_pet_stats.name) - 1] = '\0';
        printf("Pet name updated to: '%s' (length: %zu)\n", current_pet_stats.name, strlen(current_pet_stats.name));

#if defined(ENABLE_LVGL_HARDWARE)
        // Save pet name to KV storage
        int ret =
            tal_kv_set(PET_NAME_KV_KEY, (const uint8_t *)current_pet_stats.name, strlen(current_pet_stats.name) + 1);
        if (ret == 0) {
            printf("Pet name saved to KV storage successfully\n");
        } else {
            printf("Failed to save pet name to KV storage, error: %d\n", ret);
        }
#else
        printf("KV storage not available (PC simulator mode)\n");
#endif

        // Create a timer to refresh the screen after a short delay
        // This ensures the screen_back() has completed before we refresh
        lv_timer_t *refresh_timer = lv_timer_create(refresh_info_screen_timer_cb, 200, NULL);
        lv_timer_set_repeat_count(refresh_timer, 1);
    } else {
        printf("Keyboard input cancelled or empty\n");
    }
}

/**
 * @brief Show keyboard for pet name editing
 */
static void show_keyboard_for_pet_name(void)
{
    printf("show_keyboard_for_pet_name called\n");
    printf("  current pet name: '%s'\n", current_pet_stats.name);
    printf("  callback function: %p\n", (void *)keyboard_callback);
    keyboard_screen_show_with_callback(current_pet_stats.name, keyboard_callback, NULL);
}

/**
 * @brief Initialize the info menu screen
 *
 * This function creates the info menu UI with pet information display,
 * statistics bars, and action buttons.
 */
void menu_info_screen_init(void)
{
#if defined(ENABLE_LVGL_HARDWARE)
    // Try to load pet name from KV storage
    uint8_t *stored_name = NULL;
    size_t name_length = 0;
    int ret = tal_kv_get(PET_NAME_KV_KEY, &stored_name, &name_length);

    if (ret == 0 && stored_name != NULL && name_length > 0) {
        // Successfully loaded pet name from storage
        strncpy(current_pet_stats.name, (const char *)stored_name, sizeof(current_pet_stats.name) - 1);
        current_pet_stats.name[sizeof(current_pet_stats.name) - 1] = '\0';
        printf("Pet name loaded from KV storage: '%s'\n", current_pet_stats.name);

        // Free the allocated memory
        tal_kv_free(stored_name);
    } else {
        // No stored name found or error occurred, use default
        printf("No pet name in KV storage (ret=%d), using default name\n", ret);
        strncpy(current_pet_stats.name, "Ducky", sizeof(current_pet_stats.name) - 1);
        current_pet_stats.name[sizeof(current_pet_stats.name) - 1] = '\0';
    }
#else
    // PC simulator mode - use default name
    strncpy(current_pet_stats.name, "Ducky", sizeof(current_pet_stats.name) - 1);
    current_pet_stats.name[sizeof(current_pet_stats.name) - 1] = '\0';
    printf("PC simulator mode - using default pet name: '%s'\n", current_pet_stats.name);
#endif

    ui_info_menu_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_info_menu_screen, 384, 168);
    lv_obj_set_style_bg_color(ui_info_menu_screen, lv_color_white(), 0);

    // Title at the top
    lv_obj_t *title = lv_label_create(ui_info_menu_screen);
    lv_label_set_text(title, "Pet Information");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, MENU_INFO_FONT, 0);
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

    // Set selected_item to first selectable child (skip titles/decorative labels)
    selected_item = last_selected_item;
    uint32_t child_count = lv_obj_get_child_cnt(info_menu_list);
    for (uint32_t i = 0; i < child_count; ++i) {
        lv_obj_t *ch = lv_obj_get_child(info_menu_list, i);
        if (is_child_selectable(ch)) {
            if (selected_item == 0) {
                selected_item = (uint8_t)i;
            }
            break;
        }
    }

    // Validate selected_item is within bounds and selectable
    if (selected_item >= child_count) {
        selected_item = 0;
        last_selected_item = 0;
        for (uint32_t i = 0; i < child_count; ++i) {
            lv_obj_t *ch = lv_obj_get_child(info_menu_list, i);
            if (is_child_selectable(ch)) {
                selected_item = (uint8_t)i;
                break;
            }
        }
    } else {
        // Check if current selected_item is selectable, if not find nearest selectable
        lv_obj_t *current_child = lv_obj_get_child(info_menu_list, selected_item);
        if (!is_child_selectable(current_child)) {
            for (uint32_t i = selected_item; i < child_count; ++i) {
                lv_obj_t *ch = lv_obj_get_child(info_menu_list, i);
                if (is_child_selectable(ch)) {
                    selected_item = (uint8_t)i;
                    break;
                }
            }
        }
    }

    // Highlight the selected item
    if (child_count > 0) {
        update_selection(0, selected_item);
        printf("[%s] Restored selection to item %d\n", menu_info_screen.name, selected_item);
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
pet_stats_t *menu_info_screen_get_pet_stats(void)
{
    return &current_pet_stats;
}
