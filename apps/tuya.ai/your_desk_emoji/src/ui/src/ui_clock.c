/**
 * @file ui_clock.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include <time.h>
#include "tal_api.h"
#include "lvgl.h"
#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"
#include "font_awesome_symbols.h"
#include "app_ui.h"

/***********************************************************
************************macro define************************
***********************************************************/
/* 256 == 1.0x, 512 == 2.0x => 1.2x ≈ 256 * 1.2 = 307 */
#ifndef TIME_LABEL_ZOOM_1P2X
#define TIME_LABEL_ZOOM_1P2X 307
#endif

#ifndef UI_NUDGE_TIME_UP_PX
#define UI_NUDGE_TIME_UP_PX        6   
#endif
#ifndef UI_NUDGE_OTHERS_DOWN_PX
#define UI_NUDGE_OTHERS_DOWN_PX    6   
#endif

#ifndef lv_obj_get_content_width
#define lv_obj_get_content_width  lv_obj_get_width
#define lv_obj_get_content_height lv_obj_get_height
#endif

#define WEATHER_CLOCK_UPDATE_INTERVAL_MS    1000    // 1 second update interval
#define WEATHER_ICON_NAME_NUM               8
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    lv_obj_t   *container;
    lv_obj_t   *status_bar;
    lv_obj_t   *time_label;           // Time display
    lv_obj_t   *date_label;           // Date display
    lv_obj_t   *temperature_label;    // Temperature display
    lv_obj_t   *weather_icon_img;     // Weather icon display
    lv_obj_t   *network_label;
    lv_obj_t   *notification_label;
    lv_obj_t   *status_label;
} AI_UI_CLOCK_T;

typedef struct {
    const lv_img_dsc_t *img;
    char* name[WEATHER_ICON_NAME_NUM];
}UI_WEATHER_ICON_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
LV_IMG_DECLARE(img_sun_120);
LV_IMG_DECLARE(img_cloudy_129);
LV_IMG_DECLARE(img_rain_112);
LV_IMG_DECLARE(img_small_rain_139);
LV_IMG_DECLARE(img_snow_105);
LV_IMG_DECLARE(img_thunder_110);
LV_IMG_DECLARE(img_thundershower_143);
LV_IMG_DECLARE(img_windy_114);

static const UI_WEATHER_ICON_T cWEATHER_ICONS[] = {
    {&img_sun_120,             {"120", "119", NULL,  NULL,  NULL,  NULL,  NULL,  NULL}},
    {&img_cloudy_129,          {"129", "142", "132", NULL,  NULL,  NULL,  NULL,  NULL}},
    {&img_rain_112,            {"112", "101", "107", "108", NULL,  NULL,  NULL,  NULL}},
    {&img_small_rain_139,      {"139", NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL}},
    {&img_snow_105,            {"105", "104", "115", "124", "126", NULL,  NULL,  NULL}},
    {&img_thunder_110,         {"110", "138", NULL,  NULL,  NULL,  NULL,  NULL,  NULL}},
    {&img_thundershower_143,   {"143", "102", NULL,  NULL,  NULL,  NULL,  NULL,  NULL}},
    {&img_windy_114,           {"114", NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL}},
};


static AI_UI_CLOCK_T sg_ui;
static AI_UI_FONT_LIST_T sg_font = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __ui_font_init(void)
{
    sg_font.text       = ai_ui_get_text_font();
    sg_font.icon       = ai_ui_get_icon_font();
    sg_font.emoji      = ai_ui_get_emo_font();
    sg_font.emoji_list = ai_ui_get_emo_list();
}

static const lv_image_dsc_t* __get_weather_img(char *name)
{
    if(NULL == name) {
        return NULL;
    }

    for (int i = 0; i < CNTSOF(cWEATHER_ICONS); i++) {
        for(int j = 0; j < WEATHER_ICON_NAME_NUM; j++) {
            if(cWEATHER_ICONS[i].name[j] == NULL) {
                continue;
            }

            if (0 == strcasecmp(cWEATHER_ICONS[i].name[j], name)) {
                return (lv_image_dsc_t *)cWEATHER_ICONS[i].img;
            }
        }
    }

    return cWEATHER_ICONS[0].img;
}


int __ui_clock_init(void)
{
    __ui_font_init();

    lv_obj_t *screen = lv_screen_active();
    /* Set screen background to pure black for minimal design */
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    /* Main container with minimal styling - no borders, no shadows, no padding */
    sg_ui.container = lv_obj_create(screen);
    lv_obj_set_size(sg_ui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(sg_ui.container, 0, 0);
    lv_obj_set_style_border_width(sg_ui.container, 0, 0);
    lv_obj_set_style_radius(sg_ui.container, 0, 0);
    lv_obj_set_style_bg_color(sg_ui.container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(sg_ui.container, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(sg_ui.container, 0, 0);
    lv_obj_add_flag(sg_ui.container, LV_OBJ_FLAG_OVERFLOW_VISIBLE); 

    /* 1. Date label - top left */
    sg_ui.date_label = lv_label_create(sg_ui.container);
    lv_obj_set_style_text_font(sg_ui.date_label, sg_font.text, 0);
    lv_obj_set_style_text_color(sg_ui.date_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_align(sg_ui.date_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_letter_space(sg_ui.date_label, 0, 0);
    lv_obj_set_size(sg_ui.date_label, 80, 20);  // Increased width for longer date formats
    lv_obj_align(sg_ui.date_label, LV_ALIGN_TOP_LEFT, 8, 4);

    // Date label initially visible with current date
    lv_label_set_text(sg_ui.date_label, "--/--");
    
    /* 2. Weather icon image - top right corner (PNG image) */
    sg_ui.weather_icon_img = lv_image_create(sg_ui.container);
    lv_obj_set_size(sg_ui.weather_icon_img, 16, 16);
    lv_obj_align(sg_ui.weather_icon_img, LV_ALIGN_TOP_RIGHT, -8, 4);
    lv_obj_add_flag(sg_ui.weather_icon_img, LV_OBJ_FLAG_HIDDEN);  // Initially hidden
    
    /* 3. Temperature label - top right, left of weather icon */
    sg_ui.temperature_label = lv_label_create(sg_ui.container);
    lv_obj_set_style_text_font(sg_ui.temperature_label, sg_font.text, 0);
    lv_obj_set_style_text_color(sg_ui.temperature_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_align(sg_ui.temperature_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_letter_space(sg_ui.temperature_label, 0, 0);
    lv_obj_set_size(sg_ui.temperature_label, 60, 20);
    lv_obj_align(sg_ui.temperature_label, LV_ALIGN_TOP_RIGHT, -32, 4);  // 32px left of weather icon (16px icon + 16px gap)
    lv_obj_add_flag(sg_ui.temperature_label, LV_OBJ_FLAG_HIDDEN);  // Initially hidden

    /* 4. Time label - center */
    sg_ui.time_label = lv_label_create(sg_ui.container);
    lv_label_set_text(sg_ui.time_label, "OFFLINE");
    lv_obj_set_style_text_font(sg_ui.time_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(sg_ui.time_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(sg_ui.time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(sg_ui.time_label, 1, 0);
    /* Scale time label to 1.2x */
    lv_obj_set_style_transform_scale(sg_ui.time_label, TIME_LABEL_ZOOM_1P2X, 0);
    lv_obj_set_width(sg_ui.time_label, LV_HOR_RES);
    /* Center time label */

    lv_obj_align(sg_ui.time_label, LV_ALIGN_CENTER, -13, 0);

    /* Initially show the weather clock (startup display) */
    lv_obj_clear_flag(sg_ui.container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(sg_ui.container);

    PR_DEBUG("Minimal weather clock UI initialized successfully");

    return 0;
}

void __ui_clock_show(void)
{
    /* Hide other UI elements first by moving weather clock to front */
    lv_obj_move_foreground(sg_ui.container);

    lv_obj_clear_flag(sg_ui.container, LV_OBJ_FLAG_HIDDEN);
}

void __ui_clock_hide(void)
{
    lv_obj_add_flag(sg_ui.container, LV_OBJ_FLAG_HIDDEN);
}

void __ui_clock_update_weather(int weather_code, int temperature)
{
    char weather_code_str[8] = {0};
    char temper_str[16] = {0};

    snprintf(weather_code_str, sizeof(weather_code_str), "%d", weather_code);
    snprintf(temper_str, sizeof(temper_str), "%d°C", temperature);

    const lv_image_dsc_t *icon_dsc = __get_weather_img(weather_code_str);
    if (icon_dsc != NULL) {
        lv_image_set_src(sg_ui.weather_icon_img, icon_dsc);
        lv_obj_clear_flag(sg_ui.weather_icon_img, LV_OBJ_FLAG_HIDDEN);
    }

    lv_label_set_text(sg_ui.temperature_label, temper_str);
    lv_obj_clear_flag(sg_ui.temperature_label, LV_OBJ_FLAG_HIDDEN);
}

void __ui_clock_update_time(POSIX_TM_S *curr_time)
{
    if(NULL == curr_time) {
        PR_ERR("time update info is NULL");
        return;
    } 

    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
                                         curr_time->tm_hour,\
                                         curr_time->tm_min, \
                                         curr_time->tm_sec);
    lv_label_set_text(sg_ui.time_label, time_str);

    char date_str[16];
    snprintf(date_str, sizeof(date_str), "%02d/%02d",
                                         curr_time->tm_mon + 1,\
                                         curr_time->tm_mday);
    lv_label_set_text(sg_ui.date_label, date_str);
}