/**
 * @file menu_health_screen.c
 * @brief Implementation of the health menu screen for the application
 *
 * This file contains the implementation of the health menu screen which displays
 * health and medical options for the pet.
 *
 * The health menu includes:
 * - Health status indicators with visual bars
 * - Medical care options
 * - Doctor visit functionality
 * - Keyboard event handling for navigation
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "menu_health_screen.h"
#include "screen_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_menu_health_screen_screen;
static lv_obj_t *menu_health_screen_list;
static lv_timer_t *timer;
static uint8_t selected_item = 0;
static health_status_t current_health_status;
static health_event_callback_t health_callback = NULL;
static void *health_callback_user_data = NULL;

Screen_t menu_health_screen = {
    .init = menu_health_screen_init,
    .deinit = menu_health_screen_deinit,
    .screen_obj = &ui_menu_health_screen_screen,
    .name = "health_menu",
};

// Health actions configuration
typedef struct {
    const char *name;           /**< Action name */
    const char *icon;           /**< Action icon symbol */
    const char *description;    /**< Action description */
    health_action_t action;     /**< Action type */
} health_action_item_t;

static health_action_item_t health_actions[] = {
    {"See Doctor", LV_SYMBOL_PLUS, "Visit doctor for checkup", HEALTH_ACTION_SEE_DOCTOR},
    {"Take Medicine", LV_SYMBOL_REFRESH, "Take prescribed medicine", HEALTH_ACTION_TAKE_MEDICINE},
    {"Check Symptoms", LV_SYMBOL_EYE_OPEN, "Check current symptoms", HEALTH_ACTION_CHECK_SYMPTOMS},
    {"Exercise", LV_SYMBOL_CHARGE, "Do physical exercise", HEALTH_ACTION_EXERCISE},
};

#define HEALTH_ACTIONS_COUNT (sizeof(health_actions) / sizeof(health_actions[0]))

// UI Constants
#define STAT_CONTAINER_HEIGHT 30
#define STAT_CONTAINER_WIDTH 320
#define SEPARATOR_HEIGHT 2

// External image declarations
LV_IMG_DECLARE(family_star);

/***********************************************************
********************function declaration********************
***********************************************************/

static void menu_health_screen_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);
static void create_health_status_display(void);
static void create_separator(void);
static void create_health_actions(void);
static void create_health_action_item(health_action_item_t *action, uint8_t index);
static void create_stat_icon_bar(const char *label, int value);
static void create_stat_display_item(const char *label, const char *value);
static void update_selection(uint8_t old_selection, uint8_t new_selection);
static void handle_health_selection(void);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Timer callback for the health menu screen
 *
 * This function is called when the health menu timer expires.
 * It can be used for periodic health updates.
 *
 * @param timer The timer object
 */
static void menu_health_screen_timer_cb(lv_timer_t *timer)
{
    printf("[%s] health menu timer callback\n", menu_health_screen.name);
    // Add any periodic update logic here
}

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the health menu screen.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", menu_health_screen.name, key);

    uint32_t child_count = lv_obj_get_child_cnt(menu_health_screen_list);
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
            handle_health_selection();
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
 * @brief Create health status display
 */
static void create_health_status_display(void)
{
    // Status title
    lv_obj_t *status_title = lv_label_create(menu_health_screen_list);
    lv_label_set_text(status_title, "Health Status:");
    lv_obj_align(status_title, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(status_title, lv_color_black(), 0);
    lv_obj_set_style_text_font(status_title, &lv_font_montserrat_14, 0);
    lv_obj_add_flag(status_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(status_title, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    create_stat_icon_bar("Health:", current_health_status.health_level);
    create_stat_icon_bar("Energy:", current_health_status.energy_level);

    // Overall condition indicator
    lv_obj_t *condition_container = lv_obj_create(menu_health_screen_list);
    lv_obj_set_size(condition_container, STAT_CONTAINER_WIDTH, STAT_CONTAINER_HEIGHT);
    lv_obj_set_style_bg_opa(condition_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(condition_container, 0, 0);
    lv_obj_set_style_pad_all(condition_container, 2, 0);

    lv_obj_t *condition_label = lv_label_create(condition_container);
    lv_label_set_text(condition_label, "Condition:");
    lv_obj_align(condition_label, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_text_color(condition_label, lv_color_black(), 0);

    lv_obj_t *condition_status = lv_label_create(condition_container);
    const char *condition_text = current_health_status.is_sick ? "Sick" :
                                current_health_status.needs_doctor ? "Needs Checkup" : "Healthy";
    lv_label_set_text(condition_status, condition_text);
    lv_obj_align(condition_status, LV_ALIGN_RIGHT_MID, -5, 0);

    lv_color_t condition_color = current_health_status.is_sick ? lv_color_make(255, 0, 0) :
                                current_health_status.needs_doctor ? lv_color_make(255, 165, 0) :
                                lv_color_make(0, 128, 0);
    lv_obj_set_style_text_color(condition_status, condition_color, 0);

    // Symptoms display if any
    if (strlen(current_health_status.symptoms) > 0) {
        create_stat_display_item("Symptoms:", current_health_status.symptoms);
    }
}

/**
 * @brief Create a visual separator
 */
static void create_separator(void)
{
    lv_obj_t *separator = lv_obj_create(menu_health_screen_list);
    lv_obj_set_size(separator, STAT_CONTAINER_WIDTH, SEPARATOR_HEIGHT);
    lv_obj_set_style_bg_color(separator, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(separator, LV_OPA_50, 0);
}

/**
 * @brief Create health actions section
 */
static void create_health_actions(void)
{
    // Actions title
    lv_obj_t *actions_title = lv_label_create(menu_health_screen_list);
    lv_label_set_text(actions_title, "Health Actions:");
    lv_obj_align(actions_title, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(actions_title, lv_color_black(), 0);
    lv_obj_set_style_text_font(actions_title, &lv_font_montserrat_14, 0);
    lv_obj_add_flag(actions_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(actions_title, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    for (uint8_t i = 0; i < HEALTH_ACTIONS_COUNT; i++) {
        create_health_action_item(&health_actions[i], i);
    }
}

/**
 * @brief Create a single health action item
 */
static void create_health_action_item(health_action_item_t *action, uint8_t index)
{
    lv_obj_t *btn = lv_list_add_btn(menu_health_screen_list, action->icon, action->name);

    // Add description as a subtitle
    lv_obj_t *desc_label = lv_label_create(btn);
    lv_label_set_text(desc_label, action->description);
    lv_obj_align(desc_label, LV_ALIGN_BOTTOM_LEFT, 30, -2);
    lv_obj_set_style_text_color(desc_label, lv_color_make(128, 128, 128), 0);
    lv_obj_set_style_text_font(desc_label, &lv_font_montserrat_10, 0);
}

/**
 * @brief Create a stat display with icon bar using family_star icons
 */
static void create_stat_icon_bar(const char *label, int value)
{
    lv_obj_t *container = lv_obj_create(menu_health_screen_list);
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
            lv_obj_align(icon, LV_ALIGN_LEFT_MID, 100 + i * 22, 0);
        }
    }

    // Add x/5 text after the icons
    char stat_text[8];
    snprintf(stat_text, sizeof(stat_text), "%d/5", filled);
    lv_obj_t *stat_label = lv_label_create(container);
    lv_label_set_text(stat_label, stat_text);
    lv_obj_align(stat_label, LV_ALIGN_LEFT_MID, 100 + 5 * 22 + 8, 0);
    lv_obj_set_style_text_color(stat_label, lv_color_black(), 0);
}

/**
 * @brief Create a simple stat display item with label and value
 */
static void create_stat_display_item(const char *label, const char *value)
{
    lv_obj_t *container = lv_obj_create(menu_health_screen_list);
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
 * @brief Update visual selection highlighting
 */
static void update_selection(uint8_t old_selection, uint8_t new_selection)
{
    uint32_t child_count = lv_obj_get_child_cnt(menu_health_screen_list);

    if (old_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(menu_health_screen_list, old_selection), lv_color_white(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(menu_health_screen_list, old_selection), lv_color_black(), 0);
    }

    if (new_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(menu_health_screen_list, new_selection), lv_color_black(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(menu_health_screen_list, new_selection), lv_color_white(), 0);
        lv_obj_scroll_to_view(lv_obj_get_child(menu_health_screen_list, new_selection), LV_ANIM_ON);
    }
}

/**
 * @brief Handle health action selection and trigger callback
 */
static void handle_health_selection(void)
{
    uint32_t child_count = lv_obj_get_child_cnt(menu_health_screen_list);

    // Find action items start (after status displays and separator)
    uint32_t action_start = 0;
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(menu_health_screen_list, i);
        if (lv_obj_check_type(child, &lv_label_class)) {
            const char *text = lv_label_get_text(child);
            if (strcmp(text, "Health Actions:") == 0) {
                action_start = i + 1;
                break;
            }
        }
    }

    if (selected_item >= action_start) {
        uint32_t action_index = selected_item - action_start;

        if (action_index < HEALTH_ACTIONS_COUNT) {
            health_action_item_t *selected_action = &health_actions[action_index];

            printf("Selected health action: %s - %s\n",
                   selected_action->name, selected_action->description);

            // Trigger callback
            if (health_callback) {
                health_callback(selected_action->action, health_callback_user_data);
            }

            // Update health status based on action
            switch (selected_action->action) {
                case HEALTH_ACTION_SEE_DOCTOR:
                    current_health_status.health_level = 100;
                    current_health_status.is_sick = false;
                    current_health_status.needs_doctor = false;
                    strcpy(current_health_status.symptoms, "");
                    break;
                case HEALTH_ACTION_TAKE_MEDICINE:
                    current_health_status.health_level += 20;
                    if (current_health_status.health_level > 100) {
                        current_health_status.health_level = 100;
                    }
                    if (current_health_status.health_level >= 80) {
                        current_health_status.is_sick = false;
                        strcpy(current_health_status.symptoms, "");
                    }
                    break;
                case HEALTH_ACTION_CHECK_SYMPTOMS:
                    if (current_health_status.health_level < 50) {
                        strcpy(current_health_status.symptoms, "Low energy, needs rest");
                    } else if (current_health_status.health_level < 80) {
                        strcpy(current_health_status.symptoms, "Mild fatigue");
                    } else {
                        strcpy(current_health_status.symptoms, "No symptoms");
                    }
                    break;
                case HEALTH_ACTION_EXERCISE:
                    current_health_status.energy_level += 15;
                    current_health_status.health_level += 5;
                    if (current_health_status.energy_level > 100) {
                        current_health_status.energy_level = 100;
                    }
                    if (current_health_status.health_level > 100) {
                        current_health_status.health_level = 100;
                    }
                    break;
            }

            // Refresh the display to show updated status
            menu_health_screen_deinit();
            menu_health_screen_init();
        }
    }
}

/**
 * @brief Initialize the health menu screen
 *
 * This function creates the health menu UI with health status
 * and medical care options.
 */
void menu_health_screen_init(void)
{
    ui_menu_health_screen_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_menu_health_screen_screen, 384, 168);
    lv_obj_set_style_bg_color(ui_menu_health_screen_screen, lv_color_white(), 0);

    // Title at the top
    lv_obj_t *title = lv_label_create(ui_menu_health_screen_screen);
    lv_label_set_text(title, "Health & Medical");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);

    // List for health menu items
    menu_health_screen_list = lv_list_create(ui_menu_health_screen_screen);
    lv_obj_set_size(menu_health_screen_list, 364, 128);
    lv_obj_align(menu_health_screen_list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_add_flag(menu_health_screen_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(menu_health_screen_list, LV_DIR_VER);

    lv_obj_set_style_border_color(menu_health_screen_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(menu_health_screen_list, 2, 0);

    // Create all UI components
    create_health_status_display();
    create_separator();
    create_health_actions();

    // Highlight first item
    selected_item = 0;
    if (lv_obj_get_child_cnt(menu_health_screen_list) > 0) {
        update_selection(0, 0);
    }

    timer = lv_timer_create(menu_health_screen_timer_cb, 1000, NULL);
    lv_obj_add_event_cb(ui_menu_health_screen_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_menu_health_screen_screen);
    lv_group_focus_obj(ui_menu_health_screen_screen);
}

/**
 * @brief Deinitialize the health menu screen
 *
 * This function cleans up the health menu by removing event callbacks
 * and freeing resources.
 */
void menu_health_screen_deinit(void)
{
    if (ui_menu_health_screen_screen) {
        printf("deinit health menu screen\n");
        lv_obj_remove_event_cb(ui_menu_health_screen_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_menu_health_screen_screen);
    }
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }
}

/**
 * @brief Set health status for display
 *
 * @param status Pointer to health status structure
 */
void menu_health_screen_set_health_status(health_status_t *status)
{
    if (status) {
        current_health_status = *status;
    }
}

/**
 * @brief Get current health status
 *
 * @return Pointer to current health status
 */
health_status_t* menu_health_screen_get_health_status(void)
{
    return &current_health_status;
}

/**
 * @brief Register health event callback
 *
 * @param callback Callback function for health events
 * @param user_data User data passed to callback
 */
void menu_health_screen_register_callback(health_event_callback_t callback, void *user_data)
{
    health_callback = callback;
    health_callback_user_data = user_data;
}
