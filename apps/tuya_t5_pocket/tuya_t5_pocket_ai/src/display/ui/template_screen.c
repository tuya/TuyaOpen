/**
 * @file template_screen.c
 * @brief Implementation of the template screen for the application
 *
 * This file contains the implementation of the template screen which is displayed
 * when the application starts. It shows a splash screen with "TuyaOpen" and
 * "AI Pocket Pet Demo" text, and automatically transitions after a timeout.
 *
 * The template screen includes:
 * - A white background splash screen
 * - Title and subtitle text
 * - Automatic transition timer
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "template_screen.h"
#include <stdio.h>
/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_template_screen;
static lv_timer_t *timer;

Screen_t template_screen = {
    .init = template_screen_init,
    .deinit = template_screen_deinit,
    .screen_obj = &ui_template_screen,
    .name = "template",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void template_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Timer callback for the template screen
 *
 * This function is called when the template screen timer expires.
 * It deinitializes the template screen.
 *
 * @param timer The timer object
 */
static void template_timer_cb(lv_timer_t *timer)
{
    printf("[%s] template timer expired, transitioning to next screen.\n", template_screen.name);
    // Modified callback function, no longer references non-existent screen_main
    // screen_load(&main_screen);
}

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the template screen.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    // lv_event_code_t code = lv_event_get_code(e);  // Get event code
    // printf("[%s] Keyboard event received: code = %d\n", template_screen.name, code);

    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", template_screen.name, key);

    switch (key) {
        case KEY_UP:
            printf("UP key pressed\n");
            break;
        case KEY_DOWN:
            printf("DOWN key pressed\n");
            break;
        case KEY_LEFT:
            printf("LEFT key pressed\n");
            break;
        case KEY_RIGHT:
            printf("RIGHT key pressed\n");
            break;
        case KEY_ENTER:
            printf("ENTER key pressed\n");
            break;
        case KEY_ESC:
            printf("ESC key pressed\n");
            break;
        default:
            printf("Unknown key pressed\n");
            break;
    }
}

/**
 * @brief Initialize the template screen
 *
 * This function creates the template screen UI with a white background,
 * title and subtitle text, and starts the transition timer.
 */
void template_screen_init(void)
{
    ui_template_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_template_screen, 384, 168);
    lv_obj_set_style_bg_color(ui_template_screen, lv_color_white(), 0);

    lv_obj_t *title = lv_label_create(ui_template_screen);
    lv_label_set_text(title, "TuyaOpen\nLVGL Temp");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    timer = lv_timer_create(template_timer_cb, 1000, NULL);
    lv_obj_add_event_cb(ui_template_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_template_screen);
    lv_group_focus_obj(ui_template_screen);
}

/**
 * @brief Deinitialize the template screen
 *
 * This function cleans up the template screen by deleting the UI object
 * and timer, and removing the event callback.
 */
void template_screen_deinit(void)
{
    if (ui_template_screen) {
        printf("deinit template screen\n");
        lv_obj_remove_event_cb(ui_template_screen, keyboard_event_cb);   // Remove event callback
        lv_group_remove_obj(ui_template_screen);                         // Remove from group
    }
    if (timer) {
        lv_timer_del(timer);    // Delete timer
        timer = NULL;
    }
}
