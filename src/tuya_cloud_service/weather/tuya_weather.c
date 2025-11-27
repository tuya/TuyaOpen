/**
 * @file tuya_weather.c
 * @brief Implementation of the Tuya IoT weather service for weather data retrieval.
 *
 * This file contains the implementation of the Tuya IoT weather service, which is used
 * for retrieving weather information from the Tuya cloud platform. It provides functions
 * for getting current weather conditions, forecast data, air quality information, and
 * other weather-related data. The implementation includes support for different weather
 * data types such as temperature, humidity, wind, air quality, and sunrise/sunset times.
 *
 * The weather service includes mechanisms for network connectivity checks, time
 * synchronization, and data parsing from JSON responses. It supports multiple weather
 * data formats and provides both international and China-specific weather data APIs.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_weather.h"

#include "tal_api.h"
#include "tuya_iot.h"

#include "atop_base.h"
#include "cJSON.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define ENABLE_WEATHER_DEBUG      0

#define WEATHER_API              "thing.weather.get"
#define API_VERSION              "1.0"

/**
 * @brief Retrieves weather data from the Tuya cloud platform.
 *
 * This function sends a weather data request to the Tuya cloud platform using
 * the specified weather codes. It performs network connectivity checks, time
 * synchronization, and constructs the appropriate API request with timestamp
 * and authentication data.
 *
 * @param code The weather data codes to request from the cloud platform.
 * @param response Pointer to store the response from the cloud platform.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or network not connected.
 *         - OPRT_MALLOC_FAILED: Memory allocation failed.
 *         - Other error codes: Operation failed.
 */
static OPERATE_RET tuya_weather_get(const char *code, atop_base_response_t *response)
{
    OPERATE_RET rt = OPRT_OK;
    TIME_T timestamp = 0;
    char *post_data = NULL;
    int post_data_len = 0;
    atop_base_request_t atop_request;

    tuya_iot_client_t *client = tuya_iot_client_get();

    rt = tal_time_check_time_sync();
    if (OPRT_OK != rt) {
        PR_ERR("tal_time_check_time_sync error:%d", rt);
        return rt;
    }

    // network check
    if (!client->config.network_check) {
        PR_ERR("network_check is NULL");
        return OPRT_COM_ERROR;
    }
    if (!client->config.network_check()) {
        PR_ERR("network is not connected");
        return OPRT_COM_ERROR;
    }

    timestamp = tal_time_get_posix();

    if (code == NULL) {
        return OPRT_INVALID_PARM;
    }

    post_data_len = snprintf(NULL, 0, "{\"codes\":[%s], \"t\":%d}", code, timestamp);
    post_data_len++; // add '\0'

    post_data = (char *)tal_malloc(post_data_len);
    if (NULL == post_data) {
        return OPRT_MALLOC_FAILED;
    }
    memset(post_data, 0, post_data_len);

    snprintf(post_data, post_data_len, "{\"codes\":[%s], \"t\":%d}", code, timestamp);
    PR_DEBUG("Post: %s", post_data);

    memset(&atop_request, 0, sizeof(atop_base_request_t));
    atop_request.devid = client->activate.devid;
    atop_request.key  = client->activate.seckey;
    atop_request.path = "/d.json";
    atop_request.timestamp = timestamp;
    atop_request.api = WEATHER_API;
    atop_request.version = API_VERSION;
    atop_request.data =  (void *)post_data;
    atop_request.datalen = strlen(post_data);

    rt = atop_base_request(&atop_request, response);
    if (OPRT_OK != rt) {
        PR_ERR("atop_base_request error:%d", rt);
    }

    tal_free(post_data);
    post_data = NULL;

    return rt;
}

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
int tuya_weather_get_current_conditions(WEATHER_CURRENT_CONDITIONS_T *current_conditions)
{
    OPERATE_RET rt = OPRT_OK;
    atop_base_response_t response;

    if (current_conditions == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (!tuya_weather_allow_update()) {
        return OPRT_COM_ERROR;
    }

    const char* request_code = "\"w.conditionNum\",\"w.temp\",\"w.humidity\",\"w.realFeel\",\"w.pressure\",\"w.uvi\",\"w.currdate\"";

    memset(&response, 0, sizeof(atop_base_response_t));

    rt = tuya_weather_get(request_code, &response);
    if (OPRT_OK != rt || !response.success) {
        PR_ERR("tuya_weather_get current conditions error:%d", rt);
        return OPRT_COM_ERROR;
    }

#if ENABLE_WEATHER_DEBUG
    char *result_value = cJSON_PrintUnformatted(response.result);
    PR_DEBUG("result: %s", result_value);
    cJSON_free(result_value);
#endif

    if (cJSON_HasObjectItem(response.result, "data")) {
        cJSON *item  = NULL;
        cJSON *data_obj = cJSON_GetObjectItem(response.result, "data");

        item = cJSON_GetObjectItem(data_obj, "w.conditionNum");
        if (item) {
            char *weather_str = item->valuestring;
            current_conditions->weather = atoi(weather_str);
        }

        item = cJSON_GetObjectItem(data_obj, "w.temp");
        if (item) {
            current_conditions->temp = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.humidity");
        if (item) {
            current_conditions->humi = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.realFeel");
        if (item) {
            current_conditions->real_feel = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.pressure");
        if (item) {
            current_conditions->mbar = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.uvi");
        if (item) {
            current_conditions->uvi = item->valueint;
        }
    }

    atop_base_response_free(&response);

    return rt;
}

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
int tuya_weather_get_today_high_low_temp(int *high_temp, int *low_temp)
{
    OPERATE_RET rt = OPRT_OK;
    atop_base_response_t response;

    if (high_temp == NULL || low_temp == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (!tuya_weather_allow_update()) {
        return OPRT_COM_ERROR;
    }

    const char* request_code = "\"w.thigh\",\"w.tlow\",\"w.date.1\"";

    memset(&response, 0, sizeof(atop_base_response_t));

    rt = tuya_weather_get(request_code, &response);
    if (OPRT_OK != rt || !response.success) {
        PR_ERR("tuya_weather_get today high low temp error:%d", rt);
        return OPRT_COM_ERROR;
    }

#if ENABLE_WEATHER_DEBUG
    char *result_value = cJSON_PrintUnformatted(response.result);
    PR_DEBUG("result: %s", result_value);
    cJSON_free(result_value);
#endif

    if (cJSON_HasObjectItem(response.result, "data")) {
        cJSON *item  = NULL;
        cJSON *data_obj = cJSON_GetObjectItem(response.result, "data");

        item = cJSON_GetObjectItem(data_obj, "w.thigh.0");
        if (item) {
            *high_temp = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.tlow.0");
        if (item) {
            *low_temp = item->valueint;
        }
    }

    atop_base_response_free(&response);

    return rt;
}

/**
 * @brief Retrieves current wind information from the Tuya cloud platform.
 *
 * This function retrieves current wind direction and wind speed from the
 * Tuya cloud platform. It performs network and update permission checks
 * before making the API request.
 *
 * @param wind_dir Pointer to store the wind direction string.
 * @param wind_speed Pointer to store the wind speed string.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_current_wind(char *wind_dir, size_t wind_dir_len, char *wind_speed, size_t wind_speed_len)
{
    OPERATE_RET rt = OPRT_OK;
    atop_base_response_t response;

    if (wind_dir == NULL || wind_speed == NULL || wind_dir_len == 0 || wind_speed_len == 0) {
        return OPRT_INVALID_PARM;
    }

    if (!tuya_weather_allow_update()) {
        return OPRT_COM_ERROR;
    }

    const char* request_code = "\"w.windDir\",\"w.windSpeed\",\"w.currdate\"";

    memset(&response, 0, sizeof(atop_base_response_t));

    rt = tuya_weather_get(request_code, &response);
    if (OPRT_OK != rt || !response.success) {
        PR_ERR("tuya_weather_get today high low temp error:%d", rt);
        return OPRT_COM_ERROR;
    }

#if ENABLE_WEATHER_DEBUG
    char *result_value = cJSON_PrintUnformatted(response.result);
    PR_DEBUG("result: %s", result_value);
    cJSON_free(result_value);
#endif

    if (cJSON_HasObjectItem(response.result, "data")) {
        cJSON *item  = NULL;
        cJSON *data_obj = cJSON_GetObjectItem(response.result, "data");

        item = cJSON_GetObjectItem(data_obj, "w.windDir");
        if (item) {
            int ret = snprintf(wind_dir, wind_dir_len, "%s", item->valuestring ? item->valuestring : "");
            if (ret < 0 || ret >= (int)wind_dir_len) {
                wind_dir[wind_dir_len - 1] = '\0';
            }
        }

        item = cJSON_GetObjectItem(data_obj, "w.windSpeed");
        if (item) {
            int ret = snprintf(wind_speed, wind_speed_len, "%s", item->valuestring ? item->valuestring : "");
            if (ret < 0 || ret >= (int)wind_speed_len) {
                wind_speed[wind_speed_len - 1] = '\0';
            }
        }
    }

    atop_base_response_free(&response);

    return rt;
}

/**
 * @brief Retrieves current wind information for China from the Tuya cloud platform.
 *
 * This function retrieves current wind direction, wind speed, and wind level
 * from the Tuya cloud platform, specifically formatted for China weather data.
 * It performs network and update permission checks before making the API request.
 *
 * @param wind_dir Pointer to store the wind direction string.
 * @param wind_speed Pointer to store the wind speed string.
 * @param wind_level Pointer to store the wind level number.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_current_wind_cn(char *wind_dir, size_t wind_dir_len, char *wind_speed, size_t wind_speed_len,
                                     int *wind_level)
{
    OPERATE_RET rt = OPRT_OK;
    atop_base_response_t response;

    if (wind_dir == NULL || wind_speed == NULL || wind_level == NULL || wind_dir_len == 0 || wind_speed_len == 0) {
        return OPRT_INVALID_PARM;
    }

    if (!tuya_weather_allow_update()) {
        return OPRT_COM_ERROR;
    }

    const char* request_code = "\"w.windDir\",\"w.windSpeed\",\"w.windLevel\",\"w.currdate\"";

    memset(&response, 0, sizeof(atop_base_response_t));

    rt = tuya_weather_get(request_code, &response);
    if (OPRT_OK != rt || !response.success) {
        PR_ERR("tuya_weather_get today high low temp error:%d", rt);
        return OPRT_COM_ERROR;
    }

#if ENABLE_WEATHER_DEBUG
    char *result_value = cJSON_PrintUnformatted(response.result);
    PR_DEBUG("result: %s", result_value);
    cJSON_free(result_value);
#endif

    if (cJSON_HasObjectItem(response.result, "data")) {
        cJSON *item  = NULL;
        cJSON *data_obj = cJSON_GetObjectItem(response.result, "data");

        item = cJSON_GetObjectItem(data_obj, "w.windDir");
        if (item) {
            int ret = snprintf(wind_dir, wind_dir_len, "%s", item->valuestring ? item->valuestring : "");
            if (ret < 0 || ret >= (int)wind_dir_len) {
                wind_dir[wind_dir_len - 1] = '\0';
            }
        }

        item = cJSON_GetObjectItem(data_obj, "w.windSpeed");
        if (item) {
            int ret = snprintf(wind_speed, wind_speed_len, "%s", item->valuestring ? item->valuestring : "");
            if (ret < 0 || ret >= (int)wind_speed_len) {
                wind_speed[wind_speed_len - 1] = '\0';
            }
        }

        item = cJSON_GetObjectItem(data_obj, "w.windLevel");
        if (item) {
            *wind_level = item->valueint;
        }
    }

    atop_base_response_free(&response);

    return rt;
}

/**
 * @brief Retrieves current sunrise and sunset times in GMT from the Tuya cloud platform.
 *
 * This function retrieves current sunrise and sunset times in GMT timezone
 * from the Tuya cloud platform. It performs network and update permission
 * checks before making the API request.
 *
 * @param sunrise Pointer to store the sunrise time string in GMT.
 * @param sunset Pointer to store the sunset time string in GMT.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_current_sunrise_sunset_gmt(char *sunrise, size_t sunrise_len, char *sunset, size_t sunset_len)
{
    OPERATE_RET rt = OPRT_OK;
    atop_base_response_t response;

    if (sunrise == NULL || sunset == NULL || sunrise_len == 0 || sunset_len == 0) {
        return OPRT_INVALID_PARM;
    }

    if (!tuya_weather_allow_update()) {
        return OPRT_COM_ERROR;
    }

    const char* request_code = "\"w.sunrise\",\"w.sunset\",\"t.unix\",\"w.currdate\"";

    memset(&response, 0, sizeof(atop_base_response_t));

    rt = tuya_weather_get(request_code, &response);
    if (OPRT_OK != rt || !response.success) {
        PR_ERR("tuya_weather_get today high low temp error:%d", rt);
        return OPRT_COM_ERROR;
    }

#if ENABLE_WEATHER_DEBUG
    char *result_value = cJSON_PrintUnformatted(response.result);
    PR_DEBUG("result: %s", result_value);
    cJSON_free(result_value);
#endif

    if (cJSON_HasObjectItem(response.result, "data")) {
        cJSON *item  = NULL;
        cJSON *data_obj = cJSON_GetObjectItem(response.result, "data");

        item = cJSON_GetObjectItem(data_obj, "w.sunrise");
        if (item) {
            int ret = snprintf(sunrise, sunrise_len, "%s", item->valuestring ? item->valuestring : "");
            if (ret < 0 || ret >= (int)sunrise_len) {
                sunrise[sunrise_len - 1] = '\0';
            }
        }

        item = cJSON_GetObjectItem(data_obj, "w.sunset");
        if (item) {
            int ret = snprintf(sunset, sunset_len, "%s", item->valuestring ? item->valuestring : "");
            if (ret < 0 || ret >= (int)sunset_len) {
                sunset[sunset_len - 1] = '\0';
            }
        }
    }

    atop_base_response_free(&response);

    return rt;
}

/**
 * @brief Retrieves current sunrise and sunset times in local timezone from the Tuya cloud platform.
 *
 * This function retrieves current sunrise and sunset times in local timezone
 * from the Tuya cloud platform. It performs network and update permission
 * checks before making the API request.
 *
 * @param sunrise Pointer to store the sunrise time string in local timezone.
 * @param sunset Pointer to store the sunset time string in local timezone.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_current_sunrise_sunset_local(char *sunrise, size_t sunrise_len, char *sunset, size_t sunset_len)
{
    OPERATE_RET rt = OPRT_OK;
    atop_base_response_t response;

    if (sunrise == NULL || sunset == NULL || sunrise_len == 0 || sunset_len == 0) {
        return OPRT_INVALID_PARM;
    }

    if (!tuya_weather_allow_update()) {
        return OPRT_COM_ERROR;
    }

    const char* request_code = "\"w.sunrise\",\"w.sunset\",\"t.local\",\"w.currdate\"";

    memset(&response, 0, sizeof(atop_base_response_t));

    rt = tuya_weather_get(request_code, &response);
    if (OPRT_OK != rt || !response.success) {
        PR_ERR("tuya_weather_get today high low temp error:%d", rt);
        return OPRT_COM_ERROR;
    }

#if ENABLE_WEATHER_DEBUG
    char *result_value = cJSON_PrintUnformatted(response.result);
    PR_DEBUG("result: %s", result_value);
    cJSON_free(result_value);
#endif

    if (cJSON_HasObjectItem(response.result, "data")) {
        cJSON *item  = NULL;
        cJSON *data_obj = cJSON_GetObjectItem(response.result, "data");

        item = cJSON_GetObjectItem(data_obj, "w.sunrise");
        if (item) {
            int ret = snprintf(sunrise, sunrise_len, "%s", item->valuestring ? item->valuestring : "");
            if (ret < 0 || ret >= (int)sunrise_len) {
                sunrise[sunrise_len - 1] = '\0';
            }
        }

        item = cJSON_GetObjectItem(data_obj, "w.sunset");
        if (item) {
            int ret = snprintf(sunset, sunset_len, "%s", item->valuestring ? item->valuestring : "");
            if (ret < 0 || ret >= (int)sunset_len) {
                sunset[sunset_len - 1] = '\0';
            }
        }
    }

    atop_base_response_free(&response);

    return rt;
}

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
int tuya_weather_get_current_aqi(WEATHER_CURRENT_AQI_T *current_aqi)
{
    OPERATE_RET rt = OPRT_OK;
    atop_base_response_t response;

    if (current_aqi == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (!tuya_weather_allow_update()) {
        return OPRT_COM_ERROR;
    }

    const char* request_code = "\"w.aqi\",\"w.qualityLevel\",\"w.pm25\",\"w.pm10\",\"w.o3\",\"w.no2\",\"w.co\",\"w.so2\",\"w.currdate\"";

    memset(&response, 0, sizeof(atop_base_response_t));

    rt = tuya_weather_get(request_code, &response);
    if (OPRT_OK != rt || !response.success) {
        PR_ERR("tuya_weather_get today high low temp error:%d", rt);
        return OPRT_COM_ERROR;
    }

#if ENABLE_WEATHER_DEBUG
    char *result_value = cJSON_PrintUnformatted(response.result);
    PR_DEBUG("result: %s", result_value);
    cJSON_free(result_value);
#endif

    if (cJSON_HasObjectItem(response.result, "data")) {
        cJSON *item  = NULL;
        cJSON *data_obj = cJSON_GetObjectItem(response.result, "data");

        item = cJSON_GetObjectItem(data_obj, "w.aqi");
        if (item) {
            current_aqi->aqi = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.qualityLevel");
        if (item) {
            current_aqi->quality_level = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.pm25");
        if (item) {
            current_aqi->pm25 = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.pm10");
        if (item) {
            current_aqi->pm10 = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.o3");
        if (item) {
            current_aqi->o3 = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.no2");
        if (item) {
            current_aqi->no2 = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.co");
        if (item) {
            current_aqi->co = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.so2");
        if (item) {
            current_aqi->so2 = item->valueint;
        }
    }

    atop_base_response_free(&response);

    return rt;
}

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
int tuya_weather_get_current_aqi_cn(WEATHER_CURRENT_AQI_T *current_aqi)
{
    OPERATE_RET rt = OPRT_OK;
    atop_base_response_t response;

    if (current_aqi == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (!tuya_weather_allow_update()) {
        return OPRT_COM_ERROR;
    }

    const char* request_code = "\"w.aqi\",\"w.rank\",\"w.qualityLevel\",\"w.pm25\",\"w.pm10\",\"w.o3\",\"w.no2\",\"w.co\",\"w.so2\",\"w.currdate\"";

    memset(&response, 0, sizeof(atop_base_response_t));

    rt = tuya_weather_get(request_code, &response);
    if (OPRT_OK != rt || !response.success) {
        PR_ERR("tuya_weather_get today high low temp error:%d", rt);
        return OPRT_COM_ERROR;
    }

#if ENABLE_WEATHER_DEBUG
    char *result_value = cJSON_PrintUnformatted(response.result);
    PR_DEBUG("result: %s", result_value);
    cJSON_free(result_value);
#endif

    if (cJSON_HasObjectItem(response.result, "data")) {
        cJSON *item  = NULL;
        cJSON *data_obj = cJSON_GetObjectItem(response.result, "data");

        item = cJSON_GetObjectItem(data_obj, "w.aqi");
        if (item) {
            current_aqi->aqi = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.rank");
        if (item) {
            int ret =
                snprintf(current_aqi->rank, sizeof(current_aqi->rank), "%s", item->valuestring ? item->valuestring : "");
            if (ret < 0 || ret >= (int)sizeof(current_aqi->rank)) {
                current_aqi->rank[sizeof(current_aqi->rank) - 1] = '\0';
            }
        }

        item = cJSON_GetObjectItem(data_obj, "w.qualityLevel");
        if (item) {
            current_aqi->quality_level = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.pm25");
        if (item) {
            current_aqi->pm25 = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.pm10");
        if (item) {
            current_aqi->pm10 = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.o3");
        if (item) {
            current_aqi->o3 = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.no2");
        if (item) {
            current_aqi->no2 = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.co");
        if (item) {
            current_aqi->co = item->valueint;
        }

        item = cJSON_GetObjectItem(data_obj, "w.so2");
        if (item) {
            current_aqi->so2 = item->valueint;
        }
    }

    atop_base_response_free(&response);

    return rt;
}

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
int tuya_weather_get_forecast_conditions(int number, WEATHER_FORECAST_CONDITIONS_T *forecast_conditions)
{
    OPERATE_RET rt = OPRT_OK;
    atop_base_response_t response;

    if (number < 1 || number > 7 || forecast_conditions == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (!tuya_weather_allow_update()) {
        return OPRT_COM_ERROR;
    }

    char request_code[80] = {0};
    snprintf(request_code, sizeof(request_code), "\"w.conditionNum\",\"w.humidity\",\"w.temp\",\"w.uvi\",\"w.pressure\",\"w.date.%d\"", number);

    memset(&response, 0, sizeof(atop_base_response_t));

    rt = tuya_weather_get(request_code, &response);
    if (OPRT_OK != rt || !response.success) {
        PR_ERR("tuya_weather_get today high low temp error:%d", rt);
        return OPRT_COM_ERROR;
    }

#if ENABLE_WEATHER_DEBUG
    char *result_value = cJSON_PrintUnformatted(response.result);
    PR_DEBUG("result: %s", result_value);
    cJSON_free(result_value);
#endif

    if (cJSON_HasObjectItem(response.result, "data")) {
        cJSON *item  = NULL;
        cJSON *data_obj = cJSON_GetObjectItem(response.result, "data");

        for (int i = 0; i < number; i++) {
            char key_name[64];
            
            snprintf(key_name, sizeof(key_name), "w.conditionNum.%d", i);
            item = cJSON_GetObjectItem(data_obj, key_name);
            if (item) {
                char *weather_str = item->valuestring;
                forecast_conditions->weather_v[i] = atoi(weather_str);
            }

            snprintf(key_name, sizeof(key_name), "w.temp.%d", i);
            item = cJSON_GetObjectItem(data_obj, key_name);
            if (item) {
                forecast_conditions->temp_v[i] = item->valueint;
            } else {
                forecast_conditions->temp_v[i] = 0; // Not support forecast temperature in Mainland China
            }

            snprintf(key_name, sizeof(key_name), "w.pressure.%d", i);
            item = cJSON_GetObjectItem(data_obj, key_name);
            if (item) {
                forecast_conditions->mbar_v[i] = item->valueint;
            } else {
                forecast_conditions->mbar_v[i] = 0; // Not support forecast atmospheric pressure in Mainland China
            }

            snprintf(key_name, sizeof(key_name), "w.humidity.%d", i);
            item = cJSON_GetObjectItem(data_obj, key_name);
            if (item) {
                forecast_conditions->humi_v[i] = item->valueint;
            }

            snprintf(key_name, sizeof(key_name), "w.uvi.%d", i);
            item = cJSON_GetObjectItem(data_obj, key_name);
            if (item) {
                forecast_conditions->uvi_v[i] = item->valueint;
            }
        }
    }

    atop_base_response_free(&response);

    return rt;
}

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
int tuya_weather_get_forecast_conditions_cn(int number, int *weather, int *humi, int *uvi)
{
    OPERATE_RET rt = OPRT_OK;
    atop_base_response_t response;

    if (number < 1 || number > 7 || weather == NULL || humi == NULL || uvi == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (!tuya_weather_allow_update()) {
        return OPRT_COM_ERROR;
    }

    char request_code[80] = {0};
    snprintf(request_code, sizeof(request_code), "\"w.conditionNum\",\"w.humidity\",\"w.uvi\",\"w.date.%d\"", number);

    memset(&response, 0, sizeof(atop_base_response_t));

    rt = tuya_weather_get(request_code, &response);
    if (OPRT_OK != rt || !response.success) {
        PR_ERR("tuya_weather_get today high low temp error:%d", rt);
        return OPRT_COM_ERROR;
    }

#if ENABLE_WEATHER_DEBUG
    char *result_value = cJSON_PrintUnformatted(response.result);
    PR_DEBUG("result: %s", result_value);
    cJSON_free(result_value);
#endif

    if (cJSON_HasObjectItem(response.result, "data")) {
        cJSON *item  = NULL;
        cJSON *data_obj = cJSON_GetObjectItem(response.result, "data");

        for (int i = 0; i < number; i++) {
            char key_name[64];
            
            snprintf(key_name, sizeof(key_name), "w.conditionNum.%d", i);
            item = cJSON_GetObjectItem(data_obj, key_name);
            if (item) {
                char *weather_str = item->valuestring;
                weather[i] = atoi(weather_str);
            }

            snprintf(key_name, sizeof(key_name), "w.humidity.%d", i);
            item = cJSON_GetObjectItem(data_obj, key_name);
            if (item) {
                humi[i] = item->valueint;
            }

            snprintf(key_name, sizeof(key_name), "w.uvi.%d", i);
            item = cJSON_GetObjectItem(data_obj, key_name);
            if (item) {
                uvi[i] = item->valueint;
            }
        }
    }

    atop_base_response_free(&response);

    return rt;
}

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
int tuya_weather_get_forecast_wind(int number, char **wind_dir, char **wind_speed)
{
    OPERATE_RET rt = OPRT_OK;
    atop_base_response_t response;

    if (number < 1 || number > 7 || wind_dir == NULL || wind_speed == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (!tuya_weather_allow_update()) {
        return OPRT_COM_ERROR;
    }

    char request_code[80] = {0};
    snprintf(request_code, sizeof(request_code), "\"w.windDir\",\"w.windSpeed\",\"w.date.%d\"", number);

    memset(&response, 0, sizeof(atop_base_response_t));

    rt = tuya_weather_get(request_code, &response);
    if (OPRT_OK != rt || !response.success) {
        PR_ERR("tuya_weather_get today high low temp error:%d", rt);
        return OPRT_COM_ERROR;
    }

#if ENABLE_WEATHER_DEBUG
    char *result_value = cJSON_PrintUnformatted(response.result);
    PR_DEBUG("result: %s", result_value);
    cJSON_free(result_value);
#endif

    if (cJSON_HasObjectItem(response.result, "data")) {
        cJSON *item  = NULL;
        cJSON *data_obj = cJSON_GetObjectItem(response.result, "data");

        for (int i = 0; i < number; i++) {
            char key_name[64];
            
            snprintf(key_name, sizeof(key_name), "w.windDir.%d", i);
            item = cJSON_GetObjectItem(data_obj, key_name);
            if (item && item->valuestring) {
                // wind_dir[i] = strdup(item->valuestring);
                wind_dir[i] = tal_malloc(strlen(item->valuestring) + 1);
                if (wind_dir[i] == NULL) {
                    PR_ERR("malloc wind_dir[%d] failed", i);
                    return OPRT_COM_ERROR;
                }
                size_t value_len = strlen(item->valuestring);
                memset(wind_dir[i], 0, value_len + 1);
                memcpy(wind_dir[i], item->valuestring, value_len);
            } else {
                wind_dir[i] = NULL;
            }

            snprintf(key_name, sizeof(key_name), "w.windSpeed.%d", i);
            item = cJSON_GetObjectItem(data_obj, key_name);
            if (item && item->valuestring) {
                // wind_speed[i] = strdup(item->valuestring);
                wind_speed[i] = tal_malloc(strlen(item->valuestring) + 1);
                if (wind_speed[i] == NULL) {
                    PR_ERR("malloc wind_speed[%d] failed", i);
                    if (wind_dir[i]) {
                        free(wind_dir[i]);
                        wind_dir[i] = NULL;
                    }
                    return OPRT_COM_ERROR;
                }
                size_t value_len = strlen(item->valuestring);
                memset(wind_speed[i], 0, value_len + 1);
                memcpy(wind_speed[i], item->valuestring, value_len);
            } else {
                wind_speed[i] = NULL;
            }
        }
    }

    atop_base_response_free(&response);

    return rt;
}

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
int tuya_weather_get_forecast_high_low_temp(int number, int *high_temp, int *low_temp)
{
    OPERATE_RET rt = OPRT_OK;
    atop_base_response_t response;

    if (number < 1 || number > 7 || high_temp == NULL || low_temp == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (!tuya_weather_allow_update()) {
        return OPRT_COM_ERROR;
    }

    char request_code[80] = {0};
    snprintf(request_code, sizeof(request_code), "\"w.thigh\",\"w.tlow\",\"w.date.%d\"", number);

    memset(&response, 0, sizeof(atop_base_response_t));

    rt = tuya_weather_get(request_code, &response);
    if (OPRT_OK != rt || !response.success) {
        PR_ERR("tuya_weather_get today high low temp error:%d", rt);
        return OPRT_COM_ERROR;
    }

#if ENABLE_WEATHER_DEBUG
    char *result_value = cJSON_PrintUnformatted(response.result);
    PR_DEBUG("result: %s", result_value);
    cJSON_free(result_value);
#endif

    if (cJSON_HasObjectItem(response.result, "data")) {
        cJSON *item  = NULL;
        cJSON *data_obj = cJSON_GetObjectItem(response.result, "data");

        for (int i = 0; i < number; i++) {
            char key_name[64];
            
            snprintf(key_name, sizeof(key_name), "w.thigh.%d", i);
            item = cJSON_GetObjectItem(data_obj, key_name);
            if (item) {
                high_temp[i] = item->valueint;
            }

            snprintf(key_name, sizeof(key_name), "w.tlow.%d", i);
            item = cJSON_GetObjectItem(data_obj, key_name);
            if (item) {
                low_temp[i] = item->valueint;
            }
        }
    }

    atop_base_response_free(&response);

    return rt;
}

/**
 * @brief Retrieves city information from the Tuya cloud platform.
 *
 * This function retrieves the current city information including province,
 * city, and area from the Tuya cloud platform. It performs network and
 * update permission checks before making the API request.
 *
 * @param province Pointer to store the province name string.
 * @param city Pointer to store the city name string.
 * @param area Pointer to store the area name string.
 *
 * @return The operation result status. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_COM_ERROR: Communication error or update not allowed.
 *         - Other error codes: Operation failed.
 */
int tuya_weather_get_city(char *province, size_t province_len, char *city, size_t city_len, char *area,
                          size_t area_len)
{
    OPERATE_RET rt = OPRT_OK;
    atop_base_response_t response;

    if (province == NULL || city == NULL || area == NULL || province_len == 0 || city_len == 0 || area_len == 0) {
        return OPRT_INVALID_PARM;
    }

    if (!tuya_weather_allow_update()) {
        return OPRT_COM_ERROR;
    }

    const char* request_code = "\"c.province\",\"c.city\",\"c.area\"";

    memset(&response, 0, sizeof(atop_base_response_t));

    rt = tuya_weather_get(request_code, &response);
    if (OPRT_OK != rt || !response.success) {
        PR_ERR("tuya_weather_get today high low temp error:%d", rt);
        return OPRT_COM_ERROR;
    }

#if ENABLE_WEATHER_DEBUG
    char *result_value = cJSON_PrintUnformatted(response.result);
    PR_DEBUG("result: %s", result_value);
    cJSON_free(result_value);
#endif

    if (cJSON_HasObjectItem(response.result, "data")) {
        cJSON *item  = NULL;
        cJSON *data_obj = cJSON_GetObjectItem(response.result, "data");

        item = cJSON_GetObjectItem(data_obj, "c.province");
        if (item) {
            int ret = snprintf(province, province_len, "%s", item->valuestring ? item->valuestring : "");
            if (ret < 0 || ret >= (int)province_len) {
                province[province_len - 1] = '\0';
            }
        }

        item = cJSON_GetObjectItem(data_obj, "c.city");
        if (item) {
            int ret = snprintf(city, city_len, "%s", item->valuestring ? item->valuestring : "");
            if (ret < 0 || ret >= (int)city_len) {
                city[city_len - 1] = '\0';
            }
        }

        item = cJSON_GetObjectItem(data_obj, "c.area");
        if (item) {
            int ret = snprintf(area, area_len, "%s", item->valuestring ? item->valuestring : "");
            if (ret < 0 || ret >= (int)area_len) {
                area[area_len - 1] = '\0';
            }
        }
    }

    atop_base_response_free(&response);

    return rt;
}

/******************************************************************************
 * PRIVATE MEMBER FUNCTIONS
 ******************************************************************************/
/**
 * @brief Checks if weather data update is allowed.
 *
 * This function checks if the Tuya IoT device is activated and the network
 * is connected, which are prerequisites for updating weather data from
 * the cloud platform.
 *
 * @return True if weather data update is allowed, false otherwise.
 */
bool tuya_weather_allow_update(void)
{
    tuya_iot_client_t *client = tuya_iot_client_get();
    if (false == tuya_iot_activated(client)) {
        return false;
    }

    // check network status
    if (client->config.network_check && client->config.network_check()) {
        return true;
    } else {
        return false;
    }
}
