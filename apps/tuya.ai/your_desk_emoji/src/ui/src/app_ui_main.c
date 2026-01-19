/**
 * @file app_ui_main.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include "tal_api.h"
#include "lvgl.h"
#include "lv_vendor.h"
#include "ai_ui_manage.h"
#include "app_ui.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define EMOJI_CHANGE_INTERVAL (5*1000) // Expression switching interval (5 seconds)

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static const lv_img_dsc_t *sg_curr_emo_img = NULL;
static const lv_img_dsc_t *sg_start_emo_img = NULL;
static lv_timer_t *sg_emo_cycle_timer = NULL; 
/***********************************************************
*************************extern define**********************
***********************************************************/
extern int __ui_emoji_init(void);
extern void __ui_emoji_show(void);
extern void __ui_emoji_hide(void);
extern const lv_img_dsc_t *__ui_emoji_get_next_img(const lv_img_dsc_t *cur_gif);
extern const lv_img_dsc_t *__ui_emoji_set_emotion(char *emotion);
extern void __ui_emoji_set_img(const lv_img_dsc_t *gif);

extern int  __ui_clock_init(void);
extern void __ui_clock_show(void);
extern void __ui_clock_hide(void);
extern void __ui_clock_update_weather(int weather_code, int temperature);
extern void __ui_clock_update_time(POSIX_TM_S *curr_time);

/***********************************************************
***********************function define**********************
***********************************************************/
static void __lvgl_init(void)
{
    lv_vendor_init(DISPLAY_NAME);

    lv_vendor_start(5, 1024*8);
}

static void __emotion_timer_cb(lv_timer_t *timer)
{
    const lv_img_dsc_t *next_img = __ui_emoji_get_next_img(sg_curr_emo_img);
    if(NULL == next_img || next_img == sg_start_emo_img) {
        lv_timer_pause(sg_emo_cycle_timer);
        PR_DEBUG("Emoji cycle completed, pausing timer");

        __ui_emoji_hide();
        __ui_clock_show();
    }else {
        __ui_emoji_set_img(next_img);
        sg_curr_emo_img = next_img;
    }
}

static OPERATE_RET __ui_init(void)
{
    __lvgl_init();

    lv_vendor_disp_lock();

    __ui_clock_init();

    __ui_emoji_init();

    sg_curr_emo_img    = __ui_emoji_set_emotion(EMOJI_HAPPY);
    sg_start_emo_img   = sg_curr_emo_img;
    sg_emo_cycle_timer = lv_timer_create(__emotion_timer_cb, EMOJI_CHANGE_INTERVAL, NULL);

    lv_vendor_disp_unlock();

    return 0;
}

static void __ui_set_emotion(char *emotion)
{
    lv_vendor_disp_lock();

    const lv_img_dsc_t *img = __ui_emoji_set_emotion(emotion);
    if(img) {
        sg_curr_emo_img = img;

        if(sg_emo_cycle_timer) {
            sg_start_emo_img = img;
            lv_timer_reset(sg_emo_cycle_timer);
        }
    }

    lv_vendor_disp_unlock();
}

static void __ui_handle_custom_type(uint32_t type, uint8_t *data, int len)
{
    lv_vendor_disp_lock();

    switch(type) {
        case AI_UI_DISP_EMOJI_UI_SHOW:
            __ui_clock_hide();
            __ui_emoji_show();
        break;
        case AI_UI_DISP_PAUSE_EMOJI_CYCLE:
            if(sg_emo_cycle_timer) {
                lv_timer_pause(sg_emo_cycle_timer);
            }
        break;
        case AI_UI_DISP_RESUME_EMOJI_CYCLE:
            if(sg_emo_cycle_timer) {
                lv_timer_resume(sg_emo_cycle_timer);
            }
            break;
        case AI_UI_DISP_CLOCK_UI_SHOW:
            __ui_emoji_hide();
            __ui_clock_show();
            break;
        case AI_UI_DISP_CLOCK_UPDATE_WEATHER:
            if(data != NULL && len == sizeof(UI_DISP_CLOCK_WEATHER_T)) {
                UI_DISP_CLOCK_WEATHER_T *info = (UI_DISP_CLOCK_WEATHER_T *)data;
                __ui_clock_update_weather(info->weather_code, info->temperature);
            }else {
                PR_ERR("Invalid weather update data");
            }
            break;
        case AI_UI_DISP_CLOCK_UPDATE_TIME:
            if(data != NULL && len == sizeof(UI_DISP_CLOCK_TIME_T)) {
                UI_DISP_CLOCK_TIME_T *time_info = (UI_DISP_CLOCK_TIME_T *)data;
                __ui_clock_update_time(&(time_info->curr_time));
            }else {
                PR_ERR("Invalid time update data");
            }
            break;
        default:
            PR_ERR("Unknown custom display type: %d", type);
            break;
    }

    lv_vendor_disp_unlock();
}

OPERATE_RET app_ai_ui_register(void)
{
    AI_UI_INTFS_T intfs;

    memset(&intfs, 0, sizeof(AI_UI_INTFS_T));

    intfs.disp_init       = __ui_init;
    intfs.disp_emotion    = __ui_set_emotion;
    intfs.disp_other_msg  = __ui_handle_custom_type;

    return ai_ui_register(&intfs);
}