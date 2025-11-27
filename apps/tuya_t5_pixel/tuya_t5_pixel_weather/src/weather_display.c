/**
 * @file weather_display.c
 * @brief Weather display engine implementation for pixel screen
 *
 * This module implements a weather display engine that shows weather information
 * on the 32x32 pixel matrix. It displays:
 * - Weather icon (16x16) in the top right corner
 * - Weather data using picofont (temperature, time, date, humidity, etc.)
 * - Cycles through different information screens
 */

#include "weather_display.h"
#include "board_pixel_api.h"
#include "tuya_weather.h"
#include "tal_api.h"
#include "tal_time_service.h"
#include "netmgr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Weather icon includes
#include "weather_icons/sunny_16dp.h"
#include "weather_icons/cloud_16dp.h"
#include "weather_icons/partly_cloudy_day_16dp.h"
#include "weather_icons/partly_cloudy_night_16dp.h"
#include "weather_icons/rainy_16dp.h"
#include "weather_icons/rainy_heavy_16dp.h"
#include "weather_icons/rainy_light_16dp.h"
#include "weather_icons/weather_snowy_16dp.h"
#include "weather_icons/thunderstorm_16dp.h"
#include "weather_icons/foggy_16dp.h"
#include "weather_icons/moon_stars_16dp.h"
#include "weather_icons/bedtime_16dp.h"
#include "weather_icons/tornado_16dp.h"
#include "weather_icons/tsunami_16dp.h"
#include "weather_icons/rainy_snow_16dp.h"

// WiFi status icon includes (animated GIF only)
#include "sys_icons/wifi_connecting_pairing.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define DISPLAY_UPDATE_INTERVAL_MS 5000 // Update display every 5 seconds
#define SCREEN_DURATION_MS         8000 // Each screen shown for 8 seconds
#define ICON_POS_X                 16   // Top right corner X position
#define ICON_POS_Y                 0    // Top right corner Y position
#define ICON_SIZE                  16
// Unified text height offsets for all weather pages
#define TEXT_LINE_1_Y                 8    // First line Y position
#define TEXT_LINE_2_Y                 16   // Second line Y position
#define TEXT_LINE_3_Y                 24   // Third line Y position
#define WIFI_STATUS_CHECK_INTERVAL_MS 2000 // Check WiFi status every 2 seconds
#define WIFI_ANIMATION_FRAME_DELAY    0    // Animation frame delay (update every iteration for faster animation)
#define WIFI_ANIMATION_FRAME_SKIP     4    // Drop some gif frames

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    SCREEN_TEMP = 0,
    SCREEN_HIGH_LOW,
    SCREEN_HUMIDITY,
    SCREEN_TIME_DATE,
    SCREEN_WIND,
    SCREEN_AQI,
    SCREEN_WIFI_STATUS, // Full-screen WiFi status page
    SCREEN_COUNT
} display_screen_e;

/***********************************************************
***********************variable define**********************
***********************************************************/
static PIXEL_FRAME_HANDLE_T g_frame = NULL;
static THREAD_HANDLE g_display_thread = NULL;
static volatile bool g_display_running = false;
static display_screen_e g_current_screen = SCREEN_TEMP;
static uint32_t g_last_screen_change = 0;
static uint32_t g_last_weather_update = 0;

// Weather data cache
static WEATHER_CURRENT_CONDITIONS_T g_current_conditions = {0};
static int g_today_high = 0, g_today_low = 0;
static char g_wind_dir[64] = {0};
static char g_wind_speed[64] = {0};
static WEATHER_CURRENT_AQI_T g_current_aqi = {0};
static bool g_weather_data_valid = false;

// WiFi status tracking
static bool g_wifi_connected = false;
static bool g_mqtt_connected = false; // MQTT connection status
static bool g_force_wifi_page = true; // Start with WiFi page by default
static uint32_t g_last_wifi_check = 0;
static uint32_t g_wifi_animation_frame = 0;

/***********************************************************
********************function declaration********************
***********************************************************/
static const pixel_frame_t *weather_code_to_icon(int weather_code);
static void convert_pixel_frame_to_bitmap(const pixel_frame_t *frame, uint8_t *bitmap);
static void render_pixel_art_to_frame(PIXEL_FRAME_HANDLE_T frame, const pixel_frame_t *pixel_frame);
static void draw_weather_icon(PIXEL_FRAME_HANDLE_T frame, int weather_code);
static void draw_screen_temp(PIXEL_FRAME_HANDLE_T frame);
static void draw_screen_high_low(PIXEL_FRAME_HANDLE_T frame);
static void draw_screen_humidity(PIXEL_FRAME_HANDLE_T frame);
static void draw_screen_time_date(PIXEL_FRAME_HANDLE_T frame);
static void draw_screen_wind(PIXEL_FRAME_HANDLE_T frame);
static void draw_screen_aqi(PIXEL_FRAME_HANDLE_T frame);
static void draw_screen_wifi_status(PIXEL_FRAME_HANDLE_T frame);
static void check_wifi_status(void);
static void update_weather_data(void);
static void display_task(void *args);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Map weather code to icon
 */
static const pixel_frame_t *weather_code_to_icon(int weather_code)
{
    switch (weather_code) {
    case TW_WEATHER_SUNNY:
    case TW_WEATHER_CLEAR:
    case TW_WEATHER_MOSTLY_CLEAR:
        return &sunny_16dp;

    case TW_WEATHER_CLOUDY:
    case TW_WEATHER_OVERCAST:
        return &cloud_16dp;

    case TW_WEATHER_PARTLY_CLOUDY:
        // Use day icon (could be enhanced to check time of day)
        return &partly_cloudy_day_16dp;

    case TW_WEATHER_RAIN:
    case TW_WEATHER_MODERATE_RAIN:
    case TW_WEATHER_LIGHT_TO_MODERATE_RAIN:
        return &rainy_16dp;

    case TW_WEATHER_HEAVY_RAIN:
    case TW_WEATHER_RAINSTORM:
    case TW_WEATHER_EXTREME_RAINSTORM:
    case TW_WEATHER_DOWNPOUR:
    case TW_WEATHER_MODERATE_TO_HEAVY_RAIN:
    case TW_WEATHER_HEAVY_RAIN_TO_RAINSTORM:
        return &rainy_heavy_16dp;

    case TW_WEATHER_LIGHT_RAIN:
    case TW_WEATHER_LIGHT_SHOWER:
    case TW_WEATHER_ISOLATED_SHOWER:
        return &rainy_light_16dp;

    case TW_WEATHER_SNOW:
    case TW_WEATHER_HEAVY_SNOW:
    case TW_WEATHER_MODERATE_SNOW:
    case TW_WEATHER_LIGHT_TO_MODERATE_SNOW:
    case TW_WEATHER_SNOW_SHOWER:
    case TW_WEATHER_LIGHT_SNOW_SHOWER:
        return &weather_snowy_16dp;

    case TW_WEATHER_THUNDERSTORM:
    case TW_WEATHER_THUNDER_AND_LIGHTNING:
    case TW_WEATHER_THUNDERSHOWER:
    case TW_WEATHER_THUNDERSHOWER_AND_HAIL:
        return &thunderstorm_16dp;

    case TW_WEATHER_FOG:
    case TW_WEATHER_FREEZING_FOG:
    case TW_WEATHER_HAZE:
        return &foggy_16dp;

    case TW_WEATHER_SLEET:
        return &rainy_snow_16dp;

    case TW_WEATHER_SANDSTORM:
    case TW_WEATHER_STRONG_SANDSTORM:
    case TW_WEATHER_SAND_BLOWING:
    case TW_WEATHER_DUST:
    case TW_WEATHER_DUST_DEVIL:
        return &tornado_16dp;

    default:
        return &cloud_16dp; // Default to cloud
    }
}

/**
 * @brief Convert pixel_frame_t to RGB bitmap format for board_pixel_draw_bitmap
 */
static void convert_pixel_frame_to_bitmap(const pixel_frame_t *frame, uint8_t *bitmap)
{
    if (frame == NULL || bitmap == NULL) {
        return;
    }

    for (uint32_t i = 0; i < (frame->width * frame->height); i++) {
        bitmap[i * 3] = frame->pixels[i].r;
        bitmap[i * 3 + 1] = frame->pixels[i].g;
        bitmap[i * 3 + 2] = frame->pixels[i].b;
    }
}

/**
 * @brief Render pixel art frame to frame buffer (following demo's approach)
 */
static void render_pixel_art_to_frame(PIXEL_FRAME_HANDLE_T frame, const pixel_frame_t *pixel_frame)
{
    if (frame == NULL || pixel_frame == NULL) {
        return;
    }

    // Convert pixel_frame_t to RGB bitmap format (pixels already in correct color)
    uint8_t *bitmap = (uint8_t *)tal_malloc(pixel_frame->width * pixel_frame->height * 3);
    if (bitmap == NULL) {
        return;
    }

    convert_pixel_frame_to_bitmap(pixel_frame, bitmap);
    board_pixel_draw_bitmap(frame, 0, 0, bitmap, pixel_frame->width, pixel_frame->height);
    tal_free(bitmap);
}

/**
 * @brief Draw weather icon in top right corner
 */
static void draw_weather_icon(PIXEL_FRAME_HANDLE_T frame, int weather_code)
{
    const pixel_frame_t *icon = weather_code_to_icon(weather_code);
    if (icon == NULL) {
        return;
    }

    // Convert pixel_frame_t to RGB bitmap format
    uint8_t *bitmap = (uint8_t *)tal_malloc(icon->width * icon->height * 3);
    if (bitmap == NULL) {
        return;
    }

    convert_pixel_frame_to_bitmap(icon, bitmap);
    board_pixel_draw_bitmap(frame, ICON_POS_X, ICON_POS_Y, bitmap, icon->width, icon->height);

    tal_free(bitmap);
}

/**
 * @brief Draw temperature screen
 */
static void draw_screen_temp(PIXEL_FRAME_HANDLE_T frame)
{
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%dC", g_current_conditions.temp);
    board_pixel_draw_text(frame, 0, TEXT_LINE_1_Y, temp_str, PIXEL_COLOR_WHITE, PIXEL_FONT_PICOPIXEL);

    // Draw "FEEL" label
    board_pixel_draw_text(frame, 0, TEXT_LINE_2_Y, "FEEL", PIXEL_COLOR_CYAN, PIXEL_FONT_PICOPIXEL);

    // Draw feels like temperature
    char feel_str[16];
    snprintf(feel_str, sizeof(feel_str), "%dC", g_current_conditions.real_feel);
    board_pixel_draw_text(frame, 0, TEXT_LINE_3_Y, feel_str, PIXEL_COLOR_CYAN, PIXEL_FONT_PICOPIXEL);
}

/**
 * @brief Draw high/low temperature screen
 */
static void draw_screen_high_low(PIXEL_FRAME_HANDLE_T frame)
{
    char high_str[16];
    snprintf(high_str, sizeof(high_str), "H:%d", g_today_high);
    board_pixel_draw_text(frame, 0, TEXT_LINE_1_Y, high_str, PIXEL_COLOR_RED, PIXEL_FONT_PICOPIXEL);

    char low_str[16];
    snprintf(low_str, sizeof(low_str), "L:%d", g_today_low);
    board_pixel_draw_text(frame, 0, TEXT_LINE_2_Y, low_str, PIXEL_COLOR_BLUE, PIXEL_FONT_PICOPIXEL);
}

/**
 * @brief Draw humidity screen
 */
static void draw_screen_humidity(PIXEL_FRAME_HANDLE_T frame)
{
    board_pixel_draw_text(frame, 0, TEXT_LINE_1_Y, "HUMI", PIXEL_COLOR_GREEN, PIXEL_FONT_PICOPIXEL);

    char humi_str[16];
    snprintf(humi_str, sizeof(humi_str), "%d%%", g_current_conditions.humi);
    board_pixel_draw_text(frame, 0, TEXT_LINE_2_Y, humi_str, PIXEL_COLOR_GREEN, PIXEL_FONT_PICOPIXEL);
}

/**
 * @brief Draw time and date screen
 */
static void draw_screen_time_date(PIXEL_FRAME_HANDLE_T frame)
{
    // Get local time using Tuya API (handles timezone automatically)
    // Pass 0 to get current local time
    POSIX_TM_S local_time;
    OPERATE_RET rt = tal_time_get_local_time_custom(0, &local_time);
    if (OPRT_OK != rt) {
        board_pixel_draw_text(frame, 0, TEXT_LINE_2_Y, "NO TIME", PIXEL_COLOR_YELLOW, PIXEL_FONT_PICOPIXEL);
        return;
    }

    // Display time (HH:MM)
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", local_time.tm_hour, local_time.tm_min);
    board_pixel_draw_text(frame, 0, TEXT_LINE_1_Y, time_str, PIXEL_COLOR_WHITE, PIXEL_FONT_PICOPIXEL);

    // Display date (MM/DD)
    // Note: tm_mon is 0-11, tm_mday is 1-31
    char date_str[16];
    snprintf(date_str, sizeof(date_str), "%02d/%02d", local_time.tm_mon + 1, local_time.tm_mday);
    board_pixel_draw_text(frame, 0, TEXT_LINE_2_Y, date_str, PIXEL_COLOR_CYAN, PIXEL_FONT_PICOPIXEL);
}

/**
 * @brief Draw wind screen
 */
static void draw_screen_wind(PIXEL_FRAME_HANDLE_T frame)
{
    board_pixel_draw_text(frame, 0, TEXT_LINE_1_Y, "WIND", PIXEL_COLOR_YELLOW, PIXEL_FONT_PICOPIXEL);

    // Display wind direction (truncate if too long)
    char wind_display[16] = {0};
    strncpy(wind_display, g_wind_dir, 8);
    wind_display[8] = '\0';
    board_pixel_draw_text(frame, 0, TEXT_LINE_2_Y, wind_display, PIXEL_COLOR_YELLOW, PIXEL_FONT_PICOPIXEL);

    // Display wind speed (truncate if too long)
    char speed_display[16] = {0};
    strncpy(speed_display, g_wind_speed, 8);
    speed_display[8] = '\0';
    board_pixel_draw_text(frame, 0, TEXT_LINE_3_Y, speed_display, PIXEL_COLOR_YELLOW, PIXEL_FONT_PICOPIXEL);
}

/**
 * @brief Draw AQI screen
 */
static void draw_screen_aqi(PIXEL_FRAME_HANDLE_T frame)
{
    board_pixel_draw_text(frame, 0, TEXT_LINE_1_Y, "AQI", PIXEL_COLOR_MAGENTA, PIXEL_FONT_PICOPIXEL);

    char aqi_str[16];
    snprintf(aqi_str, sizeof(aqi_str), "%d", g_current_aqi.aqi);
    board_pixel_draw_text(frame, 0, TEXT_LINE_2_Y, aqi_str, PIXEL_COLOR_MAGENTA, PIXEL_FONT_PICOPIXEL);
}

/**
 * @brief Check WiFi connection status
 * Simplified: Only need to check if MQTT is connected to show weather pages
 */
static void check_wifi_status(void)
{
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    g_wifi_connected = (status != NETMGR_LINK_DOWN);

    // Show WiFi page (animated GIF) until MQTT is connected
    if (!g_mqtt_connected) {
        g_force_wifi_page = true;
    } else if (g_wifi_connected && g_mqtt_connected) {
        // Both WiFi and MQTT connected - allow weather pages
        g_force_wifi_page = false;
    } else {
        // WiFi disconnected but MQTT connected (unlikely) - show WiFi page
        g_force_wifi_page = true;
    }
}

/**
 * @brief Draw WiFi status page (full screen 32x32)
 * Shows animated GIF before MQTT is connected
 */
static void draw_screen_wifi_status(PIXEL_FRAME_HANDLE_T frame)
{
    // Show animated WiFi GIF (used before MQTT connection)
    if (wifi_connecting_pairing.frame_count > 0) {
        uint32_t frame_idx = g_wifi_animation_frame % wifi_connecting_pairing.frame_count;
        const pixel_frame_t *current_frame = &wifi_connecting_pairing.frames[frame_idx];
        render_pixel_art_to_frame(frame, current_frame);
    }
}

/**
 * @brief Update weather data from Tuya weather service
 */
static void update_weather_data(void)
{
    if (false == tuya_weather_allow_update()) {
        g_weather_data_valid = false;
        return;
    }

    OPERATE_RET rt = OPRT_OK;
    bool data_ok = false;

    // Get current weather conditions
    rt = tuya_weather_get_current_conditions(&g_current_conditions);
    if (OPRT_OK == rt) {
        data_ok = true;
    } else {
        PR_DEBUG("Failed to get current conditions: %d", rt);
    }

    // Get today's high and low temperature (ignore errors, use defaults)
    rt = tuya_weather_get_today_high_low_temp(&g_today_high, &g_today_low);
    if (OPRT_OK != rt) {
        g_today_high = 0;
        g_today_low = 0;
    }

    // Get current wind (ignore errors, use defaults)
    rt = tuya_weather_get_current_wind(g_wind_dir, g_wind_speed);
    if (OPRT_OK != rt) {
        strncpy(g_wind_dir, "N/A", sizeof(g_wind_dir) - 1);
        strncpy(g_wind_speed, "N/A", sizeof(g_wind_speed) - 1);
    }

    // Get current AQI (ignore errors, use defaults)
    rt = tuya_weather_get_current_aqi(&g_current_aqi);
    if (OPRT_OK != rt) {
        memset(&g_current_aqi, 0, sizeof(g_current_aqi));
    }

    // Only mark as valid if we got current conditions
    g_weather_data_valid = data_ok;
}

/**
 * @brief Display task thread
 */
static void display_task(void *args)
{
    PR_NOTICE("Weather display task started");

    g_frame = board_pixel_frame_create();
    if (g_frame == NULL) {
        PR_ERR("Failed to create pixel frame");
        g_display_running = false;
        return;
    }

    uint32_t current_time = 0;

    while (g_display_running) {
        current_time = (uint32_t)tal_time_get_posix_ms();

        // Check WiFi status periodically
        if (current_time - g_last_wifi_check >= WIFI_STATUS_CHECK_INTERVAL_MS) {
            check_wifi_status();
            g_last_wifi_check = current_time;
        }

        // Update WiFi animation frame (animate when on WiFi status page)
        if (g_current_screen == SCREEN_WIFI_STATUS && wifi_connecting_pairing.frame_count > 0) {
            static uint32_t frame_counter = 0;
            frame_counter++;
            if (frame_counter >= WIFI_ANIMATION_FRAME_DELAY) {
                frame_counter = 0;
                g_wifi_animation_frame = (g_wifi_animation_frame + 1) % wifi_connecting_pairing.frame_count;
            }
        }

        // Check if WiFi page should be forced (priority over weather pages)
        bool should_show_wifi = g_force_wifi_page || !g_wifi_connected || !g_mqtt_connected;

        // If WiFi page is forced, always show WiFi status
        if (should_show_wifi) {
            g_current_screen = SCREEN_WIFI_STATUS;
        } else {
            // Update weather data periodically (only when WiFi and MQTT are connected)
            if (current_time - g_last_weather_update >= DISPLAY_UPDATE_INTERVAL_MS) {
                update_weather_data();
                g_last_weather_update = current_time;
            }

            // Change screen periodically (only cycle through weather pages)
            if (current_time - g_last_screen_change >= SCREEN_DURATION_MS) {
                // Skip WiFi status page when connected, cycle through weather pages only
                g_current_screen = (g_current_screen + 1) % SCREEN_COUNT;
                if (g_current_screen == SCREEN_WIFI_STATUS) {
                    g_current_screen = SCREEN_TEMP; // Skip WiFi page, go to first weather page
                }
                g_last_screen_change = current_time;
            }
        }

        // Clear frame
        board_pixel_frame_clear(g_frame);

        // Page/Frame Manager: Only one page displayed at a time
        if (should_show_wifi) {
            // Full-screen WiFi status page (32x32) - takes priority
            draw_screen_wifi_status(g_frame);
        } else {
            // Show weather pages only when WiFi and MQTT are connected
            switch (g_current_screen) {

            case SCREEN_TEMP:
            case SCREEN_HIGH_LOW:
            case SCREEN_HUMIDITY:
            case SCREEN_TIME_DATE:
            case SCREEN_WIND:
            case SCREEN_AQI:
                // Weather pages: Draw weather icon in top right corner
                if (g_weather_data_valid) {
                    draw_weather_icon(g_frame, g_current_conditions.weather);
                } else {
                    // Draw a default cloud icon when data is unavailable
                    draw_weather_icon(g_frame, TW_WEATHER_CLOUDY);
                }

                if (!g_weather_data_valid && g_current_screen != SCREEN_TIME_DATE) {
                    // Show error message for weather screens when data is unavailable
                    board_pixel_draw_text(g_frame, 0, TEXT_LINE_1_Y, "WAIT", PIXEL_COLOR_RED, PIXEL_FONT_PICOPIXEL);
                    board_pixel_draw_text(g_frame, 0, TEXT_LINE_2_Y, "FOR", PIXEL_COLOR_RED, PIXEL_FONT_PICOPIXEL);
                    board_pixel_draw_text(g_frame, 0, TEXT_LINE_3_Y, "DATA", PIXEL_COLOR_RED, PIXEL_FONT_PICOPIXEL);
                } else {
                    switch (g_current_screen) {
                    case SCREEN_TEMP:
                        draw_screen_temp(g_frame);
                        break;
                    case SCREEN_HIGH_LOW:
                        draw_screen_high_low(g_frame);
                        break;
                    case SCREEN_HUMIDITY:
                        draw_screen_humidity(g_frame);
                        break;
                    case SCREEN_TIME_DATE:
                        draw_screen_time_date(g_frame);
                        break;
                    case SCREEN_WIND:
                        draw_screen_wind(g_frame);
                        break;
                    case SCREEN_AQI:
                        draw_screen_aqi(g_frame);
                        break;
                    default:
                        break;
                    }
                }
                break;

            default:
                // Fallback to WiFi status if unknown screen
                draw_screen_wifi_status(g_frame);
                break;
            }
        }

        // Render frame
        board_pixel_frame_render(g_frame);

        // Sleep for frame rate control
        tal_system_sleep(100); // 10 FPS
    }

    // Cleanup
    if (g_frame != NULL) {
        board_pixel_frame_destroy(g_frame);
        g_frame = NULL;
    }

    PR_NOTICE("Weather display task stopped");
    g_display_thread = NULL;
}

/**
 * @brief Initialize weather display engine
 */
OPERATE_RET weather_display_init(void)
{
    PR_NOTICE("Initializing weather display engine");
    return OPRT_OK;
}

/**
 * @brief Start weather display task
 */
OPERATE_RET weather_display_start(void)
{
    if (g_display_running) {
        PR_WARN("Weather display already running");
        return OPRT_OK;
    }

    g_display_running = true;
    g_current_screen = SCREEN_WIFI_STATUS; // Start with WiFi status page
    g_last_screen_change = (uint32_t)tal_time_get_posix_ms();
    g_last_weather_update = 0;
    g_last_wifi_check = 0;
    g_wifi_animation_frame = 0;
    g_wifi_connected = false;
    g_mqtt_connected = false;
    g_force_wifi_page = true; // Force WiFi page at startup

    // Create display thread
    THREAD_CFG_T thrd_param = {.stackDepth = 4096, .priority = THREAD_PRIO_2, .thrdname = "weather_disp"};

    OPERATE_RET rt = tal_thread_create_and_start(&g_display_thread, NULL, NULL, display_task, NULL, &thrd_param);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to start weather display thread: %d", rt);
        g_display_running = false;
        return rt;
    }

    PR_NOTICE("Weather display started");
    return OPRT_OK;
}

/**
 * @brief Stop weather display task
 */
void weather_display_stop(void)
{
    if (!g_display_running) {
        return;
    }

    g_display_running = false;

    // Wait for thread to finish
    while (g_display_thread != NULL) {
        tal_system_sleep(100);
    }

    PR_NOTICE("Weather display stopped");
}

/**
 * @brief Update weather data and trigger display refresh
 */
void weather_display_update(void)
{
    update_weather_data();
}

/**
 * @brief Notify display that MQTT is connected (can show weather pages)
 */
void weather_display_mqtt_connected(void)
{
    g_mqtt_connected = true;

    // Only allow weather pages if WiFi is also connected
    if (g_wifi_connected) {
        g_force_wifi_page = false; // Allow weather pages to be shown
        PR_NOTICE("MQTT connected - switching to weather display");
    } else {
        // WiFi not connected yet, keep showing WiFi status
        g_force_wifi_page = true;
        PR_NOTICE("MQTT connected but WiFi not connected - staying on WiFi status");
    }
}

/**
 * @brief Force WiFi status page to be displayed (start connecting animation)
 */
void weather_display_show_wifi_status(void)
{
    g_force_wifi_page = true;
    g_current_screen = SCREEN_WIFI_STATUS;
    g_wifi_animation_frame = 0;
    PR_NOTICE("Forcing WiFi status page display - showing animated WiFi");
}
