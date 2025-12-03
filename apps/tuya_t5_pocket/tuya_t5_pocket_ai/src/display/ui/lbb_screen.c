/**
 * @file lbb_screen.c
 * @brief Implementation of the LBB screen
 *
 * This file contains the implementation of the LBB screen which displays
 * the LBB image in full screen.
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "lbb_screen.h"
#include "screen_manager.h"
#include <stdio.h>

// Declare the LBB image
LV_IMG_DECLARE(hmbb);

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_lbb_screen;
static lv_timer_t *timer;

Screen_t lbb_screen = {
    .init = lbb_screen_init,
    .deinit = lbb_screen_deinit,
    .screen_obj = &ui_lbb_screen,
    .name = "lbb_screen",
    .state_data = NULL,
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void lbb_screen_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Timer callback for the LBB screen
 */
static void lbb_screen_timer_cb(lv_timer_t *timer)
{
    printf("[%s] LBB screen timer callback\n", lbb_screen.name);
}

/**
 * @brief Keyboard event callback
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", lbb_screen.name, key);

    switch (key) {
    case KEY_ESC:
        printf("ESC key pressed - returning to scan menu\n");
        screen_back();
        break;
    case KEY_ENTER:
        printf("ENTER key pressed - return to menu\n");
        screen_back();
        break;
    default:
        printf("Key %d pressed\n", key);
        break;
    }
}

/**
 * @brief Initialize the LBB screen
 */
void lbb_screen_init(void)
{
    ui_lbb_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_lbb_screen, 384, 168);
    lv_obj_set_style_bg_color(ui_lbb_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ui_lbb_screen, LV_OPA_COVER, 0);

    // Create and display LBB image
    lv_obj_t *img = lv_image_create(ui_lbb_screen);
    lv_image_set_src(img, &hmbb);
    lv_obj_center(img);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE);

    // Events and group
    lv_obj_add_event_cb(ui_lbb_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_lbb_screen);
    lv_group_focus_obj(ui_lbb_screen);

    timer = lv_timer_create(lbb_screen_timer_cb, 1000, NULL);
}

/**
 * @brief Deinitialize the LBB screen
 */
void lbb_screen_deinit(void)
{
    if (ui_lbb_screen) {
        printf("deinit LBB screen\n");
        lv_obj_remove_event_cb(ui_lbb_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_lbb_screen);
    }
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }
}

/**
 * @brief Create the LBB screen (alias for init)
 */
void lbb_screen_create(void)
{
    lbb_screen_init();
}
