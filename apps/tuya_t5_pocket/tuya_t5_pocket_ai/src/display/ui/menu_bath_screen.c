/**
 * @file menu_bath_screen.c
 * @brief Implementation of the bath menu screen for the application
 *
 * This file contains the implementation of the bath menu screen which displays
 * cleaning and hygiene options for the pet.
 *
 * The bath menu includes:
 * - Toilet and bathroom facilities
 * - Bath and cleaning options with visual feedback
 * - Hygiene status indicators
 * - Keyboard event handling for navigation
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "menu_bath_screen.h"
#include "screen_manager.h"
#include "main_screen.h"
#include "toast_screen.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_menu_bath_screen_screen;
static lv_obj_t *menu_bath_screen_list;
static lv_timer_t *timer;
static lv_timer_t *pet_state_timer;  // Timer for animation
static uint8_t selected_item = 0;
static uint8_t last_selected_item = 0;
static hygiene_status_t current_hygiene_status;
static bath_event_callback_t bath_callback = NULL;
static void *bath_callback_user_data = NULL;

Screen_t menu_bath_screen = {
    .init = menu_bath_screen_init,
    .deinit = menu_bath_screen_deinit,
    .screen_obj = &ui_menu_bath_screen_screen,
    .name = "bath_menu",
};

// Bath actions configuration
typedef struct {
    const char *name;           /**< Action name */
    const char *icon;           /**< Action icon symbol */
    const char *description;    /**< Action description */
    bath_action_t action;       /**< Action type */
} bath_action_item_t;

static bath_action_item_t bath_actions[] = {
    {"Toilet", LV_SYMBOL_HOME, "Use the toilet", BATH_ACTION_TOILET},
    {"Take Bath", LV_SYMBOL_REFRESH, "Take a refreshing bath", BATH_ACTION_TAKE_BATH},
    {"Brush Teeth", LV_SYMBOL_EDIT, "Brush teeth for oral hygiene", BATH_ACTION_BRUSH_TEETH},
    {"Wash Hands", LV_SYMBOL_REFRESH, "Wash hands for cleanliness", BATH_ACTION_WASH_HANDS},
};

#define BATH_ACTIONS_COUNT (sizeof(bath_actions) / sizeof(bath_actions[0]))

// UI Constants
#define STAT_CONTAINER_HEIGHT 30
#define STAT_CONTAINER_WIDTH 320
#define SEPARATOR_HEIGHT 2

// External image declarations
LV_IMG_DECLARE(family_star);

/***********************************************************
********************function declaration********************
***********************************************************/

static void menu_bath_screen_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);
static void create_hygiene_status_display(void);
static void create_separator(void);
static void create_bath_actions(void);
static void create_bath_action_item(bath_action_item_t *action, uint8_t index);
static void create_stat_icon_bar(const char *label, int value);
static void update_selection(uint8_t old_selection, uint8_t new_selection);
static void handle_bath_selection(void);

static bool is_child_selectable(lv_obj_t *child)
{
    if (child == NULL) return false;
    return lv_obj_has_flag(child, LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Timer callback for the bath menu screen
 *
 * This function is called when the bath menu timer expires.
 * It can be used for periodic updates.
 *
 * @param timer The timer object
 */
static void menu_bath_screen_timer_cb(lv_timer_t *timer)
{
    printf("[%s] bath menu timer callback\n", menu_bath_screen.name);
    // Add any periodic update logic here
}

/**
 * @brief Timer callback for animation
 *
 * This function is called after animation to switch back to normal state.
 *
 * @param timer The timer object
 */
static void pet_state_timer_cb(lv_timer_t *timer)
{
    printf("[%s] animation timer callback - switching to normal state\n", menu_bath_screen.name);

    // Switch pet back to normal state
    main_screen_set_pet_animation_state(AI_PET_STATE_NORMAL);

    // Clean up the timer
    if (pet_state_timer) {
        lv_timer_del(pet_state_timer);
        pet_state_timer = NULL;
    }
}

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the bath menu screen.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", menu_bath_screen.name, key);

    uint32_t child_count = lv_obj_get_child_cnt(menu_bath_screen_list);
    if (child_count == 0) return;

    uint8_t old_selection = selected_item;
    uint8_t new_selection = old_selection;
    switch (key) {
        case KEY_UP:
            // Move to previous item (including non-selectable ones for scrolling)
            if (selected_item > 0) {
                new_selection = selected_item - 1;
                // Find the nearest selectable item going up
                for (int i = (int)new_selection; i >= 0; --i) {
                    lv_obj_t *ch = lv_obj_get_child(menu_bath_screen_list, i);
                    if (is_child_selectable(ch)) {
                        new_selection = (uint8_t)i;
                        break;
                    }
                }
            }
            break;
        case KEY_DOWN:
            // Move to next item (including non-selectable ones for scrolling)
            if (selected_item < child_count - 1) {
                new_selection = selected_item + 1;
                // Find the nearest selectable item going down
                for (uint32_t i = new_selection; i < child_count; ++i) {
                    lv_obj_t *ch = lv_obj_get_child(menu_bath_screen_list, i);
                    if (is_child_selectable(ch)) {
                        new_selection = (uint8_t)i;
                        break;
                    }
                }
            }
            break;
        case KEY_ENTER:
            handle_bath_selection();
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
 * @brief Create hygiene status display
 */
static void create_hygiene_status_display(void)
{
    // Status title
    lv_obj_t *status_title = lv_label_create(menu_bath_screen_list);
    lv_label_set_text(status_title, "Hygiene Status:");
    lv_obj_align(status_title, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(status_title, lv_color_black(), 0);
    lv_obj_set_style_text_font(status_title, &lv_font_montserrat_14, 0);
    lv_obj_add_flag(status_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(status_title, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    create_stat_icon_bar("Cleanliness:", current_hygiene_status.cleanliness);
    create_stat_icon_bar("Toilet Need:", current_hygiene_status.toilet_need);

    // Bath status indicator
    lv_obj_t *bath_container = lv_obj_create(menu_bath_screen_list);
    lv_obj_set_size(bath_container, STAT_CONTAINER_WIDTH, STAT_CONTAINER_HEIGHT);
    lv_obj_set_style_bg_opa(bath_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bath_container, 0, 0);
    lv_obj_set_style_pad_all(bath_container, 2, 0);

    lv_obj_t *bath_label = lv_label_create(bath_container);
    lv_label_set_text(bath_label, "Bath Status:");
    lv_obj_align(bath_label, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_text_color(bath_label, lv_color_black(), 0);

    lv_obj_t *bath_status = lv_label_create(bath_container);
    lv_label_set_text(bath_status, current_hygiene_status.needs_bath ? "Needs Bath" : "Clean");
    lv_obj_align(bath_status, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_text_color(bath_status,
                                current_hygiene_status.needs_bath ? lv_color_make(255, 0, 0) : lv_color_make(0, 128, 0), 0);
}

/**
 * @brief Create a visual separator
 */
static void create_separator(void)
{
    lv_obj_t *separator = lv_obj_create(menu_bath_screen_list);
    lv_obj_set_size(separator, STAT_CONTAINER_WIDTH, SEPARATOR_HEIGHT);
    lv_obj_set_style_bg_color(separator, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(separator, LV_OPA_50, 0);
}

/**
 * @brief Create bath actions section
 */
static void create_bath_actions(void)
{
    // Actions title
    lv_obj_t *actions_title = lv_label_create(menu_bath_screen_list);
    lv_label_set_text(actions_title, "Bath Actions:");
    lv_obj_align(actions_title, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(actions_title, lv_color_black(), 0);
    lv_obj_set_style_text_font(actions_title, &lv_font_montserrat_14, 0);
    lv_obj_add_flag(actions_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(actions_title, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    for (uint8_t i = 0; i < BATH_ACTIONS_COUNT; i++) {
        create_bath_action_item(&bath_actions[i], i);
    }
}

/**
 * @brief Create a single bath action item
 */
static void create_bath_action_item(bath_action_item_t *action, uint8_t index)
{
    lv_obj_t *btn = lv_list_add_btn(menu_bath_screen_list, action->icon, action->name);

    // Add status info on the right side
    lv_obj_t *info_label = lv_label_create(btn);
    char info_text[32];
    switch (index) {
        case 0:  // Toilet
            snprintf(info_text, sizeof(info_text), "T:-100");
            break;
        case 1:  // Take Bath
            snprintf(info_text, sizeof(info_text), "C:+100");
            break;
        case 2:  // Brush Teeth
            snprintf(info_text, sizeof(info_text), "C:+10");
            break;
        case 3:  // Wash Hands
            snprintf(info_text, sizeof(info_text), "C:+5");
            break;
        default:
            snprintf(info_text, sizeof(info_text), " ");
            break;
    }
    lv_label_set_text(info_label, info_text);
    lv_obj_align(info_label, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_text_color(info_label, lv_color_make(0, 128, 0), 0);
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_10, 0);
}

/**
 * @brief Create a stat display with icon bar using family_star icons
 */
static void create_stat_icon_bar(const char *label, int value)
{
    lv_obj_t *container = lv_obj_create(menu_bath_screen_list);
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
 * @brief Update visual selection highlighting
 */
static void update_selection(uint8_t old_selection, uint8_t new_selection)
{
    uint32_t child_count = lv_obj_get_child_cnt(menu_bath_screen_list);
    // Un-highlight nearest selectable old child
    if (old_selection < child_count) {
        for (int i = old_selection; i >= 0; --i) {
            lv_obj_t *ch = lv_obj_get_child(menu_bath_screen_list, i);
            if (is_child_selectable(ch)) {
                lv_obj_set_style_bg_color(ch, lv_color_white(), 0);
                lv_obj_set_style_text_color(ch, lv_color_black(), 0);
                break;
            }
        }
    }

    // Highlight nearest selectable new child
    if (new_selection < child_count) {
        for (uint32_t i = new_selection; i < child_count; ++i) {
            lv_obj_t *ch = lv_obj_get_child(menu_bath_screen_list, i);
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
 * @brief Handle bath action selection and trigger callback
 */
static void handle_bath_selection(void)
{
    uint32_t child_count = lv_obj_get_child_cnt(menu_bath_screen_list);

    // Find action items start (after status displays and separator)
    uint32_t action_start = 0;
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(menu_bath_screen_list, i);
        if (lv_obj_check_type(child, &lv_label_class)) {
            const char *text = lv_label_get_text(child);
            if (strcmp(text, "Bath Actions:") == 0) {
                action_start = i + 1;
                break;
            }
        }
    }

    if (selected_item >= action_start) {
        uint32_t action_index = selected_item - action_start;

        if (action_index < BATH_ACTIONS_COUNT) {
            bath_action_item_t *selected_action = &bath_actions[action_index];

            printf("Selected bath action: %s - %s\n",
                   selected_action->name, selected_action->description);

            // Handle different bath actions with specific logic
            switch (action_index) {
                case 0:  // Toilet - only this one has animation
                    printf("Toilet - returning to main screen and playing animation\n");
                    current_hygiene_status.toilet_need = 0;

                    // Trigger callback
                    if (bath_callback) {
                        bath_callback(selected_action->action, bath_callback_user_data);
                    }

                    // Return to main screen and play animation
                    screen_back();
                    main_screen_set_pet_animation_state(AI_PET_STATE_TOILET);  // Using toilet animation

                    // Start timer to switch back to normal state after 2 seconds
                    if (pet_state_timer) {
                        lv_timer_del(pet_state_timer);  // Clean up existing timer
                    }
                    pet_state_timer = lv_timer_create(pet_state_timer_cb, 3000, NULL);

                    printf("Started bath animation timer\n");
                    break;

                case 1:  // Take Bath
                    printf("Take Bath selected - showing toast\n");
                    current_hygiene_status.toilet_need = 0;

                    // Trigger callback
                    if (bath_callback) {
                        bath_callback(selected_action->action, bath_callback_user_data);
                    }

                    // Return to main screen and play animation
                    screen_back();
                    main_screen_set_pet_animation_state(AI_PET_STATE_BATH);  // Using bath animation

                    // Start timer to switch back to normal state after 2 seconds
                    if (pet_state_timer) {
                        lv_timer_del(pet_state_timer);  // Clean up existing timer
                    }
                    pet_state_timer = lv_timer_create(pet_state_timer_cb, 3000, NULL);

                    printf("Started bath animation timer\n");
                    break;

                case 2:  // Brush Teeth
                    printf("Brush Teeth selected - showing toast\n");
                    toast_screen_show("Coming Soon: Brush Teeth Feature", 2000);
                    break;

                case 3:  // Wash Hands
                    printf("Wash Hands selected - showing toast\n");
                    toast_screen_show("Coming Soon: Wash Hands Feature", 2000);
                    break;

                default:
                    printf("Unknown bath action: %d\n", action_index);
                    toast_screen_show("Unknown Action", 2000);
                    break;
            }
        }
    }
}

/**
 * @brief Initialize the bath menu screen
 *
 * This function creates the bath menu UI with cleaning options
 * and hygiene status display.
 */
void menu_bath_screen_init(void)
{
    ui_menu_bath_screen_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_menu_bath_screen_screen, 384, 168);
    lv_obj_set_style_bg_color(ui_menu_bath_screen_screen, lv_color_white(), 0);

    // Title at the top
    lv_obj_t *title = lv_label_create(ui_menu_bath_screen_screen);
    lv_label_set_text(title, "Bath & Hygiene");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);

    // List for bath menu items
    menu_bath_screen_list = lv_list_create(ui_menu_bath_screen_screen);
    lv_obj_set_size(menu_bath_screen_list, 364, 128);
    lv_obj_align(menu_bath_screen_list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_add_flag(menu_bath_screen_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(menu_bath_screen_list, LV_DIR_VER);

    lv_obj_set_style_border_color(menu_bath_screen_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(menu_bath_screen_list, 2, 0);

    // Create all UI components
    create_hygiene_status_display();
    create_separator();
    create_bath_actions();

    // Restore last selected item or find first selectable child
    selected_item = last_selected_item;
    uint32_t child_count = lv_obj_get_child_cnt(menu_bath_screen_list);

    // Validate selected_item is within bounds and selectable
    if (selected_item >= child_count) {
        selected_item = 0;
        last_selected_item = 0;
    }

    // Find first selectable child if current selected_item is 0 or not selectable
    bool found_selectable = false;
    if (child_count > 0) {
        if (selected_item > 0 && selected_item < child_count) {
            lv_obj_t *current_child = lv_obj_get_child(menu_bath_screen_list, selected_item);
            if (is_child_selectable(current_child)) {
                found_selectable = true;
            }
        }

        if (!found_selectable) {
            for (uint32_t i = 0; i < child_count; ++i) {
                lv_obj_t *ch = lv_obj_get_child(menu_bath_screen_list, i);
                if (is_child_selectable(ch)) {
                    selected_item = (uint8_t)i;
                    found_selectable = true;
                    break;
                }
            }
        }

        if (found_selectable) {
            update_selection(0, selected_item);
        }
    }

    timer = lv_timer_create(menu_bath_screen_timer_cb, 1000, NULL);
    lv_obj_add_event_cb(ui_menu_bath_screen_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_menu_bath_screen_screen);
    lv_group_focus_obj(ui_menu_bath_screen_screen);
}

/**
 * @brief Deinitialize the bath menu screen
 *
 * This function cleans up the bath menu by removing event callbacks
 * and freeing resources.
 */
void menu_bath_screen_deinit(void)
{
    if (ui_menu_bath_screen_screen) {
        printf("deinit bath menu screen\n");
        lv_obj_remove_event_cb(ui_menu_bath_screen_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_menu_bath_screen_screen);
    }
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }
    if (pet_state_timer) {
        lv_timer_del(pet_state_timer);
        pet_state_timer = NULL;
    }
}

/**
 * @brief Set hygiene status for display
 *
 * @param status Pointer to hygiene status structure
 */
void menu_bath_screen_set_hygiene_status(hygiene_status_t *status)
{
    if (status) {
        current_hygiene_status = *status;
    }
}

/**
 * @brief Get current hygiene status
 *
 * @return Pointer to current hygiene status
 */
hygiene_status_t* menu_bath_screen_get_hygiene_status(void)
{
    return &current_hygiene_status;
}

/**
 * @brief Register bath event callback
 *
 * @param callback Callback function for bath events
 * @param user_data User data passed to callback
 */
void menu_bath_screen_register_callback(bath_event_callback_t callback, void *user_data)
{
    bath_callback = callback;
    bath_callback_user_data = user_data;
}
