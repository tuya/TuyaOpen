/**
 * @file ai_ui_wechat_camera.c
 * @brief WeChat-style camera page — LVGL preview, shutter, thumbnail and close controls.
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)

#include "lvgl.h"
#include "lv_vendor.h"

#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"
#include "font_awesome_symbols.h"
#include "ai_ui_wechat_common.h"
#include "tal_image.h"

LV_IMG_DECLARE(icon_back_24_24);
LV_IMG_DECLARE(icon_photo_app);
LV_IMG_DECLARE(icon_ai_camera_off);
LV_IMG_DECLARE(icon_ai_camera_on);

/***********************************************************
************************macro define************************
***********************************************************/
#define CAMERA_THUMB_SIZE   60
#define CAMERA_SHUTTER_SIZE 72
#define CAMERA_CLOSE_SIZE   48

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#define CAMERA_UI_MALLOC    tal_psram_malloc
#define CAMERA_UI_FREE      tal_psram_free
#else
#define CAMERA_UI_MALLOC    tal_malloc
#define CAMERA_UI_FREE      tal_free
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    lv_obj_t *page;
    lv_obj_t *preview_canvas;
    lv_obj_t *thumb_img;
    lv_obj_t *shutter_btn;
    lv_obj_t *close_btn;
    lv_obj_t *ai_vision_btn;
    uint8_t  *preview_buf;
    uint16_t  preview_w;
    uint16_t  preview_h;
    uint8_t  *thumb_buf;
} AI_UI_WECHAT_CAMERA_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_UI_WECHAT_CAMERA_T sg_camera = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

/* -- button event callbacks -- */

static void __preview_click_cb(lv_event_t *e)
{
    (void)e;
    if (lv_obj_has_flag(sg_camera.shutter_btn, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(sg_camera.shutter_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(sg_camera.thumb_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(sg_camera.close_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(sg_camera.shutter_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(sg_camera.thumb_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(sg_camera.close_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

static void __thumb_btn_click_cb(lv_event_t *e)
{
    (void)e;
    ai_ui_notify_action(AI_UI_ACT_OPEN_ALBUM, NULL, 0);
}

static void __shutter_btn_click_cb(lv_event_t *e)
{
    (void)e;
    ai_ui_notify_action(AI_UI_ACT_TAKE_PHOTO, NULL, 0);
}

static void __ai_vision_btn_click_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *img = lv_obj_get_child(btn, 0);
    if (lv_obj_get_state(btn) & LV_STATE_CHECKED) {
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
        if (img) lv_img_set_src(img, &icon_ai_camera_off);
        ai_ui_notify_action(AI_UI_ACT_CAMERA_AI_OFF, NULL, 0);
    } else {
        lv_obj_add_state(btn, LV_STATE_CHECKED);
        if (img) lv_img_set_src(img, &icon_ai_camera_on);
        ai_ui_notify_action(AI_UI_ACT_CAMERA_AI_ON, NULL, 0);
    }
}

static void __close_btn_click_cb(lv_event_t *e)
{
    (void)e;
    ai_ui_notify_action(AI_UI_ACT_CLOSE_CAMER, NULL, 0);
}

/* -- display interface callbacks (registered via AI_UI_CAMERA_INTFS_T) -- */

static void __disp_open(void)
{
    if (NULL == sg_camera.page) {
        return;
    }

    lv_vendor_disp_lock();
    lv_obj_clear_flag(sg_camera.page, LV_OBJ_FLAG_HIDDEN);
    /* Always restore controls — they may have been hidden by a preview tap before close */
    lv_obj_clear_flag(sg_camera.shutter_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_camera.thumb_img,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_camera.close_btn,   LV_OBJ_FLAG_HIDDEN);
    /* Reset AI vision toggle to off on each camera open */
    lv_obj_clear_state(sg_camera.ai_vision_btn, LV_STATE_CHECKED);
    lv_obj_t *ai_vision_img = lv_obj_get_child(sg_camera.ai_vision_btn, 0);
    if (ai_vision_img) lv_img_set_src(ai_vision_img, &icon_ai_camera_off);
    lv_vendor_disp_unlock();
}

static void __disp_yuv_flush(AI_UI_VIDEO_T *video)
{
    if (NULL == video || NULL == video->yuv422 || 0 == video->width || 0 == video->height) {
        return;
    }

    /* Allocate a new RGB565 buffer for this frame */
    uint32_t buf_size = (uint32_t)video->width * video->height * 2;
    uint8_t *rgb565_buf = (uint8_t *)CAMERA_UI_MALLOC(buf_size);
    if (NULL == rgb565_buf) {
        PR_ERR("camera preview: malloc rgb565 failed, size=%u", buf_size);
        return;
    }

    /* Convert YUV422 to RGB565 (outside display lock) */
    TAL_IMAGE_YUV422_TO_RGB_T conv = {0};
    conv.in_buf     = video->yuv422;
    conv.in_width   = video->width;
    conv.in_height  = video->height;
    conv.out_buf    = rgb565_buf;
    conv.out_width  = video->width;
    conv.out_height = video->height;

    if (tal_image_convert_yuv422_to_rgb565(&conv) != OPRT_OK) {
        PR_ERR("camera preview: yuv422 to rgb565 failed");
        CAMERA_UI_FREE(rgb565_buf);
        return;
    }

    lv_vendor_disp_lock();
    lv_canvas_set_buffer(sg_camera.preview_canvas, rgb565_buf,
                         video->width, video->height, LV_COLOR_FORMAT_RGB565);

    /* Swap buffer and update canvas */
    if (sg_camera.preview_buf) {
        CAMERA_UI_FREE(sg_camera.preview_buf);
    }
    sg_camera.preview_buf = rgb565_buf;

    sg_camera.preview_w   = video->width;
    sg_camera.preview_h   = video->height;
    lv_obj_center(sg_camera.preview_canvas);
    lv_vendor_disp_unlock();
}

static void __disp_set_thumbnail_jpeg(uint8_t *jpeg, uint32_t len)
{
    if (NULL == sg_camera.thumb_img) {
        return;
    }

    lv_vendor_disp_lock();

    if (NULL == jpeg || 0 == len) {
        /* No thumbnail available — show a default image icon */
        if (sg_camera.thumb_buf) {
            CAMERA_UI_FREE(sg_camera.thumb_buf);
            sg_camera.thumb_buf = NULL;
        }

        /* Remove any previous canvas source and display icon instead */
        lv_obj_clean(sg_camera.thumb_img);
        lv_obj_t *icon = lv_img_create(sg_camera.thumb_img);
        lv_img_set_src(icon, &icon_photo_app);
        lv_obj_set_style_img_recolor(icon, lv_color_white(), 0);
        lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, 0);
        lv_obj_center(icon);

        lv_vendor_disp_unlock();
        return;
    }

    /* Decode JPEG and scale to thumbnail in one step */
    TAL_IMAGE_JPEG_SCALE_IN_T scale_in = {0};
    scale_in.method     = TAL_IMAGE_SCALE_MTH_NEAREST;
    scale_in.mode       = TAL_IMAGE_SCALE_MODE_SIZE;
    scale_in.data       = jpeg;
    scale_in.size       = len;
    scale_in.out_width  = CAMERA_THUMB_SIZE;
    scale_in.out_height = CAMERA_THUMB_SIZE;

    TAL_IMAGE_SCALE_OUT_T scale_out = {0};
    OPERATE_RET rt = tal_image_jpeg_scale_rgb565(&scale_in, &scale_out);
    if (rt != OPRT_OK) {
        PR_ERR("jpeg scale failed, rt:%d", rt);
        lv_vendor_disp_unlock();
        return;
    }

    /* Copy to our own buffer so we can free with CAMERA_UI_FREE later */
    uint32_t thumb_buf_size = CAMERA_THUMB_SIZE * CAMERA_THUMB_SIZE * 2;

    if (sg_camera.thumb_buf) {
        CAMERA_UI_FREE(sg_camera.thumb_buf);
        sg_camera.thumb_buf = NULL;
    }

    sg_camera.thumb_buf = (uint8_t *)CAMERA_UI_MALLOC(thumb_buf_size);
    if (NULL == sg_camera.thumb_buf) {
        PR_ERR("malloc thumb buf failed");
        tal_image_scale_buf_free(&scale_out);
        lv_vendor_disp_unlock();
        return;
    }
    memcpy(sg_camera.thumb_buf, scale_out.buf, thumb_buf_size);
    tal_image_scale_buf_free(&scale_out);

    /* Update thumbnail canvas */
    lv_obj_clean(sg_camera.thumb_img);
    lv_obj_t *canvas = lv_canvas_create(sg_camera.thumb_img);
    lv_obj_set_style_border_width(canvas, 0, 0);
    lv_obj_set_style_pad_all(canvas, 0, 0);
    lv_canvas_set_buffer(canvas, sg_camera.thumb_buf,
                         CAMERA_THUMB_SIZE, CAMERA_THUMB_SIZE, LV_COLOR_FORMAT_RGB565);
    lv_obj_center(canvas);

    lv_vendor_disp_unlock();
}

static void __disp_close(void)
{
    if (NULL == sg_camera.page) {
        return;
    }

    lv_vendor_disp_lock();
    lv_obj_add_flag(sg_camera.page, LV_OBJ_FLAG_HIDDEN);
    lv_vendor_disp_unlock();

    if (sg_camera.preview_buf) {
        CAMERA_UI_FREE(sg_camera.preview_buf);
        sg_camera.preview_buf = NULL;
        sg_camera.preview_w = 0;
        sg_camera.preview_h = 0;
    }
}

/* -- public API -- */

/**
 * @brief Create the camera page widgets (called once during UI init, inside lv_vendor_disp_lock).
 *
 * @param parent  Parent LVGL object (typically the main container).
 */
void ai_ui_wechat_camera_init(lv_obj_t *parent)
{

    /* ---- page root ---- */
    sg_camera.page = lv_obj_create(parent);
    lv_obj_remove_style_all(sg_camera.page);
    lv_obj_set_size(sg_camera.page, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(sg_camera.page, 0, 0);
    lv_obj_set_style_bg_color(sg_camera.page, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_camera.page, LV_OPA_COVER, 0);
    lv_obj_add_flag(sg_camera.page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_camera.page, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- preview area (canvas, centered, full page size) ---- */
    sg_camera.preview_canvas = lv_canvas_create(sg_camera.page);
    lv_obj_remove_style_all(sg_camera.preview_canvas);
    lv_obj_center(sg_camera.preview_canvas);
    lv_obj_add_flag(sg_camera.preview_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sg_camera.preview_canvas, __preview_click_cb, LV_EVENT_CLICKED, NULL);

    /* ---- close button — top-left corner, inset for rounded screens ---- */
    sg_camera.close_btn = lv_obj_create(sg_camera.page);
    lv_obj_set_size(sg_camera.close_btn, CAMERA_CLOSE_SIZE, CAMERA_CLOSE_SIZE);
    lv_obj_align(sg_camera.close_btn, LV_ALIGN_TOP_LEFT, 4 + WECHAT_SAFE_INSET, 4 + WECHAT_SAFE_INSET);
    lv_obj_set_style_bg_opa(sg_camera.close_btn, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(sg_camera.close_btn, lv_color_black(), 0);
    lv_obj_set_style_radius(sg_camera.close_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(sg_camera.close_btn, 0, 0);
    lv_obj_set_style_pad_all(sg_camera.close_btn, 0, 0);
    lv_obj_clear_flag(sg_camera.close_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sg_camera.close_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(sg_camera.close_btn, WECHAT_BTN_EXT_CLICK);
    lv_obj_add_event_cb(sg_camera.close_btn, __close_btn_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *close_icon = lv_img_create(sg_camera.close_btn);
    lv_img_set_src(close_icon, &icon_back_24_24);
    lv_obj_set_style_img_recolor(close_icon, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(close_icon, LV_OPA_COVER, 0);
    lv_obj_center(close_icon);

    /* ---- shutter button — bottom-center, moved up ---- */
    sg_camera.shutter_btn = lv_obj_create(sg_camera.page);
    lv_obj_set_size(sg_camera.shutter_btn, CAMERA_SHUTTER_SIZE, CAMERA_SHUTTER_SIZE);
    lv_obj_align(sg_camera.shutter_btn, LV_ALIGN_BOTTOM_MID, 0, -56);
    lv_obj_set_style_bg_color(sg_camera.shutter_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(sg_camera.shutter_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sg_camera.shutter_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(sg_camera.shutter_btn, 3, 0);
    lv_obj_set_style_border_color(sg_camera.shutter_btn, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_pad_all(sg_camera.shutter_btn, 0, 0);
    lv_obj_clear_flag(sg_camera.shutter_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sg_camera.shutter_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sg_camera.shutter_btn, __shutter_btn_click_cb, LV_EVENT_CLICKED, NULL);

    /* ---- thumbnail button — right of shutter, clipped to rounded corners ---- */
    sg_camera.thumb_img = lv_obj_create(sg_camera.page);
    lv_obj_set_size(sg_camera.thumb_img, CAMERA_THUMB_SIZE, CAMERA_THUMB_SIZE);
    lv_obj_align_to(sg_camera.thumb_img, sg_camera.shutter_btn, LV_ALIGN_OUT_RIGHT_MID, 38, 28);
    lv_obj_set_style_bg_color(sg_camera.thumb_img, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(sg_camera.thumb_img, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sg_camera.thumb_img, 2, 0);
    lv_obj_set_style_border_color(sg_camera.thumb_img, lv_color_white(), 0);
    lv_obj_set_style_radius(sg_camera.thumb_img, 8, 0);
    lv_obj_set_style_pad_all(sg_camera.thumb_img, 0, 0);
    lv_obj_set_style_clip_corner(sg_camera.thumb_img, true, 0);
    lv_obj_clear_flag(sg_camera.thumb_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sg_camera.thumb_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sg_camera.thumb_img, __thumb_btn_click_cb, LV_EVENT_CLICKED, NULL);

    /* Default icon for thumbnail */
    lv_obj_t *thumb_icon = lv_img_create(sg_camera.thumb_img);
    lv_img_set_src(thumb_icon, &icon_photo_app);
    lv_obj_set_style_img_recolor(thumb_icon, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(thumb_icon, LV_OPA_COVER, 0);
    lv_obj_center(thumb_icon);

    /* ---- AI vision toggle button — top-right corner, inset for rounded screens ---- */
    sg_camera.ai_vision_btn = lv_obj_create(sg_camera.page);
    lv_obj_set_size(sg_camera.ai_vision_btn, CAMERA_CLOSE_SIZE, CAMERA_CLOSE_SIZE);
    lv_obj_align(sg_camera.ai_vision_btn, LV_ALIGN_TOP_RIGHT, -4 - WECHAT_SAFE_INSET, 4 + WECHAT_SAFE_INSET);
    lv_obj_set_style_bg_color(sg_camera.ai_vision_btn, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sg_camera.ai_vision_btn, LV_OPA_50, 0);
    lv_obj_set_style_radius(sg_camera.ai_vision_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(sg_camera.ai_vision_btn, 0, 0);
    lv_obj_set_style_pad_all(sg_camera.ai_vision_btn, 0, 0);
    lv_obj_clear_flag(sg_camera.ai_vision_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sg_camera.ai_vision_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(sg_camera.ai_vision_btn, WECHAT_BTN_EXT_CLICK);
    lv_obj_add_event_cb(sg_camera.ai_vision_btn, __ai_vision_btn_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *ai_vision_icon = lv_img_create(sg_camera.ai_vision_btn);
    lv_img_set_src(ai_vision_icon, &icon_ai_camera_off);
    lv_obj_center(ai_vision_icon);
}

/**
 * @brief Register camera display callbacks with the camera management layer.
 */
void ai_ui_wechat_camera_register(void)
{
    AI_UI_CAMERA_INTFS_T intfs;
    memset(&intfs, 0, sizeof(AI_UI_CAMERA_INTFS_T));

    intfs.disp_open               = __disp_open;
    intfs.disp_yuv_flush          = __disp_yuv_flush;
    intfs.disp_set_thumbnail_jpeg = __disp_set_thumbnail_jpeg;
    intfs.disp_close              = __disp_close;

    ai_ui_camera_register(&intfs);
}

#endif /* ENABLE_AI_CHAT_GUI_WECHAT */
