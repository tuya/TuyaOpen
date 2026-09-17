/**
 * @file guided_wechat_core.c
 * @brief WeChat-style UI 鈥?main entry: LVGL init, screen, status bar, sub-page register.
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"

#if (defined(ENABLE_AI_CHAT_GUI_GUIDED_WECHAT) && (ENABLE_AI_CHAT_GUI_GUIDED_WECHAT == 1)) || \
    (defined(ENABLE_AI_CHAT_GUI_GUIDED_WECHAT) && (ENABLE_AI_CHAT_GUI_GUIDED_WECHAT == 1))
#include "lv_vendor.h"

#include "ai_ui_manage.h"
#include "ai_ui_icon_font.h"
#include "font_awesome_symbols.h"
#include "guided_wechat_core.h"
#include "guided_wechat_common.h"
#include "ai_ui_locale.h"

LV_FONT_DECLARE(font_puhui_14_1);

/***********************************************************
************************macro define************************
***********************************************************/
#define STATUS_BAR_HEIGHT  40
#define STATUS_BAR_MODE_WIDTH_PERCENT 30
#define STATUS_BAR_CENTER_WIDTH_PERCENT 50
#define STATUS_BAR_CENTER_LEFT_PERCENT 25
#define STATUS_BAR_EMOTION_GAP 5

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    lv_obj_t *container;
    lv_obj_t *status_bar;
    lv_obj_t *status_group;
    lv_obj_t *emotion_label;
    lv_obj_t *status_label;
    lv_obj_t *network_label;
    lv_obj_t *notification_label;
    lv_obj_t *mode_label;
} GUIDED_WECHAT_MAIN_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static GUIDED_WECHAT_MAIN_T sg_main = {0};
static lv_timer_t *sg_notification_tm = NULL;

static const lv_font_t *__status_bar_text_font(void)
{
    /* English mode labels are materially longer than their Chinese
     * equivalents. Use the already bundled 14 px font only in the status bar
     * so the mode, emotion, and state keep separate visual space. */
    return ai_ui_locale_is_english() ? &font_puhui_14_1 : ai_ui_get_text_font();
}

/***********************************************************
***********************function define**********************
***********************************************************/
static void __lvgl_init(void)
{
    lv_vendor_init(DISPLAY_NAME);
    lv_vendor_start(5, 1024 * 8);
}

static void __ui_notification_timeout_cb(lv_timer_t *timer)
{
    lv_timer_del(sg_notification_tm);
    sg_notification_tm = NULL;

    lv_obj_add_flag(sg_main.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_main.status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_main.emotion_label, LV_OBJ_FLAG_HIDDEN);
}

/* 鈹€鈹€ status bar callbacks (registered into AI_UI_INTFS_T) 鈹€鈹€ */

static void __ui_set_emotion(char *emotion)
{
    if (NULL == sg_main.emotion_label) {
        return;
    }

    AI_UI_FONT_LIST_T sg_font;
    sg_font.emoji      = ai_ui_get_emo_font();
    sg_font.emoji_list = ai_ui_get_emo_list();

    char *emo_icon = sg_font.emoji_list[0].emo_icon;
    for (int i = 0; i < FONT_EMO_ICON_MAX_NUM; i++) {
        if (strcmp(emotion, sg_font.emoji_list[i].emo_name) == 0) {
            emo_icon = sg_font.emoji_list[i].emo_icon;
            break;
        }
    }

    lv_vendor_disp_lock();
    lv_obj_set_style_text_font(sg_main.emotion_label, sg_font.emoji, 0);
    lv_label_set_text(sg_main.emotion_label, emo_icon);
    if (sg_main.status_label != NULL) {
        lv_obj_align_to(sg_main.emotion_label, sg_main.status_label,
                        LV_ALIGN_OUT_LEFT_MID, -STATUS_BAR_EMOTION_GAP, 0);
    }
    lv_vendor_disp_unlock();
}

static void __ui_set_status(char *status)
{
    if (sg_main.status_label == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_obj_set_style_text_font(sg_main.status_label, __status_bar_text_font(), 0);
    lv_label_set_text(sg_main.status_label, status);
    lv_obj_set_style_text_align(sg_main.status_label, LV_TEXT_ALIGN_CENTER, 0);
    if (sg_main.emotion_label != NULL) {
        lv_obj_align_to(sg_main.emotion_label, sg_main.status_label,
                        LV_ALIGN_OUT_LEFT_MID, -STATUS_BAR_EMOTION_GAP, 0);
    }
    lv_vendor_disp_unlock();
}

static void __ui_set_notification(char *notification)
{
    if (sg_main.notification_label == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_obj_set_style_text_font(sg_main.notification_label, __status_bar_text_font(), 0);
    lv_label_set_text(sg_main.notification_label, notification);
    lv_obj_add_flag(sg_main.status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sg_main.notification_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sg_main.emotion_label, LV_OBJ_FLAG_HIDDEN);

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

    if (sg_main.network_label == NULL || wifi_icon == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_label_set_text(sg_main.network_label, wifi_icon);
    lv_obj_align(sg_main.network_label, LV_ALIGN_RIGHT_MID, -WECHAT_SAFE_INSET, 0);
    lv_vendor_disp_unlock();
}

static void __ui_set_chat_mode(char *chat_mode)
{
    if (sg_main.mode_label == NULL || chat_mode == NULL) {
        return;
    }

    lv_vendor_disp_lock();
    lv_obj_set_style_text_font(sg_main.mode_label, __status_bar_text_font(), 0);
    lv_label_set_text(sg_main.mode_label, chat_mode);
    lv_obj_align(sg_main.mode_label, LV_ALIGN_LEFT_MID, WECHAT_SAFE_INSET, 0);
    lv_vendor_disp_unlock();
}

/* 鈹€鈹€ UI init 鈹€鈹€ */

OPERATE_RET guided_wechat_core_init(void)
{
    __lvgl_init();

    lv_vendor_disp_lock();

    /* Disable scrolling on the root screen so gesture events are not blocked */
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    const lv_font_t *text_font = ai_ui_get_text_font();
    const lv_font_t *icon_font = ai_ui_get_icon_font();

    /* Screen */
    lv_obj_t *screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_radius(screen, 0, 0);
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Container */
    sg_main.container = lv_obj_create(screen);
    lv_obj_set_size(sg_main.container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(sg_main.container, 0, 0);
    lv_obj_set_style_border_width(sg_main.container, 0, 0);
    lv_obj_set_style_pad_row(sg_main.container, 0, 0);
    lv_obj_set_style_radius(sg_main.container, 0, 0);
    lv_obj_clear_flag(sg_main.container, LV_OBJ_FLAG_SCROLLABLE);

    /* Status bar */
    sg_main.status_bar = lv_obj_create(sg_main.container);
    lv_obj_set_size(sg_main.status_bar, LV_HOR_RES, STATUS_BAR_HEIGHT);
    lv_obj_set_style_radius(sg_main.status_bar, 0, 0);
    lv_obj_set_style_border_width(sg_main.status_bar, 0, 0);
    lv_obj_set_style_pad_all(sg_main.status_bar, 0, 0);
    lv_obj_set_style_bg_color(sg_main.status_bar, lv_palette_main(LV_PALETTE_GREEN), 0);

    /* Reserve independent regions so long English labels cannot overlap the
     * emotion/status pair or the Wi-Fi indicator. */
    sg_main.status_group = lv_obj_create(sg_main.status_bar);
    lv_obj_set_size(sg_main.status_group,
                    LV_HOR_RES * STATUS_BAR_CENTER_WIDTH_PERCENT / 100,
                    STATUS_BAR_HEIGHT);
    lv_obj_align(sg_main.status_group, LV_ALIGN_TOP_LEFT,
                 LV_HOR_RES * STATUS_BAR_CENTER_LEFT_PERCENT / 100, 0);
    lv_obj_set_style_pad_all(sg_main.status_group, 0, 0);
    lv_obj_set_style_radius(sg_main.status_group, 0, 0);
    lv_obj_set_style_border_width(sg_main.status_group, 0, 0);
    lv_obj_set_style_bg_opa(sg_main.status_group, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(sg_main.status_group, LV_OBJ_FLAG_SCROLLABLE);

    sg_main.mode_label = lv_label_create(sg_main.status_bar);
    lv_obj_set_style_text_font(sg_main.mode_label, __status_bar_text_font(), 0);
    lv_obj_set_width(sg_main.mode_label,
                     LV_HOR_RES * STATUS_BAR_MODE_WIDTH_PERCENT / 100);
    lv_label_set_long_mode(sg_main.mode_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(sg_main.mode_label, "");
    lv_obj_align(sg_main.mode_label, LV_ALIGN_LEFT_MID, WECHAT_SAFE_INSET, 0);

    sg_main.status_label = lv_label_create(sg_main.status_group);
    lv_obj_set_style_text_font(sg_main.status_label, __status_bar_text_font(), 0);
    lv_label_set_long_mode(sg_main.status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(sg_main.status_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(sg_main.status_label, INITIALIZING);

    sg_main.emotion_label = lv_label_create(sg_main.status_group);
    lv_obj_set_style_text_font(sg_main.emotion_label, icon_font, 0);
    lv_label_set_text(sg_main.emotion_label, FONT_AWESOME_AI_CHIP);
    lv_obj_align_to(sg_main.emotion_label, sg_main.status_label,
                    LV_ALIGN_OUT_LEFT_MID, -STATUS_BAR_EMOTION_GAP, 0);

    sg_main.notification_label = lv_label_create(sg_main.status_group);
    lv_obj_set_style_text_font(sg_main.notification_label, __status_bar_text_font(), 0);
    lv_obj_set_width(sg_main.notification_label, LV_PCT(100));
    lv_label_set_long_mode(sg_main.notification_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_center(sg_main.notification_label);
    lv_label_set_text(sg_main.notification_label, "");
    lv_obj_add_flag(sg_main.notification_label, LV_OBJ_FLAG_HIDDEN);

    sg_main.network_label = lv_label_create(sg_main.status_bar);
    lv_obj_set_style_text_font(sg_main.network_label, icon_font, 0);
    lv_label_set_text(sg_main.network_label, "");
    lv_obj_align(sg_main.network_label, LV_ALIGN_RIGHT_MID, -WECHAT_SAFE_INSET, 0);

    /* Init sub-pages */
    guided_wechat_chat_init(sg_main.container);
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    guided_wechat_camera_init(screen);
#endif
#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
    guided_wechat_album_init(screen);
#endif

    lv_vendor_disp_unlock();

    return OPRT_OK;
}

/**
 * @brief Register WeChat-style chat UI implementation.
 */
OPERATE_RET guided_wechat_register_with_init(OPERATE_RET (*disp_init)(void))
{
    AI_UI_INTFS_T intfs;
    memset(&intfs, 0, sizeof(AI_UI_INTFS_T));

    intfs.disp_init         = disp_init != NULL ? disp_init : guided_wechat_core_init;
    intfs.disp_emotion      = __ui_set_emotion;
    intfs.disp_ai_mode_state = __ui_set_status;
    intfs.disp_notification = __ui_set_notification;
    intfs.disp_wifi_state   = __ui_set_network;
    intfs.disp_ai_chat_mode = __ui_set_chat_mode;

    ai_ui_register(&intfs);

    /* Sub-pages register independently */
    guided_wechat_chat_register();
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    guided_wechat_camera_register();
#endif
#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
    guided_wechat_album_register();
#endif

    return OPRT_OK;
}

OPERATE_RET guided_wechat_core_register(void)
{
    return guided_wechat_register_with_init(guided_wechat_core_init);
}

#endif /* WeChat UI variants */
