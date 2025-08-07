/**
 * @file keyboard.c
 * Custom Keyboard Widget for AI Pocket Pet
 */

/*********************
 *      INCLUDES
 *********************/
#include "keyboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*********************
 *      DEFINES
 *********************/
#define KEYBOARD_MAX_TEXT_LENGTH 15
#define KEYBOARD_ROWS 4
#define KEYBOARD_COLS 10
#define KEY_WIDTH 34
#define KEY_HEIGHT 25
#define KEY_SPACING 2

// Key codes - match the main demo
#define KEY_UP    17  // LV_KEY_UP
#define KEY_LEFT  20  // LV_KEY_LEFT
#define KEY_DOWN  18  // LV_KEY_DOWN
#define KEY_RIGHT 19  // LV_KEY_RIGHT
#define KEY_ENTER 10  // LV_KEY_ENTER
#define KEY_ESC   27  // LV_KEY_ESC
#define KEY_M     109 // 'm' key for menu
#define KEY_BACKSPACE 8

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    lv_obj_t *keyboard_screen;
    lv_obj_t *text_area;
    lv_obj_t *keyboard_container;
    lv_obj_t *keys[KEYBOARD_ROWS][KEYBOARD_COLS];
    lv_obj_t *text_label;
    lv_obj_t *char_count_label;

    char current_text[KEYBOARD_MAX_TEXT_LENGTH + 1];
    uint8_t text_length;
    uint8_t selected_row;
    uint8_t selected_col;

    keyboard_callback_t callback;
    void *user_data;

    bool is_active;
} keyboard_widget_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void create_keyboard_layout(keyboard_widget_t *keyboard);
static void create_text_area(keyboard_widget_t *keyboard);
static void key_button_event_cb(lv_event_t *e);
static void keyboard_event_cb(lv_event_t *e);
static void update_text_display(keyboard_widget_t *keyboard);
static void update_selection_highlight(keyboard_widget_t *keyboard);
static void handle_key_press(keyboard_widget_t *keyboard, const char *key_text);
static void handle_special_key(keyboard_widget_t *keyboard, uint32_t key_code);

/**********************
 *  STATIC VARIABLES
 **********************/
static keyboard_widget_t g_keyboard_widget;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void keyboard_init(void)
{
    memset(&g_keyboard_widget, 0, sizeof(keyboard_widget_t));
    g_keyboard_widget.is_active = false;
    g_keyboard_widget.selected_row = 0;
    g_keyboard_widget.selected_col = 0;
    g_keyboard_widget.text_length = 0;
    memset(g_keyboard_widget.current_text, 0, sizeof(g_keyboard_widget.current_text));
}

void keyboard_show(const char *initial_text, keyboard_callback_t callback, void *user_data)
{
    keyboard_widget_t *keyboard = &g_keyboard_widget;

    // Store callback and user data
    keyboard->callback = callback;
    keyboard->user_data = user_data;

    // Initialize text
    if (initial_text) {
        strncpy(keyboard->current_text, initial_text, KEYBOARD_MAX_TEXT_LENGTH);
        keyboard->text_length = strlen(keyboard->current_text);
    } else {
        memset(keyboard->current_text, 0, sizeof(keyboard->current_text));
        keyboard->text_length = 0;
    }

    // Create keyboard screen
    keyboard->keyboard_screen = lv_obj_create(NULL);
    lv_obj_set_size(keyboard->keyboard_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(keyboard->keyboard_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(keyboard->keyboard_screen, LV_OPA_COVER, 0);

    // Add keyboard event handler
    lv_obj_add_event_cb(keyboard->keyboard_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), keyboard->keyboard_screen);

    // Create UI components
    create_text_area(keyboard);
    create_keyboard_layout(keyboard);

    // Load the screen
    lv_screen_load(keyboard->keyboard_screen);

    keyboard->is_active = true;
    update_selection_highlight(keyboard);
}

void keyboard_hide(void)
{
    keyboard_widget_t *keyboard = &g_keyboard_widget;

    if (keyboard->is_active && keyboard->keyboard_screen) {
        // Check if the keyboard screen is currently active
        lv_obj_t *active_screen = lv_screen_active();
        if (active_screen == keyboard->keyboard_screen) {
            // The keyboard screen is active, we need to load a different screen first
            // to avoid deleting the active screen
            printf("Warning: Keyboard screen is active, cannot delete safely\n");
            // Don't delete for now - let the callback handle screen restoration
            keyboard->is_active = false;
            return;
        }

        lv_obj_del(keyboard->keyboard_screen);
        keyboard->keyboard_screen = NULL;
        keyboard->is_active = false;
    }
}

void keyboard_handle_input(uint32_t key)
{
    keyboard_widget_t *keyboard = &g_keyboard_widget;

    if (!keyboard->is_active) {
        return;
    }

    switch (key) {
        case KEY_UP:
            if (keyboard->selected_row > 0) {
                keyboard->selected_row--;
                update_selection_highlight(keyboard);
            }
            break;

        case KEY_DOWN:
            if (keyboard->selected_row < KEYBOARD_ROWS - 1) {
                keyboard->selected_row++;
                update_selection_highlight(keyboard);
            }
            break;

        case KEY_LEFT:
            if (keyboard->selected_col > 0) {
                keyboard->selected_col--;
                update_selection_highlight(keyboard);
            }
            break;

        case KEY_RIGHT:
            if (keyboard->selected_col < KEYBOARD_COLS - 1) {
                keyboard->selected_col++;
                update_selection_highlight(keyboard);
            }
            break;

        case KEY_ENTER:
            // Activate the selected key
            if (keyboard->keys[keyboard->selected_row][keyboard->selected_col]) {
                lv_obj_t *key_obj = keyboard->keys[keyboard->selected_row][keyboard->selected_col];
                lv_obj_t *label = lv_obj_get_child(key_obj, 0);
                if (label) {
                    const char *key_text = lv_label_get_text(label);
                    if (key_text) {
                        handle_key_press(keyboard, key_text);
                    }
                }
            }
            break;

        case KEY_ESC:
            // Cancel input
            if (keyboard->callback) {
                keyboard->callback(KEYBOARD_RESULT_CANCEL, NULL, keyboard->user_data);
            }
            keyboard_hide();
            break;

        case KEY_BACKSPACE:
            // Handle backspace key
            if (keyboard->text_length > 0) {
                keyboard->text_length--;
                keyboard->current_text[keyboard->text_length] = '\0';
                update_text_display(keyboard);
            }
            break;

        case KEY_M:
            // Menu button - treat as special function
            if (keyboard->callback) {
                keyboard->callback(KEYBOARD_RESULT_MENU, NULL, keyboard->user_data);
            }
            // Don't hide keyboard for menu key - let callback decide
            break;

        default:
            // Handle other keys if needed
            handle_special_key(keyboard, key);
            break;
    }
}

bool keyboard_is_active(void)
{
    return g_keyboard_widget.is_active;
}

void keyboard_cleanup(void)
{
    keyboard_widget_t *keyboard = &g_keyboard_widget;

    // Now it's safe to delete the keyboard screen if it exists
    if (keyboard->keyboard_screen) {
        lv_obj_del(keyboard->keyboard_screen);
        keyboard->keyboard_screen = NULL;
    }
    keyboard->is_active = false;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void create_text_area(keyboard_widget_t *keyboard)
{
    // Create text display area at the top
    keyboard->text_area = lv_obj_create(keyboard->keyboard_screen);
    lv_obj_set_size(keyboard->text_area, AI_PET_SCREEN_WIDTH - 20, 40);
    lv_obj_align(keyboard->text_area, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_color(keyboard->text_area, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(keyboard->text_area, LV_OPA_10, 0);
    lv_obj_set_style_border_width(keyboard->text_area, 1, 0);
    lv_obj_set_style_border_color(keyboard->text_area, lv_color_black(), 0);
    lv_obj_set_style_radius(keyboard->text_area, 5, 0);

    // Text label
    keyboard->text_label = lv_label_create(keyboard->text_area);
    lv_obj_align(keyboard->text_label, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_text_color(keyboard->text_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(keyboard->text_label, &lv_font_montserrat_14, 0);

    // Character count label
    keyboard->char_count_label = lv_label_create(keyboard->text_area);
    lv_obj_align(keyboard->char_count_label, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_text_color(keyboard->char_count_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(keyboard->char_count_label, &lv_font_montserrat_14, 0);

    update_text_display(keyboard);
}

static void create_keyboard_layout(keyboard_widget_t *keyboard)
{
    // Create keyboard container
    keyboard->keyboard_container = lv_obj_create(keyboard->keyboard_screen);
    lv_obj_set_size(keyboard->keyboard_container, AI_PET_SCREEN_WIDTH - 20, 120);
    lv_obj_align(keyboard->keyboard_container, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_bg_opa(keyboard->keyboard_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(keyboard->keyboard_container, 0, 0);
    lv_obj_set_style_pad_all(keyboard->keyboard_container, 2, 0);

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
            keyboard->keys[row][col] = lv_btn_create(keyboard->keyboard_container);
            lv_obj_set_size(keyboard->keys[row][col], KEY_WIDTH, KEY_HEIGHT);

            // Position keys
            int x_pos = col * (KEY_WIDTH + KEY_SPACING);
            int y_pos = row * (KEY_HEIGHT + KEY_SPACING);
            lv_obj_set_pos(keyboard->keys[row][col], x_pos, y_pos);

            // Style keys
            lv_obj_set_style_bg_color(keyboard->keys[row][col], lv_color_white(), 0);
            lv_obj_set_style_bg_opa(keyboard->keys[row][col], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(keyboard->keys[row][col], 1, 0);
            lv_obj_set_style_border_color(keyboard->keys[row][col], lv_color_black(), 0);
            lv_obj_set_style_radius(keyboard->keys[row][col], 3, 0);
            lv_obj_set_style_text_color(keyboard->keys[row][col], lv_color_black(), 0);
            lv_obj_set_style_text_font(keyboard->keys[row][col], &lv_font_montserrat_14, 0);

            // Create label for key
            lv_obj_t *label = lv_label_create(keyboard->keys[row][col]);
            lv_label_set_text(label, keyboard_layout[row][col]);
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

            // Add event callback
            lv_obj_add_event_cb(keyboard->keys[row][col], key_button_event_cb, LV_EVENT_CLICKED, keyboard);
        }
    }
}

static void key_button_event_cb(lv_event_t *e)
{
    keyboard_widget_t *keyboard = (keyboard_widget_t *)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);

    // Find which key was pressed
    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        for (int col = 0; col < KEYBOARD_COLS; col++) {
            if (keyboard->keys[row][col] == btn) {
                lv_obj_t *label = lv_obj_get_child(btn, 0);
                if (label) {
                    const char *key_text = lv_label_get_text(label);
                    if (key_text) {
                        handle_key_press(keyboard, key_text);
                    }
                }
                break;
            }
        }
    }
}

static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    keyboard_handle_input(key);
}

static void update_text_display(keyboard_widget_t *keyboard)
{
    // Update text label
    lv_label_set_text(keyboard->text_label, keyboard->current_text);

    // Update character count
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%d/%d", keyboard->text_length, KEYBOARD_MAX_TEXT_LENGTH);
    lv_label_set_text(keyboard->char_count_label, count_str);
}

static void update_selection_highlight(keyboard_widget_t *keyboard)
{
    // Reset all keys to default style
    for (int row = 0; row < KEYBOARD_ROWS; row++) {
        for (int col = 0; col < KEYBOARD_COLS; col++) {
            if (keyboard->keys[row][col]) {
                lv_obj_set_style_bg_color(keyboard->keys[row][col], lv_color_white(), 0);
                lv_obj_set_style_border_color(keyboard->keys[row][col], lv_color_black(), 0);
                lv_obj_set_style_border_width(keyboard->keys[row][col], 1, 0);
                lv_obj_set_style_text_color(keyboard->keys[row][col], lv_color_black(), 0);
            }
        }
    }

    // Highlight selected key
    if (keyboard->keys[keyboard->selected_row][keyboard->selected_col]) {
        lv_obj_set_style_bg_color(keyboard->keys[keyboard->selected_row][keyboard->selected_col], lv_color_black(), 0);
        lv_obj_set_style_border_color(keyboard->keys[keyboard->selected_row][keyboard->selected_col], lv_color_black(), 0);
        lv_obj_set_style_border_width(keyboard->keys[keyboard->selected_row][keyboard->selected_col], 2, 0);
        lv_obj_set_style_text_color(keyboard->keys[keyboard->selected_row][keyboard->selected_col], lv_color_white(), 0);
    }
}

static void handle_key_press(keyboard_widget_t *keyboard, const char *key_text)
{
    if (strcmp(key_text, "<-") == 0) {
        // Backspace
        if (keyboard->text_length > 0) {
            keyboard->text_length--;
            keyboard->current_text[keyboard->text_length] = '\0';
            update_text_display(keyboard);
        }
    } else if (strcmp(key_text, "OK") == 0) {
        // Enter - confirm input
        if (keyboard->callback) {
            keyboard->callback(KEYBOARD_RESULT_OK, keyboard->current_text, keyboard->user_data);
        }
        keyboard_hide();
    } else if (strcmp(key_text, "ESC") == 0) {
        // Escape - cancel input
        if (keyboard->callback) {
            keyboard->callback(KEYBOARD_RESULT_CANCEL, NULL, keyboard->user_data);
        }
        keyboard_hide();
    } else if (strcmp(key_text, " ") == 0) {
        // Space character
        if (keyboard->text_length < KEYBOARD_MAX_TEXT_LENGTH) {
            keyboard->current_text[keyboard->text_length] = ' ';
            keyboard->text_length++;
            keyboard->current_text[keyboard->text_length] = '\0';
            update_text_display(keyboard);
        }
    } else if (strlen(key_text) == 1) {
        // Regular character (excluding 'M' which is handled above)
        if (keyboard->text_length < KEYBOARD_MAX_TEXT_LENGTH) {
            keyboard->current_text[keyboard->text_length] = key_text[0];
            keyboard->text_length++;
            keyboard->current_text[keyboard->text_length] = '\0';
            update_text_display(keyboard);
        }
    }
}

static void handle_special_key(keyboard_widget_t *keyboard, uint32_t key_code)
{
    (void)keyboard;
    (void)key_code;
    // Handle special keys if needed
    // For now, just ignore unknown keys
}
