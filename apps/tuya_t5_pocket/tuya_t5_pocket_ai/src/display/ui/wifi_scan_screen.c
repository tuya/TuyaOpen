/**
 * @file wifi_scan_screen.c
 * @brief Implementation of the WiFi scan screen for the application
 *
 * This file contains the implementation of the WiFi scan screen which provides
 * WiFi access point scanning functionality with hardware integration.
 * Implementation matches peripherals_scan.c exactly for consistency.
 *
 * The WiFi scan screen includes:
 * - WiFi access point scanning and detection
 * - AP list display with signal strength and channel information
 * - Real-time scanning with hardware WiFi module
 * - Navigation and scrolling capabilities
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "wifi_scan_screen.h"
#include <stdio.h>
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/

// Use same constants as peripherals_scan.c
#ifndef AI_PET_SCREEN_WIDTH
#define AI_PET_SCREEN_WIDTH  384
#endif
#ifndef AI_PET_SCREEN_HEIGHT
#define AI_PET_SCREEN_HEIGHT 168
#endif

// Hardware abstraction - consistent with peripherals_scan.c
#ifdef ENABLE_LVGL_HARDWARE
#include "tuya_cloud_types.h"
#include "tal_wifi.h"
#endif

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_wifi_scan_screen;

// UI components - match peripherals_scan.c structure
static lv_obj_t *ap_list = NULL;
static lv_obj_t *title_label = NULL;

// WiFi state - match peripherals_scan.c widget structure
typedef struct {
    lv_obj_t *wifi_screen;
    lv_obj_t *ap_list;
    bool is_active;
} wifi_widget_t;

static wifi_widget_t g_wifi_widget;

// State preservation structure for wifi scan screen
typedef struct {
    uint8_t dummy; // No specific state to preserve for now
} wifi_scan_screen_state_t;

static wifi_scan_screen_state_t wifi_scan_screen_state = {
    .dummy = 0
};

Screen_t wifi_scan_screen = {
    .init = wifi_scan_screen_init,
    .deinit = wifi_scan_screen_deinit,
    .screen_obj = &ui_wifi_scan_screen,
    .name = "wifi_scan",
    .state_data = &wifi_scan_screen_state,
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void keyboard_event_cb(lv_event_t *e);
static void create_wifi_scan_ui(void);
static void reset_scroll_position_cb(lv_timer_t *timer);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Create WiFi scan UI following peripherals_scan.c exactly
 */
static void create_wifi_scan_ui(void)
{
    wifi_widget_t *w = &g_wifi_widget;

    if (w->is_active) {
        // Clean up existing screen
        if (w->wifi_screen) {
            lv_group_t * g = lv_group_get_default();
            if (g) {
                lv_group_remove_obj(w->wifi_screen);
            }
            lv_obj_del(w->wifi_screen);
            w->wifi_screen = NULL;
            w->ap_list = NULL;
            w->is_active = false;
        }
    }

    // Create wifi scan screen exactly like peripherals_scan.c
    w->wifi_screen = lv_obj_create(NULL);
    lv_obj_set_size(w->wifi_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(w->wifi_screen, lv_color_white(), 0);

    // AP list exactly like peripherals_scan.c
    w->ap_list = lv_list_create(w->wifi_screen);
    lv_obj_set_size(w->ap_list, AI_PET_SCREEN_WIDTH - 20, AI_PET_SCREEN_HEIGHT - 60);
    lv_obj_align(w->ap_list, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_color(w->ap_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(w->ap_list, 2, 0);
    
    // Initialize scroll position to top immediately after creation
    lv_obj_scroll_to_y(w->ap_list, 0, LV_ANIM_OFF);

    lv_obj_clear_flag(w->wifi_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(w->wifi_screen, LV_OBJ_FLAG_CLICKABLE);

#ifdef ENABLE_LVGL_HARDWARE
    // Scan APs using hardware - exactly like peripherals_scan.c
    AP_IF_S *ap_info = NULL;
    uint32_t ap_info_nums = 0;
    int result = tal_wifi_all_ap_scan(&ap_info, &ap_info_nums);
    printf("Found %d wifi APs", ap_info_nums);

    if (result == OPRT_OK && ap_info) {
        for (uint32_t i = 0; i < ap_info_nums; i++) {
            char wifi_msg[256];
            snprintf(wifi_msg, sizeof(wifi_msg), "SSID: %s, RSSI: %d dB, channel: %d",
                     (const char *)ap_info[i].ssid, ap_info[i].rssi, ap_info[i].channel);
            lv_list_add_btn(w->ap_list, LV_SYMBOL_WIFI, wifi_msg);
        }
        // Reset scroll position to top after adding items
        lv_obj_scroll_to_y(w->ap_list, 0, LV_ANIM_OFF);
    }
#else
    // Simulator mode - add example data like peripherals_scan.c would  
    const char *example_aps[] = {
        "SSID: HomeWiFi, RSSI: -45 dB, channel: 6",
        "SSID: Office_Network, RSSI: -52 dB, channel: 11", 
        "SSID: Guest_WiFi, RSSI: -68 dB, channel: 1",
        "SSID: Mobile_Hotspot, RSSI: -71 dB, channel: 9",
        "SSID: Public_WiFi, RSSI: -78 dB, channel: 3",
        "SSID: Neighbor_WiFi, RSSI: -82 dB, channel: 6",
        "SSID: CoffeeShop_Free, RSSI: -75 dB, channel: 11",
        "SSID: Hotel_Lobby, RSSI: -69 dB, channel: 1",
        "SSID: Company_Guest, RSSI: -55 dB, channel: 9",
        "SSID: Library_Public, RSSI: -88 dB, channel: 3"
    };

    for (int i = 0; i < 10; i++) {
        lv_list_add_btn(w->ap_list, LV_SYMBOL_WIFI, example_aps[i]);
    }
    
    // Reset scroll position to top after adding items
    lv_obj_scroll_to_y(w->ap_list, 0, LV_ANIM_OFF);
#endif

    // Title - place after AP list creation like peripherals_scan.c
    lv_obj_t *title = lv_label_create(w->wifi_screen);
    lv_label_set_text(title, "WiFi Scan Results");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);

    // Load screen and focus exactly like peripherals_scan.c
    lv_group_t *grp = lv_group_get_default();
    if (grp) {
        lv_group_add_obj(grp, w->wifi_screen);
        lv_group_focus_obj(w->wifi_screen);
    }
    lv_screen_load(w->wifi_screen);
    w->is_active = true;

    // Force scroll to top after screen is loaded and active
    if (w->ap_list) {
        lv_obj_scroll_to_y(w->ap_list, 0, LV_ANIM_OFF);
        // Also clear any scroll animation that might be running
        lv_obj_clear_flag(w->ap_list, LV_OBJ_FLAG_SCROLL_MOMENTUM);
        lv_obj_add_flag(w->ap_list, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    }

    // Create a one-shot timer to ensure scroll position is reset after UI is fully rendered
    lv_timer_create(reset_scroll_position_cb, 50, NULL);

        // Update global references for screen manager
    ui_wifi_scan_screen = w->wifi_screen;
    ap_list = w->ap_list;
    title_label = title;
}

/**
 * @brief Timer callback to reset scroll position after UI is fully rendered
 */
static void reset_scroll_position_cb(lv_timer_t *timer)
{
    wifi_widget_t *w = &g_wifi_widget;
    
    if (w->is_active && w->ap_list) {
        printf("Timer: Resetting WiFi list scroll position to top\n");
        lv_obj_scroll_to_y(w->ap_list, 0, LV_ANIM_OFF);
    }
    
    // Delete the timer after use
    lv_timer_del(timer);
}

/**
 * @brief Initialize the WiFi scan screen
 */

/**
 * @brief Keyboard event callback - match peripherals_scan.c key handling exactly
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", wifi_scan_screen.name, key);

    wifi_widget_t *w = &g_wifi_widget;
    if (!w->is_active) return;

    switch (key) {
        case KEY_ESC:
            printf("WiFi scan: ESC key detected, returning to main menu\n");
            screen_back();
            break;
        case KEY_UP:
            // Scroll content up - exactly like peripherals_scan.c
            if (ap_list) {
                // Check if already scrolled to top
                lv_coord_t scroll_top = lv_obj_get_scroll_top(ap_list);
                if (scroll_top > 0) {
                    // Limit scroll step to remaining scrollable distance
                    lv_coord_t scroll_step = (scroll_top > 30) ? 30 : scroll_top;
                    lv_obj_scroll_by(ap_list, 0, scroll_step, LV_ANIM_ON);
                    printf("WiFi scan: Scrolled up by %d pixels\n", scroll_step);
                } else {
                    printf("WiFi scan: Already at top\n");
                }
            }
            break;
        case KEY_DOWN:
            // Scroll content down - exactly like peripherals_scan.c
            if (ap_list) {
                // Check if already scrolled to bottom
                lv_coord_t scroll_bottom = lv_obj_get_scroll_bottom(ap_list);
                if (scroll_bottom > 0) {
                    // Limit scroll step to remaining scrollable distance
                    lv_coord_t scroll_step = (scroll_bottom > 30) ? 30 : scroll_bottom;
                    lv_obj_scroll_by(ap_list, 0, -scroll_step, LV_ANIM_ON);
                    printf("WiFi scan: Scrolled down by %d pixels\n", scroll_step);
                } else {
                    printf("WiFi scan: Already at bottom\n");
                }
            }
            break;
        case KEY_ENTER:
            // Refresh scan - rescan WiFi APs
            create_wifi_scan_ui();
            break;
        default:
            break;
    }
}

/**
 * @brief Create WiFi scan UI following peripherals_scan.c exactly
 */

/**
 * @brief Initialize the WiFi scan screen
 */
void wifi_scan_screen_init(void)
{
    // Initialize the widget state
    memset(&g_wifi_widget, 0, sizeof(wifi_widget_t));

    // Create WiFi scan UI
    create_wifi_scan_ui();

    // Event handling
    lv_obj_add_event_cb(ui_wifi_scan_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_wifi_scan_screen);
    lv_group_focus_obj(ui_wifi_scan_screen);
}

/**
 * @brief Deinitialize the WiFi scan screen
 */
void wifi_scan_screen_deinit(void)
{
    wifi_widget_t *w = &g_wifi_widget;

    if (ui_wifi_scan_screen) {
        printf("deinit WiFi scan screen\n");
        lv_obj_remove_event_cb(ui_wifi_scan_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_wifi_scan_screen);
    }

    // Clean up widget state
    if (w->is_active) {
        if (w->wifi_screen) {
            lv_group_t * g = lv_group_get_default();
            if (g) {
                lv_group_remove_obj(w->wifi_screen);
            }
            // Don't delete here as screen manager will handle it
            w->wifi_screen = NULL;
            w->ap_list = NULL;
            w->is_active = false;
        }
    }

    // Reset pointers
    ap_list = NULL;
    title_label = NULL;
}
