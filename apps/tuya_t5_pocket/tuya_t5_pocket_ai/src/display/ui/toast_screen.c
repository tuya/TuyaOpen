/**
 * @file toast_screen.c
 * @brief Implementation of the toast screen for the application
 *
 * This file contains the implementation of the toast screen which displays
 * toast messages with customizable text and auto-hide functionality.
 *
 * The toast screen includes:
 * - Toast message container with black background and white border
 * - Customizable message text with white color
 * - Auto-hide timer functionality
 * - Keyboard event handling for manual dismissal
 * - Z-order management to appear on top
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "toast_screen.h"
#include <stdio.h>
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define TOAST_PADDING 20
#define TOAST_MAX_WIDTH 344  // (384 - 40)
#define TOAST_MIN_HEIGHT 60
#define TOAST_DEFAULT_DELAY 3000
/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_toast_screen;
static lv_obj_t *toast_container;
static lv_obj_t *toast_label;
static lv_timer_t *timer;
static bool is_visible = false;

Screen_t toast_screen = {
    .init = toast_screen_init,
    .deinit = toast_screen_deinit,
    .screen_obj = &ui_toast_screen,
    .name = "toast_screen",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void toast_screen_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);
static void toast_screen_hide(void);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Timer callback for the toast screen
 *
 * This function is called when the toast screen timer expires.
 * It automatically hides the toast message.
 *
 * @param timer The timer object
 */
static void toast_screen_timer_cb(lv_timer_t *timer)
{
    printf("[%s] toast timer expired, returning to previous screen.\n", toast_screen.name);
    // Use screen_back to return to previous screen instead of hiding
    toast_screen_hide();
    screen_back();
}

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the toast screen.
 * ESC key can be used to manually dismiss the toast.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", toast_screen.name, key);

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
            // Don't hide toast on ENTER, let it auto-hide via timer
            printf("ENTER key pressed - ignoring for toast\n");
            break;
        case KEY_ESC:
            printf("ESC key pressed - returning to previous screen\n");
            screen_back();
            break;
        default:
            printf("Key %d pressed\n", key);
            break;
    }
}

/**
 * @brief Initialize the toast screen
 *
 * This function creates the toast screen UI with a toast container,
 * message label, and sets up event handling.
 */
void toast_screen_init(void)
{
    // Clean up any existing resources first
    if (timer) {
        printf("Cleaning up existing timer in init\n");
        lv_timer_del(timer);
        timer = NULL;
    }

    // Reset state
    toast_container = NULL;
    toast_label = NULL;
    is_visible = false;

    ui_toast_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_toast_screen, 384, 168);
    lv_obj_set_style_bg_color(ui_toast_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ui_toast_screen, LV_OPA_TRANSP, 0);  // Transparent background

    // Create toast container
    toast_container = lv_obj_create(ui_toast_screen);
    lv_obj_set_size(toast_container, TOAST_MAX_WIDTH, TOAST_MIN_HEIGHT);
    lv_obj_align(toast_container, LV_ALIGN_CENTER, 0, 0);

    // Style the toast container (matching toast.c styling)
    lv_obj_set_style_bg_color(toast_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(toast_container, LV_OPA_80, 0);
    lv_obj_set_style_border_width(toast_container, 2, 0);
    lv_obj_set_style_border_color(toast_container, lv_color_white(), 0);
    lv_obj_set_style_radius(toast_container, 10, 0);
    lv_obj_set_style_pad_all(toast_container, TOAST_PADDING, 0);
    lv_obj_set_style_shadow_width(toast_container, 10, 0);
    lv_obj_set_style_shadow_color(toast_container, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(toast_container, LV_OPA_50, 0);

    // Create toast label
    toast_label = lv_label_create(toast_container);
    lv_label_set_text(toast_label, "Toast Message");
    lv_obj_align(toast_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(toast_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(toast_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(toast_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(toast_label, TOAST_MAX_WIDTH - (TOAST_PADDING * 2));

    // Move toast to top of screen (highest z-order)
    lv_obj_move_foreground(toast_container);

    // Initially hide the toast
    lv_obj_add_flag(toast_container, LV_OBJ_FLAG_HIDDEN);
    is_visible = false;

    // Initialize timer to NULL
    timer = NULL;

    // Add keyboard event handler to handle ESC key for returning
    lv_obj_add_event_cb(ui_toast_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_toast_screen);
    lv_group_focus_obj(ui_toast_screen);
}

/**
 * @brief Deinitialize the toast screen
 *
 * This function cleans up the toast screen by deleting the UI object
 * and timer, and removing the event callback.
 */
void toast_screen_deinit(void)
{
    if (ui_toast_screen) {
        printf("deinit toast screen\n");
        lv_obj_remove_event_cb(ui_toast_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_toast_screen);
    }
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }

    // Reset state
    toast_container = NULL;
    toast_label = NULL;
    is_visible = false;
}

/**
 * @brief Show toast message
 *
 * @param message The message text to display
 * @param delay_ms Auto-hide delay in milliseconds (0 for default delay)
 */
void toast_screen_show(const char *message, uint32_t delay_ms)
{
    printf("[%s] Showing toast message: %s\n", toast_screen.name, message);
    screen_load_no_anim(&toast_screen);

    // Cancel any existing timer first to prevent multiple timers
    if (timer) {
        printf("Canceling existing timer\n");
        lv_timer_del(timer);
        timer = NULL;
    }

    // Set the message text
    if (message) {
        lv_label_set_text(toast_label, message);
    } else {
        lv_label_set_text(toast_label, "Toast Message");
    }

    // Set a reasonable height for the toast container
    lv_obj_set_height(toast_container, TOAST_MIN_HEIGHT);

    // Move toast to top of screen (highest z-order)
    lv_obj_move_foreground(toast_container);

    // Show the toast immediately
    lv_obj_clear_flag(toast_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(toast_container, LV_OPA_COVER, 0);
    is_visible = true;
    printf("Toast container shown, opacity set to COVER\n");

    // Set up new timer to hide the toast
    uint32_t actual_delay = (delay_ms > 0) ? delay_ms : TOAST_DEFAULT_DELAY;
    timer = lv_timer_create(toast_screen_timer_cb, actual_delay, NULL);
    printf("Created new timer with delay: %d ms\n", actual_delay);
}

/**
 * @brief Hide toast message
 */
static void toast_screen_hide(void)
{
    if (!toast_container) {
        return;
    }

    printf("toast_screen_hide called\n");

    // Cancel existing timer
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }

    // Hide immediately
    lv_obj_add_flag(toast_container, LV_OBJ_FLAG_HIDDEN);
    is_visible = false;
    printf("Toast container hidden\n");
}
