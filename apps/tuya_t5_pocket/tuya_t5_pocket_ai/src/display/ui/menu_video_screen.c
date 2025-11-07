/**
 * @file menu_video_screen.c
 * @brief Implementation of the video menu screen
 */

#include "menu_video_screen.h"
#include "screen_manager.h"
#include "toast_screen.h"
#include <stdio.h>

static lv_obj_t *ui_menu_video_screen_screen;
static lv_obj_t *menu_video_screen_list;
static lv_timer_t *timer;
static uint8_t selected_item = 0;
static uint8_t last_selected_item = 0;

Screen_t menu_video_screen = {
    .init = menu_video_screen_init,
    .deinit = menu_video_screen_deinit,
    .screen_obj = &ui_menu_video_screen_screen,
    .name = "video_menu",
};

typedef struct {
    const char *name;
    const char *icon;
    video_action_t action;
} video_action_item_t;

static video_action_item_t video_actions[] = {
    {"Play Video", LV_SYMBOL_PLAY, VIDEO_ACTION_PLAY_VIDEO},
    {"Take Photo", LV_SYMBOL_IMAGE, VIDEO_ACTION_TAKE_PHOTO},
    {"Record Video", LV_SYMBOL_VIDEO, VIDEO_ACTION_RECORD_VIDEO},
    {"View Gallery", LV_SYMBOL_DIRECTORY, VIDEO_ACTION_VIEW_GALLERY},
};

#define VIDEO_ACTIONS_COUNT (sizeof(video_actions) / sizeof(video_actions[0]))

static void menu_video_screen_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);
static void update_selection(uint8_t old_selection, uint8_t new_selection);
static void handle_video_selection(void);

static void menu_video_screen_timer_cb(lv_timer_t *timer)
{
    printf("[%s] video menu timer callback\n", menu_video_screen.name);
}

static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    uint32_t child_count = lv_obj_get_child_cnt(menu_video_screen_list);
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
            handle_video_selection();
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

static void update_selection(uint8_t old_selection, uint8_t new_selection)
{
    uint32_t child_count = lv_obj_get_child_cnt(menu_video_screen_list);

    if (old_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(menu_video_screen_list, old_selection), lv_color_white(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(menu_video_screen_list, old_selection), lv_color_black(), 0);
    }

    if (new_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(menu_video_screen_list, new_selection), lv_color_black(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(menu_video_screen_list, new_selection), lv_color_white(), 0);
        lv_obj_scroll_to_view(lv_obj_get_child(menu_video_screen_list, new_selection), LV_ANIM_ON);
    }
}

static void handle_video_selection(void)
{
    if (selected_item < VIDEO_ACTIONS_COUNT) {
        last_selected_item = selected_item;

        video_action_item_t *selected_action = &video_actions[selected_item];
        printf("Selected video action: %s\n", selected_action->name);
        // Set the message after loading the screen
        toast_screen_show("Unlock at Higher Level", 2000);

        // if (video_callback) {
        //     video_callback(selected_action->action, video_callback_user_data);
        // }
    }
}

void menu_video_screen_init(void)
{
    ui_menu_video_screen_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_menu_video_screen_screen, 384, 168);
    lv_obj_set_style_bg_color(ui_menu_video_screen_screen, lv_color_white(), 0);

    lv_obj_t *title = lv_label_create(ui_menu_video_screen_screen);
    lv_label_set_text(title, "Video & Camera");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    menu_video_screen_list = lv_list_create(ui_menu_video_screen_screen);
    lv_obj_set_size(menu_video_screen_list, 364, 128);
    lv_obj_align(menu_video_screen_list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_border_color(menu_video_screen_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(menu_video_screen_list, 2, 0);

    for (uint8_t i = 0; i < VIDEO_ACTIONS_COUNT; i++) {
        lv_list_add_btn(menu_video_screen_list, video_actions[i].icon, video_actions[i].name);
    }

    selected_item = last_selected_item;
    uint32_t child_count = lv_obj_get_child_cnt(menu_video_screen_list);

    if (selected_item >= child_count) {
        selected_item = 0;
        last_selected_item = 0;
    }

    if (child_count > 0) {
        update_selection(0, selected_item);
        printf("[%s] Restored selection to item %d\n", menu_video_screen.name, selected_item);
    }

    timer = lv_timer_create(menu_video_screen_timer_cb, 1000, NULL);
    lv_obj_add_event_cb(ui_menu_video_screen_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_menu_video_screen_screen);
    lv_group_focus_obj(ui_menu_video_screen_screen);
}

void menu_video_screen_deinit(void)
{
    if (ui_menu_video_screen_screen) {
        lv_obj_remove_event_cb(ui_menu_video_screen_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_menu_video_screen_screen);
    }
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }
}
