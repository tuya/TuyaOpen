/**
 * @file ui_chatbot.c
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

#include "tuya_cloud_types.h"

#if defined(ENABLE_GUI_CHATBOT) && (ENABLE_GUI_CHATBOT == 1)

#include "ui_display.h"

#include "font_awesome_symbols.h"
#include "lvgl.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
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
} APP_THEME_COLORS_T;

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
} APP_UI_T;

typedef struct {
    APP_UI_T ui;
    APP_THEME_COLORS_T theme;

    UI_FONT_T font;

    lv_timer_t *notification_tm;
} APP_CHATBOT_UI_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_CHATBOT_UI_T sg_ui = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __ui_light_theme_init(APP_THEME_COLORS_T *theme)
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

static __attribute__((unused)) void __ui_dark_theme_init(APP_THEME_COLORS_T *theme)
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

int __ui_font_init(UI_FONT_T *ui_font)
{
    if (ui_font == NULL) {
        return -1;
    }

    sg_ui.font.text = ui_font->text;
    sg_ui.font.icon = ui_font->icon;
    sg_ui.font.emoji = ui_font->emoji;
    sg_ui.font.emoji_list = ui_font->emoji_list;

    return 0;
}

static void __ui_notification_timeout_cb(lv_timer_t *timer)
{
    lv_timer_del(sg_ui.notification_tm);
    sg_ui.notification_tm = NULL;

    lv_obj_add_flag(sg_ui.ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.ui.status_label, LV_OBJ_FLAG_HIDDEN);
}

int ui_init(UI_FONT_T *ui_font)
{
    // Theme init
    __ui_light_theme_init(&sg_ui.theme);

    // Font init
    __ui_font_init(ui_font);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, sg_ui.font.text, 0);
    lv_obj_set_style_text_color(screen, sg_ui.theme.text, 0);
    lv_obj_set_style_bg_color(screen, sg_ui.theme.background, 0);

    // Container
    sg_ui.ui.container = lv_obj_create(screen);
    lv_obj_set_size(sg_ui.ui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(sg_ui.ui.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sg_ui.ui.container, 0, 0);
    lv_obj_set_style_border_width(sg_ui.ui.container, 0, 0);
    lv_obj_set_style_pad_row(sg_ui.ui.container, 0, 0);
    lv_obj_set_style_bg_color(sg_ui.ui.container, sg_ui.theme.background, 0);
    lv_obj_set_style_border_color(sg_ui.ui.container, sg_ui.theme.border, 0);

    // Status bar
    sg_ui.ui.status_bar = lv_obj_create(sg_ui.ui.container);
    lv_obj_set_size(sg_ui.ui.status_bar, LV_HOR_RES, sg_ui.font.text->line_height);
    lv_obj_set_style_radius(sg_ui.ui.status_bar, 0, 0);

    // Content
    sg_ui.ui.content = lv_obj_create(sg_ui.ui.container);
    lv_obj_set_scrollbar_mode(sg_ui.ui.content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(sg_ui.ui.content, 0, 0);
    lv_obj_set_width(sg_ui.ui.content, LV_HOR_RES);
    lv_obj_set_flex_grow(sg_ui.ui.content, 1);
    lv_obj_set_flex_flow(sg_ui.ui.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sg_ui.ui.content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);

    // Emotion
    sg_ui.ui.emotion_label = lv_label_create(sg_ui.ui.content);
    lv_obj_set_style_text_font(sg_ui.ui.emotion_label, sg_ui.font.emoji, 0);
    lv_label_set_text(sg_ui.ui.emotion_label, sg_ui.font.emoji_list[0].emo_icon);

    // Chat message
    sg_ui.ui.chat_message_label = lv_label_create(sg_ui.ui.content);
    lv_label_set_text(sg_ui.ui.chat_message_label, "");
    lv_obj_set_width(sg_ui.ui.chat_message_label, LV_HOR_RES * 0.9);         // Limit width to 90% of screen width
    lv_label_set_long_mode(sg_ui.ui.chat_message_label, LV_LABEL_LONG_WRAP); // Set to automatic line break mode
    lv_obj_set_style_text_align(sg_ui.ui.chat_message_label, LV_TEXT_ALIGN_CENTER, 0); // Set text to center alignment
    lv_label_set_text(sg_ui.ui.chat_message_label, "");

    // Status bar
    // lv_obj_set_flex_flow(sg_ui.ui.status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(sg_ui.ui.status_bar, 0, 0);
    lv_obj_set_style_border_width(sg_ui.ui.status_bar, 0, 0);
    lv_obj_set_style_pad_column(sg_ui.ui.status_bar, 0, 0);
    lv_obj_set_style_pad_left(sg_ui.ui.status_bar, 2, 0);
    lv_obj_set_style_bg_color(sg_ui.ui.status_bar, sg_ui.theme.background, 0);

    sg_ui.ui.chat_mode_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_style_text_color(sg_ui.ui.chat_mode_label, sg_ui.theme.text, 0);
    lv_label_set_text(sg_ui.ui.chat_mode_label, "");
    lv_obj_align(sg_ui.ui.chat_mode_label, LV_ALIGN_LEFT_MID, 5, 0);

    // Notification label
    sg_ui.ui.notification_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.ui.notification_label, 1);
    lv_obj_set_style_text_align(sg_ui.ui.notification_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(sg_ui.ui.notification_label, sg_ui.theme.text, 0);
    lv_label_set_text(sg_ui.ui.notification_label, "");
    lv_obj_align(sg_ui.ui.notification_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(sg_ui.ui.notification_label, LV_OBJ_FLAG_HIDDEN);

    // Status label
    sg_ui.ui.status_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.ui.status_label, 1);
    lv_label_set_long_mode(sg_ui.ui.status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(sg_ui.ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(sg_ui.ui.status_label, sg_ui.theme.text, 0);
    lv_label_set_text(sg_ui.ui.status_label, INITIALIZING);
    lv_obj_align(sg_ui.ui.status_label, LV_ALIGN_CENTER, 0, 0);

    // Network status
    sg_ui.ui.network_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_style_text_font(sg_ui.ui.network_label, sg_ui.font.icon, 0);
    lv_obj_set_style_text_color(sg_ui.ui.network_label, sg_ui.theme.text, 0);
    lv_obj_align(sg_ui.ui.network_label, LV_ALIGN_RIGHT_MID, -5, 0);

    return 0;
}

void ui_set_user_msg(const char *text)
{
    if (sg_ui.ui.chat_message_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.chat_message_label, text);
    lv_obj_set_style_bg_color(sg_ui.ui.chat_message_label, sg_ui.theme.user_bubble, 0);
    lv_obj_set_style_text_color(sg_ui.ui.chat_message_label, sg_ui.theme.text, 0);
}

void ui_set_assistant_msg(const char *text)
{
    if (sg_ui.ui.chat_message_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.chat_message_label, text);
    lv_obj_set_style_bg_color(sg_ui.ui.chat_message_label, sg_ui.theme.assistant_bubble, 0);
    lv_obj_set_style_text_color(sg_ui.ui.chat_message_label, sg_ui.theme.text, 0);
}

void ui_set_system_msg(const char *text)
{
    if (sg_ui.ui.chat_message_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.chat_message_label, text);
    lv_obj_set_style_bg_color(sg_ui.ui.chat_message_label, sg_ui.theme.system_bubble, 0);
    lv_obj_set_style_text_color(sg_ui.ui.chat_message_label, sg_ui.theme.system_text, 0);
}

void ui_set_emotion(const char *emotion)
{
    if (NULL == sg_ui.ui.emotion_label) {
        return;
    }

    char *emo_icon = sg_ui.font.emoji_list[0].emo_icon;
    for (int i = 0; i < EMO_ICON_MAX_NUM; i++) {
        if (strcmp(emotion, sg_ui.font.emoji_list[i].emo_text) == 0) {
            emo_icon = sg_ui.font.emoji_list[i].emo_icon;
            break;
        }
    }

    lv_obj_set_style_text_font(sg_ui.ui.emotion_label, sg_ui.font.emoji, 0);
    lv_label_set_text(sg_ui.ui.emotion_label, emo_icon);
}

void ui_set_status(const char *status)
{
    if (sg_ui.ui.status_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.status_label, status);
    lv_obj_set_style_text_color(sg_ui.ui.status_label, sg_ui.theme.text, 0);
    lv_obj_set_style_text_align(sg_ui.ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
}

void ui_set_notification(const char *notification)
{
    if (sg_ui.ui.notification_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.notification_label, notification);
    lv_obj_add_flag(sg_ui.ui.status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    if (NULL == sg_ui.notification_tm) {
        sg_ui.notification_tm = lv_timer_create(__ui_notification_timeout_cb, 3000, NULL);
    } else {
        lv_timer_reset(sg_ui.notification_tm);
    }
}

void ui_set_network(char *wifi_icon)
{
    if (sg_ui.ui.network_label == NULL || wifi_icon == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.network_label, wifi_icon);
}

void ui_set_chat_mode(const char *chat_mode)
{
    if (sg_ui.ui.chat_mode_label == NULL || NULL == chat_mode) {
        return;
    }

    lv_label_set_text(sg_ui.ui.chat_mode_label, chat_mode);
}

void ui_set_status_bar_pad(int32_t value)
{
    if (sg_ui.ui.status_bar == NULL) {
        return;
    }

    lv_obj_set_style_pad_left(sg_ui.ui.status_bar, value, 0);
    lv_obj_set_style_pad_right(sg_ui.ui.status_bar, value, 0);
}

#endif
