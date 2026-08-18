/**
 * @file app_launcher.c
 * @brief Gravity-driven horizontal app launcher (chat / sand / settings)
 * @version 0.2
 * @date 2026-08-11
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * Gestures (impulse / rate threshold — NOT continuous tilt angle):
 *   up/down shake  → prev/next menu item (IMU AX dominant)
 *   left/right shake → enter / back (IMU AY dominant)
 */

#include "app_launcher.h"

#if defined(BOARD_CHOICE_NICEMCU_T5_0_96ISP) && (BOARD_CHOICE_NICEMCU_T5_0_96ISP == 1)

#include "app_gravity_scroll.h"
#include "sand_fx.h"
#include "board_com_api.h"

#include "tal_api.h"
#include "lv_vendor.h"
#include "lvgl.h"

#include "ai_ui_icon_font.h"

#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define LAUNCH_POLL_MS     20u
#define LAUNCH_STACK       (1024 * 6)
#define LAUNCH_BOOT_MS     1500u
#define LAUNCH_TILE_CNT    4

typedef enum {
    LAUNCH_APP_CHAT = 0,
    LAUNCH_APP_SAND,
    LAUNCH_APP_SETTINGS,
} LAUNCH_APP_E;

typedef enum {
    LAUNCH_PAGE_MENU = 0,
    LAUNCH_PAGE_APP,
} LAUNCH_PAGE_E;

typedef struct {
    LAUNCH_APP_E app;
    const char  *title;
    const char  *subtitle;
} LAUNCH_TILE_T;

/* ---------------------------------------------------------------------------
 * File-scope variables
 * --------------------------------------------------------------------------- */
static const LAUNCH_TILE_T s_tiles[LAUNCH_TILE_CNT] = {
    {LAUNCH_APP_CHAT,     "聊天",     "Chat"},
    {LAUNCH_APP_SAND,     "沙粒动画", "Sand"},
    {LAUNCH_APP_SETTINGS, "设置",     "Settings"},
    {LAUNCH_APP_CHAT,     "聊天",     "Chat"}, /* wrap visual: ...设置、聊天 */
};

static THREAD_HANDLE       s_thrd = NULL;
static APP_GRAVITY_SHAKE_T s_shake_ud; /* menu scroll */
static APP_GRAVITY_SHAKE_T s_shake_lr; /* enter / back */

static lv_obj_t *s_menu_scr     = NULL;
static lv_obj_t *s_strip        = NULL;
static lv_obj_t *s_hint_lbl     = NULL;
static lv_obj_t *s_dot[LAUNCH_TILE_CNT];
static lv_obj_t *s_chat_scr     = NULL;
static lv_obj_t *s_settings_scr = NULL;

static LAUNCH_PAGE_E s_page = LAUNCH_PAGE_MENU;
static LAUNCH_APP_E  s_cur_app = LAUNCH_APP_CHAT;
static int           s_focus = 0;
static int32_t       s_page_w = 160;
static uint8_t       s_started = 0;
static uint8_t       s_imu_ready = 0;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Map carousel index to unique app id
 * @param[in] idx tile index 0..3
 * @return app enum
 */
static LAUNCH_APP_E __launch_app_of(int idx)
{
    if (idx < 0 || idx >= LAUNCH_TILE_CNT) {
        return LAUNCH_APP_CHAT;
    }
    return s_tiles[idx].app;
}

/**
 * @brief Update bottom dots for focus
 * @param[in] idx focused tile
 * @return none
 */
static void __launch_update_dots(int idx)
{
    int i = 0;

    for (i = 0; i < LAUNCH_TILE_CNT; i++) {
        if (NULL == s_dot[i]) {
            continue;
        }
        if (i == idx) {
            lv_obj_set_style_bg_color(s_dot[i], lv_color_hex(0x4C8DFF), 0);
            lv_obj_set_size(s_dot[i], 8, 8);
        } else {
            lv_obj_set_style_bg_color(s_dot[i], lv_color_hex(0x4A5568), 0);
            lv_obj_set_size(s_dot[i], 6, 6);
        }
    }
}

/**
 * @brief Snap strip to focused tile center
 * @param[in] idx tile index
 * @param[in] anim use animation
 * @return none
 */
static void __launch_snap_to(int idx, uint8_t anim)
{
    if (NULL == s_strip || s_page_w <= 0) {
        return;
    }
    if (idx < 0) {
        idx = 0;
    }
    if (idx >= LAUNCH_TILE_CNT) {
        idx = LAUNCH_TILE_CNT - 1;
    }
    s_focus = idx;
    lv_obj_scroll_to_x(s_strip, idx * s_page_w, anim ? LV_ANIM_ON : LV_ANIM_OFF);
    __launch_update_dots(idx);
}

/**
 * @brief Move focus by delta with wrap
 * @param[in] delta +1 next / -1 prev
 * @return none
 */
static void __launch_move_focus(int delta)
{
    int idx = s_focus + delta;

    if (idx < 0) {
        idx = LAUNCH_TILE_CNT - 1;
    } else if (idx >= LAUNCH_TILE_CNT) {
        idx = 0;
    }
    __launch_snap_to(idx, 1);
    PR_NOTICE("launcher: focus=%d (%s)", idx, s_tiles[idx].title);
}

/**
 * @brief Build settings placeholder screen
 * @return none
 */
static void __launch_create_settings(void)
{
    lv_obj_t *title = NULL;
    lv_obj_t *body = NULL;
    lv_font_t *font = ai_ui_get_text_font();

    s_settings_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_settings_scr, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(s_settings_scr, LV_OPA_COVER, 0);

    title = lv_label_create(s_settings_scr);
    lv_label_set_text(title, "设置");
    if (font) {
        lv_obj_set_style_text_font(title, font, 0);
    }
    lv_obj_set_style_text_color(title, lv_color_hex(0xE8EEF5), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    body = lv_label_create(s_settings_scr);
    lv_label_set_text(body, "亮度 / 音量\n(占位)\n\n左甩返回");
    if (font) {
        lv_obj_set_style_text_font(body, font, 0);
    }
    lv_obj_set_style_text_color(body, lv_color_hex(0xA0AEC0), 0);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(body, LV_PCT(90));
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_align(body, LV_ALIGN_CENTER, 0, 4);
}

/**
 * @brief Build horizontal menu screen
 * @return none
 */
static void __launch_create_menu(void)
{
    lv_obj_t *dots = NULL;
    lv_font_t *font = ai_ui_get_text_font();
    int i = 0;

    s_page_w = (int32_t)LV_HOR_RES;
    if (s_page_w < 80) {
        s_page_w = 160;
    }

    s_menu_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_menu_scr, lv_color_hex(0x0B1220), 0);
    lv_obj_set_style_bg_opa(s_menu_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_menu_scr, 0, 0);

    s_hint_lbl = lv_label_create(s_menu_scr);
    lv_label_set_text(s_hint_lbl, "静止校准...");
    if (font) {
        lv_obj_set_style_text_font(s_hint_lbl, font, 0);
    }
    lv_obj_set_style_text_color(s_hint_lbl, lv_color_hex(0x8FA3B8), 0);
    lv_obj_set_width(s_hint_lbl, LV_PCT(96));
    lv_obj_set_style_text_align(s_hint_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_hint_lbl, LV_ALIGN_TOP_MID, 0, 2);

    s_strip = lv_obj_create(s_menu_scr);
    lv_obj_set_size(s_strip, LV_HOR_RES, LV_VER_RES - 18);
    lv_obj_align(s_strip, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_strip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_strip, 0, 0);
    lv_obj_set_style_pad_all(s_strip, 0, 0);
    lv_obj_set_flex_flow(s_strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_scroll_dir(s_strip, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(s_strip, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(s_strip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_snap_x(s_strip, LV_SCROLL_SNAP_CENTER);

    for (i = 0; i < LAUNCH_TILE_CNT; i++) {
        lv_obj_t *tile = NULL;
        lv_obj_t *title = NULL;
        lv_obj_t *sub = NULL;

        tile = lv_obj_create(s_strip);
        lv_obj_set_size(tile, s_page_w, LV_PCT(100));
        lv_obj_set_style_bg_color(tile, lv_color_hex(0x162033), 0);
        lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(tile, 0, 0);
        lv_obj_set_style_border_width(tile, 0, 0);
        lv_obj_set_style_pad_all(tile, 4, 0);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

        title = lv_label_create(tile);
        lv_label_set_text(title, s_tiles[i].title);
        if (font) {
            lv_obj_set_style_text_font(title, font, 0);
        }
        lv_obj_set_style_text_color(title, lv_color_hex(0xF7FAFC), 0);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, -8);

        sub = lv_label_create(tile);
        lv_label_set_text(sub, s_tiles[i].subtitle);
        lv_obj_set_style_text_color(sub, lv_color_hex(0x718096), 0);
        lv_obj_align(sub, LV_ALIGN_CENTER, 0, 12);
    }

    dots = lv_obj_create(s_menu_scr);
    lv_obj_set_size(dots, LV_PCT(80), 12);
    lv_obj_set_style_bg_opa(dots, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dots, 0, 0);
    lv_obj_set_style_pad_all(dots, 0, 0);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dots, 6, 0);
    lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -1);
    lv_obj_clear_flag(dots, LV_OBJ_FLAG_SCROLLABLE);

    for (i = 0; i < LAUNCH_TILE_CNT; i++) {
        s_dot[i] = lv_obj_create(dots);
        lv_obj_set_size(s_dot[i], 6, 6);
        lv_obj_set_style_radius(s_dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_dot[i], 0, 0);
        lv_obj_set_style_bg_opa(s_dot[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(s_dot[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_obj_update_layout(s_strip);
    __launch_snap_to(0, 0);
}

/**
 * @brief Show menu screen
 * @return none
 */
static void __launch_show_menu(void)
{
    if (s_cur_app == LAUNCH_APP_SAND) {
        sand_fx_leave();
    }
    s_page = LAUNCH_PAGE_MENU;
    s_cur_app = LAUNCH_APP_CHAT;
    if (NULL != s_menu_scr) {
        lv_screen_load(s_menu_scr);
    }
    app_gravity_shake_reset(&s_shake_ud);
    app_gravity_shake_reset(&s_shake_lr);
}

/**
 * @brief Enter currently focused app
 * @return none
 */
static void __launch_enter_app(void)
{
    LAUNCH_APP_E app = __launch_app_of(s_focus);

    s_page = LAUNCH_PAGE_APP;
    s_cur_app = app;

    if (app == LAUNCH_APP_CHAT) {
        if (NULL != s_chat_scr) {
            lv_screen_load(s_chat_scr);
        }
        PR_NOTICE("launcher: enter chat");
    } else if (app == LAUNCH_APP_SAND) {
        sand_fx_enter();
        PR_NOTICE("launcher: enter sand");
    } else {
        if (NULL != s_settings_scr) {
            lv_screen_load(s_settings_scr);
        }
        PR_NOTICE("launcher: enter settings");
    }
    app_gravity_shake_reset(&s_shake_lr);
}

/**
 * @brief Handle left/right shake: enter or back
 * @param[in] s shake
 * @return none
 */
static void __launch_handle_lr(APP_GRAVITY_SHAKE_E s)
{
    if (s == APP_GRAVITY_SHAKE_NONE) {
        return;
    }
    /* POS ≈ screen-right → enter; NEG ≈ screen-left → back */
    if (s == APP_GRAVITY_SHAKE_POS) {
        if (s_page == LAUNCH_PAGE_MENU) {
            __launch_enter_app();
        }
    } else {
        if (s_page == LAUNCH_PAGE_APP) {
            __launch_show_menu();
            PR_NOTICE("launcher: back to menu");
        }
    }
}

/**
 * @brief Handle discrete up/down shake (menu switch only)
 * @param[in] s shake
 * @return none
 */
static void __launch_handle_ud(APP_GRAVITY_SHAKE_E s)
{
    if (s_page != LAUNCH_PAGE_MENU || s == APP_GRAVITY_SHAKE_NONE) {
        return;
    }
    if (s == APP_GRAVITY_SHAKE_POS) {
        __launch_move_focus(1);
    } else {
        __launch_move_focus(-1);
    }
}

/**
 * @brief Apply one UI frame under LVGL lock
 * @param[in] ud up/down shake
 * @param[in] lr left/right shake
 * @return none
 */
static void __launch_apply_frame(APP_GRAVITY_SHAKE_E ud, APP_GRAVITY_SHAKE_E lr)
{
    lv_vendor_disp_lock();

    /* Prefer LR (enter/back) over UD if both fire in one frame */
    if (lr != APP_GRAVITY_SHAKE_NONE) {
        __launch_handle_lr(lr);
    } else if (ud != APP_GRAVITY_SHAKE_NONE) {
        __launch_handle_ud(ud);
    }

    if (NULL != s_hint_lbl && s_page == LAUNCH_PAGE_MENU) {
        if (0 == s_imu_ready) {
            lv_label_set_text(s_hint_lbl, "静止校准...");
        } else {
            lv_label_set_text(s_hint_lbl, "上下甩切换  右甩进入");
        }
    }

    lv_vendor_disp_unlock();
}

/**
 * @brief Launcher worker thread
 * @param[in] arg unused
 * @return none
 */
static void __launch_task(void *arg)
{
    APP_GRAVITY_SHAKE_CFG_T cfg_ud;
    APP_GRAVITY_SHAKE_CFG_T cfg_lr;
    sh3001_dev_t           *imu = NULL;
    uint32_t                now_ms = 0;
    float                   ax, ay, az;
    float                   gx, gy, gz;
    APP_GRAVITY_SHAKE_E     ud;
    APP_GRAVITY_SHAKE_E     lr;

    (void)arg;

    tal_system_sleep(LAUNCH_BOOT_MS);

    lv_vendor_disp_lock();
    s_chat_scr = lv_screen_active();
    __launch_create_menu();
    __launch_create_settings();
    lv_vendor_disp_unlock();

    sand_fx_init();

    /*
     * Axis split after board remap (chip +45° about Y):
     *   screen up/down  ≈ board AX  → menu scroll
     *   screen left/right ≈ board AY → enter / back
     * dominance_ratio rejects cross-axis coupling.
     */
    app_gravity_shake_cfg_default(&cfg_ud);
    cfg_ud.accel_axis       = APP_GRAVITY_AXIS_X;
    cfg_ud.gyro_axis        = APP_GRAVITY_AXIS_X;
    cfg_ud.invert           = 1;
    cfg_ud.dominance_ratio  = 1.40f;
    cfg_ud.accel_thresh_g   = 0.55f;
    cfg_ud.accel_release_g  = 0.20f;
    cfg_ud.gyro_thresh_dps  = 200.0f;
    cfg_ud.gyro_release_dps = 50.0f;
    cfg_ud.cooldown_ms      = 480;
    app_gravity_shake_init(&s_shake_ud, &cfg_ud);

    app_gravity_shake_cfg_default(&cfg_lr);
    cfg_lr.accel_axis       = APP_GRAVITY_AXIS_Y;
    cfg_lr.gyro_axis        = APP_GRAVITY_AXIS_Y;
    cfg_lr.invert           = -1; /* SX = -ay: +screen-right → POS → enter */
    cfg_lr.dominance_ratio  = 1.40f;
    cfg_lr.accel_thresh_g   = 0.55f;
    cfg_lr.accel_release_g  = 0.20f;
    cfg_lr.gyro_thresh_dps  = 200.0f;
    cfg_lr.gyro_release_dps = 50.0f;
    cfg_lr.cooldown_ms      = 550;
    app_gravity_shake_init(&s_shake_lr, &cfg_lr);

    lv_vendor_disp_lock();
    lv_screen_load(s_menu_scr);
    lv_vendor_disp_unlock();

    s_page = LAUNCH_PAGE_MENU;
    PR_NOTICE("launcher: UD=scroll(board AX), LR=enter/back(AY), +45deg remap");

    while (1) {
        tal_system_sleep(LAUNCH_POLL_MS);
        now_ms = tal_system_get_millisecond();

        imu = board_sh3001_get_dev();
        if (NULL == imu) {
            continue;
        }
        if (OPRT_OK != sh3001_read_accel(imu, &ax, &ay, &az)) {
            continue;
        }
        if (OPRT_OK != sh3001_read_gyro(imu, &gx, &gy, &gz)) {
            gx = gy = gz = 0.0f;
        }
        {
            float bx, by, bz, bgx, bgy, bgz;
            board_sh3001_map_accel_to_board(ax, ay, az, &bx, &by, &bz);
            board_sh3001_map_gyro_to_board(gx, gy, gz, &bgx, &bgy, &bgz);
            ax = bx;
            ay = by;
            az = bz;
            gx = bgx;
            gy = bgy;
            gz = bgz;
        }

        ud = app_gravity_shake_update(&s_shake_ud, ax, ay, az, gx, gy, gz, now_ms);
        lr = app_gravity_shake_update(&s_shake_lr, ax, ay, az, gx, gy, gz, now_ms);
        if (s_shake_ud.base_ready || s_shake_lr.base_ready) {
            s_imu_ready = 1;
        }

        /* In app: ignore UD scroll; still accept LR back */
        if (s_page != LAUNCH_PAGE_MENU) {
            ud = APP_GRAVITY_SHAKE_NONE;
        }

        __launch_apply_frame(ud, lr);
    }
}

/**
 * @brief Start gravity launcher
 * @return OPRT_OK on success
 */
OPERATE_RET app_launcher_start(void)
{
    THREAD_CFG_T cfg;

    if (s_started) {
        return OPRT_OK;
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.thrdname = "launcher";
    cfg.priority = THREAD_PRIO_2;
    cfg.stackDepth = LAUNCH_STACK;

    if (OPRT_OK != tal_thread_create_and_start(&s_thrd, NULL, NULL, __launch_task, NULL, &cfg)) {
        PR_ERR("launcher: thread create failed");
        return OPRT_COM_ERROR;
    }
    s_started = 1;
    PR_NOTICE("launcher: started");
    return OPRT_OK;
}

#else /* !XIAO */

OPERATE_RET app_launcher_start(void)
{
    return OPRT_NOT_SUPPORTED;
}

#endif
