/**
 * @file app_weather.h
 * @brief Weather service for weather clock
 *
 * This header file provides the interface for weather data retrieval and management
 * in the weather clock application. It includes functions for fetching weather
 * information from Tuya Cloud and updating the display.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __APP_WEATHER_H__
#define __APP_WEATHER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define WEATHER_UPDATE_INTERVAL_MS    (30 * 60 * 1000)  // 30 minutes

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    int weather_type;      // Weather type code from Tuya Cloud
    int temperature;       // Current temperature in Celsius
    char weather_icon[8];  // Weather type code as string (e.g., "0", "100", "104")
    char temp_str[16];     // Temperature string (e.g., "25°C")
    BOOL_T is_valid;       // Whether weather data is valid
} WEATHER_DATA_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize weather service
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET app_weather_init(void);

/**
 * @brief Start weather update timer
 * @return OPERATE_RET Start result, OPRT_OK indicates success
 */
OPERATE_RET app_weather_start_timer(void);

/**
 * @brief Stop weather update timer
 * @return OPERATE_RET Stop result, OPRT_OK indicates success
 */
OPERATE_RET app_weather_stop_timer(void);

/**
 * @brief Get current weather data
 * @param weather_data Pointer to weather data structure
 * @return OPERATE_RET Get result, OPRT_OK indicates success
 */
OPERATE_RET app_weather_get_current(WEATHER_DATA_T *weather_data);

/**
 * @brief Check if weather service is ready and try to update
 * @return OPERATE_RET Update result, OPRT_OK indicates success
 */
OPERATE_RET app_weather_check_and_update(void);

/**
 * @brief Update weather data immediately
 * @return OPERATE_RET Update result, OPRT_OK indicates success
 */
OPERATE_RET app_weather_update_now(void);

/**
 * @brief Cleanup weather service
 */
void app_weather_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_WEATHER_H__ */
