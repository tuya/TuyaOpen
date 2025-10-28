/**
 * @file menu_sleep_screen.c
 * @brief Implementation of the sleep menu screen
 */

#include "menu_sleep_screen.h"
#include "screen_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static lv_obj_t *ui_menu_sleep_screen_screen;
static lv_obj_t *menu_sleep_screen_list;
static lv_timer_t *timer;
static uint8_t selected_item = 0;
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

static void menu_sleep_screen_timer_cb(lv_timer_t *timer)
{
    printf("[%s] sleep menu timer callback\n", menu_sleep_screen.name);
}

static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    uint32_t child_count = lv_obj_get_child_cnt(menu_sleep_screen_list);
    if (child_count == 0) return;

    uint8_t old_selection = selected_item;
    uint8_t new_selection = old_selection;

    switch (key) {
        case KEY_UP:
            if (selected_item > 0) new_selection = selected_item - 1;
            break;
        case KEY_DOWN:
            if (selected_item < child_count - 1) new_selection = selected_item + 1;
            break;
        case KEY_ENTER:
            handle_sleep_selection();
            break;
        case KEY_ESC:
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
}

static void create_sleep_actions(void)
{
    lv_obj_t *actions_title = lv_label_create(menu_sleep_screen_list);
    lv_label_set_text(actions_title, "Sleep Actions:");
    lv_obj_set_style_text_font(actions_title, &lv_font_montserrat_14, 0);

    for (uint8_t i = 0; i < SLEEP_ACTIONS_COUNT; i++) {
        lv_list_add_btn(menu_sleep_screen_list, sleep_actions[i].icon, sleep_actions[i].name);
    }
}

static void update_selection(uint8_t old_selection, uint8_t new_selection)
{
    uint32_t child_count = lv_obj_get_child_cnt(menu_sleep_screen_list);

    if (old_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(menu_sleep_screen_list, old_selection), lv_color_white(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(menu_sleep_screen_list, old_selection), lv_color_black(), 0);
    }

    if (new_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(menu_sleep_screen_list, new_selection), lv_color_black(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(menu_sleep_screen_list, new_selection), lv_color_white(), 0);
        lv_obj_scroll_to_view(lv_obj_get_child(menu_sleep_screen_list, new_selection), LV_ANIM_ON);
    }
}

static void handle_sleep_selection(void)
{
    printf("Sleep action selected at index %d\n", selected_item);
    // if (sleep_callback) {
    //     sleep_callback(SLEEP_ACTION_SLEEP, sleep_callback_user_data);
    // }
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
    lv_obj_set_style_border_color(menu_sleep_screen_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(menu_sleep_screen_list, 2, 0);

    create_sleep_status_display();
    create_separator();
    create_sleep_actions();

    selected_item = 0;
    if (lv_obj_get_child_cnt(menu_sleep_screen_list) > 0) {
        update_selection(0, 0);
    }

    timer = lv_timer_create(menu_sleep_screen_timer_cb, 1000, NULL);
    lv_obj_add_event_cb(ui_menu_sleep_screen_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_menu_sleep_screen_screen);
    lv_group_focus_obj(ui_menu_sleep_screen_screen);
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
}

void menu_sleep_screen_set_sleep_status(sleep_status_t *status)
{
    if (status) current_sleep_status = *status;
}

sleep_status_t* menu_sleep_screen_get_sleep_status(void)
{
    return &current_sleep_status;
}

