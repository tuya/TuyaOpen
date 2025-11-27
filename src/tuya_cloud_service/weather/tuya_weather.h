/**
 * @file tuya_weather.h
 * @brief Header file for the Tuya IoT weather service for weather data retrieval.
 *
 * This header file contains the declarations for the Tuya IoT weather service,
 * which is used for retrieving weather information from the Tuya cloud platform.
 * It provides function declarations for getting current weather conditions,
 * forecast data, air quality information, and other weather-related data.
 *
 * The weather service includes mechanisms for network connectivity checks, time
 * synchronization, and data parsing from JSON responses. It supports multiple
 * weather data formats and provides both international and China-specific
 * weather data APIs.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_WEATHER_H__
#define __TUYA_WEATHER_H__

/******************************************************************************
 * INCLUDE
 ******************************************************************************/
#include <stddef.h>
#include "tal_api.h"
#include "atop_base.h"

/******************************************************************************
 * CONSTANTS
 ******************************************************************************/
// Weather data
// https://developer.tuya.com/en/docs/mcu-standard-protocol/mcusdk-wifi-weather?id=Kd2fvzw7ny80s#title-14-Appendix%204%3A%20Weather%20data%20in%20UTF-8%20encoding
#define TW_WEATHER_SUNNY                    (120)
#define TW_WEATHER_HEAVY_RAIN               (101)
#define TW_WEATHER_THUNDERSTORM             (102)
#define TW_WEATHER_SANDSTORM                (103)
#define TW_WEATHER_LIGHT_SNOW               (104)
#define TW_WEATHER_SNOW                     (105)
#define TW_WEATHER_FREEZING_FOG             (106)
#define TW_WEATHER_RAINSTORM                (107)
#define TW_WEATHER_ISOLATED_SHOWER          (108)
#define TW_WEATHER_DUST                     (109)
#define TW_WEATHER_THUNDER_AND_LIGHTNING    (110)
#define TW_WEATHER_LIGHT_SHOWER             (111)
#define TW_WEATHER_RAIN                     (112)
#define TW_WEATHER_SLEET                    (113)
#define TW_WEATHER_DUST_DEVIL               (114)
#define TW_WEATHER_ICE_PELLETS              (115)
#define TW_WEATHER_STRONG_SANDSTORM         (116)
#define TW_WEATHER_SAND_BLOWING             (117)
#define TW_WEATHER_LIGHT_TO_MODERATE_RAIN   (118)
#define TW_WEATHER_MOSTLY_CLEAR             (119)
#define TW_WEATHER_FOG                      (121)
#define TW_WEATHER_SHOWER                   (122)
#define TW_WEATHER_HEAVY_SHOWER             (123)
#define TW_WEATHER_HEAVY_SNOW               (124)
#define TW_WEATHER_EXTREME_RAINSTORM        (125)
#define TW_WEATHER_BLIZZARD                 (126)
#define TW_WEATHER_HAIL                     (127)
#define TW_WEATHER_LIGHT_TO_MODERATE_SNOW   (128)
#define TW_WEATHER_PARTLY_CLOUDY            (129)
#define TW_WEATHER_LIGHT_SNOW_SHOWER        (130)
#define TW_WEATHER_MODERATE_SNOW            (131)
#define TW_WEATHER_OVERCAST                 (132)
#define TW_WEATHER_NEEDLE_ICE               (133)
#define TW_WEATHER_DOWNPOUR                 (134)
#define TW_WEATHER_THUNDERSHOWER_AND_HAIL   (136)
#define TW_WEATHER_FREEZING_RAIN            (137)
#define TW_WEATHER_SNOW_SHOWER              (138)
#define TW_WEATHER_LIGHT_RAIN               (139)
#define TW_WEATHER_HAZE                     (140)
#define TW_WEATHER_MODERATE_RAIN            (141)
#define TW_WEATHER_CLOUDY                   (142)
#define TW_WEATHER_THUNDERSHOWER            (143)
#define TW_WEATHER_MODERATE_TO_HEAVY_RAIN   (144)
#define TW_WEATHER_HEAVY_RAIN_TO_RAINSTORM  (145)
#define TW_WEATHER_CLEAR                    (146)

// Wind direction code
// https://developer.tuya.com/en/docs/mcu-standard-protocol/mcusdk-wifi-weather?id=Kd2fvzw7ny80s#title-15-Appendix%205%3A%20Wind%20direction%20code

/******************************************************************************
 * TYPEDEF
 ******************************************************************************/
typedef struct {
    int weather;
    int temp;
    int humi;
    int real_feel;
    int mbar;
    int uvi;
} WEATHER_CURRENT_CONDITIONS_T;

typedef struct {
    int aqi;
    int quality_level;
    int pm25;
    int pm10;
    int o3;
    int no2;
    int co;
    int so2;
    char rank[64];  // Air quality rank string (China specific)
} WEATHER_CURRENT_AQI_T;

typedef struct {
    int weather_v[7];
    int temp_v[7];
    int humi_v[7];
    int uvi_v[7];
    int mbar_v[7];
} WEATHER_FORECAST_CONDITIONS_T;

typedef struct {
    int weather_v[7];
    int humi_v[7];
    int uvi_v[7];
} WEATHER_FORECAST_CONDITIONS_CN_T;

/******************************************************************************
 * FUNCTION DECLARATIONS
 ******************************************************************************/

/**
 * @brief Retrieves current weather conditions from the Tuya cloud platform.
 *
 * This function retrieves current weather conditions including weather type,
 * temperature, humidity, real feel temperature, atmospheric pressure, and
 * UV index. It performs network and update permission checks before making
 * the API request.
 *
 * @param current_conditions Pointer to WEATHER_CURRENT_CONDITIONS_T structure
 *                          to store the current weather conditions.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_current_conditions(WEATHER_CURRENT_CONDITIONS_T *current_conditions);

/**
 * @brief Retrieves today's high and low temperature from the Tuya cloud platform.
 *
 * This function retrieves the forecasted high and low temperatures for today
 * from the Tuya cloud platform. It performs network and update permission
 * checks before making the API request.
 *
 * @param high_temp Pointer to store the high temperature for today.
 * @param low_temp Pointer to store the low temperature for today.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_today_high_low_temp(int *high_temp, int *low_temp);

/**
 * @brief Retrieves current wind information from the Tuya cloud platform.
 *
 * This function retrieves current wind direction and wind speed from the
 * Tuya cloud platform. It performs network and update permission checks
 * before making the API request.
 *
 * @param wind_dir Pointer to store the wind direction string.
 * @param wind_dir_len Length of the wind direction buffer.
 * @param wind_speed Pointer to store the wind speed string.
 * @param wind_speed_len Length of the wind speed buffer.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_current_wind(char *wind_dir, size_t wind_dir_len, char *wind_speed, size_t wind_speed_len);

/**
 * @brief Retrieves current wind information for China from the Tuya cloud platform.
 *
 * This function retrieves current wind direction, wind speed, and wind level
 * from the Tuya cloud platform, specifically formatted for China weather data.
 * It performs network and update permission checks before making the API request.
 *
 * @param wind_dir Pointer to store the wind direction string.
 * @param wind_dir_len Length of the wind direction buffer.
 * @param wind_speed Pointer to store the wind speed string.
 * @param wind_speed_len Length of the wind speed buffer.
 * @param wind_level Pointer to store the wind level number.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_current_wind_cn(char *wind_dir, size_t wind_dir_len, char *wind_speed, size_t wind_speed_len,
                                     int *wind_level);

/**
 * @brief Retrieves current sunrise and sunset times in GMT from the Tuya cloud platform.
 *
 * This function retrieves current sunrise and sunset times in GMT timezone
 * from the Tuya cloud platform. It performs network and update permission
 * checks before making the API request.
 *
 * @param sunrise Pointer to store the sunrise time string in GMT.
 * @param sunrise_len Length of the sunrise buffer.
 * @param sunset Pointer to store the sunset time string in GMT.
 * @param sunset_len Length of the sunset buffer.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_current_sunrise_sunset_gmt(char *sunrise, size_t sunrise_len, char *sunset, size_t sunset_len);

/**
 * @brief Retrieves current sunrise and sunset times in local timezone from the Tuya cloud platform.
 *
 * This function retrieves current sunrise and sunset times in local timezone
 * from the Tuya cloud platform. It performs network and update permission
 * checks before making the API request.
 *
 * @param sunrise Pointer to store the sunrise time string in local timezone.
 * @param sunrise_len Length of the sunrise buffer.
 * @param sunset Pointer to store the sunset time string in local timezone.
 * @param sunset_len Length of the sunset buffer.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_current_sunrise_sunset_local(char *sunrise, size_t sunrise_len, char *sunset, size_t sunset_len);

/**
 * @brief Retrieves current air quality information from the Tuya cloud platform.
 *
 * This function retrieves current air quality index and related pollutant
 * data from the Tuya cloud platform. It performs network and update
 * permission checks before making the API request.
 *
 * @param current_aqi Pointer to structure to store current air quality data.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_current_aqi(WEATHER_CURRENT_AQI_T *current_aqi);

/**
 * @brief Retrieves current air quality information for China from the Tuya cloud platform.
 *
 * This function retrieves current air quality index and related pollutant
 * data from the Tuya cloud platform, specifically formatted for China
 * weather data. It performs network and update permission checks before
 * making the API request.
 *
 * @param current_aqi Pointer to structure to store current air quality data.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_current_aqi_cn(WEATHER_CURRENT_AQI_T *current_aqi);

/**
 * @brief Retrieves forecast weather conditions from the Tuya cloud platform.
 *
 * This function retrieves forecast weather conditions for the specified
 * number of days from the Tuya cloud platform. It performs network and
 * update permission checks before making the API request.
 *
 * @param number The number of forecast days (1-7).
 * @param forecast_conditions Pointer to structure to store forecast weather data for each day.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_INVALID_PARM: Invalid number of days provided.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_forecast_conditions(int number, WEATHER_FORECAST_CONDITIONS_T *forecast_conditions);

/**
 * @brief Retrieves forecast weather conditions for China from the Tuya cloud platform.
 *
 * This function retrieves forecast weather conditions for the specified
 * number of days from the Tuya cloud platform, specifically formatted
 * for China weather data. It performs network and update permission
 * checks before making the API request.
 *
 * @param number The number of forecast days (1-7).
 * @param weather Array to store weather condition numbers for each day.
 * @param humi Array to store humidity percentages for each day.
 * @param uvi Array to store UV indices for each day.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_INVALID_PARM: Invalid number of days provided.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_forecast_conditions_cn(int number, int *weather, int *humi, int *uvi);

/**
 * @brief Retrieves forecast wind information from the Tuya cloud platform.
 *
 * This function retrieves forecast wind direction and wind speed for the
 * specified number of days from the Tuya cloud platform. It performs
 * network and update permission checks before making the API request.
 *
 * @param number The number of forecast days (1-7).
 * @param wind_dir Array of pointers to store wind direction strings for each day.
 * @param wind_speed Array of pointers to store wind speed strings for each day.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_INVALID_PARM: Invalid number of days provided.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_forecast_wind(int number, char **wind_dir, char **wind_speed);

/**
 * @brief Retrieves forecast high and low temperatures from the Tuya cloud platform.
 *
 * This function retrieves forecast high and low temperatures for the
 * specified number of days from the Tuya cloud platform. It performs
 * network and update permission checks before making the API request.
 *
 * @param number The number of forecast days (1-7).
 * @param high_temp Array to store high temperatures for each day.
 * @param low_temp Array to store low temperatures for each day.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_INVALID_PARM: Invalid number of days provided.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_forecast_high_low_temp(int number, int *high_temp, int *low_temp);

/**
 * @brief Retrieves city information from the Tuya cloud platform.
 *
 * This function retrieves the current city information including province,
 * city, and area from the Tuya cloud platform. It performs network and
 * update permission checks before making the API request.
 *
 * @param province Pointer to store the province name string.
 * @param province_len Length of the province buffer.
 * @param city Pointer to store the city name string.
 * @param city_len Length of the city buffer.
 * @param area Pointer to store the area name string.
 * @param area_len Length of the area buffer.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_city(char *province, size_t province_len, char *city, size_t city_len, char *area,
                          size_t area_len);

/**
 * @brief Checks if weather data update is allowed.
 *
 * This function checks if the Tuya IoT device is activated and the network
 * is connected, which are prerequisites for updating weather data from
 * the cloud platform.
 *
 * @return True if weather data update is allowed, false otherwise.
 */
bool tuya_weather_allow_update(void);

#endif // !__TUYA_WEATHER_H__
