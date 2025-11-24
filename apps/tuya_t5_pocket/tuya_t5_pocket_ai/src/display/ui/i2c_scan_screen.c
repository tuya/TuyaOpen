/**
 * @file i2c_scan_screen.c
 * @brief Implementation of the I2C scan screen for the application
 *
 * This file contains the implementation of the I2C scan screen which provides
 * I2C device scanning functionality across multiple ports with hardware integration.
 *
 * The I2C scan screen includes:
 * - I2C port switching and configuration
 * - Device address matrix display
 * - Real-time device detection with hardware integration
 * - Port navigation with visual indicators
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "i2c_scan_screen.h"
#include <stdio.h>
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/

// Use consistent screen dimensions with peripherals_scan.c
#ifndef AI_PET_SCREEN_WIDTH
#define AI_PET_SCREEN_WIDTH  384
#endif
#ifndef AI_PET_SCREEN_HEIGHT
#define AI_PET_SCREEN_HEIGHT 168
#endif

// Hardware abstraction - consistent with peripherals_scan.c
#ifdef ENABLE_LVGL_HARDWARE
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_pinmux.h"
#include "tkl_i2c.h"
#endif

// Import icon declarations
LV_IMG_DECLARE(peripherals_scan_left_icon);
LV_IMG_DECLARE(peripherals_scan_right_icon);

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_i2c_scan_screen;

// UI components
static lv_obj_t *dev_list = NULL;
static lv_obj_t *title_label = NULL;
static lv_obj_t *info_bar = NULL;
static lv_obj_t *left_icon = NULL;
static lv_obj_t *right_icon = NULL;

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

// Current port index (no state preservation)
static int current_port_index = 0;

Screen_t i2c_scan_screen = {
    .init = i2c_scan_screen_init,
    .deinit = i2c_scan_screen_deinit,
    .screen_obj = &ui_i2c_scan_screen,
    .name = "i2c_scan",
    .state_data = NULL,
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void keyboard_event_cb(lv_event_t *e);
static void create_scan_matrix(void);
static void update_port_display(void);
static void switch_to_port(int port_index);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Switch to specified I2C port
 */
static void switch_to_port(int port_index)
{
    if (port_index < 0 || port_index >= (int)(sizeof(port_info) / sizeof(port_info[0]))) {
        return;
    }

    current_port_index = port_index;

#ifdef ENABLE_LVGL_HARDWARE
    printf("[Scan] Switching to PORT: %d\n", current_port_index);
    tkl_io_pinmux_config(port_info[current_port_index].scl, current_port_index*2);
    tkl_io_pinmux_config(port_info[current_port_index].sda, current_port_index*2+1);

    TUYA_IIC_BASE_CFG_T cfg;
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    tkl_i2c_init(current_port_index, &cfg);
#endif

    // Update display
    update_port_display();
    create_scan_matrix();
}

/**
 * @brief Update port information display
 */
static void update_port_display(void)
{
    if (info_bar) {
        char port_text[32];
        snprintf(port_text, sizeof(port_text), "%s : SCL=%d, SDA=%d",
                 port_info[current_port_index].port_name,
                 port_info[current_port_index].scl,
                 port_info[current_port_index].sda);
        lv_label_set_text(info_bar, port_text);
    }

    printf("[Scan] Displaying PORT %d: SCL=%d, SDA=%d\n", current_port_index,
           port_info[current_port_index].scl, port_info[current_port_index].sda);
}

/**
 * @brief Create I2C scan matrix
 */
static void create_scan_matrix(void)
{
    // Clear existing matrix if present
    if (dev_list) {
        lv_obj_del(dev_list);
    }

    // Create matrix to display I2C addresses
    dev_list = lv_obj_create(ui_i2c_scan_screen);
    lv_obj_set_size(dev_list, AI_PET_SCREEN_WIDTH - 20, AI_PET_SCREEN_HEIGHT - 50);
    lv_obj_align(dev_list, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_border_color(dev_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(dev_list, 2, 0);
    lv_obj_set_flex_flow(dev_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(dev_list, 2, 0);
    lv_obj_clear_flag(dev_list, LV_OBJ_FLAG_SCROLLABLE);

    // Create header row (display 0 1 2 3 4 5 6 7 8 9 A B C D E F)
    lv_obj_t *header_row = lv_obj_create(dev_list);
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
        lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
    }

    // Create scrollable content container
    lv_obj_t *content_container = lv_obj_create(dev_list);
    lv_obj_set_size(content_container, LV_PCT(100), AI_PET_SCREEN_HEIGHT - 100);
    lv_obj_set_flex_flow(content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content_container, 0, 0);
    lv_obj_set_style_border_width(content_container, 0, 0);
    lv_obj_set_scroll_dir(content_container, LV_DIR_VER);
    lv_obj_set_style_pad_gap(content_container, 0, 0);

    // Create matrix of 128 addresses (0x00 - 0x7F)
    uint8_t i2c_addr = 0;
    uint8_t dev_num = 0;
    for (int row = 0; row < 8; row++) {
        lv_obj_t *row_container = lv_obj_create(content_container);
        lv_obj_set_size(row_container, LV_PCT(100), 16);
        lv_obj_set_flex_flow(row_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_gap(row_container, 1, 0);
        lv_obj_set_style_pad_all(row_container, 1, 0);

        // Add row header (0x 1x 2x ... 7x)
        lv_obj_t *row_label = lv_label_create(row_container);
        char row_text[4];
        snprintf(row_text, sizeof(row_text), "%Xx", row);
        lv_label_set_text(row_label, row_text);
        lv_obj_set_width(row_label, 30);
        lv_obj_set_style_text_font(row_label, &lv_font_montserrat_10, 0);

        // Add cells for each column
        for (int col = 0; col < 16; col++) {
            lv_obj_t *cell = lv_label_create(row_container);
            uint8_t addr = (row << 4) | col;
            char addr_text[4];

            // For valid I2C address range, test device presence
            if (addr <= 0x7F) {
#ifdef ENABLE_LVGL_HARDWARE
                i2c_addr = addr;
                uint8_t data_buf[1] = {0};

                if (OPRT_OK == tkl_i2c_master_send(current_port_index, i2c_addr, data_buf, 0, TRUE)) {
                    dev_num++;
                    if (dev_num >= i2c_addr) {
                        lv_label_set_text(cell, "");
                        continue;
                    }
                    if (i2c_addr <= 0x7F) {
                        snprintf(addr_text, sizeof(addr_text), "%02X", i2c_addr);
                        PR_DEBUG("Found I2C device at address %s", addr_text);
                        lv_label_set_text(cell, addr_text);
                    }
                    else {
                        lv_label_set_text(cell, "");
                    }
                } else {
                    // No device - display placeholder
                    lv_label_set_text(cell, "");
                    lv_obj_set_style_bg_color(cell, lv_color_white(), 0);
                    lv_obj_set_style_text_color(cell, lv_color_black(), 0);
                }
#else
                // Simulator mode - show some dummy data like peripherals_scan.c
                if (addr == 0x48 || addr == 0x50 || addr == 0x68) {
                    snprintf(addr_text, sizeof(addr_text), "%02X", addr);
                    lv_label_set_text(cell, addr_text);
                    lv_obj_set_style_bg_color(cell, lv_color_hex(0x00ff00), 0);
                    lv_obj_set_style_text_color(cell, lv_color_black(), 0);
                } else {
                    lv_label_set_text(cell, "");
                    lv_obj_set_style_bg_color(cell, lv_color_hex(0xf0f0f0), 0);
                    lv_obj_set_style_text_color(cell, lv_color_hex(0x808080), 0);
                }
#endif
            } else {
                lv_label_set_text(cell, "");
                lv_obj_set_style_bg_color(cell, lv_color_white(), 0);
                lv_obj_set_style_text_color(cell, lv_color_black(), 0);
            }

            lv_obj_set_width(cell, 16);
            lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_radius(cell, 3, 0);
            lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
            lv_obj_set_style_text_font(cell, &lv_font_montserrat_10, 0);
        }
    }
}

/**
 * @brief Keyboard event callback
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", i2c_scan_screen.name, key);

    switch (key) {
        case KEY_ESC:
            printf("I2C scan: ESC key detected, returning to main menu\n");
            screen_back();
            break;
        case KEY_UP:
            // Scroll content up - exactly like peripherals_scan.c
            if (dev_list) {
                lv_obj_t *content_container = lv_obj_get_child(dev_list, 1); // get content container
                if (content_container) {
                    // Check if already scrolled to top
                    lv_coord_t scroll_top = lv_obj_get_scroll_top(content_container);
                    if (scroll_top > 0) {
                        // Limit scroll step to remaining scrollable distance
                        lv_coord_t scroll_step = (scroll_top > 20) ? 20 : scroll_top;
                        lv_obj_scroll_by(content_container, 0, scroll_step, LV_ANIM_ON);
                        printf("I2C scan: Scrolled up by %d pixels\n", scroll_step);
                    } else {
                        printf("I2C scan: Already at top\n");
                    }
                }
            }
            break;
        case KEY_DOWN:
            // Scroll content down - exactly like peripherals_scan.c
            if (dev_list) {
                lv_obj_t *content_container = lv_obj_get_child(dev_list, 1); // get content container
                if (content_container) {
                    // Check if already scrolled to bottom
                    lv_coord_t scroll_bottom = lv_obj_get_scroll_bottom(content_container);
                    if (scroll_bottom > 0) {
                        // Limit scroll step to remaining scrollable distance
                        lv_coord_t scroll_step = (scroll_bottom > 20) ? 20 : scroll_bottom;
                        lv_obj_scroll_by(content_container, 0, -scroll_step, LV_ANIM_ON);
                        printf("I2C scan: Scrolled down by %d pixels\n", scroll_step);
                    } else {
                        printf("I2C scan: Already at bottom\n");
                    }
                }
            }
            break;
        case KEY_LEFT:
            // Switch to previous PORT
            if (current_port_index > 0) {
                switch_to_port(current_port_index - 1);
            }
            break;
        case KEY_RIGHT:
            // Switch to next PORT
            if (current_port_index < (int)(sizeof(port_info) / sizeof(port_info[0]) - 1)) {
                switch_to_port(current_port_index + 1);
            }
            break;
        case KEY_ENTER:
            // Refresh scan
            create_scan_matrix();
            break;
        default:
            break;
    }
}

/**
 * @brief Show I2C scan for specific port
 */
void i2c_scan_screen_show_port(uint8_t port)
{
#ifdef ENABLE_LVGL_HARDWARE
    if (port >= TUYA_I2C_NUM_MAX) {
        printf("[ERROR] Invalid I2C port number: %d\n", port);
        return;
    }
#endif

    current_port_index = port;
    screen_load(&i2c_scan_screen);
}

/**
 * @brief Initialize the I2C scan screen
 */
void i2c_scan_screen_init(void)
{
    ui_i2c_scan_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_i2c_scan_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_i2c_scan_screen, lv_color_white(), 0);

    // Create title
    title_label = lv_label_create(ui_i2c_scan_screen);
    lv_label_set_text(title_label, "I2C Device Scan Results");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);

    // Create PORT info row
    // Left icon
    left_icon = lv_img_create(ui_i2c_scan_screen);
    lv_img_set_src(left_icon, &peripherals_scan_left_icon);
    lv_obj_align(left_icon, LV_ALIGN_TOP_MID, -85, 25);
    lv_img_set_zoom(left_icon, 200);

    // Port info bar
    info_bar = lv_label_create(ui_i2c_scan_screen);
    lv_obj_align(info_bar, LV_ALIGN_TOP_MID, 0, 29);
    lv_obj_set_style_text_font(info_bar, &lv_font_montserrat_12, 0);

    // Right icon
    right_icon = lv_img_create(ui_i2c_scan_screen);
    lv_img_set_src(right_icon, &peripherals_scan_right_icon);
    lv_obj_align(right_icon, LV_ALIGN_TOP_MID, 85, 25);
    lv_img_set_zoom(right_icon, 200);

    // Initialize I2C port - always start from PORT 0
    current_port_index = 0;
    switch_to_port(current_port_index);

    // Event handling
    lv_obj_add_event_cb(ui_i2c_scan_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_i2c_scan_screen);
    lv_group_focus_obj(ui_i2c_scan_screen);
}

/**
 * @brief Deinitialize the I2C scan screen
 */
void i2c_scan_screen_deinit(void)
{
    if (ui_i2c_scan_screen) {
        printf("deinit I2C scan screen\n");
        lv_obj_remove_event_cb(ui_i2c_scan_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_i2c_scan_screen);

#ifdef ENABLE_LVGL_HARDWARE
        tkl_i2c_deinit(TUYA_I2C_NUM_1);
        tkl_i2c_deinit(TUYA_I2C_NUM_2);
#endif
    }

    // Reset pointers
    dev_list = NULL;
    title_label = NULL;
    info_bar = NULL;
    left_icon = NULL;
    right_icon = NULL;
}
