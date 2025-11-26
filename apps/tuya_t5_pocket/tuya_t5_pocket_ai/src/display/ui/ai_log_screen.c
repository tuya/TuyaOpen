/**
 * @file ai_log_screen.c
 * @brief Implementation of the AI log screen for the application
 *
 * This file contains the implementation of the AI log screen which displays
 * analysis logs in a scrollable text area. The screen shows a title and
 * a black-bordered text box for displaying log content.
 *
 * The AI log screen includes:
 * - Title label at the top
 * - Black-bordered text area for log display
 * - Scrollable content support
 * - Dynamic log update functionality
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "ai_log_screen.h"
#include <stdio.h>
#include <string.h>

#ifdef ENABLE_LVGL_HARDWARE
#include "tkl_fs.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/

// Screen dimensions
#ifndef AI_PET_SCREEN_WIDTH
#define AI_PET_SCREEN_WIDTH 384
#endif
#ifndef AI_PET_SCREEN_HEIGHT
#define AI_PET_SCREEN_HEIGHT 168
#endif

// Maximum log buffer size
#define MAX_LOG_SIZE 2048

#ifdef ENABLE_LVGL_HARDWARE
// SD card paths
#define SDCARD_MOUNT_PATH "/sdcard"
#define AI_LOG_FILE_PATH  "/sdcard/ai_log.txt"
#endif

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_ai_log_screen;
static lv_obj_t *title_label = NULL;
static lv_obj_t *log_container = NULL;
static lv_obj_t *log_text_area = NULL;

// Log buffer to store content
static char log_buffer[MAX_LOG_SIZE] = {0};
static size_t log_buffer_used = 0;

// Lifecycle callback
static ai_log_screen_lifecycle_cb_t sg_lifecycle_callback = NULL;

#ifdef ENABLE_LVGL_HARDWARE
// SD card mount status
static bool sd_card_mounted = false;
#endif

Screen_t ai_log_screen = {
    .init = ai_log_screen_init,
    .deinit = ai_log_screen_deinit,
    .screen_obj = &ui_ai_log_screen,
    .name = "ai_log",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void keyboard_event_cb(lv_event_t *e);

#ifdef ENABLE_LVGL_HARDWARE
static void save_log_to_sd(char *log_text, size_t length);
#endif

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Register lifecycle callback for AI log screen
 * This allows external modules to be notified when the screen is shown/hidden
 *
 * @param callback Callback function, NULL to unregister
 */
void ai_log_screen_register_lifecycle_cb(ai_log_screen_lifecycle_cb_t callback)
{
    sg_lifecycle_callback = callback;
    printf("[AI Log] Lifecycle callback %s\n", callback ? "registered" : "unregistered");
}

#ifdef ENABLE_LVGL_HARDWARE
/**
 * @brief Save log to SD card
 *
 * This function saves the log content to a text file on the SD card.
 *
 * @param log_text Pointer to log text string
 * @param length Length of the log text
 */
static void save_log_to_sd(char *log_text, size_t length)
{
    if (!sd_card_mounted || !log_text || length == 0) {
        return;
    }

    TUYA_FILE file_hdl = tkl_fopen(AI_LOG_FILE_PATH, "w");
    if (NULL == file_hdl) {
        printf("[AI Log] Failed to open file %s for writing\n", AI_LOG_FILE_PATH);
        return;
    }

    uint32_t ret_len = tkl_fwrite(log_text, length, file_hdl);
    if (ret_len != length) {
        printf("[AI Log] Failed to write to file %s: wrote %d/%zu bytes\n", AI_LOG_FILE_PATH, ret_len, length);
    } else {
        printf("[AI Log] Successfully saved %zu bytes to %s\n", length, AI_LOG_FILE_PATH);
    }

    tkl_fclose(file_hdl);
}
#endif

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the AI log screen.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", ai_log_screen.name, key);

    switch (key) {
    case KEY_UP:
        // Scroll up
        if (log_container) {
            lv_coord_t scroll_top = lv_obj_get_scroll_top(log_container);
            if (scroll_top > 0) {
                lv_coord_t scroll_step = (scroll_top > 20) ? 20 : scroll_top;
                lv_obj_scroll_by(log_container, 0, scroll_step, LV_ANIM_ON);
                printf("AI Log: Scrolled up by %d pixels\n", scroll_step);
            } else {
                printf("AI Log: Already at top\n");
            }
        }
        break;

    case KEY_DOWN:
        // Scroll down
        if (log_container) {
            lv_coord_t scroll_bottom = lv_obj_get_scroll_bottom(log_container);
            if (scroll_bottom > 0) {
                lv_coord_t scroll_step = (scroll_bottom > 20) ? 20 : scroll_bottom;
                lv_obj_scroll_by(log_container, 0, -scroll_step, LV_ANIM_ON);
                printf("AI Log: Scrolled down by %d pixels\n", scroll_step);
            } else {
                printf("AI Log: Already at bottom\n");
            }
        }
        break;

    case KEY_LEFT:
        printf("LEFT key pressed\n");
        break;

    case KEY_RIGHT:
        printf("RIGHT key pressed\n");
        break;

    case KEY_ENTER:
        printf("ENTER key pressed - Clear log\n");
        ai_log_screen_clear_log();
        break;

    case KEY_ESC:
        printf("ESC key pressed - Return to previous screen\n");
        screen_back();
        break;

    default:
        printf("Unknown key pressed\n");
        break;
    }
}

/**
 * @brief Update AI log content
 *
 * This function replaces the current log content with new text.
 *
 * @param log_text Pointer to log text string
 * @param length Length of the log text
 */
void ai_log_screen_update_log(const char *log_text, size_t length)
{
    if (!log_text || length == 0) {
        return;
    }

    // Clear existing content
    log_buffer_used = 0;
    memset(log_buffer, 0, sizeof(log_buffer));

    // Copy new content
    size_t copy_len = (length < MAX_LOG_SIZE - 1) ? length : (MAX_LOG_SIZE - 1);
    memcpy(log_buffer, log_text, copy_len);
    log_buffer[copy_len] = '\0';
    log_buffer_used = copy_len;

    // Update text area
    if (log_text_area) {
        lv_label_set_text(log_text_area, log_buffer);
    }
    // Scroll container to top
    if (log_container) {
        lv_obj_scroll_to_y(log_container, 0, LV_ANIM_OFF);
    }

    printf("[AI Log] Updated log content: %zu bytes\n", copy_len);

#ifdef ENABLE_LVGL_HARDWARE
    // Save log to SD card
    save_log_to_sd(log_buffer, log_buffer_used);
#endif
}

/**
 * @brief Append text to existing log content
 *
 * This function adds new text to the end of the current log.
 *
 * @param log_text Pointer to log text string to append
 * @param length Length of the log text
 */
void ai_log_screen_append_log(const char *log_text, size_t length)
{
    if (!log_text || length == 0) {
        return;
    }

    // Calculate available space
    size_t available = MAX_LOG_SIZE - log_buffer_used - 1;
    if (available == 0) {
        printf("[AI Log] Buffer full, cannot append\n");
        return;
    }

    // Copy new content to buffer
    size_t copy_len = (length < available) ? length : available;
    memcpy(log_buffer + log_buffer_used, log_text, copy_len);
    log_buffer_used += copy_len;
    log_buffer[log_buffer_used] = '\0';

    // Update text area
    if (log_text_area) {
        lv_label_set_text(log_text_area, log_buffer);
    }
    // Scroll container to bottom to show new content
    if (log_container) {
        lv_obj_scroll_to_y(log_container, LV_COORD_MAX, LV_ANIM_ON);
    }

    printf("[AI Log] Appended log content: %zu bytes (total: %zu)\n", copy_len, log_buffer_used);

#ifdef ENABLE_LVGL_HARDWARE
    // Save log to SD card
    save_log_to_sd(log_buffer, log_buffer_used);
#endif
}

/**
 * @brief Clear all log content
 *
 * This function removes all text from the log display.
 */
void ai_log_screen_clear_log(void)
{
    log_buffer_used = 0;
    memset(log_buffer, 0, sizeof(log_buffer));

    if (log_text_area) {
        lv_label_set_text(log_text_area, "");
    }

    printf("[AI Log] Cleared log content\n");
}

/**
 * @brief Initialize the AI log screen
 *
 * This function creates the AI log screen UI with a white background,
 * title text, and a black-bordered text area for displaying logs.
 */
void ai_log_screen_init(void)
{
#ifdef ENABLE_LVGL_HARDWARE
    // Mount SD card
    OPERATE_RET rt = tkl_fs_mount(SDCARD_MOUNT_PATH, DEV_SDCARD);
    if (rt == OPRT_OK) {
        sd_card_mounted = true;
        printf("[AI Log] SD card mounted successfully at %s\n", SDCARD_MOUNT_PATH);
    } else {
        sd_card_mounted = false;
        printf("[AI Log] Failed to mount SD card: %d\n", rt);
    }

    // Notify external modules that screen is initialized
    if (sg_lifecycle_callback) {
        sg_lifecycle_callback(TRUE);
    }
#endif

    ui_ai_log_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_ai_log_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_ai_log_screen, lv_color_white(), 0);

    // Create title
    title_label = lv_label_create(ui_ai_log_screen);
    lv_label_set_text(title_label, "AI Analysis Log");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);

    // Create text area container (black border box)
    log_container = lv_obj_create(ui_ai_log_screen);
    lv_obj_set_size(log_container, AI_PET_SCREEN_WIDTH - 20, AI_PET_SCREEN_HEIGHT - 45);
    lv_obj_align(log_container, LV_ALIGN_CENTER, 0, 8);
    lv_obj_set_style_bg_color(log_container, lv_color_white(), 0);
    lv_obj_set_style_border_color(log_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(log_container, 2, 0);
    lv_obj_set_style_radius(log_container, 0, 0);
    lv_obj_set_style_pad_all(log_container, 5, 0);
    lv_obj_set_scroll_dir(log_container, LV_DIR_VER);

    // Create label inside container for log text
    log_text_area = lv_label_create(log_container);
    lv_label_set_text(log_text_area, "");
    lv_obj_set_width(log_text_area, AI_PET_SCREEN_WIDTH - 30);
    lv_obj_set_style_text_font(log_text_area, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(log_text_area, lv_color_black(), 0);
    lv_label_set_long_mode(log_text_area, LV_LABEL_LONG_WRAP);
    lv_obj_align(log_text_area, LV_ALIGN_TOP_LEFT, 0, 0);

    // Add keyboard event handler
    lv_obj_add_event_cb(ui_ai_log_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_ai_log_screen);
    lv_group_focus_obj(ui_ai_log_screen);
}

/**
 * @brief Deinitialize the AI log screen
 *
 * This function cleans up the AI log screen by deleting the UI object
 * and removing the event callback.
 */
void ai_log_screen_deinit(void)
{
    if (ui_ai_log_screen) {
        printf("deinit AI log screen\n");
        lv_obj_remove_event_cb(ui_ai_log_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_ai_log_screen);
    }

    // Clear log buffer
    log_buffer_used = 0;
    memset(log_buffer, 0, sizeof(log_buffer));

    // Reset pointers
    title_label = NULL;
    log_container = NULL;
    log_text_area = NULL;

#ifdef ENABLE_LVGL_HARDWARE
    // Notify external modules that screen is deinitialized
    if (sg_lifecycle_callback) {
        sg_lifecycle_callback(FALSE);
    }

    // Unmount SD card
    if (sd_card_mounted) {
        tkl_fs_unmount(SDCARD_MOUNT_PATH);
        sd_card_mounted = false;
        printf("[AI Log] SD card unmounted\n");
    }
#endif
}