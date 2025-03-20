/**
 * @file tuya_display_lvgl.c
 * @version 0.1
 * @date 2025-03-19
 */
#include "tkl_display.h"
#include "tkl_queue.h"
#include "tkl_memory.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tal_mutex.h"
#include "tal_log.h"

#include "lvgl.h"
#include "lv_port_disp.h"
#include "font_awesome_symbols.h"
#include "tuya_display.h"
#include "tuya_lcd_device.h"
/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************extern  declaration********************
***********************************************************/
extern const lv_img_dsc_t TuyaOpen_img;
extern const lv_img_dsc_t LISTEN_icon;
/***********************************************************
***********************variable define**********************
***********************************************************/
static MUTEX_HANDLE     sg_lvgl_mutex_hdl = NULL;
static THREAD_HANDLE    sg_lvgl_thrd_hdl = NULL;

static TKL_DISP_DEVICE_S sg_display_device = {
    .device_id = 0,
    .device_port = TKL_DISP_LCD,
    .device_info = NULL,
};

/****lvgl****/
static lv_style_t style_avatar;
static lv_style_t style_ai_bubble;
static lv_style_t style_user_bubble;

static lv_obj_t* sg_title_bar;
static lv_obj_t* sg_msg_container;

static inline uint32_t calc_bubble_width() {
    return DISPLAY_LCD_WIDTH - 85;
}

LV_FONT_DECLARE(FONT_SY_20);
LV_FONT_DECLARE(font_awesome_30_4);
/***********************************************************
***********************function define**********************
***********************************************************/
static uint32_t lv_tick_get_cb(void)
{
    return (uint32_t)tkl_system_get_millisecond();
}

static void __init_styles(void) 
{
    lv_style_init(&style_avatar);
    lv_style_set_radius(&style_avatar, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&style_avatar, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_border_width(&style_avatar, 1);
    lv_style_set_border_color(&style_avatar, lv_palette_darken(LV_PALETTE_GREY, 2));

    lv_style_init(&style_ai_bubble);
    lv_style_set_bg_color(&style_ai_bubble, lv_color_white());
    lv_style_set_radius(&style_ai_bubble, 15);
    lv_style_set_pad_all(&style_ai_bubble, 12);
    lv_style_set_shadow_width(&style_ai_bubble, 12);
    lv_style_set_shadow_color(&style_ai_bubble, lv_color_hex(0xCCCCCC));

    lv_style_init(&style_user_bubble);
    lv_style_set_bg_color(&style_user_bubble, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_text_color(&style_user_bubble, lv_color_white());
    lv_style_set_radius(&style_user_bubble, 15);
    lv_style_set_pad_all(&style_user_bubble, 12);
    lv_style_set_shadow_width(&style_user_bubble, 12);
    lv_style_set_shadow_color(&style_user_bubble, lv_palette_darken(LV_PALETTE_GREEN, 2));
}

static void __create_ai_chat_ui(void) 
{
    __init_styles();

    lv_obj_t* main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_cont, DISPLAY_LCD_WIDTH, DISPLAY_LCD_HEIGHT);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_pad_all(main_cont, 0, 0);
    // lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_text_font(main_cont, &FONT_SY_20, 0);
    lv_obj_set_style_text_color(main_cont, lv_color_black(), 0);
    lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(main_cont, LV_DIR_NONE);

    lv_obj_t* sg_title_bar = lv_obj_create(main_cont);
    lv_obj_set_size(sg_title_bar, LV_PCT(100), 40);
    lv_obj_set_style_bg_color(sg_title_bar, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_scrollbar_mode(sg_title_bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(sg_title_bar, LV_DIR_NONE);
    lv_obj_t* title = lv_label_create(sg_title_bar);
    lv_label_set_text(title,  "AI聊天伙伴");
    lv_obj_center(title);

    sg_msg_container = lv_obj_create(main_cont);
    lv_obj_set_size(sg_msg_container, DISPLAY_LCD_WIDTH, DISPLAY_LCD_HEIGHT - 40); 
    lv_obj_set_flex_flow(sg_msg_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_ver(sg_msg_container, 8, 0);
    lv_obj_set_style_pad_hor(sg_msg_container, 10, 0);
    lv_obj_set_y(sg_msg_container, 40);
    lv_obj_move_background(sg_msg_container);

    lv_obj_set_scroll_dir(sg_msg_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(sg_msg_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(sg_msg_container, LV_OPA_TRANSP, 0);
}

static void __add_wifi_state(bool is_connected) 
{
    static lv_obj_t* icon = NULL;

    if(NULL == sg_title_bar) {
        return;
    }

    if(icon == NULL) {
        icon = lv_label_create(sg_title_bar);
    }

    lv_label_set_text(icon,  (is_connected == true)? FONT_AWESOME_WIFI:FONT_AWESOME_WIFI_OFF);
    lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 30, 0);
};


static void __create_message(const char* text, bool is_ai) 
{
    lv_obj_t* msg_cont = lv_obj_create(sg_msg_container);
    lv_obj_remove_style_all(msg_cont);
    lv_obj_set_size(msg_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(msg_cont, 6, 0);
    lv_obj_set_flex_flow(msg_cont, is_ai ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_ROW_REVERSE);
    lv_obj_set_style_pad_column(msg_cont, 10, 0);

    lv_obj_t* avatar = lv_obj_create(msg_cont);
    lv_obj_set_style_text_font(avatar, &font_awesome_30_4, 0);
    lv_obj_add_style(avatar, &style_avatar, 0);
    lv_obj_set_size(avatar, 40, 40);
    lv_obj_t* icon = lv_label_create(avatar);
    lv_label_set_text(icon, is_ai ? FONT_AWESOME_USER_ROBOT : FONT_AWESOME_USER);
    lv_obj_center(icon);

    lv_obj_t* bubble = lv_obj_create(msg_cont);
    lv_obj_set_width(bubble, calc_bubble_width());
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_add_style(bubble, is_ai ? &style_ai_bubble : &style_user_bubble, 0);
    
    lv_obj_set_scrollbar_mode(bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(bubble, LV_DIR_NONE);

    lv_obj_t* text_cont = lv_obj_create(bubble);
    lv_obj_remove_style_all(text_cont);
    lv_obj_set_size(text_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* label = lv_label_create(text_cont);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, calc_bubble_width() - 24);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

    lv_obj_scroll_to_view(msg_cont, LV_ANIM_ON);
    lv_obj_update_layout(sg_msg_container);
}

static void __create_homepage(void) 
{
    lv_obj_t * img = lv_image_create(lv_scr_act());
    lv_image_set_src(img, &TuyaOpen_img);

    lv_obj_center(img);
}


static void __lvgl_task(void *args)
{
    uint32_t sleep_time = 0;

    while(1) {
        tal_mutex_lock(sg_lvgl_mutex_hdl);

        sleep_time = lv_timer_handler();

        tal_mutex_unlock(sg_lvgl_mutex_hdl);

        if(sleep_time > 500) {
            sleep_time = 500;
        }else if(sleep_time<4) {
            sleep_time = 4;
        }
        
        tal_system_sleep(sleep_time);
    }
}

OPERATE_RET tuya_display_lvgl_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    //register lcd device
    TUYA_CALL_ERR_RETURN(tuya_lcd_device_register(sg_display_device.device_id));

    /*lvgl init*/
    lv_init();
    lv_tick_set_cb(lv_tick_get_cb);
    lv_port_disp_init(&sg_display_device);
 
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_lvgl_mutex_hdl));

    THREAD_CFG_T cfg = {
        .thrdname = "lvgl",
        .priority = THREAD_PRIO_1,
        .stackDepth = 1024*4,
    };

    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_lvgl_thrd_hdl, NULL, NULL, __lvgl_task, NULL, &cfg));

    return OPRT_OK;
}

void tuya_display_lv_homepage(void) 
{
    tal_mutex_lock(sg_lvgl_mutex_hdl);
    __create_homepage();
    tal_mutex_unlock(sg_lvgl_mutex_hdl);
}

void tuya_display_lv_chat_message(const char* text, bool is_ai)
{
    static bool is_create_ui = false;

    tal_mutex_lock(sg_lvgl_mutex_hdl);

    if(false == is_create_ui) {
        __create_ai_chat_ui();
        is_create_ui = true;
    }

    __create_message(text, is_ai);

    tal_mutex_unlock(sg_lvgl_mutex_hdl);
}

void tuya_display_lv_wifi_state(bool is_connected)
{
    static bool is_create_ui = false;

    tal_mutex_lock(sg_lvgl_mutex_hdl);

    __add_wifi_state(is_connected);

    tal_mutex_unlock(sg_lvgl_mutex_hdl);
}