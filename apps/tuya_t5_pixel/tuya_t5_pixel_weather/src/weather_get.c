/**
 * @file weather_get.c
 * @brief Demonstrates retrieving weather information via the Tuya Cloud Weather Service
 *
 * This example implements weather querying features, including fetching current
 * conditions, today's high/low temperatures, wind direction and speed, sunrise
 * and sunset (both GMT and local time), air quality, multi-day forecasts, and
 * city information. Mainland China specific APIs are also demonstrated.
 * The example is implemented using Tuya weather-related APIs.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tuya_weather.h"

// void weather_get(int argc, char *argv[])
void weather_get_workqueue_cb(void *data)
{
    OPERATE_RET rt = OPRT_OK;

    if (false == tuya_weather_allow_update()) {
        return;
    }

    /* Get current weather conditions */
    WEATHER_CURRENT_CONDITIONS_T current_conditions = {0};
    rt = tuya_weather_get_current_conditions(&current_conditions);
    if (OPRT_OK != rt) {
        PR_ERR("get current conditions failed: %d", rt);
        return;
    }

    PR_DEBUG_RAW("----------current conditions----------\n");
    PR_DEBUG_RAW("weather: %d\n", current_conditions.weather);
    PR_DEBUG_RAW("temp: %d\n", current_conditions.temp);
    PR_DEBUG_RAW("humi: %d\n", current_conditions.humi);
    PR_DEBUG_RAW("real_feel: %d\n", current_conditions.real_feel);
    PR_DEBUG_RAW("mbar: %d\n", current_conditions.mbar);
    PR_DEBUG_RAW("uvi: %d\n", current_conditions.uvi);

    /* Get today's high and low temperature */
    int today_high = 0, today_low = 0;
    rt = tuya_weather_get_today_high_low_temp(&today_high, &today_low);
    if (OPRT_OK != rt) {
        PR_ERR("get today high low temp failed: %d", rt);
        return;
    }

    PR_DEBUG_RAW("----------today high low temp----------\n");
    PR_DEBUG_RAW("today_high: %d\n", today_high);
    PR_DEBUG_RAW("today_low: %d\n", today_low);

    /* Get current wind */
    char wind_dir[64] = {0}, wind_speed[64] = {0};
    rt = tuya_weather_get_current_wind(wind_dir, wind_speed);
    if (OPRT_OK != rt) {
        PR_ERR("get current wind failed: %d", rt);
        return;
    }

    PR_DEBUG_RAW("----------current wind----------\n");
    PR_DEBUG_RAW("wind_dir: %s\n", wind_dir);
    PR_DEBUG_RAW("wind_speed: %s\n", wind_speed);

    /* Get current wind for China */
    int wind_level = 0;
    rt = tuya_weather_get_current_wind_cn(wind_dir, wind_speed, &wind_level); // wind_level only support Mainland China
    if (OPRT_OK != rt) {
        PR_ERR("get current wind cn failed: %d", rt);
        return;
    }

    PR_DEBUG_RAW("----------current wind cn----------\n");
    PR_DEBUG_RAW("wind_dir: %s\n", wind_dir);
    PR_DEBUG_RAW("wind_speed: %s\n", wind_speed);
    PR_DEBUG_RAW("wind_level: %d\n", wind_level);

    /* Get current sunrise and sunset */
    char sunrise[64] = {0};
    char sunset[64] = {0};
    rt = tuya_weather_get_current_sunrise_sunset_gmt(sunrise, sunset);
    if (OPRT_OK != rt) {
        PR_ERR("get current sunrise sunset gmt failed: %d", rt);
        return;
    }

    PR_DEBUG_RAW("----------current sunrise sunset gmt----------\n");
    PR_DEBUG_RAW("sunrise: %s\n", sunrise);
    PR_DEBUG_RAW("sunset: %s\n", sunset);

    /* Get current sunrise and sunset in local timezone */
    rt = tuya_weather_get_current_sunrise_sunset_local(sunrise, sunset);
    if (OPRT_OK != rt) {
        PR_ERR("get current sunrise sunset local failed: %d", rt);
        return;
    }

    PR_DEBUG_RAW("----------current sunrise sunset local----------\n");
    PR_DEBUG_RAW("sunrise: %s\n", sunrise);
    PR_DEBUG_RAW("sunset: %s\n", sunset);

    /* Get current air quality */
    WEATHER_CURRENT_AQI_T current_aqi = {0};
    rt = tuya_weather_get_current_aqi(&current_aqi);
    if (OPRT_OK != rt) {
        PR_ERR("get current aqi failed: %d", rt);
        return;
    }

    PR_DEBUG_RAW("----------current aqi----------\n");
    PR_DEBUG_RAW("aqi: %d\n", current_aqi.aqi);
    PR_DEBUG_RAW("quality_level: %d\n", current_aqi.quality_level);
    PR_DEBUG_RAW("pm25: %d\n", current_aqi.pm25);
    PR_DEBUG_RAW("pm10: %d\n", current_aqi.pm10);
    PR_DEBUG_RAW("o3: %d\n", current_aqi.o3);
    PR_DEBUG_RAW("no2: %d\n", current_aqi.no2);
    PR_DEBUG_RAW("co: %d\n", current_aqi.co);
    PR_DEBUG_RAW("so2: %d\n", current_aqi.so2);

    /* Get current air quality for China */
    WEATHER_CURRENT_AQI_T current_aqi_cn = {0};
    rt = tuya_weather_get_current_aqi_cn(&current_aqi_cn); // rank only support Mainland China
    if (OPRT_OK != rt) {
        PR_ERR("get current aqi cn failed: %d", rt);
        return;
    }

    PR_DEBUG_RAW("----------current aqi cn----------\n");
    PR_DEBUG_RAW("aqi: %d\n", current_aqi_cn.aqi);
    PR_DEBUG_RAW("rank: %s\n", current_aqi_cn.rank);
    PR_DEBUG_RAW("quality_level: %d\n", current_aqi_cn.quality_level);
    PR_DEBUG_RAW("pm25: %d\n", current_aqi_cn.pm25);
    PR_DEBUG_RAW("pm10: %d\n", current_aqi_cn.pm10);
    PR_DEBUG_RAW("o3: %d\n", current_aqi_cn.o3);
    PR_DEBUG_RAW("no2: %d\n", current_aqi_cn.no2);
    PR_DEBUG_RAW("co: %d\n", current_aqi_cn.co);
    PR_DEBUG_RAW("so2: %d\n", current_aqi_cn.so2);

    /* Get forecast weather */
    int number = 7;
    WEATHER_FORECAST_CONDITIONS_T forecast_conditions = {0};

    /* temp, pressure not support forecast in Mainland China
    please use tuya_weather_get_forecast_conditions_cn() to get forecast weather in Mainland China */
    rt = tuya_weather_get_forecast_conditions(number, &forecast_conditions);
    if (OPRT_OK != rt) {
        PR_ERR("get forecast conditions failed: %d", rt);
        return;
    }

    PR_DEBUG_RAW("----------forecast weather----------\n");
    for (int i = 0; i < number; i++) {
        PR_DEBUG_RAW("weather[%d]: %d\n", i, forecast_conditions.weather_v[i]);
        PR_DEBUG_RAW("temp[%d]: %d\n", i, forecast_conditions.temp_v[i]);
        PR_DEBUG_RAW("humi[%d]: %d\n", i, forecast_conditions.humi_v[i]);
        PR_DEBUG_RAW("uvi[%d]: %d\n", i, forecast_conditions.uvi_v[i]);
        PR_DEBUG_RAW("mbar[%d]: %d\n", i, forecast_conditions.mbar_v[i]);
    }

    /* Get forecast weather for China */
    number = 7;
    int weather_v[7] = {0}, humi_v[7] = {0}, uvi_v[7] = {0};
    rt = tuya_weather_get_forecast_conditions_cn(number, weather_v, humi_v, uvi_v);
    if (OPRT_OK != rt) {
        PR_ERR("get forecast conditions cn failed: %d", rt);
        return;
    }

    PR_DEBUG_RAW("----------forecast weather cn----------\n");
    for (int i = 0; i < number; i++) {
        PR_DEBUG_RAW("weather[%d]: %d\n", i, weather_v[i]);
        PR_DEBUG_RAW("humi[%d]: %d\n", i, humi_v[i]);
        PR_DEBUG_RAW("uvi[%d]: %d\n", i, uvi_v[i]);
    }

    /* Get forecast wind */
    char *wind_dir_v[7] = {NULL};
    char *wind_speed_v[7] = {NULL};
    number = 7;
    rt = tuya_weather_get_forecast_wind(number, wind_dir_v, wind_speed_v);
    if (OPRT_OK != rt) {
        PR_ERR("get forecast wind failed: %d", rt);
        return;
    }

    PR_DEBUG_RAW("----------forecast wind----------\n");
    for (int i = 0; i < number; i++) {
        PR_DEBUG_RAW("wind_dir[%d]: %s\n", i, wind_dir_v[i] ? wind_dir_v[i] : "N/A");
        PR_DEBUG_RAW("wind_speed[%d]: %s\n", i, wind_speed_v[i] ? wind_speed_v[i] : "N/A");
    }

    /* free dynamic allocated memory */
    for (int i = 0; i < number; i++) {
        if (wind_dir_v[i]) {
            tal_free(wind_dir_v[i]);
            wind_dir_v[i] = NULL;
        }
        if (wind_speed_v[i]) {
            tal_free(wind_speed_v[i]);
            wind_speed_v[i] = NULL;
        }
    }

    /* Get forecast high and low temperature */
    int high_temp_v[7] = {0}, low_temp_v[7] = {0};
    number = 7;
    rt = tuya_weather_get_forecast_high_low_temp(number, high_temp_v, low_temp_v);
    if (OPRT_OK != rt) {
        PR_ERR("get forecast high low temp failed: %d", rt);
        return;
    }

    PR_DEBUG_RAW("----------forecast high low temp----------\n");
    for (int i = 0; i < number; i++) {
        PR_DEBUG_RAW("high_temp[%d]: %d\n", i, high_temp_v[i]);
        PR_DEBUG_RAW("low_temp[%d]: %d\n", i, low_temp_v[i]);
    }

    /* Get city */
    char province[64] = {0}, city[64] = {0}, area[64] = {0};
    rt = tuya_weather_get_city(province, city, area);
    if (OPRT_OK != rt) {
        PR_ERR("get city failed: %d", rt);
        return;
    }

    PR_DEBUG_RAW("----------city----------\n");
    PR_DEBUG_RAW("province: %s\n", province);
    PR_DEBUG_RAW("city: %s\n", city);
    PR_DEBUG_RAW("area: %s\n", area);
}

void weather_get(int argc, char *argv[])
{
    tal_workq_schedule(WORKQ_SYSTEM, weather_get_workqueue_cb, NULL);
}
