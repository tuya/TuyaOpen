#include "app_WeatherPage.h"
#include "ui_WeatherPage.h"
#include "tal_log.h"

///////////////////// VARIABLES ////////////////////

static char * day[] = { "Sun.", "Mon.", "Tues.", "Wed.", "Thurs.", "Fri.", "Sat." };

struct ui_weather_para_t{
    uint8_t day_of_week;
    int year;
    int month;
    int date;
    LocationInfo_t location;
    WeatherInfo_t weather_info;
    bool first_enter;
};

struct ui_weather_para_t ui_weather_para={
    .day_of_week = 0,
    .year = 0,
    .month = 0,
    .date = 0,
    .location = { "未知地", "440300" },
    .weather_info = { "多云", "14", "75", "≤3" },
    .first_enter = 1
};

lv_obj_t * ui_ImgCloud;
lv_obj_t * ui_LabelCity;
lv_obj_t * ui_LabelDate;
lv_obj_t * ui_LabelTemp;
lv_obj_t * ui_LabelWeather;
lv_obj_t * ui_LabelWind;
lv_obj_t * ui_LabelHumi;
lv_timer_t * ui_weather_timer;

///////////////////// ANIMATIONS ////////////////////

static void _cloud_move_anim(void)
{
    int16_t y_pos_now = 20;
    lv_lib_anim_user_animation(ui_ImgCloud, 0, 1000, y_pos_now, y_pos_now+10, 0, 0, 0, 0, lv_anim_path_linear, lv_lib_anim_callback_set_y, NULL);
    y_pos_now+=10;
    lv_lib_anim_user_animation(ui_ImgCloud, 1000, 1000, y_pos_now, y_pos_now-10, 0, 0, 0, 0, lv_anim_path_linear, lv_lib_anim_callback_set_y, NULL);
    y_pos_now-=10;
}

///////////////////// FUNCTIONS ////////////////////

static void _ui_weather_timer_cb(lv_timer_t * timer)
{
    static uint16_t time_count = 0;
    time_count++;
    // 5分钟更新一次weather info
    if(time_count >= 5*60)
    {
        time_count = 0;
        ui_weather_para.first_enter = 1;
    }
    if(ui_weather_para.first_enter) 
    {
        ui_weather_para.first_enter = 0;
        // get location info
        if(ui_system_para.auto_location == true)
        {
            if(sys_get_auto_location_by_tuya_weather(&ui_weather_para.location) != 0) 
            {
                LV_LOG_WARN("Failed to get location info.");
                sprintf(ui_weather_para.location.city, "%s", "未知地");
                lv_label_set_text(ui_LabelCity, "未知地");
            } 
        }
        else
        {
            sprintf(ui_weather_para.location.city, "%s", ui_system_para.location.city);
            lv_label_set_text(ui_LabelCity, ui_system_para.location.city);
        }
        // get weather info
        if(get_weather_info( &ui_weather_para.weather_info) != 0) 
        {    
            // show msg box
            ui_msgbox_info("Error", "weather info get fail.");
        } 
        else
        {
            
            char str[36];
            sprintf(str, "%s", ui_weather_para.location.city);
            lv_label_set_text(ui_LabelCity, str);
            sprintf(str, "%02d.%02d %s", ui_weather_para.month, ui_weather_para.date, day[ui_weather_para.day_of_week]);
            lv_label_set_text(ui_LabelDate, str);
            sprintf(str, "%s°", ui_weather_para.weather_info.temperature);
            lv_label_set_text(ui_LabelTemp, str);
            sprintf(str, "%s", ui_weather_para.weather_info.weather);
            lv_label_set_text(ui_LabelWeather, str);
            sprintf(str, "%s", ui_weather_para.weather_info.windpower);
            lv_label_set_text(ui_LabelWind, str);
            sprintf(str, "%s%%", ui_weather_para.weather_info.humidity);
            lv_label_set_text(ui_LabelHumi, str);
        }
    }
    static uint16_t time_count2 = 0;
    time_count2++;
    if(time_count2 >= 4)
    {
        time_count2 = 0;
        _cloud_move_anim();
    }
    
}

static void _ui_enent_Gesture(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_GESTURE)
    {
        if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT || lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT)
        {
            lv_lib_pm_OpenPrePage(&page_manager);
        }
    }
}

static void _ui_WeatherPage_Para_Init(void)
{
    int year; int month; int day; int hour; int minute; int second;
    sys_get_time(&year, &month, &day, &hour, &minute, &second);
    ui_weather_para.year = year;
    ui_weather_para.month = month;
    ui_weather_para.date = day;
    ui_weather_para.day_of_week = sys_get_day_of_week(ui_weather_para.year, ui_weather_para.month, ui_weather_para.date);
    strcpy(ui_weather_para.location.city, ui_system_para.location.city);
    strcpy(ui_weather_para.location.adcode, ui_system_para.location.adcode);
    ui_weather_para.first_enter = 1;
}

///////////////////// SCREEN init ////////////////////

void ui_WeatherPage_init(void)
{
    _ui_WeatherPage_Para_Init();
    lv_obj_t * ui_WeatherPage = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_WeatherPage, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t * ui_ImgSun = lv_image_create(ui_WeatherPage);
    lv_image_set_src(ui_ImgSun, &ui_img_sun_png);
    lv_obj_set_width(ui_ImgSun, LV_SIZE_CONTENT);   /// 164
    lv_obj_set_height(ui_ImgSun, LV_SIZE_CONTENT);    /// 306
    lv_obj_set_x(ui_ImgSun, 15);
    lv_obj_set_y(ui_ImgSun, -45);
    lv_obj_set_align(ui_ImgSun, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_ImgSun, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_remove_flag(ui_ImgSun, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_image_set_scale(ui_ImgSun, 208);

    ui_ImgCloud = lv_image_create(ui_WeatherPage);
    lv_image_set_src(ui_ImgCloud, &ui_img_clouds_png);
    lv_obj_set_width(ui_ImgCloud, LV_SIZE_CONTENT);   /// 168
    lv_obj_set_height(ui_ImgCloud, LV_SIZE_CONTENT);    /// 192
    lv_obj_set_x(ui_ImgCloud, 20);
    lv_obj_set_y(ui_ImgCloud, 20);
    lv_obj_set_align(ui_ImgCloud, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_ImgCloud, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_remove_flag(ui_ImgCloud, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_image_set_scale(ui_ImgCloud, 206);

    lv_obj_t * ui_LabelsPanel = lv_obj_create(ui_WeatherPage);
    lv_obj_set_width(ui_LabelsPanel, 200);
    lv_obj_set_height(ui_LabelsPanel, 240);
    lv_obj_set_x(ui_LabelsPanel, -50);
    lv_obj_set_y(ui_LabelsPanel, 0);
    lv_obj_set_align(ui_LabelsPanel, LV_ALIGN_CENTER);
    lv_obj_remove_flag(ui_LabelsPanel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_LabelsPanel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LabelsPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_LabelsPanel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_LabelsPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelCity = lv_label_create(ui_LabelsPanel);
    lv_obj_set_width(ui_LabelCity, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelCity, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelCity, -10);
    lv_obj_set_y(ui_LabelCity, 0);
    lv_obj_set_align(ui_LabelCity, LV_ALIGN_TOP_MID);
    char str[36];
    sprintf(str, "%s", ui_weather_para.location.city);
    lv_label_set_text(ui_LabelCity, str);
    lv_obj_set_style_text_font(ui_LabelCity, &ui_font_heiti22, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelDate = lv_label_create(ui_LabelsPanel);
    lv_obj_set_width(ui_LabelDate, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelDate, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelDate, -10);
    lv_obj_set_y(ui_LabelDate, 30);
    lv_obj_set_align(ui_LabelDate, LV_ALIGN_TOP_MID);
    sprintf(str, "%02d.%02d %s", ui_weather_para.month, ui_weather_para.date, day[ui_weather_para.day_of_week]);
    lv_label_set_text(ui_LabelDate, str);
    lv_obj_set_style_text_color(ui_LabelDate, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelDate, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelDate, &ui_font_heiti22, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelTemp = lv_label_create(ui_LabelsPanel);
    lv_obj_set_width(ui_LabelTemp, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelTemp, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelTemp, 0);
    lv_obj_set_y(ui_LabelTemp, 5);
    lv_obj_set_align(ui_LabelTemp, LV_ALIGN_CENTER);
    sprintf(str, "%s°", ui_weather_para.weather_info.temperature);
    lv_label_set_text(ui_LabelTemp, str);
    lv_obj_set_style_text_font(ui_LabelTemp, &ui_font_NuberBig90, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelWeather = lv_label_create(ui_LabelsPanel);
    lv_obj_set_width(ui_LabelWeather, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelWeather, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelWeather, -10);
    lv_obj_set_y(ui_LabelWeather, 150);
    lv_obj_set_align(ui_LabelWeather, LV_ALIGN_TOP_MID);
    sprintf(str, "%s", ui_weather_para.weather_info.weather);
    lv_label_set_text(ui_LabelWeather, str);
    lv_obj_set_style_text_color(ui_LabelWeather, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelWeather, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelWeather, &ui_font_heiti22, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_BtmLabelsPanel = lv_obj_create(ui_WeatherPage);
    lv_obj_set_width(ui_BtmLabelsPanel, 320);
    lv_obj_set_height(ui_BtmLabelsPanel, 50);
    lv_obj_set_x(ui_BtmLabelsPanel, 0);
    lv_obj_set_y(ui_BtmLabelsPanel, 95);
    lv_obj_set_align(ui_BtmLabelsPanel, LV_ALIGN_CENTER);
    lv_obj_remove_flag(ui_BtmLabelsPanel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_BtmLabelsPanel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BtmLabelsPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_BtmLabelsPanel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_BtmLabelsPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_IconWind = lv_label_create(ui_BtmLabelsPanel);
    lv_obj_set_width(ui_IconWind, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconWind, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_IconWind, -70);
    lv_obj_set_y(ui_IconWind, 0);
    lv_obj_set_align(ui_IconWind, LV_ALIGN_CENTER);
    lv_label_set_text(ui_IconWind, "");
    lv_obj_set_style_text_color(ui_IconWind, lv_color_hex(0xEEB27D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_IconWind, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_IconWind, &ui_font_iconfont30, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelWind = lv_label_create(ui_BtmLabelsPanel);
    lv_obj_set_width(ui_LabelWind, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelWind, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelWind, -35);
    lv_obj_set_y(ui_LabelWind, 0);
    lv_obj_set_align(ui_LabelWind, LV_ALIGN_CENTER);
    sprintf(str, "%s", ui_weather_para.weather_info.windpower);
    lv_label_set_text(ui_LabelWind, str);
    lv_obj_set_style_text_color(ui_LabelWind, lv_color_hex(0xEEB27D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelWind, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelWind, &ui_font_heiti22, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_IconHumi = lv_label_create(ui_BtmLabelsPanel);
    lv_obj_set_width(ui_IconHumi, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconHumi, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_IconHumi, 35);
    lv_obj_set_y(ui_IconHumi, 0);
    lv_obj_set_align(ui_IconHumi, LV_ALIGN_CENTER);
    lv_label_set_text(ui_IconHumi, "");
    lv_obj_set_style_text_color(ui_IconHumi, lv_color_hex(0x42D2F4), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_IconHumi, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_IconHumi, &ui_font_iconfont30, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelHumi = lv_label_create(ui_BtmLabelsPanel);
    lv_obj_set_width(ui_LabelHumi, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelHumi, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelHumi, 75);
    lv_obj_set_y(ui_LabelHumi, 1);
    lv_obj_set_align(ui_LabelHumi, LV_ALIGN_CENTER);
    sprintf(str, "%s%%", ui_weather_para.weather_info.humidity);
    lv_label_set_text(ui_LabelHumi, str);
    lv_obj_set_style_text_color(ui_LabelHumi, lv_color_hex(0x42D2F4), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelHumi, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelHumi, &ui_font_heiti22, LV_PART_MAIN | LV_STATE_DEFAULT);

    // event
    lv_obj_add_event_cb(ui_WeatherPage, _ui_enent_Gesture, LV_EVENT_ALL, NULL);

    // timer
    ui_weather_timer = lv_timer_create(_ui_weather_timer_cb, 1000, NULL);

    // load page
    lv_scr_load_anim(ui_WeatherPage, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

/////////////////// SCREEN deinit ////////////////////

void ui_WeatherPage_deinit()
{
    // deinit
    lv_timer_delete(ui_weather_timer);
}