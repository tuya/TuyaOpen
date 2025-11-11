#include "app_WeatherPage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tuya_weather.h"
#include "tal_log.h"

/**
 * @brief 将天气代码转换为中文描述
 * @param weather_code 天气代码（来自tuya_weather.h中的定义）
 * @param weather_str 输出的天气描述字符串
 * @param str_len 字符串缓冲区长度
 */
static void weather_code_to_string(int weather_code, char *weather_str, size_t str_len)
{
    switch(weather_code) {
        case TW_WEATHER_SUNNY:
        case TW_WEATHER_CLEAR:
            snprintf(weather_str, str_len, "晴");
            break;
        case TW_WEATHER_MOSTLY_CLEAR:
            snprintf(weather_str, str_len, "大部晴朗");
            break;
        case TW_WEATHER_PARTLY_CLOUDY:
            snprintf(weather_str, str_len, "多云");
            break;
        case TW_WEATHER_CLOUDY:
            snprintf(weather_str, str_len, "阴");
            break;
        case TW_WEATHER_OVERCAST:
            snprintf(weather_str, str_len, "阴天");
            break;
        case TW_WEATHER_FOG:
            snprintf(weather_str, str_len, "雾");
            break;
        case TW_WEATHER_FREEZING_FOG:
            snprintf(weather_str, str_len, "冻雾");
            break;
        case TW_WEATHER_HAZE:
            snprintf(weather_str, str_len, "霾");
            break;
        case TW_WEATHER_LIGHT_RAIN:
            snprintf(weather_str, str_len, "小雨");
            break;
        case TW_WEATHER_MODERATE_RAIN:
            snprintf(weather_str, str_len, "中雨");
            break;
        case TW_WEATHER_RAIN:
            snprintf(weather_str, str_len, "雨");
            break;
        case TW_WEATHER_HEAVY_RAIN:
            snprintf(weather_str, str_len, "大雨");
            break;
        case TW_WEATHER_RAINSTORM:
            snprintf(weather_str, str_len, "暴雨");
            break;
        case TW_WEATHER_EXTREME_RAINSTORM:
            snprintf(weather_str, str_len, "大暴雨");
            break;
        case TW_WEATHER_DOWNPOUR:
            snprintf(weather_str, str_len, "特大暴雨");
            break;
        case TW_WEATHER_LIGHT_TO_MODERATE_RAIN:
            snprintf(weather_str, str_len, "小到中雨");
            break;
        case TW_WEATHER_MODERATE_TO_HEAVY_RAIN:
            snprintf(weather_str, str_len, "中到大雨");
            break;
        case TW_WEATHER_HEAVY_RAIN_TO_RAINSTORM:
            snprintf(weather_str, str_len, "大到暴雨");
            break;
        case TW_WEATHER_SHOWER:
            snprintf(weather_str, str_len, "阵雨");
            break;
        case TW_WEATHER_LIGHT_SHOWER:
            snprintf(weather_str, str_len, "小阵雨");
            break;
        case TW_WEATHER_HEAVY_SHOWER:
            snprintf(weather_str, str_len, "大阵雨");
            break;
        case TW_WEATHER_ISOLATED_SHOWER:
            snprintf(weather_str, str_len, "局部阵雨");
            break;
        case TW_WEATHER_FREEZING_RAIN:
            snprintf(weather_str, str_len, "冻雨");
            break;
        case TW_WEATHER_SLEET:
            snprintf(weather_str, str_len, "雨夹雪");
            break;
        case TW_WEATHER_LIGHT_SNOW:
            snprintf(weather_str, str_len, "小雪");
            break;
        case TW_WEATHER_MODERATE_SNOW:
            snprintf(weather_str, str_len, "中雪");
            break;
        case TW_WEATHER_SNOW:
            snprintf(weather_str, str_len, "雪");
            break;
        case TW_WEATHER_HEAVY_SNOW:
            snprintf(weather_str, str_len, "大雪");
            break;
        case TW_WEATHER_LIGHT_TO_MODERATE_SNOW:
            snprintf(weather_str, str_len, "小到中雪");
            break;
        case TW_WEATHER_SNOW_SHOWER:
            snprintf(weather_str, str_len, "阵雪");
            break;
        case TW_WEATHER_LIGHT_SNOW_SHOWER:
            snprintf(weather_str, str_len, "小阵雪");
            break;
        case TW_WEATHER_BLIZZARD:
            snprintf(weather_str, str_len, "暴雪");
            break;
        case TW_WEATHER_HAIL:
            snprintf(weather_str, str_len, "冰雹");
            break;
        case TW_WEATHER_NEEDLE_ICE:
            snprintf(weather_str, str_len, "冰针");
            break;
        case TW_WEATHER_ICE_PELLETS:
            snprintf(weather_str, str_len, "冰粒");
            break;
        case TW_WEATHER_THUNDERSTORM:
            snprintf(weather_str, str_len, "雷暴");
            break;
        case TW_WEATHER_THUNDER_AND_LIGHTNING:
            snprintf(weather_str, str_len, "雷电");
            break;
        case TW_WEATHER_THUNDERSHOWER:
            snprintf(weather_str, str_len, "雷阵雨");
            break;
        case TW_WEATHER_THUNDERSHOWER_AND_HAIL:
            snprintf(weather_str, str_len, "雷阵雨冰雹");
            break;
        case TW_WEATHER_SANDSTORM:
            snprintf(weather_str, str_len, "沙尘暴");
            break;
        case TW_WEATHER_STRONG_SANDSTORM:
            snprintf(weather_str, str_len, "强沙尘暴");
            break;
        case TW_WEATHER_SAND_BLOWING:
            snprintf(weather_str, str_len, "扬沙");
            break;
        case TW_WEATHER_DUST:
            snprintf(weather_str, str_len, "浮尘");
            break;
        case TW_WEATHER_DUST_DEVIL:
            snprintf(weather_str, str_len, "尘卷风");
            break;
        default:
            snprintf(weather_str, str_len, "未知");
            break;
    }
}

/**
 * @brief 使用Tuya天气服务API获取实时天气信息
 * @param weather_info 用于存储天气信息的结构体指针
 * @return 0: 成功, -1: 失败
 */
int get_weather_info(WeatherInfo_t* weather_info)
{
    OPERATE_RET rt = OPRT_OK;
    WEATHER_CURRENT_CONDITIONS_T current_conditions = {0};
    char wind_dir[64] = {0};
    char wind_speed[64] = {0};
    int wind_level = 0;

    if(weather_info == NULL)
    {
        PR_ERR("weather_info is NULL");
        return -1;
    }

    // 检查是否允许更新天气数据
    if(!tuya_weather_allow_update())
    {
        PR_ERR("Weather update not allowed (device not activated or network not connected)");
        return -1;
    }

    // 获取当前天气条件
    rt = tuya_weather_get_current_conditions(&current_conditions);
    if(rt != OPRT_OK)
    {
        PR_ERR("Failed to get current weather conditions: %d", rt);
        return -1;
    }

    // 获取当前风力信息（使用中国版API，包含风力等级）
    rt = tuya_weather_get_current_wind_cn(wind_dir, wind_speed, &wind_level);
    if(rt != OPRT_OK)
    {
        PR_WARN("Failed to get wind info, using default: %d", rt);
        // 如果获取风力失败，使用默认值
        snprintf(wind_speed, sizeof(wind_speed), "≤3");
        wind_level = 0;
    }

    // 将天气代码转换为中文描述
    weather_code_to_string(current_conditions.weather, weather_info->weather, sizeof(weather_info->weather));

    // 将温度转换为字符串
    snprintf(weather_info->temperature, sizeof(weather_info->temperature), "%d", current_conditions.temp);

    // 将湿度转换为字符串
    snprintf(weather_info->humidity, sizeof(weather_info->humidity), "%d", current_conditions.humi);

    // 格式化风力信息
    if(wind_level > 0)
    {
        snprintf(weather_info->windpower, sizeof(weather_info->windpower), "%d", wind_level);
    }
    else if(wind_speed[0] != '\0')
    {
        // 如果只有风速字符串，直接使用
        snprintf(weather_info->windpower, sizeof(weather_info->windpower), "%s", wind_speed);
    }
    else
    {
        // 默认值
        snprintf(weather_info->windpower, sizeof(weather_info->windpower), "≤3");
    }

    PR_DEBUG("Weather info: %s, %s°C, %s%%, %s", 
             weather_info->weather, 
             weather_info->temperature, 
             weather_info->humidity, 
             weather_info->windpower);

    return 0;
}