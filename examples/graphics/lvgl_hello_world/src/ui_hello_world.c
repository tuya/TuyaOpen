/**
 * @file ui_hello_world.c
 * @brief UI implementation for hello world example
 *
 * This file implements the UI for the hello world example, including:
 * - Button creation and styling
 * - Button click event handling
 * - Message box display
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "ui_hello_world.h"

#if defined(ENABLE_LIBLVGL) && (ENABLE_LIBLVGL == 1)

#include "tal_api.h"
#include "tkl_output.h"
#include "lvgl.h"

/***********************************************************
*********************** macro define ***********************
***********************************************************/

/***********************************************************
********************** typedef define **********************
***********************************************************/

/***********************************************************
********************** variable define *********************
***********************************************************/
static lv_obj_t *hello_button = NULL;
static lv_obj_t *msgbox = NULL;

/***********************************************************
********************** function define *********************
***********************************************************/

/**
 * @brief Close button event handler for message box
 * 
 * @param e Event object
 */
static void msgbox_close_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (msgbox != NULL) {
            lv_msgbox_close(msgbox);
            msgbox = NULL;
        }
    }
}

/**
 * @brief Show hello world message box
 */
static void show_hello_world_msgbox(void)
{
    // Close existing message box if any
    if (msgbox != NULL) {
        lv_msgbox_close(msgbox);
        msgbox = NULL;
    }

    // Create a modal message box (NULL parent = modal)
    msgbox = lv_msgbox_create(NULL);

    // Set message box size (width, height)
    lv_obj_set_size(msgbox, 300, 200);  // Increased height

    // Add title
    lv_msgbox_add_title(msgbox, "Msg Box Title");

    // Add text content and get the text label
    lv_obj_t *text_label = lv_msgbox_add_text(msgbox, "hello world ~~");
    
    // Set larger font for the text
    if (text_label != NULL) {
#if defined(LV_FONT_MONTSERRAT_24) && (LV_FONT_MONTSERRAT_24 == 1)
        lv_obj_set_style_text_font(text_label, &lv_font_montserrat_24, 0);
#elif defined(LV_FONT_MONTSERRAT_20) && (LV_FONT_MONTSERRAT_20 == 1)
        lv_obj_set_style_text_font(text_label, &lv_font_montserrat_20, 0);
#elif defined(LV_FONT_MONTSERRAT_18) && (LV_FONT_MONTSERRAT_18 == 1)
        lv_obj_set_style_text_font(text_label, &lv_font_montserrat_18, 0);
#elif defined(LV_FONT_MONTSERRAT_16) && (LV_FONT_MONTSERRAT_16 == 1)
        lv_obj_set_style_text_font(text_label, &lv_font_montserrat_16, 0);
#else
        lv_obj_set_style_text_font(text_label, &lv_font_montserrat_14, 0);
#endif
    }

    // Add close button
    lv_obj_t *close_btn = lv_msgbox_add_close_button(msgbox);
    if (close_btn != NULL) {
        lv_obj_add_event_cb(close_btn, msgbox_close_event_cb, LV_EVENT_CLICKED, NULL);
    }

    // Center the message box on screen
    lv_obj_center(msgbox);
}

/**
 * @brief Button click event handler
 * 
 * @param e Event object
 */
static void button_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {
        PR_NOTICE("Button clicked, showing hello world message box");
        show_hello_world_msgbox();
    }
}

/**
 * @brief Initialize the hello world UI
 * 
 * Creates a button on the screen that displays a "hello world" message box when clicked.
 */
void ui_hello_world_init(void)
{
    // Get the active screen
    lv_obj_t *screen = lv_screen_active();
    
    // Set screen background color to white
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFFFFF), 0);

    // Create a button
    hello_button = lv_button_create(screen);
    
    // Set button size
    lv_obj_set_size(hello_button, 150, 60);
    
    // Center the button on screen
    lv_obj_center(hello_button);
    
    // Add click event handler
    lv_obj_add_event_cb(hello_button, button_click_event_cb, LV_EVENT_CLICKED, NULL);
    
    // Create a label inside the button
    lv_obj_t *label = lv_label_create(hello_button);
    lv_label_set_text(label, "Button");
    lv_obj_center(label);
    
    // Style the button label
#if defined(LV_FONT_MONTSERRAT_18) && (LV_FONT_MONTSERRAT_18 == 1)
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
#elif defined(LV_FONT_MONTSERRAT_16) && (LV_FONT_MONTSERRAT_16 == 1)
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
#else
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
#endif
    
    PR_NOTICE("Hello world UI initialized");
}

#endif // ENABLE_LIBLVGL

