#include "ui_HomePage.h"
#include "tal_log.h"
#include "lvgl.h"

///////////////////// VARIABLES ////////////////////

#define _APP_CONTAINER_MAX_PAGES 2

ui_desktop_data_t ui_desktop_data = {
    .witdh = 320,  // 使用固定值，适配 320x480 屏幕
    .height = 480,
    .container_total_pages = _APP_CONTAINER_MAX_PAGES,
    .app_container_index = 0,
    .show_dropdown = false,
    .scroll_busy = false        
};

lv_obj_t * ui_ScrollDots[_APP_CONTAINER_MAX_PAGES];

lv_timer_t * ui_home_timer;
lv_obj_t * ui_WifiLabel;
lv_obj_t * ui_NoWifiLabel;


//////////////////////// Timer //////////////////////

static void _wifi_connected_icon_set(bool connected)
{
    if(connected)
    {
        lv_obj_remove_flag(ui_WifiLabel, LV_OBJ_FLAG_HIDDEN);     /// Flags
        lv_obj_add_flag(ui_NoWifiLabel, LV_OBJ_FLAG_HIDDEN);     /// Flags
    }
    else
    {
        lv_obj_remove_flag(ui_NoWifiLabel, LV_OBJ_FLAG_HIDDEN);     /// Flags
        lv_obj_add_flag(ui_WifiLabel, LV_OBJ_FLAG_HIDDEN);     /// Flags
    }
}

static void ui_home_timer_cb(lv_timer_t * timer)
{
    lv_obj_t * timelabel = lv_timer_get_user_data(timer);
    int year; int month; int day; int hour; int minute; int second;
    sys_get_time(&year, &month, &day, &hour, &minute, &second);
    if(minute!=ui_system_para.minute)
    {
        char time_str[6];
        sprintf(time_str, "%02d:%02d", hour, minute);
        lv_label_set_text(timelabel, time_str);
        ui_system_para.year = year;
        ui_system_para.month = month;
        ui_system_para.day = day;
        ui_system_para.hour = hour;
        ui_system_para.minute = minute;
    }
    _wifi_connected_icon_set(ui_system_para.wifi_connected);
}

///////////////////// ANIMATIONS ////////////////////

static void _ui_anim_completed_cb()
{
    ui_desktop_data.scroll_busy = false;
}

static void AppContLeft_Animation(lv_obj_t * TargetObject, int delay)
{
    int32_t x_pos_now = lv_obj_get_x(TargetObject);
    lv_lib_anim_user_animation(TargetObject, 0, 100, x_pos_now, x_pos_now - ui_desktop_data.witdh, 0, 0, 0, 0, lv_anim_path_ease_out, lv_lib_anim_callback_set_x, _ui_anim_completed_cb);
}

static void AppContRight_Animation(lv_obj_t * TargetObject, int delay)
{
    int32_t x_pos_now = lv_obj_get_x(TargetObject);
    lv_lib_anim_user_animation(TargetObject, 0, 100, x_pos_now, x_pos_now + ui_desktop_data.witdh, 0, 0, 0, 0, lv_anim_path_ease_out, lv_lib_anim_callback_set_x, _ui_anim_completed_cb);
}

///////////////////// FUNCTIONS ////////////////////

static void ui_event_TopDrag(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    lv_obj_t * dropdown_panel = lv_event_get_user_data(e);

    if(event_code == LV_EVENT_PRESSING) {
        lv_indev_t * indev = lv_indev_active();
        if(indev == NULL)  return;

        lv_point_t vect;
        lv_indev_get_vect(indev, &vect);

        int32_t y = lv_obj_get_y_aligned(obj) + vect.y;
        lv_obj_set_y(obj, y); // set drag position follow the touch
        lv_obj_set_y(dropdown_panel, y-ui_desktop_data.height); // set dorp down panel
    }

    else if(event_code == LV_EVENT_RELEASED) {
        int32_t y = lv_obj_get_y_aligned(obj);
        if(!ui_desktop_data.show_dropdown && y >= ui_desktop_data.height/3)
        {
            lv_obj_set_y(dropdown_panel, 0);
            lv_obj_set_y(obj, ui_desktop_data.height-lv_obj_get_height(obj));
            ui_desktop_data.show_dropdown = true;
        }
        else if(ui_desktop_data.show_dropdown && y <= ui_desktop_data.height * 2/3)
        {
            lv_obj_set_y(dropdown_panel, -ui_desktop_data.height);
            lv_obj_set_y(obj, 0);
            ui_desktop_data.show_dropdown = false;
        }
    }
}

static void ui_event_scroll_gesture(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * AppContainer = lv_event_get_user_data(e);
    if(event_code == LV_EVENT_GESTURE && !ui_desktop_data.scroll_busy && !ui_desktop_data.show_dropdown) {
        if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT)
        {   
            // not in the end page
            if(ui_desktop_data.app_container_index < ui_desktop_data.container_total_pages-1)
            {
                ui_desktop_data.scroll_busy = true;
                AppContLeft_Animation(AppContainer, 0);
                lv_obj_set_style_bg_opa(ui_ScrollDots[ui_desktop_data.app_container_index], 96, LV_PART_MAIN | LV_STATE_DEFAULT);
                ui_desktop_data.app_container_index++;
                lv_obj_set_style_bg_opa(ui_ScrollDots[ui_desktop_data.app_container_index], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
        else if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT)
        {
            // not in the first page
            if(ui_desktop_data.app_container_index > 0)
            {
                ui_desktop_data.scroll_busy = true;
                AppContRight_Animation(AppContainer, 0);
                lv_obj_set_style_bg_opa(ui_ScrollDots[ui_desktop_data.app_container_index], 96, LV_PART_MAIN | LV_STATE_DEFAULT);
                ui_desktop_data.app_container_index--;
                lv_obj_set_style_bg_opa(ui_ScrollDots[ui_desktop_data.app_container_index], 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        }
    }
}

static void ui_event_AppsBtn(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    char * pagename = lv_event_get_user_data(e);
    if(event_code == LV_EVENT_CLICKED && !ui_desktop_data.scroll_busy) 
    {
        lv_lib_pm_OpenPage(&page_manager, NULL, pagename);
    }
    PR_DEBUG("----------->ui_event_AppsBtn: %s", pagename);
}

// sliders
static void ui_event_SoundSlider(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * slider = lv_event_get_target(e);
    if(event_code == LV_EVENT_RELEASED)
    {
        ui_system_para.sound = lv_slider_get_value(slider);
        sys_set_volume(ui_system_para.sound);
    }
}

static void ui_event_LightSlider(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * slider = lv_event_get_target(e);
    if(event_code == LV_EVENT_VALUE_CHANGED)
    {
        ui_system_para.brightness = lv_slider_get_value(slider);
        sys_set_lcd_brightness(ui_system_para.brightness);
    }
}

// btns on dropdown
static void ui_event_SetBtn(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if(event_code == LV_EVENT_CLICKED)
    {
        lv_lib_pm_OpenPage(&page_manager, NULL, "SettingPage");   
    }
}

///////////////////// SCREEN init ////////////////////

void ui_HomePage_init(void)
{
    // params init
    ui_desktop_data.scroll_busy = false;
    ui_desktop_data.show_dropdown = false;
    int year; int month; int day; int hour; int minute; int second;
    sys_get_time(&year, &month, &day, &hour, &minute, &second);
    ui_system_para.hour = hour;
    ui_system_para.minute = minute;
    ui_system_para.year = year;
    ui_system_para.month = month;
    ui_system_para.day = day;
    // ui_system_para.wifi_connected = sys_get_wifi_status();

    // home screen
    lv_obj_t * ui_HomeScreen = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_HomeScreen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    // apps‘ container, to contain all apps btns
    lv_obj_t * ui_AppIconContainer = lv_obj_create(ui_HomeScreen);
    lv_obj_set_width(ui_AppIconContainer, ui_desktop_data.witdh * ui_desktop_data.container_total_pages);
    lv_obj_set_height(ui_AppIconContainer, ui_desktop_data.height);
    lv_obj_set_x(ui_AppIconContainer, -ui_desktop_data.app_container_index * ui_desktop_data.witdh);
    lv_obj_set_align(ui_AppIconContainer, LV_ALIGN_LEFT_MID);
    lv_obj_remove_flag(ui_AppIconContainer, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_AppIconContainer, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_AppIconContainer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_AppIconContainer, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_AppIconContainer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // gesture event
    lv_obj_add_event_cb(ui_HomeScreen, ui_event_scroll_gesture, LV_EVENT_GESTURE, ui_AppIconContainer);

    // time Label
    lv_obj_t * ui_TimeLabel = lv_label_create(ui_HomeScreen);
    lv_obj_set_width(ui_TimeLabel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_TimeLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_TimeLabel, 0);
    lv_obj_set_y(ui_TimeLabel, 5);
    lv_obj_set_align(ui_TimeLabel, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_TimeLabel, "11:59");
    lv_obj_set_style_text_font(ui_TimeLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
    char time_str[6];
    sprintf(time_str, "%02d:%02d", ui_system_para.hour, ui_system_para.minute);
    lv_label_set_text(ui_TimeLabel, time_str);

    // wifi connected Label
    ui_WifiLabel = lv_label_create(ui_HomeScreen);
    lv_obj_set_width(ui_WifiLabel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_WifiLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_WifiLabel, 130);
    lv_obj_set_y(ui_WifiLabel, 7);
    lv_obj_set_align(ui_WifiLabel, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_WifiLabel, "");
    lv_obj_set_style_text_font(ui_WifiLabel, &ui_font_iconfont26, LV_PART_MAIN | LV_STATE_DEFAULT);

    // wifi not connected Label
    ui_NoWifiLabel = lv_label_create(ui_HomeScreen);
    lv_obj_set_width(ui_NoWifiLabel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_NoWifiLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_NoWifiLabel, 130);
    lv_obj_set_y(ui_NoWifiLabel, 7);
    lv_obj_set_align(ui_NoWifiLabel, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_NoWifiLabel, "");
    lv_obj_set_style_text_font(ui_NoWifiLabel, &ui_font_iconfont26, LV_PART_MAIN | LV_STATE_DEFAULT);
    if(ui_system_para.wifi_connected == true)
    {
        lv_obj_remove_flag(ui_WifiLabel, LV_OBJ_FLAG_HIDDEN);     /// Flags
        lv_obj_add_flag(ui_NoWifiLabel, LV_OBJ_FLAG_HIDDEN);     /// Flags
    }
    else
    {
        lv_obj_remove_flag(ui_NoWifiLabel, LV_OBJ_FLAG_HIDDEN);     /// Flags
        lv_obj_add_flag(ui_WifiLabel, LV_OBJ_FLAG_HIDDEN);     /// Flags
    }

    // page dots
    for(int i = 0; i < _APP_CONTAINER_MAX_PAGES; i++)
    {
        int16_t start_pos;
        if(_APP_CONTAINER_MAX_PAGES%2==0)
        {
            start_pos = -20*(_APP_CONTAINER_MAX_PAGES/2) + 10;
        }
        else
        {
            start_pos = -20*(_APP_CONTAINER_MAX_PAGES/2 + 1) + 20;
        }
        ui_ScrollDots[i] = lv_obj_create(ui_HomeScreen);
        lv_obj_set_width(ui_ScrollDots[i], 8);
        lv_obj_set_height(ui_ScrollDots[i], 8);
        lv_obj_set_x(ui_ScrollDots[i], start_pos + 20 * i);
        lv_obj_set_y(ui_ScrollDots[i], -10);
        lv_obj_set_align(ui_ScrollDots[i], LV_ALIGN_BOTTOM_MID);
        lv_obj_remove_flag(ui_ScrollDots[i], LV_OBJ_FLAG_SCROLLABLE);      /// Flags
        lv_obj_set_style_radius(ui_ScrollDots[i], 20, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_ScrollDots[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_ScrollDots[i], 96, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(ui_ScrollDots[i], lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(ui_ScrollDots[i], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_set_style_bg_opa(ui_ScrollDots[ui_desktop_data.app_container_index], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // dropdown panel
    lv_obj_t * ui_DropdownPanel = lv_obj_create(ui_HomeScreen);
    lv_obj_set_width(ui_DropdownPanel, ui_desktop_data.witdh);
    lv_obj_set_height(ui_DropdownPanel, ui_desktop_data.height);
    lv_obj_set_x(ui_DropdownPanel, 0);
    if( ui_desktop_data.show_dropdown == true)
    {lv_obj_set_y(ui_DropdownPanel, ui_desktop_data.height);}
    lv_obj_set_y(ui_DropdownPanel, -ui_desktop_data.height);
    lv_obj_set_align(ui_DropdownPanel, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_DropdownPanel,
                       LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_CHAIN);     /// Flags
    lv_obj_set_scrollbar_mode(ui_DropdownPanel, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_scroll_dir(ui_DropdownPanel, LV_DIR_VER);
    lv_obj_set_style_bg_color(ui_DropdownPanel, lv_color_hex(0x323232), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_DropdownPanel, 242, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_DropdownPanel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_DropdownPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_LightSlider = lv_slider_create(ui_DropdownPanel);
    lv_slider_set_value(ui_LightSlider, 50, LV_ANIM_OFF);
    if(lv_slider_get_mode(ui_LightSlider) == LV_SLIDER_MODE_RANGE) lv_slider_set_left_value(ui_LightSlider, 0, LV_ANIM_OFF);
    lv_slider_set_value(ui_LightSlider, ui_system_para.brightness, LV_ANIM_OFF);
    lv_obj_set_width(ui_LightSlider, 50);
    lv_obj_set_height(ui_LightSlider, 150);
    lv_obj_set_x(ui_LightSlider, 50);
    lv_obj_set_y(ui_LightSlider, -10);
    lv_obj_set_align(ui_LightSlider, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_LightSlider, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LightSlider, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LightSlider, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_radius(ui_LightSlider, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LightSlider, lv_color_hex(0x3264C8), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LightSlider, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_radius(ui_LightSlider, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LightSlider, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LightSlider, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(ui_LightSlider, ui_event_LightSlider, LV_EVENT_VALUE_CHANGED, NULL);
    //Compensating for LVGL9.1 draw crash with bar/slider max value when top-padding is nonzero and right-padding is 0
    if(lv_obj_get_style_pad_top(ui_LightSlider, LV_PART_MAIN) > 0) lv_obj_set_style_pad_right(ui_LightSlider,
                                                                                                  lv_obj_get_style_pad_right(ui_LightSlider, LV_PART_MAIN) + 1, LV_PART_MAIN);
    lv_obj_t * ui_LightIcon = lv_label_create(ui_LightSlider);
    lv_obj_set_width(ui_LightIcon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LightIcon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LightIcon, 1);
    lv_obj_set_y(ui_LightIcon, 50);
    lv_obj_set_align(ui_LightIcon, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LightIcon, "");
    lv_obj_set_style_text_color(ui_LightIcon, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LightIcon, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LightIcon, &ui_font_iconfont44, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_SoundSlider = lv_slider_create(ui_DropdownPanel);
    lv_slider_set_value(ui_SoundSlider, 50, LV_ANIM_OFF);
    if(lv_slider_get_mode(ui_SoundSlider) == LV_SLIDER_MODE_RANGE) lv_slider_set_left_value(ui_SoundSlider, 0, LV_ANIM_OFF);
    lv_slider_set_value(ui_SoundSlider, ui_system_para.sound, LV_ANIM_OFF);
    lv_obj_set_width(ui_SoundSlider, 50);
    lv_obj_set_height(ui_SoundSlider, 150);
    lv_obj_set_x(ui_SoundSlider, 115);
    lv_obj_set_y(ui_SoundSlider, -10);
    lv_obj_set_align(ui_SoundSlider, LV_ALIGN_CENTER);
    lv_obj_set_style_radius(ui_SoundSlider, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SoundSlider, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_SoundSlider, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_radius(ui_SoundSlider, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SoundSlider, lv_color_hex(0x3264C8), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_SoundSlider, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_radius(ui_SoundSlider, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SoundSlider, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_SoundSlider, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_SoundSlider, ui_event_SoundSlider, LV_EVENT_RELEASED, NULL);

    //Compensating for LVGL9.1 draw crash with bar/slider max value when top-padding is nonzero and right-padding is 0
    if(lv_obj_get_style_pad_top(ui_SoundSlider, LV_PART_MAIN) > 0) lv_obj_set_style_pad_right(ui_SoundSlider,
                                                                                                  lv_obj_get_style_pad_right(ui_SoundSlider, LV_PART_MAIN) + 1, LV_PART_MAIN);
    lv_obj_t * ui_SoundIcon = lv_label_create(ui_SoundSlider);
    lv_obj_set_width(ui_SoundIcon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SoundIcon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SoundIcon, 1);
    lv_obj_set_y(ui_SoundIcon, 50);
    lv_obj_set_align(ui_SoundIcon, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SoundIcon, "");
    lv_obj_set_style_text_color(ui_SoundIcon, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_SoundIcon, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_SoundIcon, &ui_font_iconfont36, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_WifiBtn = lv_button_create(ui_DropdownPanel);
    lv_obj_set_width(ui_WifiBtn, 60);
    lv_obj_set_height(ui_WifiBtn, 60);
    lv_obj_set_x(ui_WifiBtn, -100);
    lv_obj_set_y(ui_WifiBtn, -50);
    lv_obj_set_align(ui_WifiBtn, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_WifiBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_WifiBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_WifiBtn, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_WifiBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_DropWifiIcon = lv_label_create(ui_WifiBtn);
    lv_obj_set_width(ui_DropWifiIcon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_DropWifiIcon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_DropWifiIcon, LV_ALIGN_CENTER);
    lv_label_set_text(ui_DropWifiIcon, "");
    lv_obj_set_style_text_font(ui_DropWifiIcon, &ui_font_iconfont36, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_BLEBtn = lv_button_create(ui_DropdownPanel);
    lv_obj_set_width(ui_BLEBtn, 60);
    lv_obj_set_height(ui_BLEBtn, 60);
    lv_obj_set_x(ui_BLEBtn, -25);
    lv_obj_set_y(ui_BLEBtn, -50);
    lv_obj_set_align(ui_BLEBtn, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_BLEBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_BLEBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_BLEBtn, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BLEBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_DropBLEIcon = lv_label_create(ui_BLEBtn);
    lv_obj_set_width(ui_DropBLEIcon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_DropBLEIcon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_DropBLEIcon, LV_ALIGN_CENTER);
    lv_label_set_text(ui_DropBLEIcon, "");
    lv_obj_set_style_text_font(ui_DropBLEIcon, &ui_font_iconfont36, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_DropSetBtn = lv_button_create(ui_DropdownPanel);
    lv_obj_set_width(ui_DropSetBtn, 60);
    lv_obj_set_height(ui_DropSetBtn, 60);
    lv_obj_set_x(ui_DropSetBtn, -100);
    lv_obj_set_y(ui_DropSetBtn, 30);
    lv_obj_set_align(ui_DropSetBtn, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_DropSetBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_DropSetBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_DropSetBtn, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_DropSetBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(ui_DropSetBtn, ui_event_SetBtn, LV_EVENT_CLICKED, "SettingPage");

    lv_obj_t * ui_DropSetIcon = lv_label_create(ui_DropSetBtn);
    lv_obj_set_width(ui_DropSetIcon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_DropSetIcon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_DropSetIcon, LV_ALIGN_CENTER);
    lv_label_set_text(ui_DropSetIcon, "");
    lv_obj_set_style_text_font(ui_DropSetIcon, &ui_font_iconfont36, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_HomeBtn = lv_button_create(ui_DropdownPanel);
    lv_obj_set_width(ui_HomeBtn, 60);
    lv_obj_set_height(ui_HomeBtn, 60);
    lv_obj_set_x(ui_HomeBtn, -25);
    lv_obj_set_y(ui_HomeBtn, 30);
    lv_obj_set_align(ui_HomeBtn, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_HomeBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_HomeBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_HomeBtn, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HomeBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_DropHomeIcon = lv_label_create(ui_HomeBtn);
    lv_obj_set_width(ui_DropHomeIcon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_DropHomeIcon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_DropHomeIcon, LV_ALIGN_CENTER);
    lv_label_set_text(ui_DropHomeIcon, "");
    lv_obj_set_style_text_font(ui_DropHomeIcon, &ui_font_iconfont36, LV_PART_MAIN | LV_STATE_DEFAULT);

    // top drag
    lv_obj_t * ui_TopDragPanel = lv_obj_create(ui_HomeScreen);
    lv_obj_set_width(ui_TopDragPanel, ui_desktop_data.witdh);
    lv_obj_set_height(ui_TopDragPanel, 30);
    lv_obj_set_align(ui_TopDragPanel, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_TopDragPanel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_TopDragPanel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_TopDragPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_TopDragPanel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_TopDragPanel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // top drag event
    lv_obj_add_event_cb(ui_TopDragPanel, ui_event_TopDrag, LV_EVENT_PRESSING, ui_DropdownPanel);
    lv_obj_add_event_cb(ui_TopDragPanel, ui_event_TopDrag, LV_EVENT_RELEASED, ui_DropdownPanel);


    // setting app
    lv_obj_t * ui_SettingBtn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_SettingBtn, 70);
    lv_obj_set_height(ui_SettingBtn, 70);
    lv_obj_set_x(ui_SettingBtn, 15);
    lv_obj_set_y(ui_SettingBtn, -45);
    lv_obj_set_align(ui_SettingBtn, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_SettingBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_SettingBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_SettingBtn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SettingBtn, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_SettingBtn, 168, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_SettingIcon = lv_label_create(ui_SettingBtn);
    lv_obj_set_width(ui_SettingIcon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SettingIcon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_SettingIcon, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SettingIcon, "");
    lv_obj_set_style_text_font(ui_SettingIcon, &ui_font_iconfont48, LV_PART_MAIN | LV_STATE_DEFAULT);
    // event
    lv_obj_add_event_cb(ui_SettingBtn, ui_event_AppsBtn, LV_EVENT_CLICKED, "SettingPage");


    // weather app
    lv_obj_t * ui_WeatherBtn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_WeatherBtn, 70);
    lv_obj_set_height(ui_WeatherBtn, 70);
    lv_obj_set_x(ui_WeatherBtn, 110);
    lv_obj_set_y(ui_WeatherBtn, -45);
    lv_obj_set_align(ui_WeatherBtn, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_WeatherBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_WeatherBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_WeatherBtn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(ui_WeatherBtn, &ui_img_weather64_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    // event
    lv_obj_add_event_cb(ui_WeatherBtn, ui_event_AppsBtn, LV_EVENT_CLICKED, "WeatherPage");


    // calendar app
    lv_obj_t * ui_CalendarBtn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_CalendarBtn, 70);
    lv_obj_set_height(ui_CalendarBtn, 70);
    lv_obj_set_x(ui_CalendarBtn, 205);
    lv_obj_set_y(ui_CalendarBtn, -45);
    lv_obj_set_align(ui_CalendarBtn, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_CalendarBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_CalendarBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_CalendarBtn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_CalendarBtn, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_CalendarBtn, 224, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(ui_CalendarBtn, &ui_img_calendar64_png, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_DateLabel = lv_label_create(ui_CalendarBtn);
    lv_obj_set_width(ui_DateLabel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_DateLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_DateLabel, 0);
    lv_obj_set_y(ui_DateLabel, 8);
    lv_obj_set_align(ui_DateLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_DateLabel, "7");
    char date_str[2];
    sprintf(date_str, "%d", ui_system_para.day);
    lv_label_set_text(ui_DateLabel, date_str);
    lv_obj_set_style_text_color(ui_DateLabel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_DateLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_DateLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
    // calendar event
    lv_obj_add_event_cb(ui_CalendarBtn, ui_event_AppsBtn, LV_EVENT_CLICKED, "CalendarPage");

#if 0 //记事本功能暂时不支持
    // memo app
    lv_obj_t * ui_MemoBtn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_MemoBtn, 70);
    lv_obj_set_height(ui_MemoBtn, 70);
    lv_obj_set_x(ui_MemoBtn, 15);
    lv_obj_set_y(ui_MemoBtn, 55);
    lv_obj_set_align(ui_MemoBtn, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_MemoBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_MemoBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_MemoBtn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_MemoBtn, lv_color_hex(0xB83B5E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_MemoBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(ui_MemoBtn, &ui_img_memo64_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    // event
    lv_obj_add_event_cb(ui_MemoBtn, ui_event_AppsBtn, LV_EVENT_CLICKED, "MemoPage");
#endif
    //GameDinoPage
    lv_obj_t * ui_DinosoBtn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_DinosoBtn, 70);
    lv_obj_set_height(ui_DinosoBtn, 70);
    lv_obj_set_x(ui_DinosoBtn, 15);
    lv_obj_set_y(ui_DinosoBtn, 55);
    lv_obj_set_align(ui_DinosoBtn, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_DinosoBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_DinosoBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_DinosoBtn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_DinosoBtn, lv_color_hex(0xB83B5E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_DinosoBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(ui_DinosoBtn, &dinoso, LV_PART_MAIN | LV_STATE_DEFAULT);
    // event
    lv_obj_add_event_cb(ui_DinosoBtn, ui_event_AppsBtn, LV_EVENT_CLICKED, "GameDinoPage");

#if 1
    // game muyu app
    lv_obj_t * ui_GameMuyuBtn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_GameMuyuBtn, 70);
    lv_obj_set_height(ui_GameMuyuBtn, 70);
    lv_obj_set_x(ui_GameMuyuBtn, 110);
    lv_obj_set_y(ui_GameMuyuBtn, 55);
    lv_obj_set_align(ui_GameMuyuBtn, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_GameMuyuBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_GameMuyuBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_GameMuyuBtn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_GameMuyuBtn, lv_color_hex(0x355C7D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_GameMuyuBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    // event
    lv_obj_add_event_cb(ui_GameMuyuBtn, ui_event_AppsBtn, LV_EVENT_CLICKED, "GameMuyuPage");

    lv_obj_t * ui_MuyuIcon = lv_label_create(ui_GameMuyuBtn);
    lv_obj_set_width(ui_MuyuIcon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_MuyuIcon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_MuyuIcon, LV_ALIGN_CENTER);
    lv_label_set_text(ui_MuyuIcon, "");
    lv_obj_set_style_text_color(ui_MuyuIcon, lv_color_hex(0xF6AB75), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_MuyuIcon, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_MuyuIcon, &ui_font_iconfont36, LV_PART_MAIN | LV_STATE_DEFAULT);

#endif
    // game 2048 app
    lv_obj_t * ui_Game2048Btn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_Game2048Btn, 70);
    lv_obj_set_height(ui_Game2048Btn, 70);
    lv_obj_set_x(ui_Game2048Btn, 205);
    lv_obj_set_y(ui_Game2048Btn, 55);
    lv_obj_set_align(ui_Game2048Btn, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_Game2048Btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_Game2048Btn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Game2048Btn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Game2048Btn, lv_color_hex(0x11999E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Game2048Btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_Icon2048 = lv_label_create(ui_Game2048Btn);
    lv_obj_set_width(ui_Icon2048, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Icon2048, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Icon2048, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Icon2048, "");
    lv_obj_set_style_text_color(ui_Icon2048, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Icon2048, 196, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Icon2048, &ui_font_iconfont48, LV_PART_MAIN | LV_STATE_DEFAULT);
    // event
    lv_obj_add_event_cb(ui_Game2048Btn, ui_event_AppsBtn, LV_EVENT_CLICKED, "Game2048Page");


    // AI Chat Bot app
    lv_obj_t * ui_AIChatBtn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_AIChatBtn, 70);
    lv_obj_set_height(ui_AIChatBtn, 70);
    lv_obj_set_x(ui_AIChatBtn, 335);
    lv_obj_set_y(ui_AIChatBtn, -45);
    lv_obj_set_align(ui_AIChatBtn, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_AIChatBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_AIChatBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_AIChatBtn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_AIChatBtn, lv_color_hex(0xA56CC1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_AIChatBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_BotIcon = lv_label_create(ui_AIChatBtn);
    lv_obj_set_width(ui_BotIcon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_BotIcon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_BotIcon, LV_ALIGN_CENTER);
    lv_label_set_text(ui_BotIcon, "");
    lv_obj_set_style_text_color(ui_BotIcon, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_BotIcon, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_BotIcon, &ui_font_iconfont48, LV_PART_MAIN | LV_STATE_DEFAULT);
    // AI Chat event
    lv_obj_add_event_cb(ui_AIChatBtn, ui_event_AppsBtn, LV_EVENT_CLICKED, "ChatBotPage");


    // Camera app
    lv_obj_t * ui_CameraBtn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_CameraBtn, 70);
    lv_obj_set_height(ui_CameraBtn, 70);
    lv_obj_set_x(ui_CameraBtn, 430);
    lv_obj_set_y(ui_CameraBtn, -45);
    lv_obj_set_align(ui_CameraBtn, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_CameraBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_CameraBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_CameraBtn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_CameraBtn, lv_color_hex(0xFF9A00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_CameraBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_CameraIcon = lv_label_create(ui_CameraBtn);
    lv_obj_set_width(ui_CameraIcon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_CameraIcon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_CameraIcon, 2);
    lv_obj_set_y(ui_CameraIcon, 0);
    lv_obj_set_align(ui_CameraIcon, LV_ALIGN_CENTER);
    lv_label_set_text(ui_CameraIcon, "");
    lv_obj_set_style_text_color(ui_CameraIcon, lv_color_hex(0xF6F7D7), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_CameraIcon, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_CameraIcon, &ui_font_iconfont48, LV_PART_MAIN | LV_STATE_DEFAULT);
    // event
    lv_obj_add_event_cb(ui_CameraBtn, ui_event_AppsBtn, LV_EVENT_CLICKED, "CameraPage");

    #if 0
    // YOLO app
    lv_obj_t * ui_YOLOBtn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_YOLOBtn, 70);
    lv_obj_set_height(ui_YOLOBtn, 70);
    lv_obj_set_x(ui_YOLOBtn, 525);
    lv_obj_set_y(ui_YOLOBtn, -45);
    lv_obj_set_align(ui_YOLOBtn, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_YOLOBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_YOLOBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_YOLOBtn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_YOLOBtn, lv_color_hex(0xEEEF48), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_YOLOBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_YOLOLabel = lv_label_create(ui_YOLOBtn);
    lv_obj_set_width(ui_YOLOLabel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_YOLOLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_YOLOLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_YOLOLabel, "YOLO");
    lv_obj_set_style_text_color(ui_YOLOLabel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_YOLOLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_YOLOLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
    // event
    lv_obj_add_event_cb(ui_YOLOBtn, ui_event_AppsBtn, LV_EVENT_CLICKED, "YOLOPage");

    #endif
    // Game Memory app
    lv_obj_t * ui_GameMemBtn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_GameMemBtn, 70);
    lv_obj_set_height(ui_GameMemBtn, 70);
    lv_obj_set_x(ui_GameMemBtn, 335);
    lv_obj_set_y(ui_GameMemBtn, 55);
    lv_obj_set_align(ui_GameMemBtn, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_GameMemBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_GameMemBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_GameMemBtn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_GameMemBtn, lv_color_hex(0xDBE2EF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_GameMemBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(ui_GameMemBtn, &ui_img_gamememory64_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    // event
    lv_obj_add_event_cb(ui_GameMemBtn, ui_event_AppsBtn, LV_EVENT_CLICKED, "GameMemoryPage");

    // Draw app
    lv_obj_t * ui_DrawBtn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_DrawBtn, 70);
    lv_obj_set_height(ui_DrawBtn, 70);
    lv_obj_set_x(ui_DrawBtn, 430);
    lv_obj_set_y(ui_DrawBtn, 55);
    lv_obj_set_align(ui_DrawBtn, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_DrawBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_DrawBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_DrawBtn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_DrawBtn, lv_color_hex(0x86A8E5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_DrawBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(ui_DrawBtn, &ui_img_paint60_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    // event
    lv_obj_add_event_cb(ui_DrawBtn, ui_event_AppsBtn, LV_EVENT_CLICKED, "DrawPage");

    // Calculator app
    lv_obj_t * ui_CalculatorBtn = lv_button_create(ui_AppIconContainer);
    lv_obj_set_width(ui_CalculatorBtn, 70);
    lv_obj_set_height(ui_CalculatorBtn, 70);
    lv_obj_set_x(ui_CalculatorBtn, 525);
    lv_obj_set_y(ui_CalculatorBtn, 55);
    lv_obj_set_align(ui_CalculatorBtn, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_CalculatorBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_CalculatorBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_CalculatorBtn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_CalculatorBtn, lv_color_hex(0x707070), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_CalculatorBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_CalculatorIcon = lv_label_create(ui_CalculatorBtn);
    lv_obj_set_width(ui_CalculatorIcon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_CalculatorIcon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_CalculatorIcon, LV_ALIGN_CENTER);
    lv_label_set_text(ui_CalculatorIcon, "");
    lv_obj_set_style_text_color(ui_CalculatorIcon, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_CalculatorIcon, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_CalculatorIcon, &ui_font_iconfont48, LV_PART_MAIN | LV_STATE_DEFAULT);
    // event
    lv_obj_add_event_cb(ui_CalculatorBtn, ui_event_AppsBtn, LV_EVENT_CLICKED, "CalculatorPage");

    // timer
    ui_home_timer = lv_timer_create(ui_home_timer_cb, 5000, ui_TimeLabel);

    // load page
    lv_scr_load_anim(ui_HomeScreen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);

}

///////////////////// SCREEN deinit ////////////////////

void ui_HomePage_deinit(void)
{
    lv_timer_delete(ui_home_timer);
    return;
}