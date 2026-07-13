/**
 * @file ai_ui_wechat_album.c
 * @brief WeChat-style album page — single image view, all thumbnails grid
 *        with batch select/delete, and single-select for chat attachment.
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)

#include "lvgl.h"
#include "lv_vendor.h"

#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"
#include "ai_ui_wechat_common.h"
#include "tal_image.h"
#include "image_album.h"

LV_IMG_DECLARE(icon_back_24_24);
LV_IMG_DECLARE(icon_photo_app);
LV_IMG_DECLARE(icon_choose);
LV_IMG_DECLARE(icon_delete);
#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
LV_IMG_DECLARE(icon_printer_app);
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define CTRL_BAR_HEIGHT         48
#define THUMB_SIZE              80
#define THUMB_GAP               8

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    /* Image view */
    lv_obj_t *view_page;
    lv_obj_t *view_canvas;
    lv_obj_t *view_empty_label;
    lv_obj_t *view_delete_btn;
    uint8_t  *view_buf;
    char      view_current_name[ALBUM_FILENAME_MAX_LEN + 1];

    /* All thumbnails */
    lv_obj_t *all_page;
    lv_obj_t *all_grid;
    lv_obj_t *all_empty_label;
    lv_obj_t *all_select_btn;
    lv_obj_t *all_bottom_bar;
    bool      all_select_mode;
    uint8_t   all_select_count;

    /* Select (chat attachment) */
    lv_obj_t *select_page;
    lv_obj_t *select_grid;
    lv_obj_t *select_empty_label;

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    lv_obj_t *print_overlay;
    lv_obj_t *print_status_label;
    lv_obj_t *print_btn_row;
    lv_timer_t *print_result_tm;
#endif
} AI_UI_WECHAT_ALBUM_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_UI_WECHAT_ALBUM_T sg_album = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

/* ── view page callbacks ── */

static void __view_gesture_cb(lv_event_t *e)
{
    (void)e;
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir == LV_DIR_LEFT) {
        ai_ui_notify_action(AI_UI_ACT_VIEW_NEXT_IMG, NULL, 0);
    } else if (dir == LV_DIR_RIGHT) {
        ai_ui_notify_action(AI_UI_ACT_VIEW_PREV_IMG, NULL, 0);
    }
}

static void __view_all_btn_cb(lv_event_t *e)
{
    (void)e;
    ai_ui_notify_action(AI_UI_ACT_VIEW_ALL_IMG, NULL, 0);
}

static void __view_delete_btn_cb(lv_event_t *e)
{
    (void)e;
    if (sg_album.view_current_name[0] == '\0') {
        return;
    }
    ai_ui_notify_action(AI_UI_ACT_DELETE_IMG,
                        (uint8_t *)sg_album.view_current_name,
                        (uint32_t)strlen(sg_album.view_current_name));
}

static void __view_close_btn_cb(lv_event_t *e)
{
    (void)e;
    ai_ui_notify_action(AI_UI_ACT_CLOSE_ALBUM, NULL, 0);
}

/* ── all page callbacks ── */

static void __all_back_btn_cb(lv_event_t *e)
{
    (void)e;
    ai_ui_notify_action(AI_UI_ACT_CLOSE_ALBUM, NULL, 0);
}

static void __all_enter_select_mode(void)
{
    lv_vendor_disp_lock();

    sg_album.all_select_mode  = true;
    sg_album.all_select_count = 0;

    lv_obj_add_flag(sg_album.all_select_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_album.all_bottom_bar, LV_OBJ_FLAG_HIDDEN);

    /* Show circle indicator on every thumbnail */
    uint32_t cnt = lv_obj_get_child_cnt(sg_album.all_grid);
    uint32_t i;
    for (i = 0; i < cnt; i++) {
        lv_obj_t *item = lv_obj_get_child(sg_album.all_grid, i);
        if (!item) continue;
        uint32_t n = lv_obj_get_child_cnt(item);
        if (n == 0) continue;
        lv_obj_t *circle = lv_obj_get_child(item, n - 1);
        if (circle) lv_obj_clear_flag(circle, LV_OBJ_FLAG_HIDDEN);
    }

    lv_vendor_disp_unlock();
}

static void __all_exit_select_mode(void)
{
    lv_vendor_disp_lock();

    sg_album.all_select_mode  = false;
    sg_album.all_select_count = 0;

    lv_obj_clear_flag(sg_album.all_select_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_album.all_bottom_bar, LV_OBJ_FLAG_HIDDEN);

    /* Hide circle indicators and clear their selection state */
    uint32_t cnt = lv_obj_get_child_cnt(sg_album.all_grid);
    uint32_t i;
    for (i = 0; i < cnt; i++) {
        lv_obj_t *item = lv_obj_get_child(sg_album.all_grid, i);
        if (!item) continue;
        uint32_t n = lv_obj_get_child_cnt(item);
        if (n == 0) continue;
        lv_obj_t *circle = lv_obj_get_child(item, n - 1);
        if (circle) {
            lv_obj_add_flag(circle, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_state(circle, LV_STATE_CHECKED);
            lv_obj_set_style_bg_opa(circle, LV_OPA_TRANSP, 0);
        }
    }

    lv_vendor_disp_unlock();
}

static void __all_select_btn_cb(lv_event_t *e)
{
    (void)e;
    __all_enter_select_mode();
}

static void __all_item_toggle_cb(lv_event_t *e)
{
    lv_obj_t *container = lv_event_get_target(e);
    uint32_t cnt = lv_obj_get_child_cnt(container);
    if (cnt == 0) return;

    /* Circle indicator is always the last child of the container */
    lv_obj_t *circle = lv_obj_get_child(container, cnt - 1);
    if (!circle) return;

    if (lv_obj_get_state(circle) & LV_STATE_CHECKED) {
        lv_obj_clear_state(circle, LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(circle, LV_OPA_TRANSP, 0);
        if (sg_album.all_select_count > 0) sg_album.all_select_count--;
    } else {
        if (sg_album.all_select_count >= AI_UI_BATCH_DELETE_MAX) {
            return;
        }
        lv_obj_add_state(circle, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(circle, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
        sg_album.all_select_count++;
    }
}

static void __all_select_delete_cb(lv_event_t *e)
{
    (void)e;

    AI_UI_BATCH_DELETE_T *batch = (AI_UI_BATCH_DELETE_T *)Malloc(sizeof(AI_UI_BATCH_DELETE_T));
    if (NULL == batch) {
        return;
    }
    memset(batch, 0, sizeof(AI_UI_BATCH_DELETE_T));

    /* Collect selected items and their LVGL object pointers */
    lv_obj_t *to_delete[AI_UI_BATCH_DELETE_MAX];
    uint32_t del_cnt = 0;
    uint32_t child_cnt = lv_obj_get_child_cnt(sg_album.all_grid);
    uint32_t i;

    for (i = 0; i < child_cnt && batch->count < AI_UI_BATCH_DELETE_MAX; i++) {
        lv_obj_t *item = lv_obj_get_child(sg_album.all_grid, i);
        if (!item) continue;
        uint32_t cnt = lv_obj_get_child_cnt(item);
        if (cnt == 0) continue;
        lv_obj_t *circle = lv_obj_get_child(item, cnt - 1);
        if (!circle) continue;
        if (!(lv_obj_get_state(circle) & LV_STATE_CHECKED)) continue;

        char *name = (char *)lv_obj_get_user_data(item);
        if (name && name[0]) {
            strncpy(batch->names[batch->count], name, AI_UI_BATCH_DELETE_NAME_LEN);
            batch->names[batch->count][AI_UI_BATCH_DELETE_NAME_LEN] = '\0';
            batch->count++;
            to_delete[del_cnt++] = item;
        }
    }

    if (batch->count == 0) {
        Free(batch);
        __all_exit_select_mode();
        return;
    }

    /* Remove selected thumbnails from the grid immediately */
    lv_vendor_disp_lock();
    for (i = 0; i < del_cnt; i++) {
        lv_obj_del(to_delete[i]);
    }
    if (lv_obj_get_child_cnt(sg_album.all_grid) == 0) {
        lv_obj_clear_flag(sg_album.all_empty_label, LV_OBJ_FLAG_HIDDEN);
    }
    lv_vendor_disp_unlock();

    /* Restore normal mode UI on remaining items */
    __all_exit_select_mode();

    /* Notify app to delete files from filesystem */
    ai_ui_notify_action(AI_UI_ACT_BATCH_DELETE_IMG,
                        (uint8_t *)batch, sizeof(AI_UI_BATCH_DELETE_T));
    Free(batch);
}

/* ── select page callbacks (chat attachment) ── */

static void __select_cancel_btn_cb(lv_event_t *e)
{
    (void)e;
    ai_ui_notify_action(AI_UI_ACT_CLOSE_ALBUM, NULL, 0);
}

static void __select_item_click_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    char *name = (char *)lv_obj_get_user_data(target);
    if (name != NULL) {
        ai_ui_notify_action(AI_UI_ACT_ADD_IMG_ATTACH,
                            (uint8_t *)name,
                            (uint32_t)strlen(name));
    }
    ai_ui_notify_action(AI_UI_ACT_CLOSE_ALBUM, NULL, 0);
}

/* ── helper: create an image icon button ── */

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
static void __view_print_btn_cb(lv_event_t *e)
{
    (void)e;
    if (sg_album.view_current_name[0] == '\0') {
        return;
    }
    lv_obj_clear_flag(sg_album.print_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(sg_album.print_overlay);
}

static void __print_cancel_btn_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_add_flag(sg_album.print_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void __print_confirm_btn_cb(lv_event_t *e)
{
    (void)e;
    /* Switch overlay to "printing" state — keep it visible so the user sees feedback */
    lv_label_set_text(sg_album.print_status_label, PRINTING);
    lv_obj_add_flag(sg_album.print_btn_row, LV_OBJ_FLAG_HIDDEN);
    ai_ui_notify_action(AI_UI_ACT_PRINT_IMG,
                        (uint8_t *)sg_album.view_current_name,
                        (uint32_t)strlen(sg_album.view_current_name));
}

static void __print_result_timeout_cb(lv_timer_t *timer)
{
    (void)timer;
    lv_timer_del(sg_album.print_result_tm);
    sg_album.print_result_tm = NULL;
    lv_obj_add_flag(sg_album.print_overlay, LV_OBJ_FLAG_HIDDEN);
    /* Restore overlay to confirmation state for next use */
    lv_label_set_text(sg_album.print_status_label, PRINT_IMAGE);
    lv_obj_clear_flag(sg_album.print_btn_row, LV_OBJ_FLAG_HIDDEN);
}

static void __disp_print_result(bool ok)
{
    lv_vendor_disp_lock();
    lv_label_set_text(sg_album.print_status_label, ok ? PRINT_SUCCESS : PRINT_FAILED);
    if (sg_album.print_result_tm == NULL) {
        sg_album.print_result_tm = lv_timer_create(__print_result_timeout_cb, 2000, NULL);
    } else {
        lv_timer_reset(sg_album.print_result_tm);
    }
    lv_vendor_disp_unlock();
}
#endif /* ENABLE_PRINTER */

static lv_obj_t *__create_icon_btn(lv_obj_t *parent, const lv_img_dsc_t *src,
                                    lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, 40, 40);
    lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_50, 0);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *img = lv_img_create(btn);
    lv_img_set_src(img, src);
    lv_obj_set_style_img_recolor(img, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_center(img);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    return btn;
}

/* ── sub-page creation ── */

static void __create_view_page(lv_obj_t *parent)
{
    lv_coord_t page_w = LV_HOR_RES;
    lv_coord_t page_h = LV_VER_RES;

    /* View page container */
    sg_album.view_page = lv_obj_create(parent);
    lv_obj_set_size(sg_album.view_page, page_w, page_h);
    lv_obj_set_style_bg_color(sg_album.view_page, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_album.view_page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sg_album.view_page, 0, 0);
    lv_obj_set_style_pad_all(sg_album.view_page, 0, 0);
    lv_obj_set_style_radius(sg_album.view_page, 0, 0);
    lv_obj_set_scrollbar_mode(sg_album.view_page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(sg_album.view_page, LV_OBJ_FLAG_SCROLLABLE);
    /* Stop gesture bubbling here so swipes fire on view_page, not the root screen */
    lv_obj_clear_flag(sg_album.view_page, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_pos(sg_album.view_page, 0, 0);
    lv_obj_add_flag(sg_album.view_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_album.view_page, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sg_album.view_page, __view_gesture_cb, LV_EVENT_GESTURE, NULL);

    /* Canvas for full-screen image display.
     * lv_canvas_constructor clears CLICKABLE — touches pass through to view_page,
     * and GESTURE_BUBBLE (default) means any canvas gesture bubbles up to view_page. */
    sg_album.view_canvas = lv_canvas_create(sg_album.view_page);
    lv_obj_center(sg_album.view_canvas);

    /* Empty state label */
    sg_album.view_empty_label = lv_label_create(sg_album.view_page);
    lv_obj_set_style_text_color(sg_album.view_empty_label, lv_color_white(), 0);
    lv_label_set_text(sg_album.view_empty_label, NO_IMAGE);
    lv_obj_center(sg_album.view_empty_label);
    lv_obj_add_flag(sg_album.view_empty_label, LV_OBJ_FLAG_HIDDEN);

    /* Back button — top-left, inset from corner for rounded screens */
    lv_obj_t *view_back_btn = lv_obj_create(sg_album.view_page);
    lv_obj_set_size(view_back_btn, 40, 40);
    lv_obj_align(view_back_btn, LV_ALIGN_TOP_LEFT, 4 + WECHAT_SAFE_INSET, 4 + WECHAT_SAFE_INSET);
    lv_obj_set_style_bg_opa(view_back_btn, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(view_back_btn, lv_color_black(), 0);
    lv_obj_set_style_radius(view_back_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(view_back_btn, 0, 0);
    lv_obj_set_style_pad_all(view_back_btn, 0, 0);
    lv_obj_clear_flag(view_back_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(view_back_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(view_back_btn, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_ext_click_area(view_back_btn, WECHAT_BTN_EXT_CLICK);
    lv_obj_add_event_cb(view_back_btn, __view_close_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_icon = lv_img_create(view_back_btn);
    lv_img_set_src(back_icon, &icon_back_24_24);
    lv_obj_set_style_img_recolor(back_icon, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(back_icon, LV_OPA_COVER, 0);
    lv_obj_center(back_icon);

    /* Delete button — bottom-left, inset from corner for rounded screens */
    sg_album.view_delete_btn = lv_obj_create(sg_album.view_page);
    lv_obj_set_size(sg_album.view_delete_btn, 40, 40);
    lv_obj_align(sg_album.view_delete_btn, LV_ALIGN_BOTTOM_LEFT, 8 + WECHAT_SAFE_INSET, -16 - WECHAT_SAFE_INSET);
    lv_obj_set_style_bg_opa(sg_album.view_delete_btn, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(sg_album.view_delete_btn, lv_color_black(), 0);
    lv_obj_set_style_radius(sg_album.view_delete_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(sg_album.view_delete_btn, 0, 0);
    lv_obj_set_style_pad_all(sg_album.view_delete_btn, 0, 0);
    lv_obj_clear_flag(sg_album.view_delete_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sg_album.view_delete_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(sg_album.view_delete_btn, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_ext_click_area(sg_album.view_delete_btn, WECHAT_BTN_EXT_CLICK);
    lv_obj_add_event_cb(sg_album.view_delete_btn, __view_delete_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *del_icon = lv_img_create(sg_album.view_delete_btn);
    lv_img_set_src(del_icon, &icon_delete);
    lv_obj_set_style_img_recolor(del_icon, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(del_icon, LV_OPA_COVER, 0);
    lv_obj_center(del_icon);

    /* "所有照片 >" button — bottom-right, text style with semi-transparent bg */
    lv_obj_t *all_photos_btn = lv_obj_create(sg_album.view_page);
    lv_obj_remove_style_all(all_photos_btn);
    lv_obj_set_size(all_photos_btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(all_photos_btn, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(all_photos_btn, LV_OPA_50, 0);
    lv_obj_set_style_radius(all_photos_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_left(all_photos_btn, 12, 0);
    lv_obj_set_style_pad_right(all_photos_btn, 12, 0);
    lv_obj_set_style_pad_top(all_photos_btn, 8, 0);
    lv_obj_set_style_pad_bottom(all_photos_btn, 8, 0);
    lv_obj_add_flag(all_photos_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(all_photos_btn, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_ext_click_area(all_photos_btn, WECHAT_BTN_EXT_CLICK);
    lv_obj_add_event_cb(all_photos_btn, __view_all_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *all_photos_label = lv_label_create(all_photos_btn);
    lv_obj_set_style_text_color(all_photos_label, lv_color_white(), 0);
    lv_label_set_text_fmt(all_photos_label, "%s >", ALL_PHOTOS);
    lv_obj_center(all_photos_label);

    /* Align after label text is set so LV_SIZE_CONTENT is computed correctly */
    lv_obj_align(all_photos_btn, LV_ALIGN_BOTTOM_RIGHT, -8 - WECHAT_SAFE_INSET, -12 - WECHAT_SAFE_INSET);

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    /* Print button — top-right, inset from corner for rounded screens */
    lv_obj_t *print_btn = __create_icon_btn(sg_album.view_page, &icon_printer_app,
                                             __view_print_btn_cb);
    lv_obj_align(print_btn, LV_ALIGN_TOP_RIGHT, -4 - WECHAT_SAFE_INSET, 4 + WECHAT_SAFE_INSET);
    lv_obj_add_flag(print_btn, LV_OBJ_FLAG_GESTURE_BUBBLE);

    /* Print confirmation overlay — full-screen semi-transparent panel */
    sg_album.print_overlay = lv_obj_create(sg_album.view_page);
    lv_obj_set_size(sg_album.print_overlay, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(sg_album.print_overlay, 0, 0);
    lv_obj_set_style_bg_color(sg_album.print_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_album.print_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(sg_album.print_overlay, 0, 0);
    lv_obj_set_style_radius(sg_album.print_overlay, 0, 0);
    lv_obj_clear_flag(sg_album.print_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sg_album.print_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_flex_flow(sg_album.print_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sg_album.print_overlay, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(sg_album.print_overlay, 24, 0);

    /* Title */
    lv_obj_t *print_title = lv_label_create(sg_album.print_overlay);
    lv_obj_set_style_text_color(print_title, lv_color_white(), 0);
    lv_label_set_text(print_title, PRINT_IMAGE);
    sg_album.print_status_label = print_title;

    /* Button row */
    lv_obj_t *btn_row = lv_obj_create(sg_album.print_overlay);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 24, 0);
    sg_album.print_btn_row = btn_row;

    /* Cancel button */
    lv_obj_t *cancel_btn = lv_obj_create(btn_row);
    lv_obj_set_size(cancel_btn, 80, 36);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x555555), 0);
    lv_obj_set_style_bg_opa(cancel_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cancel_btn, 8, 0);
    lv_obj_set_style_border_width(cancel_btn, 0, 0);
    lv_obj_set_style_pad_all(cancel_btn, 0, 0);
    lv_obj_clear_flag(cancel_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cancel_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_obj_set_style_text_color(cancel_label, lv_color_white(), 0);
    lv_label_set_text(cancel_label, CANCEL);
    lv_obj_center(cancel_label);
    lv_obj_add_event_cb(cancel_btn, __print_cancel_btn_cb, LV_EVENT_CLICKED, NULL);

    /* Confirm button */
    lv_obj_t *confirm_btn = lv_obj_create(btn_row);
    lv_obj_set_size(confirm_btn, 80, 36);
    lv_obj_set_style_bg_color(confirm_btn, lv_color_hex(0x07C160), 0);
    lv_obj_set_style_bg_opa(confirm_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(confirm_btn, 8, 0);
    lv_obj_set_style_border_width(confirm_btn, 0, 0);
    lv_obj_set_style_pad_all(confirm_btn, 0, 0);
    lv_obj_clear_flag(confirm_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(confirm_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *confirm_label = lv_label_create(confirm_btn);
    lv_obj_set_style_text_color(confirm_label, lv_color_white(), 0);
    lv_label_set_text(confirm_label, CONFIRM_TEXT);
    lv_obj_center(confirm_label);
    lv_obj_add_event_cb(confirm_btn, __print_confirm_btn_cb, LV_EVENT_CLICKED, NULL);
#endif /* ENABLE_PRINTER */
}

static void __create_all_page(lv_obj_t *parent)
{
    lv_coord_t page_w = LV_HOR_RES;
    lv_coord_t page_h = LV_VER_RES;

    sg_album.all_page = lv_obj_create(parent);
    lv_obj_set_size(sg_album.all_page, page_w, page_h);
    lv_obj_set_style_bg_color(sg_album.all_page, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(sg_album.all_page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sg_album.all_page, 0, 0);
    lv_obj_set_style_pad_all(sg_album.all_page, 0, 0);
    lv_obj_set_style_radius(sg_album.all_page, 0, 0);
    lv_obj_set_scrollbar_mode(sg_album.all_page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(sg_album.all_page, LV_OBJ_FLAG_HIDDEN);

    /* Back button in top-left, inset from corner for rounded screens */
    lv_obj_t *back_btn = lv_obj_create(sg_album.all_page);
    lv_obj_set_size(back_btn, 40, 40);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 4 + WECHAT_SAFE_INSET, 4 + WECHAT_SAFE_INSET);
    lv_obj_set_style_bg_color(back_btn, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_50, 0);
    lv_obj_set_style_radius(back_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_pad_all(back_btn, 0, 0);
    lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(back_btn, WECHAT_BTN_EXT_CLICK);

    lv_obj_t *back_img = lv_img_create(back_btn);
    lv_img_set_src(back_img, &icon_back_24_24);
    lv_obj_set_style_img_recolor(back_img, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(back_img, LV_OPA_COVER, 0);
    lv_obj_center(back_img);
    lv_obj_add_event_cb(back_btn, __all_back_btn_cb, LV_EVENT_CLICKED, NULL);

    /* "选择" icon button in top-right, inset from corner for rounded screens */
    sg_album.all_select_btn = __create_icon_btn(sg_album.all_page,
                                                &icon_choose, __all_select_btn_cb);
    lv_obj_align(sg_album.all_select_btn, LV_ALIGN_TOP_RIGHT, -4 - WECHAT_SAFE_INSET, 4 + WECHAT_SAFE_INSET);
    lv_obj_set_ext_click_area(sg_album.all_select_btn, WECHAT_BTN_EXT_CLICK);

    /* Scrollable grid container — fixed position below top bar, never moves */
    sg_album.all_grid = lv_obj_create(sg_album.all_page);
    lv_obj_set_pos(sg_album.all_grid, 0, CTRL_BAR_HEIGHT);
    lv_obj_set_size(sg_album.all_grid, page_w, page_h - CTRL_BAR_HEIGHT);
    lv_obj_set_style_bg_opa(sg_album.all_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sg_album.all_grid, 0, 0);
    lv_obj_set_style_radius(sg_album.all_grid, 0, 0);
    lv_obj_set_style_pad_row(sg_album.all_grid, THUMB_GAP, 0);
    lv_obj_set_style_pad_column(sg_album.all_grid, THUMB_GAP, 0);
    lv_obj_set_style_pad_top(sg_album.all_grid, THUMB_GAP, 0);
    lv_obj_set_style_pad_bottom(sg_album.all_grid, THUMB_GAP, 0);
    /* Center thumbnails: compute equal left/right padding so the grid is symmetric */
    {
        lv_coord_t cols      = (page_w + THUMB_GAP) / (THUMB_SIZE + THUMB_GAP);
        if (cols < 1) cols = 1;
        lv_coord_t content_w = cols * THUMB_SIZE + (cols - 1) * THUMB_GAP;
        lv_coord_t side_pad  = (page_w - content_w) / 2;
        if (side_pad < 0) side_pad = 0;
        lv_obj_set_style_pad_left(sg_album.all_grid, side_pad, 0);
        lv_obj_set_style_pad_right(sg_album.all_grid, side_pad, 0);
    }
    lv_obj_set_flex_flow(sg_album.all_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_scrollbar_mode(sg_album.all_grid, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(sg_album.all_grid, LV_DIR_VER);

    /* Empty state label */
    sg_album.all_empty_label = lv_label_create(sg_album.all_page);
    lv_obj_set_style_text_color(sg_album.all_empty_label, lv_color_hex(0x333333), 0);
    lv_label_set_text(sg_album.all_empty_label, NO_IMAGE);
    lv_obj_center(sg_album.all_empty_label);
    lv_obj_add_flag(sg_album.all_empty_label, LV_OBJ_FLAG_HIDDEN);

    /* Bottom bar for batch select mode (hidden by default) */
    sg_album.all_bottom_bar = lv_obj_create(sg_album.all_page);
    lv_obj_set_size(sg_album.all_bottom_bar, page_w, CTRL_BAR_HEIGHT);
    lv_obj_align(sg_album.all_bottom_bar, LV_ALIGN_BOTTOM_MID, 0, -WECHAT_SAFE_INSET);
    lv_obj_set_style_bg_color(sg_album.all_bottom_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(sg_album.all_bottom_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sg_album.all_bottom_bar, 0, 0);
    lv_obj_set_style_pad_all(sg_album.all_bottom_bar, 4, 0);
    lv_obj_set_style_radius(sg_album.all_bottom_bar, 0, 0);
    lv_obj_set_flex_flow(sg_album.all_bottom_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sg_album.all_bottom_bar, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(sg_album.all_bottom_bar, LV_OBJ_FLAG_HIDDEN);

    /* Delete icon button — centered */
    lv_obj_t *del_btn = lv_obj_create(sg_album.all_bottom_bar);
    lv_obj_remove_style_all(del_btn);
    lv_obj_set_size(del_btn, 40, 40);
    lv_obj_add_flag(del_btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *del_img = lv_img_create(del_btn);
    lv_img_set_src(del_img, &icon_delete);
    lv_obj_set_style_img_recolor(del_img, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(del_img, LV_OPA_COVER, 0);
    lv_obj_center(del_img);
    lv_obj_add_event_cb(del_btn, __all_select_delete_cb, LV_EVENT_CLICKED, NULL);
}

static void __create_select_page(lv_obj_t *parent)
{
    lv_coord_t page_w = LV_HOR_RES;
    lv_coord_t page_h = LV_VER_RES;

    sg_album.select_page = lv_obj_create(parent);
    lv_obj_set_size(sg_album.select_page, page_w, page_h);
    lv_obj_set_style_bg_color(sg_album.select_page, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(sg_album.select_page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sg_album.select_page, 0, 0);
    lv_obj_set_style_pad_all(sg_album.select_page, 0, 0);
    lv_obj_set_style_radius(sg_album.select_page, 0, 0);
    lv_obj_set_scrollbar_mode(sg_album.select_page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(sg_album.select_page, LV_OBJ_FLAG_HIDDEN);

    /* Back button in top-left, inset from corner for rounded screens */
    lv_obj_t *select_back_btn = __create_icon_btn(sg_album.select_page,
                                                  &icon_back_24_24, __select_cancel_btn_cb);
    lv_obj_align(select_back_btn, LV_ALIGN_TOP_LEFT, 4 + WECHAT_SAFE_INSET, 4 + WECHAT_SAFE_INSET);
    lv_obj_set_ext_click_area(select_back_btn, WECHAT_BTN_EXT_CLICK);

    /* Scrollable grid container — fixed position below top bar, never moves */
    sg_album.select_grid = lv_obj_create(sg_album.select_page);
    lv_obj_set_pos(sg_album.select_grid, 0, CTRL_BAR_HEIGHT);
    lv_obj_set_size(sg_album.select_grid, page_w, page_h - CTRL_BAR_HEIGHT);
    lv_obj_set_style_bg_opa(sg_album.select_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sg_album.select_grid, 0, 0);
    lv_obj_set_style_radius(sg_album.select_grid, 0, 0);
    lv_obj_set_style_pad_row(sg_album.select_grid, THUMB_GAP, 0);
    lv_obj_set_style_pad_column(sg_album.select_grid, THUMB_GAP, 0);
    lv_obj_set_style_pad_top(sg_album.select_grid, THUMB_GAP, 0);
    lv_obj_set_style_pad_bottom(sg_album.select_grid, THUMB_GAP, 0);
    /* Center thumbnails: compute equal left/right padding so the grid is symmetric */
    {
        lv_coord_t cols      = (page_w + THUMB_GAP) / (THUMB_SIZE + THUMB_GAP);
        if (cols < 1) cols = 1;
        lv_coord_t content_w = cols * THUMB_SIZE + (cols - 1) * THUMB_GAP;
        lv_coord_t side_pad  = (page_w - content_w) / 2;
        if (side_pad < 0) side_pad = 0;
        lv_obj_set_style_pad_left(sg_album.select_grid, side_pad, 0);
        lv_obj_set_style_pad_right(sg_album.select_grid, side_pad, 0);
    }
    lv_obj_set_flex_flow(sg_album.select_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_scrollbar_mode(sg_album.select_grid, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(sg_album.select_grid, LV_DIR_VER);

    /* Empty state label */
    sg_album.select_empty_label = lv_label_create(sg_album.select_page);
    lv_obj_set_style_text_color(sg_album.select_empty_label, lv_color_hex(0x333333), 0);
    lv_label_set_text(sg_album.select_empty_label, NO_IMAGE);
    lv_obj_center(sg_album.select_empty_label);
    lv_obj_add_flag(sg_album.select_empty_label, LV_OBJ_FLAG_HIDDEN);
}

/* ── album display callbacks ── */

static void __disp_open(void)
{
    lv_vendor_disp_lock();
    lv_obj_clear_flag(sg_album.view_page, LV_OBJ_FLAG_HIDDEN);
    lv_vendor_disp_unlock();
}

static void __disp_image(AI_UI_IMG_T *img)
{
    if (NULL == img || NULL == img->data || 0 == img->len) {
        /* No image to display (album empty after delete) — show empty state in-place */
        sg_album.view_current_name[0] = '\0';
        lv_vendor_disp_lock();
        lv_obj_add_flag(sg_album.view_canvas, LV_OBJ_FLAG_HIDDEN);
        if (sg_album.view_buf) {
            Free(sg_album.view_buf);
            sg_album.view_buf = NULL;
        }
        lv_obj_clear_flag(sg_album.view_empty_label, LV_OBJ_FLAG_HIDDEN);
#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
        lv_obj_add_flag(sg_album.print_overlay, LV_OBJ_FLAG_HIDDEN);
#endif
        lv_vendor_disp_unlock();
        return;
    }

    /* Record current image name for delete */
    if (img->name) {
        strncpy(sg_album.view_current_name, img->name, ALBUM_FILENAME_MAX_LEN);
        sg_album.view_current_name[ALBUM_FILENAME_MAX_LEN] = '\0';
    } else {
        sg_album.view_current_name[0] = '\0';
    }

    TAL_IMAGE_JPEG_INFO_T info = {0};
    OPERATE_RET rt = tal_image_jpeg_get_info(img->data, img->len, &info);
    if (rt != OPRT_OK) {
        PR_ERR("jpeg get info failed, rt:%d", rt);
        return;
    }

    /* Decode JPEG at original size */
    uint32_t buf_size = (uint32_t)info.width * info.height * 2;
    uint8_t *rgb565_buf = (uint8_t *)Malloc(buf_size);
    if (NULL == rgb565_buf) {
        PR_ERR("malloc decode buf failed, size:%u", buf_size);
        return;
    }

    TAL_IMAGE_JPEG_OUTPUT_T out = {0};
    out.out_buf      = rgb565_buf;
    out.out_buf_size = buf_size;
    out.out_width    = info.width;
    out.out_height   = info.height;

    rt = tal_image_jpeg_decode_rgb565(img->data, img->len, &out);
    if (rt != OPRT_OK) {
        PR_ERR("jpeg decode failed, rt:%d", rt);
        Free(rgb565_buf);
        return;
    }

    lv_vendor_disp_lock();

    /* Free previous buffer */
    if (sg_album.view_buf) {
        Free(sg_album.view_buf);
        sg_album.view_buf = NULL;
    }

    sg_album.view_buf = rgb565_buf;
    lv_canvas_set_buffer(sg_album.view_canvas, sg_album.view_buf,
                         info.width, info.height, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(sg_album.view_canvas, info.width, info.height);
    lv_obj_center(sg_album.view_canvas);
    lv_obj_clear_flag(sg_album.view_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_album.view_empty_label, LV_OBJ_FLAG_HIDDEN);
#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    lv_obj_add_flag(sg_album.print_overlay, LV_OBJ_FLAG_HIDDEN);
#endif

    lv_vendor_disp_unlock();
}

static void __disp_all_img_thumb_list(AI_UI_IMG_T *item_arr, uint32_t arr_cnt)
{
    lv_vendor_disp_lock();

    /* Show all page, hide others */
    lv_obj_clear_flag(sg_album.all_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_album.view_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_album.select_page, LV_OBJ_FLAG_HIDDEN);

    /* Restore normal mode UI (select/cancel are local-only, delete triggers rebuild) */
    lv_obj_clear_flag(sg_album.all_select_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_album.all_bottom_bar, LV_OBJ_FLAG_HIDDEN);
    sg_album.all_select_count = 0;

    /* Clear old grid children */
    lv_obj_clean(sg_album.all_grid);

    if (NULL == item_arr || 0 == arr_cnt) {
        lv_obj_clear_flag(sg_album.all_empty_label, LV_OBJ_FLAG_HIDDEN);
        lv_vendor_disp_unlock();
        return;
    }

    lv_obj_add_flag(sg_album.all_empty_label, LV_OBJ_FLAG_HIDDEN);

    /* Create thumbnail items */
    uint32_t i;
    for (i = 0; i < arr_cnt; i++) {
        AI_UI_IMG_T *item = &item_arr[i];

        lv_obj_t *container = lv_obj_create(sg_album.all_grid);
        lv_obj_set_size(container, THUMB_SIZE, THUMB_SIZE);
        lv_obj_set_style_pad_all(container, 0, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_radius(container, 2, 0);
        lv_obj_set_style_bg_color(container, lv_color_black(), 0);
        lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_user_data(container, (void *)item->name);

        if (item->data != NULL && item->width > 0 && item->height > 0) {
            lv_obj_t *canvas = lv_canvas_create(container);
            lv_canvas_set_buffer(canvas, item->data,
                                 item->width, item->height,
                                 LV_COLOR_FORMAT_RGB565);
            lv_obj_center(canvas);
        }

        lv_obj_add_flag(container, LV_OBJ_FLAG_CLICKABLE);

        /* Circle indicator — always created, hidden until select mode is entered */
        lv_obj_t *circle = lv_obj_create(container);
        lv_obj_remove_style_all(circle);
        lv_obj_set_size(circle, 22, 22);
        lv_obj_align(circle, LV_ALIGN_TOP_LEFT, 3, 3);
        lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(circle, lv_color_white(), 0);
        lv_obj_set_style_border_width(circle, 2, 0);
        lv_obj_set_style_border_opa(circle, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_opa(circle, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(circle, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(circle, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_event_cb(container, __all_item_toggle_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_vendor_disp_unlock();
}

static void __disp_select_img_thumb_list(AI_UI_IMG_T *item_arr, uint32_t arr_cnt,
                                         uint8_t select_num_max)
{
    (void)select_num_max;

    lv_vendor_disp_lock();

    /* Show select page, hide others */
    lv_obj_clear_flag(sg_album.select_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_album.view_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_album.all_page, LV_OBJ_FLAG_HIDDEN);

    /* Clear old grid children */
    lv_obj_clean(sg_album.select_grid);

    if (NULL == item_arr || 0 == arr_cnt) {
        lv_obj_clear_flag(sg_album.select_empty_label, LV_OBJ_FLAG_HIDDEN);
        lv_vendor_disp_unlock();
        return;
    }

    lv_obj_add_flag(sg_album.select_empty_label, LV_OBJ_FLAG_HIDDEN);

    /* Create thumbnail items — single-click to select and close */
    uint32_t i;
    for (i = 0; i < arr_cnt; i++) {
        AI_UI_IMG_T *item = &item_arr[i];

        lv_obj_t *container = lv_obj_create(sg_album.select_grid);
        lv_obj_set_size(container, THUMB_SIZE, THUMB_SIZE);
        lv_obj_set_style_pad_all(container, 0, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_radius(container, 2, 0);
        lv_obj_set_style_bg_color(container, lv_color_black(), 0);
        lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_user_data(container, (void *)item->name);

        if (item->data != NULL && item->width > 0 && item->height > 0) {
            lv_obj_t *canvas = lv_canvas_create(container);
            lv_canvas_set_buffer(canvas, item->data,
                                 item->width, item->height,
                                 LV_COLOR_FORMAT_RGB565);
            lv_obj_center(canvas);
        }

        /* Tap to select as attachment and auto-close */
        lv_obj_add_flag(container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(container, __select_item_click_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_vendor_disp_unlock();
}

static void __disp_close(void)
{
    lv_vendor_disp_lock();

    lv_obj_add_flag(sg_album.view_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_album.all_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_album.select_page, LV_OBJ_FLAG_HIDDEN);

    /* Reset select mode */
    sg_album.all_select_mode = false;

    /* Free view buffer */
    if (sg_album.view_buf) {
        Free(sg_album.view_buf);
        sg_album.view_buf = NULL;
    }

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    lv_obj_add_flag(sg_album.print_overlay, LV_OBJ_FLAG_HIDDEN);
#endif

    lv_vendor_disp_unlock();
}

/* ── public API ── */

/**
 * @brief Initialize album sub-pages. Called once during wechat UI init
 *        (inside lv_vendor_disp_lock).
 *
 * @param parent  Parent LVGL object (main container).
 */
void ai_ui_wechat_album_init(lv_obj_t *parent)
{
    memset(&sg_album, 0, sizeof(AI_UI_WECHAT_ALBUM_T));

    __create_view_page(parent);
    __create_all_page(parent);
    __create_select_page(parent);
}

/**
 * @brief Register album display callbacks into ai_ui_image_album.
 */
void ai_ui_wechat_album_register(void)
{
    AI_UI_ALBUM_INTFS_T intfs;
    memset(&intfs, 0, sizeof(AI_UI_ALBUM_INTFS_T));

    intfs.disp_open                   = __disp_open;
    intfs.disp_image                  = __disp_image;
    intfs.disp_all_img_thumb_list     = __disp_all_img_thumb_list;
    intfs.disp_select_img_thumb_list  = __disp_select_img_thumb_list;
    intfs.disp_close                  = __disp_close;
#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    intfs.disp_print_result           = __disp_print_result;
#endif

    ai_ui_image_album_register(&intfs);
}

#endif
