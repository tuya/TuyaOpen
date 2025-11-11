#include "ui_CalendarPage.h"

///////////////////// VARIABLES ////////////////////

static const char *day_names[] =
{
    "日", "一", "二", "三", "四", "五", "六"
};

///////////////////// ANIMATIONS ////////////////////


///////////////////// FUNCTIONS ////////////////////

static void ui_enent_Gesture(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    (void)e; // 避免未使用参数警告
    if(code == LV_EVENT_GESTURE)
    {
        if(lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT || lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT)
        {
            lv_lib_pm_OpenPrePage(&page_manager);
        }
    }
}

///////////////////// SCREEN init ////////////////////

void ui_CalendarPage_init()
{
    lv_obj_t * ui_CalendarPage = lv_obj_create(NULL);
    // lv_obj_remove_flag(ui_CalendarPage, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_t * calendar = lv_calendar_create(ui_CalendarPage);
    lv_obj_set_width(calendar, 480);
    lv_obj_set_height(calendar, 320);
    lv_obj_align(calendar, LV_ALIGN_TOP_MID, 0, 0);
    int year; int month; int day; int hour; int minute; int second;
    sys_get_time(&year, &month, &day, &hour, &minute, &second);
    lv_calendar_set_today_date(calendar, year, month, day);
    lv_calendar_set_showed_date(calendar, year, month);

    lv_calendar_set_day_names(calendar, day_names);

    // lv_calendar_set_chinese_mode(calendar, true); // 此函数在标准LVGL中不存在
    lv_obj_set_style_text_font(calendar, &ui_font_heiti14, LV_PART_MAIN);

    #if LV_USE_CALENDAR_HEADER_DROPDOWN
    lv_obj_t * calendar_head = lv_calendar_header_dropdown_create(calendar);
    #elif LV_USE_CALENDAR_HEADER_ARROW
        lv_calendar_header_arrow_create(calendar);
    #endif

    lv_obj_set_style_text_font(calendar_head, &lv_font_montserrat_14,  LV_PART_MAIN | LV_STATE_DEFAULT);

    // event
    lv_obj_add_event_cb(ui_CalendarPage, ui_enent_Gesture, LV_EVENT_ALL, NULL);

    // load page
    lv_scr_load_anim(ui_CalendarPage, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

/////////////////// SCREEN deinit ////////////////////

void ui_CalendarPage_deinit()
{
    // deinit
}
