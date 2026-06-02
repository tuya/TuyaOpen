
/**
 * @file example_ui.c
 * @brief example_ui module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "lvgl.h"
#include "lv_vendor.h"
#include "example_ui.h"
#include "tal_image.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define SHUTTER_BTN_RADIUS   30
#define BACK_BTN_SIZE        40
#define ALBUM_BTN_SIZE       52  /**< Larger touch target for delete / grid-toggle buttons */

/* Thumbnail grid layout */
#define GRID_THUMB_GAP       4   /**< Gap between cells and between cells and edge (px) */

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    lv_obj_t *home;
    lv_obj_t *camera;
    lv_obj_t *album;

    lv_obj_t *camera_canvas;
    lv_obj_t *lbl_camera_status;

    lv_obj_t *lbl_album_info;
    lv_obj_t *album_viewer;
    lv_obj_t *btn_delete;

    /* Thumbnail grid — lives on the album screen, hidden by default */
    lv_obj_t *grid_container;   /**< Scrollable flex-wrap container */
    lv_obj_t *btn_grid_toggle;  /**< Switches between viewer and grid mode */
    lv_obj_t *lbl_grid_toggle;  /**< Label inside btn_grid_toggle (icon text) */
}EXAMPLE_UI_OBJ_T;

typedef struct {
    UI_ACTION_EVENT_E action;
    void             *data;
} UI_ACTION_MSG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static EXAMPLE_UI_OBJ_T     sg_ui_obj;
static EXAMPLE_UI_ACTION_CB sg_action_cb;
static QUEUE_HANDLE         sg_action_queue_hdl;
static THREAD_HANDLE        sg_action_thrd_hdl;

static uint8_t             *sg_camera_canvas_buf = NULL;

static lv_image_dsc_t       sg_viewer_dsc;
static uint8_t             *sg_viewer_rgb565_buf = NULL;
static uint32_t             sg_album_img_count = 0;
/***********************************************************
***********************function define**********************
***********************************************************/
static void __ui_notify_action(UI_ACTION_EVENT_E action, void *data);


/**
 * @brief "Take Photo" button click on home screen
 * @param[in] e LVGL event
 * @return none
 */
static void __btn_camera_cb(lv_event_t *e)
{
    (void)e;
    __ui_notify_action(UI_ACTION_OPEN_CAMERA, NULL);
}

/**
 * @brief "View Album" button click on home screen
 * @param[in] e LVGL event
 * @return none
 */
static void __btn_album_cb(lv_event_t *e)
{
    (void)e;
    __ui_notify_action(UI_ACTION_OPEN_ALBUM, NULL);
}

/**
 * @brief Shutter button click on camera screen
 * @param[in] e LVGL event
 * @return none
 */
static void __btn_shutter_cb(lv_event_t *e)
{
    (void)e;
    __ui_notify_action(UI_ACTION_TAKE_PHOTO, NULL);
}

/**
 * @brief Delete button click on album screen
 * @param[in] e LVGL event
 * @return none
 */
static void __btn_delete_cb(lv_event_t *e)
{
    (void)e;
    __ui_notify_action(UI_ACTION_DELETE_PHOTO, NULL);
}

/**
 * @brief Grid-toggle button click on album screen
 * @param[in] e LVGL event
 * @return none
 */
static void __btn_grid_toggle_cb(lv_event_t *e)
{
    (void)e;
    __ui_notify_action(UI_ACTION_TOGGLE_GRID, NULL);
}

/**
 * @param[in] e LVGL event
 * @return none
 */
static void __btn_back_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    lv_obj_t *parent = (lv_obj_t *)lv_obj_get_user_data(target);

    if (parent == sg_ui_obj.camera) {
        __ui_notify_action(UI_ACTION_CLOSE_CAMERA, NULL);
    } else {
        __ui_notify_action(UI_ACTION_CLOSE_ALBUM, NULL);
    }
}

/**
 * @brief Gesture handler for album screen: swipe left = next, swipe right = prev
 * @param[in] e LVGL event
 * @return none
 */
static void __album_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());

    if (dir == LV_DIR_LEFT) {
        __ui_notify_action(UI_ACTION_VIEW_NEXT_PHOTO, NULL);
    } else if (dir == LV_DIR_RIGHT) {
        __ui_notify_action(UI_ACTION_VIEW_PREV_PHOTO, NULL);
    }
}

/**
 * @brief Thumbnail cell click: fire UI_ACTION_SELECT_PHOTO with the 0-based index.
 */
static void __thumb_click_cb(lv_event_t *e)
{
    uintptr_t idx = (uintptr_t)lv_event_get_user_data(e);
    __ui_notify_action(UI_ACTION_SELECT_PHOTO, (void *)idx);
}

/**
 * @brief Cell delete event: free the lv_image_dsc_t and its pixel buffer that
 *        were allocated in example_ui_grid_add_thumb().
 */
static void __cell_delete_cb(lv_event_t *e)
{
    lv_obj_t *cell = lv_event_get_target(e);
    lv_image_dsc_t *dsc = (lv_image_dsc_t *)lv_obj_get_user_data(cell);
    if (dsc != NULL) {
        if (dsc->data != NULL) {
            Free((void *)dsc->data);
        }
        Free(dsc);
        lv_obj_set_user_data(cell, NULL);
    }
}

static lv_obj_t *__create_back_btn(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, BACK_BTN_SIZE, BACK_BTN_SIZE);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 6, 6);
    lv_obj_set_style_radius(btn, BACK_BTN_SIZE / 2, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_80, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);

    lv_obj_set_user_data(btn, (void *)parent);
    lv_obj_add_event_cb(btn, __btn_back_cb, LV_EVENT_CLICKED, NULL);

    return btn;
}

static void __create_home_screen(void)
{
    sg_ui_obj.home = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(sg_ui_obj.home, lv_color_hex(0x1A1A2E), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(sg_ui_obj.home);
    lv_label_set_text(title, LV_SYMBOL_IMAGE " Photo Album");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, LV_VER_RES / 8);

    lv_coord_t btn_w = LV_HOR_RES * 2 / 3;
    lv_coord_t btn_h = LV_VER_RES / 6;

    lv_obj_t *btn_camera = lv_btn_create(sg_ui_obj.home);
    lv_obj_set_size(btn_camera, btn_w, btn_h);
    lv_obj_align(btn_camera, LV_ALIGN_CENTER, 0, -btn_h / 2 - 10);
    lv_obj_set_style_bg_color(btn_camera, lv_color_hex(0xE94560), 0);
    lv_obj_set_style_radius(btn_camera, 12, 0);
    lv_obj_set_style_shadow_width(btn_camera, 8, 0);
    lv_obj_set_style_shadow_color(btn_camera, lv_color_hex(0xE94560), 0);
    lv_obj_set_style_shadow_opa(btn_camera, LV_OPA_40, 0);

    lv_obj_t *lbl_camera = lv_label_create(btn_camera);
    lv_label_set_text(lbl_camera, LV_SYMBOL_IMAGE "  Take Photo");
    lv_obj_set_style_text_color(lbl_camera, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_camera, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_camera);

    lv_obj_add_event_cb(btn_camera, __btn_camera_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_album = lv_btn_create(sg_ui_obj.home);
    lv_obj_set_size(btn_album, btn_w, btn_h);
    lv_obj_align(btn_album, LV_ALIGN_CENTER, 0, btn_h / 2 + 10);
    lv_obj_set_style_bg_color(btn_album, lv_color_hex(0x0F3460), 0);
    lv_obj_set_style_radius(btn_album, 12, 0);
    lv_obj_set_style_shadow_width(btn_album, 8, 0);
    lv_obj_set_style_shadow_color(btn_album, lv_color_hex(0x0F3460), 0);
    lv_obj_set_style_shadow_opa(btn_album, LV_OPA_40, 0);

    lv_obj_t *lbl_album = lv_label_create(btn_album);
    lv_label_set_text(lbl_album, LV_SYMBOL_DIRECTORY "  View Album");
    lv_obj_set_style_text_color(lbl_album, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_album, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_album);

    lv_obj_add_event_cb(btn_album, __btn_album_cb, LV_EVENT_CLICKED, NULL);
}

static void __create_camera_screen(void)
{
    sg_ui_obj.camera = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(sg_ui_obj.camera, lv_color_hex(0x000000), LV_PART_MAIN);

    sg_ui_obj.camera_canvas = lv_canvas_create(sg_ui_obj.camera);
    lv_obj_set_size(sg_ui_obj.camera_canvas, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(sg_ui_obj.camera_canvas, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(sg_ui_obj.camera_canvas, LV_OBJ_FLAG_HIDDEN);

    __create_back_btn(sg_ui_obj.camera);

    lv_obj_t *btn_shutter = lv_btn_create(sg_ui_obj.camera);
    lv_obj_set_size(btn_shutter, SHUTTER_BTN_RADIUS * 2, SHUTTER_BTN_RADIUS * 2);
    lv_obj_align(btn_shutter, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_radius(btn_shutter, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn_shutter, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_color(btn_shutter, lv_color_hex(0xCC0000), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn_shutter, lv_color_white(), 0);
    lv_obj_set_style_border_width(btn_shutter, 3, 0);
    lv_obj_set_style_shadow_width(btn_shutter, 12, 0);
    lv_obj_set_style_shadow_color(btn_shutter, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_shadow_opa(btn_shutter, LV_OPA_50, 0);

    lv_obj_t *lbl_shutter = lv_label_create(btn_shutter);
    lv_label_set_text(lbl_shutter, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(lbl_shutter, lv_color_white(), 0);
    lv_obj_center(lbl_shutter);

    lv_obj_add_event_cb(btn_shutter, __btn_shutter_cb, LV_EVENT_CLICKED, NULL);

    sg_ui_obj.lbl_camera_status = lv_label_create(sg_ui_obj.camera);
    lv_obj_set_style_text_color(sg_ui_obj.lbl_camera_status, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_bg_color(sg_ui_obj.lbl_camera_status, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(sg_ui_obj.lbl_camera_status, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(sg_ui_obj.lbl_camera_status, 4, 0);
    lv_label_set_text(sg_ui_obj.lbl_camera_status, "");
    lv_obj_align(sg_ui_obj.lbl_camera_status, LV_ALIGN_TOP_MID, 0, 6);
}

static void __create_album_screen(void)
{
    sg_ui_obj.album = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(sg_ui_obj.album, lv_color_hex(0x000000), LV_PART_MAIN);

    /* ---- Full-screen photo viewer (shown by default) ---- */
    sg_ui_obj.album_viewer = lv_image_create(sg_ui_obj.album);
    lv_obj_set_size(sg_ui_obj.album_viewer, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(sg_ui_obj.album_viewer, 0, 0);
    lv_obj_add_flag(sg_ui_obj.album_viewer, LV_OBJ_FLAG_HIDDEN);

    /* ---- Thumbnail grid container (hidden by default) ---- */
    sg_ui_obj.grid_container = lv_obj_create(sg_ui_obj.album);
    lv_obj_set_size(sg_ui_obj.grid_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(sg_ui_obj.grid_container, 0, 0);
    lv_obj_set_style_bg_color(sg_ui_obj.grid_container, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_border_width(sg_ui_obj.grid_container, 0, 0);
    /* Pad top so cells don't go under the back button; pad bottom for info label */
    lv_obj_set_style_pad_top(sg_ui_obj.grid_container, BACK_BTN_SIZE + 12, 0);
    lv_obj_set_style_pad_bottom(sg_ui_obj.grid_container, BACK_BTN_SIZE + 12, 0);
    lv_obj_set_style_pad_left(sg_ui_obj.grid_container, GRID_THUMB_GAP, 0);
    lv_obj_set_style_pad_right(sg_ui_obj.grid_container, GRID_THUMB_GAP, 0);
    lv_obj_set_style_pad_row(sg_ui_obj.grid_container, GRID_THUMB_GAP, 0);
    lv_obj_set_style_pad_column(sg_ui_obj.grid_container, GRID_THUMB_GAP, 0);
    lv_obj_set_flex_flow(sg_ui_obj.grid_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_add_flag(sg_ui_obj.grid_container, LV_OBJ_FLAG_HIDDEN);

    /* ---- Swipe gestures (on the album background, for viewer navigation) ---- */
    lv_obj_add_event_cb(sg_ui_obj.album, __album_gesture_cb, LV_EVENT_GESTURE, NULL);

    /* ---- Overlay buttons and labels (rendered above viewer and grid) ---- */
    __create_back_btn(sg_ui_obj.album);

    sg_ui_obj.btn_delete = lv_btn_create(sg_ui_obj.album);
    lv_obj_set_size(sg_ui_obj.btn_delete, ALBUM_BTN_SIZE, ALBUM_BTN_SIZE);
    lv_obj_align(sg_ui_obj.btn_delete, LV_ALIGN_BOTTOM_RIGHT, -6, -6);
    lv_obj_set_style_radius(sg_ui_obj.btn_delete, ALBUM_BTN_SIZE / 2, 0);
    lv_obj_set_style_bg_color(sg_ui_obj.btn_delete, lv_color_hex(0xCC2222), 0);
    lv_obj_set_style_bg_opa(sg_ui_obj.btn_delete, LV_OPA_80, 0);
    lv_obj_set_style_shadow_width(sg_ui_obj.btn_delete, 0, 0);
    lv_obj_add_flag(sg_ui_obj.btn_delete, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *lbl_del = lv_label_create(sg_ui_obj.btn_delete);
    lv_label_set_text(lbl_del, LV_SYMBOL_TRASH);
    lv_obj_set_style_text_color(lbl_del, lv_color_white(), 0);
    lv_obj_center(lbl_del);

    lv_obj_add_event_cb(sg_ui_obj.btn_delete, __btn_delete_cb, LV_EVENT_CLICKED, NULL);

    /* View-toggle button: top-right corner */
    sg_ui_obj.btn_grid_toggle = lv_btn_create(sg_ui_obj.album);
    lv_obj_set_size(sg_ui_obj.btn_grid_toggle, ALBUM_BTN_SIZE, ALBUM_BTN_SIZE);
    lv_obj_align(sg_ui_obj.btn_grid_toggle, LV_ALIGN_TOP_RIGHT, -6, 6);
    lv_obj_set_style_radius(sg_ui_obj.btn_grid_toggle, ALBUM_BTN_SIZE / 2, 0);
    lv_obj_set_style_bg_color(sg_ui_obj.btn_grid_toggle, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(sg_ui_obj.btn_grid_toggle, LV_OPA_80, 0);
    lv_obj_set_style_shadow_width(sg_ui_obj.btn_grid_toggle, 0, 0);

    sg_ui_obj.lbl_grid_toggle = lv_label_create(sg_ui_obj.btn_grid_toggle);
    /* LV_SYMBOL_LIST = show-grid icon; LV_SYMBOL_IMAGE = show-viewer icon */
    lv_label_set_text(sg_ui_obj.lbl_grid_toggle, LV_SYMBOL_LIST);
    lv_obj_set_style_text_color(sg_ui_obj.lbl_grid_toggle, lv_color_white(), 0);
    lv_obj_center(sg_ui_obj.lbl_grid_toggle);

    lv_obj_add_event_cb(sg_ui_obj.btn_grid_toggle, __btn_grid_toggle_cb, LV_EVENT_CLICKED, NULL);

    sg_ui_obj.lbl_album_info = lv_label_create(sg_ui_obj.album);
    lv_obj_set_style_text_color(sg_ui_obj.lbl_album_info, lv_color_white(), 0);
    lv_obj_set_style_bg_color(sg_ui_obj.lbl_album_info, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(sg_ui_obj.lbl_album_info, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(sg_ui_obj.lbl_album_info, 4, 0);
    lv_label_set_text(sg_ui_obj.lbl_album_info, "0 / 0");
    lv_obj_align(sg_ui_obj.lbl_album_info, LV_ALIGN_BOTTOM_MID, 0, -6);
}

static void __ui_aciton_task(void *args)
{
    UI_ACTION_MSG_T msg_data = {0};

    (void)args;

    for (;;) {
        memset(&msg_data, 0, sizeof(UI_ACTION_MSG_T));
        tal_queue_fetch(sg_action_queue_hdl, &msg_data, SEM_WAIT_FOREVER);

        if (sg_action_cb != NULL) {
            sg_action_cb(msg_data.action, msg_data.data);
        }
    }
}

static void __ui_notify_action(UI_ACTION_EVENT_E action, void *data)
{
    UI_ACTION_MSG_T msg_data;

    if (sg_action_cb == NULL) {
        return;
    }

    memset(&msg_data, 0, sizeof(UI_ACTION_MSG_T));
    msg_data.action = action;
    msg_data.data = data;

    tal_queue_post(sg_action_queue_hdl, &msg_data, SEM_WAIT_FOREVER);
}


OPERATE_RET example_ui_init(EXAMPLE_UI_ACTION_CB action_cb)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tal_queue_create_init(&sg_action_queue_hdl, sizeof(UI_ACTION_MSG_T), 8));

    THREAD_CFG_T thrd_cfg = {0};

    thrd_cfg.stackDepth = 1024 * 4;
    thrd_cfg.priority   = THREAD_PRIO_2;
    thrd_cfg.thrdname   = "ui_action";
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_action_thrd_hdl, NULL, NULL,
                                                     __ui_aciton_task, NULL, &thrd_cfg));

    sg_action_cb = action_cb;

    lv_vendor_init(DISPLAY_NAME);
    lv_vendor_start(THREAD_PRIO_1, 1024 * 8);

    lv_vendor_disp_lock();

    __create_home_screen();
    __create_camera_screen();
    __create_album_screen();

    lv_screen_load(sg_ui_obj.home);

    lv_vendor_disp_unlock();

    return OPRT_OK;
}

void example_ui_video_start(uint16_t width, uint16_t height)
{
    uint32_t buf_size = (uint32_t)width * height * 2;

    lv_vendor_disp_lock();

    sg_camera_canvas_buf = (uint8_t *)Malloc(buf_size);
    if (sg_camera_canvas_buf != NULL) {
        memset(sg_camera_canvas_buf, 0, buf_size);
        lv_canvas_set_buffer( sg_ui_obj.camera_canvas, sg_camera_canvas_buf, width, height,
                             LV_COLOR_FORMAT_RGB565);
        lv_obj_clear_flag(sg_ui_obj.camera_canvas, LV_OBJ_FLAG_HIDDEN);
    }else {
        PR_ERR("malloc failed");
        lv_vendor_disp_unlock();
        return;
    }

    lv_obj_add_flag(sg_ui_obj.lbl_camera_status, LV_OBJ_FLAG_HIDDEN);
    lv_screen_load(sg_ui_obj.camera);

    lv_vendor_disp_unlock();
}

void example_ui_video_flush(uint16_t width, uint16_t height, uint8_t *data)
{
    if(NULL == sg_camera_canvas_buf) {
        PR_ERR("open video first");
        return;
    }

    TAL_IMAGE_YUV422_TO_RGB_T conv_cfg = {
        .in_buf     = data,
        .in_width   = width,
        .in_height  = height,
        .out_buf    = sg_camera_canvas_buf,
        .out_width  = width,
        .out_height = height,
    };

    tal_image_convert_yuv422_to_rgb565(&conv_cfg);

    lv_vendor_disp_lock();

    lv_area_t inv_area;
    lv_area_set(&inv_area, 0, 0, (int32_t)width - 1, (int32_t)height - 1);
    lv_obj_invalidate_area(sg_ui_obj.camera_canvas, &inv_area);

    lv_vendor_disp_unlock();
}

void example_ui_video_status(char *string)
{
    lv_vendor_disp_lock();
    lv_obj_clear_flag(sg_ui_obj.lbl_camera_status, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(sg_ui_obj.lbl_camera_status, string);
    lv_vendor_disp_unlock();
}


void example_ui_video_end(void)
{
    lv_vendor_disp_lock();

    lv_screen_load(sg_ui_obj.home);

    lv_obj_add_flag(sg_ui_obj.camera_canvas, LV_OBJ_FLAG_HIDDEN);

    if(sg_camera_canvas_buf) {
        Free(sg_camera_canvas_buf);
        sg_camera_canvas_buf = NULL;
    }

    lv_vendor_disp_unlock();

}

void example_ui_view_album_start(uint32_t count)
{
    sg_album_img_count = count;

    lv_vendor_disp_lock();

    char info_buf[32];
    snprintf(info_buf, sizeof(info_buf), "0 / %u", sg_album_img_count);
    lv_label_set_text(sg_ui_obj.lbl_album_info, info_buf);
    lv_screen_load(sg_ui_obj.album);

    lv_vendor_disp_unlock();
}

void example_ui_view_album_update_count(uint32_t count)
{
    sg_album_img_count = count;
}

void example_ui_view_photo(uint16_t width, uint16_t height, uint8_t *jpeg, uint32_t len, uint32_t idx)
{
    uint32_t rgb_size = (uint32_t)width * height * 2;
    uint8_t *rgb_buf = (uint8_t *)Malloc(rgb_size);
    if (rgb_buf == NULL) {
        return;
    }

    TAL_IMAGE_JPEG_OUTPUT_T decode_out = {
        .out_buf      = rgb_buf,
        .out_buf_size = rgb_size,
        .out_width    = width,
        .out_height   = height,
    };

    OPERATE_RET rt = tal_image_jpeg_decode_rgb565(jpeg, len, &decode_out);
    if (rt != OPRT_OK) {
        Free(rgb_buf);
        return ;
    }

    if(sg_viewer_rgb565_buf) {
        Free(sg_viewer_rgb565_buf);
    }

    sg_viewer_rgb565_buf = rgb_buf;

    lv_vendor_disp_lock();

    memset(&sg_viewer_dsc, 0, sizeof(sg_viewer_dsc));
    sg_viewer_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    sg_viewer_dsc.header.w  = width;
    sg_viewer_dsc.header.h  = height;
    sg_viewer_dsc.data      = sg_viewer_rgb565_buf;
    sg_viewer_dsc.data_size = (uint32_t)width * height * 2;

    lv_image_set_src(sg_ui_obj.album_viewer, &sg_viewer_dsc);
    lv_obj_set_size(sg_ui_obj.album_viewer, LV_HOR_RES, LV_VER_RES);
    lv_image_set_inner_align(sg_ui_obj.album_viewer, LV_IMAGE_ALIGN_CENTER);

    /* Make sure viewer is visible and grid is hidden */
    lv_obj_clear_flag(sg_ui_obj.album_viewer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_ui_obj.grid_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui_obj.btn_delete, LV_OBJ_FLAG_HIDDEN);

    /* Toggle button shows "switch to grid" icon */
    lv_label_set_text(sg_ui_obj.lbl_grid_toggle, LV_SYMBOL_LIST);

    char info_buf[32];
    snprintf(info_buf, sizeof(info_buf), "%u / %u", idx, sg_album_img_count);
    lv_label_set_text(sg_ui_obj.lbl_album_info, info_buf);

    lv_vendor_disp_unlock();

}

void example_ui_view_album_end(void)
{
    lv_vendor_disp_lock();

    lv_obj_add_flag(sg_ui_obj.album_viewer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_ui_obj.btn_delete, LV_OBJ_FLAG_HIDDEN);

    /* Clean grid cells (fires LV_EVENT_DELETE on each, freeing pixel buffers) */
    lv_obj_add_flag(sg_ui_obj.grid_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(sg_ui_obj.grid_container);

    lv_screen_load(sg_ui_obj.home);

    if(sg_viewer_rgb565_buf) {
        Free(sg_viewer_rgb565_buf);
        sg_viewer_rgb565_buf = NULL;
    }

    lv_vendor_disp_unlock();
}

/* -----------------------------------------------------------------------
 * Thumbnail grid API
 * --------------------------------------------------------------------- */

void example_ui_grid_show(uint32_t count)
{
    lv_vendor_disp_lock();

    /* Remove cells from any previous grid session */
    lv_obj_clean(sg_ui_obj.grid_container);

    char info_buf[32];
    snprintf(info_buf, sizeof(info_buf), "%u photos", count);
    lv_label_set_text(sg_ui_obj.lbl_album_info, info_buf);

    /* Toggle button shows "switch to viewer" icon */
    lv_label_set_text(sg_ui_obj.lbl_grid_toggle, LV_SYMBOL_IMAGE);

    lv_obj_add_flag(sg_ui_obj.album_viewer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_ui_obj.btn_delete, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui_obj.grid_container, LV_OBJ_FLAG_HIDDEN);

    lv_vendor_disp_unlock();
}

void example_ui_grid_add_thumb(uint32_t idx, uint16_t w, uint16_t h,
                                const uint8_t *rgb565, uint32_t size)
{
    /* Copy pixel data so the caller can free its batch immediately */
    uint8_t *buf_copy = (uint8_t *)Malloc(size);
    if (buf_copy == NULL) {
        PR_ERR("grid_add_thumb: malloc failed (idx=%u)", idx);
        return;
    }
    memcpy(buf_copy, rgb565, size);

    lv_image_dsc_t *dsc = (lv_image_dsc_t *)Malloc(sizeof(lv_image_dsc_t));
    if (dsc == NULL) {
        Free(buf_copy);
        return;
    }
    memset(dsc, 0, sizeof(lv_image_dsc_t));
    dsc->header.cf = LV_COLOR_FORMAT_RGB565;
    dsc->header.w  = w;
    dsc->header.h  = h;
    dsc->data      = buf_copy;
    dsc->data_size = size;

    lv_vendor_disp_lock();

    lv_obj_t *cell = lv_obj_create(sg_ui_obj.grid_container);
    lv_obj_set_size(cell, w + GRID_THUMB_GAP, h + GRID_THUMB_GAP);
    lv_obj_set_style_bg_color(cell, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_border_width(cell, 0, 0);
    lv_obj_set_style_radius(cell, 4, 0);
    lv_obj_set_style_pad_all(cell, 2, 0);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    /* Store dsc pointer so __cell_delete_cb can free it */
    lv_obj_set_user_data(cell, (void *)dsc);
    lv_obj_add_event_cb(cell, __cell_delete_cb, LV_EVENT_DELETE, NULL);
    lv_obj_add_event_cb(cell, __thumb_click_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)idx);

    lv_obj_t *img = lv_image_create(cell);
    lv_image_set_src(img, dsc);
    lv_obj_center(img);

    lv_vendor_disp_unlock();
}

void example_ui_show_viewer(void)
{
    lv_vendor_disp_lock();

    lv_obj_add_flag(sg_ui_obj.grid_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui_obj.album_viewer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_ui_obj.btn_delete, LV_OBJ_FLAG_HIDDEN);

    /* Toggle button shows "switch to grid" icon */
    lv_label_set_text(sg_ui_obj.lbl_grid_toggle, LV_SYMBOL_LIST);

    lv_vendor_disp_unlock();
}
