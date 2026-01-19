/**
 * @file rfid_scan_screen.c
 * @brief Implementation of the RFID scan screen for the application
 *
 * This file contains the implementation of the RFID scan screen which provides
 * RFID tag scanning and information display functionality with a clean monochrome UI.
 *
 * The RFID scan screen includes:
 * - Compact card-style information display
 * - Device ID (1 byte), Tag Type (2 bytes), and UID (4-16 bytes) visualization
 * - Black and white monochrome design
 * - All information visible simultaneously on screen
 * - Keyboard navigation support
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "rfid_scan_screen.h"
#include <stdio.h>
#include <string.h>

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

// Color scheme - Black and White only
#define COLOR_PRIMARY        lv_color_black()
#define COLOR_SECONDARY      lv_color_black()
#define COLOR_ACCENT         lv_color_black()
#define COLOR_BACKGROUND     lv_color_white()
#define COLOR_CARD           lv_color_white()
#define COLOR_TEXT_PRIMARY   lv_color_black()
#define COLOR_TEXT_SECONDARY lv_color_black()
#define COLOR_SUCCESS        lv_color_black()
#define COLOR_WARNING        lv_color_black()

// Font definitions - easily customizable
#define SCREEN_TITLE_FONT   &lv_font_terminusTTF_Bold_18
#define SCREEN_CONTENT_FONT &lv_font_terminusTTF_Bold_16
#define SCREEN_INFO_FONT    &lv_font_terminusTTF_Bold_14

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_rfid_scan_screen;

// UI components
static lv_obj_t *title_label = NULL;
static lv_obj_t *info_card = NULL;
static lv_obj_t *dev_id_label = NULL;
static lv_obj_t *dev_id_value = NULL;
static lv_obj_t *tag_type_label = NULL;
static lv_obj_t *tag_type_value = NULL;
static lv_obj_t *uid_label = NULL;
static lv_obj_t *uid_value = NULL;
static lv_obj_t *hint_label = NULL;

// Current tag information
static rfid_tag_info_t current_tag = {0};

// Pending tag information (updated by callback, used by timer)
static rfid_tag_info_t pending_tag = {0};
static bool pending_update = false;
static uint16_t back_time = 0;

// UI refresh timer
static lv_timer_t *ui_refresh_timer = NULL;

Screen_t rfid_scan_screen = {
    .init = rfid_scan_screen_init,
    .deinit = rfid_scan_screen_deinit,
    .screen_obj = &ui_rfid_scan_screen,
    .name = "rfid_scan",
    .state_data = NULL,
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void keyboard_event_cb(lv_event_t *e);
static void create_ui_components(void);
static void update_display(void);
static void simulate_tag_scan(void);
static void ui_refresh_timer_cb(lv_timer_t *timer);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Timer callback to refresh UI with pending data
 */
static void ui_refresh_timer_cb(lv_timer_t *timer)
{
    if (pending_update) {
        // Copy pending data to current tag
        memcpy(&current_tag, &pending_tag, sizeof(rfid_tag_info_t));
        pending_update = false;

        // Update display
        update_display();

        // Reset back_time when new data received
        back_time = 0;

        printf("[RFID Timer] UI refreshed - DevID=0x%02X, Type=0x%04X, UID_len=%d\n", current_tag.dev_id,
               current_tag.tag_type, current_tag.uid_length);
    }

    // Auto return to previous screen after 15 seconds of no new data
    if (current_tag.is_valid) {
        if (++back_time >= 15 * 10) {
            back_time = 0;
            screen_back();
        }
    }
}

/**
 * @brief Update display with current tag information
 */
static void update_display(void)
{
    if (!current_tag.is_valid) {
        // No tag detected - show waiting state
        if (dev_id_value)
            lv_label_set_text(dev_id_value, "--");
        if (tag_type_value)
            lv_label_set_text(tag_type_value, "----");
        if (uid_value)
            lv_label_set_text(uid_value, "No tag detected");

        if (hint_label) {
            lv_label_set_text(hint_label, "Place tag near reader");
            lv_obj_set_style_text_color(hint_label, COLOR_TEXT_SECONDARY, 0);
        }
        return;
    }

    // Tag detected - display information
    char buffer[128];

    // Device ID
    if (dev_id_value) {
        snprintf(buffer, sizeof(buffer), "0x%02X", current_tag.dev_id);
        lv_label_set_text(dev_id_value, buffer);
    }

    // Tag Type
    if (tag_type_value) {
        // Add tag type description based on RFID_TAG_TYPE_E enum
        const char *type_name = "Unknown";
        switch (current_tag.tag_type) {
        case RFID_TAG_TYPE_MIFARE_CLASSIC_1K:
            type_name = "Mifare Classic 1K";
            break;
        case RFID_TAG_TYPE_MIFARE_CLASSIC_4K:
            type_name = "Mifare Classic 4K";
            break;
        case RFID_TAG_TYPE_MIFARE_ULTRALIGHT:
            type_name = "Mifare Ultralight";
            break;
        case RFID_TAG_TYPE_TYPE_B:
            type_name = "Type B (CN ID)";
            break;
        case RFID_TAG_TYPE_15693:
            type_name = "ISO15693";
            break;
        case RFID_TAG_TYPE_UNKNOWN:
        default:
            type_name = "Unknown";
            break;
        }
        snprintf(buffer, sizeof(buffer), "0x%04X (%s)", current_tag.tag_type, type_name);
        lv_label_set_text(tag_type_value, buffer);
    }

    // UID
    if (uid_value && current_tag.uid_length > 0) {
        buffer[0] = '\0';
        int offset = 0;
        for (uint8_t i = 0; i < current_tag.uid_length && i < 16; i++) {
            int ret = snprintf(buffer + offset, sizeof(buffer) - offset, "%02X", current_tag.uid[i]);
            if (ret < 0 || ret >= (int)(sizeof(buffer) - offset)) {
                break; // Buffer full, stop
            }
            offset += ret;

            if (i < current_tag.uid_length - 1 && i < 15) {
                ret = snprintf(buffer + offset, sizeof(buffer) - offset, ":");
                if (ret < 0 || ret >= (int)(sizeof(buffer) - offset)) {
                    break; // Buffer full, stop
                }
                offset += ret;
            }
        }
        lv_label_set_text(uid_value, buffer);
    }

    // Update hint
    if (hint_label) {
        lv_label_set_text(hint_label, "[OK] Tag detected");
        lv_obj_set_style_text_color(hint_label, COLOR_SUCCESS, 0);
    }
}

/**
 * @brief Simulate RFID tag scan (for testing/demo purposes)
 */
static void simulate_tag_scan(void)
{
#ifndef ENABLE_LVGL_HARDWARE
    // Simulation mode - use fake data for testing
    // Create simulated tag data
    static uint8_t scan_count = 0;
    scan_count++;

    // Alternate between different tag types for demonstration
    if (scan_count % 3 == 1) {
        pending_tag.dev_id = 0x01;
        pending_tag.tag_type = RFID_TAG_TYPE_MIFARE_CLASSIC_1K;
        pending_tag.uid_length = RFID_SCAN_LENGTH_4_BYTES;
        pending_tag.uid[0] = 0xA1;
        pending_tag.uid[1] = 0xB2;
        pending_tag.uid[2] = 0xC3;
        pending_tag.uid[3] = 0xD4;
        pending_tag.is_valid = true;
    } else if (scan_count % 3 == 2) {
        pending_tag.dev_id = 0x02;
        pending_tag.tag_type = RFID_TAG_TYPE_MIFARE_ULTRALIGHT;
        pending_tag.uid_length = RFID_SCAN_LENGTH_7_BYTES;
        pending_tag.uid[0] = 0x04;
        pending_tag.uid[1] = 0x5E;
        pending_tag.uid[2] = 0x7A;
        pending_tag.uid[3] = 0x3B;
        pending_tag.uid[4] = 0x2F;
        pending_tag.uid[5] = 0x4C;
        pending_tag.uid[6] = 0x80;
        pending_tag.is_valid = true;
    } else {
        pending_tag.dev_id = 0x03;
        pending_tag.tag_type = RFID_TAG_TYPE_MIFARE_CLASSIC_4K;
        pending_tag.uid_length = RFID_SCAN_LENGTH_4_BYTES;
        pending_tag.uid[0] = 0x11;
        pending_tag.uid[1] = 0x22;
        pending_tag.uid[2] = 0x33;
        pending_tag.uid[3] = 0x44;
        pending_tag.is_valid = true;
    }

    // Set flag to trigger UI update
    pending_update = true;

    printf("[RFID Simulate] Tag scan simulated: DevID=0x%02X, Type=0x%04X, UID_len=%d\n", pending_tag.dev_id,
           pending_tag.tag_type, pending_tag.uid_length);
#else
    // Hardware mode - data comes from actual RFID reader via callback
    printf("[RFID] Simulation disabled in hardware mode - waiting for real RFID data\n");
#endif
}

/**
 * @brief Create UI components for RFID scan screen
 */
static void create_ui_components(void)
{
    // Create title
    title_label = lv_label_create(ui_rfid_scan_screen);
    lv_label_set_text(title_label, "RFID Scanner");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(title_label, SCREEN_TITLE_FONT, 0);
    lv_obj_set_style_text_color(title_label, COLOR_PRIMARY, 0);

    // Create info card container - compact layout
    info_card = lv_obj_create(ui_rfid_scan_screen);
    lv_obj_set_size(info_card, AI_PET_SCREEN_WIDTH - 20, AI_PET_SCREEN_HEIGHT - 45);
    lv_obj_align(info_card, LV_ALIGN_CENTER, 0, 5);
    lv_obj_set_style_bg_color(info_card, COLOR_CARD, 0);
    lv_obj_set_style_border_color(info_card, lv_color_black(), 0);
    lv_obj_set_style_border_width(info_card, 2, 0);
    lv_obj_set_style_radius(info_card, 0, 0);
    lv_obj_set_style_pad_all(info_card, 8, 0);
    lv_obj_clear_flag(info_card, LV_OBJ_FLAG_SCROLLABLE);

    int y_offset = 5;

    // Device ID section - single line with larger font
    lv_obj_t *dev_id_container = lv_obj_create(info_card);
    lv_obj_set_size(dev_id_container, AI_PET_SCREEN_WIDTH - 36, 30);
    lv_obj_align(dev_id_container, LV_ALIGN_TOP_LEFT, 0, y_offset);
    lv_obj_set_style_bg_color(dev_id_container, COLOR_CARD, 0);
    lv_obj_set_style_border_width(dev_id_container, 0, 0);
    lv_obj_set_style_pad_all(dev_id_container, 0, 0);
    lv_obj_clear_flag(dev_id_container, LV_OBJ_FLAG_SCROLLABLE);

    dev_id_label = lv_label_create(dev_id_container);
    lv_label_set_text(dev_id_label, "Device:");
    lv_obj_align(dev_id_label, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_font(dev_id_label, SCREEN_TITLE_FONT, 0);
    lv_obj_set_style_text_color(dev_id_label, COLOR_TEXT_PRIMARY, 0);

    dev_id_value = lv_label_create(dev_id_container);
    lv_label_set_text(dev_id_value, "--");
    lv_obj_align(dev_id_value, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_font(dev_id_value, SCREEN_TITLE_FONT, 0);
    lv_obj_set_style_text_color(dev_id_value, COLOR_TEXT_PRIMARY, 0);

    y_offset += 33;

    // Separator line
    lv_obj_t *separator1 = lv_obj_create(info_card);
    lv_obj_set_size(separator1, AI_PET_SCREEN_WIDTH - 36, 1);
    lv_obj_align(separator1, LV_ALIGN_TOP_LEFT, 0, y_offset);
    lv_obj_set_style_bg_color(separator1, lv_color_black(), 0);
    lv_obj_set_style_border_width(separator1, 0, 0);

    y_offset += 5;

    // Tag Type section - single line with larger font
    lv_obj_t *tag_type_container = lv_obj_create(info_card);
    lv_obj_set_size(tag_type_container, AI_PET_SCREEN_WIDTH - 36, 30);
    lv_obj_align(tag_type_container, LV_ALIGN_TOP_LEFT, 0, y_offset);
    lv_obj_set_style_bg_color(tag_type_container, COLOR_CARD, 0);
    lv_obj_set_style_border_width(tag_type_container, 0, 0);
    lv_obj_set_style_pad_all(tag_type_container, 0, 0);
    lv_obj_clear_flag(tag_type_container, LV_OBJ_FLAG_SCROLLABLE);

    tag_type_label = lv_label_create(tag_type_container);
    lv_label_set_text(tag_type_label, "Type:");
    lv_obj_align(tag_type_label, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_font(tag_type_label, SCREEN_CONTENT_FONT, 0);
    lv_obj_set_style_text_color(tag_type_label, COLOR_TEXT_PRIMARY, 0);

    tag_type_value = lv_label_create(tag_type_container);
    lv_label_set_text(tag_type_value, "----");
    lv_obj_align(tag_type_value, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_font(tag_type_value, SCREEN_CONTENT_FONT, 0);
    lv_obj_set_style_text_color(tag_type_value, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_long_mode(tag_type_value, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(tag_type_value, AI_PET_SCREEN_WIDTH - 100);

    y_offset += 33;

    // Separator line
    lv_obj_t *separator2 = lv_obj_create(info_card);
    lv_obj_set_size(separator2, AI_PET_SCREEN_WIDTH - 36, 1);
    lv_obj_align(separator2, LV_ALIGN_TOP_LEFT, 0, y_offset);
    lv_obj_set_style_bg_color(separator2, lv_color_black(), 0);
    lv_obj_set_style_border_width(separator2, 0, 0);

    y_offset += 5;

    // UID section - single line with scroll support for long UIDs
    lv_obj_t *uid_container = lv_obj_create(info_card);
    lv_obj_set_size(uid_container, AI_PET_SCREEN_WIDTH - 36, 30);
    lv_obj_align(uid_container, LV_ALIGN_TOP_LEFT, 0, y_offset);
    lv_obj_set_style_bg_color(uid_container, COLOR_CARD, 0);
    lv_obj_set_style_border_width(uid_container, 0, 0);
    lv_obj_set_style_pad_all(uid_container, 0, 0);
    lv_obj_clear_flag(uid_container, LV_OBJ_FLAG_SCROLLABLE);

    uid_label = lv_label_create(uid_container);
    lv_label_set_text(uid_label, "UID:");
    lv_obj_align(uid_label, LV_ALIGN_TOP_LEFT, 10, 8);
    lv_obj_set_style_text_font(uid_label, SCREEN_CONTENT_FONT, 0);
    lv_obj_set_style_text_color(uid_label, COLOR_TEXT_PRIMARY, 0);

    uid_value = lv_label_create(uid_container);
    lv_label_set_text(uid_value, "No tag");
    lv_obj_align(uid_value, LV_ALIGN_LEFT_MID, 50, 0);
    lv_obj_set_style_text_font(uid_value, SCREEN_CONTENT_FONT, 0);
    lv_obj_set_style_text_color(uid_value, COLOR_TEXT_PRIMARY, 0);
    lv_label_set_long_mode(uid_value, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(uid_value, AI_PET_SCREEN_WIDTH - 86);

    // Hint text at bottom
    hint_label = lv_label_create(ui_rfid_scan_screen);
    lv_label_set_text(hint_label, "Place tag near reader");
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -3);
    lv_obj_set_style_text_font(hint_label, SCREEN_INFO_FONT, 0);
    lv_obj_set_style_text_color(hint_label, COLOR_TEXT_SECONDARY, 0);
}

/**
 * @brief Keyboard event callback
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", rfid_scan_screen.name, key);

    switch (key) {
    case KEY_ESC:
        printf("RFID scan: ESC key detected, returning to previous screen\n");
        screen_back();
        break;

    case KEY_ENTER:
        printf("RFID scan: ENTER key - simulating tag scan\n");
        simulate_tag_scan();
        break;

    case KEY_DOWN:
    case KEY_RIGHT:
        printf("RFID scan: Simulating new tag scan\n");
        simulate_tag_scan();
        break;

    case KEY_UP:
    case KEY_LEFT:
        printf("RFID scan: Clearing tag information\n");
        memset(&current_tag, 0, sizeof(rfid_tag_info_t));
        break;

    default:
        break;
    }
}

/**
 * @brief Callback function to update RFID tag information from rfid_scan module
 * This function is called by rfid_scan.c when new tag data is received
 */
void rfid_scan_screen_data_update(uint8_t dev_id, uint16_t tag_type, const uint8_t *uid, uint8_t uid_length)
{
    if (!uid || uid_length == 0 || uid_length > 16) {
        printf("[RFID Screen] Invalid callback parameters: uid=%p, uid_length=%d\n", (void *)uid, uid_length);
        return;
    }

    // Clear old pending data first
    memset(&pending_tag, 0, sizeof(rfid_tag_info_t));

    // Update pending tag information (will be applied by timer)
    pending_tag.dev_id = dev_id;
    pending_tag.tag_type = tag_type;
    pending_tag.uid_length = uid_length;
    memcpy(pending_tag.uid, uid, uid_length);
    pending_tag.is_valid = true;

    // Set flag to trigger UI update in timer
    pending_update = true;

#ifdef ENABLE_LVGL_HARDWARE
    printf("[RFID Screen HW] Data received - DevID=0x%02X, Type=0x%04X, UID_len=%d (pending UI update)\n", dev_id,
           tag_type, uid_length);
#else
    printf("[RFID Screen SIM] Data received - DevID=0x%02X, Type=0x%04X, UID_len=%d (pending UI update)\n", dev_id,
           tag_type, uid_length);
#endif
}

/**
 * @brief Initialize the RFID scan screen
 */
void rfid_scan_screen_init(void)
{
    ui_rfid_scan_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_rfid_scan_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_rfid_scan_screen, COLOR_BACKGROUND, 0);

    // Create UI components
    create_ui_components();

    // Check if there's pending data before init (data received before screen was created)
    if (pending_update) {
        // Copy pending data to current tag immediately
        memcpy(&current_tag, &pending_tag, sizeof(rfid_tag_info_t));
        pending_update = false;
        printf("[RFID] Applied pending data during init - DevID=0x%02X, Type=0x%04X\n", current_tag.dev_id,
               current_tag.tag_type);
    }

    // Display current state
    update_display();

    // Create timer for UI refresh (100ms interval)
    ui_refresh_timer = lv_timer_create(ui_refresh_timer_cb, 100, NULL);

    // Event handling
    lv_obj_add_event_cb(ui_rfid_scan_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_rfid_scan_screen);
    lv_group_focus_obj(ui_rfid_scan_screen);

#ifdef ENABLE_LVGL_HARDWARE
    printf("[RFID] Screen initialized with timer refresh (Hardware mode)\n");
#else
    printf("[RFID] Screen initialized with timer refresh (Simulation mode)\n");
#endif
}

/**
 * @brief Deinitialize the RFID scan screen
 */
void rfid_scan_screen_deinit(void)
{
    if (ui_rfid_scan_screen) {
        printf("deinit RFID scan screen\n");
        lv_obj_remove_event_cb(ui_rfid_scan_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_rfid_scan_screen);
    }

    // Delete timer
    if (ui_refresh_timer) {
        lv_timer_del(ui_refresh_timer);
        ui_refresh_timer = NULL;
    }

    // Reset pointers
    back_time = 0;
    title_label = NULL;
    info_card = NULL;
    dev_id_label = NULL;
    dev_id_value = NULL;
    tag_type_label = NULL;
    tag_type_value = NULL;
    uid_label = NULL;
    uid_value = NULL;
    hint_label = NULL;
}

void rfid_scan_screen_load(void)
{
    if (screen_get_now_screen() != &rfid_scan_screen) {
        screen_load(&rfid_scan_screen);
    }
}