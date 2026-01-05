/**
 * @file ui_weather_clock.h
 * @brief Header file for Weather Clock UI
 *
 * This header file defines the interface for the weather clock display module,
 * including initialization functions and data structures for managing the
 * weather clock interface with time, date, and weather information.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __UI_WEATHER_CLOCK_H__
#define __UI_WEATHER_CLOCK_H__

#include "tuya_cloud_types.h"
#include "lvgl.h"
#include "app_display.h"
#include "ui_display.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define WEATHER_CLOCK_UPDATE_INTERVAL_MS    1000    // 1 second update interval

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    lv_obj_t *container;
    lv_obj_t *status_bar;
    lv_obj_t *time_label;           // Time display
    lv_obj_t *date_weather_label;   // Date display
    lv_obj_t *temperature_label;    // Temperature display
    lv_obj_t *weather_icon_img;     // Weather icon display
    lv_obj_t *network_label;
    lv_obj_t *notification_label;
    lv_obj_t *status_label;
    lv_timer_t *update_timer;
} WEATHER_CLOCK_UI_T;

typedef struct {
    WEATHER_CLOCK_UI_T ui;
    UI_FONT_T font;
    BOOL_T is_visible;
    time_t base_timestamp;      // Base timestamp for independent time calculation
    SYS_TICK_T base_uptime_ms;  // System uptime when timestamp was set
} WEATHER_CLOCK_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize the weather clock UI
 * @param ui_font Font configuration for the UI
 * @return 0 on success, -1 on failure
 */
int ui_weather_clock_init(UI_FONT_T *ui_font);

/**
 * @brief Show the weather clock interface
 */
void ui_weather_clock_show(void);

/**
 * @brief Hide the weather clock interface
 */
void ui_weather_clock_hide(void);

/**
 * @brief Update weather information (legacy function for compatibility)
 * @param weather_icon Weather icon (e.g., "☀️", "🌧️", "⛅")
 * @param temperature Temperature in Celsius (e.g., "22°C")
 */
void ui_weather_clock_update_weather(const char *weather_icon, const char *temperature);

/**
 * @brief Update time display
 * @param time_str Time string (e.g., "14:30:25")
 */
void ui_weather_clock_update_time_display(const char *time_str);

/**
 * @brief Update date display
 * @param date_str Date string (e.g., "12/25")
 */
void ui_weather_clock_update_date_display(const char *date_str);

/**
 * @brief Update temperature display
 * @param temperature Temperature string (e.g., "25°C")
 */
void ui_weather_clock_update_temperature_display(const char *temperature);

/**
 * @brief Update weather icon display
 * @param weather_icon Weather icon identifier (e.g., "120", "cloudy_112", etc.)
 */
void ui_weather_clock_update_weather_icon_display(const char *weather_icon);

/**
 * @brief Set base timestamp for independent time calculation
 * @param timestamp Unix timestamp from time sync
 */
void ui_weather_clock_set_timestamp(int timestamp);

/**
 * @brief Update time display using current system time
 */
void ui_weather_clock_update_time(void);

/**
 * @brief Update network status
 * @param wifi_icon WiFi icon from font awesome
 */
void ui_weather_clock_update_network(const char *wifi_icon);

/**
 * @brief Show notification message
 * @param notification Notification text
 */
void ui_weather_clock_show_notification(const char *notification);

/**
 * @brief Check if weather clock is currently visible
 * @return TRUE if visible, FALSE otherwise
 */
BOOL_T ui_weather_clock_is_visible(void);

/**
 * @brief Cleanup weather clock resources
 */
void ui_weather_clock_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_WEATHER_CLOCK_H__ */
