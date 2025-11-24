/**
 * @file txt_viewer_screen.c
 * @brief Simple text file viewer screen implementation
 *
 * This file contains the implementation of a simple text viewer that reads
 * and displays text files using a single label widget.
 *
 * Features:
 * - Load and display text files
 * - Simple scrollable text display
 * - Keyboard navigation support
 * - Clean white background with black text
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "txt_viewer_screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_txt_viewer_screen;
static lv_obj_t *content_container;
static lv_obj_t *title_label;
static lv_obj_t *text_label;
static lv_obj_t *status_label;
static txt_viewer_state_t viewer_state = {0};

Screen_t txt_viewer_screen = {
    .init = txt_viewer_screen_init,
    .deinit = txt_viewer_screen_deinit,
    .screen_obj = &ui_txt_viewer_screen,
    .name = "txt_viewer",
    .state_data = &viewer_state,
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void keyboard_event_cb(lv_event_t *e);
static void create_ui(void);
static void update_display(void);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Keyboard event callback
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", txt_viewer_screen.name, key);

    switch (key) {
        case KEY_UP:
            // Scroll up
            lv_obj_scroll_by(content_container, 0, 16, LV_ANIM_ON);
            printf("Scroll up\n");
            break;

        case KEY_DOWN:
            // Scroll down
            lv_obj_scroll_by(content_container, 0, -16, LV_ANIM_ON);
            printf("Scroll down\n");
            break;

        case KEY_ESC:
            printf("ESC key pressed - going back\n");
            screen_back();
            break;

        default:
            printf("Unknown key: %d\n", key);
            break;
    }
}

/**
 * @brief Create the UI layout
 */
static void create_ui(void)
{
    // Create main container
    content_container = lv_obj_create(ui_txt_viewer_screen);
    lv_obj_set_size(content_container, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(content_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(content_container, 0, 0);
    lv_obj_set_style_pad_all(content_container, 8, 0);
    lv_obj_set_scrollbar_mode(content_container, LV_SCROLLBAR_MODE_AUTO);

    // Title label
    title_label = lv_label_create(content_container);
    lv_label_set_text(title_label, "Text Viewer");
    lv_obj_set_width(title_label, AI_PET_SCREEN_WIDTH - 16);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);

    // Text display label
    text_label = lv_label_create(content_container);
    lv_obj_set_width(text_label, AI_PET_SCREEN_WIDTH - 32);
    lv_obj_align(text_label, LV_ALIGN_TOP_LEFT, 0, 24);
    lv_obj_set_style_text_color(text_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(text_label, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(text_label, 2, 0);
    lv_label_set_text(text_label, "No file loaded");

    // Status label at bottom
    status_label = lv_label_create(ui_txt_viewer_screen);
    lv_obj_set_width(status_label, AI_PET_SCREEN_WIDTH - 16);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_text_color(status_label, lv_color_make(100, 100, 100), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(status_label, LV_SYMBOL_UP LV_SYMBOL_DOWN " Scroll | " LV_SYMBOL_CLOSE " Back");
}

/**
 * @brief Update the text display
 */
static void update_display(void)
{
    if (!viewer_state.content_loaded || !viewer_state.content) {
        lv_label_set_text(text_label, "No content loaded");
        lv_label_set_text(title_label, "Text Viewer");
        return;
    }

    // Update title with filename
    char title[64];
    snprintf(title, sizeof(title), "File: %s", viewer_state.current_file);
    lv_label_set_text(title_label, title);

    // Display content
    lv_label_set_text(text_label, viewer_state.content);

    // Update status
    char status[64];
    snprintf(status, sizeof(status), "Size: %zu bytes | " LV_SYMBOL_UP LV_SYMBOL_DOWN " Scroll | " LV_SYMBOL_CLOSE " Back",
             viewer_state.content_size);
    lv_label_set_text(status_label, status);

    // Scroll to top
    lv_obj_scroll_to_y(content_container, 0, LV_ANIM_OFF);

    printf("Display updated: %s (%zu bytes)\n", viewer_state.current_file, viewer_state.content_size);
}

/**
 * @brief Load a text file for viewing
 */
bool txt_viewer_load_file(const char *filepath)
{
    if (!filepath) {
        printf("Error: No filepath provided\n");
        return false;
    }

    // Free old content if any
    if (viewer_state.content) {
        free(viewer_state.content);
        viewer_state.content = NULL;
    }

    // Open file
    FILE *file = fopen(filepath, "r");
    if (!file) {
        printf("Failed to open file: %s\n", filepath);
        return false;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        printf("File is empty or invalid: %s\n", filepath);
        fclose(file);
        return false;
    }

    if (file_size > TXT_MAX_SIZE) {
        printf("File too large: %ld bytes (max: %d)\n", file_size, TXT_MAX_SIZE);
        fclose(file);
        return false;
    }

    // Allocate memory
    viewer_state.content = malloc(file_size + 1);
    if (!viewer_state.content) {
        printf("Failed to allocate memory for file content\n");
        fclose(file);
        return false;
    }

    // Read file
    size_t bytes_read = fread(viewer_state.content, 1, file_size, file);
    viewer_state.content[bytes_read] = '\0';
    viewer_state.content_size = bytes_read;

    fclose(file);

    // Store filename
    const char *filename = strrchr(filepath, '/');
    if (filename) {
        filename++; // Skip the '/'
    } else {
        filename = filepath;
    }
    strncpy(viewer_state.current_file, filename, sizeof(viewer_state.current_file) - 1);
    viewer_state.current_file[sizeof(viewer_state.current_file) - 1] = '\0';

    viewer_state.content_loaded = true;

    // Update display if screen is initialized
    if (text_label) {
        update_display();
    }

    printf("Loaded file: %s (%zu bytes)\n", filepath, bytes_read);
    return true;
}

/**
 * @brief Clear the displayed text
 */
void txt_viewer_clear(void)
{
    if (viewer_state.content) {
        free(viewer_state.content);
        viewer_state.content = NULL;
    }

    viewer_state.content_size = 0;
    viewer_state.content_loaded = false;
    memset(viewer_state.current_file, 0, sizeof(viewer_state.current_file));

    if (text_label) {
        lv_label_set_text(text_label, "No file loaded");
        lv_label_set_text(title_label, "Text Viewer");
        lv_label_set_text(status_label, LV_SYMBOL_UP LV_SYMBOL_DOWN " Scroll | " LV_SYMBOL_CLOSE " Back");
    }

    printf("Viewer cleared\n");
}

/**
 * @brief Initialize the text viewer screen
 */
void txt_viewer_screen_init(void)
{
    printf("[%s] Initializing text viewer screen\n", txt_viewer_screen.name);

    // Initialize state
    memset(&viewer_state, 0, sizeof(viewer_state));

    // Create the main screen
    ui_txt_viewer_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_txt_viewer_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_txt_viewer_screen, lv_color_white(), 0);
    lv_obj_set_style_pad_all(ui_txt_viewer_screen, 0, 0);

    // Create UI
    create_ui();

    // Set up keyboard event handling
    lv_obj_add_event_cb(ui_txt_viewer_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_txt_viewer_screen);
    lv_group_focus_obj(ui_txt_viewer_screen);

    // Try to load a default file if it exists
    char default_file[512];
    snprintf(default_file, sizeof(default_file), "%s/The_old_man_and_The_sea.txt", TXT_DIR_PATH);
    txt_viewer_load_file(default_file);

    printf("[%s] Text viewer screen initialized\n", txt_viewer_screen.name);
}

/**
 * @brief Deinitialize the text viewer screen
 */
void txt_viewer_screen_deinit(void)
{
    printf("[%s] Deinitializing text viewer screen\n", txt_viewer_screen.name);

    // Clear content
    txt_viewer_clear();

    // Remove event callback
    if (ui_txt_viewer_screen) {
        lv_obj_remove_event_cb(ui_txt_viewer_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_txt_viewer_screen);
    }

    printf("[%s] Text viewer screen deinitialized\n", txt_viewer_screen.name);
}
