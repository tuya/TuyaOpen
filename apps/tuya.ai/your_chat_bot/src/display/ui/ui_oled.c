/**
 * @file ui_oled_init.c
 * @brief ui_oled_init module is used to
 * @version 0.1
 * @date 2025-05-12
 */

#include "tuya_cloud_types.h"

#if defined(ENABLE_GUI_OLED) && (ENABLE_GUI_OLED == 1)

#include "ui_display.h"

#include "font_awesome_symbols.h"
#include "lvgl.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    lv_obj_t *container;
    lv_obj_t *status_bar;
    lv_obj_t *content;
    lv_obj_t *emotion_label;
    lv_obj_t *side_bar;
    lv_obj_t *chat_message_label;
    lv_obj_t *status_label;
    lv_obj_t *network_label;
    lv_obj_t *notification_label;
    lv_obj_t *mute_label;
    lv_obj_t *content_left;
    lv_obj_t *content_right;
    lv_anim_t msg_anim;
} APP_UI_T;

typedef struct {
    APP_UI_T ui;

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

static int ui_init_128X32(UI_FONT_T *ui_font)
{
    // Font init
    __ui_font_init(ui_font);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, sg_ui.font.text, 0);

    // Container
    sg_ui.ui.container = lv_obj_create(screen);
    lv_obj_set_size(sg_ui.ui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(sg_ui.ui.container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(sg_ui.ui.container, 0, 0);
    lv_obj_set_style_border_width(sg_ui.ui.container, 0, 0);
    lv_obj_set_style_pad_column(sg_ui.ui.container, 0, 0);

    // Content
    sg_ui.ui.content = lv_obj_create(sg_ui.ui.container);
    lv_obj_set_size(sg_ui.ui.content, 32, 32);
    lv_obj_set_style_pad_all(sg_ui.ui.content, 0, 0);
    lv_obj_set_style_border_width(sg_ui.ui.content, 0, 0);
    lv_obj_set_style_radius(sg_ui.ui.content, 0, 0);

    sg_ui.ui.emotion_label = lv_label_create(sg_ui.ui.content);
    lv_obj_set_style_text_font(sg_ui.ui.emotion_label, sg_ui.font.icon, 0);
    lv_label_set_text(sg_ui.ui.emotion_label, FONT_AWESOME_AI_CHIP);
    lv_obj_center(sg_ui.ui.emotion_label);

    /* Right side */
    sg_ui.ui.side_bar = lv_obj_create(sg_ui.ui.container);
    lv_obj_set_size(sg_ui.ui.side_bar, LV_HOR_RES - 32, 32);
    lv_obj_set_flex_flow(sg_ui.ui.side_bar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sg_ui.ui.side_bar, 0, 0);
    lv_obj_set_style_border_width(sg_ui.ui.side_bar, 0, 0);
    lv_obj_set_style_radius(sg_ui.ui.side_bar, 0, 0);
    lv_obj_set_style_pad_row(sg_ui.ui.side_bar, 0, 0);

    /* Status bar */
    sg_ui.ui.status_bar = lv_obj_create(sg_ui.ui.side_bar);
    lv_obj_set_size(sg_ui.ui.status_bar, LV_HOR_RES - 32, 16);
    lv_obj_set_style_radius(sg_ui.ui.status_bar, 0, 0);
    lv_obj_set_flex_flow(sg_ui.ui.status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(sg_ui.ui.status_bar, 0, 0);
    lv_obj_set_style_border_width(sg_ui.ui.status_bar, 0, 0);
    lv_obj_set_style_pad_column(sg_ui.ui.status_bar, 0, 0);

    sg_ui.ui.status_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.ui.status_label, 1);
    lv_obj_set_style_pad_left(sg_ui.ui.status_label, 2, 0);
    lv_label_set_text(sg_ui.ui.status_label, INITIALIZING);

    sg_ui.ui.notification_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.ui.notification_label, 1);
    lv_obj_set_style_pad_left(sg_ui.ui.notification_label, 2, 0);
    lv_label_set_text(sg_ui.ui.notification_label, "");
    lv_obj_add_flag(sg_ui.ui.notification_label, LV_OBJ_FLAG_HIDDEN);

    sg_ui.ui.mute_label = lv_label_create(sg_ui.ui.status_bar);
    lv_label_set_text(sg_ui.ui.mute_label, "");
    lv_obj_set_style_text_font(sg_ui.ui.mute_label, sg_ui.font.icon, 0);

    sg_ui.ui.network_label = lv_label_create(sg_ui.ui.status_bar);
    lv_label_set_text(sg_ui.ui.network_label, "");
    lv_obj_set_style_text_font(sg_ui.ui.network_label, sg_ui.font.icon, 0);
    sg_ui.ui.chat_message_label = lv_label_create(sg_ui.ui.side_bar);
    lv_obj_set_size(sg_ui.ui.chat_message_label, LV_HOR_RES - 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(sg_ui.ui.chat_message_label, 2, 0);
    lv_label_set_long_mode(sg_ui.ui.chat_message_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(sg_ui.ui.chat_message_label, "");

    lv_anim_init(&sg_ui.ui.msg_anim);
    lv_anim_set_delay(&sg_ui.ui.msg_anim, 1000);
    lv_anim_set_repeat_count(&sg_ui.ui.msg_anim, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(sg_ui.ui.chat_message_label, &sg_ui.ui.msg_anim, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(sg_ui.ui.chat_message_label, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);

    return 0;
}

static int ui_init_128X64(UI_FONT_T *ui_font)
{
    // Font init
    __ui_font_init(ui_font);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, sg_ui.font.text, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    sg_ui.ui.container = lv_obj_create(screen);
    lv_obj_set_size(sg_ui.ui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(sg_ui.ui.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sg_ui.ui.container, 0, 0);
    lv_obj_set_style_border_width(sg_ui.ui.container, 0, 0);
    lv_obj_set_style_pad_row(sg_ui.ui.container, 0, 0);

    /* Status bar */
    sg_ui.ui.status_bar = lv_obj_create(sg_ui.ui.container);
    lv_obj_set_size(sg_ui.ui.status_bar, LV_HOR_RES, 16);
    lv_obj_set_style_border_width(sg_ui.ui.status_bar, 0, 0);
    lv_obj_set_style_pad_all(sg_ui.ui.status_bar, 0, 0);
    lv_obj_set_style_radius(sg_ui.ui.status_bar, 0, 0);

    /* Content */
    sg_ui.ui.content = lv_obj_create(sg_ui.ui.container);
    lv_obj_set_scrollbar_mode(sg_ui.ui.content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(sg_ui.ui.content, 0, 0);
    lv_obj_set_style_pad_all(sg_ui.ui.content, 0, 0);
    lv_obj_set_width(sg_ui.ui.content, LV_HOR_RES);
    lv_obj_set_flex_grow(sg_ui.ui.content, 1);
    lv_obj_set_flex_flow(sg_ui.ui.content, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(sg_ui.ui.content, LV_FLEX_ALIGN_CENTER, 0);

    sg_ui.ui.content_left = lv_obj_create(sg_ui.ui.content);
    lv_obj_set_size(sg_ui.ui.content_left, 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(sg_ui.ui.content_left, 0, 0);
    lv_obj_set_style_border_width(sg_ui.ui.content_left, 0, 0);

    sg_ui.ui.emotion_label = lv_label_create(sg_ui.ui.content_left);
    lv_obj_set_style_text_font(sg_ui.ui.emotion_label, sg_ui.font.emoji, 0);
    lv_label_set_text(sg_ui.ui.emotion_label, FONT_AWESOME_AI_CHIP);
    lv_obj_center(sg_ui.ui.emotion_label);
    lv_obj_set_style_pad_top(sg_ui.ui.emotion_label, 8, 0);

    sg_ui.ui.content_right = lv_obj_create(sg_ui.ui.content);
    lv_obj_set_size(sg_ui.ui.content_right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(sg_ui.ui.content_right, 0, 0);
    lv_obj_set_style_border_width(sg_ui.ui.content_right, 0, 0);
    lv_obj_set_flex_grow(sg_ui.ui.content_right, 1);
    lv_obj_add_flag(sg_ui.ui.content_right, LV_OBJ_FLAG_HIDDEN);

    sg_ui.ui.chat_message_label = lv_label_create(sg_ui.ui.content_right);
    lv_label_set_text(sg_ui.ui.chat_message_label, "");
    lv_label_set_long_mode(sg_ui.ui.chat_message_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(sg_ui.ui.chat_message_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(sg_ui.ui.chat_message_label, LV_HOR_RES - 32);
    lv_obj_set_style_pad_top(sg_ui.ui.chat_message_label, 14, 0);

    lv_anim_init(&sg_ui.ui.msg_anim);
    lv_anim_set_delay(&sg_ui.ui.msg_anim, 1000);
    lv_anim_set_repeat_count(&sg_ui.ui.msg_anim, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(sg_ui.ui.chat_message_label, &sg_ui.ui.msg_anim, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(sg_ui.ui.chat_message_label, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);

    /* Status bar */
    lv_obj_set_flex_flow(sg_ui.ui.status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(sg_ui.ui.status_bar, 0, 0);
    lv_obj_set_style_border_width(sg_ui.ui.status_bar, 0, 0);
    lv_obj_set_style_pad_column(sg_ui.ui.status_bar, 0, 0);

    sg_ui.ui.network_label = lv_label_create(sg_ui.ui.status_bar);
    lv_label_set_text(sg_ui.ui.network_label, "");
    lv_obj_set_style_text_font(sg_ui.ui.network_label, sg_ui.font.icon, 0);

    sg_ui.ui.notification_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.ui.notification_label, 1);
    lv_obj_set_style_text_align(sg_ui.ui.notification_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(sg_ui.ui.notification_label, "");
    lv_obj_add_flag(sg_ui.ui.notification_label, LV_OBJ_FLAG_HIDDEN);

    sg_ui.ui.status_label = lv_label_create(sg_ui.ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.ui.status_label, 1);
    lv_label_set_text(sg_ui.ui.status_label, INITIALIZING);
    lv_obj_set_style_text_align(sg_ui.ui.status_label, LV_TEXT_ALIGN_CENTER, 0);

    return 0;
}

int ui_init(UI_FONT_T *ui_font)
{
    if (LV_HOR_RES == 128 && LV_VER_RES == 32) {
        return ui_init_128X32(ui_font);
    } else if (LV_HOR_RES == 128 && LV_VER_RES == 64) {
        return ui_init_128X64(ui_font);
    } else {
        return -1; // Unsupported resolution
    }
}

void ui_set_user_msg(const char *text)
{
    if (sg_ui.ui.chat_message_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.chat_message_label, text);
}

void ui_set_assistant_msg(const char *text)
{
    if (sg_ui.ui.chat_message_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.chat_message_label, text);
}

void ui_set_system_msg(const char *text)
{
    if (sg_ui.ui.chat_message_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.chat_message_label, text);
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
    lv_obj_clear_flag(sg_ui.ui.status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_ui.ui.notification_label, LV_OBJ_FLAG_HIDDEN);
}

void ui_set_notification(const char *notification)
{
    if (sg_ui.ui.notification_label == NULL) {
        return;
    }

    lv_label_set_text(sg_ui.ui.notification_label, notification);
    lv_obj_clear_flag(sg_ui.ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_ui.ui.status_label, LV_OBJ_FLAG_HIDDEN);

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
    ;
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
