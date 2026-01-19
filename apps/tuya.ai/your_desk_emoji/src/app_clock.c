/**
 * @file app_clock.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include "tal_api.h"
#include "tuya_weather.h"
#include "app_ui.h"
#include "app_clock.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define WEATHER_UPDATE_INTERVAL_MS    (30 * 60 * 1000)  // 30 minutes
#define TIME_UPDATE_INTERVAL_MS       (1000)    // 1 second update interval
/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/ 
static TIMER_ID sg_get_weather_tm;
static TIMER_ID sg_get_time_tm;
/***********************************************************
***********************function define**********************
***********************************************************/
static void __display_weather_info(WEATHER_CURRENT_CONDITIONS_T *info)
{
    UI_DISP_CLOCK_WEATHER_T disp_info;

    if(NULL == info) {
        return;
    }

    disp_info.weather_code = info->weather;
    disp_info.temperature = info->temp;

    ai_ui_disp_msg(AI_UI_DISP_CLOCK_UPDATE_WEATHER, (uint8_t *)&disp_info, sizeof(UI_DISP_CLOCK_WEATHER_T));
}

static OPERATE_RET __update_weather_info(void)
{
    OPERATE_RET rt = OPRT_OK;
    
    PR_DEBUG("=== STARTING WEATHER DATA UPDATE ===");
    
    // Check if weather service is available
    if (false == tuya_weather_allow_update()) {
        return OPRT_INVALID_PARM;
    }

    // Get current weather conditions
    WEATHER_CURRENT_CONDITIONS_T current_conditions = {0};
    TUYA_CALL_ERR_RETURN(tuya_weather_get_current_conditions(&current_conditions));

    // Print detailed weather information
    PR_DEBUG("=== DETAILED WEATHER INFORMATION ===");
    PR_DEBUG("Weather type: %d", current_conditions.weather);
    PR_DEBUG("Temperature: %d°C", current_conditions.temp);
    PR_DEBUG("Humidity: %d%%", current_conditions.humi);
    PR_DEBUG("Real feel: %d°C", current_conditions.real_feel);
    PR_DEBUG("Pressure: %d mbar", current_conditions.mbar);
    PR_DEBUG("UV Index: %d", current_conditions.uvi);
    PR_DEBUG("=== END DETAILED WEATHER INFORMATION ===");
    
    __display_weather_info(&current_conditions);
    
    return OPRT_OK;
}

static void __update_local_time(void)
{
    OPERATE_RET ret = OPRT_OK;
    UI_DISP_CLOCK_TIME_T time_info;

    if(OPRT_OK != tal_time_check_time_sync()) {
        return;
    }

    memset(&time_info, 0, sizeof(UI_DISP_CLOCK_TIME_T));

    ret = tal_time_get_local_time_custom(0, &time_info.curr_time);
    if(OPRT_OK != ret) {
        PR_ERR("Failed to get local time, ret=%d", ret);
        return;
    }

#if 0
    PR_NOTICE("Current Time: %04d-%02d-%02d %02d:%02d:%02d", \
               time_info.curr_time.tm_year + 1900, \
               time_info.curr_time.tm_mon + 1, \
               time_info.curr_time.tm_mday, \
               time_info.curr_time.tm_hour, \
               time_info.curr_time.tm_min, \
               time_info.curr_time.tm_sec);
#endif

    ai_ui_disp_msg(AI_UI_DISP_CLOCK_UPDATE_TIME, (uint8_t *)&time_info, sizeof(UI_DISP_CLOCK_TIME_T));
}

static void __get_weather_timer_cb(TIMER_ID timer_id, void *arg)
{
    __update_weather_info();
}

static void __get_local_timer_cb(TIMER_ID timer_id, void *arg)
{
    __update_local_time();
}

static int __weather_time_sync(void *data)
{
    __update_local_time();
    __update_weather_info();
    
    return OPRT_OK;
}

OPERATE_RET app_clock_init(void)
{
    OPERATE_RET rt;

    TUYA_CALL_ERR_RETURN(tal_event_subscribe("app.time.sync", "update", \
                                             __weather_time_sync, SUBSCRIBE_TYPE_NORMAL));

    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__get_weather_timer_cb,\
                                             NULL, &sg_get_weather_tm));
                 
    TUYA_CALL_ERR_RETURN(tal_sw_timer_start(sg_get_weather_tm, WEATHER_UPDATE_INTERVAL_MS, TAL_TIMER_CYCLE)); 
    
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__get_local_timer_cb,\
                                             NULL, &sg_get_time_tm));

    TUYA_CALL_ERR_RETURN(tal_sw_timer_start(sg_get_time_tm, TIME_UPDATE_INTERVAL_MS, TAL_TIMER_CYCLE)); 

    return rt;
}