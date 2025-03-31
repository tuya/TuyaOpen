/**
 * @file display_gui_main.c
 * @brief Implementation of the GUI for WeChat-like chat interface
 *
 * This source file provides the implementation for initializing and managing
 * the GUI components of a WeChat-like chat interface. It includes functions
 * to initialize the display, create the chat frame, handle chat messages,
 * and manage various display states such as listening, speaking, and network
 * status.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "lvgl.h"
#include "font_awesome_symbols.h"
#include "display_gui.h"
#include "tuya_lvgl.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************extern define************************
***********************************************************/
extern const lv_img_dsc_t TuyaOpen_img;
extern const lv_img_dsc_t LISTEN_icon;

/***********************************************************
***********************const declaration********************
***********************************************************/
const char *POWER_TEXT   = "你好啊，我来了，让我们一起玩耍吧";
const char *NET_OK_TEXT  = "我已联网，让我们开始对话吧";
const char *NET_CFG_TEXT = "我已进入配网状态，你能帮我用涂鸦智能app配网嘛";

/***********************************************************
***********************variable define**********************
***********************************************************/
static lv_style_t style_avatar;
static lv_style_t style_ai_bubble;
static lv_style_t style_user_bubble;

static lv_obj_t *sg_title_bar;
static lv_obj_t *sg_title_text;
static lv_obj_t *sg_msg_container;

LV_FONT_DECLARE(FONT_SY_20);
LV_FONT_DECLARE(font_awesome_30_4);

/***********************************************************
***********************function define**********************
***********************************************************/
static inline uint32_t calc_bubble_width()
{
    return LV_PCT(75);
}

static void __gui_lv_styles_init(void)
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

static void __gui_ai_chat_frame_init(void)
{
    __gui_lv_styles_init();

    lv_obj_t *main_cont = lv_obj_create(lv_scr_act());
    // lv_obj_set_size(main_cont, DISPLAY_LCD_WIDTH, DISPLAY_LCD_HEIGHT);
    lv_obj_set_size(main_cont,  LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_pad_all(main_cont, 0, 0);

    lv_obj_set_style_text_font(main_cont, &FONT_SY_20, 0);
    lv_obj_set_style_text_color(main_cont, lv_color_black(), 0);
    lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(main_cont, LV_DIR_NONE);

    sg_title_bar = lv_obj_create(main_cont);
    lv_obj_set_size(sg_title_bar, LV_PCT(100), 40);
    lv_obj_set_style_bg_color(sg_title_bar, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_scrollbar_mode(sg_title_bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(sg_title_bar, LV_DIR_NONE);
    sg_title_text = lv_label_create(sg_title_bar);
    lv_label_set_text(sg_title_text, "AI聊天伙伴");
    lv_obj_center(sg_title_text);

    sg_msg_container = lv_obj_create(main_cont);
    lv_obj_set_size(sg_msg_container, LV_PCT(100), LV_PCT(92));
    lv_obj_set_flex_flow(sg_msg_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_ver(sg_msg_container, 8, 0);
    lv_obj_set_style_pad_hor(sg_msg_container, 10, 0);
    lv_obj_set_y(sg_msg_container, 40);
    lv_obj_move_background(sg_msg_container);

    lv_obj_set_scroll_dir(sg_msg_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(sg_msg_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(sg_msg_container, LV_OPA_TRANSP, 0);
}

static void __gui_add_wifi_icon(bool is_connected)
{
    static lv_obj_t *icon = NULL;

    if (icon == NULL) {
        icon = lv_label_create(sg_title_bar);
        lv_obj_set_style_text_font(icon, &font_awesome_30_4, 0);
    }

    lv_label_set_text(icon, (is_connected == true) ? FONT_AWESOME_WIFI : FONT_AWESOME_WIFI_OFF);
    lv_obj_align(icon, LV_ALIGN_RIGHT_MID, 0, 0);
};

static void __gui_show_listen_icon(bool is_listen)
{
    static lv_obj_t *listen_icon = NULL;

    if (listen_icon == NULL) {
        listen_icon = lv_image_create(sg_title_bar);
        lv_image_set_src(listen_icon, &LISTEN_icon);
        lv_obj_align(listen_icon, LV_ALIGN_LEFT_MID, LV_PCT(15), 0);
    }

    if (is_listen) {
        lv_label_set_text(sg_title_text, "聆听中......");
        lv_obj_clear_flag(listen_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(sg_title_text, "AI聊天伙伴");
        lv_obj_add_flag(listen_icon, LV_OBJ_FLAG_HIDDEN);
    }
};

static void __gui_create_message(const char *text, bool is_ai)
{
    lv_obj_t *msg_cont = lv_obj_create(sg_msg_container);
    lv_obj_remove_style_all(msg_cont);
    lv_obj_set_size(msg_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(msg_cont, 6, 0);
    lv_obj_set_flex_flow(msg_cont, is_ai ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_ROW_REVERSE);
    lv_obj_set_style_pad_column(msg_cont, 10, 0);

    lv_obj_t *avatar = lv_obj_create(msg_cont);
    lv_obj_set_style_text_font(avatar, &font_awesome_30_4, 0);
    lv_obj_add_style(avatar, &style_avatar, 0);
    lv_obj_set_size(avatar, 40, 40);
    lv_obj_t *icon = lv_label_create(avatar);
    lv_label_set_text(icon, is_ai ? FONT_AWESOME_USER_ROBOT : FONT_AWESOME_USER);
    lv_obj_center(icon);

    lv_obj_t *bubble = lv_obj_create(msg_cont);
    lv_obj_set_width(bubble, LV_PCT(75));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_add_style(bubble, is_ai ? &style_ai_bubble : &style_user_bubble, 0);

    lv_obj_set_scrollbar_mode(bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(bubble, LV_DIR_NONE);

    lv_obj_t *text_cont = lv_obj_create(bubble);
    lv_obj_remove_style_all(text_cont);
    lv_obj_set_size(text_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *label = lv_label_create(text_cont);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

    lv_obj_scroll_to_view(msg_cont, LV_ANIM_ON);
    lv_obj_update_layout(sg_msg_container);
}

/**
 * @brief Initialize the GUI display
 * 
 * @param None
 * @return OPERATE_RET Initialization result, OPRT_OK indicates success
 */
OPERATE_RET display_gui_init(void)
{
    return tuya_lvgl_init();
}

/**
 * @brief Display the homepage of the GUI
 * 
 * @param None
 * @return None
 */
void display_gui_homepage(void)
{
    tuya_lvgl_mutex_lock();

    lv_obj_t *homepage_img = lv_image_create(lv_scr_act());
    lv_image_set_src(homepage_img, &TuyaOpen_img);

    lv_obj_center(homepage_img);

    tuya_lvgl_mutex_unlock();

    tal_system_sleep(2000);
}

/**
 * @brief Initialize the chat frame in the GUI
 * 
 * @param None
 * @return None
 */
void display_gui_chat_frame_init(void)
{
    tuya_lvgl_mutex_lock();
    
    __gui_ai_chat_frame_init();

    tuya_lvgl_mutex_unlock();
}

/**
 * @brief Handle display messages for the GUI
 * 
 * @param msg Pointer to the chat message structure
 * @return None
 */
void display_gui_chat_msg_handle(DISP_CHAT_MSG_T *msg)
{
    if(NULL == msg) {
        return;
    }

    tuya_lvgl_mutex_lock();

    switch(msg->type) {
        case TY_DISPLAY_TP_HUMAN_CHAT:
            __gui_create_message(msg->data, false);
            break;
        case TY_DISPLAY_TP_AI_CHAT:
            __gui_create_message(msg->data, true);
            break;
        case TY_DISPLAY_TP_STAT_LISTEN:
            __gui_show_listen_icon(true);
            break;
        case TY_DISPLAY_TP_STAT_SPEAK:
            break;
        case TY_DISPLAY_TP_STAT_IDLE:
            __gui_show_listen_icon(false);
            break;
        case TY_DISPLAY_TP_STAT_NETCFG:
            __gui_add_wifi_icon(false);
            __gui_create_message(NET_CFG_TEXT, true);
            break;
        case TY_DISPLAY_TP_STAT_POWERON:
            __gui_create_message(POWER_TEXT, true);
            break;
        case TY_DISPLAY_TP_STAT_ONLINE:
            __gui_add_wifi_icon(true);
            __gui_create_message(NET_OK_TEXT, true);
            break;
        default:
            break;
        }

    tuya_lvgl_mutex_unlock();
}
