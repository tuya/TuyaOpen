/**
 * @file ai_ui_chat_wechat.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_AI_CHAT_GUI_WECHAT) && (ENABLE_AI_CHAT_GUI_WECHAT == 1)
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
#include "tdl_display_manage.h"
#endif

#include "lvgl.h"
#include "lv_vendor.h"
#include "lv_port_disp.h"

#include "font_awesome_symbols.h"

#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"

#include "ai_ui_chat_wechat.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define MAX_MASSAGE_NUM           20

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    lv_style_t style_avatar;
    lv_style_t style_ai_bubble;
    lv_style_t style_user_bubble;

    lv_obj_t *container;
    lv_obj_t *status_bar;
    lv_obj_t *content;
    lv_obj_t *emotion_label;
    lv_obj_t *chat_message_label;
    lv_obj_t *status_label;
    lv_obj_t *network_label;
    lv_obj_t *notification_label;
    lv_obj_t *mute_label;
    lv_obj_t *mode_label;

    lv_obj_t *stream_msg_cont;
    lv_obj_t *stream_bubble;
    lv_obj_t *stream_label;
}AI_UI_WECHAT_T;

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
typedef struct {
    TDL_DISP_DEV_INFO_T    disp_info; 
    TDL_DISP_HANDLE_T      disp_handle;
    TDL_FB_MANAGE_HANDLE_T fb_manage;
    bool                   is_disp_start;
}AI_UI_DISP_CAMERA_T;
#endif

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_UI_WECHAT_T      sg_ui = {0};
static AI_UI_FONT_LIST_T   sg_font = {0};
static lv_timer_t         *sg_notification_tm = NULL;
static bool                sg_is_streaming = false;

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
static AI_UI_DISP_CAMERA_T sg_disp_camera = {0};
#endif
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

static void __ui_styles_init(void)
{
    lv_style_init(&sg_ui.style_avatar);
    lv_style_set_radius(&sg_ui.style_avatar, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&sg_ui.style_avatar, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_border_width(&sg_ui.style_avatar, 1);
    lv_style_set_border_color(&sg_ui.style_avatar, lv_palette_darken(LV_PALETTE_GREY, 2));

    lv_style_init(&sg_ui.style_ai_bubble);
    lv_style_set_bg_color(&sg_ui.style_ai_bubble, lv_color_white());
    lv_style_set_radius(&sg_ui.style_ai_bubble, 15);
    lv_style_set_pad_all(&sg_ui.style_ai_bubble, 12);
    lv_style_set_shadow_width(&sg_ui.style_ai_bubble, 12);
    lv_style_set_shadow_color(&sg_ui.style_ai_bubble, lv_color_hex(0xCCCCCC));

    lv_style_init(&sg_ui.style_user_bubble);
    lv_style_set_bg_color(&sg_ui.style_user_bubble, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_text_color(&sg_ui.style_user_bubble, lv_color_white());
    lv_style_set_radius(&sg_ui.style_user_bubble, 15);
    lv_style_set_pad_all(&sg_ui.style_user_bubble, 12);
    lv_style_set_shadow_width(&sg_ui.style_user_bubble, 12);
    lv_style_set_shadow_color(&sg_ui.style_user_bubble, lv_palette_darken(LV_PALETTE_GREEN, 2));
}

static void __ui_notification_timeout_cb(lv_timer_t *timer)
{
    lv_timer_del(sg_notification_tm);
    sg_notification_tm = NULL;

    lv_obj_add_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui.status_label, LV_OBJ_FLAG_HIDDEN);
    // Show emotion label when notification timeout
    lv_obj_clear_flag(sg_ui.emotion_label, LV_OBJ_FLAG_HIDDEN);
}

static OPERATE_RET __ui_init(void)
{
    __lvgl_init();

    lv_vendor_disp_lock();

    __ui_styles_init();
    __ui_font_init();

    lv_obj_t *screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    lv_obj_set_style_text_font(screen, sg_font.text, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(screen, LV_DIR_VER);

    // Container
    sg_ui.container = lv_obj_create(screen);
    lv_obj_set_size(sg_ui.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(sg_ui.container, 0, 0);
    lv_obj_set_style_border_width(sg_ui.container, 0, 0);
    lv_obj_set_style_pad_row(sg_ui.container, 0, 0);

    // Status bar
    sg_ui.status_bar = lv_obj_create(sg_ui.container);
    lv_obj_set_size(sg_ui.status_bar, LV_HOR_RES, 40);
    lv_obj_set_style_bg_color(sg_ui.status_bar, lv_palette_main(LV_PALETTE_GREEN), 0);

    // Mode label (leftmost)
    sg_ui.mode_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_style_text_font(sg_ui.mode_label, sg_font.text, 0);
    lv_label_set_text(sg_ui.mode_label, "");
    lv_obj_align(sg_ui.mode_label, LV_ALIGN_LEFT_MID, 0, 0);

    // Status label
    sg_ui.status_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.status_label, 1);
    lv_label_set_long_mode(sg_ui.status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_center(sg_ui.status_label);
    lv_label_set_text(sg_ui.status_label, INITIALIZING);

    // Emotion (left of status label)
    sg_ui.emotion_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_style_text_font(sg_ui.emotion_label, sg_font.icon, 0);
    lv_obj_align_to(sg_ui.emotion_label, sg_ui.status_label, LV_ALIGN_OUT_LEFT_MID, -5, 0);
    lv_label_set_text(sg_ui.emotion_label, FONT_AWESOME_AI_CHIP);

    // Notification label
    sg_ui.notification_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_flex_grow(sg_ui.notification_label, 1);
    lv_label_set_long_mode(sg_ui.status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_center(sg_ui.notification_label);
    lv_label_set_text(sg_ui.notification_label, "");
    lv_obj_add_flag(sg_ui.notification_label, LV_OBJ_FLAG_HIDDEN);

    // Network status (rightmost)
    sg_ui.network_label = lv_label_create(sg_ui.status_bar);
    lv_obj_set_style_text_font(sg_ui.network_label, sg_font.icon, 0);
    lv_obj_align(sg_ui.network_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // content
    sg_ui.content = lv_obj_create(sg_ui.container);
    lv_obj_set_size(sg_ui.content, LV_HOR_RES, LV_VER_RES - 40);
    lv_obj_set_flex_flow(sg_ui.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_ver(sg_ui.content, 8, 0);
    lv_obj_set_style_pad_hor(sg_ui.content, 10, 0);
    lv_obj_align(sg_ui.content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_move_background(sg_ui.content);

    lv_obj_set_scroll_dir(sg_ui.content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(sg_ui.content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(sg_ui.content, LV_OPA_TRANSP, 0);

    lv_vendor_disp_unlock();

    return OPRT_OK;
}

static void __ui_set_user_msg(char *text)
{
    if (sg_ui.content == NULL || text == NULL || strlen(text) == 0) {
        return;
    }

    lv_vendor_disp_lock();

    // Check if the number of messages exceeds the limit
    uint32_t child_count = lv_obj_get_child_cnt(sg_ui.content);
    if (child_count >= MAX_MASSAGE_NUM) {
        lv_obj_t *first_child = lv_obj_get_child(sg_ui.content, 0);
        if (first_child) {
            lv_obj_del(first_child);
        }
    }

    lv_obj_t *msg_cont = lv_obj_create(sg_ui.content);
    lv_obj_remove_style_all(msg_cont);
    lv_obj_set_size(msg_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(msg_cont, 6, 0);
    lv_obj_set_style_pad_column(msg_cont, 10, 0);

    lv_obj_t *avatar = lv_obj_create(msg_cont);
    lv_obj_set_style_text_font(avatar, sg_font.icon, 0);
    lv_obj_add_style(avatar, &sg_ui.style_avatar, 0);
    lv_obj_set_size(avatar, 40, 40);
    lv_obj_align(avatar, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t *icon = lv_label_create(avatar);
    lv_label_set_text(icon, FONT_AWESOME_USER);
    lv_obj_center(icon);

    lv_obj_t *bubble = lv_obj_create(msg_cont);
    lv_obj_set_width(bubble, LV_PCT(75));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_add_style(bubble, &sg_ui.style_user_bubble, 0);
    lv_obj_align_to(bubble, avatar, LV_ALIGN_OUT_LEFT_TOP, -10, 0);

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

    lv_obj_scroll_to_view_recursive(msg_cont, LV_ANIM_ON);
    lv_obj_update_layout(sg_ui.content);

    lv_vendor_disp_unlock();
}

static void __ui_set_ai_msg(char *text)
{
    if (sg_ui.content == NULL || text == NULL || strlen(text) == 0) {
        return;
    }

    lv_vendor_disp_lock();

    // Check if the number of messages exceeds the limit
    uint32_t child_count = lv_obj_get_child_cnt(sg_ui.content);
    if (child_count >= MAX_MASSAGE_NUM) {
        lv_obj_t *first_child = lv_obj_get_child(sg_ui.content, 0);
        if (first_child) {
            lv_obj_del(first_child);
        }
    }

    lv_obj_t *msg_cont = lv_obj_create(sg_ui.content);
    lv_obj_remove_style_all(msg_cont);
    lv_obj_set_size(msg_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(msg_cont, 6, 0);
    lv_obj_set_style_pad_column(msg_cont, 10, 0);

    lv_obj_t *avatar = lv_obj_create(msg_cont);
    lv_obj_set_style_text_font(avatar, sg_font.icon, 0);
    lv_obj_add_style(avatar, &sg_ui.style_avatar, 0);
    lv_obj_set_size(avatar, 40, 40);
    lv_obj_align(avatar, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *icon = lv_label_create(avatar);
    lv_label_set_text(icon, FONT_AWESOME_USER_ROBOT);
    lv_obj_center(icon);

    lv_obj_t *bubble = lv_obj_create(msg_cont);
    lv_obj_set_width(bubble, LV_PCT(75));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_add_style(bubble, &sg_ui.style_ai_bubble, 0);
    lv_obj_align_to(bubble, avatar, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);

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

    lv_obj_scroll_to_view_recursive(msg_cont, LV_ANIM_ON);
    lv_obj_update_layout(sg_ui.content);

    lv_vendor_disp_unlock();
}

static void __ui_set_ai_msg_stream_start(void)
{
    if (sg_ui.content == NULL) {
        return;
    }

    lv_vendor_disp_lock();

    // Check if the number of messages exceeds the limit
    uint32_t child_count = lv_obj_get_child_cnt(sg_ui.content);
    if (child_count >= MAX_MASSAGE_NUM) {
        lv_obj_t *first_child = lv_obj_get_child(sg_ui.content, 0);
        if (first_child) {
            lv_obj_del(first_child);
        }
    }

    sg_ui.stream_msg_cont = lv_obj_create(sg_ui.content);
    lv_obj_remove_style_all(sg_ui.stream_msg_cont);
    lv_obj_set_size(sg_ui.stream_msg_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(sg_ui.stream_msg_cont, 6, 0);
    lv_obj_set_style_pad_column(sg_ui.stream_msg_cont, 10, 0);

    lv_obj_t *avatar = lv_obj_create(sg_ui.stream_msg_cont);
    lv_obj_set_style_text_font(avatar, sg_font.icon, 0);
    lv_obj_add_style(avatar, &sg_ui.style_avatar, 0);
    lv_obj_set_size(avatar, 40, 40);
    lv_obj_align(avatar, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *icon = lv_label_create(avatar);
    lv_label_set_text(icon, FONT_AWESOME_USER_ROBOT);
    lv_obj_center(icon);

    sg_ui.stream_bubble = lv_obj_create(sg_ui.stream_msg_cont);
    lv_obj_set_width(sg_ui.stream_bubble, LV_PCT(75));
    lv_obj_set_height(sg_ui.stream_bubble, LV_SIZE_CONTENT);
    lv_obj_add_style(sg_ui.stream_bubble, &sg_ui.style_ai_bubble, 0);
    lv_obj_align_to(sg_ui.stream_bubble, avatar, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);
    lv_obj_set_scrollbar_mode(sg_ui.stream_bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(sg_ui.stream_bubble, LV_DIR_VER);

    lv_obj_t *text_cont = lv_obj_create(sg_ui.stream_bubble);
    lv_obj_remove_style_all(text_cont);
    lv_obj_set_size(text_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);

    sg_ui.stream_label = lv_label_create(text_cont);
    lv_label_set_text(sg_ui.stream_label, "");
    lv_obj_set_width(sg_ui.stream_label, LV_PCT(100));
    lv_label_set_long_mode(sg_ui.stream_label, LV_LABEL_LONG_WRAP);

    lv_vendor_disp_unlock();

    sg_is_streaming = true;
}

static void __ui_set_ai_msg_stream_data(char *text)
{
    if(false == sg_is_streaming) {
        return;
    }

    lv_vendor_disp_lock();

    lv_label_ins_text(sg_ui.stream_label, LV_LABEL_POS_LAST, text);

    lv_coord_t content_height = lv_obj_get_height(sg_ui.stream_msg_cont);
    lv_coord_t height = lv_obj_get_height(sg_ui.content);

    if (content_height > height) {
        lv_coord_t offset = 0;
        offset = lv_obj_get_scroll_bottom(sg_ui.content);
        if (offset > 0) {
            lv_obj_scroll_by_bounded(sg_ui.content, 0, -offset, LV_ANIM_OFF);
        }
    } else {
        lv_obj_scroll_to_view_recursive(sg_ui.stream_msg_cont, LV_ANIM_OFF);
    }

    lv_obj_update_layout(sg_ui.content);

    lv_vendor_disp_unlock();
}

static void __ui_set_ai_msg_stream_end(void)
{
    sg_is_streaming = false;
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
            PR_DEBUG("find emo:%s", emotion);
            break;
        }
    }
    
    lv_vendor_disp_lock();
    lv_obj_set_style_text_font(sg_ui.emotion_label, sg_font.emoji, 0);
    lv_label_set_text(sg_ui.emotion_label, emo_icon);
    // Re-align emotion label (left of status label)
    if (sg_ui.status_label != NULL) {
        lv_obj_align_to(sg_ui.emotion_label, sg_ui.status_label, LV_ALIGN_OUT_LEFT_MID, -5, 0);
    }
    lv_vendor_disp_unlock();
}

static void __ui_set_status(char *status)
{
    if (sg_ui.status_label == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.status_label, status);
    lv_obj_set_style_text_align(sg_ui.status_label, LV_TEXT_ALIGN_CENTER, 0);
    // Re-align emotion label after status label text change
    if (sg_ui.emotion_label != NULL) {
        lv_obj_align_to(sg_ui.emotion_label, sg_ui.status_label, LV_ALIGN_OUT_LEFT_MID, -5, 0);
    }
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
    // Hide emotion label when showing notification
    lv_obj_add_flag(sg_ui.emotion_label, LV_OBJ_FLAG_HIDDEN);
    
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
    // Re-align network label to rightmost position
    lv_obj_align(sg_ui.network_label, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_vendor_disp_unlock();
}

static void __ui_set_chat_mode(char *chat_mode)
{
    if (sg_ui.mode_label == NULL || chat_mode == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_ui.mode_label, chat_mode);
    // Re-align mode label to leftmost position
    lv_obj_align(sg_ui.mode_label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_vendor_disp_unlock();
}

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
static OPERATE_RET __disp_camera_start(uint16_t width, uint16_t height)
{
    OPERATE_RET rt = OPRT_OK;

    if(0 == width || 0 == height) {
        PR_ERR("invalid width or height");
        return OPRT_INVALID_PARM;
    }

    sg_disp_camera.disp_handle = tdl_disp_find_dev(DISPLAY_NAME);
    tdl_disp_dev_get_info(sg_disp_camera.disp_handle, &sg_disp_camera.disp_info);

    if(NULL == sg_disp_camera.fb_manage) {
        TUYA_CALL_ERR_RETURN(tdl_disp_fb_manage_init(&sg_disp_camera.fb_manage));

        for(uint8_t i=0; i<2; i++) {
            TUYA_CALL_ERR_RETURN(tdl_disp_fb_manage_add(sg_disp_camera.fb_manage, \
                                                        sg_disp_camera.disp_info.fmt, \
                                                        width, height));
        } 
    }

    /*disable lvgl display*/
    disp_disable_update(NULL);

    sg_disp_camera.is_disp_start = true;

    return rt;
}

OPERATE_RET __disp_camera_flush(uint8_t *data, uint16_t width, uint16_t height)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_DISP_FRAME_BUFF_T *convert_fb = NULL;

    if(false == sg_disp_camera.is_disp_start) {
        return OPRT_COM_ERROR;
    }

    convert_fb = tdl_disp_get_free_fb(sg_disp_camera.fb_manage);
    TUYA_CHECK_NULL_RETURN(convert_fb, OPRT_COM_ERROR);

    TUYA_CALL_ERR_LOG(tdl_disp_convert_yuv422_to_framebuffer(data,
                                                             width,
                                                             height, 
                                                             convert_fb));

    tdl_disp_dev_flush(sg_disp_camera.disp_handle, convert_fb);

    return rt;
}

OPERATE_RET __disp_camera_end(void)
{
    sg_disp_camera.is_disp_start = false;

    /*enable lvgl display*/
    disp_enable_update(NULL);

    // tdl_disp_fb_manage_release(&sg_disp_camera.fb_manage);

    PR_NOTICE("app display camera end success");

    return OPRT_OK;
}

#endif

OPERATE_RET ai_ui_chat_wechat_register(void)
{
    AI_UI_INTFS_T intfs;

    memset(&intfs, 0, sizeof(AI_UI_INTFS_T));

    intfs.disp_init                = __ui_init;
    intfs.disp_user_msg            = __ui_set_user_msg;
    intfs.disp_ai_msg              = __ui_set_ai_msg;   
    intfs.disp_ai_msg_stream_start = __ui_set_ai_msg_stream_start;
    intfs.disp_ai_msg_stream_data  = __ui_set_ai_msg_stream_data;
    intfs.disp_ai_msg_stream_end   = __ui_set_ai_msg_stream_end;
    intfs.disp_emotion             = __ui_set_emotion;
    intfs.disp_ai_mode_state       = __ui_set_status;
    intfs.disp_notification        = __ui_set_notification;
    intfs.disp_wifi_state          = __ui_set_network;
    intfs.disp_ai_chat_mode        = __ui_set_chat_mode;

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    intfs.disp_camera_start        = __disp_camera_start;
    intfs.disp_camera_flush        = __disp_camera_flush;
    intfs.disp_camera_end          = __disp_camera_end;
#endif

    return ai_ui_register(&intfs);
}

#endif