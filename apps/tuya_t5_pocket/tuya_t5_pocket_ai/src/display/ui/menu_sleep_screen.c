/**
 * @file menu_sleep_screen.c
 * @brief Implementation of the sleep menu screen
 */

#include "menu_sleep_screen.h"
#include "screen_manager.h"
#include "main_screen.h"
#include "toast_screen.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static lv_obj_t *ui_menu_sleep_screen_screen;
static lv_obj_t *menu_sleep_screen_list;
static lv_timer_t *timer;
static lv_timer_t *pet_state_timer;  // Timer for animation
static uint8_t selected_item = 0;
static int last_selected_item = -1;
static sleep_status_t current_sleep_status = {false, 80, 8, 22};
// static sleep_event_callback_t sleep_callback = NULL;
// static void *sleep_callback_user_data = NULL;

Screen_t menu_sleep_screen = {
    .init = menu_sleep_screen_init,
    .deinit = menu_sleep_screen_deinit,
    .screen_obj = &ui_menu_sleep_screen_screen,
    .name = "sleep_menu",
};

LV_IMG_DECLARE(family_star);

typedef struct {
    const char *name;
    const char *icon;
    const char *description;
    sleep_action_t action;
} sleep_action_item_t;

static sleep_action_item_t sleep_actions[] = {
    {"Sleep", LV_SYMBOL_POWER, "Go to sleep", SLEEP_ACTION_SLEEP},
    {"Wake Up", LV_SYMBOL_REFRESH, "Wake up from sleep", SLEEP_ACTION_WAKE_UP},
    {"Set Bedtime", LV_SYMBOL_SETTINGS, "Set bedtime schedule", SLEEP_ACTION_SET_BEDTIME},
    {"Sleep Status", LV_SYMBOL_EYE_OPEN, "Check sleep quality", SLEEP_ACTION_CHECK_SLEEP_STATUS},
};

#define SLEEP_ACTIONS_COUNT (sizeof(sleep_actions) / sizeof(sleep_actions[0]))

static void menu_sleep_screen_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);
static void create_sleep_status_display(void);
static void create_separator(void);
static void create_sleep_actions(void);
static void update_selection(uint8_t old_selection, uint8_t new_selection);
static void handle_sleep_selection(void);
static bool is_child_selectable(lv_obj_t *child);

static void menu_sleep_screen_timer_cb(lv_timer_t *timer)
{
    printf("[%s] sleep menu timer callback\n", menu_sleep_screen.name);
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
    printf("[%s] animation timer callback - switching to normal state\n", menu_sleep_screen.name);

    // Switch pet back to normal state
    main_screen_set_pet_animation_state(AI_PET_STATE_NORMAL);

    // Clean up the timer
    if (pet_state_timer) {
        lv_timer_del(pet_state_timer);
        pet_state_timer = NULL;
    }
}

static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    uint32_t child_count = lv_obj_get_child_cnt(menu_sleep_screen_list);
    if (child_count == 0) return;

    uint8_t old_selection = selected_item;
    uint8_t new_selection = old_selection;

    switch (key) {
        case KEY_UP: {
            for (int i = (int)selected_item - 1; i >= 0; --i) {
                lv_obj_t *ch = lv_obj_get_child(menu_sleep_screen_list, i);
                if (is_child_selectable(ch)) {
                    new_selection = i;
                    break;
                }
            }
        } break;
        case KEY_DOWN: {
            for (int i = (int)selected_item + 1; i < (int)child_count; ++i) {
                lv_obj_t *ch = lv_obj_get_child(menu_sleep_screen_list, i);
                if (is_child_selectable(ch)) {
                    new_selection = i;
                    break;
                }
            }
        } break;
        case KEY_ENTER:
            handle_sleep_selection();
            break;
        case KEY_ESC:
            last_selected_item = 0;
            screen_back();
            break;
    }

    if (new_selection != old_selection) {
        update_selection(old_selection, new_selection);
        selected_item = new_selection;
    }
}

static void create_sleep_status_display(void)
{
    lv_obj_t *status_title = lv_label_create(menu_sleep_screen_list);
    lv_label_set_text(status_title, "Sleep Status:");
    lv_obj_set_style_text_font(status_title, &lv_font_montserrat_14, 0);
    /* Ensure title is not focusable/selectable */
    lv_obj_add_flag(status_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(status_title, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    // Sleep state
    // lv_obj_t *btn = lv_list_add_btn(menu_sleep_screen_list, LV_SYMBOL_HOME,
    //                                current_sleep_status.is_sleeping ? "Currently Sleeping" : "Awake");

    // Sleep quality
    char quality_text[32];
    snprintf(quality_text, sizeof(quality_text), "Sleep Quality: %d/100", current_sleep_status.sleep_quality);
    lv_list_add_btn(menu_sleep_screen_list, LV_SYMBOL_BATTERY_FULL, quality_text);

    // Bedtime
    char bedtime_text[32];
    snprintf(bedtime_text, sizeof(bedtime_text), "Bedtime: %02d:00", current_sleep_status.bedtime_hour);
    lv_list_add_btn(menu_sleep_screen_list, LV_SYMBOL_SETTINGS, bedtime_text);
}

static void create_separator(void)
{
    lv_obj_t *separator = lv_obj_create(menu_sleep_screen_list);
    lv_obj_set_size(separator, 320, 2);
    lv_obj_set_style_bg_color(separator, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(separator, LV_OPA_50, 0);
    /* Make separator non-selectable */
    lv_obj_add_flag(separator, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(separator, LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

static void create_sleep_actions(void)
{
    lv_obj_t *actions_title = lv_label_create(menu_sleep_screen_list);
    lv_label_set_text(actions_title, "Sleep Actions:");
    lv_obj_set_style_text_font(actions_title, &lv_font_montserrat_14, 0);
    /* Ensure actions title is not selectable */
    lv_obj_add_flag(actions_title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(actions_title, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    for (uint8_t i = 0; i < SLEEP_ACTIONS_COUNT; i++) {
        lv_obj_t *btn = lv_list_add_btn(menu_sleep_screen_list, sleep_actions[i].icon, sleep_actions[i].name);
        /* Make sure action buttons are focusable/selectable */
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);

        // Add status info on the right side
        lv_obj_t *info_label = lv_label_create(btn);
        char info_text[32];
        switch (i) {
            case 0:  // Sleep
                snprintf(info_text, sizeof(info_text), "Rest");
                break;
            case 1:  // Wake Up
                snprintf(info_text, sizeof(info_text), "E:+50");
                break;
            case 2:  // Set Bedtime
                snprintf(info_text, sizeof(info_text), "Settings");
                break;
            case 3:  // Sleep Status
                snprintf(info_text, sizeof(info_text), "Info");
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
}

static void update_selection(uint8_t old_selection, uint8_t new_selection)
{
    uint32_t child_count = lv_obj_get_child_cnt(menu_sleep_screen_list);
    // Un-highlight nearest selectable old child
    if (old_selection < child_count) {
        for (int i = old_selection; i >= 0; --i) {
            lv_obj_t *ch = lv_obj_get_child(menu_sleep_screen_list, i);
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
            lv_obj_t *ch = lv_obj_get_child(menu_sleep_screen_list, i);
            if (is_child_selectable(ch)) {
                lv_obj_set_style_bg_color(ch, lv_color_black(), 0);
                lv_obj_set_style_text_color(ch, lv_color_white(), 0);
                lv_obj_scroll_to_view(ch, LV_ANIM_ON);
                break;
            }
        }
    }
}

static void handle_sleep_selection(void)
{
    uint32_t child_count = lv_obj_get_child_cnt(menu_sleep_screen_list);

    // Find action items start (after status displays and separator)
    uint32_t action_start = 0;
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(menu_sleep_screen_list, i);
        if (lv_obj_check_type(child, &lv_label_class)) {
            const char *text = lv_label_get_text(child);
            if (strcmp(text, "Sleep Actions:") == 0) {
                action_start = i + 1;
                break;
            }
        }
    }

    if (selected_item >= action_start) {
        uint32_t action_index = selected_item - action_start;

        if (action_index < SLEEP_ACTIONS_COUNT) {
            sleep_action_item_t *selected_action = &sleep_actions[action_index];

            printf("Selected sleep action: %s - %s\n",
                   selected_action->name, selected_action->description);

            // Handle different sleep actions with specific logic
            switch (action_index) {
                case 0:  // Sleep - only this one has animation
                    printf("Sleep - returning to main screen and playing animation\n");
                    current_sleep_status.is_sleeping = true;

                    // Return to main screen and play sleep animation
                    screen_back();
                    main_screen_set_pet_animation_state(AI_PET_STATE_SLEEP);

                    // Start timer to switch back to normal state after 2 seconds
                    if (pet_state_timer) {
                        lv_timer_del(pet_state_timer);  // Clean up existing timer
                    }
                    pet_state_timer = lv_timer_create(pet_state_timer_cb, 3000, NULL);

                    printf("Started sleep animation timer\n");
                    break;

                case 1:  // Wake Up
                    printf("Wake Up selected - showing toast\n");
                    toast_screen_show("Coming Soon: Wake Up Feature", 2000);
                    break;

                case 2:  // Set Bedtime
                    printf("Set Bedtime selected - showing toast\n");
                    toast_screen_show("Coming Soon: Set Bedtime Feature", 2000);
                    break;

                case 3:  // Sleep Status
                    printf("Sleep Status selected - showing toast\n");
                    toast_screen_show("Coming Soon: Sleep Status Feature", 2000);
                    break;

                default:
                    printf("Unknown sleep action: %d\n", action_index);
                    toast_screen_show("Unknown Action", 2000);
                    break;
            }
        }
    }

    last_selected_item = selected_item;
}

void menu_sleep_screen_init(void)
{
    ui_menu_sleep_screen_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_menu_sleep_screen_screen, 384, 168);
    lv_obj_set_style_bg_color(ui_menu_sleep_screen_screen, lv_color_white(), 0);

    lv_obj_t *title = lv_label_create(ui_menu_sleep_screen_screen);
    lv_label_set_text(title, "Sleep & Rest");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    menu_sleep_screen_list = lv_list_create(ui_menu_sleep_screen_screen);
    lv_obj_set_size(menu_sleep_screen_list, 364, 128);
    lv_obj_align(menu_sleep_screen_list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_add_flag(menu_sleep_screen_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(menu_sleep_screen_list, LV_DIR_VER);

    lv_obj_set_style_border_color(menu_sleep_screen_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(menu_sleep_screen_list, 2, 0);

    create_sleep_status_display();
    create_separator();
    create_sleep_actions();

    // Restore last selected item or find first selectable child
    uint32_t child_count = lv_obj_get_child_cnt(menu_sleep_screen_list);
    if (last_selected_item >= 0 && (uint32_t)last_selected_item < child_count) {
        selected_item = (uint8_t)last_selected_item;
    } else {
        selected_item = 0;
        last_selected_item = 0;
    }

    // Validate selected_item is within bounds and selectable
    bool found_selectable = false;
    if (child_count > 0) {
        if (selected_item > 0 && selected_item < child_count) {
            lv_obj_t *current_child = lv_obj_get_child(menu_sleep_screen_list, selected_item);
            if (is_child_selectable(current_child)) {
                found_selectable = true;
            }
        }

        if (!found_selectable) {
            for (uint32_t i = 0; i < child_count; ++i) {
                lv_obj_t *ch = lv_obj_get_child(menu_sleep_screen_list, i);
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

    timer = lv_timer_create(menu_sleep_screen_timer_cb, 1000, NULL);
    lv_obj_add_event_cb(ui_menu_sleep_screen_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_menu_sleep_screen_screen);
    lv_group_focus_obj(ui_menu_sleep_screen_screen);
}

static bool is_child_selectable(lv_obj_t *child)
{
    if (!child) return false;
    return lv_obj_has_flag(child, LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

void menu_sleep_screen_deinit(void)
{
    if (ui_menu_sleep_screen_screen) {
        lv_obj_remove_event_cb(ui_menu_sleep_screen_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_menu_sleep_screen_screen);
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

void menu_sleep_screen_set_sleep_status(sleep_status_t *status)
{
    if (status) current_sleep_status = *status;
}

sleep_status_t* menu_sleep_screen_get_sleep_status(void)
{
    return &current_sleep_status;
}
