/**
 * @file standby_screen.c
 * @brief Implementation of the standby screen for the application
 *
 * This file contains the implementation of the standby screen which displays
 * "TuyaOpen" text with a 3D horizontal rotation animation effect in black and white.
 *
 * The standby screen includes:
 * - A monochrome white background
 * - "TuyaOpen" text with 3D rotation animation (black text)
 * - Smooth animation using LVGL animation API
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "standby_screen.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ROTATION_DURATION 2000  // Animation duration in milliseconds
#define LETTER_SPACING 8        // Spacing between letters

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_standby_screen;
static lv_obj_t *letter_labels[8];  // "TuyaOpen" has 8 letters
static lv_anim_t rotation_anims[8];
static int16_t rotation_angles[8] = {0};
static const char *text = "TuyaOpen";

Screen_t standby_screen = {
    .init = standby_screen_init,
    .deinit = standby_screen_deinit,
    .screen_obj = &ui_standby_screen,
    .name = "standby",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void keyboard_event_cb(lv_event_t *e);
static void rotation_anim_cb(void *var, int32_t value);
static void create_letter_label(int index, char letter, lv_coord_t x_offset);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the standby screen.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", standby_screen.name, key);
    screen_back();

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
            printf("ESC key pressed - going back\n");
            break;
        default:
            printf("Unknown key pressed\n");
            break;
    }
}

/**
 * @brief Animation callback for 3D rotation effect
 *
 * This function is called during animation to update the letter's appearance
 * based on rotation angle, creating a 3D rotation effect.
 *
 * @param var Pointer to the letter index
 * @param value Current rotation angle (0-360 degrees)
 */
static void rotation_anim_cb(void *var, int32_t value)
{
    int index = (int)(intptr_t)var;
    rotation_angles[index] = value;

    lv_obj_t *label = letter_labels[index];
    if (!label) return;

    // Convert angle to radians
    float angle_rad = (value % 360) * M_PI / 180.0f;

    // Calculate scale based on cosine (simulating 3D rotation)
    // When angle is 0° or 360°, cos = 1 (full width)
    // When angle is 90° or 270°, cos = 0 (minimal width)
    float cos_val = cosf(angle_rad);
    float scale_x = fabsf(cos_val);

    // Minimum scale to prevent complete disappearance
    if (scale_x < 0.1f) scale_x = 0.1f;

    // Apply horizontal scaling to simulate 3D rotation
    lv_obj_set_style_transform_pivot_x(label, lv_obj_get_width(label) / 2, 0);
    lv_obj_set_style_transform_pivot_y(label, lv_obj_get_height(label) / 2, 0);

    // Scale horizontally
    int32_t scale_x_256 = (int32_t)(scale_x * 256);
    lv_obj_set_style_transform_scale_x(label, scale_x_256, 0);

    // Adjust opacity based on angle (fade when edge-on)
    lv_opa_t opa = (lv_opa_t)(LV_OPA_COVER * (0.3f + 0.7f * scale_x));
    lv_obj_set_style_opa(label, opa, 0);
}

/**
 * @brief Create a single letter label
 *
 * This function creates a label for a single letter with appropriate styling.
 *
 * @param index Index of the letter in the text
 * @param letter The character to display
 * @param x_offset Horizontal offset from center
 */
static void create_letter_label(int index, char letter, lv_coord_t x_offset)
{
    letter_labels[index] = lv_label_create(ui_standby_screen);

    char letter_str[2] = {letter, '\0'};
    lv_label_set_text(letter_labels[index], letter_str);

    // Set font to a larger size for better visibility
    lv_obj_set_style_text_font(letter_labels[index], &lv_font_montserrat_32, 0);

    // Set text color - black only for monochrome display
    lv_obj_set_style_text_color(letter_labels[index], lv_color_black(), 0);

    // Position the letter
    lv_obj_align(letter_labels[index], LV_ALIGN_CENTER, x_offset, 0);

    // Enable transform for this object
    lv_obj_set_style_transform_pivot_x(letter_labels[index],
                                       lv_obj_get_width(letter_labels[index]) / 2, 0);
    lv_obj_set_style_transform_pivot_y(letter_labels[index],
                                       lv_obj_get_height(letter_labels[index]) / 2, 0);
}

/**
 * @brief Initialize the standby screen
 *
 * This function creates the standby screen UI with a white background,
 * creates individual letter labels for "TuyaOpen" in black, and starts the rotation animations
 * with phase offsets to create a wave effect.
 */
void standby_screen_init(void)
{
    printf("[%s] Initializing standby screen\n", standby_screen.name);

    // Create the main screen
    ui_standby_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_standby_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);

    // Set white background for monochrome display
    lv_obj_set_style_bg_color(ui_standby_screen, lv_color_white(), 0);

    // Calculate total width of text
    int text_len = strlen(text);
    lv_coord_t letter_width = 20;  // Approximate width per letter
    lv_coord_t total_width = text_len * (letter_width + LETTER_SPACING);
    lv_coord_t start_x = -total_width / 2 + letter_width / 2;

    // Create individual letter labels
    for (int i = 0; i < text_len; i++) {
        lv_coord_t x_offset = start_x + i * (letter_width + LETTER_SPACING);
        create_letter_label(i, text[i], x_offset);
    }

    // Start rotation animations with phase offsets for wave effect
    for (int i = 0; i < text_len; i++) {
        lv_anim_init(&rotation_anims[i]);
        lv_anim_set_var(&rotation_anims[i], (void *)(intptr_t)i);
        lv_anim_set_exec_cb(&rotation_anims[i], rotation_anim_cb);
        lv_anim_set_duration(&rotation_anims[i], ROTATION_DURATION);
        lv_anim_set_repeat_count(&rotation_anims[i], LV_ANIM_REPEAT_INFINITE);
        
        // Distribute 8 letters across 180° range with 22.5° spacing (reversed order)
        // Letter 0: 157.5°, Letter 1: 135°, Letter 2: 112.5°, ..., Letter 7: 0°
        int32_t start_angle = (text_len - 1 - i) * 22;  // Reverse order: 7*22, 6*22, ..., 0*22
        int32_t end_angle = start_angle + 360;
        lv_anim_set_values(&rotation_anims[i], start_angle, end_angle);
        
        // Use linear path for smooth continuous rotation
        lv_anim_set_path_cb(&rotation_anims[i], lv_anim_path_linear);

        lv_anim_start(&rotation_anims[i]);
    }

    // Add keyboard event handling
    lv_obj_add_event_cb(ui_standby_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_standby_screen);
    lv_group_focus_obj(ui_standby_screen);

    printf("[%s] Standby screen initialized successfully\n", standby_screen.name);
}

/**
 * @brief Deinitialize the standby screen
 *
 * This function cleans up the standby screen by stopping all animations,
 * deleting the UI objects, and removing event callbacks.
 */
void standby_screen_deinit(void)
{
    printf("[%s] Deinitializing standby screen\n", standby_screen.name);

    // Stop all animations
    for (int i = 0; i < 8; i++) {
        lv_anim_delete(&rotation_anims[i], NULL);
        letter_labels[i] = NULL;
    }

    if (ui_standby_screen) {
        lv_obj_remove_event_cb(ui_standby_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_standby_screen);
        // Screen object will be cleaned up by screen manager
    }

    printf("[%s] Standby screen deinitialized\n", standby_screen.name);
}
