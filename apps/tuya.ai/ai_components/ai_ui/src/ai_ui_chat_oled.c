/**
 * @file ai_ui_chat_oled.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_AI_CHAT_GUI_OLED) && (ENABLE_AI_CHAT_GUI_OLED == 1)
#include "lvgl.h"
#include "lv_vendor.h"

#include "font_awesome_symbols.h"

#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"

#include "ai_ui_chat_oled.h"

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
} AI_UI_OLED_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_UI_OLED_T      sg_ui;
static AI_UI_FONT_LIST_T sg_font = {0};
static lv_timer_t       *sg_notification_tm = NULL;
static bool              sg_is_streaming = false;

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

static void __ui_notification_timeout_cb(lv_timer_t *timer)
{
    lv_timer_del(sg_notification_tm);
    sg_notification_tm = NULL;

    lv_obj_add_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.status_label, LV_OBJ_FLAG_HIDDEN);
}

#if defined(AI_CHAT_GUI_OLED_SIZE_128_64) && (AI_CHAT_GUI_OLED_SIZE_128_64 == 1)
static void __ui_init_128X64(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, sg_font.text, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    sg_ui.container = lv_obj_create(screen);
    lv_obj_set_size(sg_ui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(sg_ui.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sg_ui.container, 0, 0);
    lv_obj_set_style_border_width(sg_ui.container, 0, 0);
    lv_obj_set_style_pad_row(sg_ui.container, 0, 0);

    /* Status bar */
    sg_ui.status_bar = lv_obj_create(sg_ui.container);
    lv_obj_set_size(sg_ui.status_bar, LV_HOR_RES, 16);
    lv_obj_set_style_border_width(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_pad_all(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_radius(sg_ui.status_bar, 0, 0);

    /* Content */
    sg_ui.content = lv_obj_create(sg_ui.container);
    lv_obj_set_scrollbar_mode(sg_ui.content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(sg_ui.content, 0, 0);
    lv_obj_set_style_pad_all(sg_ui.content, 0, 0);
    lv_obj_set_width(sg_ui.content, LV_HOR_RES);
    lv_obj_set_flex_grow(sg_ui.content, 1);
    lv_obj_set_flex_flow(sg_ui.content, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(sg_ui.content, LV_FLEX_ALIGN_CENTER, 0);

    sg_ui.content_left = lv_obj_create(sg_ui.content);
    lv_obj_set_size(sg_ui.content_left, 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(sg_ui.content_left, 0, 0);
    lv_obj_set_style_border_width(sg_ui.content_left, 0, 0);

    sg_ui.emotion_label = lv_label_create(sg_ui.content_left);
    lv_obj_set_style_text_font(sg_ui.emotion_label, sg_font.emoji, 0);
    lv_label_set_text(sg_ui.emotion_label, FONT_AWESOME_AI_CHIP);
    lv_obj_center(sg_ui.emotion_label);
    lv_obj_set_style_pad_top(sg_ui.emotion_label, 8, 0);

    sg_ui.content_right = lv_obj_create(sg_ui.content);
    lv_obj_set_size(sg_ui.content_right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(sg_ui.content_right, 0, 0);
    lv_obj_set_style_border_width(sg_ui.content_right, 0, 0);
    lv_obj_set_flex_grow(sg_ui.content_right, 1);
    lv_obj_add_flag(sg_ui.content_right, LV_OBJ_FLAG_HIDDEN);

    sg_ui.chat_message_label = lv_label_create(sg_ui.content_right);
    lv_label_set_text(sg_ui.chat_message_label, "");
    lv_label_set_long_mode(sg_ui.chat_message_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(sg_ui.chat_message_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(sg_ui.chat_message_label, LV_HOR_RES - 32);
    lv_obj_set_style_pad_top(sg_ui.chat_message_label, 14, 0);

    lv_anim_init(&sg_ui.msg_anim);
    lv_anim_set_delay(&sg_ui.msg_anim, 1000);
    lv_anim_set_repeat_count(&sg_ui.msg_anim, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(sg_ui.chat_message_label, &sg_ui.msg_anim, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(sg_ui.chat_message_label, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);

    /* Status bar */
    lv_obj_set_flex_flow(sg_ui.status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_border_width(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_pad_column(sg_ui.status_bar, 0, 0);

    sg_ui.network_label = lv_label_create(sg_ui.status_bar);
    lv_label_set_text(sg_ui.network_label, "");
    lv_obj_set_style_text_font(sg_ui.network_label, sg_font.icon, 0);

    sg_ui.notification_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.notification_label, 1);
    lv_obj_set_style_text_align(sg_ui.notification_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(sg_ui.notification_label, "");
    lv_obj_add_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);

    sg_ui.status_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.status_label, 1);
    lv_label_set_text(sg_ui.status_label, INITIALIZING);
    lv_obj_set_style_text_align(sg_ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
}
#elif defined(AI_CHAT_GUI_OLED_SIZE_128_32) && (AI_CHAT_GUI_OLED_SIZE_128_32 == 1)
static void __ui_init_128X32(void)
{
    lv_obj_t *screen = lv_screen_active();
    if(NULL == screen) {
        PR_ERR("screen is null");
        return;
    }

    if(NULL == sg_font.text) {
        PR_ERR("sg_font.text is null");
        return;
    }

    lv_obj_set_style_text_font(screen, sg_font.text, 0);

    // Container
    sg_ui.container = lv_obj_create(screen);
    lv_obj_set_size(sg_ui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(sg_ui.container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(sg_ui.container, 0, 0);
    lv_obj_set_style_border_width(sg_ui.container, 0, 0);
    lv_obj_set_style_pad_column(sg_ui.container, 0, 0);

    // Content
    sg_ui.content = lv_obj_create(sg_ui.container);
    lv_obj_set_size(sg_ui.content, 32, 32);
    lv_obj_set_style_pad_all(sg_ui.content, 0, 0);
    lv_obj_set_style_border_width(sg_ui.content, 0, 0);
    lv_obj_set_style_radius(sg_ui.content, 0, 0);

    sg_ui.emotion_label = lv_label_create(sg_ui.content);
    lv_obj_set_style_text_font(sg_ui.emotion_label, sg_font.icon, 0);
    lv_label_set_text(sg_ui.emotion_label, FONT_AWESOME_AI_CHIP);
    lv_obj_center(sg_ui.emotion_label);

    /* Right side */
    sg_ui.side_bar = lv_obj_create(sg_ui.container);
    lv_obj_set_size(sg_ui.side_bar, LV_HOR_RES - 32, 32);
    lv_obj_set_flex_flow(sg_ui.side_bar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sg_ui.side_bar, 0, 0);
    lv_obj_set_style_border_width(sg_ui.side_bar, 0, 0);
    lv_obj_set_style_radius(sg_ui.side_bar, 0, 0);
    lv_obj_set_style_pad_row(sg_ui.side_bar, 0, 0);

    /* Status bar */
    sg_ui.status_bar = lv_obj_create(sg_ui.side_bar);
    lv_obj_set_size(sg_ui.status_bar, LV_HOR_RES - 32, 16);
    lv_obj_set_style_radius(sg_ui.status_bar, 0, 0);
    lv_obj_set_flex_flow(sg_ui.status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_border_width(sg_ui.status_bar, 0, 0);
    lv_obj_set_style_pad_column(sg_ui.status_bar, 0, 0);

    sg_ui.status_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.status_label, 1);
    lv_obj_set_style_pad_left(sg_ui.status_label, 2, 0);
    lv_label_set_text(sg_ui.status_label, INITIALIZING);

    sg_ui.notification_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.notification_label, 1);
    lv_obj_set_style_pad_left(sg_ui.notification_label, 2, 0);
    lv_label_set_text(sg_ui.notification_label, "");
    lv_obj_add_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);

    sg_ui.mute_label = lv_label_create(sg_ui.status_bar);
    lv_label_set_text(sg_ui.mute_label, "");
    lv_obj_set_style_text_font(sg_ui.mute_label, sg_font.icon, 0);

    sg_ui.network_label = lv_label_create(sg_ui.status_bar);
    lv_label_set_text(sg_ui.network_label, "");
    lv_obj_set_style_text_font(sg_ui.network_label, sg_font.icon, 0);
    sg_ui.chat_message_label = lv_label_create(sg_ui.side_bar);
    lv_obj_set_size(sg_ui.chat_message_label, LV_HOR_RES - 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(sg_ui.chat_message_label, 2, 0);
    lv_label_set_long_mode(sg_ui.chat_message_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(sg_ui.chat_message_label, "");

    lv_anim_init(&sg_ui.msg_anim);
    lv_anim_set_delay(&sg_ui.msg_anim, 1000);
    lv_anim_set_repeat_count(&sg_ui.msg_anim, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(sg_ui.chat_message_label, &sg_ui.msg_anim, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(sg_ui.chat_message_label, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);
}
#endif

static OPERATE_RET __ui_init(void)
{
    __lvgl_init();

    lv_vendor_disp_lock();

    __ui_font_init();

#if defined(AI_CHAT_GUI_OLED_SIZE_128_64) && (AI_CHAT_GUI_OLED_SIZE_128_64 == 1)
    __ui_init_128X64();
#elif defined(AI_CHAT_GUI_OLED_SIZE_128_32) && (AI_CHAT_GUI_OLED_SIZE_128_32 == 1)
    __ui_init_128X32();
#else   
    #error "Please define OLED size in ai_ui_config.h"
#endif

    lv_vendor_disp_unlock();

    return 0;
}

void __ui_set_user_msg(char *text)
{
    if (sg_ui.chat_message_label == NULL) {
        return;
    }
    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_message_label, text);
    lv_vendor_disp_unlock();
}

void __ui_set_ai_msg(char *text)
{
    if (sg_ui.chat_message_label == NULL) {
        return;
    }
    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_message_label, text);
    lv_vendor_disp_unlock();
}

static void __ui_set_ai_msg_stream_start(void)
{
    if (sg_ui.chat_message_label == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.chat_message_label, "");
    lv_vendor_disp_unlock();

    sg_is_streaming = true;
}

static void __ui_set_ai_msg_stream_data(char *text)
{
    if (sg_ui.chat_message_label == NULL || !sg_is_streaming) {
        return;
    }

    lv_vendor_disp_lock(); 
    // lv_label_ins_text(sg_ui.chat_message_label, LV_LABEL_POS_LAST, text);
    lv_label_set_text(sg_ui.chat_message_label, text);
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
    lv_obj_clear_flag(sg_ui.status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_vendor_disp_unlock();
}

static void __ui_set_notification(char *notification)
{
    if (sg_ui.notification_label == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.notification_label, notification);
    lv_obj_clear_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_ui.status_label, LV_OBJ_FLAG_HIDDEN);

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

OPERATE_RET ai_ui_chat_oled_register(void)
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

    return ai_ui_register(&intfs);
}
#endif