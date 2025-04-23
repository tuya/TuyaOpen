/**
 * @file display_gui_chatbot.c
 * @brief Implementation of the GUI for chat bot interface
 *
 * This source file provides the implementation for initializing and managing
 * the GUI components of a chat bot interface. It includes functions
 * to initialize the display, create the chat frame, handle chat messages,
 * and manage various display states such as listening, speaking, and network
 * status.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tal_api.h"
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
const char *POWER_TEXT = "你好啊，我来了，让我们一起玩耍吧";
const char *NET_OK_TEXT = "我已联网，让我们开始对话吧";
const char *NET_CFG_TEXT = "我已进入配网状态，你能帮我用涂鸦智能app配网嘛";

/***********************************************************
***********************variable define**********************
***********************************************************/
static lv_obj_t *sg_title_bar;

lv_obj_t *chat_message_label_;
lv_obj_t *status_label_;

LV_FONT_DECLARE(font_puhui_18_2);
LV_FONT_DECLARE(font_awesome_30_4);

/***********************************************************
***********************function define**********************
***********************************************************/
static void __gui_ai_chat_frame_init(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_text_font(screen, &font_puhui_18_2, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    lv_obj_t *container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Status bar */
    lv_obj_t *status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, font_puhui_18_2.line_height);
    lv_obj_set_style_radius(status_bar_, 0, 0);

    /* Content */
    lv_obj_t *content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);

    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN); // Vertical layout (from top to bottom)
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_SPACE_EVENLY); // Child objects are center-aligned and evenly distributed

    lv_obj_t *emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);

    chat_message_label_ = lv_label_create(content_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.9);                   // Limit width to 90% of screen width
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);           // Set to automatic line break mode
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0); // Set text to center alignment
    lv_label_set_text(chat_message_label_, "");

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);
    lv_obj_set_style_pad_left(status_bar_, 2, 0);
    lv_obj_set_style_pad_right(status_bar_, 2, 0);

    lv_obj_t *network_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_text_font(network_label_, &font_awesome_30_4, 0);
    lv_label_set_text(network_label_, FONT_AWESOME_WIFI);

    lv_obj_t *notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(status_label_, "待命");

    lv_obj_t *mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");

    lv_obj_t *battery_label_ = lv_label_create(status_bar_);
    lv_obj_set_style_text_font(battery_label_, &font_awesome_30_4, 0);
    lv_label_set_text(battery_label_, FONT_AWESOME_BATTERY_CHARGING);
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

static void __gui_show_listen_icon(uint8_t status)
{
    if (status == 0) {
        lv_label_set_text(status_label_, "待命");
    } else if (status == 1) {
        lv_label_set_text(status_label_, "聆听中...");
    } else if (status == 2) {
        lv_label_set_text(status_label_, "说话中...");
    }
};

static void __gui_create_message(const char *text, bool is_ai)
{
    lv_label_set_text(chat_message_label_, text);
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
    if (NULL == msg) {
        return;
    }

    tuya_lvgl_mutex_lock();

    switch (msg->type) {
    case TY_DISPLAY_TP_HUMAN_CHAT:
        __gui_create_message(msg->data, false);
        break;
    case TY_DISPLAY_TP_AI_CHAT:
        __gui_create_message(msg->data, true);
        break;
    case TY_DISPLAY_TP_STAT_LISTEN:
        __gui_show_listen_icon(1);
        break;
    case TY_DISPLAY_TP_STAT_SPEAK:
        __gui_show_listen_icon(2);
        break;
    case TY_DISPLAY_TP_STAT_IDLE:
        __gui_show_listen_icon(0);
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
