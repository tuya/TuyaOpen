/**
 * @file wifi_scan_screen.c
 * @brief Implementation of the WiFi scan screen for the application
 *
 * This file contains the implementation of the WiFi scan screen which provides
 * WiFi access point scanning functionality with hardware integration.
 *
 * The WiFi scan screen includes:
 * - WiFi access point scanning and detection
 * - AP list display with signal strength and security
 * - Real-time scanning with hardware WiFi module
 * - Navigation and selection capabilities
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "wifi_scan_screen.h"
#include <stdio.h>
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/

// Screen dimensions
#define SCREEN_WIDTH  384
#define SCREEN_HEIGHT 168

// Hardware abstraction
#ifdef ENABLE_LVGL_HARDWARE
#include "tuya_cloud_types.h"
#include "tal_wifi.h"
#endif

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_wifi_scan_screen;

// UI components
static lv_obj_t *ap_list = NULL;
static lv_obj_t *title_label = NULL;
static lv_obj_t *status_label = NULL;

// WiFi state
typedef struct {
    uint8_t is_scanning;
    uint8_t scan_complete;
    uint32_t ap_count;
} wifi_state_t;

static wifi_state_t g_wifi_state;

Screen_t wifi_scan_screen = {
    .init = wifi_scan_screen_init,
    .deinit = wifi_scan_screen_deinit,
    .screen_obj = &ui_wifi_scan_screen,
    .name = "wifi_scan",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void keyboard_event_cb(lv_event_t *e);
static void create_ap_list(void);
static void start_wifi_scan(void);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Start WiFi scan
 */
static void start_wifi_scan(void)
{
    if (g_wifi_state.is_scanning) {
        return;
    }

    g_wifi_state.is_scanning = 1;
    g_wifi_state.scan_complete = 0;
    g_wifi_state.ap_count = 0;

    // Update status
    if (status_label) {
        lv_label_set_text(status_label, "Scanning WiFi APs...");
    }

    // Clear existing list
    if (ap_list) {
        lv_obj_clean(ap_list);
    }

#ifdef ENABLE_LVGL_HARDWARE
    // Scan APs using hardware
    AP_IF_S *ap_info = NULL;
    uint32_t ap_info_nums = 0;

    int result = tal_wifi_all_ap_scan(&ap_info, &ap_info_nums);

    if (result == OPRT_OK && ap_info) {
        g_wifi_state.ap_count = ap_info_nums;
        printf("Found %d WiFi APs\n", ap_info_nums);

        // Add APs to list
        for (uint32_t i = 0; i < ap_info_nums; i++) {
            char ap_text[128];
            snprintf(ap_text, sizeof(ap_text), "%s (RSSI: %d, Security: %s)",
                     ap_info[i].ssid,
                     ap_info[i].rssi,
                     (ap_info[i].security == 0) ? "Open" : "Secured");

            lv_obj_t *ap_item = lv_list_add_btn(ap_list, NULL, ap_text);
            lv_obj_set_style_text_font(ap_item, &lv_font_montserrat_12, 0);
        }

        // Update status
        char status_text[64];
        snprintf(status_text, sizeof(status_text), "Found %d APs", ap_info_nums);
        lv_label_set_text(status_label, status_text);
    } else {
        printf("WiFi scan failed or no APs found\n");
        lv_label_set_text(status_label, "Scan failed or no APs found");
    }
#else
    // Simulator mode - add dummy APs
    const char *dummy_aps[] = {
        "HomeWiFi (RSSI: -45, Security: Secured)",
        "Office_Network (RSSI: -52, Security: Secured)",
        "Guest_WiFi (RSSI: -68, Security: Open)",
        "Mobile_Hotspot (RSSI: -71, Security: Secured)",
        "Public_WiFi (RSSI: -78, Security: Open)"
    };

    int num_dummy_aps = sizeof(dummy_aps) / sizeof(dummy_aps[0]);
    g_wifi_state.ap_count = num_dummy_aps;

    for (int i = 0; i < num_dummy_aps; i++) {
        lv_obj_t *ap_item = lv_list_add_btn(ap_list, NULL, dummy_aps[i]);
        lv_obj_set_style_text_font(ap_item, &lv_font_montserrat_12, 0);
    }

    char status_text[64];
    snprintf(status_text, sizeof(status_text), "Found %d APs (Simulated)", num_dummy_aps);
    lv_label_set_text(status_label, status_text);
#endif

    g_wifi_state.is_scanning = 0;
    g_wifi_state.scan_complete = 1;
}

/**
 * @brief Create AP list container
 */
static void create_ap_list(void)
{
    // AP list
    ap_list = lv_list_create(ui_wifi_scan_screen);
    lv_obj_set_size(ap_list, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 80);
    lv_obj_align(ap_list, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_border_color(ap_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(ap_list, 2, 0);
    lv_obj_set_style_bg_color(ap_list, lv_color_white(), 0);
}

/**
 * @brief Keyboard event callback
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", wifi_scan_screen.name, key);

    switch (key) {
        case KEY_ESC:
            printf("WiFi scan: ESC key detected, returning to main menu\n");
            screen_back();
            break;
        case KEY_ENTER:
            // Refresh scan
            start_wifi_scan();
            break;
        case KEY_UP:
            // Navigate list up (handled by LVGL automatically if list is focused)
            break;
        case KEY_DOWN:
            // Navigate list down (handled by LVGL automatically if list is focused)
            break;
        default:
            break;
    }
}

/**
 * @brief Initialize the WiFi scan screen
 */
void wifi_scan_screen_init(void)
{
    ui_wifi_scan_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_wifi_scan_screen, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_wifi_scan_screen, lv_color_white(), 0);

    // Initialize WiFi state
    memset(&g_wifi_state, 0, sizeof(wifi_state_t));

    // Create title
    title_label = lv_label_create(ui_wifi_scan_screen);
    lv_label_set_text(title_label, "WiFi Scan Results");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);

    // Create status label
    status_label = lv_label_create(ui_wifi_scan_screen);
    lv_label_set_text(status_label, "Press ENTER to scan");
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(status_label, lv_color_make(100, 100, 100), 0);

    // Create AP list
    create_ap_list();

    // Event handling
    lv_obj_add_event_cb(ui_wifi_scan_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_wifi_scan_screen);
    lv_group_focus_obj(ui_wifi_scan_screen);

    // Start initial scan
    start_wifi_scan();
}

/**
 * @brief Deinitialize the WiFi scan screen
 */
void wifi_scan_screen_deinit(void)
{
    if (ui_wifi_scan_screen) {
        printf("deinit WiFi scan screen\n");
        lv_obj_remove_event_cb(ui_wifi_scan_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_wifi_scan_screen);
    }

    // Reset pointers
    ap_list = NULL;
    title_label = NULL;
    status_label = NULL;

    // Reset state
    memset(&g_wifi_state, 0, sizeof(wifi_state_t));
}
