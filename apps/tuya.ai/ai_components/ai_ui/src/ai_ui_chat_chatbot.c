/**
 * @file ai_ui_chat_chatbot.c
 * @brief ai_ui_chat_chatbot module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_AI_CHAT_GUI_CHATBOT) && (ENABLE_AI_CHAT_GUI_CHATBOT == 1)
#include "lvgl.h"
#include "lv_vendor.h"

#include "font_awesome_symbols.h"

#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"

#include "ai_ui_chat_chatbot.h"

/***********************************************************
************************macro define************************
***********************************************************/
// Theme color structure
typedef struct {
    lv_color_t background;
    lv_color_t text;
    lv_color_t chat_background;
    lv_color_t user_bubble;
    lv_color_t assistant_bubble;
    lv_color_t system_bubble;
    lv_color_t system_text;
    lv_color_t border;
    lv_color_t low_battery;
}UI_THEME_COLORS_T;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *status_bar;
    lv_obj_t *content;
    lv_obj_t *emotion_label;
    lv_obj_t *chat_message_label;
    lv_obj_t *status_label;
    lv_obj_t *network_label;
    lv_obj_t *notification_label;
    lv_obj_t *mute_label;
    lv_obj_t *chat_mode_label;
} AI_UI_CHATBOT_T;


/***********************************************************
***********************typedef define***********************
***********************************************************/
static UI_THEME_COLORS_T sg_theme_colors;
static AI_UI_CHATBOT_T   sg_ui;
static AI_UI_FONT_LIST_T sg_font = {0};
static lv_timer_t       *sg_notification_tm = NULL;
static bool              sg_is_streaming = false;

/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/
static void __lvgl_init(void)
{
    lv_vendor_init(DISPLAY_NAME);

    lv_vendor_start(5, 1024*8);
}

static void __ui_font_init(void)
{
    sg_font.text       = ai_ui_get_text_font();
    sg_font.icon       = ai_ui_get_icon_font();
    sg_font.emoji      = ai_ui_get_emo_font();
    sg_font.emoji_list = ai_ui_get_emo_list();
}

static void __ui_light_theme_init(UI_THEME_COLORS_T *theme)
{
    if (theme == NULL) {
        return;
    }

    theme->background = lv_color_white();
    theme->text = lv_color_black();
    theme->chat_background = lv_color_hex(0xE0E0E0);
    theme->user_bubble = lv_color_hex(0x95EC69);
    theme->assistant_bubble = lv_color_white();
    theme->system_bubble = lv_color_hex(0xE0E0E0);
    theme->system_text = lv_color_hex(0x666666);
    theme->border = lv_color_hex(0xE0E0E0);
    theme->low_battery = lv_color_black();
}

static __attribute__((unused)) void __ui_dark_theme_init(UI_THEME_COLORS_T *theme)
{
    if (theme == NULL) {
        return;
    }

    theme->background = lv_color_hex(0x121212);
    theme->text = lv_color_white();
    theme->chat_background = lv_color_hex(0x1E1E1E);
    theme->user_bubble = lv_color_hex(0x1A6C37);
    theme->assistant_bubble = lv_color_hex(0x333333);
    theme->system_bubble = lv_color_hex(0x2A2A2A);
    theme->system_text = lv_color_hex(0xAAAAAA);
    theme->border = lv_color_hex(0x333333);
    theme->low_battery = lv_color_hex(0x333333);
}

static void __ui_notification_timeout_cb(lv_timer_t *timer)
{
    lv_timer_del(sg_notification_tm);
    sg_notification_tm = NULL;

    lv_obj_add_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.status_label, LV_OBJ_FLAG_HIDDEN);
}

static OPERATE_RET __ui_init(void)
{
    __lvgl_init();

    lv_vendor_disp_lock();

    __ui_light_theme_init(&sg_theme_colors);
    __ui_font_init();

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, sg_font.text, 0);
    lv_obj_set_style_text_color(screen, sg_theme_colors.text, 0);
    lv_obj_set_style_bg_color(screen, sg_theme_colors.background, 0);

    // Container
    sg_ui.container = lv_obj_create(screen);
    lv_obj_set_size(sg_ui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(sg_ui.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sg_ui.container, 0, 0);
    lv_obj_set_style_border_width(sg_ui.container, 0, 0);
    lv_obj_set_style_pad_row(sg_ui.container, 0, 0);
    lv_obj_set_style_bg_color(sg_ui.container, sg_theme_colors.background, 0);
    lv_obj_set_style_border_color(sg_ui.container, sg_theme_colors.border, 0);

    // Status bar
    sg_ui.status_bar = lv_obj_create(sg_ui.container);
    lv_obj_set_size(sg_ui.status_bar, LV_HOR_RES, sg_font.text->line_height);
    lv_obj_set_style_radius(sg_ui.status_bar, 0, 0);

    // Content
    sg_ui.content = lv_obj_create(sg_ui.container);
    lv_obj_set_scrollbar_mode(sg_ui.content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(sg_ui.content, 0, 0);
    lv_obj_set_width(sg_ui.content, LV_HOR_RES);
    lv_obj_set_flex_grow(sg_ui.content, 1);
    lv_obj_set_flex_flow(sg_ui.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sg_ui.content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);

    // Emotion
    sg_ui.emotion_label = lv_label_create(sg_ui.content);
    lv_obj_set_style_text_font(sg_ui.emotion_label, sg_font.emoji, 0);
    lv_label_set_text(sg_ui.emotion_label, sg_font.emoji_list[0].emo_icon);

    // Chat message
    sg_ui.chat_message_label = lv_label_create(sg_ui.content);
    lv_label_set_text(sg_ui.chat_message_label, "");
    lv_obj_set_width(sg_ui.chat_message_label, LV_HOR_RES * 0.9);         // Limit width to 90% of screen width
    lv_obj_set_height(sg_ui.chat_message_label, LV_VER_RES * 0.5);         // Limit height to 50% of screen height
    lv_label_set_long_mode(sg_ui.chat_message_label, LV_LABEL_LONG_WRAP); // Set to automatic line break mode
    lv_obj_set_style_text_align(sg_ui.chat_message_label, LV_TEXT_ALIGN_CENTER, 0); // Set text to center alignment
    lv_label_set_text(sg_ui.chat_message_label, "");

    // Status bar
    // lv_obj_set_flex_flow(sg_ui.status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_border_width(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_pad_column(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_pad_left(sg_ui.status_bar, 2, 0);
    lv_obj_set_style_bg_color(sg_ui.status_bar, sg_theme_colors.background, 0);

    sg_ui.chat_mode_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_style_text_color(sg_ui.chat_mode_label, sg_theme_colors.text, 0);
    lv_label_set_text(sg_ui.chat_mode_label, "");
    lv_obj_align(sg_ui.chat_mode_label, LV_ALIGN_LEFT_MID, 5, 0);

    // Notification label
    sg_ui.notification_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.notification_label, 1);
    lv_obj_set_style_text_align(sg_ui.notification_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(sg_ui.notification_label, sg_theme_colors.text, 0);
    lv_label_set_text(sg_ui.notification_label, "");
    lv_obj_align(sg_ui.notification_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);

    // Status label
    sg_ui.status_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.status_label, 1);
    lv_label_set_long_mode(sg_ui.status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(sg_ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(sg_ui.status_label, sg_theme_colors.text, 0);
    lv_label_set_text(sg_ui.status_label, INITIALIZING);
    lv_obj_align(sg_ui.status_label, LV_ALIGN_CENTER, 0, 0);

    // Network status
    sg_ui.network_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_style_text_font(sg_ui.network_label, sg_font.icon, 0);
    lv_obj_set_style_text_color(sg_ui.network_label, sg_theme_colors.text, 0);
    lv_obj_align(sg_ui.network_label, LV_ALIGN_RIGHT_MID, -5, 0);

#if defined(ENABLE_CIRCLE_UI_STYLE) && (ENABLE_CIRCLE_UI_STYLE == 1)    
    lv_obj_set_style_pad_left(sg_ui.status_bar, LV_HOR_RES * 0.1, 0);
    lv_obj_set_style_pad_right(sg_ui.status_bar, LV_HOR_RES * 0.1, 0);
#endif

    lv_vendor_disp_unlock();

    return 0;
}

static void __ui_set_user_msg(char *text)
{
    if (sg_ui.chat_message_label == NULL) {
        return;
    }
    
    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_message_label, text);
    lv_obj_set_style_bg_color(sg_ui.chat_message_label, sg_theme_colors.user_bubble, 0);
    lv_obj_set_style_text_color(sg_ui.chat_message_label, sg_theme_colors.text, 0);
    lv_vendor_disp_unlock();
}

static void __ui_set_ai_msg(char *text)
{
    if (sg_ui.chat_message_label == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_message_label, text);
    lv_obj_set_style_bg_color(sg_ui.chat_message_label, sg_theme_colors.assistant_bubble, 0);
    lv_obj_set_style_text_color(sg_ui.chat_message_label, sg_theme_colors.text, 0);
    lv_vendor_disp_unlock();
}

static void __ui_set_ai_msg_stream_start(void)
{
    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_message_label, "");
    lv_obj_set_style_bg_color(sg_ui.chat_message_label, sg_theme_colors.assistant_bubble, 0);
    lv_obj_set_style_text_color(sg_ui.chat_message_label, sg_theme_colors.text, 0);
    lv_vendor_disp_unlock();

    sg_is_streaming = true;
}

static void __ui_set_ai_msg_stream_data(char *text)
{
    if (sg_ui.chat_message_label == NULL || !sg_is_streaming) {
        return;
    }

    lv_vendor_disp_lock(); 
    lv_label_ins_text(sg_ui.chat_message_label, LV_LABEL_POS_LAST, text);
    lv_vendor_disp_unlock();
}

static void __ui_set_ai_msg_stream_end(void)
{
    sg_is_streaming = false;
}

static void __ui_set_system_msg(char *text)
{
    if (sg_ui.chat_message_label == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_message_label, text);
    lv_obj_set_style_bg_color(sg_ui.chat_message_label, sg_theme_colors.system_bubble, 0);
    lv_obj_set_style_text_color(sg_ui.chat_message_label, sg_theme_colors.system_text, 0);
    lv_vendor_disp_unlock();
}

static void __ui_set_emotion(char *emotion)
{
    if (NULL == sg_ui.emotion_label) {
        return;
    }

    char *emo_icon = sg_font.emoji_list[0].emo_icon;
    for (int i = 0; i < FONT_EMO_ICON_MAX_NUM; i++) {
        if (strcmp(emotion, sg_font.emoji_list[i].emo_name) == 0) {
            emo_icon = sg_font.emoji_list[i].emo_icon;
            break;
        }
    }

    lv_vendor_disp_lock();
    lv_obj_set_style_text_font(sg_ui.emotion_label, sg_font.emoji, 0);
    lv_label_set_text(sg_ui.emotion_label, emo_icon);
    lv_vendor_disp_unlock();
}

static void __ui_set_status(char *status)
{
    if (sg_ui.status_label == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.status_label, status);
    lv_obj_set_style_text_color(sg_ui.status_label, sg_theme_colors.text, 0);
    lv_obj_set_style_text_align(sg_ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_vendor_disp_unlock();
}

static void __ui_set_notification(char *notification)
{
    if (sg_ui.notification_label == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.notification_label, notification);
    lv_obj_add_flag(sg_ui.status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    if (NULL == sg_notification_tm) {
        sg_notification_tm = lv_timer_create(__ui_notification_timeout_cb, 3000, NULL);
    } else {
        lv_timer_reset(sg_notification_tm);
    }
    lv_vendor_disp_unlock();
}

static void __ui_set_network(AI_UI_WIFI_STATUS_E wifi_status)
{
    char *wifi_icon = ai_ui_get_wifi_icon(wifi_status);

    if (sg_ui.network_label == NULL || wifi_icon == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.network_label, wifi_icon);
    lv_vendor_disp_unlock();
}

static void __ui_set_chat_mode(char *chat_mode)
{
    if (sg_ui.chat_mode_label == NULL || NULL == chat_mode) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_mode_label, chat_mode);
    lv_vendor_disp_unlock();
}

OPERATE_RET ai_ui_chat_chatbot_register(void)
{
    AI_UI_INTFS_T intfs;

    memset(&intfs, 0, sizeof(AI_UI_INTFS_T));

    intfs.disp_init                = __ui_init;
    intfs.disp_user_msg            = __ui_set_user_msg;
    intfs.disp_ai_msg              = __ui_set_ai_msg;   
    intfs.disp_ai_msg_stream_start = __ui_set_ai_msg_stream_start;
    intfs.disp_ai_msg_stream_data  = __ui_set_ai_msg_stream_data;
    intfs.disp_ai_msg_stream_end   = __ui_set_ai_msg_stream_end;
    intfs.disp_system_msg          = __ui_set_system_msg;
    intfs.disp_emotion             = __ui_set_emotion;
    intfs.disp_ai_mode_state       = __ui_set_status;
    intfs.disp_notification        = __ui_set_notification;
    intfs.disp_wifi_state          = __ui_set_network;
    intfs.disp_ai_chat_mode        = __ui_set_chat_mode;

    return ai_ui_register(&intfs);
}
#endif