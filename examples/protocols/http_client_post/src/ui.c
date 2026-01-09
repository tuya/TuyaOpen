/**
 * @file ui_http_client_post.c
 * @brief UI implementation for HTTP client POST example
 *
 * This file implements the UI for the HTTP client POST example, including
 * button creation, response display, and Wi-Fi status indicator.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "ui.h"

#if defined(ENABLE_LIBLVGL) && (ENABLE_LIBLVGL == 1)

#include "tal_api.h"
#include "tkl_output.h"
#include "lvgl.h"
#include "lv_vendor.h"
#include "board_com_api.h"
#include "netmgr.h"

/***********************************************************
********************** variable define *********************
***********************************************************/
static lv_obj_t *response_label = NULL;      // Text box above button
static lv_obj_t *response_container = NULL;  // Container for response text box
static lv_obj_t *receive_label = NULL;       // "Receive" label above text box
static lv_obj_t *send_button = NULL;         // Send button
static lv_obj_t *wifi_status_dot = NULL;     // Wi-Fi status indicator (top-right)
static lv_obj_t *wifi_label = NULL;          // Wi-Fi text label (left of dot)
static ui_button_click_cb_t button_click_cb = NULL;  // Button click callback

/***********************************************************
********************** function define *********************
***********************************************************/

/**
 * @brief Button click event handler
 */
static void button_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {
        PR_NOTICE("Button clicked");
        
        if (button_click_cb != NULL) {
            button_click_cb();
        }
    }
}

/**
 * @brief Initialize LVGL display UI
 */
void ui_http_client_post_init(ui_button_click_cb_t button_cb)
{
    button_click_cb = button_cb;
    
    board_register_hardware();
    lv_vendor_init(DISPLAY_NAME);
    lv_vendor_disp_lock();

    lv_obj_t *screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    // Wi-Fi text label (left of status dot)
    wifi_label = lv_label_create(screen);
    lv_label_set_text(wifi_label, "Wi-Fi");
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(0x000000), 0);
#if defined(LV_FONT_MONTSERRAT_16) && (LV_FONT_MONTSERRAT_16 == 1)
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_16, 0);
#else
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_14, 0);
#endif
    lv_obj_align(wifi_label, LV_ALIGN_TOP_RIGHT, -35, 10);

    // Wi-Fi status indicator (green/red dot in top-right corner)
    wifi_status_dot = lv_obj_create(screen);
    lv_obj_set_size(wifi_status_dot, 20, 20);
    lv_obj_set_style_radius(wifi_status_dot, LV_RADIUS_CIRCLE, 0);  // Make it circular
    lv_obj_set_style_bg_color(wifi_status_dot, lv_color_hex(0xFF0000), 0);  // Red by default (disconnected)
    lv_obj_set_style_bg_opa(wifi_status_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wifi_status_dot, 0, 0);
    lv_obj_align(wifi_status_dot, LV_ALIGN_TOP_RIGHT, -10, 10);

    // Response container (box to wrap response text)
    response_container = lv_obj_create(screen);
    lv_obj_set_size(response_container, LV_HOR_RES - 40, 120);
    lv_obj_set_style_bg_color(response_container, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(response_container, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(response_container, 10, 0);  // Rounded corners
    lv_obj_set_style_border_width(response_container, 2, 0);
    lv_obj_set_style_border_color(response_container, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_pad_all(response_container, 10, 0);
    lv_obj_align(response_container, LV_ALIGN_CENTER, 0, -60);

    // Response text box (inside container)
    response_label = lv_label_create(response_container);
    lv_label_set_text(response_label, "Click button to send request");
    lv_obj_set_style_text_color(response_label, lv_color_hex(0x666666), 0);
#if defined(LV_FONT_MONTSERRAT_18) && (LV_FONT_MONTSERRAT_18 == 1)
    lv_obj_set_style_text_font(response_label, &lv_font_montserrat_18, 0);
#elif defined(LV_FONT_MONTSERRAT_16) && (LV_FONT_MONTSERRAT_16 == 1)
    lv_obj_set_style_text_font(response_label, &lv_font_montserrat_16, 0);
#else
    lv_obj_set_style_text_font(response_label, &lv_font_montserrat_14, 0);
#endif
    lv_obj_align(response_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(response_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(response_label, LV_HOR_RES - 60);

    // "Receive" label (left-top of container)
    receive_label = lv_label_create(screen);
    lv_label_set_text(receive_label, "Receive");
    lv_obj_set_style_text_color(receive_label, lv_color_hex(0x000000), 0);
#if defined(LV_FONT_MONTSERRAT_16) && (LV_FONT_MONTSERRAT_16 == 1)
    lv_obj_set_style_text_font(receive_label, &lv_font_montserrat_16, 0);
#else
    lv_obj_set_style_text_font(receive_label, &lv_font_montserrat_14, 0);
#endif
    // Position "Receive" label at left-top of response_container
    lv_obj_align_to(receive_label, response_container, LV_ALIGN_OUT_TOP_LEFT, 0, -5);

    // Send button (bottom of screen, slightly up)
    send_button = lv_button_create(screen);
    lv_obj_set_size(send_button, 150, 60);
    lv_obj_align(send_button, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_add_event_cb(send_button, button_click_event_cb, LV_EVENT_CLICKED, NULL);

    // Button label
    lv_obj_t *button_label = lv_label_create(send_button);
    lv_label_set_text(button_label, "Send Request");
#if defined(LV_FONT_MONTSERRAT_16) && (LV_FONT_MONTSERRAT_16 == 1)
    lv_obj_set_style_text_font(button_label, &lv_font_montserrat_16, 0);
#else
    lv_obj_set_style_text_font(button_label, &lv_font_montserrat_14, 0);
#endif
    lv_obj_center(button_label);

    lv_vendor_disp_unlock();
    lv_vendor_start(5, 1024 * 8);
    PR_NOTICE("LVGL display initialized");
}

/**
 * @brief Update Wi-Fi status indicator
 */
void ui_update_wifi_status(bool connected)
{
    if (wifi_status_dot == NULL) {
        return;
    }
    
    lv_vendor_disp_lock();
    if (connected) {
        lv_obj_set_style_bg_color(wifi_status_dot, lv_color_hex(0x00FF00), 0);  // Green
    } else {
        lv_obj_set_style_bg_color(wifi_status_dot, lv_color_hex(0xFF0000), 0);   // Red
    }
    lv_vendor_disp_unlock();
}

/**
 * @brief Update response text display
 */
void ui_update_response_text(const char *text, bool is_error)
{
    if (response_label == NULL) {
        return;
    }
    
    lv_vendor_disp_lock();
    if (text != NULL) {
        lv_label_set_text(response_label, text);
        lv_obj_set_style_text_color(response_label, 
                                     is_error ? lv_color_hex(0xFF0000) : lv_color_hex(0x000000), 0);
    } else {
        lv_label_set_text(response_label, "Click button to send request");
        lv_obj_set_style_text_color(response_label, lv_color_hex(0x666666), 0);
    }
    lv_vendor_disp_unlock();
}

/**
 * @brief Update response text display with sending status
 */
void ui_update_response_sending(void)
{
    if (response_label == NULL) {
        return;
    }
    
    lv_vendor_disp_lock();
    lv_label_set_text(response_label, "Sending...");
    lv_obj_set_style_text_color(response_label, lv_color_hex(0x0000FF), 0);
    lv_vendor_disp_unlock();
}

#endif // ENABLE_LIBLVGL

