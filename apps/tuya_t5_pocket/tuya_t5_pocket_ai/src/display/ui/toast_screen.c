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

static lv_obj_t *ui_toast_screen;  // Keep for compatibility but not used as screen
static lv_obj_t *toast_container;
static lv_obj_t *toast_label;
static lv_timer_t *timer;
static bool is_visible = false;  // Track if toast is currently visible

Screen_t toast_screen = {
    .init = NULL,
    .deinit = NULL,
    .screen_obj = &ui_toast_screen,
    .name = "toast_screen",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void toast_screen_timer_cb(lv_timer_t *timer);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Timer callback for the toast screen
 *
 * This function is called when the toast screen timer expires.
 * It automatically hides the toast message by deleting the overlay.
 *
 * @param timer The timer object
 */
static void toast_screen_timer_cb(lv_timer_t *timer_param)
{
    printf("[%s] toast timer expired, hiding toast overlay.\n", toast_screen.name);
    // Clean up timer
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }

    // Delete the toast container overlay
    if (toast_container) {
        lv_obj_del(toast_container);
        toast_container = NULL;
        toast_label = NULL;  // Label is child of container, will be deleted automatically
    }

    is_visible = false;
    printf("[%s] Toast overlay hidden.\n", toast_screen.name);
}

/**
 * @brief Show toast message as an overlay on the current screen
 *
 * @param message The message text to display
 * @param delay_ms Auto-hide delay in milliseconds (0 for default delay)
 *
 * This function creates a floating toast overlay on top of the current active screen.
 * The toast does not replace the current screen, it simply appears on top.
 */
void toast_screen_show(const char *message, uint32_t delay_ms)
{
    printf("[%s] Showing toast overlay: %s\n", toast_screen.name, message ? message : "NULL");

    // Clean up any existing toast first
    if (toast_container) {
        printf("Cleaning up existing toast overlay\n");
        lv_obj_del(toast_container);
        toast_container = NULL;
        toast_label = NULL;
    }

    // Cancel any existing timer
    if (timer) {
        printf("Canceling existing timer\n");
        lv_timer_del(timer);
        timer = NULL;
    }

    // Get the current active screen to add toast overlay on top
    lv_obj_t *active_screen = lv_scr_act();
    if (!active_screen) {
        printf("Error: No active screen found\n");
        return;
    }

    // Create toast container as a child of the active screen
    toast_container = lv_obj_create(active_screen);
    lv_obj_set_size(toast_container, TOAST_MAX_WIDTH, TOAST_MIN_HEIGHT);
    lv_obj_align(toast_container, LV_ALIGN_CENTER, 0, 0);

    // Style the toast container with semi-transparent black background
    lv_obj_set_style_bg_color(toast_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(toast_container, LV_OPA_80, 0);
    lv_obj_set_style_border_width(toast_container, 2, 0);
    lv_obj_set_style_border_color(toast_container, lv_color_white(), 0);
    lv_obj_set_style_radius(toast_container, 10, 0);
    lv_obj_set_style_pad_all(toast_container, TOAST_PADDING, 0);
    lv_obj_set_style_shadow_width(toast_container, 10, 0);
    lv_obj_set_style_shadow_color(toast_container, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(toast_container, LV_OPA_50, 0);

    // Make toast container clickable (but not scrollable)
    // lv_obj_clear_flag(toast_container, LV_OBJ_FLAG_SCROLLABLE);
    // lv_obj_add_flag(toast_container, LV_OBJ_FLAG_CLICKABLE);

    // Create toast label
    toast_label = lv_label_create(toast_container);
    lv_label_set_text(toast_label, message ? message : "Toast Message");
    lv_obj_align(toast_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(toast_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(toast_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(toast_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(toast_label, TOAST_MAX_WIDTH - (TOAST_PADDING * 2));

    // Move toast to the foreground (highest z-order) to appear on top
    lv_obj_move_foreground(toast_container);

    // Force immediate redraw
    lv_obj_invalidate(active_screen);

    is_visible = true;
    printf("Toast overlay created and shown\n");

    // Set up timer to auto-hide the toast
    uint32_t actual_delay = (delay_ms > 0) ? delay_ms : TOAST_DEFAULT_DELAY;
    timer = lv_timer_create(toast_screen_timer_cb, actual_delay, NULL);
    // lv_timer_set_repeat_count(timer, 1);  // Run only once
    printf("Created auto-hide timer with delay: %d ms\n", actual_delay);
}

/**
 * @brief Hide toast message
 */
/**
 * @brief Manually hide the toast overlay
 *
 * This function can be called to immediately dismiss the toast overlay.
 */
void toast_screen_hide(void)
{
    if (!is_visible || !toast_container) {
        printf("[%s] Toast not visible, nothing to hide\n", toast_screen.name);
        return;
    }

    printf("[%s] Manually hiding toast overlay\n", toast_screen.name);

    // Delete the toast container
    if (toast_container) {
        lv_obj_del(toast_container);
        toast_container = NULL;
        toast_label = NULL;
    }

    // Cancel existing timer
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }

    is_visible = false;
    printf("Toast overlay hidden\n");
}
