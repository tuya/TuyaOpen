/**
 * @file app_weather.c
 * @brief Weather service implementation for weather clock
 *
 * This source file provides the implementation for weather data retrieval and management
 * in the weather clock application. It fetches weather information from Tuya Cloud
 * and updates the display at regular intervals.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "app_weather.h"
#include "app_display.h"
#include "tuya_weather.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_workq_service.h"
#include "tkl_memory.h"
#include <string.h>
#include <stdio.h>

// Static function declarations
static OPERATE_RET __update_weather_data(void);
static void __send_weather_to_display(void);
static void __weather_work_callback(void *data);

/***********************************************************
***********************variable define**********************
***********************************************************/
static WEATHER_DATA_T sg_weather_data = {0};
static DELAYED_WORK_HANDLE sg_weather_work = NULL;
static BOOL_T sg_weather_initialized = FALSE;

/***********************************************************
***********************function define**********************
***********************************************************/


/**
 * @brief Update weather data from Tuya Cloud
 * @return OPERATE_RET Update result, OPRT_OK indicates success
 */
static OPERATE_RET __update_weather_data(void)
{
    OPERATE_RET rt = OPRT_OK;
    
    PR_DEBUG("=== STARTING WEATHER DATA UPDATE ===");
    
    // Check if weather service is available
    if (false == tuya_weather_allow_update()) {
        PR_ERR("Weather service not available for update");
        PR_DEBUG("Checking weather service status...");
        PR_DEBUG("This might be because:");
        PR_DEBUG("1. Device is not connected to Tuya Cloud yet");
        PR_DEBUG("2. Weather service is not initialized");
        PR_DEBUG("3. Network connection is not ready");
        PR_DEBUG("Will retry later when service becomes available");
        return OPRT_INVALID_PARM;
    }
    PR_DEBUG("Weather service is available for update");
    
    // Get current weather conditions
    WEATHER_CURRENT_CONDITIONS_T current_conditions = {0};
    PR_DEBUG("Calling tuya_weather_get_current_conditions()...");
    rt = tuya_weather_get_current_conditions(&current_conditions);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to get current weather conditions: %d", rt);
        return rt;
    }
    PR_DEBUG("Successfully retrieved weather conditions from Tuya Cloud");
    
    // Print detailed weather information
    PR_DEBUG("=== DETAILED WEATHER INFORMATION ===");
    PR_DEBUG("Weather type: %d", current_conditions.weather);
    PR_DEBUG("Temperature: %d°C", current_conditions.temp);
    PR_DEBUG("Humidity: %d%%", current_conditions.humi);
    PR_DEBUG("Real feel: %d°C", current_conditions.real_feel);
    PR_DEBUG("Pressure: %d mbar", current_conditions.mbar);
    PR_DEBUG("UV Index: %d", current_conditions.uvi);
    PR_DEBUG("=== END DETAILED WEATHER INFORMATION ===");
    
    // Update weather data
    sg_weather_data.weather_type = current_conditions.weather;
    sg_weather_data.temperature = current_conditions.temp;
    sg_weather_data.is_valid = TRUE;
    
    // Store weather type directly as icon string
    snprintf(sg_weather_data.weather_icon, sizeof(sg_weather_data.weather_icon), "%d", 
             current_conditions.weather);
    
    // Format temperature string
    snprintf(sg_weather_data.temp_str, sizeof(sg_weather_data.temp_str), "%d°C", 
             current_conditions.temp);
    
    PR_DEBUG("Weather data updated successfully:");
    PR_DEBUG("  - Weather type: %d", sg_weather_data.weather_type);
    PR_DEBUG("  - Temperature: %d°C", sg_weather_data.temperature);
    PR_DEBUG("  - Weather icon: %s", sg_weather_data.weather_icon);
    PR_DEBUG("  - Temperature string: %s", sg_weather_data.temp_str);
    PR_DEBUG("  - Data valid: %s", sg_weather_data.is_valid ? "TRUE" : "FALSE");
    PR_DEBUG("=== WEATHER DATA UPDATE COMPLETED ===");
    
    // Send updated weather data to display system
    PR_DEBUG("Sending updated weather data to display system...");
    __send_weather_to_display();
    
    return OPRT_OK;
}

/**
 * @brief Send weather update to display system
 */
static void __send_weather_to_display(void)
{
    PR_DEBUG("=== SENDING WEATHER TO DISPLAY ===");
    
    if (!sg_weather_data.is_valid) {
        PR_ERR("Weather data not valid, skipping display update");
        return;
    }
    PR_DEBUG("Weather data is valid, proceeding with display update");
    
    // Format weather data for display (icon,temperature)
    char weather_display[32];
    snprintf(weather_display, sizeof(weather_display), "%s,%s", 
             sg_weather_data.weather_icon, sg_weather_data.temp_str);
    
    PR_DEBUG("Formatted weather display string: '%s'", weather_display);
    PR_DEBUG("String length: %d", strlen(weather_display));
    
    // Send weather update message to display system
    PR_DEBUG("Sending weather update message to display system...");
    PR_DEBUG("Message type: TY_DISPLAY_TP_WEATHER_CLOCK_UPDATE_WEATHER");
    OPERATE_RET rt = app_display_send_msg(TY_DISPLAY_TP_WEATHER_CLOCK_UPDATE_WEATHER, 
                                         (uint8_t *)weather_display, strlen(weather_display));
    if (rt != OPRT_OK) {
        PR_ERR("Failed to send weather update to display: %d", rt);
    } else {
        PR_DEBUG("Weather update message sent to display successfully");
        PR_DEBUG("Display should now show: %s", weather_display);
    }
    PR_DEBUG("=== WEATHER DISPLAY UPDATE COMPLETED ===");
}

/**
 * @brief Weather update workqueue callback
 * @param data User data (unused)
 */
static void __weather_work_callback(void *data)
{
    (void)data;
    
    PR_DEBUG("=== WEATHER WORKQUEUE CALLBACK TRIGGERED ===");
    PR_DEBUG("30-minute weather update timer has triggered");
    
    // Update weather data from Tuya Cloud
    PR_DEBUG("Starting weather data update from Tuya Cloud...");
    OPERATE_RET rt = __update_weather_data();
    if (rt == OPRT_OK) {
        PR_DEBUG("Weather data update successful (display update already sent)");
        PR_DEBUG("Weather update cycle completed successfully");
    } else {
        PR_ERR("Failed to update weather data: %d", rt);
        PR_ERR("Weather update cycle failed");
        PR_DEBUG("This is normal if weather service is not ready yet");
        PR_DEBUG("Will continue trying every 30 minutes until service becomes available");
    }
    PR_DEBUG("=== WEATHER WORKQUEUE CALLBACK COMPLETED ===");
}

/**
 * @brief Initialize weather service
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET app_weather_init(void)
{
    PR_DEBUG("=== INITIALIZING WEATHER SERVICE ===");
    
    if (sg_weather_initialized) {
        PR_DEBUG("Weather service already initialized");
        return OPRT_OK;
    }
    
    // Initialize weather data
    PR_DEBUG("Initializing weather data structure...");
    memset(&sg_weather_data, 0, sizeof(WEATHER_DATA_T));
    sg_weather_data.is_valid = FALSE;
    
    // Set default weather data
    PR_DEBUG("Setting default weather data...");
    snprintf(sg_weather_data.weather_icon, sizeof(sg_weather_data.weather_icon), "%d", 0);
    strcpy(sg_weather_data.temp_str, "25°C");
    sg_weather_data.temperature = 22;
    sg_weather_data.weather_type = 0;
    
    PR_DEBUG("Default weather data set:");
    PR_DEBUG("  - Weather icon: %s", sg_weather_data.weather_icon);
    PR_DEBUG("  - Temperature string: %s", sg_weather_data.temp_str);
    PR_DEBUG("  - Temperature: %d°C", sg_weather_data.temperature);
    PR_DEBUG("  - Weather type: %d", sg_weather_data.weather_type);
    
    sg_weather_initialized = TRUE;
    PR_DEBUG("Weather service initialized successfully");
    
    // Immediately fetch weather data from Tuya Cloud
    PR_DEBUG("=== FETCHING INITIAL WEATHER DATA ===");
    PR_DEBUG("Attempting to get weather data from Tuya Cloud...");
    OPERATE_RET rt = __update_weather_data();
    if (rt == OPRT_OK) {
        PR_DEBUG("Initial weather data fetch successful");
        // Send initial weather data to display
        PR_DEBUG("Sending initial weather data to display...");
        __send_weather_to_display();
        PR_DEBUG("Initial weather data sent to display successfully");
    } else {
        PR_ERR("Initial weather data fetch failed: %d", rt);
        PR_DEBUG("Will use default weather data until next update");
        PR_DEBUG("Weather service will retry automatically when available");
    }
    PR_DEBUG("=== INITIAL WEATHER DATA FETCH COMPLETED ===");
    
    PR_DEBUG("=== WEATHER SERVICE INITIALIZATION COMPLETED ===");
    return OPRT_OK;
}

/**
 * @brief Start weather update timer
 * @return OPERATE_RET Start result, OPRT_OK indicates success
 */
OPERATE_RET app_weather_start_timer(void)
{
    PR_DEBUG("=== STARTING WEATHER UPDATE TIMER ===");
    
    if (!sg_weather_initialized) {
        PR_ERR("Weather service not initialized");
        return OPRT_INVALID_PARM;
    }
    PR_DEBUG("Weather service is initialized, proceeding with timer setup");
    
    if (sg_weather_work != NULL) {
        PR_DEBUG("Weather workqueue already started");
        return OPRT_OK;
    }
    
    // Initialize delayed work for weather updates
    PR_DEBUG("Initializing delayed workqueue for weather updates...");
    PR_DEBUG("Workqueue type: WORKQ_SYSTEM");
    PR_DEBUG("Callback function: __weather_work_callback");
    OPERATE_RET rt = tal_workq_init_delayed(WORKQ_SYSTEM, __weather_work_callback, NULL, &sg_weather_work);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to initialize weather workqueue: %d", rt);
        return rt;
    }
    PR_DEBUG("Weather workqueue initialized successfully");
    
    // Start the delayed work (30 minutes interval, loop continuously)
    PR_DEBUG("Starting delayed workqueue...");
    PR_DEBUG("Update interval: %d ms (30 minutes)", WEATHER_UPDATE_INTERVAL_MS);
    PR_DEBUG("Loop type: LOOP_CYCLE (continuous)");
    rt = tal_workq_start_delayed(sg_weather_work, WEATHER_UPDATE_INTERVAL_MS, LOOP_CYCLE);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to start weather workqueue: %d", rt);
        return rt;
    }
    
    PR_DEBUG("Weather update workqueue started successfully");
    PR_DEBUG("Weather will be updated every 30 minutes automatically");
    
    // Note: Initial weather update is already done in app_weather_init()
    PR_DEBUG("Initial weather data was already fetched during initialization");
    
    PR_DEBUG("=== WEATHER UPDATE TIMER STARTED ===");
    return OPRT_OK;
}

/**
 * @brief Stop weather update timer
 * @return OPERATE_RET Stop result, OPRT_OK indicates success
 */
OPERATE_RET app_weather_stop_timer(void)
{
    if (sg_weather_work == NULL) {
        PR_DEBUG("Weather workqueue not started");
        return OPRT_OK;
    }
    
    // Stop the delayed work
    OPERATE_RET rt = tal_workq_stop_delayed(sg_weather_work);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to stop weather workqueue: %d", rt);
    }
    
    sg_weather_work = NULL;
    
    PR_DEBUG("Weather update workqueue stopped");
    return OPRT_OK;
}

/**
 * @brief Get current weather data
 * @param weather_data Pointer to weather data structure
 * @return OPERATE_RET Get result, OPRT_OK indicates success
 */
OPERATE_RET app_weather_get_current(WEATHER_DATA_T *weather_data)
{
    if (weather_data == NULL) {
        PR_ERR("Invalid weather data pointer");
        return OPRT_INVALID_PARM;
    }
    
    if (!sg_weather_initialized) {
        PR_ERR("Weather service not initialized");
        return OPRT_INVALID_PARM;
    }
    
    // Copy current weather data
    memcpy(weather_data, &sg_weather_data, sizeof(WEATHER_DATA_T));
    
    return OPRT_OK;
}

/**
 * @brief Check if weather service is ready and try to update
 * @return OPERATE_RET Update result, OPRT_OK indicates success
 */
OPERATE_RET app_weather_check_and_update(void)
{
    PR_DEBUG("=== CHECKING WEATHER SERVICE STATUS ===");
    
    if (!sg_weather_initialized) {
        PR_ERR("Weather service not initialized");
        return OPRT_INVALID_PARM;
    }
    
    // Check if weather service is now available
    if (false == tuya_weather_allow_update()) {
        PR_DEBUG("Weather service still not available, will retry later");
        return OPRT_INVALID_PARM;
    }
    
    PR_DEBUG("Weather service is now available, attempting update...");
    return app_weather_update_now();
}

/**
 * @brief Update weather data immediately
 * @return OPERATE_RET Update result, OPRT_OK indicates success
 */
OPERATE_RET app_weather_update_now(void)
{
    PR_DEBUG("=== MANUAL WEATHER UPDATE REQUESTED ===");
    
    if (!sg_weather_initialized) {
        PR_ERR("Weather service not initialized");
        return OPRT_INVALID_PARM;
    }
    PR_DEBUG("Weather service is initialized, proceeding with manual update");
    
    PR_DEBUG("Starting immediate weather data update...");
    
    // Update weather data from Tuya Cloud
    OPERATE_RET rt = __update_weather_data();
    if (rt == OPRT_OK) {
        PR_DEBUG("Weather data update successful (display update already sent)");
        PR_DEBUG("Manual weather update completed successfully");
    } else {
        PR_ERR("Failed to update weather data: %d", rt);
        PR_ERR("Manual weather update failed");
    }
    
    PR_DEBUG("=== MANUAL WEATHER UPDATE COMPLETED ===");
    return rt;
}

/**
 * @brief Cleanup weather service
 */
void app_weather_cleanup(void)
{
    // Stop workqueue if running
    app_weather_stop_timer();
    
    // Clear weather data
    memset(&sg_weather_data, 0, sizeof(WEATHER_DATA_T));
    sg_weather_initialized = FALSE;
    
    PR_DEBUG("Weather service cleaned up");
}
