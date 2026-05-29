/**
 * @file ai_ui_wechat_chat.c
 * @brief WeChat-style chat page — message list, streaming, link display, image display.
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_AI_CHAT_GUI_WECHAT) && (ENABLE_AI_CHAT_GUI_WECHAT == 1)

#include "lvgl.h"
#include "lv_vendor.h"

#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"
#include "font_awesome_symbols.h"
#include "ai_ui_wechat_common.h"
#include "tal_image.h"
#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
#include "image_album.h"
#endif

#ifndef ALBUM_FILENAME_MAX_LEN
#define ALBUM_FILENAME_MAX_LEN  64
#endif

LV_IMG_DECLARE(icon_ai_icon);
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
LV_IMG_DECLARE(icon_camera_app);
#endif
#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
LV_IMG_DECLARE(icon_photo_app);
LV_IMG_DECLARE(icon_add_img);
#endif
#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
LV_IMG_DECLARE(icon_printer_app);
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define MAX_MESSAGE_NUM           20
#define ATTACH_THUMB_SIZE         48
#define ATTACH_BAR_HEIGHT         56
#define MAX_ATTACH_NUM            4
#define PLUS_BTN_SIZE             40
#define POPUP_WIDTH               120
#define POPUP_ITEM_H              36
#define POPUP_ICON_SIZE           24


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    lv_style_t style_avatar;
    lv_style_t style_ai_bubble;
    lv_style_t style_user_bubble;
    lv_style_t style_link;

    lv_obj_t *content;
    lv_obj_t *picture;
    lv_obj_t *picture_canvas;

    lv_obj_t *stream_msg_cont;
    lv_obj_t *stream_bubble;
    lv_obj_t *stream_label;
    bool      is_streaming;

    lv_obj_t *attach_bar;
    uint8_t  *attach_bufs[MAX_ATTACH_NUM];
    uint8_t   attach_count;

    lv_obj_t *plus_btn;
    lv_obj_t *popup_menu;

    char      cur_img_name[ALBUM_FILENAME_MAX_LEN + 1];

    lv_obj_t *picture_action_bar;
    lv_obj_t *picture_print_btn;
    lv_obj_t *picture_attach_btn;
#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    lv_obj_t   *picture_print_overlay;
    lv_obj_t   *picture_print_status_label;
    lv_obj_t   *picture_print_btn_row;
    lv_timer_t *picture_print_result_tm;
#endif
} AI_UI_WECHAT_CHAT_T;


/* ── link click event callback ── */

typedef struct {
    AI_UI_CHAT_LINK_CB cb;
    void              *cb_arg;
    uint32_t           len;
} UI_LINK_CB_DATA_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_UI_WECHAT_CHAT_T sg_chat = {0};
static uint8_t *sg_picture_buffer = NULL;
static lv_timer_t *sg_image_auto_return_tm = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/


/**
 * @brief Delete the oldest message when message count exceeds limit.
 */
static void __chat_check_msg_limit(void)
{
    uint32_t child_count = lv_obj_get_child_cnt(sg_chat.content);
    if (child_count >= MAX_MESSAGE_NUM) {
        lv_obj_t *first_child = lv_obj_get_child(sg_chat.content, 0);
        if (first_child) {
            lv_obj_del(first_child);
        }
    }
}

/**
 * @brief Create a message container with avatar on the AI side (left).
 *
 * @param parent Parent object.
 * @param text   Text to display in the bubble.
 * @return Pointer to the label inside the bubble.
 */
static lv_obj_t *__create_ai_msg_label(lv_obj_t *parent, char *text)
{
    lv_obj_t *msg_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(msg_cont);
    lv_obj_set_size(msg_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(msg_cont, 6, 0);
    lv_obj_set_style_pad_column(msg_cont, 10, 0);

    lv_obj_t *avatar = lv_obj_create(msg_cont);
    lv_obj_add_style(avatar, &sg_chat.style_avatar, 0);
    lv_obj_set_size(avatar, 40, 40);
    lv_obj_align(avatar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(avatar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(avatar, 0, 0);
    lv_obj_clear_flag(avatar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = lv_img_create(avatar);
    lv_img_set_src(icon, &icon_ai_icon);
    lv_obj_center(icon);

    lv_obj_t *bubble = lv_obj_create(msg_cont);
    lv_obj_set_width(bubble, LV_PCT(75));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_add_style(bubble, &sg_chat.style_ai_bubble, 0);
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

    return label;
}

/**
 * @brief Create a message container with avatar on the user side (right).
 *
 * @param parent Parent object.
 * @param text   Text to display in the bubble.
 * @return Pointer to the label inside the bubble.
 */
static lv_obj_t *__create_user_msg_label(lv_obj_t *parent, char *text)
{
    const lv_font_t *icon_font = ai_ui_get_icon_font();

    lv_obj_t *msg_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(msg_cont);
    lv_obj_set_size(msg_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(msg_cont, 6, 0);
    lv_obj_set_style_pad_column(msg_cont, 10, 0);

    lv_obj_t *avatar = lv_obj_create(msg_cont);
    lv_obj_set_style_text_font(avatar, icon_font, 0);
    lv_obj_add_style(avatar, &sg_chat.style_avatar, 0);
    lv_obj_set_size(avatar, 40, 40);
    lv_obj_align(avatar, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t *icon = lv_label_create(avatar);
    lv_label_set_text(icon, FONT_AWESOME_USER);
    lv_obj_center(icon);

    lv_obj_t *bubble = lv_obj_create(msg_cont);
    lv_obj_set_width(bubble, LV_PCT(75));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_add_style(bubble, &sg_chat.style_user_bubble, 0);
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

    return label;
}

/* ── picture view event callbacks ── */

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
static void __picture_print_result_timeout_cb(lv_timer_t *timer)
{
    (void)timer;
    lv_timer_del(sg_chat.picture_print_result_tm);
    sg_chat.picture_print_result_tm = NULL;
    if (sg_chat.picture_print_overlay != NULL) {
        lv_obj_add_flag(sg_chat.picture_print_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (sg_chat.picture_print_status_label != NULL) {
        lv_label_set_text(sg_chat.picture_print_status_label, PRINT_IMAGE);
    }
    if (sg_chat.picture_print_btn_row != NULL) {
        lv_obj_clear_flag(sg_chat.picture_print_btn_row, LV_OBJ_FLAG_HIDDEN);
    }
}

static void __picture_print_cancel_btn_cb(lv_event_t *e)
{
    lv_event_stop_bubbling(e);
    lv_obj_add_flag(sg_chat.picture_print_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void __picture_print_confirm_btn_cb(lv_event_t *e)
{
    lv_event_stop_bubbling(e);
    lv_label_set_text(sg_chat.picture_print_status_label, PRINTING);
    lv_obj_add_flag(sg_chat.picture_print_btn_row, LV_OBJ_FLAG_HIDDEN);
    ai_ui_notify_action(AI_UI_ACT_PRINT_IMG,
                        (uint8_t *)sg_chat.cur_img_name,
                        (uint32_t)strlen(sg_chat.cur_img_name));
}

static void __picture_disp_print_result(bool ok)
{
    lv_vendor_disp_lock();
    /* The print overlay/labels are created lazily when the user first opens
     * the picture page. If a print-result message arrives before that
     * (e.g. printing was triggered from another path), the labels may be
     * NULL. Skip the UI update silently in that case to avoid a NULL deref
     * crash inside lv_label_set_text(). */
    if (sg_chat.picture_print_status_label == NULL) {
        PR_NOTICE("picture: print result(%d) ignored, overlay not ready", ok);
        lv_vendor_disp_unlock();
        return;
    }
    lv_label_set_text(sg_chat.picture_print_status_label,
                      ok ? PRINT_SUCCESS : PRINT_FAILED);
    if (sg_chat.picture_print_result_tm == NULL) {
        sg_chat.picture_print_result_tm =
            lv_timer_create(__picture_print_result_timeout_cb, 2000, NULL);
    } else {
        lv_timer_reset(sg_chat.picture_print_result_tm);
    }
    lv_vendor_disp_unlock();
}

static void __picture_print_btn_cb(lv_event_t *e)
{
    PR_NOTICE("picture: print btn clicked, cur_img='%s'", sg_chat.cur_img_name);
    lv_event_stop_bubbling(e);
    if (sg_chat.cur_img_name[0] == '\0') {
        PR_NOTICE("picture: print btn — no image name, abort");
        return;
    }
    lv_obj_clear_flag(sg_chat.picture_print_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(sg_chat.picture_print_overlay);
}
#endif

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
static void __picture_attach_btn_cb(lv_event_t *e)
{
    PR_NOTICE("picture: attach btn clicked, cur_img='%s'", sg_chat.cur_img_name);
    lv_event_stop_bubbling(e);
    if (sg_chat.cur_img_name[0] == '\0') {
        PR_NOTICE("picture: attach btn — no image name, abort");
        return;
    }
    ai_ui_notify_action(AI_UI_ACT_ADD_IMG_ATTACH,
                        (uint8_t *)sg_chat.cur_img_name,
                        (uint32_t)strlen(sg_chat.cur_img_name));
    /* Return to chat so the attach bar update is visible */
    lv_obj_add_flag(sg_chat.picture, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_chat.content, LV_OBJ_FLAG_HIDDEN);
#if defined(ENABLE_LVGL_TP) && (ENABLE_LVGL_TP == 1)
    lv_obj_clear_flag(sg_chat.plus_btn, LV_OBJ_FLAG_HIDDEN);
#endif
    sg_chat.cur_img_name[0] = '\0';
}
#endif

/**
 * @brief Click picture canvas — hide picture view, show chat content.
 */
static void __do_return_chat_content(void)
{
    lv_obj_add_flag(sg_chat.picture, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_chat.content, LV_OBJ_FLAG_HIDDEN);
#if defined(ENABLE_LVGL_TP) && (ENABLE_LVGL_TP == 1)
    lv_obj_clear_flag(sg_chat.plus_btn, LV_OBJ_FLAG_HIDDEN);
#endif
    sg_chat.cur_img_name[0] = '\0';
}

static void __image_auto_return_cb(lv_timer_t *timer)
{
    lv_timer_del(timer);
    sg_image_auto_return_tm = NULL;
    __do_return_chat_content();
}

static void __return_chat_content_event_cb(lv_event_t *e)
{
    PR_NOTICE("picture: return_chat triggered, target=%p picture=%p",
              lv_event_get_target(e), sg_chat.picture);
    if (sg_image_auto_return_tm != NULL) {
        lv_timer_del(sg_image_auto_return_tm);
        sg_image_auto_return_tm = NULL;
    }
    __do_return_chat_content();
}

/**
 * @brief Generic link click handler — invokes the stored callback with argument.
 */
static void __link_click_event_cb(lv_event_t *e)
{
    UI_LINK_CB_DATA_T *data = (UI_LINK_CB_DATA_T *)lv_event_get_user_data(e);
    if (data && data->cb) {
        data->cb(data->cb_arg);
    }
}

/**
 * @brief Free link callback data when the label object is deleted.
 */
static void __link_delete_event_cb(lv_event_t *e)
{
    UI_LINK_CB_DATA_T *data = (UI_LINK_CB_DATA_T *)lv_event_get_user_data(e);
    if (data) {
        /* Free the copied cb_arg (allocated when len > 0 in __ui_disp_link) */
        if (data->len > 0 && data->cb_arg) {
            Free(data->cb_arg);
        }
        Free(data);
    }
}

/* ── "+" button and popup menu callbacks ── */

static void __popup_dismiss(void)
{
#if defined(ENABLE_LVGL_TP) && (ENABLE_LVGL_TP == 1)
    if (!lv_obj_has_flag(sg_chat.popup_menu, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(sg_chat.popup_menu, LV_OBJ_FLAG_HIDDEN);
    }
#endif
}

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
static void __popup_camera_cb(lv_event_t *e)
{
    (void)e;
    __popup_dismiss();
    ai_ui_notify_action(AI_UI_ACT_OPEN_CAMERA, NULL, 0);
}
#endif

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
static void __popup_album_cb(lv_event_t *e)
{
    (void)e;
    __popup_dismiss();
    ai_ui_notify_action(AI_UI_ACT_OPEN_ALBUM, NULL, 0);
}

static void __popup_add_img_cb(lv_event_t *e)
{
    (void)e;
    __popup_dismiss();
    ai_ui_notify_action(AI_UI_ACT_OPEN_IMG_ATTACH_LIST, NULL, 0);
}
#endif


/**
 * @brief Dismiss popup when user clicks on the chat content area.
 */
static void __content_click_cb(lv_event_t *e)
{
    (void)e;
    __popup_dismiss();
}

#if defined(ENABLE_LVGL_TP) && (ENABLE_LVGL_TP == 1)
static void __plus_btn_click_cb(lv_event_t *e)
{
    (void)e;
    PR_DEBUG("plus_click: enter");
    if (lv_obj_has_flag(sg_chat.popup_menu, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(sg_chat.popup_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align_to(sg_chat.popup_menu, sg_chat.plus_btn, LV_ALIGN_OUT_TOP_RIGHT, 0, -4);
        lv_obj_move_foreground(sg_chat.popup_menu);
        PR_DEBUG("plus_click: popup shown");
    } else {
        __popup_dismiss();
        PR_DEBUG("plus_click: popup hidden");
    }
}
#endif /* ENABLE_LVGL_TP */

/* ── chat page callbacks (registered into AI_UI_INTFS_T) ── */

/**
 * @brief Show chat content area, hide picture view.
 */
static void __ui_open_chat(void)
{
    lv_vendor_disp_lock();
    lv_obj_clear_flag(sg_chat.content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_chat.picture, LV_OBJ_FLAG_HIDDEN);
#if defined(ENABLE_LVGL_TP) && (ENABLE_LVGL_TP == 1)
    lv_obj_clear_flag(sg_chat.plus_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_chat.popup_menu, LV_OBJ_FLAG_HIDDEN);
#endif
    lv_vendor_disp_unlock();
}

/**
 * @brief Hide chat content area.
 */
static void __ui_close_chat(void)
{
    lv_vendor_disp_lock();
    lv_obj_add_flag(sg_chat.content, LV_OBJ_FLAG_HIDDEN);
#if defined(ENABLE_LVGL_TP) && (ENABLE_LVGL_TP == 1)
    lv_obj_add_flag(sg_chat.plus_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_chat.popup_menu, LV_OBJ_FLAG_HIDDEN);
#endif
    lv_vendor_disp_unlock();
}

/**
 * @brief Display a user message bubble (right-aligned, green, user avatar).
 *
 * @param text User message text.
 */
static void __ui_set_user_msg(char *text)
{
    if (sg_chat.content == NULL || text == NULL || strlen(text) == 0) {
        return;
    }

    lv_vendor_disp_lock();

    __chat_check_msg_limit();

    __create_user_msg_label(sg_chat.content, text);

    lv_obj_update_layout(sg_chat.content);

    lv_vendor_disp_unlock();
}

/**
 * @brief Display an AI message bubble (left-aligned, white, robot avatar).
 *
 * @param text AI message text.
 */
static void __ui_set_ai_msg(char *text)
{
    if (sg_chat.content == NULL || text == NULL || strlen(text) == 0) {
        return;
    }

    lv_vendor_disp_lock();

    __chat_check_msg_limit();

    __create_ai_msg_label(sg_chat.content, text);

    lv_obj_update_layout(sg_chat.content);

    lv_vendor_disp_unlock();
}

/**
 * @brief Start streaming AI message — create empty AI bubble.
 */
static void __ui_set_ai_msg_stream_start(void)
{
    if (sg_chat.content == NULL) {
        return;
    }

    lv_vendor_disp_lock();

    __chat_check_msg_limit();

    sg_chat.stream_msg_cont = lv_obj_create(sg_chat.content);
    lv_obj_remove_style_all(sg_chat.stream_msg_cont);
    lv_obj_set_size(sg_chat.stream_msg_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(sg_chat.stream_msg_cont, 6, 0);
    lv_obj_set_style_pad_column(sg_chat.stream_msg_cont, 10, 0);

    lv_obj_t *avatar = lv_obj_create(sg_chat.stream_msg_cont);
    lv_obj_add_style(avatar, &sg_chat.style_avatar, 0);
    lv_obj_set_size(avatar, 40, 40);
    lv_obj_align(avatar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(avatar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(avatar, 0, 0);
    lv_obj_clear_flag(avatar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = lv_img_create(avatar);
    lv_img_set_src(icon, &icon_ai_icon);
    lv_obj_center(icon);

    sg_chat.stream_bubble = lv_obj_create(sg_chat.stream_msg_cont);
    lv_obj_set_width(sg_chat.stream_bubble, LV_PCT(75));
    lv_obj_set_height(sg_chat.stream_bubble, LV_SIZE_CONTENT);
    lv_obj_add_style(sg_chat.stream_bubble, &sg_chat.style_ai_bubble, 0);
    lv_obj_align_to(sg_chat.stream_bubble, avatar, LV_ALIGN_OUT_RIGHT_TOP, 10, 0);
    lv_obj_set_scrollbar_mode(sg_chat.stream_bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(sg_chat.stream_bubble, LV_DIR_VER);

    lv_obj_t *text_cont = lv_obj_create(sg_chat.stream_bubble);
    lv_obj_remove_style_all(text_cont);
    lv_obj_set_size(text_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);

    sg_chat.stream_label = lv_label_create(text_cont);
    lv_label_set_text(sg_chat.stream_label, "");
    lv_obj_set_width(sg_chat.stream_label, LV_PCT(100));
    lv_label_set_long_mode(sg_chat.stream_label, LV_LABEL_LONG_WRAP);

    lv_vendor_disp_unlock();

    sg_chat.is_streaming = true;
}

/**
 * @brief Append text to the streaming AI bubble.
 *
 * @param text Text chunk to append.
 */
static void __ui_set_ai_msg_stream_data(char *text)
{
    if (false == sg_chat.is_streaming) {
        return;
    }

    lv_vendor_disp_lock();

    lv_label_ins_text(sg_chat.stream_label, LV_LABEL_POS_LAST, text);

    lv_coord_t content_height = lv_obj_get_height(sg_chat.stream_msg_cont);
    lv_coord_t height = lv_obj_get_height(sg_chat.content);

    if (content_height > height) {
        lv_coord_t offset = 0;
        offset = lv_obj_get_scroll_bottom(sg_chat.content);
        if (offset > 0) {
            lv_obj_scroll_by_bounded(sg_chat.content, 0, -offset, LV_ANIM_OFF);
        }
    } else {
        lv_obj_scroll_to_view_recursive(sg_chat.stream_msg_cont, LV_ANIM_OFF);
    }

    lv_obj_update_layout(sg_chat.content);

    lv_vendor_disp_unlock();
}

/**
 * @brief Mark streaming done.
 */
static void __ui_set_ai_msg_stream_end(void)
{
    sg_chat.is_streaming = false;
}

/**
 * @brief Decode JPEG and display on picture canvas, add "View Image" link in chat.
 *
 * @param jpeg Pointer to JPEG data.
 * @param len  Length of JPEG data.
 */
static void __ui_disp_image(AI_UI_IMG_T *img)
{
    if (img == NULL || img->data == NULL || img->len == 0) {
        return;
    }
    uint8_t  *jpeg = img->data;
    uint32_t  len  = img->len;

    /* Store album filename so the action buttons can reference it */
    if (img->name != NULL) {
        strncpy(sg_chat.cur_img_name, img->name, ALBUM_FILENAME_MAX_LEN);
        sg_chat.cur_img_name[ALBUM_FILENAME_MAX_LEN] = '\0';
    } else {
        sg_chat.cur_img_name[0] = '\0';
    }
    PR_NOTICE("picture: disp_image name='%s'", sg_chat.cur_img_name);

    TAL_IMAGE_JPEG_INFO_T info = {0};

    if (tal_image_jpeg_get_info(jpeg, len, &info) != OPRT_OK) {
        PR_ERR("wechat chat: jpeg get info failed");
        return;
    }

    uint32_t rgb565_size = info.width * info.height * 2;
    PR_NOTICE("wechat chat: rgb565 width=%u, height=%u, size=%u", info.width, info.height, rgb565_size);
    uint8_t *rgb565_buf = Malloc(rgb565_size);
    if (rgb565_buf == NULL) {
        PR_ERR("wechat chat: malloc rgb565 buf failed, size=%u", rgb565_size);
        return;
    }
    memset(rgb565_buf, 0, rgb565_size);

    TAL_IMAGE_JPEG_OUTPUT_T out = {0};
    out.out_buf      = rgb565_buf;
    out.out_buf_size = rgb565_size;
    out.out_width    = info.width;
    out.out_height   = info.height;

    if (tal_image_jpeg_decode_rgb565(jpeg, len, &out) != OPRT_OK) {
        PR_ERR("wechat chat: jpeg decode rgb565 failed");
        Free(rgb565_buf);
        return;
    }

    lv_vendor_disp_lock();

    if (NULL == sg_chat.picture_canvas) {
        sg_chat.picture_canvas = lv_canvas_create(sg_chat.picture);
        lv_obj_set_size(sg_chat.picture_canvas, LV_HOR_RES, LV_VER_RES);
    }

    lv_canvas_set_buffer(sg_chat.picture_canvas, rgb565_buf,
                         info.width, info.height, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(sg_chat.picture_canvas, info.width, info.height);
    lv_obj_center(sg_chat.picture_canvas);

    /* Create action bar lazily AFTER the canvas so it is always on top (higher z-order). */
    if (NULL == sg_chat.picture_action_bar) {
        PR_NOTICE("picture: creating action bar (first show)");
        sg_chat.picture_action_bar = lv_obj_create(sg_chat.picture);
        lv_obj_set_size(sg_chat.picture_action_bar, 44, 84);
        lv_obj_align(sg_chat.picture_action_bar, LV_ALIGN_RIGHT_MID, -8 - WECHAT_SAFE_INSET, 0);
        lv_obj_set_style_bg_color(sg_chat.picture_action_bar, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(sg_chat.picture_action_bar, 140, 0);
        lv_obj_set_style_radius(sg_chat.picture_action_bar, 18, 0);
        lv_obj_set_style_border_width(sg_chat.picture_action_bar, 0, 0);
        lv_obj_set_style_pad_all(sg_chat.picture_action_bar, 0, 0);
        lv_obj_clear_flag(sg_chat.picture_action_bar, LV_OBJ_FLAG_SCROLLABLE);

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
        sg_chat.picture_print_btn = lv_obj_create(sg_chat.picture_action_bar);
        lv_obj_set_size(sg_chat.picture_print_btn, 40, 40);
        lv_obj_align(sg_chat.picture_print_btn, LV_ALIGN_TOP_MID, 0, 2);
        lv_obj_set_style_bg_opa(sg_chat.picture_print_btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(sg_chat.picture_print_btn, 0, 0);
        lv_obj_set_style_pad_all(sg_chat.picture_print_btn, 0, 0);
        lv_obj_set_style_radius(sg_chat.picture_print_btn, 0, 0);
        lv_obj_clear_flag(sg_chat.picture_print_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(sg_chat.picture_print_btn, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *print_icon = lv_img_create(sg_chat.picture_print_btn);
        lv_img_set_src(print_icon, &icon_printer_app);
        lv_obj_set_style_img_recolor(print_icon, lv_color_white(), 0);
        lv_obj_set_style_img_recolor_opa(print_icon, LV_OPA_COVER, 0);
        lv_obj_center(print_icon);

        lv_obj_add_event_cb(sg_chat.picture_print_btn, __picture_print_btn_cb, LV_EVENT_CLICKED, NULL);
#endif /* ENABLE_PRINTER */

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
        sg_chat.picture_attach_btn = lv_obj_create(sg_chat.picture_action_bar);
        lv_obj_set_size(sg_chat.picture_attach_btn, 40, 40);
        lv_obj_align(sg_chat.picture_attach_btn, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_obj_set_style_bg_opa(sg_chat.picture_attach_btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(sg_chat.picture_attach_btn, 0, 0);
        lv_obj_set_style_pad_all(sg_chat.picture_attach_btn, 0, 0);
        lv_obj_set_style_radius(sg_chat.picture_attach_btn, 0, 0);
        lv_obj_clear_flag(sg_chat.picture_attach_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(sg_chat.picture_attach_btn, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *attach_icon = lv_img_create(sg_chat.picture_attach_btn);
        lv_img_set_src(attach_icon, &icon_add_img);
        lv_obj_set_style_img_recolor(attach_icon, lv_color_white(), 0);
        lv_obj_set_style_img_recolor_opa(attach_icon, LV_OPA_COVER, 0);
        lv_obj_center(attach_icon);

        lv_obj_add_event_cb(sg_chat.picture_attach_btn, __picture_attach_btn_cb, LV_EVENT_CLICKED, NULL);
#endif /* ENABLE_IMAGE_ALBUM */
    }

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    if (NULL == sg_chat.picture_print_overlay) {
        PR_NOTICE("picture: creating print overlay (first show)");
        sg_chat.picture_print_overlay = lv_obj_create(sg_chat.picture);
        lv_obj_set_size(sg_chat.picture_print_overlay, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_pos(sg_chat.picture_print_overlay, 0, 0);
        lv_obj_set_style_bg_color(sg_chat.picture_print_overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(sg_chat.picture_print_overlay, LV_OPA_70, 0);
        lv_obj_set_style_border_width(sg_chat.picture_print_overlay, 0, 0);
        lv_obj_set_style_radius(sg_chat.picture_print_overlay, 0, 0);
        lv_obj_clear_flag(sg_chat.picture_print_overlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(sg_chat.picture_print_overlay, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_flex_flow(sg_chat.picture_print_overlay, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(sg_chat.picture_print_overlay, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(sg_chat.picture_print_overlay, 24, 0);

        /* Title label */
        lv_obj_t *print_title = lv_label_create(sg_chat.picture_print_overlay);
        lv_obj_set_style_text_color(print_title, lv_color_white(), 0);
        lv_label_set_text(print_title, PRINT_IMAGE);
        sg_chat.picture_print_status_label = print_title;

        /* Button row */
        lv_obj_t *btn_row = lv_obj_create(sg_chat.picture_print_overlay);
        lv_obj_remove_style_all(btn_row);
        lv_obj_set_size(btn_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(btn_row, 24, 0);
        sg_chat.picture_print_btn_row = btn_row;

        /* Cancel button */
        lv_obj_t *cancel_btn = lv_obj_create(btn_row);
        lv_obj_set_size(cancel_btn, 80, 36);
        lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x555555), 0);
        lv_obj_set_style_bg_opa(cancel_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cancel_btn, 0, 0);
        lv_obj_set_style_radius(cancel_btn, 8, 0);
        lv_obj_set_style_pad_all(cancel_btn, 0, 0);
        lv_obj_clear_flag(cancel_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(cancel_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *cancel_label = lv_label_create(cancel_btn);
        lv_label_set_text(cancel_label, CANCEL);
        lv_obj_set_style_text_color(cancel_label, lv_color_white(), 0);
        lv_obj_center(cancel_label);
        lv_obj_add_event_cb(cancel_btn, __picture_print_cancel_btn_cb, LV_EVENT_CLICKED, NULL);

        /* Confirm button */
        lv_obj_t *confirm_btn = lv_obj_create(btn_row);
        lv_obj_set_size(confirm_btn, 80, 36);
        lv_obj_set_style_bg_color(confirm_btn, lv_color_hex(0x07C160), 0);
        lv_obj_set_style_bg_opa(confirm_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(confirm_btn, 0, 0);
        lv_obj_set_style_radius(confirm_btn, 8, 0);
        lv_obj_set_style_pad_all(confirm_btn, 0, 0);
        lv_obj_clear_flag(confirm_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(confirm_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *confirm_label = lv_label_create(confirm_btn);
        lv_label_set_text(confirm_label, CONFIRM_TEXT);
        lv_obj_set_style_text_color(confirm_label, lv_color_white(), 0);
        lv_obj_center(confirm_label);
        lv_obj_add_event_cb(confirm_btn, __picture_print_confirm_btn_cb, LV_EVENT_CLICKED, NULL);
    }
#endif /* ENABLE_PRINTER */

    if (sg_picture_buffer) {
        Free(sg_picture_buffer);
        sg_picture_buffer = NULL;
    }
    sg_picture_buffer = rgb565_buf;

    lv_obj_add_flag(sg_chat.content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_chat.picture, LV_OBJ_FLAG_HIDDEN);
#if defined(ENABLE_LVGL_TP) && (ENABLE_LVGL_TP == 1)
    lv_obj_add_flag(sg_chat.plus_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_chat.popup_menu, LV_OBJ_FLAG_HIDDEN);
#endif

    if (sg_image_auto_return_tm != NULL) {
        lv_timer_del(sg_image_auto_return_tm);
        sg_image_auto_return_tm = NULL;
    }
    sg_image_auto_return_tm = lv_timer_create(__image_auto_return_cb, 10000, NULL);

    lv_vendor_disp_unlock();
}

/**
 * @brief Display a clickable link label in the chat area.
 *
 * @param is_ai  true = AI side (left), false = user side (right).
 * @param text   Link text to display.
 * @param cb     Callback invoked when user clicks the link.
 * @param cb_arg Argument passed to the callback.
 * @param len    Length of cb_arg data.
 */
static void __create_link_label(bool is_ai, char *text, AI_UI_CHAT_LINK_CB cb, void *cb_arg, uint32_t len)
{
    __chat_check_msg_limit();

    lv_obj_t *label;
    if (is_ai) {
        label = __create_ai_msg_label(sg_chat.content, text);
    } else {
        label = __create_user_msg_label(sg_chat.content, text);
    }

    lv_obj_add_style(label, &sg_chat.style_link, LV_STATE_DEFAULT);
    lv_obj_add_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(label, 12);

    if (cb) {
        UI_LINK_CB_DATA_T *link_data = Malloc(sizeof(UI_LINK_CB_DATA_T));
        if (link_data) {
            link_data->cb  = cb;
            link_data->len = len;
            if (len > 0 && cb_arg != NULL) {
                link_data->cb_arg = Malloc(len + 1);
                if (link_data->cb_arg) {
                    memcpy(link_data->cb_arg, cb_arg, len);
                    ((char *)link_data->cb_arg)[len] = '\0';
                }
            } else {
                link_data->cb_arg = cb_arg;
            }
            lv_obj_add_event_cb(label, __link_click_event_cb, LV_EVENT_CLICKED, link_data);
            lv_obj_add_event_cb(label, __link_delete_event_cb, LV_EVENT_DELETE, link_data);
        }
    }

    lv_obj_update_layout(sg_chat.content);
    lv_obj_scroll_to_view_recursive(label, LV_ANIM_ON);
}

static void __ui_disp_link(bool is_ai, char *text, AI_UI_CHAT_LINK_CB cb, void *cb_arg, uint32_t len)
{
    if (sg_chat.content == NULL || text == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    __create_link_label(is_ai, text, cb, cb_arg, len);
    lv_vendor_disp_unlock();

    if (cb) {
        cb(cb_arg);
    }
}

/**
 * @brief Remove a single attached image when user taps the X button.
 */
static void __attach_remove_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *container = lv_obj_get_parent(btn);
    if (NULL == container) {
        return;
    }

    /* Free the thumbnail buffer stored in user_data */
    uint8_t *buf = (uint8_t *)lv_obj_get_user_data(container);
    if (buf) {
        for (uint8_t i = 0; i < sg_chat.attach_count; i++) {
            if (sg_chat.attach_bufs[i] == buf) {
                Free(sg_chat.attach_bufs[i]);
                /* Shift remaining entries */
                for (uint8_t j = i; j < sg_chat.attach_count - 1; j++) {
                    sg_chat.attach_bufs[j] = sg_chat.attach_bufs[j + 1];
                }
                sg_chat.attach_bufs[sg_chat.attach_count - 1] = NULL;
                sg_chat.attach_count--;
                break;
            }
        }
    }

    lv_obj_del(container);

    /* Hide attach bar if empty */
    if (sg_chat.attach_count == 0) {
        lv_obj_add_flag(sg_chat.attach_bar, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Display an attached image thumbnail in the chat attach bar.
 *
 * @param img Pointer to the image data (JPEG from album).
 */
static void __ui_add_chat_attch_img(AI_UI_IMG_T *img)
{
    if (sg_chat.attach_bar == NULL || img == NULL || img->data == NULL || img->len == 0) {
        return;
    }

    if (sg_chat.attach_count >= MAX_ATTACH_NUM) {
        return;
    }

    /* Decode JPEG and scale to thumbnail in one step */
    TAL_IMAGE_JPEG_SCALE_IN_T scale_in = {0};
    scale_in.method     = TAL_IMAGE_SCALE_MTH_NEAREST;
    scale_in.mode       = TAL_IMAGE_SCALE_MODE_SIZE;
    scale_in.data       = img->data;
    scale_in.size       = img->len;
    scale_in.out_width  = ATTACH_THUMB_SIZE;
    scale_in.out_height = ATTACH_THUMB_SIZE;

    TAL_IMAGE_SCALE_OUT_T scale_out = {0};
    if (tal_image_jpeg_scale_rgb565(&scale_in, &scale_out) != OPRT_OK) {
        PR_ERR("attach: jpeg scale failed");
        return;
    }

    /* Copy to our own buffer so we can free with Free() later */
    uint32_t thumb_buf_size = ATTACH_THUMB_SIZE * ATTACH_THUMB_SIZE * 2;
    uint8_t *thumb_buf = (uint8_t *)Malloc(thumb_buf_size);
    if (NULL == thumb_buf) {
        PR_ERR("attach: malloc thumb failed");
        tal_image_scale_buf_free(&scale_out);
        return;
    }
    memcpy(thumb_buf, scale_out.buf, thumb_buf_size);
    tal_image_scale_buf_free(&scale_out);

    lv_vendor_disp_lock();

    /* Container wrapping thumbnail + close button */
    lv_obj_t *container = lv_obj_create(sg_chat.attach_bar);
    lv_obj_set_size(container, ATTACH_THUMB_SIZE, ATTACH_THUMB_SIZE);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_radius(container, 4, 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(container, thumb_buf);

    /* Thumbnail canvas */
    lv_obj_t *canvas = lv_canvas_create(container);
    lv_canvas_set_buffer(canvas, thumb_buf,
                         ATTACH_THUMB_SIZE, ATTACH_THUMB_SIZE, LV_COLOR_FORMAT_RGB565);

    /* Close button — top-right corner, solid red badge */
    lv_obj_t *close_btn = lv_obj_create(container);
    lv_obj_set_size(close_btn, 18, 18);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFF3B30), 0);
    lv_obj_set_style_bg_opa(close_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(close_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(close_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_set_style_pad_all(close_btn, 0, 0);
    lv_obj_clear_flag(close_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(close_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(close_btn);

    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_obj_set_style_text_color(close_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(close_label, ai_ui_get_icon_font(), 0);
    lv_label_set_text(close_label, FONT_AWESOME_XMARK);
    lv_obj_center(close_label);

    lv_obj_add_event_cb(close_btn, __attach_remove_cb, LV_EVENT_CLICKED, NULL);

    sg_chat.attach_bufs[sg_chat.attach_count] = thumb_buf;
    sg_chat.attach_count++;

    /* Show attach bar */
    lv_obj_clear_flag(sg_chat.attach_bar, LV_OBJ_FLAG_HIDDEN);

    lv_vendor_disp_unlock();
}

/**
 * @brief Clear all attached image thumbnails from the chat attach bar.
 */
static void __ui_clear_chat_attach(void)
{
    if (sg_chat.attach_bar == NULL) {
        return;
    }

    lv_vendor_disp_lock();

    lv_obj_clean(sg_chat.attach_bar);

    for (uint8_t i = 0; i < sg_chat.attach_count; i++) {
        if (sg_chat.attach_bufs[i]) {
            Free(sg_chat.attach_bufs[i]);
            sg_chat.attach_bufs[i] = NULL;
        }
    }
    sg_chat.attach_count = 0;

    lv_obj_add_flag(sg_chat.attach_bar, LV_OBJ_FLAG_HIDDEN);

    lv_vendor_disp_unlock();
}

/* ── public API ── */

/**
 * @brief Initialize the wechat chat page widgets. Called once during UI init
 *        (already inside lv_vendor_disp_lock).
 *
 * @param parent Parent LVGL container (the main screen container).
 */
void ai_ui_wechat_chat_init(lv_obj_t *parent)
{
    lv_coord_t content_w = LV_HOR_RES - (WECHAT_SAFE_INSET * 2);
    lv_coord_t attach_w = LV_HOR_RES - (WECHAT_SAFE_INSET * 2);
    lv_coord_t content_h = LV_VER_RES - 40 - WECHAT_SAFE_INSET;

    if (content_w < 1) {
        content_w = LV_HOR_RES;
    }
    if (attach_w < 1) {
        attach_w = LV_HOR_RES;
    }
    if (content_h < 1) {
        content_h = LV_VER_RES - 40;
    }

    /* Style init — already inside disp lock, safe to call LVGL style APIs */
    lv_style_init(&sg_chat.style_avatar);
    lv_style_set_radius(&sg_chat.style_avatar, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&sg_chat.style_avatar, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_border_width(&sg_chat.style_avatar, 1);
    lv_style_set_border_color(&sg_chat.style_avatar, lv_palette_darken(LV_PALETTE_GREY, 2));

    lv_style_init(&sg_chat.style_ai_bubble);
    lv_style_set_bg_color(&sg_chat.style_ai_bubble, lv_color_white());
    lv_style_set_radius(&sg_chat.style_ai_bubble, 15);
    lv_style_set_pad_all(&sg_chat.style_ai_bubble, 12);
    lv_style_set_shadow_width(&sg_chat.style_ai_bubble, 12);
    lv_style_set_shadow_color(&sg_chat.style_ai_bubble, lv_color_hex(0xCCCCCC));

    lv_style_init(&sg_chat.style_user_bubble);
    lv_style_set_bg_color(&sg_chat.style_user_bubble, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_text_color(&sg_chat.style_user_bubble, lv_color_white());
    lv_style_set_radius(&sg_chat.style_user_bubble, 15);
    lv_style_set_pad_all(&sg_chat.style_user_bubble, 12);
    lv_style_set_shadow_width(&sg_chat.style_user_bubble, 12);
    lv_style_set_shadow_color(&sg_chat.style_user_bubble, lv_palette_darken(LV_PALETTE_GREEN, 2));

    lv_style_init(&sg_chat.style_link);
    lv_style_set_text_color(&sg_chat.style_link, lv_color_hex(0x0066CC));
    lv_style_set_text_decor(&sg_chat.style_link, LV_TEXT_DECOR_UNDERLINE);

    /* Chat content area */
    sg_chat.content = lv_obj_create(parent);
    lv_obj_set_size(sg_chat.content, content_w, content_h);
    lv_obj_set_flex_flow(sg_chat.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_ver(sg_chat.content, 8, 0);
    lv_obj_set_style_pad_hor(sg_chat.content, 10, 0);
    lv_obj_align(sg_chat.content, LV_ALIGN_BOTTOM_MID, 0, -WECHAT_SAFE_INSET);

    lv_obj_set_scroll_dir(sg_chat.content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(sg_chat.content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(sg_chat.content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sg_chat.content, 0, 0);
    lv_obj_add_flag(sg_chat.content, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sg_chat.content, __content_click_cb, LV_EVENT_CLICKED, NULL);

    /* Picture view area — hidden by default */
    sg_chat.picture = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sg_chat.picture, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(sg_chat.picture, 0, 0);
    lv_obj_set_style_border_width(sg_chat.picture, 0, 0);
    lv_obj_set_style_radius(sg_chat.picture, 0, 0);
    lv_obj_align(sg_chat.picture, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(sg_chat.picture, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(sg_chat.picture, __return_chat_content_event_cb, LV_EVENT_CLICKED, NULL);

    /* Attach bar — horizontal row of thumbnails at bottom, hidden by default */
    sg_chat.attach_bar = lv_obj_create(parent);
    lv_obj_set_size(sg_chat.attach_bar, attach_w, ATTACH_BAR_HEIGHT);
    lv_obj_align(sg_chat.attach_bar, LV_ALIGN_BOTTOM_MID, 0, -WECHAT_SAFE_INSET);
    lv_obj_set_style_bg_color(sg_chat.attach_bar, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_bg_opa(sg_chat.attach_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(sg_chat.attach_bar, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_border_width(sg_chat.attach_bar, 1, 0);
    lv_obj_set_style_border_side(sg_chat.attach_bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_all(sg_chat.attach_bar, 6, 0);
    lv_obj_set_style_pad_column(sg_chat.attach_bar, 6, 0);
    lv_obj_set_style_radius(sg_chat.attach_bar, 0, 0);
    lv_obj_set_flex_flow(sg_chat.attach_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sg_chat.attach_bar, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(sg_chat.attach_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sg_chat.attach_bar, LV_OBJ_FLAG_HIDDEN);

#if defined(ENABLE_LVGL_TP) && (ENABLE_LVGL_TP == 1)
    /* "+" button — bottom-right, offset inward so it's not flush with the corner */
    sg_chat.plus_btn = lv_obj_create(parent);
    lv_obj_set_size(sg_chat.plus_btn, PLUS_BTN_SIZE, PLUS_BTN_SIZE);
    lv_obj_align(sg_chat.plus_btn, LV_ALIGN_BOTTOM_RIGHT, -24 - WECHAT_SAFE_INSET, -20 - WECHAT_SAFE_INSET);
    lv_obj_set_style_bg_color(sg_chat.plus_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(sg_chat.plus_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sg_chat.plus_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(sg_chat.plus_btn, 8, 0);
    lv_obj_set_style_shadow_color(sg_chat.plus_btn, lv_color_hex(0xBBBBBB), 0);
    lv_obj_set_style_border_width(sg_chat.plus_btn, 1, 0);
    lv_obj_set_style_border_color(sg_chat.plus_btn, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_pad_all(sg_chat.plus_btn, 0, 0);
    lv_obj_clear_flag(sg_chat.plus_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(sg_chat.plus_btn, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_flag(sg_chat.plus_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(sg_chat.plus_btn, 10);

    lv_obj_t *plus_label = lv_label_create(sg_chat.plus_btn);
    lv_obj_set_style_text_color(plus_label, lv_color_black(), 0);
    lv_label_set_text(plus_label, "+");
    lv_obj_center(plus_label);
    lv_obj_add_event_cb(sg_chat.plus_btn, __plus_btn_click_cb, LV_EVENT_CLICKED, NULL);

    /* Popup menu — above "+" button, hidden by default */
    sg_chat.popup_menu = lv_obj_create(parent);
    lv_obj_set_size(sg_chat.popup_menu, POPUP_WIDTH, POPUP_ITEM_H * 3 + 4);
    lv_obj_align_to(sg_chat.popup_menu, sg_chat.plus_btn, LV_ALIGN_OUT_TOP_RIGHT, 0, -4);
    lv_obj_set_style_bg_color(sg_chat.popup_menu, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(sg_chat.popup_menu, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sg_chat.popup_menu, 8, 0);
    lv_obj_set_style_border_width(sg_chat.popup_menu, 1, 0);
    lv_obj_set_style_border_color(sg_chat.popup_menu, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_shadow_width(sg_chat.popup_menu, 8, 0);
    lv_obj_set_style_shadow_color(sg_chat.popup_menu, lv_color_hex(0xBBBBBB), 0);
    lv_obj_set_style_pad_all(sg_chat.popup_menu, 2, 0);
    lv_obj_set_style_pad_row(sg_chat.popup_menu, 0, 0);
    lv_obj_set_flex_flow(sg_chat.popup_menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(sg_chat.popup_menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sg_chat.popup_menu, LV_OBJ_FLAG_HIDDEN);

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    /* Camera option */
    lv_obj_t *cam_btn = lv_obj_create(sg_chat.popup_menu);
    lv_obj_set_size(cam_btn, POPUP_WIDTH - 4, POPUP_ITEM_H);
    lv_obj_set_style_bg_opa(cam_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cam_btn, 0, 0);
    lv_obj_set_style_pad_all(cam_btn, 0, 0);
    lv_obj_set_style_pad_left(cam_btn, 8, 0);
    lv_obj_set_style_pad_column(cam_btn, 8, 0);
    lv_obj_set_style_radius(cam_btn, 6, 0);
    lv_obj_clear_flag(cam_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cam_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(cam_btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cam_btn, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *cam_icon_wrap = lv_obj_create(cam_btn);
    lv_obj_remove_style_all(cam_icon_wrap);
    lv_obj_set_size(cam_icon_wrap, POPUP_ICON_SIZE, POPUP_ICON_SIZE);
    lv_obj_clear_flag(cam_icon_wrap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *cam_icon = lv_img_create(cam_icon_wrap);
    lv_img_set_src(cam_icon, &icon_camera_app);
    lv_obj_set_style_img_recolor(cam_icon, lv_color_hex(0x333333), 0);
    lv_obj_set_style_img_recolor_opa(cam_icon, LV_OPA_COVER, 0);
    lv_obj_center(cam_icon);

    lv_obj_t *cam_label = lv_label_create(cam_btn);
    lv_obj_set_style_text_color(cam_label, lv_color_hex(0x333333), 0);
    lv_label_set_text(cam_label, CAMERA);
    lv_obj_add_event_cb(cam_btn, __popup_camera_cb, LV_EVENT_CLICKED, NULL);
#endif /* ENABLE_COMP_AI_VIDEO */

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
    /* Album option */
    lv_obj_t *album_btn = lv_obj_create(sg_chat.popup_menu);
    lv_obj_set_size(album_btn, POPUP_WIDTH - 4, POPUP_ITEM_H);
    lv_obj_set_style_bg_opa(album_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(album_btn, 0, 0);
    lv_obj_set_style_pad_all(album_btn, 0, 0);
    lv_obj_set_style_pad_left(album_btn, 8, 0);
    lv_obj_set_style_pad_column(album_btn, 8, 0);
    lv_obj_set_style_radius(album_btn, 6, 0);
    lv_obj_clear_flag(album_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(album_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(album_btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(album_btn, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *album_icon_wrap = lv_obj_create(album_btn);
    lv_obj_remove_style_all(album_icon_wrap);
    lv_obj_set_size(album_icon_wrap, POPUP_ICON_SIZE, POPUP_ICON_SIZE);
    lv_obj_clear_flag(album_icon_wrap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *album_icon = lv_img_create(album_icon_wrap);
    lv_img_set_src(album_icon, &icon_photo_app);
    lv_obj_set_style_img_recolor(album_icon, lv_color_hex(0x333333), 0);
    lv_obj_set_style_img_recolor_opa(album_icon, LV_OPA_COVER, 0);
    lv_obj_center(album_icon);

    lv_obj_t *album_label = lv_label_create(album_btn);
    lv_obj_set_style_text_color(album_label, lv_color_hex(0x333333), 0);
    lv_label_set_text(album_label, ALBUM);
    lv_obj_add_event_cb(album_btn, __popup_album_cb, LV_EVENT_CLICKED, NULL);

    /* Add Image option */
    lv_obj_t *add_img_btn = lv_obj_create(sg_chat.popup_menu);
    lv_obj_set_size(add_img_btn, POPUP_WIDTH - 4, POPUP_ITEM_H);
    lv_obj_set_style_bg_opa(add_img_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(add_img_btn, 0, 0);
    lv_obj_set_style_pad_all(add_img_btn, 0, 0);
    lv_obj_set_style_pad_left(add_img_btn, 8, 0);
    lv_obj_set_style_pad_column(add_img_btn, 8, 0);
    lv_obj_set_style_radius(add_img_btn, 6, 0);
    lv_obj_clear_flag(add_img_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(add_img_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(add_img_btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(add_img_btn, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *add_img_icon_wrap = lv_obj_create(add_img_btn);
    lv_obj_remove_style_all(add_img_icon_wrap);
    lv_obj_set_size(add_img_icon_wrap, POPUP_ICON_SIZE, POPUP_ICON_SIZE);
    lv_obj_clear_flag(add_img_icon_wrap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *add_img_icon = lv_img_create(add_img_icon_wrap);
    lv_img_set_src(add_img_icon, &icon_add_img);
    lv_obj_set_style_img_recolor(add_img_icon, lv_color_hex(0x333333), 0);
    lv_obj_set_style_img_recolor_opa(add_img_icon, LV_OPA_COVER, 0);
    lv_obj_center(add_img_icon);

    lv_obj_t *add_img_label = lv_label_create(add_img_btn);
    lv_obj_set_style_text_color(add_img_label, lv_color_hex(0x333333), 0);
    lv_label_set_text(add_img_label, ADD_IMAGE);
    lv_obj_add_event_cb(add_img_btn, __popup_add_img_cb, LV_EVENT_CLICKED, NULL);
#endif /* ENABLE_IMAGE_ALBUM */
#endif /* ENABLE_LVGL_TP */
}

/**
 * @brief Register chat-specific callbacks with the chat management layer.
 */
void ai_ui_wechat_chat_register(void)
{
    AI_UI_CHAT_INTFS_T intfs;
    memset(&intfs, 0, sizeof(AI_UI_CHAT_INTFS_T));

    intfs.disp_open               = __ui_open_chat;
    intfs.disp_close              = __ui_close_chat;
    intfs.disp_user_msg           = __ui_set_user_msg;
    intfs.disp_ai_msg             = __ui_set_ai_msg;
    intfs.disp_ai_msg_stream_start = __ui_set_ai_msg_stream_start;
    intfs.disp_ai_msg_stream_data = __ui_set_ai_msg_stream_data;
    intfs.disp_ai_msg_stream_end  = __ui_set_ai_msg_stream_end;
    intfs.disp_image              = __ui_disp_image;
    intfs.disp_link               = __ui_disp_link;
    intfs.disp_add_chat_attch_img = __ui_add_chat_attch_img;
    intfs.disp_clear_chat_attach  = __ui_clear_chat_attach;
#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    intfs.disp_print_result       = __picture_disp_print_result;
#endif

    ai_ui_chat_register(&intfs);
}

#endif
