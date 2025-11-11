#include "ui_SettingPage.h"


// #include "lv_font.h"

///////////////////// VARIABLES ////////////////////

// start year is 2025
lv_obj_t * ui_SettingRootMenu;
lv_obj_t * ui_LabelLocationName;
lv_obj_t * ui_LabelLocationName2;
lv_obj_t * ui_LabelBrightValue;  // 亮度数值显示标签
lv_obj_t * ui_LabelSoundValue;   // 音量数值显示标签
///////////////////// ANIMATIONS ////////////////////


///////////////////// FUNCTIONS ////////////////////

static void back_event_handler(lv_event_t * e)
{
    lv_obj_t * page_now = lv_event_get_user_data(e);
    // open pre page & save system hardware paras
    if(page_now == ui_SettingRootMenu)
    {
        PR_DEBUG("Save system parameters.");
        sys_save_system_parameters(sys_config_path, &ui_system_para);
    }
    lv_lib_pm_OpenPrePage(&page_manager);
}

static void sub_menu_click_cb(lv_event_t * e)
{
    const char * page_name = lv_event_get_user_data(e);
    if(page_name == NULL) return;
    lv_lib_pm_OpenPage(&page_manager, NULL, (char *)page_name);
}

static void light_slider_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        // set system brightness
        ui_system_para.brightness = lv_slider_get_value(lv_event_get_target(e));
        sys_set_lcd_brightness(ui_system_para.brightness);
        // 更新数值显示
        if(ui_LabelBrightValue != NULL) {
            char value_str[8];
            sprintf(value_str, "%d%%", ui_system_para.brightness);
            lv_label_set_text(ui_LabelBrightValue, value_str);
        }
    }
}

static void sound_slider_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_RELEASED || code == LV_EVENT_VALUE_CHANGED) {
        // set system sound
        ui_system_para.sound = lv_slider_get_value(lv_event_get_target(e));
        sys_set_volume(ui_system_para.sound);
        // 更新数值显示
        if(ui_LabelSoundValue != NULL) {
            char value_str[8];
            sprintf(value_str, "%d%%", ui_system_para.sound);
            lv_label_set_text(ui_LabelSoundValue, value_str);
        }
    }
}

static void AutoTime_event_cb(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    lv_obj_t * ui_TimeDateSection2 = lv_event_get_user_data(e);

    if(event_code == LV_EVENT_VALUE_CHANGED &&  lv_obj_has_state(target, LV_STATE_CHECKED)) {
        lv_obj_add_flag(ui_TimeDateSection2, LV_OBJ_FLAG_HIDDEN);
        ui_system_para.auto_time = true;
        // get time via network
        if(sys_get_time_from_ntp("ntp.aliyun.com", &ui_system_para.year, &ui_system_para.month, &ui_system_para.day, &ui_system_para.hour, &ui_system_para.minute, NULL))
        {
            // show msg box
            ui_msgbox_info("Error", "Auto NTP time get fail.");
        }
        else
        {
            sys_set_time(ui_system_para.year, ui_system_para.month, ui_system_para.day, ui_system_para.hour, ui_system_para.minute, 0);
            // show msg box
            ui_msgbox_info("Note", "Auto NTP time get success.");
        }
    }
    if(event_code == LV_EVENT_VALUE_CHANGED &&  !lv_obj_has_state(target, LV_STATE_CHECKED)) {
        lv_obj_remove_flag(ui_TimeDateSection2, LV_OBJ_FLAG_HIDDEN);
        ui_system_para.auto_time = false;
    }
}

static void IPlocatiing_event_cb(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    lv_obj_t * ui_LocationSection2 = lv_event_get_user_data(e);

    if(event_code == LV_EVENT_VALUE_CHANGED &&  lv_obj_has_state(target, LV_STATE_CHECKED)) {
        lv_obj_add_flag(ui_LocationSection2, LV_OBJ_FLAG_HIDDEN);
        ui_system_para.auto_location = true;
        // get location via network
        if(sys_get_auto_location_by_tuya_weather(&ui_system_para.location) == 0) {
            lv_label_set_text(ui_LabelLocationName, ui_system_para.location.city);
            // show msg box
            ui_msgbox_info("Note", "Auto Location get success.");
        }
        else {
            // show msg box
            ui_msgbox_info("Error", "Auto Location get failed.");
        }
    }
    if(event_code == LV_EVENT_VALUE_CHANGED &&  !lv_obj_has_state(target, LV_STATE_CHECKED)) {
        lv_obj_remove_flag(ui_LocationSection2, LV_OBJ_FLAG_HIDDEN);
        ui_system_para.auto_location = false;
    }
}

static void time_set_confirm_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ui_TimeSetSection1 = lv_event_get_user_data(e);
    lv_obj_t * ui_RollerHour = lv_obj_get_child(ui_TimeSetSection1, 0);
    lv_obj_t * ui_RollerMinute = lv_obj_get_child(ui_TimeSetSection1, 1);
    
    if(code == LV_EVENT_CLICKED) {
        // set system time
        sys_get_time(&ui_system_para.year, &ui_system_para.month, &ui_system_para.day, &ui_system_para.hour, &ui_system_para.minute, NULL);
        ui_system_para.hour = lv_roller_get_selected(ui_RollerHour);
        ui_system_para.minute = lv_roller_get_selected(ui_RollerMinute);
        sys_set_time(ui_system_para.year, ui_system_para.month, ui_system_para.day, ui_system_para.hour, ui_system_para.minute, 0);
        // show msg box
        ui_msgbox_info("Note", "Time set success.");
    }
}

static void date_set_confirm_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ui_DateSetSection1 = lv_event_get_user_data(e);
    lv_obj_t * ui_RollerYear = lv_obj_get_child(ui_DateSetSection1, 0);
    lv_obj_t * ui_RollerMonth = lv_obj_get_child(ui_DateSetSection1, 1);
    lv_obj_t * ui_RollerDay = lv_obj_get_child(ui_DateSetSection1, 2);
    if(code == LV_EVENT_CLICKED) {
        // set system date
        sys_get_time(&ui_system_para.year, &ui_system_para.month, &ui_system_para.day, &ui_system_para.hour, &ui_system_para.minute, NULL);
        ui_system_para.year = lv_roller_get_selected(ui_RollerYear) + 2025;
        ui_system_para.month = lv_roller_get_selected(ui_RollerMonth) + 1;
        ui_system_para.day = lv_roller_get_selected(ui_RollerDay) + 1;
        sys_set_time(ui_system_para.year, ui_system_para.month, ui_system_para.day, ui_system_para.hour, ui_system_para.minute, 0);
        // show msg box
        ui_msgbox_info("Note", "Date set success.");
    }
}

static void adcode_set_confirm_cb(lv_event_t * e)
{
    lv_obj_t * ui_AdcodeSetSection1 = lv_event_get_user_data(e);
    lv_obj_t * ui_RollerLocation0 = lv_obj_get_child(ui_AdcodeSetSection1, 0);
    lv_obj_t * ui_RollerLocation1 = lv_obj_get_child(ui_AdcodeSetSection1, 1);
    lv_obj_t * ui_RollerLocation2 = lv_obj_get_child(ui_AdcodeSetSection1, 2);
    lv_obj_t * ui_RollerLocation3 = lv_obj_get_child(ui_AdcodeSetSection1, 3);
    lv_obj_t * ui_RollerLocation4 = lv_obj_get_child(ui_AdcodeSetSection1, 4);
    lv_obj_t * ui_RollerLocation5 = lv_obj_get_child(ui_AdcodeSetSection1, 5);

    // set system location
    ui_system_para.location.adcode[0] = lv_roller_get_selected(ui_RollerLocation0) + '0';
    ui_system_para.location.adcode[1] = lv_roller_get_selected(ui_RollerLocation1) + '0';
    ui_system_para.location.adcode[2] = lv_roller_get_selected(ui_RollerLocation2) + '0';
    ui_system_para.location.adcode[3] = lv_roller_get_selected(ui_RollerLocation3) + '0';
    ui_system_para.location.adcode[4] = lv_roller_get_selected(ui_RollerLocation4) + '0';
    ui_system_para.location.adcode[5] = lv_roller_get_selected(ui_RollerLocation5) + '0';
    const char * city_name = sys_get_city_name();
    if(city_name) {
        strcpy(ui_system_para.location.city, city_name);
        lv_label_set_text(ui_LabelLocationName2, city_name);
        // show msg box
        ui_msgbox_info("Note", "Location set success.");
    }
    else {
        // show msg box
        ui_msgbox_info("Error", "Location set failed. not in the location map.");
        // default 110101
        lv_roller_set_selected(ui_RollerLocation0, 1, LV_ANIM_OFF);
        lv_roller_set_selected(ui_RollerLocation1, 1, LV_ANIM_OFF);
        lv_roller_set_selected(ui_RollerLocation2, 0, LV_ANIM_OFF);
        lv_roller_set_selected(ui_RollerLocation3, 1, LV_ANIM_OFF);
        lv_roller_set_selected(ui_RollerLocation4, 0, LV_ANIM_OFF);
        lv_roller_set_selected(ui_RollerLocation5, 1, LV_ANIM_OFF);
    }
}

///////////////////// sub screens ////////////////////

static void ui_CommonMenu_init(void)
{
    lv_obj_t * ui_CommonMenu = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_CommonMenu, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t * ui_BtnBackCommon = lv_button_create(ui_CommonMenu);
    lv_obj_set_width(ui_BtnBackCommon, 50);
    lv_obj_set_height(ui_BtnBackCommon, 45);
    lv_obj_set_x(ui_BtnBackCommon, 5);
    lv_obj_set_y(ui_BtnBackCommon, 0);
    lv_obj_add_flag(ui_BtnBackCommon, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_BtnBackCommon, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_BtnBackCommon, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BtnBackCommon, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_BtnBackCommon, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_BtnBackCommon, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_BtnBackCommon, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_BtnBackCommon, 64, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(ui_BtnBackCommon, back_event_handler, LV_EVENT_CLICKED, ui_BtnBackCommon);

    lv_obj_t * ui_LabelBackCommon = lv_label_create(ui_BtnBackCommon);
    lv_obj_set_width(ui_LabelBackCommon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelBackCommon, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(ui_LabelBackCommon, "");
    lv_obj_set_style_text_font(ui_LabelBackCommon, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelCommonMenu = lv_obj_create(ui_CommonMenu);
    lv_obj_set_width(ui_PanelCommonMenu, 320);
    lv_obj_set_height(ui_PanelCommonMenu, 200);
    lv_obj_set_x(ui_PanelCommonMenu, 0);
    lv_obj_set_y(ui_PanelCommonMenu, 40);
    lv_obj_set_align(ui_PanelCommonMenu, LV_ALIGN_TOP_MID);
    lv_obj_set_scroll_dir(ui_PanelCommonMenu, LV_DIR_VER);
    lv_obj_set_style_bg_color(ui_PanelCommonMenu, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelCommonMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelCommonMenu, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelCommonMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_CommonSection1 = lv_obj_create(ui_PanelCommonMenu);
    lv_obj_set_width(ui_CommonSection1, 300);
    lv_obj_set_height(ui_CommonSection1, 130);
    lv_obj_set_x(ui_CommonSection1, 0);
    lv_obj_set_y(ui_CommonSection1, -10);
    lv_obj_set_align(ui_CommonSection1, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_CommonSection1, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_CommonSection1, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_CommonSection1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_CommonSection1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_CommonSection1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_CommonSection1, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_CommonSection1, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_LabelBright = lv_label_create(ui_CommonSection1);
    lv_obj_set_width(ui_LabelBright, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelBright, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelBright, 0);
    lv_obj_set_y(ui_LabelBright, -40);
    lv_obj_set_align(ui_LabelBright, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_LabelBright, "Brightness");
    lv_obj_set_style_text_font(ui_LabelBright, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_IconBright = lv_label_create(ui_CommonSection1);
    lv_obj_set_width(ui_IconBright, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconBright, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_IconBright, 0);
    lv_obj_set_y(ui_IconBright, -15);
    lv_obj_set_align(ui_IconBright, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_IconBright, "");
    lv_obj_set_style_text_font(ui_IconBright, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_BrightSlider = lv_slider_create(ui_CommonSection1);
    // 设置滑块范围 0-100
    lv_slider_set_range(ui_BrightSlider, 0, 100);
    // 确保默认值为100
    if(ui_system_para.brightness == 0 || ui_system_para.brightness < 10) {
        ui_system_para.brightness = 100;
    }
    lv_slider_set_value(ui_BrightSlider, ui_system_para.brightness, LV_ANIM_OFF);
    if(lv_slider_get_mode(ui_BrightSlider) == LV_SLIDER_MODE_RANGE) lv_slider_set_left_value(ui_BrightSlider, 0,
                                                                                                 LV_ANIM_OFF);
    lv_obj_set_width(ui_BrightSlider, 240);
    lv_obj_set_height(ui_BrightSlider, 15);
    lv_obj_set_x(ui_BrightSlider, 15);
    lv_obj_set_y(ui_BrightSlider, -15);
    lv_obj_set_align(ui_BrightSlider, LV_ALIGN_CENTER);

    lv_obj_add_event_cb(ui_BrightSlider, light_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 创建亮度数值显示标签
    ui_LabelBrightValue = lv_label_create(ui_CommonSection1);
    lv_obj_set_width(ui_LabelBrightValue, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_LabelBrightValue, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_LabelBrightValue, 0);
    lv_obj_set_y(ui_LabelBrightValue, -15);
    lv_obj_set_align(ui_LabelBrightValue, LV_ALIGN_RIGHT_MID);
    char bright_str[8];
    sprintf(bright_str, "%d%%", ui_system_para.brightness);
    lv_label_set_text(ui_LabelBrightValue, bright_str);
    lv_obj_set_style_text_font(ui_LabelBrightValue, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    //Compensating for LVGL9.1 draw crash with bar/slider max value when top-padding is nonzero and right-padding is 0
    if(lv_obj_get_style_pad_top(ui_BrightSlider, LV_PART_MAIN) > 0) lv_obj_set_style_pad_right(ui_BrightSlider,
                                                                                                   lv_obj_get_style_pad_right(ui_BrightSlider, LV_PART_MAIN) + 1, LV_PART_MAIN);
    lv_obj_t * ui_SoundSlider = lv_slider_create(ui_CommonSection1);
    // 设置滑块范围 0-100
    lv_slider_set_range(ui_SoundSlider, 0, 100);
    // 确保默认值为100
    if(ui_system_para.sound == 0 || ui_system_para.sound < 10) {
        ui_system_para.sound = 100;
    }
    lv_slider_set_value(ui_SoundSlider, ui_system_para.sound, LV_ANIM_OFF);
    if(lv_slider_get_mode(ui_SoundSlider) == LV_SLIDER_MODE_RANGE) lv_slider_set_left_value(ui_SoundSlider, 0, LV_ANIM_OFF);
    lv_obj_set_width(ui_SoundSlider, 240);
    lv_obj_set_height(ui_SoundSlider, 15);
    lv_obj_set_x(ui_SoundSlider, 15);
    lv_obj_set_y(ui_SoundSlider, 45);
    lv_obj_set_align(ui_SoundSlider, LV_ALIGN_CENTER);

    lv_obj_add_event_cb(ui_SoundSlider, sound_slider_event_cb, LV_EVENT_RELEASED | LV_EVENT_VALUE_CHANGED, NULL);

    // 创建音量数值显示标签
    ui_LabelSoundValue = lv_label_create(ui_CommonSection1);
    lv_obj_set_width(ui_LabelSoundValue, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_LabelSoundValue, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_LabelSoundValue, 0);
    lv_obj_set_y(ui_LabelSoundValue, 45);
    lv_obj_set_align(ui_LabelSoundValue, LV_ALIGN_RIGHT_MID);
    char sound_str[8];
    sprintf(sound_str, "%d%%", ui_system_para.sound);
    lv_label_set_text(ui_LabelSoundValue, sound_str);
    lv_obj_set_style_text_font(ui_LabelSoundValue, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    //Compensating for LVGL9.1 draw crash with bar/slider max value when top-padding is nonzero and right-padding is 0
    if(lv_obj_get_style_pad_top(ui_SoundSlider, LV_PART_MAIN) > 0) lv_obj_set_style_pad_right(ui_SoundSlider,
                                                                                                  lv_obj_get_style_pad_right(ui_SoundSlider, LV_PART_MAIN) + 1, LV_PART_MAIN);
    lv_obj_t * ui_IconSound = lv_label_create(ui_CommonSection1);
    lv_obj_set_width(ui_IconSound, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconSound, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_IconSound, 0);
    lv_obj_set_y(ui_IconSound, 45);
    lv_obj_set_align(ui_IconSound, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_IconSound, "");
    lv_obj_set_style_text_font(ui_IconSound, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_LabelSound = lv_label_create(ui_CommonSection1);
    lv_obj_set_width(ui_LabelSound, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelSound, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelSound, 0);
    lv_obj_set_y(ui_LabelSound, 20);
    lv_obj_set_align(ui_LabelSound, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_LabelSound, "Sound");
    lv_obj_set_style_text_font(ui_LabelSound, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    // load page
    lv_scr_load_anim(ui_CommonMenu, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

static void ui_CommonMenu_deinit(void)
{
    // deinit
}

static void ui_TimeDateMenu_init(void)
{
    lv_obj_t * ui_TimeDateMenu = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_TimeDateMenu, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t * ui_BtnBackTimeDate = lv_button_create(ui_TimeDateMenu);
    lv_obj_set_width(ui_BtnBackTimeDate, 50);
    lv_obj_set_height(ui_BtnBackTimeDate, 45);
    lv_obj_set_x(ui_BtnBackTimeDate, 5);
    lv_obj_set_y(ui_BtnBackTimeDate, 0);
    lv_obj_add_flag(ui_BtnBackTimeDate, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_BtnBackTimeDate, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_BtnBackTimeDate, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BtnBackTimeDate, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_BtnBackTimeDate, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_BtnBackTimeDate, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_BtnBackTimeDate, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_BtnBackTimeDate, 64, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(ui_BtnBackTimeDate, back_event_handler, LV_EVENT_CLICKED, ui_BtnBackTimeDate);

    lv_obj_t * ui_LabelBackTimeDate = lv_label_create(ui_BtnBackTimeDate);
    lv_obj_set_width(ui_LabelBackTimeDate, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelBackTimeDate, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(ui_LabelBackTimeDate, "");
    lv_obj_set_style_text_font(ui_LabelBackTimeDate, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelTimeDateMenu = lv_obj_create(ui_TimeDateMenu);
    lv_obj_set_width(ui_PanelTimeDateMenu, 320);
    lv_obj_set_height(ui_PanelTimeDateMenu, 200);
    lv_obj_set_x(ui_PanelTimeDateMenu, 0);
    lv_obj_set_y(ui_PanelTimeDateMenu, 40);
    lv_obj_set_align(ui_PanelTimeDateMenu, LV_ALIGN_TOP_MID);
    lv_obj_set_scroll_dir(ui_PanelTimeDateMenu, LV_DIR_VER);
    lv_obj_set_style_bg_color(ui_PanelTimeDateMenu, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelTimeDateMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelTimeDateMenu, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelTimeDateMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_TimeDateSection1 = lv_obj_create(ui_PanelTimeDateMenu);
    lv_obj_set_width(ui_TimeDateSection1, 300);
    lv_obj_set_height(ui_TimeDateSection1, 50);
    lv_obj_set_x(ui_TimeDateSection1, 0);
    lv_obj_set_y(ui_TimeDateSection1, -10);
    lv_obj_set_align(ui_TimeDateSection1, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_TimeDateSection1, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_TimeDateSection1, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_TimeDateSection1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_TimeDateSection1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_TimeDateSection1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_TimeDateSection1, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_TimeDateSection1, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_IconAutoTime = lv_label_create(ui_TimeDateSection1);
    lv_obj_set_width(ui_IconAutoTime, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconAutoTime, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_IconAutoTime, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_IconAutoTime, "");
    lv_obj_set_style_text_font(ui_IconAutoTime, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_LabelAutoTime = lv_label_create(ui_TimeDateSection1);
    lv_obj_set_width(ui_LabelAutoTime, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAutoTime, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelAutoTime, 30);
    lv_obj_set_y(ui_LabelAutoTime, 0);
    lv_obj_set_align(ui_LabelAutoTime, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_LabelAutoTime, "Auto Update");
    lv_obj_set_style_text_font(ui_LabelAutoTime, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_SwitchAutoTime = lv_switch_create(ui_TimeDateSection1);
    lv_obj_set_width(ui_SwitchAutoTime, 50);
    lv_obj_set_height(ui_SwitchAutoTime, 25);
    lv_obj_set_align(ui_SwitchAutoTime, LV_ALIGN_RIGHT_MID);
    lv_obj_add_state(ui_SwitchAutoTime, ui_system_para.auto_time ? LV_STATE_CHECKED : 0);

    lv_obj_t * ui_TimeDateSection2 = lv_obj_create(ui_PanelTimeDateMenu);
    lv_obj_set_width(ui_TimeDateSection2, 300);
    lv_obj_set_height(ui_TimeDateSection2, 80);
    lv_obj_set_x(ui_TimeDateSection2, 0);
    lv_obj_set_y(ui_TimeDateSection2, 80);
    lv_obj_set_align(ui_TimeDateSection2, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_TimeDateSection2, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_TimeDateSection2, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_TimeDateSection2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_TimeDateSection2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_TimeDateSection2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_TimeDateSection2, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_TimeDateSection2, 255, LV_PART_MAIN | LV_STATE_PRESSED);
    
    lv_obj_t * ui_PanelTimeSet = lv_obj_create(ui_TimeDateSection2);
    lv_obj_set_width(ui_PanelTimeSet, 300);
    lv_obj_set_height(ui_PanelTimeSet, 40);
    lv_obj_set_x(ui_PanelTimeSet, 0);
    lv_obj_set_y(ui_PanelTimeSet, -15);
    lv_obj_set_align(ui_PanelTimeSet, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_PanelTimeSet, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelTimeSet, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelTimeSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelTimeSet, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelTimeSet, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_PanelTimeSet, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_PanelTimeSet, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_LabelTimeSet = lv_label_create(ui_PanelTimeSet);
    lv_obj_set_width(ui_LabelTimeSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelTimeSet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelTimeSet, 30);
    lv_obj_set_y(ui_LabelTimeSet, 0);
    lv_obj_set_align(ui_LabelTimeSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_LabelTimeSet, "Time Set");
    lv_obj_set_style_text_font(ui_LabelTimeSet, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_IconTimeSet = lv_label_create(ui_PanelTimeSet);
    lv_obj_set_width(ui_IconTimeSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconTimeSet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_IconTimeSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_IconTimeSet, "");
    lv_obj_set_style_text_font(ui_IconTimeSet, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelDateSet = lv_obj_create(ui_TimeDateSection2);
    lv_obj_set_width(ui_PanelDateSet, 300);
    lv_obj_set_height(ui_PanelDateSet, 40);
    lv_obj_set_x(ui_PanelDateSet, 0);
    lv_obj_set_y(ui_PanelDateSet, 25);
    lv_obj_set_align(ui_PanelDateSet, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_PanelDateSet, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelDateSet, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelDateSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelDateSet, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelDateSet, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_PanelDateSet, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_PanelDateSet, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_LabelDateSet = lv_label_create(ui_PanelDateSet);
    lv_obj_set_width(ui_LabelDateSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelDateSet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelDateSet, 30);
    lv_obj_set_y(ui_LabelDateSet, 0);
    lv_obj_set_align(ui_LabelDateSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_LabelDateSet, "Date Set");
    lv_obj_set_style_text_font(ui_LabelDateSet, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_IconDateSet = lv_label_create(ui_PanelDateSet);
    lv_obj_set_width(ui_IconDateSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconDateSet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_IconDateSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_IconDateSet, "");
    lv_obj_set_style_text_font(ui_IconDateSet, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);

    if(ui_system_para.auto_time == true) {
        lv_obj_add_flag(ui_TimeDateSection2, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_event_cb(ui_SwitchAutoTime, AutoTime_event_cb, LV_EVENT_ALL, ui_TimeDateSection2);
    lv_obj_add_event_cb(ui_PanelTimeSet, sub_menu_click_cb, LV_EVENT_CLICKED, "TimeSetMenu");
    lv_obj_add_event_cb(ui_PanelDateSet, sub_menu_click_cb, LV_EVENT_CLICKED, "DateSetMenu");

    // load page
    lv_scr_load_anim(ui_TimeDateMenu, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);

}

static void ui_TimeDateMenu_deinit(void)
{
    // deinit
}

static void ui_LocationMenu_init(void)
{
    lv_obj_t * ui_LocationMenu = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_LocationMenu, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t * ui_BtnBackLocation = lv_button_create(ui_LocationMenu);
    lv_obj_set_width(ui_BtnBackLocation, 50);
    lv_obj_set_height(ui_BtnBackLocation, 45);
    lv_obj_set_x(ui_BtnBackLocation, 5);
    lv_obj_set_y(ui_BtnBackLocation, 0);
    lv_obj_add_flag(ui_BtnBackLocation, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_BtnBackLocation, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_BtnBackLocation, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BtnBackLocation, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_BtnBackLocation, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_BtnBackLocation, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_BtnBackLocation, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_BtnBackLocation, 64, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(ui_BtnBackLocation, back_event_handler, LV_EVENT_CLICKED, ui_BtnBackLocation);

    lv_obj_t * ui_LabelBackLocation = lv_label_create(ui_BtnBackLocation);
    lv_obj_set_width(ui_LabelBackLocation, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelBackLocation, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(ui_LabelBackLocation, "");
    lv_obj_set_style_text_font(ui_LabelBackLocation, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelLocationMenu = lv_obj_create(ui_LocationMenu);
    lv_obj_set_width(ui_PanelLocationMenu, 320);
    lv_obj_set_height(ui_PanelLocationMenu, 200);
    lv_obj_set_x(ui_PanelLocationMenu, 0);
    lv_obj_set_y(ui_PanelLocationMenu, 40);
    lv_obj_set_align(ui_PanelLocationMenu, LV_ALIGN_TOP_MID);
    lv_obj_set_scroll_dir(ui_PanelLocationMenu, LV_DIR_VER);
    lv_obj_set_style_bg_color(ui_PanelLocationMenu, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelLocationMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelLocationMenu, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelLocationMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_LocationSection1 = lv_obj_create(ui_PanelLocationMenu);
    lv_obj_set_width(ui_LocationSection1, 300);
    lv_obj_set_height(ui_LocationSection1, 50);
    lv_obj_set_x(ui_LocationSection1, 0);
    lv_obj_set_y(ui_LocationSection1, -10);
    lv_obj_set_align(ui_LocationSection1, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_LocationSection1, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_LocationSection1, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LocationSection1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_LocationSection1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_LocationSection1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LocationSection1, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_LocationSection1, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_IconIPlocating = lv_label_create(ui_LocationSection1);
    lv_obj_set_width(ui_IconIPlocating, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconIPlocating, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_IconIPlocating, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_IconIPlocating, "");
    lv_obj_set_style_text_font(ui_IconIPlocating, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_LabelIPlocating = lv_label_create(ui_LocationSection1);
    lv_obj_set_width(ui_LabelIPlocating, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelIPlocating, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelIPlocating, 30);
    lv_obj_set_y(ui_LabelIPlocating, 0);
    lv_obj_set_align(ui_LabelIPlocating, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_LabelIPlocating, "IP Locating");
    lv_obj_set_style_text_font(ui_LabelIPlocating, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_SwitchIPlocatiing = lv_switch_create(ui_LocationSection1);
    lv_obj_set_width(ui_SwitchIPlocatiing, 50);
    lv_obj_set_height(ui_SwitchIPlocatiing, 25);
    lv_obj_set_align(ui_SwitchIPlocatiing, LV_ALIGN_RIGHT_MID);
    lv_obj_add_state(ui_SwitchIPlocatiing, ui_system_para.auto_location ? LV_STATE_CHECKED : 0);


    lv_obj_t * ui_LocationSection2 = lv_obj_create(ui_PanelLocationMenu);
    lv_obj_set_width(ui_LocationSection2, 300);
    lv_obj_set_height(ui_LocationSection2, 40);
    lv_obj_set_x(ui_LocationSection2, 0);
    lv_obj_set_y(ui_LocationSection2, 80);
    lv_obj_set_align(ui_LocationSection2, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_LocationSection2, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_LocationSection2, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LocationSection2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_LocationSection2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_LocationSection2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LocationSection2, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_LocationSection2, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_PanelADCodeSet = lv_obj_create(ui_LocationSection2);
    lv_obj_set_width(ui_PanelADCodeSet, 300);
    lv_obj_set_height(ui_PanelADCodeSet, 40);
    lv_obj_set_x(ui_PanelADCodeSet, 0);
    lv_obj_set_y(ui_PanelADCodeSet, -15);
    lv_obj_set_align(ui_PanelADCodeSet, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_PanelADCodeSet, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelADCodeSet, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelADCodeSet, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelADCodeSet, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelADCodeSet, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_PanelADCodeSet, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_PanelADCodeSet, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_LabelLocCodeSet = lv_label_create(ui_PanelADCodeSet);
    lv_obj_set_width(ui_LabelLocCodeSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelLocCodeSet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelLocCodeSet, 30);
    lv_obj_set_y(ui_LabelLocCodeSet, 0);
    lv_obj_set_align(ui_LabelLocCodeSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_LabelLocCodeSet, "adcode set");
    lv_obj_set_style_text_font(ui_LabelLocCodeSet, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_IconLocCodeSet = lv_label_create(ui_PanelADCodeSet);
    lv_obj_set_width(ui_IconLocCodeSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconLocCodeSet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_IconLocCodeSet, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_IconLocCodeSet, "");
    lv_obj_set_style_text_font(ui_IconLocCodeSet, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelLocationName = lv_label_create(ui_PanelLocationMenu);
    lv_obj_set_width(ui_LabelLocationName, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelLocationName, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelLocationName, 0);
    lv_obj_set_y(ui_LabelLocationName, -25);
    lv_obj_set_align(ui_LabelLocationName, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelLocationName, ui_system_para.location.city);
    lv_obj_set_style_text_font(ui_LabelLocationName, &ui_font_heiti22, LV_PART_MAIN | LV_STATE_DEFAULT);

    if(ui_system_para.auto_location == true) {
        lv_obj_add_flag(ui_LocationSection2, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_event_cb(ui_SwitchIPlocatiing, IPlocatiing_event_cb, LV_EVENT_ALL, ui_LocationSection2);
    lv_obj_add_event_cb(ui_PanelADCodeSet, sub_menu_click_cb, LV_EVENT_CLICKED, "AdcodeSetMenu");

    // load page
    lv_scr_load_anim(ui_LocationMenu, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

static void ui_LocationMenu_deinit(void)
{
    // deinit
}

static void ui_AboutMenu_init(void)
{
    lv_obj_t * ui_AboutMenu = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_AboutMenu, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t * ui_BtnBackAbout = lv_button_create(ui_AboutMenu);
    lv_obj_set_width(ui_BtnBackAbout, 50);
    lv_obj_set_height(ui_BtnBackAbout, 45);
    lv_obj_set_x(ui_BtnBackAbout, 5);
    lv_obj_set_y(ui_BtnBackAbout, 0);
    lv_obj_add_flag(ui_BtnBackAbout, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_BtnBackAbout, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_BtnBackAbout, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BtnBackAbout, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_BtnBackAbout, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_BtnBackAbout, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_BtnBackAbout, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_BtnBackAbout, 64, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(ui_BtnBackAbout, back_event_handler, LV_EVENT_CLICKED, ui_BtnBackAbout);

    lv_obj_t * ui_LabelBackAbout = lv_label_create(ui_BtnBackAbout);
    lv_obj_set_width(ui_LabelBackAbout, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelBackAbout, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(ui_LabelBackAbout, "");
    lv_obj_set_style_text_font(ui_LabelBackAbout, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelAboutMenu = lv_obj_create(ui_AboutMenu);
    lv_obj_set_width(ui_PanelAboutMenu, 320);
    lv_obj_set_height(ui_PanelAboutMenu, 200);
    lv_obj_set_x(ui_PanelAboutMenu, 0);
    lv_obj_set_y(ui_PanelAboutMenu, 40);
    lv_obj_set_align(ui_PanelAboutMenu, LV_ALIGN_TOP_MID);
    lv_obj_set_scroll_dir(ui_PanelAboutMenu, LV_DIR_VER);
    lv_obj_set_style_bg_color(ui_PanelAboutMenu, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelAboutMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelAboutMenu, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelAboutMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_TimeAboutSection1 = lv_obj_create(ui_PanelAboutMenu);
    lv_obj_set_width(ui_TimeAboutSection1, 300);
    lv_obj_set_height(ui_TimeAboutSection1, 80);
    lv_obj_set_x(ui_TimeAboutSection1, 0);
    lv_obj_set_y(ui_TimeAboutSection1, -10);
    lv_obj_set_align(ui_TimeAboutSection1, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_TimeAboutSection1, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_TimeAboutSection1, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_TimeAboutSection1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_TimeAboutSection1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_TimeAboutSection1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_TimeAboutSection1, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_TimeAboutSection1, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_PanelSoftwareInfo = lv_obj_create(ui_TimeAboutSection1);
    lv_obj_set_width(ui_PanelSoftwareInfo, 300);
    lv_obj_set_height(ui_PanelSoftwareInfo, 40);
    lv_obj_set_x(ui_PanelSoftwareInfo, 0);
    lv_obj_set_y(ui_PanelSoftwareInfo, -15);
    lv_obj_set_align(ui_PanelSoftwareInfo, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_PanelSoftwareInfo, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelSoftwareInfo, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelSoftwareInfo, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelSoftwareInfo, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelSoftwareInfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_PanelSoftwareInfo, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_PanelSoftwareInfo, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_LabelSoftwareInfo = lv_label_create(ui_PanelSoftwareInfo);
    lv_obj_set_width(ui_LabelSoftwareInfo, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelSoftwareInfo, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelSoftwareInfo, 30);
    lv_obj_set_y(ui_LabelSoftwareInfo, 0);
    lv_obj_set_align(ui_LabelSoftwareInfo, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_LabelSoftwareInfo, "Software Info");
    lv_obj_set_style_text_font(ui_LabelSoftwareInfo, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_IconSoftwareInfo = lv_label_create(ui_PanelSoftwareInfo);
    lv_obj_set_width(ui_IconSoftwareInfo, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconSoftwareInfo, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_IconSoftwareInfo, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_IconSoftwareInfo, "");
    lv_obj_set_style_text_font(ui_IconSoftwareInfo, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelLegalInfo = lv_obj_create(ui_TimeAboutSection1);
    lv_obj_set_width(ui_PanelLegalInfo, 300);
    lv_obj_set_height(ui_PanelLegalInfo, 40);
    lv_obj_set_x(ui_PanelLegalInfo, 0);
    lv_obj_set_y(ui_PanelLegalInfo, 25);
    lv_obj_set_align(ui_PanelLegalInfo, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_PanelLegalInfo, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelLegalInfo, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelLegalInfo, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelLegalInfo, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelLegalInfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_PanelLegalInfo, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_PanelLegalInfo, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_LabelLegalInfo = lv_label_create(ui_PanelLegalInfo);
    lv_obj_set_width(ui_LabelLegalInfo, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelLegalInfo, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelLegalInfo, 30);
    lv_obj_set_y(ui_LabelLegalInfo, 0);
    lv_obj_set_align(ui_LabelLegalInfo, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_LabelLegalInfo, "Legal Info");
    lv_obj_set_style_text_font(ui_LabelLegalInfo, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_IconLegalInfo = lv_label_create(ui_PanelLegalInfo);
    lv_obj_set_width(ui_IconLegalInfo, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconLegalInfo, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_IconLegalInfo, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_IconLegalInfo, "");
    lv_obj_set_style_text_font(ui_IconLegalInfo, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_PanelSoftwareInfo, sub_menu_click_cb, LV_EVENT_CLICKED, "SoftwareInfoMenu");
    lv_obj_add_event_cb(ui_PanelLegalInfo, sub_menu_click_cb, LV_EVENT_CLICKED, "LegalInfoMenu");

    // load page
    lv_scr_load_anim(ui_AboutMenu, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);

}

static void ui_AboutMenu_deinit(void)
{
    // deinit
}

static void ui_TimeSetMenu_init(void)
{
    lv_obj_t * ui_TimeSetMenu = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_TimeSetMenu, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t * ui_BtnBackTimeSet = lv_button_create(ui_TimeSetMenu);
    lv_obj_set_width(ui_BtnBackTimeSet, 50);
    lv_obj_set_height(ui_BtnBackTimeSet, 45);
    lv_obj_set_x(ui_BtnBackTimeSet, 5);
    lv_obj_set_y(ui_BtnBackTimeSet, 0);
    lv_obj_add_flag(ui_BtnBackTimeSet, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_BtnBackTimeSet, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_BtnBackTimeSet, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BtnBackTimeSet, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_BtnBackTimeSet, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_BtnBackTimeSet, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_BtnBackTimeSet, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_BtnBackTimeSet, 64, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(ui_BtnBackTimeSet, back_event_handler, LV_EVENT_CLICKED, ui_BtnBackTimeSet);

    lv_obj_t * ui_LabelBackTimeSet = lv_label_create(ui_BtnBackTimeSet);
    lv_obj_set_width(ui_LabelBackTimeSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelBackTimeSet, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(ui_LabelBackTimeSet, "");
    lv_obj_set_style_text_font(ui_LabelBackTimeSet, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelTimeSetMenu = lv_obj_create(ui_TimeSetMenu);
    lv_obj_set_width(ui_PanelTimeSetMenu, 320);
    lv_obj_set_height(ui_PanelTimeSetMenu, 200);
    lv_obj_set_x(ui_PanelTimeSetMenu, 0);
    lv_obj_set_y(ui_PanelTimeSetMenu, 40);
    lv_obj_set_align(ui_PanelTimeSetMenu, LV_ALIGN_TOP_MID);
    lv_obj_set_scroll_dir(ui_PanelTimeSetMenu, LV_DIR_VER);
    lv_obj_set_style_bg_color(ui_PanelTimeSetMenu, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelTimeSetMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelTimeSetMenu, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelTimeSetMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_TimeSetSection1 = lv_obj_create(ui_PanelTimeSetMenu);
    lv_obj_set_width(ui_TimeSetSection1, 300);
    lv_obj_set_height(ui_TimeSetSection1, 130);
    lv_obj_set_x(ui_TimeSetSection1, 0);
    lv_obj_set_y(ui_TimeSetSection1, -10);
    lv_obj_set_align(ui_TimeSetSection1, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_TimeSetSection1, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_TimeSetSection1, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_TimeSetSection1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_TimeSetSection1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_TimeSetSection1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_TimeSetSection1, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_TimeSetSection1, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_RollerHour = lv_roller_create(ui_TimeSetSection1);
    lv_roller_set_options(ui_RollerHour,
                          "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23",
                          LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_RollerHour, 85);
    lv_obj_set_height(ui_RollerHour, 110);
    lv_obj_set_x(ui_RollerHour, -95);
    lv_obj_set_y(ui_RollerHour, 0);
    lv_obj_set_align(ui_RollerHour, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(ui_RollerHour, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_RollerMinute = lv_roller_create(ui_TimeSetSection1);
    lv_roller_set_options(ui_RollerMinute,
                          "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59",
                          LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_RollerMinute, 85);
    lv_obj_set_height(ui_RollerMinute, 110);
    lv_obj_set_align(ui_RollerMinute, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(ui_RollerMinute, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_RollerSecond = lv_roller_create(ui_TimeSetSection1);
    lv_roller_set_options(ui_RollerSecond,
                          "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59",
                          LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_RollerSecond, 85);
    lv_obj_set_height(ui_RollerSecond, 110);
    lv_obj_set_x(ui_RollerSecond, 95);
    lv_obj_set_y(ui_RollerSecond, 0);
    lv_obj_set_align(ui_RollerSecond, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(ui_RollerSecond, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    // set roller value
    lv_roller_set_selected(ui_RollerHour, ui_system_para.hour, LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerMinute, ui_system_para.minute, LV_ANIM_OFF);

    lv_obj_t * ui_BtnTimeSetConfirme = lv_button_create(ui_PanelTimeSetMenu);
    lv_obj_set_width(ui_BtnTimeSetConfirme, 75);
    lv_obj_set_height(ui_BtnTimeSetConfirme, 45);
    lv_obj_set_align(ui_BtnTimeSetConfirme, LV_ALIGN_BOTTOM_MID);
    lv_obj_add_flag(ui_BtnTimeSetConfirme, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_BtnTimeSetConfirme, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t * ui_LabelTimeSetConfirm = lv_label_create(ui_BtnTimeSetConfirme);
    lv_obj_set_width(ui_LabelTimeSetConfirm, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelTimeSetConfirm, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelTimeSetConfirm, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelTimeSetConfirm, "Set");
    lv_obj_set_style_text_font(ui_LabelTimeSetConfirm, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_BtnTimeSetConfirme, time_set_confirm_cb, LV_EVENT_CLICKED, ui_TimeSetSection1);

    // load page
    lv_scr_load_anim(ui_TimeSetMenu, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);

}

static void ui_TimeSetMenu_deinit(void)
{
    // deinit
}

static void ui_DateSetMenu_init(void)
{
    lv_obj_t * ui_DateSetMenu = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_DateSetMenu, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t * ui_BtnBackDateSet = lv_button_create(ui_DateSetMenu);
    lv_obj_set_width(ui_BtnBackDateSet, 50);
    lv_obj_set_height(ui_BtnBackDateSet, 45);
    lv_obj_set_x(ui_BtnBackDateSet, 5);
    lv_obj_set_y(ui_BtnBackDateSet, 0);
    lv_obj_add_flag(ui_BtnBackDateSet, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_BtnBackDateSet, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_BtnBackDateSet, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BtnBackDateSet, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_BtnBackDateSet, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_BtnBackDateSet, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_BtnBackDateSet, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_BtnBackDateSet, 64, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(ui_BtnBackDateSet, back_event_handler, LV_EVENT_CLICKED, ui_BtnBackDateSet);

    lv_obj_t * ui_LabelBackDateSet = lv_label_create(ui_BtnBackDateSet);
    lv_obj_set_width(ui_LabelBackDateSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelBackDateSet, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(ui_LabelBackDateSet, "");
    lv_obj_set_style_text_font(ui_LabelBackDateSet, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelDateMenu = lv_obj_create(ui_DateSetMenu);
    lv_obj_set_width(ui_PanelDateMenu, 320);
    lv_obj_set_height(ui_PanelDateMenu, 200);
    lv_obj_set_x(ui_PanelDateMenu, 0);
    lv_obj_set_y(ui_PanelDateMenu, 40);
    lv_obj_set_align(ui_PanelDateMenu, LV_ALIGN_TOP_MID);
    lv_obj_set_scroll_dir(ui_PanelDateMenu, LV_DIR_VER);
    lv_obj_set_style_bg_color(ui_PanelDateMenu, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelDateMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelDateMenu, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelDateMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_DateSetSection1 = lv_obj_create(ui_PanelDateMenu);
    lv_obj_set_width(ui_DateSetSection1, 300);
    lv_obj_set_height(ui_DateSetSection1, 130);
    lv_obj_set_x(ui_DateSetSection1, 0);
    lv_obj_set_y(ui_DateSetSection1, -10);
    lv_obj_set_align(ui_DateSetSection1, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_DateSetSection1, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_DateSetSection1, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_DateSetSection1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_DateSetSection1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_DateSetSection1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_DateSetSection1, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_DateSetSection1, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_RollerYear = lv_roller_create(ui_DateSetSection1);
    lv_roller_set_options(ui_RollerYear,
                          "2025\n2026\n2027\n2028\n2029\n2030\n2031\n2032\n2033\n2034\n2035\n2036\n2037\n2038\n2039\n2040",
                          LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_RollerYear, 85);
    lv_obj_set_height(ui_RollerYear, 110);
    lv_obj_set_x(ui_RollerYear, -95);
    lv_obj_set_y(ui_RollerYear, 0);
    lv_obj_set_align(ui_RollerYear, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(ui_RollerYear, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_RollerMonth = lv_roller_create(ui_DateSetSection1);
    lv_roller_set_options(ui_RollerMonth, "01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_RollerMonth, 85);
    lv_obj_set_height(ui_RollerMonth, 110);
    lv_obj_set_align(ui_RollerMonth, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(ui_RollerMonth, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_RollerDay = lv_roller_create(ui_DateSetSection1);
    lv_roller_set_options(ui_RollerDay,
                          "01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31",
                          LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_RollerDay, 85);
    lv_obj_set_height(ui_RollerDay, 110);
    lv_obj_set_x(ui_RollerDay, 95);
    lv_obj_set_y(ui_RollerDay, 0);
    lv_obj_set_align(ui_RollerDay, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font(ui_RollerDay, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    // set roller value
    lv_roller_set_selected(ui_RollerYear, ui_system_para.year - 2025, LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerMonth, ui_system_para.month - 1, LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerDay, ui_system_para.day - 1, LV_ANIM_OFF);

    lv_obj_t * ui_BtnDateSetConfirme = lv_button_create(ui_PanelDateMenu);
    lv_obj_set_width(ui_BtnDateSetConfirme, 75);
    lv_obj_set_height(ui_BtnDateSetConfirme, 45);
    lv_obj_set_align(ui_BtnDateSetConfirme, LV_ALIGN_BOTTOM_MID);
    lv_obj_add_flag(ui_BtnDateSetConfirme, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_BtnDateSetConfirme, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t * ui_LabelDateSetConfirm = lv_label_create(ui_BtnDateSetConfirme);
    lv_obj_set_width(ui_LabelDateSetConfirm, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelDateSetConfirm, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelDateSetConfirm, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelDateSetConfirm, "Set");
    lv_obj_set_style_text_font(ui_LabelDateSetConfirm, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_BtnDateSetConfirme, date_set_confirm_cb, LV_EVENT_CLICKED, ui_DateSetSection1);

    // load page
    lv_scr_load_anim(ui_DateSetMenu, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);

}

static void ui_DateSetMenu_deinit(void)
{
    // deinit
}

static void ui_AdcodeSetMenu_init(void)
{
    lv_obj_t * ui_AdcodeSetMenu = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_AdcodeSetMenu, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t * ui_BtnBackAdcodeSet = lv_button_create(ui_AdcodeSetMenu);
    lv_obj_set_width(ui_BtnBackAdcodeSet, 50);
    lv_obj_set_height(ui_BtnBackAdcodeSet, 45);
    lv_obj_set_x(ui_BtnBackAdcodeSet, 5);
    lv_obj_set_y(ui_BtnBackAdcodeSet, 0);
    lv_obj_add_flag(ui_BtnBackAdcodeSet, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_BtnBackAdcodeSet, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_BtnBackAdcodeSet, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BtnBackAdcodeSet, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_BtnBackAdcodeSet, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_BtnBackAdcodeSet, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_BtnBackAdcodeSet, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_BtnBackAdcodeSet, 64, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(ui_BtnBackAdcodeSet, back_event_handler, LV_EVENT_CLICKED, ui_BtnBackAdcodeSet);

    lv_obj_t * ui_LabelAdcodeSet = lv_label_create(ui_BtnBackAdcodeSet);
    lv_obj_set_width(ui_LabelAdcodeSet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAdcodeSet, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(ui_LabelAdcodeSet, "");
    lv_obj_set_style_text_font(ui_LabelAdcodeSet, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelAdcodeSetMenu = lv_obj_create(ui_AdcodeSetMenu);
    lv_obj_set_width(ui_PanelAdcodeSetMenu, 320);
    lv_obj_set_height(ui_PanelAdcodeSetMenu, 200);
    lv_obj_set_x(ui_PanelAdcodeSetMenu, 0);
    lv_obj_set_y(ui_PanelAdcodeSetMenu, 40);
    lv_obj_set_align(ui_PanelAdcodeSetMenu, LV_ALIGN_TOP_MID);
    lv_obj_set_scroll_dir(ui_PanelAdcodeSetMenu, LV_DIR_VER);
    lv_obj_set_style_bg_color(ui_PanelAdcodeSetMenu, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelAdcodeSetMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelAdcodeSetMenu, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelAdcodeSetMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_AdcodeSetSection1 = lv_obj_create(ui_PanelAdcodeSetMenu);
    lv_obj_set_width(ui_AdcodeSetSection1, 305);
    lv_obj_set_height(ui_AdcodeSetSection1, 105);
    lv_obj_set_x(ui_AdcodeSetSection1, 0);
    lv_obj_set_y(ui_AdcodeSetSection1, -10);
    lv_obj_set_align(ui_AdcodeSetSection1, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_AdcodeSetSection1, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_AdcodeSetSection1, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_AdcodeSetSection1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_AdcodeSetSection1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_AdcodeSetSection1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_AdcodeSetSection1, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_AdcodeSetSection1, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_RollerAdcode1 = lv_roller_create(ui_AdcodeSetSection1);
    lv_roller_set_options(ui_RollerAdcode1, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_RollerAdcode1, 40);
    lv_obj_set_height(ui_RollerAdcode1, 90);
    lv_obj_set_x(ui_RollerAdcode1, -7);
    lv_obj_set_y(ui_RollerAdcode1, 0);
    lv_obj_set_align(ui_RollerAdcode1, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_RollerAdcode1, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_RollerAdcode2 = lv_roller_create(ui_AdcodeSetSection1);
    lv_roller_set_options(ui_RollerAdcode2, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_RollerAdcode2, 40);
    lv_obj_set_height(ui_RollerAdcode2, 90);
    lv_obj_set_x(ui_RollerAdcode2, 42);
    lv_obj_set_y(ui_RollerAdcode2, 0);
    lv_obj_set_align(ui_RollerAdcode2, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_RollerAdcode2, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_RollerAdcode3 = lv_roller_create(ui_AdcodeSetSection1);
    lv_roller_set_options(ui_RollerAdcode3, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_RollerAdcode3, 40);
    lv_obj_set_height(ui_RollerAdcode3, 90);
    lv_obj_set_x(ui_RollerAdcode3, 92);
    lv_obj_set_y(ui_RollerAdcode3, 0);
    lv_obj_set_align(ui_RollerAdcode3, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_RollerAdcode3, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_RollerAdcode4 = lv_roller_create(ui_AdcodeSetSection1);
    lv_roller_set_options(ui_RollerAdcode4, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_RollerAdcode4, 40);
    lv_obj_set_height(ui_RollerAdcode4, 90);
    lv_obj_set_x(ui_RollerAdcode4, 142);
    lv_obj_set_y(ui_RollerAdcode4, 0);
    lv_obj_set_align(ui_RollerAdcode4, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_RollerAdcode4, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_RollerAdcode5 = lv_roller_create(ui_AdcodeSetSection1);
    lv_roller_set_options(ui_RollerAdcode5, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_RollerAdcode5, 40);
    lv_obj_set_height(ui_RollerAdcode5, 90);
    lv_obj_set_x(ui_RollerAdcode5, 192);
    lv_obj_set_y(ui_RollerAdcode5, 0);
    lv_obj_set_align(ui_RollerAdcode5, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_RollerAdcode5, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_RollerAdcode6 = lv_roller_create(ui_AdcodeSetSection1);
    lv_roller_set_options(ui_RollerAdcode6, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_RollerAdcode6, 40);
    lv_obj_set_height(ui_RollerAdcode6, 90);
    lv_obj_set_x(ui_RollerAdcode6, 242);
    lv_obj_set_y(ui_RollerAdcode6, 0);
    lv_obj_set_align(ui_RollerAdcode6, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_RollerAdcode6, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    // set roller value
    lv_roller_set_selected(ui_RollerAdcode1, ui_system_para.location.adcode[0] - '0', LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerAdcode2, ui_system_para.location.adcode[1] - '0', LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerAdcode3, ui_system_para.location.adcode[2] - '0', LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerAdcode4, ui_system_para.location.adcode[3] - '0', LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerAdcode5, ui_system_para.location.adcode[4] - '0', LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerAdcode6, ui_system_para.location.adcode[5] - '0', LV_ANIM_OFF);

    lv_obj_t * ui_BtnAdcodeSetConfirme = lv_button_create(ui_PanelAdcodeSetMenu);
    lv_obj_set_width(ui_BtnAdcodeSetConfirme, 75);
    lv_obj_set_height(ui_BtnAdcodeSetConfirme, 45);
    lv_obj_set_align(ui_BtnAdcodeSetConfirme, LV_ALIGN_BOTTOM_MID);
    lv_obj_add_flag(ui_BtnAdcodeSetConfirme, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_BtnAdcodeSetConfirme, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t * ui_LabelAdcodeSetConfirm = lv_label_create(ui_BtnAdcodeSetConfirme);
    lv_obj_set_width(ui_LabelAdcodeSetConfirm, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAdcodeSetConfirm, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelAdcodeSetConfirm, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelAdcodeSetConfirm, "Set");
    lv_obj_set_style_text_font(ui_LabelAdcodeSetConfirm, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelLocationName2 = lv_label_create(ui_PanelAdcodeSetMenu);
    lv_obj_set_width(ui_LabelLocationName2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelLocationName2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelLocationName2, 0);
    lv_obj_set_y(ui_LabelLocationName2, 25);
    lv_obj_set_align(ui_LabelLocationName2, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelLocationName2, ui_system_para.location.city);
    lv_obj_set_style_text_font(ui_LabelLocationName2, &ui_font_heiti22, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_BtnAdcodeSetConfirme, adcode_set_confirm_cb, LV_EVENT_CLICKED, ui_AdcodeSetSection1);

    // load page
    lv_scr_load_anim(ui_AdcodeSetMenu, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);

}

static void ui_AdcodeSetMenu_deinit(void)
{
    // deinit
}

static void ui_SoftWareInfoMenu_init(void)
{
    lv_obj_t * ui_SoftWareInfoMenu = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_SoftWareInfoMenu, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t * ui_BtnBackSoftWareInfo = lv_button_create(ui_SoftWareInfoMenu);
    lv_obj_set_width(ui_BtnBackSoftWareInfo, 50);
    lv_obj_set_height(ui_BtnBackSoftWareInfo, 45);
    lv_obj_set_x(ui_BtnBackSoftWareInfo, 5);
    lv_obj_set_y(ui_BtnBackSoftWareInfo, 0);
    lv_obj_add_flag(ui_BtnBackSoftWareInfo, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_BtnBackSoftWareInfo, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_BtnBackSoftWareInfo, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BtnBackSoftWareInfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_BtnBackSoftWareInfo, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_BtnBackSoftWareInfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_BtnBackSoftWareInfo, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_BtnBackSoftWareInfo, 64, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(ui_BtnBackSoftWareInfo, back_event_handler, LV_EVENT_CLICKED, ui_BtnBackSoftWareInfo);

    lv_obj_t * ui_LabelBackSoftWareInfo = lv_label_create(ui_BtnBackSoftWareInfo);
    lv_obj_set_width(ui_LabelBackSoftWareInfo, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelBackSoftWareInfo, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(ui_LabelBackSoftWareInfo, "");
    lv_obj_set_style_text_font(ui_LabelBackSoftWareInfo, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelSoftwareInfoMenu = lv_obj_create(ui_SoftWareInfoMenu);
    lv_obj_set_width(ui_PanelSoftwareInfoMenu, 320);
    lv_obj_set_height(ui_PanelSoftwareInfoMenu, 200);
    lv_obj_set_x(ui_PanelSoftwareInfoMenu, 0);
    lv_obj_set_y(ui_PanelSoftwareInfoMenu, 40);
    lv_obj_set_align(ui_PanelSoftwareInfoMenu, LV_ALIGN_TOP_MID);
    lv_obj_set_scroll_dir(ui_PanelSoftwareInfoMenu, LV_DIR_VER);
    lv_obj_set_style_bg_color(ui_PanelSoftwareInfoMenu, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelSoftwareInfoMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelSoftwareInfoMenu, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelSoftwareInfoMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_LabelSoftwareInfos = lv_label_create(ui_PanelSoftwareInfoMenu);
    lv_obj_set_width(ui_LabelSoftwareInfos, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelSoftwareInfos, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(ui_LabelSoftwareInfos, "Power by Tuya Open AI\nSoftware version is V1.0.1\n\n\n\n\n");
    lv_obj_set_style_text_font(ui_LabelSoftwareInfos, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    //load page
    lv_scr_load_anim(ui_SoftWareInfoMenu, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);

}

static void ui_SoftWareInfoMenu_deinit(void)
{
    // deinit
}

static void ui_LegalInfoMenu_init(void)
{
    lv_obj_t * ui_LegalInfoMenu = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_LegalInfoMenu, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t * ui_BtnBackLegalInfo = lv_button_create(ui_LegalInfoMenu);
    lv_obj_set_width(ui_BtnBackLegalInfo, 50);
    lv_obj_set_height(ui_BtnBackLegalInfo, 45);
    lv_obj_set_x(ui_BtnBackLegalInfo, 5);
    lv_obj_set_y(ui_BtnBackLegalInfo, 0);
    lv_obj_add_flag(ui_BtnBackLegalInfo, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_BtnBackLegalInfo, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_BtnBackLegalInfo, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BtnBackLegalInfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_BtnBackLegalInfo, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_BtnBackLegalInfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_BtnBackLegalInfo, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_BtnBackLegalInfo, 64, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(ui_BtnBackLegalInfo, back_event_handler, LV_EVENT_CLICKED, ui_BtnBackLegalInfo);

    lv_obj_t * ui_LabelBackLegalInfo = lv_label_create(ui_BtnBackLegalInfo);
    lv_obj_set_width(ui_LabelBackLegalInfo, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelBackLegalInfo, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(ui_LabelBackLegalInfo, "");
    lv_obj_set_style_text_font(ui_LabelBackLegalInfo, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelLegalInfoMenu = lv_obj_create(ui_LegalInfoMenu);
    lv_obj_set_width(ui_PanelLegalInfoMenu, 320);
    lv_obj_set_height(ui_PanelLegalInfoMenu, 200);
    lv_obj_set_x(ui_PanelLegalInfoMenu, 0);
    lv_obj_set_y(ui_PanelLegalInfoMenu, 40);
    lv_obj_set_align(ui_PanelLegalInfoMenu, LV_ALIGN_TOP_MID);
    lv_obj_set_scroll_dir(ui_PanelLegalInfoMenu, LV_DIR_VER);
    lv_obj_set_style_bg_color(ui_PanelLegalInfoMenu, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelLegalInfoMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelLegalInfoMenu, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelLegalInfoMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_LabelLegalInfos = lv_label_create(ui_PanelLegalInfoMenu);
    lv_obj_set_width(ui_LabelLegalInfos, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelLegalInfos, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(ui_LabelLegalInfos, "legal info: ...");
    lv_obj_set_style_text_font(ui_LabelLegalInfos, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    //load page
    lv_scr_load_anim(ui_LegalInfoMenu, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

static void ui_LegalInfoMenu_deinit(void)
{
    // deinit
}

///////////// sub screens page manager //////////////

#define _SUB_MENU_NUMS 9

ui_app_data_t ui_sub_menu_apps[_SUB_MENU_NUMS] = 
{
    {
        .name = "CommonSetMenu",
        .init = ui_CommonMenu_init,
        .deinit = ui_CommonMenu_deinit,
        .page_obj = NULL
    },
    {
        .name = "TimeDateMenu",
        .init = ui_TimeDateMenu_init,
        .deinit = ui_TimeDateMenu_deinit,
        .page_obj = NULL
    },
    {
        .name = "LocationMenu",
        .init = ui_LocationMenu_init,
        .deinit = ui_LocationMenu_deinit,
        .page_obj = NULL
    },
    {
        .name = "AboutMenu",
        .init = ui_AboutMenu_init,
        .deinit = ui_AboutMenu_deinit,
        .page_obj = NULL
    },
    {
        .name = "TimeSetMenu",
        .init = ui_TimeSetMenu_init,
        .deinit = ui_TimeSetMenu_deinit,
        .page_obj = NULL
    },
    {
        .name = "DateSetMenu",
        .init = ui_DateSetMenu_init,
        .deinit = ui_DateSetMenu_deinit,
        .page_obj = NULL
    },
    {
        .name = "AdcodeSetMenu",
        .init = ui_AdcodeSetMenu_init,
        .deinit = ui_AdcodeSetMenu_deinit,
        .page_obj = NULL
    },
    {
        .name = "SoftwareInfoMenu",
        .init = ui_SoftWareInfoMenu_init,
        .deinit = ui_SoftWareInfoMenu_deinit,
        .page_obj = NULL
    },
    {
        .name = "LegalInfoMenu",
        .init = ui_LegalInfoMenu_init,
        .deinit = ui_LegalInfoMenu_deinit,
        .page_obj = NULL
    }
};


static void _ui_sub_menus_creat(void)
{
    for(int i = 0; i < _SUB_MENU_NUMS; i++)
    {
        lv_lib_pm_CreatePage(&page_manager, ui_sub_menu_apps[i].name, ui_sub_menu_apps[i].init, ui_sub_menu_apps[i].deinit, NULL);
    }    
}

///////////////////// SCREEN init ////////////////////

void ui_SettingPage_init()
{
    static bool inited = false;
    if(inited == false)
    {
        _ui_sub_menus_creat();
        PR_DEBUG("SettingPage sub menus created.");
        inited = true;
    }
    ui_SettingRootMenu = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_SettingRootMenu, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t * ui_BtnBack = lv_button_create(ui_SettingRootMenu);
    lv_obj_set_width(ui_BtnBack, 50);
    lv_obj_set_height(ui_BtnBack, 45);
    lv_obj_set_x(ui_BtnBack, 5);
    lv_obj_set_y(ui_BtnBack, 0);
    lv_obj_add_flag(ui_BtnBack, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_remove_flag(ui_BtnBack, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_BtnBack, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BtnBack, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_BtnBack, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_BtnBack, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_BtnBack, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_BtnBack, 64, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_event_cb(ui_BtnBack, back_event_handler, LV_EVENT_CLICKED, ui_SettingRootMenu);

    lv_obj_t * ui_LabelBack = lv_label_create(ui_BtnBack);
    lv_obj_set_width(ui_LabelBack, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelBack, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(ui_LabelBack, "");
    lv_obj_set_style_text_font(ui_LabelBack, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelMainMenu = lv_obj_create(ui_SettingRootMenu);
    lv_obj_set_width(ui_PanelMainMenu, 320);
    lv_obj_set_height(ui_PanelMainMenu, 200);
    lv_obj_set_x(ui_PanelMainMenu, 0);
    lv_obj_set_y(ui_PanelMainMenu, 40);
    lv_obj_set_align(ui_PanelMainMenu, LV_ALIGN_TOP_MID);
    lv_obj_set_scroll_dir(ui_PanelMainMenu, LV_DIR_VER);
    lv_obj_set_style_bg_color(ui_PanelMainMenu, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelMainMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelMainMenu, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelMainMenu, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelMenuSection1 = lv_obj_create(ui_PanelMainMenu);
    lv_obj_set_width(ui_PanelMenuSection1, 300);
    lv_obj_set_height(ui_PanelMenuSection1, 120);
    lv_obj_set_x(ui_PanelMenuSection1, 0);
    lv_obj_set_y(ui_PanelMenuSection1, -10);
    lv_obj_set_align(ui_PanelMenuSection1, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_PanelMenuSection1, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelMenuSection1, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelMenuSection1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelMenuSection1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelMenuSection1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_PanelMenuSection1, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_PanelMenuSection1, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_PanelCommon = lv_obj_create(ui_PanelMenuSection1);
    lv_obj_set_width(ui_PanelCommon, 300);
    lv_obj_set_height(ui_PanelCommon, 40);
    lv_obj_set_x(ui_PanelCommon, 0);
    lv_obj_set_y(ui_PanelCommon, -15);
    lv_obj_set_align(ui_PanelCommon, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_PanelCommon, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelCommon, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelCommon, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelCommon, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelCommon, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_PanelCommon, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_PanelCommon, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(ui_PanelCommon, sub_menu_click_cb, LV_EVENT_CLICKED, "CommonSetMenu");

    lv_obj_t * ui_LabelCommon = lv_label_create(ui_PanelCommon);
    lv_obj_set_width(ui_LabelCommon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelCommon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelCommon, 30);
    lv_obj_set_y(ui_LabelCommon, 0);
    lv_obj_set_align(ui_LabelCommon, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_LabelCommon, "Common");
    lv_obj_set_style_text_font(ui_LabelCommon, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_IconCommon = lv_label_create(ui_PanelCommon);
    lv_obj_set_width(ui_IconCommon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconCommon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_IconCommon, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_IconCommon, "");
    lv_obj_set_style_text_font(ui_IconCommon, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelTime = lv_obj_create(ui_PanelMenuSection1);
    lv_obj_set_width(ui_PanelTime, 300);
    lv_obj_set_height(ui_PanelTime, 40);
    lv_obj_set_x(ui_PanelTime, 0);
    lv_obj_set_y(ui_PanelTime, 25);
    lv_obj_set_align(ui_PanelTime, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_PanelTime, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelTime, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelTime, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelTime, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelTime, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_PanelTime, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_PanelTime, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(ui_PanelTime, sub_menu_click_cb, LV_EVENT_CLICKED, "TimeDateMenu");

    lv_obj_t * ui_LabelTime = lv_label_create(ui_PanelTime);
    lv_obj_set_width(ui_LabelTime, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelTime, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelTime, 30);
    lv_obj_set_y(ui_LabelTime, 0);
    lv_obj_set_align(ui_LabelTime, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_LabelTime, "Time");
    lv_obj_set_style_text_font(ui_LabelTime, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_IconTime = lv_label_create(ui_PanelTime);
    lv_obj_set_width(ui_IconTime, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconTime, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_IconTime, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_IconTime, "");
    lv_obj_set_style_text_font(ui_IconTime, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelLocation = lv_obj_create(ui_PanelMenuSection1);
    lv_obj_set_width(ui_PanelLocation, 300);
    lv_obj_set_height(ui_PanelLocation, 40);
    lv_obj_set_x(ui_PanelLocation, 0);
    lv_obj_set_y(ui_PanelLocation, 65);
    lv_obj_set_align(ui_PanelLocation, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_PanelLocation, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelLocation, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelLocation, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelLocation, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelLocation, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_PanelLocation, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_PanelLocation, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(ui_PanelLocation, sub_menu_click_cb, LV_EVENT_CLICKED, "LocationMenu");

    lv_obj_t * ui_LabelLocation = lv_label_create(ui_PanelLocation);
    lv_obj_set_width(ui_LabelLocation, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelLocation, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelLocation, 30);
    lv_obj_set_y(ui_LabelLocation, 0);
    lv_obj_set_align(ui_LabelLocation, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_LabelLocation, "Location");
    lv_obj_set_style_text_font(ui_LabelLocation, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_IconLocation = lv_label_create(ui_PanelLocation);
    lv_obj_set_width(ui_IconLocation, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconLocation, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_IconLocation, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_IconLocation, "");
    lv_obj_set_style_text_font(ui_IconLocation, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_LabelSection2 = lv_label_create(ui_PanelMainMenu);
    lv_obj_set_width(ui_LabelSection2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelSection2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelSection2, -105);
    lv_obj_set_y(ui_LabelSection2, 45);
    lv_obj_set_align(ui_LabelSection2, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelSection2, "Others");
    lv_obj_set_style_text_font(ui_LabelSection2, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelMenuSection2 = lv_obj_create(ui_PanelMainMenu);
    lv_obj_set_width(ui_PanelMenuSection2, 300);
    lv_obj_set_height(ui_PanelMenuSection2, 80);
    lv_obj_set_x(ui_PanelMenuSection2, 0);
    lv_obj_set_y(ui_PanelMenuSection2, 150);
    lv_obj_set_align(ui_PanelMenuSection2, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_PanelMenuSection2, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelMenuSection2, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelMenuSection2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelMenuSection2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelMenuSection2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_PanelMenuSection2, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_PanelMenuSection2, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_PanelAbout = lv_obj_create(ui_PanelMenuSection2);
    lv_obj_set_width(ui_PanelAbout, 300);
    lv_obj_set_height(ui_PanelAbout, 40);
    lv_obj_set_x(ui_PanelAbout, 0);
    lv_obj_set_y(ui_PanelAbout, -15);
    lv_obj_set_align(ui_PanelAbout, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_PanelAbout, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelAbout, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelAbout, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelAbout, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelAbout, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_PanelAbout, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_PanelAbout, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_add_event_cb(ui_PanelAbout, sub_menu_click_cb, LV_EVENT_CLICKED, "AboutMenu");

    lv_obj_t * ui_LabelAbout = lv_label_create(ui_PanelAbout);
    lv_obj_set_width(ui_LabelAbout, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelAbout, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelAbout, 30);
    lv_obj_set_y(ui_LabelAbout, 0);
    lv_obj_set_align(ui_LabelAbout, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_LabelAbout, "About");
    lv_obj_set_style_text_font(ui_LabelAbout, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_IconAbout = lv_label_create(ui_PanelAbout);
    lv_obj_set_width(ui_IconAbout, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconAbout, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_IconAbout, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_IconAbout, "");
    lv_obj_set_style_text_font(ui_IconAbout, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_PanelMenuMode = lv_obj_create(ui_PanelMenuSection2);
    lv_obj_set_width(ui_PanelMenuMode, 300);
    lv_obj_set_height(ui_PanelMenuMode, 40);
    lv_obj_set_x(ui_PanelMenuMode, 0);
    lv_obj_set_y(ui_PanelMenuMode, 25);
    lv_obj_set_align(ui_PanelMenuMode, LV_ALIGN_TOP_MID);
    lv_obj_remove_flag(ui_PanelMenuMode, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelMenuMode, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelMenuMode, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelMenuMode, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelMenuMode, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_PanelMenuMode, lv_color_hex(0x606060), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_PanelMenuMode, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_LabelMenuMode = lv_label_create(ui_PanelMenuMode);
    lv_obj_set_width(ui_LabelMenuMode, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelMenuMode, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelMenuMode, 30);
    lv_obj_set_y(ui_LabelMenuMode, 0);
    lv_obj_set_align(ui_LabelMenuMode, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_LabelMenuMode, "Menu Mode");
    lv_obj_set_style_text_font(ui_LabelMenuMode, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_IconMenuMode = lv_label_create(ui_PanelMenuMode);
    lv_obj_set_width(ui_IconMenuMode, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_IconMenuMode, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_IconMenuMode, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_IconMenuMode, "");
    lv_obj_set_style_text_font(ui_IconMenuMode, &ui_font_iconfont20, LV_PART_MAIN | LV_STATE_DEFAULT);

    // load page
    lv_scr_load_anim(ui_SettingRootMenu, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

/////////////////// SCREEN deinit ////////////////////

void ui_SettingPage_deinit()
{
    // deinit
}