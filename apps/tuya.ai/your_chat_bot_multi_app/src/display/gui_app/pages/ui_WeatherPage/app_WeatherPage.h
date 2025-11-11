#ifndef _APP_WEATHERPAGE_H
#define _APP_WEATHERPAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ui_WeatherPage.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
// #include <curl/curl.h>
// #include <json-c/json.h>

typedef struct {
    char weather[32];
    char temperature[16];
    char humidity[16];
    char windpower[16];
} WeatherInfo_t;

/**
 * @brief 根据adcode获取天气信息，并填充WeatherInfo结构体。
 *
 * @param adcode 城市adcode。
 * @param weather_info 用于存储天气信息的结构体。
 * @return 0: success, -1: fail
 */
int get_weather_info(WeatherInfo_t* weather_info);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif