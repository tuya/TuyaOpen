/**
 * @file app_ui.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __APP_UI_H__
#define __APP_UI_H__

#include "tuya_cloud_types.h"
#include "ai_ui_manage.h"
#include "tal_time_service.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum{
    AI_UI_DISP_EMOJI_UI_SHOW = AI_UI_DISP_SYS_MAX+1,
    AI_UI_DISP_PAUSE_EMOJI_CYCLE,
    AI_UI_DISP_RESUME_EMOJI_CYCLE,
    AI_UI_DISP_CLOCK_UI_SHOW,
    AI_UI_DISP_CLOCK_UPDATE_WEATHER,
    AI_UI_DISP_CLOCK_UPDATE_TIME,
}CUSTOM_AI_UI_DISP_TYPE_E;

typedef struct {
    POSIX_TM_S curr_time;
}UI_DISP_CLOCK_TIME_T;

typedef struct {
    int weather_code;  // Weather condition code
    int temperature;   // Temperature value
} UI_DISP_CLOCK_WEATHER_T;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET app_ai_ui_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_UI_H__ */
