/**
 * @file ui_pomodoro_clock.c
 * @brief 番茄钟和实时时钟UI模块
 * @version 0.1
 * @date 2025-01-01
 */

#include "ui_display.h"
#include "font_awesome_symbols.h"
#include "lvgl.h"
#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define POMODORO_WORK_TIME    25  // 番茄钟默认工作时间（分钟）
#define POMODORO_SHORT_BREAK  5   // 短休息时间（分钟）
#define POMODORO_LONG_BREAK   15  // 长休息时间（分钟）
#define POMODORO_LONG_BREAK_INTERVAL 4  // 每4个番茄钟后长休息

// 当前选择的工作时间（分钟）
static uint32_t sg_current_work_time = POMODORO_WORK_TIME;

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    POMODORO_STATE_IDLE = 0,
    POMODORO_STATE_WORK,
    POMODORO_STATE_SHORT_BREAK,
    POMODORO_STATE_LONG_BREAK,
} POMODORO_STATE_E;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *left_panel;      // 左侧番茄钟面板
    lv_obj_t *right_panel;     // 右侧时钟面板
    
    // 番茄钟相关UI元素
    lv_obj_t *pomodoro_timer_label;
    lv_obj_t *pomodoro_status_label;
    lv_obj_t *pomodoro_control_btn;
    lv_obj_t *pomodoro_reset_btn;     // 独立复位按钮
    lv_obj_t *pomodoro_progress_bar;
    lv_obj_t *pomodoro_cycles_label;
    lv_obj_t *pomodoro_time_dropdown; // 时间选择下拉框
    
    // 时钟相关UI元素
    lv_obj_t *clock_time_label;
    lv_obj_t *clock_date_label;
    lv_obj_t *clock_weekday_label;
    
    // 番茄钟状态
    POMODORO_STATE_E state;
    uint32_t remaining_seconds;
    uint32_t total_seconds;
    uint8_t completed_cycles;
    lv_timer_t *pomodoro_timer;
    
    // 时钟状态
    lv_timer_t *clock_timer;
    
    UI_FONT_T font;
    UI_THEME_T theme;
} POMODORO_CLOCK_UI_T;

/***********************************************************
********************function declaration********************
***********************************************************/
static void __pomodoro_timer_cb(lv_timer_t *timer);
static void __clock_timer_cb(lv_timer_t *timer);
static void __pomodoro_control_btn_cb(lv_event_t *e);
static void __pomodoro_reset_btn_cb(lv_event_t *e);
static void __pomodoro_time_dropdown_cb(lv_event_t *e);
static void __update_pomodoro_display(void);
static void __update_clock_display(void);
static void __pomodoro_start(void);
static void __pomodoro_pause(void);
static void __pomodoro_reset(void);
static void __pomodoro_next_phase(void);
static const char* __get_pomodoro_status_text(void);
static uint32_t __get_pomodoro_duration(void);

/***********************************************************
***********************variable define**********************
***********************************************************/
static POMODORO_CLOCK_UI_T sg_ui = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

static void __pomodoro_timer_cb(lv_timer_t *timer)
{
    if (sg_ui.remaining_seconds > 0) {
        sg_ui.remaining_seconds--;
        __update_pomodoro_display();
    } else {
        __pomodoro_next_phase();
    }
}

static void __clock_timer_cb(lv_timer_t *timer)
{
    __update_clock_display();
}

static void __pomodoro_reset_btn_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {
        // 点击复位按钮
        __pomodoro_reset();
    }
}

static void __pomodoro_time_dropdown_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *dropdown = lv_event_get_target(e);
        uint16_t selected = lv_dropdown_get_selected(dropdown);
        
        // 更新选择的工作时间
        switch (selected) {
            case 0: sg_current_work_time = 5; break;
            case 1: sg_current_work_time = 10; break;
            case 2: sg_current_work_time = 20; break;
            case 3: sg_current_work_time = 25; break;
            default: sg_current_work_time = 25; break;
        }
        
        // 如果当前是空闲状态，更新显示
        if (sg_ui.state == POMODORO_STATE_IDLE) {
            __update_pomodoro_display();
        }
    }
}

static void __pomodoro_control_btn_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {
        switch (sg_ui.state) {
            case POMODORO_STATE_IDLE:
                // 开始
                __pomodoro_start();
                break;
            case POMODORO_STATE_WORK:
            case POMODORO_STATE_SHORT_BREAK:
            case POMODORO_STATE_LONG_BREAK:
                if (sg_ui.pomodoro_timer != NULL && lv_timer_get_paused(sg_ui.pomodoro_timer)) {
                    // 继续
                    lv_timer_resume(sg_ui.pomodoro_timer);
                } else {
                    // 暂停
                    __pomodoro_pause();
                }
                break;
        }
    }
}



static void __update_pomodoro_display(void)
{
    if (sg_ui.pomodoro_timer_label == NULL) return;
    
    if (sg_ui.state == POMODORO_STATE_IDLE) {
        // 空闲状态显示当前选择的时间
        char time_str[10];
        snprintf(time_str, sizeof(time_str), "%02d:00", sg_current_work_time);
        lv_label_set_text(sg_ui.pomodoro_timer_label, time_str);
    } else {
        // 运行状态显示剩余时间
        uint32_t minutes = sg_ui.remaining_seconds / 60;
        uint32_t seconds = sg_ui.remaining_seconds % 60;
        
        char time_str[10];
        snprintf(time_str, sizeof(time_str), "%02d:%02d", minutes, seconds);
        lv_label_set_text(sg_ui.pomodoro_timer_label, time_str);
    }
    
    // 更新进度条
    if (sg_ui.pomodoro_progress_bar != NULL && sg_ui.total_seconds > 0) {
        uint32_t progress = ((sg_ui.total_seconds - sg_ui.remaining_seconds) * 100) / sg_ui.total_seconds;
        lv_bar_set_value(sg_ui.pomodoro_progress_bar, progress, LV_ANIM_ON);
    }
    
    // 更新状态文本
    if (sg_ui.pomodoro_status_label != NULL) {
        lv_label_set_text(sg_ui.pomodoro_status_label, __get_pomodoro_status_text());
    }
    
    // 更新控制按钮文本
    if (sg_ui.pomodoro_control_btn != NULL) {
        lv_obj_t *btn_label = lv_obj_get_child(sg_ui.pomodoro_control_btn, 0);
        if (btn_label) {
            const char *btn_text;
            if (sg_ui.state == POMODORO_STATE_IDLE) {
                btn_text = "开始";
            } else if (sg_ui.pomodoro_timer != NULL && lv_timer_get_paused(sg_ui.pomodoro_timer)) {
                btn_text = "继续";
            } else {
                btn_text = "暂停";
            }
            lv_label_set_text(btn_label, btn_text);
        }
    }
    
    // 更新完成周期数
    if (sg_ui.pomodoro_cycles_label != NULL) {
        char cycles_str[20];
        snprintf(cycles_str, sizeof(cycles_str), "完成: %d 个", sg_ui.completed_cycles);
        lv_label_set_text(sg_ui.pomodoro_cycles_label, cycles_str);
    }
}

static void __update_clock_display(void)
{
    POSIX_TM_S tm = {0};
    tal_time_get_local_time_custom(0, &tm);
    
    // 更新时间
    if (sg_ui.clock_time_label != NULL) {
        char time_str[10];
        snprintf(time_str, sizeof(time_str), "%02d:%02d", tm.tm_hour, tm.tm_min);
        lv_label_set_text(sg_ui.clock_time_label, time_str);
    }
    
    // 更新日期
    if (sg_ui.clock_date_label != NULL) {
        char date_str[12];
        snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", 
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
        lv_label_set_text(sg_ui.clock_date_label, date_str);
    }
    
    // 更新星期
    if (sg_ui.clock_weekday_label != NULL) {
        const char *weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
        lv_label_set_text(sg_ui.clock_weekday_label, weekdays[tm.tm_wday]);
    }
}

static void __pomodoro_start(void)
{
    if (sg_ui.state == POMODORO_STATE_IDLE) {
        sg_ui.state = POMODORO_STATE_WORK;
        sg_ui.total_seconds = __get_pomodoro_duration();
        sg_ui.remaining_seconds = sg_ui.total_seconds;
    }
    
    if (sg_ui.pomodoro_timer == NULL) {
        sg_ui.pomodoro_timer = lv_timer_create(__pomodoro_timer_cb, 1000, NULL);
    } else {
        lv_timer_resume(sg_ui.pomodoro_timer);
    }
    
    __update_pomodoro_display();
}

static void __pomodoro_pause(void)
{
    if (sg_ui.pomodoro_timer != NULL) {
        lv_timer_pause(sg_ui.pomodoro_timer);
    }
    __update_pomodoro_display();
}

static void __pomodoro_reset(void)
{
    if (sg_ui.pomodoro_timer != NULL) {
        lv_timer_del(sg_ui.pomodoro_timer);
        sg_ui.pomodoro_timer = NULL;
    }
    
    sg_ui.state = POMODORO_STATE_IDLE;
    sg_ui.remaining_seconds = 0;
    sg_ui.total_seconds = 0;
    sg_ui.completed_cycles = 0;
    
    __update_pomodoro_display();
}

static void __pomodoro_next_phase(void)
{
    if (sg_ui.pomodoro_timer != NULL) {
        lv_timer_del(sg_ui.pomodoro_timer);
        sg_ui.pomodoro_timer = NULL;
    }
    
    switch (sg_ui.state) {
        case POMODORO_STATE_WORK:
            sg_ui.completed_cycles++;
            if (sg_ui.completed_cycles % POMODORO_LONG_BREAK_INTERVAL == 0) {
                sg_ui.state = POMODORO_STATE_LONG_BREAK;
            } else {
                sg_ui.state = POMODORO_STATE_SHORT_BREAK;
            }
            break;
        case POMODORO_STATE_SHORT_BREAK:
        case POMODORO_STATE_LONG_BREAK:
            sg_ui.state = POMODORO_STATE_WORK;
            break;
        default:
            sg_ui.state = POMODORO_STATE_IDLE;
            break;
    }
    
    if (sg_ui.state != POMODORO_STATE_IDLE) {
        sg_ui.total_seconds = __get_pomodoro_duration();
        sg_ui.remaining_seconds = sg_ui.total_seconds;
        __pomodoro_start();
    } else {
        __update_pomodoro_display();
    }
}

static const char* __get_pomodoro_status_text(void)
{
    switch (sg_ui.state) {
        case POMODORO_STATE_IDLE:
            return "准备开始";
        case POMODORO_STATE_WORK:
            return "专注工作";
        case POMODORO_STATE_SHORT_BREAK:
            return "短暂休息";
        case POMODORO_STATE_LONG_BREAK:
            return "长时休息";
        default:
            return "未知状态";
    }
}

static uint32_t __get_pomodoro_duration(void)
{
    switch (sg_ui.state) {
        case POMODORO_STATE_WORK:
            return sg_current_work_time * 60;
        case POMODORO_STATE_SHORT_BREAK:
            return POMODORO_SHORT_BREAK * 60;
        case POMODORO_STATE_LONG_BREAK:
            return POMODORO_LONG_BREAK * 60;
        default:
            return 0;
    }
}

int ui_pomodoro_clock_init(UI_FONT_T *ui_font, UI_THEME_T *ui_theme)
{
    if (ui_font == NULL || ui_theme == NULL) {
        return -1;
    }
    
    // 保存字体和主题
    sg_ui.font = *ui_font;
    sg_ui.theme = *ui_theme;
    
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, sg_ui.font.text, 0);
    lv_obj_set_style_text_color(screen, sg_ui.theme.text, 0);
    lv_obj_set_style_bg_color(screen, sg_ui.theme.background, 0);
    
    // 主容器
    sg_ui.container = lv_obj_create(screen);
    lv_obj_set_size(sg_ui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(sg_ui.container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sg_ui.container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);
    lv_obj_set_style_pad_all(sg_ui.container, 5, 0);
    lv_obj_set_style_border_width(sg_ui.container, 0, 0);
    lv_obj_set_style_radius(sg_ui.container, 0, 0);
    
    // 番茄钟面板
    sg_ui.left_panel = lv_obj_create(sg_ui.container);
    lv_obj_set_size(sg_ui.left_panel, LV_HOR_RES / 2 - 10, LV_VER_RES - 20);
    lv_obj_set_flex_flow(sg_ui.left_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sg_ui.left_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);
    lv_obj_set_style_pad_all(sg_ui.left_panel, 10, 0);
    lv_obj_set_style_border_width(sg_ui.left_panel, 2, 0);
    lv_obj_set_style_border_color(sg_ui.left_panel, sg_ui.theme.border, 0);
    lv_obj_set_style_radius(sg_ui.left_panel, 10, 0);
    
    // 番茄钟标题
    lv_obj_t *title_label = lv_label_create(sg_ui.left_panel);
    lv_obj_set_style_text_font(title_label, sg_ui.font.text, 0);
    lv_label_set_text(title_label, "🍅 番茄钟");
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // 番茄钟倒计时
    sg_ui.pomodoro_timer_label = lv_label_create(sg_ui.left_panel);
    lv_obj_set_style_text_font(sg_ui.pomodoro_timer_label, sg_ui.font.text, 0);
    lv_obj_set_style_text_color(sg_ui.pomodoro_timer_label, lv_color_hex(0xFF6B6B), 0);
    lv_label_set_text(sg_ui.pomodoro_timer_label, "25:00");
    lv_obj_set_style_text_align(sg_ui.pomodoro_timer_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // 进度条
    sg_ui.pomodoro_progress_bar = lv_bar_create(sg_ui.left_panel);
    lv_obj_set_size(sg_ui.pomodoro_progress_bar, LV_HOR_RES / 3 - 20, 10);
    lv_bar_set_range(sg_ui.pomodoro_progress_bar, 0, 100);
    lv_bar_set_value(sg_ui.pomodoro_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sg_ui.pomodoro_progress_bar, lv_color_hex(0xFF6B6B), LV_PART_INDICATOR);
    
    // 番茄钟状态
    sg_ui.pomodoro_status_label = lv_label_create(sg_ui.left_panel);
    lv_obj_set_style_text_font(sg_ui.pomodoro_status_label, sg_ui.font.text, 0);
    lv_label_set_text(sg_ui.pomodoro_status_label, "准备开始");
    lv_obj_set_style_text_align(sg_ui.pomodoro_status_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // 控制按钮容器
    lv_obj_t *control_container = lv_obj_create(sg_ui.left_panel);
    lv_obj_set_size(control_container, LV_SIZE_CONTENT, 40);
    lv_obj_set_flex_flow(control_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(control_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);
    lv_obj_set_style_pad_all(control_container, 5, 0);
    lv_obj_set_style_border_width(control_container, 0, 0);
    lv_obj_set_style_bg_opa(control_container, LV_OPA_TRANSP, 0);
    
    // 多功能控制按钮（开始/暂停/继续）
    sg_ui.pomodoro_control_btn = lv_btn_create(control_container);
    lv_obj_set_size(sg_ui.pomodoro_control_btn, 80, 40);
    lv_obj_add_event_cb(sg_ui.pomodoro_control_btn, __pomodoro_control_btn_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *btn_label = lv_label_create(sg_ui.pomodoro_control_btn);
    lv_label_set_text(btn_label, "开始");
    lv_obj_center(btn_label);

    // 独立复位按钮
    sg_ui.pomodoro_reset_btn = lv_btn_create(control_container);
    lv_obj_set_size(sg_ui.pomodoro_reset_btn, 80, 40);
    lv_obj_add_event_cb(sg_ui.pomodoro_reset_btn, __pomodoro_reset_btn_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *reset_label = lv_label_create(sg_ui.pomodoro_reset_btn);
    lv_label_set_text(reset_label, "复位");
    lv_obj_center(reset_label);
    

    
    // 时间选择下拉框
    lv_obj_t *time_label = lv_label_create(sg_ui.left_panel);
    lv_obj_set_style_text_font(time_label, sg_ui.font.text, 0);
    lv_label_set_text(time_label, "选择时间:");
    lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_CENTER, 0);

    sg_ui.pomodoro_time_dropdown = lv_dropdown_create(sg_ui.left_panel);
    lv_obj_set_size(sg_ui.pomodoro_time_dropdown, 120, 40);
    lv_dropdown_set_options(sg_ui.pomodoro_time_dropdown, "5分钟\n10分钟\n20分钟\n25分钟");
    lv_dropdown_set_selected(sg_ui.pomodoro_time_dropdown, 3); // 默认选择25分钟
    lv_obj_add_event_cb(sg_ui.pomodoro_time_dropdown, __pomodoro_time_dropdown_cb, LV_EVENT_ALL, NULL);

    // 完成周期数
    sg_ui.pomodoro_cycles_label = lv_label_create(sg_ui.left_panel);
    lv_obj_set_style_text_font(sg_ui.pomodoro_cycles_label, sg_ui.font.text, 0);
    lv_label_set_text(sg_ui.pomodoro_cycles_label, "完成: 0 个");
    lv_obj_set_style_text_align(sg_ui.pomodoro_cycles_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // 时钟面板
    sg_ui.right_panel = lv_obj_create(sg_ui.container);
    lv_obj_set_size(sg_ui.right_panel, LV_HOR_RES / 2 - 10, LV_VER_RES - 20);
    lv_obj_set_flex_flow(sg_ui.right_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sg_ui.right_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);
    lv_obj_set_style_pad_all(sg_ui.right_panel, 10, 0);
    lv_obj_set_style_border_width(sg_ui.right_panel, 2, 0);
    lv_obj_set_style_border_color(sg_ui.right_panel, sg_ui.theme.border, 0);
    lv_obj_set_style_radius(sg_ui.right_panel, 10, 0);
    
    // 时钟标题
    lv_obj_t *clock_title = lv_label_create(sg_ui.right_panel);
    lv_obj_set_style_text_font(clock_title, sg_ui.font.text, 0);
    lv_label_set_text(clock_title, "🕐 实时时钟");
    lv_obj_set_style_text_align(clock_title, LV_TEXT_ALIGN_CENTER, 0);
    
    // 当前时间
    sg_ui.clock_time_label = lv_label_create(sg_ui.right_panel);
    lv_obj_set_style_text_font(sg_ui.clock_time_label, sg_ui.font.text, 0);
    lv_obj_set_style_text_color(sg_ui.clock_time_label, lv_color_hex(0x4ECDC4), 0);
    lv_label_set_text(sg_ui.clock_time_label, "12:00");
    lv_obj_set_style_text_align(sg_ui.clock_time_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // 当前日期
    sg_ui.clock_date_label = lv_label_create(sg_ui.right_panel);
    lv_obj_set_style_text_font(sg_ui.clock_date_label, sg_ui.font.text, 0);
    lv_label_set_text(sg_ui.clock_date_label, "2025-01-01");
    lv_obj_set_style_text_align(sg_ui.clock_date_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // 星期
    sg_ui.clock_weekday_label = lv_label_create(sg_ui.right_panel);
    lv_obj_set_style_text_font(sg_ui.clock_weekday_label, sg_ui.font.text, 0);
    lv_label_set_text(sg_ui.clock_weekday_label, "周三");
    lv_obj_set_style_text_align(sg_ui.clock_weekday_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // 初始化时钟显示
    __update_clock_display();
    
    // 启动时钟定时器
    sg_ui.clock_timer = lv_timer_create(__clock_timer_cb, 1000, NULL);
    
    // 初始化番茄钟状态
    sg_ui.state = POMODORO_STATE_IDLE;
    sg_ui.completed_cycles = 0;
    sg_current_work_time = 25; // 默认25分钟
    __update_pomodoro_display();
    
    return 0;
}

void ui_pomodoro_clock_deinit(void)
{
    if (sg_ui.pomodoro_timer != NULL) {
        lv_timer_del(sg_ui.pomodoro_timer);
        sg_ui.pomodoro_timer = NULL;
    }
    
    if (sg_ui.clock_timer != NULL) {
        lv_timer_del(sg_ui.clock_timer);
        sg_ui.clock_timer = NULL;
    }
    
    if (sg_ui.container != NULL) {
        lv_obj_del(sg_ui.container);
        sg_ui.container = NULL;
    }
    
    memset(&sg_ui, 0, sizeof(sg_ui));
}

void ui_pomodoro_reset(void)
{
    __pomodoro_reset();
}

void ui_pomodoro_pause(void)
{
    __pomodoro_pause();
}

void ui_pomodoro_start(void)
{
    __pomodoro_start();
}

POMODORO_STATE_E ui_pomodoro_get_state(void)
{
    return sg_ui.state;
}

uint32_t ui_pomodoro_get_remaining_seconds(void)
{
    return sg_ui.remaining_seconds;
}

uint8_t ui_pomodoro_get_completed_cycles(void)
{
    return sg_ui.completed_cycles;
}