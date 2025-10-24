/**
 * @file peripherals_scan.c
 * @brief Implements peripheral scanning functionality for IoT device
 *
 * This source file provides the implementation of peripheral scanning functionalities
 * required for an IoT device. It includes functionality for scanning and identifying
 * connected peripherals, managing device connections, and handling communication
 * protocols. The implementation supports various peripheral types and ensures
 * seamless integration with the Tuya IoT platform. This file is essential for developers
 * working on IoT applications that require peripheral management and integration
 * with the Tuya IoT ecosystem.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "peripherals_scan.h"
#include "ai_pocket_pet_app.h"
#include "toast.h"
#include "stdio.h"
#ifndef LVGL_SIMULATOR
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_pinmux.h"
#include "tkl_i2c.h"
#include "tal_wifi.h"
#endif

LV_IMG_DECLARE(peripherals_scan_left_icon);
LV_IMG_DECLARE(peripherals_scan_right_icon);
/*********************
 *      DEFINES
 *********************/
#define KEY_UP    17  // LV_KEY_UP
#define KEY_LEFT  20  // LV_KEY_LEFT
#define KEY_DOWN  18  // LV_KEY_DOWN
#define KEY_RIGHT 19  // LV_KEY_RIGHT
#define KEY_ENTER 10  // LV_KEY_ENTER
#define KEY_ESC   27  // LV_KEY_ESC

typedef struct {
    lv_obj_t *scan_screen;    // main screen object
    lv_obj_t *dev_list;       // device list object
    bool is_active;           // state flag
} scan_widget_t;

static scan_widget_t g_scan_widget;

typedef struct {
    lv_obj_t *wifi_screen;
    lv_obj_t *ap_list;
    bool is_active;
} wifi_widget_t;

static wifi_widget_t g_wifi_widget;

void i2c_scan_hidden(void)
{
    scan_widget_t *widget = &g_scan_widget;
    printf("Hiding scan screen");

    if (widget->is_active) {
        if (widget->scan_screen) {
            // Remove object from input group
            lv_group_t * g = lv_group_get_default();
            if(g) {
                lv_group_remove_obj(widget->scan_screen);
            }

            // Delete the entire scan screen
            printf("Deleting scan screen");
            // lv_obj_del_async(widget->scan_screen);
            lv_obj_del(widget->scan_screen);
            widget->scan_screen = NULL;
            widget->dev_list = NULL;
            widget->is_active = false;

            printf("Scan screen hidden and deleted");
        }
    }
}

/*
 * Expose scan active state and input handler so the main input dispatcher
 * can route key presses to the scan screen (same model as keyboard widget).
 */
bool i2c_scan_is_active(void)
{
    return g_scan_widget.is_active;
}

// Port information structure
typedef struct {
    char port_name[10];
    int scl;
    int sda;
} port_info_t;

// Define array of port info containing PORT0, PORT1 and PORT2
static port_info_t port_info[] = {
    {"PORT 0", 20, 21},
    {"PORT 1", 4, 5},
    {"PORT 2", 6, 7}
};

static int current_port_index = 0; // current port index

void i2c_scan_handle_input(uint32_t key)
{
    scan_widget_t *widget = &g_scan_widget;
    if (!widget->is_active) return;

    printf("[Scan] handle_input key=%d", key);

    switch(key) {
        case KEY_ESC:
            printf("[Scan] ESC pressed via input handler");
            i2c_scan_hidden();
            lv_screen_load(lv_demo_ai_pocket_pet_get_main_screen());
            break;
        case KEY_UP:
            // Scroll content up
            {
                // widget->dev_list is a matrix_container
                if (widget->dev_list) {
                    lv_obj_t *content_container = lv_obj_get_child(widget->dev_list, 1); // get content container
                    // Check if already scrolled to top
                    lv_coord_t scroll_top = lv_obj_get_scroll_top(content_container);
                    if (scroll_top > 0) {
                        // Limit scroll step to remaining scrollable distance
                        lv_coord_t scroll_step = (scroll_top > 20) ? 20 : scroll_top;
                        lv_obj_scroll_by(content_container, 0, scroll_step, LV_ANIM_ON);
                    }
                }
            }
            break;
        case KEY_DOWN:
            // Scroll content down
            {
                // widget->dev_list is a matrix_container
                if (widget->dev_list) {
                    lv_obj_t *content_container = lv_obj_get_child(widget->dev_list, 1); // get content container
                    // Check if already scrolled to bottom
                    lv_coord_t scroll_bottom = lv_obj_get_scroll_bottom(content_container);
                    if (scroll_bottom > 0) {
                        // Limit scroll step to remaining scrollable distance
                        lv_coord_t scroll_step = (scroll_bottom > 20) ? 20 : scroll_bottom;
                        lv_obj_scroll_by(content_container, 0, -scroll_step, LV_ANIM_ON);
                    }
                }
            }
            break;
        case KEY_LEFT:
            // Switch to previous PORT
            if (current_port_index > 0) {
                current_port_index--;
                // update PORT info display
                lv_obj_t *info_bar = lv_obj_get_child(widget->scan_screen, 2); // get info_bar object
                if (info_bar) {
                    char port_text[32];
                    snprintf(port_text, sizeof(port_text), "%s : SCL=%d, SDA=%d",
                             port_info[current_port_index].port_name,
                             port_info[current_port_index].scl,
                             port_info[current_port_index].sda);
                    lv_label_set_text(info_bar, port_text);
#ifndef LVGL_SIMULATOR
                    printf("[Scan] Current PORT: %d", current_port_index);
                    tkl_io_pinmux_config(port_info[current_port_index].scl, current_port_index*2);
                    tkl_io_pinmux_config(port_info[current_port_index].sda, current_port_index*2+1);
                    TUYA_IIC_BASE_CFG_T cfg;
                    cfg.role = TUYA_IIC_MODE_MASTER;
                    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
                    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

                    tkl_i2c_init(current_port_index, &cfg);
                    i2c_scan_show(current_port_index);
#endif
                }
            }
            break;
        case KEY_RIGHT:
            // Switch to next PORT
            if (current_port_index < (sizeof(port_info) / sizeof(port_info[0]) - 1)) {
                current_port_index++;
                // Update PORT info display
                lv_obj_t *info_bar = lv_obj_get_child(widget->scan_screen, 2); // get info_bar object
                if (info_bar) {
                    char port_text[32];
                    snprintf(port_text, sizeof(port_text), "%s : SCL=%d, SDA=%d",
                             port_info[current_port_index].port_name,
                             port_info[current_port_index].scl,
                             port_info[current_port_index].sda);
                    lv_label_set_text(info_bar, port_text);
#ifndef LVGL_SIMULATOR
                    tkl_io_pinmux_config(port_info[current_port_index].scl, current_port_index*2);
                    tkl_io_pinmux_config(port_info[current_port_index].sda, current_port_index*2+1);
                    TUYA_IIC_BASE_CFG_T cfg;
                    cfg.role = TUYA_IIC_MODE_MASTER;
                    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
                    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

                    tkl_i2c_init(current_port_index, &cfg);
                    i2c_scan_show(current_port_index);
#endif
                }
            }
            break;
        case KEY_ENTER:
            // TODO: handle selection if needed
            break;
        default:
            break;
    }
}

void i2c_scan_show(uint8_t i2c_port)
{
#ifndef LVGL_SIMULATOR
    if (i2c_port >= TUYA_I2C_NUM_MAX) {
        printf("[ERROR] Invalid I2C port number: %d", i2c_port);
        return;
    }
#endif
    scan_widget_t *widget = &g_scan_widget;

    // Set current PORT index to the input port number
    current_port_index = i2c_port;
    uint8_t dev_num = 0;

    // If already exists, clean up first
    if (widget->is_active) {
        i2c_scan_hidden();
    }

    // Create new scan screen
    widget->scan_screen = lv_obj_create(NULL);
    lv_obj_set_size(widget->scan_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(widget->scan_screen, lv_color_white(), 0);

    // Create title
    lv_obj_t *title = lv_label_create(widget->scan_screen);
    lv_label_set_text(title, "I2C Device Scan Results");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);

    // Create PORT info row (placed below the title)
    // Left icon
    lv_obj_t *left_icon = lv_img_create(widget->scan_screen);
    lv_img_set_src(left_icon, &peripherals_scan_left_icon);
    lv_obj_align(left_icon, LV_ALIGN_TOP_MID, -85, 25);
    lv_img_set_zoom(left_icon, 200); // Scale to about 78% of original size (256 is original size, 200/256≈0.78)

    lv_obj_t *info_bar = lv_label_create(widget->scan_screen);
    char port_text[32];
    snprintf(port_text, sizeof(port_text), "%s : SCL=%d, SDA=%d",
             port_info[current_port_index].port_name,
             port_info[current_port_index].scl,
             port_info[current_port_index].sda);
    lv_label_set_text(info_bar, port_text);
    lv_obj_align(info_bar, LV_ALIGN_TOP_MID, 0, 29);
    lv_obj_set_style_text_font(info_bar, &lv_font_montserrat_12, 0);

    printf("[Scan] Displaying PORT %d: SCL=%d, SDA=%d", current_port_index,
           port_info[current_port_index].scl, port_info[current_port_index].sda);

    // Right icon
    lv_obj_t *right_icon = lv_img_create(widget->scan_screen);
    lv_img_set_src(right_icon, &peripherals_scan_right_icon);
    lv_obj_align(right_icon, LV_ALIGN_TOP_MID, 85, 25);
    lv_img_set_zoom(right_icon, 200); // Scale to about 78% of original size (256 is original size, 200/256≈0.78)

    // Create matrix to display I2C addresses
    lv_obj_t *matrix_container = lv_obj_create(widget->scan_screen);
    widget->dev_list = matrix_container; // Save reference for scrolling
    lv_obj_set_size(matrix_container, AI_PET_SCREEN_WIDTH - 20, AI_PET_SCREEN_HEIGHT - 50); // Matrix size
    lv_obj_align(matrix_container, LV_ALIGN_CENTER, 0, 20); // Move down a bit more
    lv_obj_set_style_border_color(matrix_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(matrix_container, 2, 0);
    lv_obj_set_flex_flow(matrix_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(matrix_container, 2, 0);
    lv_obj_clear_flag(matrix_container, LV_OBJ_FLAG_SCROLLABLE); // Disable container scrolling

    // Create header row (display 0 1 2 3 4 5 6 7 8 9 A B C D E F)
    lv_obj_t *header_row = lv_obj_create(matrix_container);
    lv_obj_set_size(header_row, LV_PCT(100), 20);
    lv_obj_set_flex_flow(header_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(header_row, 1, 0);
    lv_obj_set_style_pad_all(header_row, 2, 0);

    // Add empty space for top-left corner
    lv_obj_t *empty_label = lv_label_create(header_row);
    lv_label_set_text(empty_label, "");
    lv_obj_set_width(empty_label, 30);

    // Add hexadecimal column headers
    for (int col = 0; col < 16; col++) {
        lv_obj_t *label = lv_label_create(header_row);
        char hex_char[2];
        if (col < 10) {
            hex_char[0] = '0' + col;
        } else {
            hex_char[0] = 'A' + (col - 10);
        }
        hex_char[1] = '\0';
        lv_label_set_text(label, hex_char);
        lv_obj_set_width(label, 16);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);  // Use smaller font
    }

    // Create scrollable content container
    lv_obj_t *content_container = lv_obj_create(matrix_container);
    lv_obj_set_size(content_container, LV_PCT(100), AI_PET_SCREEN_HEIGHT - 100); // Increase height by 20 pixels
    lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content_container, 0, 0);
    lv_obj_set_style_border_width(content_container, 0, 0);
    lv_obj_set_scroll_dir(content_container, LV_DIR_VER); // Only allow vertical scrolling
    lv_obj_set_style_pad_gap(content_container, 0, 0);

    // Create matrix of 128 addresses (0x00 - 0x7F)
    for (int row = 0; row < 8; row++) {
        lv_obj_t *row_container = lv_obj_create(content_container);
        lv_obj_set_size(row_container, LV_PCT(100), 16);  // Reduce row height
        lv_obj_set_flex_flow(row_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_gap(row_container, 1, 0);
        lv_obj_set_style_pad_all(row_container, 1, 0);  // Reduce padding

        // Add row header (0x 1x 2x ... 7x)
        lv_obj_t *row_label = lv_label_create(row_container);
        char row_text[4];
        snprintf(row_text, sizeof(row_text), "%Xx", row);
        lv_label_set_text(row_label, row_text);
        lv_obj_set_width(row_label, 30);  // Increase width to align with empty column header
        lv_obj_set_style_text_font(row_label, &lv_font_montserrat_10, 0);  // Use smaller font
        lv_obj_set_style_text_align(row_label, LV_TEXT_ALIGN_CENTER, 0); // Center align text

        // Add 16 address cells for this row
        for (int col = 0; col < 16; col++) {
            uint8_t addr = (row << 4) | col;
            lv_obj_t *cell = lv_label_create(row_container);
#if LVGL_SIMULATOR
            // For valid I2C address range, display address
            if (addr <= 0x7F) {
                char addr_text[5];
                snprintf(addr_text, sizeof(addr_text), "%02X", addr);
                lv_label_set_text(cell, addr_text);
            } else {
                lv_label_set_text(cell, "--");
            }

            lv_obj_set_width(cell, 16);  // Consistent with column header width
            lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_radius(cell, 3, 0);
            lv_obj_set_style_bg_color(cell, lv_color_hex(0xf0f0f0), 0);
            lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
            lv_obj_set_style_text_font(cell, &lv_font_montserrat_10, 0);  // Use smaller font
#else
            uint8_t i2c_addr = addr;

            uint8_t data_buf[1] = {0};
            if (OPRT_OK == tkl_i2c_master_send(i2c_port, i2c_addr, data_buf, 0, TRUE)) {
                dev_num++;
                if (dev_num >= i2c_addr) {
                    lv_label_set_text(cell, "");
                    continue;
                }
                if (i2c_addr <= 0x7F) {
                    char addr_text[5];
                    snprintf(addr_text, sizeof(addr_text), "%02X", i2c_addr);
                    PR_DEBUG("Found I2C device at address %s", addr_text);
                    lv_label_set_text(cell, addr_text);
                }
                else {
                    lv_label_set_text(cell, "");
                }

                lv_obj_set_width(cell, 16);  // Consistent with column header width
                lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_set_style_radius(cell, 3, 0);
                lv_obj_set_style_bg_color(cell, lv_color_white(), 0);
                lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
                lv_obj_set_style_text_font(cell, &lv_font_montserrat_10, 0);  // Use smaller font
            }
            else
            {
                lv_label_set_text(cell, "");
                lv_obj_set_width(cell, 16);  // Consistent with column header width
                lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_set_style_radius(cell, 3, 0);
                lv_obj_set_style_bg_color(cell, lv_color_white(), 0);
                lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
                lv_obj_set_style_text_font(cell, &lv_font_montserrat_10, 0);  // Use smaller font
            }
#endif
        }
    }

    // Set screen properties
    lv_obj_clear_flag(widget->scan_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(widget->scan_screen, LV_OBJ_FLAG_CLICKABLE);

    // Load screen
    lv_screen_load(widget->scan_screen);
    widget->is_active = true;

    printf("I2C scan matrix screen created");
}

void wifi_scan_hidden(void)
{
    wifi_widget_t *w = &g_wifi_widget;
    printf("Hiding wifi scan screen");
    if (w->is_active) {
        if (w->wifi_screen) {
            lv_group_t * g = lv_group_get_default();
            if (g) {
                lv_group_remove_obj(w->wifi_screen);
            }
            printf("Deleting wifi scan screen");
            lv_obj_del(w->wifi_screen);
            w->wifi_screen = NULL;
            w->ap_list = NULL;
            w->is_active = false;
            printf("Wifi scan screen hidden and deleted");
        }
    }
}

void wifi_scan_show(void)
{
    wifi_widget_t *w = &g_wifi_widget;

    if (w->is_active) {
        wifi_scan_hidden();
    }

    // Create wifi scan screen
    w->wifi_screen = lv_obj_create(NULL);
    lv_obj_set_size(w->wifi_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(w->wifi_screen, lv_color_white(), 0);

    // AP list
    w->ap_list = lv_list_create(w->wifi_screen);
    lv_obj_set_size(w->ap_list, AI_PET_SCREEN_WIDTH - 20, AI_PET_SCREEN_HEIGHT - 60);
    lv_obj_align(w->ap_list, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_color(w->ap_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(w->ap_list, 2, 0);

    lv_obj_clear_flag(w->wifi_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(w->wifi_screen, LV_OBJ_FLAG_CLICKABLE);

#if LVGL_SIMULATOR
#else
    // Scan APs
    AP_IF_S *ap_info = NULL;
    uint32_t ap_info_nums = 0;
    toast_show("Scanning WiFi APs...", 2000);
    tal_wifi_all_ap_scan(&ap_info, &ap_info_nums);
    printf("Found %d wifi APs", ap_info_nums);
    for (uint32_t i = 0; i < ap_info_nums; i++) {
        char wifi_msg[256];
        snprintf(wifi_msg, sizeof(wifi_msg), "SSID: %s, RSSI: %d dB, channel: %d",
                 (const char *)ap_info[i].ssid, ap_info[i].rssi, ap_info[i].channel);
        lv_list_add_btn(w->ap_list, LV_SYMBOL_WIFI, wifi_msg);
    }
#endif
    // Title
    lv_obj_t *title = lv_label_create(w->wifi_screen);
    lv_label_set_text(title, "WiFi Scan Results");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);

    // Load screen and focus
    lv_group_t *grp = lv_group_get_default();
    if (grp) {
        lv_group_add_obj(grp, w->wifi_screen);
        lv_group_focus_obj(w->wifi_screen);
    }
    lv_screen_load(w->wifi_screen);
    w->is_active = true;
}

bool wifi_scan_is_active(void)
{
    return g_wifi_widget.is_active;
}

void wifi_scan_handle_input(uint32_t key)
{
    wifi_widget_t *w = &g_wifi_widget;
    if (!w->is_active) return;

    switch (key) {
        case KEY_ESC:
            wifi_scan_hidden();
            lv_screen_load(lv_demo_ai_pocket_pet_get_main_screen());
            break;
        case KEY_UP:
            if (w->ap_list) lv_obj_scroll_by(w->ap_list, 0, 30, LV_ANIM_ON);
            break;
        case KEY_DOWN:
            if (w->ap_list) lv_obj_scroll_by(w->ap_list, 0, -30, LV_ANIM_ON);
            break;
        case KEY_ENTER:
            // optional: selection handling
            break;
        default:
            break;
    }
}