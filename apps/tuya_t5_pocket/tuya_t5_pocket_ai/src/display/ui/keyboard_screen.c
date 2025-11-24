/**
 * @file keyboard_screen.c
 * @brief Implementation of the keyboard screen for the application
 *
 * This file contains the implementation of the keyboard screen which provides
 * a virtual keyboard for text input with support for navigation and callbacks.
 *
 * The keyboard screen includes:
 * - Virtual keyboard layout with QWERTY design
 * - Text input area with character counter
 * - Navigation with arrow keys and selection highlighting
 * - Callback system for text completion
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "keyboard_screen.h"
#include <stdio.h>
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/

#define KEYBOARD_MAX_TEXT_LENGTH 15
#define KEYBOARD_ROWS 4
#define KEYBOARD_COLS 10
#define KEY_WIDTH 34
#define KEY_HEIGHT 25
#define KEY_SPACING 2

// Screen dimensions
#define SCREEN_WIDTH  384
#define SCREEN_HEIGHT 168

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_keyboard_screen;

// UI components
static lv_obj_t *text_area = NULL;
static lv_obj_t *keyboard_container = NULL;
static lv_obj_t *keys[KEYBOARD_ROWS][KEYBOARD_COLS];
static lv_obj_t *text_label = NULL;
static lv_obj_t *char_count_label = NULL;

// Keyboard state
typedef struct {
    char current_text[KEYBOARD_MAX_TEXT_LENGTH + 1];
    uint8_t text_length;
    uint8_t selected_row;
    uint8_t selected_col;
    keyboard_callback_t callback;
    void *user_data;
    uint8_t is_active;
} keyboard_state_t;

static keyboard_state_t g_keyboard_state;

Screen_t keyboard_screen = {
    .init = keyboard_screen_init,
    .deinit = keyboard_screen_deinit,
    .screen_obj = &ui_keyboard_screen,
    .name = "keyboard",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void keyboard_event_cb(lv_event_t *e);
static void create_keyboard_layout(void);
static void create_text_area(void);
static void update_text_display(void);
static void update_selection_highlight(void);
static void handle_key_press(const char *key_text);
static void key_button_event_cb(lv_event_t *e);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Create text input area
 */
static void create_text_area(void)
{
    // Create text display area at the top
    text_area = lv_obj_create(ui_keyboard_screen);
    lv_obj_set_size(text_area, SCREEN_WIDTH - 20, 40);
    lv_obj_align(text_area, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_color(text_area, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(text_area, LV_OPA_10, 0);
    lv_obj_set_style_border_width(text_area, 1, 0);
    lv_obj_set_style_border_color(text_area, lv_color_black(), 0);
    lv_obj_set_style_radius(text_area, 5, 0);

    // Text label
    text_label = lv_label_create(text_area);
    lv_obj_align(text_label, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_text_color(text_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(text_label, &lv_font_montserrat_14, 0);

    // Character count label
    char_count_label = lv_label_create(text_area);
    lv_obj_align(char_count_label, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_text_color(char_count_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(char_count_label, &lv_font_montserrat_14, 0);

    update_text_display();
}

/**
 * @brief Create keyboard layout
 */
static void create_keyboard_layout(void)
{
    // Create keyboard container
    keyboard_container = lv_obj_create(ui_keyboard_screen);
    lv_obj_set_size(keyboard_container, SCREEN_WIDTH - 20, 120);
    lv_obj_align(keyboard_container, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_bg_opa(keyboard_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(keyboard_container, 0, 0);
    lv_obj_set_style_pad_all(keyboard_container, 2, 0);

    // Define keyboard layout
    const char *keyboard_layout[KEYBOARD_ROWS][KEYBOARD_COLS] = {
        {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
        {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
        {"a", "s", "d", "f", "g", "h", "j", "k", "l", "<-"},
        {"z", "x", "c", "v", "b", "n", "m", " ", "OK", "ESC"}
    };

    // Create keys
    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        for (int col = 0; col < KEYBOARD_COLS; col++) {
            keys[row][col] = lv_btn_create(keyboard_container);
            lv_obj_set_size(keys[row][col], KEY_WIDTH, KEY_HEIGHT);

            // Position keys
            int x_pos = col * (KEY_WIDTH + KEY_SPACING);
            int y_pos = row * (KEY_HEIGHT + KEY_SPACING);
            lv_obj_set_pos(keys[row][col], x_pos, y_pos);

            // Style keys
            lv_obj_set_style_bg_color(keys[row][col], lv_color_white(), 0);
            lv_obj_set_style_bg_opa(keys[row][col], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(keys[row][col], 1, 0);
            lv_obj_set_style_border_color(keys[row][col], lv_color_black(), 0);
            lv_obj_set_style_radius(keys[row][col], 3, 0);
            lv_obj_set_style_text_color(keys[row][col], lv_color_black(), 0);
            lv_obj_set_style_text_font(keys[row][col], &lv_font_montserrat_14, 0);

            // Create label for key
            lv_obj_t *label = lv_label_create(keys[row][col]);
            lv_label_set_text(label, keyboard_layout[row][col]);
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

            // Add event callback
            lv_obj_add_event_cb(keys[row][col], key_button_event_cb, LV_EVENT_CLICKED, NULL);
        }
    }
}

/**
 * @brief Update text display
 */
static void update_text_display(void)
{
    // Update text label
    lv_label_set_text(text_label, g_keyboard_state.current_text);

    // Update character count
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%d/%d", g_keyboard_state.text_length, KEYBOARD_MAX_TEXT_LENGTH);
    lv_label_set_text(char_count_label, count_str);
}

/**
 * @brief Update selection highlight
 */
static void update_selection_highlight(void)
{
    // Reset all keys to default style
    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        for (int col = 0; col < KEYBOARD_COLS; col++) {
            if (keys[row][col]) {
                lv_obj_set_style_bg_color(keys[row][col], lv_color_white(), 0);
                lv_obj_set_style_border_color(keys[row][col], lv_color_black(), 0);
                lv_obj_set_style_border_width(keys[row][col], 1, 0);
                lv_obj_set_style_text_color(keys[row][col], lv_color_black(), 0);
            }
        }
    }

    // Highlight selected key
    if (keys[g_keyboard_state.selected_row][g_keyboard_state.selected_col]) {
        lv_obj_set_style_bg_color(keys[g_keyboard_state.selected_row][g_keyboard_state.selected_col], lv_color_black(), 0);
        lv_obj_set_style_border_color(keys[g_keyboard_state.selected_row][g_keyboard_state.selected_col], lv_color_black(), 0);
        lv_obj_set_style_border_width(keys[g_keyboard_state.selected_row][g_keyboard_state.selected_col], 2, 0);
        lv_obj_set_style_text_color(keys[g_keyboard_state.selected_row][g_keyboard_state.selected_col], lv_color_white(), 0);
    }
}

/**
 * @brief Handle key press
 */
static void handle_key_press(const char *key_text)
{
    if (strcmp(key_text, "<-") == 0) {
        // Backspace
        if (g_keyboard_state.text_length > 0) {
            g_keyboard_state.current_text[--g_keyboard_state.text_length] = '\0';
            update_text_display();
        }
    } else if (strcmp(key_text, "OK") == 0) {
        // Enter - confirm input
        printf("OK key pressed, current text: '%s'\n", g_keyboard_state.current_text);

        // Store callback and data before screen_back() to avoid issues
        keyboard_callback_t callback = g_keyboard_state.callback;
        void *user_data = g_keyboard_state.user_data;
        char text_copy[KEYBOARD_MAX_TEXT_LENGTH + 1];
        strncpy(text_copy, g_keyboard_state.current_text, sizeof(text_copy));
        text_copy[KEYBOARD_MAX_TEXT_LENGTH] = '\0';  // Ensure null termination

        printf("Executing callback with text: '%s'\n", text_copy);
        // Execute callback BEFORE calling screen_back to avoid deinit clearing the callback
        if (callback) {
            callback(text_copy, user_data);
        } else {
            printf("Warning: No callback function set!\n");
        }

        printf("Calling screen_back()\n");
        // Return to previous screen after callback execution
        screen_back();
    } else if (strcmp(key_text, "ESC") == 0) {
        // Escape - cancel input
        // Store callback and data before screen_back() to avoid issues
        keyboard_callback_t callback = g_keyboard_state.callback;
        void *user_data = g_keyboard_state.user_data;

        // Execute callback BEFORE calling screen_back to avoid deinit clearing the callback
        if (callback) {
            callback(NULL, user_data);
        }

        // Return to previous screen after callback execution
        screen_back();
    } else if (strcmp(key_text, " ") == 0) {
        // Space character
        if (g_keyboard_state.text_length < KEYBOARD_MAX_TEXT_LENGTH) {
            g_keyboard_state.current_text[g_keyboard_state.text_length++] = ' ';
            g_keyboard_state.current_text[g_keyboard_state.text_length] = '\0';
            update_text_display();
        }
    } else if (strlen(key_text) == 1) {
        // Regular character
        if (g_keyboard_state.text_length < KEYBOARD_MAX_TEXT_LENGTH) {
            g_keyboard_state.current_text[g_keyboard_state.text_length++] = key_text[0];
            g_keyboard_state.current_text[g_keyboard_state.text_length] = '\0';
            update_text_display();
        }
    }
}

/**
 * @brief Key button event callback
 */
static void key_button_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);

    // Find which key was pressed
    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        for (int col = 0; col < KEYBOARD_COLS; col++) {
            if (keys[row][col] == btn) {
                // Update selection position
                g_keyboard_state.selected_row = row;
                g_keyboard_state.selected_col = col;
                update_selection_highlight();

                // Get key text and handle it
                lv_obj_t *label = lv_obj_get_child(btn, 0);
                const char *key_text = lv_label_get_text(label);
                handle_key_press(key_text);
                return;
            }
        }
    }
}

/**
 * @brief Keyboard event callback
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", keyboard_screen.name, key);

    switch (key) {
        case KEY_UP:
            if (g_keyboard_state.selected_row > 0) {
                g_keyboard_state.selected_row--;
                update_selection_highlight();
            }
            break;
        case KEY_DOWN:
            if (g_keyboard_state.selected_row < KEYBOARD_ROWS - 1) {
                g_keyboard_state.selected_row++;
                update_selection_highlight();
            }
            break;
        case KEY_LEFT:
            if (g_keyboard_state.selected_col > 0) {
                g_keyboard_state.selected_col--;
                update_selection_highlight();
            }
            break;
        case KEY_RIGHT:
            if (g_keyboard_state.selected_col < KEYBOARD_COLS - 1) {
                g_keyboard_state.selected_col++;
                update_selection_highlight();
            }
            break;
        case KEY_ENTER:
            // Activate the selected key
            if (keys[g_keyboard_state.selected_row][g_keyboard_state.selected_col]) {
                lv_obj_t *label = lv_obj_get_child(keys[g_keyboard_state.selected_row][g_keyboard_state.selected_col], 0);
                const char *key_text = lv_label_get_text(label);
                handle_key_press(key_text);
            }
            break;
        case KEY_ESC:
            // Cancel input
            // Store callback and data before screen_back() to avoid issues
            {
                keyboard_callback_t callback = g_keyboard_state.callback;
                void *user_data = g_keyboard_state.user_data;

                // Execute callback BEFORE calling screen_back to avoid deinit clearing the callback
                if (callback) {
                    callback(NULL, user_data);
                }

                // Return to previous screen after callback execution
                screen_back();
            }
            break;
        default:
            break;
    }
}

/**
 * @brief Show keyboard with callback
 */
void keyboard_screen_show_with_callback(const char *initial_text, keyboard_callback_t callback, void *user_data)
{
    printf("keyboard_screen_show_with_callback called\n");
    printf("  initial_text: '%s'\n", initial_text ? initial_text : "NULL");
    printf("  callback: %p\n", (void*)callback);

    // Store callback and user data
    g_keyboard_state.callback = callback;
    g_keyboard_state.user_data = user_data;

    // Initialize text
    if (initial_text) {
        strncpy(g_keyboard_state.current_text, initial_text, KEYBOARD_MAX_TEXT_LENGTH);
        g_keyboard_state.current_text[KEYBOARD_MAX_TEXT_LENGTH] = '\0';  // Ensure null termination
        g_keyboard_state.text_length = strlen(g_keyboard_state.current_text);
    } else {
        memset(g_keyboard_state.current_text, 0, sizeof(g_keyboard_state.current_text));
        g_keyboard_state.text_length = 0;
    }

    printf("  current_text after init: '%s' (length: %d)\n", g_keyboard_state.current_text, g_keyboard_state.text_length);

    // Load the screen
    screen_load(&keyboard_screen);
}

/**
 * @brief Initialize the keyboard screen
 */
void keyboard_screen_init(void)
{
    ui_keyboard_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_keyboard_screen, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_keyboard_screen, lv_color_white(), 0);

    // Initialize keyboard state - preserve callback and user_data if already set
    // Only reset UI-related state, not the callback
    g_keyboard_state.selected_row = 0;
    g_keyboard_state.selected_col = 0;
    g_keyboard_state.is_active = 1;
    // Note: Do NOT reset callback and user_data here as they are set before init

    // Create UI components
    create_text_area();
    create_keyboard_layout();

    // Event handling
    lv_obj_add_event_cb(ui_keyboard_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_keyboard_screen);
    lv_group_focus_obj(ui_keyboard_screen);

    // Initial selection highlight
    update_selection_highlight();
}

/**
 * @brief Deinitialize the keyboard screen
 */
void keyboard_screen_deinit(void)
{
    if (ui_keyboard_screen) {
        printf("deinit keyboard screen\n");
        lv_obj_remove_event_cb(ui_keyboard_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_keyboard_screen);
    }

    // Reset pointers
    text_area = NULL;
    keyboard_container = NULL;
    text_label = NULL;
    char_count_label = NULL;

    // Reset state
    memset(&g_keyboard_state, 0, sizeof(keyboard_state_t));
}
