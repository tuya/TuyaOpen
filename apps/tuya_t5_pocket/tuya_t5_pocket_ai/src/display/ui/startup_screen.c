/**
 * @file startup_screen.c
 * @brief Implementation of the startup screen for the application
 *
 * This file contains the implementation of the startup screen which is displayed
 * when the application starts. It shows a splash screen with "TuyaOpen" and
 * "AI Pocket Pet Demo" text, and automatically transitions after a timeout.
 *
 * The startup screen includes:
 * - A white background splash screen
 * - Title and subtitle text
 * - Automatic transition timer
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "startup_screen.h"
#include "main_screen.h"
#include <stdio.h>

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_startup_screen;
static lv_timer_t *timer;

Screen_t startup_screen = {
    .init = startup_screen_init,
    .deinit = startup_screen_deinit,
    .screen_obj = &ui_startup_screen,
    .name = "Startup",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void startup_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Timer callback for the startup screen
 *
 * This function is called when the startup screen timer expires.
 * It deinitializes the startup screen.
 *
 * @param timer The timer object
 */
static void startup_timer_cb(lv_timer_t *timer)
{
    // printf("[%s] Startup timer expired, transitioning to next screen.\n", startup_screen.name);
    // Modified callback function, no longer references non-existent screen_main
    screen_load(&main_screen);
}

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the startup screen.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    // lv_event_code_t code = lv_event_get_code(e);  // Get event code
    // printf("[%s] Keyboard event received: code = %d\n", startup_screen.name, code);

    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", startup_screen.name, key);
}

/**
 * @brief Initialize the startup screen
 *
 * This function creates the startup screen UI with a white background,
 * title and subtitle text, and starts the transition timer.
 */
void startup_screen_init(void)
{
    ui_startup_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_startup_screen, 384, 168);
    lv_obj_set_style_bg_color(ui_startup_screen, lv_color_white(), 0);

    lv_obj_t *title = lv_label_create(ui_startup_screen);
    lv_label_set_text(title, "TuyaOpen");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    lv_obj_t *subtitle = lv_label_create(ui_startup_screen);
    lv_label_set_text(subtitle, "AI Pocket Pet Demo");
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 20);

    timer = lv_timer_create(startup_timer_cb, 1000, NULL);
    lv_obj_add_event_cb(ui_startup_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_startup_screen);
}

/**
 * @brief Deinitialize the startup screen
 *
 * This function cleans up the startup screen by deleting the UI object
 * and timer, and removing the event callback.
 */
void startup_screen_deinit(void)
{
    if (ui_startup_screen) {
        printf("deinit startup screen\n");
        lv_obj_remove_event_cb(ui_startup_screen, keyboard_event_cb);   // Remove event callback
        lv_group_remove_obj(ui_startup_screen);                         // Remove from group
        // lv_obj_del(ui_startup_screen);
        // ui_startup_screen = NULL;
    }
    if (timer) {
        lv_timer_del(timer);    // Delete timer
        timer = NULL;
    }
}
