#include "ui_CameraPage.h"
#include "tal_log.h"
#include "lvgl.h"

///////////////////// VARIABLES ////////////////////

#define DISP_WIDTH  320
#define DISP_HEIGHT 240
#define PIXEL_SIZE  2 // 对于RGB565格式来说是2字节每像素

LV_FONT_DECLARE(font_puhui_18_2);

// static bool _first_into = true;

// static lv_timer_t * timer;
///////////////////// ANIMATIONS ////////////////////

///////////////////// VARIABLES ////////////////////
lv_obj_t * ui_CameraPage;

///////////////////// FUNCTIONS ////////////////////

static void ui_enent_Gesture(lv_event_t * e)
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

///////////////////// SCREEN init ////////////////////

void ui_CameraPage_init(void)
{
    ui_CameraPage = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_CameraPage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_CameraPage, lv_color_hex(0xFAF8EF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_CameraPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // 创建一个标签控件
    lv_obj_t *label = lv_label_create(ui_CameraPage);
    // 设置标签文字
    lv_label_set_text(label, "长按KEY键5s切换摄像头和UI\n目前此功能还在开发中,暂不支持\n请耐心等待!");
    
    // 设置标签在父容器中居中
    lv_obj_center(label);
    
    // 直接设置样式（字体大小、颜色等）
    lv_obj_set_style_text_font(label, &font_puhui_18_2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // 添加手势事件
    lv_obj_add_event_cb(ui_CameraPage, ui_enent_Gesture, LV_EVENT_GESTURE, NULL);
    
    // 加载页面到屏幕（这是关键！）
    lv_scr_load_anim(ui_CameraPage, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, true);
}

/////////////////// SCREEN deinit ////////////////////

void ui_CameraPage_deinit(void)
{

}