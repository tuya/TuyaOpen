#ifndef _UI_H
#define _UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "./common/lv_lib.h"
#include "../sys_manager.h"
#include "tal_log.h"

#define UI_SCREEN_WIDTH LV_HOR_RES
#define UI_SCREEN_HEIGHT LV_VER_RES



// extern variables
extern lv_lib_pm_t page_manager;
extern system_para_t ui_system_para;

// IMAGES AND IMAGE SETS
extern const lv_image_dsc_t ui_img_weather64_png;    // assets/weather64.png
extern const lv_image_dsc_t ui_img_calendar64_png;    // assets/calendar64.png
extern const lv_image_dsc_t ui_img_memo64_png;    // assets/Memo64.png
extern const lv_image_dsc_t ui_img_gamememory64_png;    // assets/GameMemory64.png
extern const lv_image_dsc_t ui_img_paint60_png;    // assets/paint60.png
extern const lv_image_dsc_t ui_img_question60_png;    // assets/question60.png
extern const lv_image_dsc_t ui_img_think60_png;    // assets/think60.png
extern const lv_image_dsc_t ui_img_hand60_png;    // assets/hand60.png
extern const lv_image_dsc_t ui_img_muyu128_png;    // assets/muyu128.png
extern const lv_image_dsc_t ui_img_sun_png;    // assets/sun.png
extern const lv_image_dsc_t ui_img_clouds_png;    // assets/clouds.png
extern const lv_image_dsc_t dinoso;    // assets/dinoso.png
extern const lv_image_dsc_t dinoso_icon;    // assets/dinoso_icon.png

// FONTS
LV_FONT_DECLARE(ui_font_iconfont20);
LV_FONT_DECLARE(ui_font_iconfont26);
LV_FONT_DECLARE(ui_font_iconfont30);
LV_FONT_DECLARE(ui_font_iconfont36);
LV_FONT_DECLARE(ui_font_iconfont44);
LV_FONT_DECLARE(ui_font_iconfont48);
LV_FONT_DECLARE(ui_font_heiti14);
LV_FONT_DECLARE(ui_font_heiti22);
LV_FONT_DECLARE(ui_font_heiti24);
LV_FONT_DECLARE(ui_font_shuhei22);
LV_FONT_DECLARE(ui_font_NuberBig90);
LV_FONT_DECLARE(lv_font_montserrat_16);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_26);

// ui apps data
typedef lv_lib_pm_page_t ui_app_data_t;

// UI INIT
void app_ui_init(void);

// UI INFO MSGBOX
void ui_msgbox_info(const char * title, const char * text);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif