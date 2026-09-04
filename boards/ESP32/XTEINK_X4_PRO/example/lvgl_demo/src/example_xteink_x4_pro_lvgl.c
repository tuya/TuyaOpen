/**
 * @file example_xteink_x4_pro_lvgl.c
 * @brief X4 Pro LVGL lab: portrait 480x800 UI on the portrait-mounted panel,
 *        splash, boot EPD test pattern (gray16 bands), dashboard
 *        (battery/SD/IO/touch), display settings screen
 *        (frontlight brightness + warmth, refresh mode).
 * @version 0.2
 * @date 2026-08-19
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tal_system.h"
#include "tkl_output.h"

#include "lvgl.h"

#include "board_com_api.h"
#include "board_config.h"
#include "tal_log.h"
#include "xteink_x4_pro_buttons.h"

#include <stdio.h>
#include <string.h>

#if LV_FONT_MONTSERRAT_48
LV_FONT_DECLARE(lv_font_montserrat_48);
#endif
#if LV_FONT_MONTSERRAT_36
LV_FONT_DECLARE(lv_font_montserrat_36);
#endif
#if LV_FONT_MONTSERRAT_24
LV_FONT_DECLARE(lv_font_montserrat_24);
#endif
#if LV_FONT_MONTSERRAT_14
LV_FONT_DECLARE(lv_font_montserrat_14);
#endif

#define X4PRO_PWR_HOLD_MS 1500U

#define X4PRO_EPD_W        ((int32_t)X4PRO_EPD_WIDTH)
#define X4PRO_EPD_H        ((int32_t)X4PRO_EPD_HEIGHT)
#define X4PRO_EPD_STRIDE   (X4PRO_EPD_W / 8U)
#define X4PRO_EPD_BUF_SIZE (X4PRO_EPD_STRIDE * X4PRO_EPD_H)

/* The device mounts the 800x480 panel portrait. LVGL works in the portrait
 * frame; the flush path rotates 90 deg CW into the native landscape EPD RAM. */
#define X4PRO_VIEW_W X4PRO_EPD_H /* 480 portrait width  */
#define X4PRO_VIEW_H X4PRO_EPD_W /* 800 portrait height */

#define X4PRO_LV_DRAW_LINES 24
#define X4PRO_LV_BUF_PIXELS (X4PRO_VIEW_W * X4PRO_LV_DRAW_LINES)
#define X4PRO_LV_BUF_BYTES  (X4PRO_LV_BUF_PIXELS * (int)sizeof(lv_color16_t))

#define X4PRO_SPLASH_HOLD_MS 1000U

/* Boot EPD validation paint (ported from the working X4 lab demo): pushed
 * as an absolute full refresh, so a healthy panel must show it regardless of
 * the differential DU baseline. Hold is short — the full refresh already
 * takes ~1.6 s to paint, which is the real dwell time. */
#define X4PRO_GRAY16_HOLD_MS 400U

/* Portrait viewable region: the panel bezel insets (board_config.h, landscape
 * top/right/bottom/left = 9/7/3/7) rotated with the 90 deg CW mount. */
#define X4PRO_VIEW_PAD_TOP    ((int32_t)X4PRO_PANEL_VIEWABLE_LEFT_PX)   /* 7 */
#define X4PRO_VIEW_PAD_RIGHT  ((int32_t)X4PRO_PANEL_VIEWABLE_TOP_PX)    /* 9 */
#define X4PRO_VIEW_PAD_BOTTOM ((int32_t)X4PRO_PANEL_VIEWABLE_RIGHT_PX)  /* 7 */
#define X4PRO_VIEW_PAD_LEFT   ((int32_t)X4PRO_PANEL_VIEWABLE_BOTTOM_PX) /* 3 */

#define X4PRO_RENDER_W (X4PRO_VIEW_W - X4PRO_VIEW_PAD_LEFT - X4PRO_VIEW_PAD_RIGHT) /* 468 */
#define X4PRO_RENDER_H (X4PRO_VIEW_H - X4PRO_VIEW_PAD_TOP - X4PRO_VIEW_PAD_BOTTOM) /* 786 */

/* Dashboard: title bar + 4 stacked full-width boxes + footer. */
#define X4PRO_BAR_H   56
#define X4PRO_FOOT_H  40
#define X4PRO_ROW_GAP 4
#define X4PRO_MID_H   (X4PRO_RENDER_H - X4PRO_BAR_H - X4PRO_FOOT_H)
#define X4PRO_ROW_H   ((X4PRO_MID_H - 3 * X4PRO_ROW_GAP) / 4)

#define X4PRO_INPUT_POLL_MS 40U
#define X4PRO_EPD_PUSH_MS   100U
#define X4PRO_HUB_SLOW_N    5U
#define X4PRO_DIAG_TICKS 250U /* ~10 s: periodic serial diagnostics heartbeat */

typedef enum {
    X4PRO_SCR_DASHBOARD = 0,
    X4PRO_SCR_SETTINGS,
    X4PRO_SCR_SHUTDOWN,
} X4PRO_SCREEN_E;

static uint8_t         s_epd_fb[X4PRO_EPD_BUF_SIZE];
static lv_display_t   *s_disp;
static volatile BOOL_T s_epd_dirty;
static uint32_t        s_boot_ms;

/* dashboard widgets */
static lv_obj_t *s_bar_title;
static lv_obj_t *s_bar_time;
static lv_obj_t *s_q_bat;
static lv_obj_t *s_q_sd;
static lv_obj_t *s_key_lbl[4];
static lv_obj_t *s_q_epd;
static lv_obj_t *s_foot_lbl;

/* settings widgets */
static lv_obj_t *s_set_bright_slider;
static lv_obj_t *s_set_bright_val;
static lv_obj_t *s_set_warmth_slider;
static lv_obj_t *s_set_warmth_val;
static lv_obj_t *s_set_refresh_btn;

static BOOL_T        s_sd_mounted;
static THREAD_HANDLE s_lvgl_thread;

static lv_timer_t *s_status_timer;
static SYS_TIME_T  s_pwr_down_ms;   /* 0 = PWR up; else timestamp of press */
static BOOL_T      s_pwr_off_armed; /* long-press latched: release powers off */
static lv_obj_t   *s_pwr_popup;     /* power-off confirm dialog (top layer) */
static BOOL_T      s_power_off_started;
static BOOL_T      s_in_status_cb; /* reentry guard: arm/seq pumps run handlers */
static uint8_t     s_prev_btn;
static uint8_t     s_hub_slow_tick;
static uint32_t    s_diag_tick;

static char s_sd_smoke_msg[192];

/* display settings state (driver bring-up defaults: 50 % brightness, cold) */
static uint8_t s_fl_brightness = 50U;
static uint8_t s_fl_warmth     = 0U;
static BOOL_T  s_use_full_refresh; /* settings: default refresh mode */
static BOOL_T  s_force_full_once;  /* one-shot full refresh (Home key) */

static X4PRO_SCREEN_E s_screen = X4PRO_SCR_DASHBOARD;

/* charge state: mirrored from the battery estimator task (event-driven) */
static X4PRO_CHARGE_STATE_E s_charge_state   = X4PRO_CHARGE_IDLE;
static volatile BOOL_T      s_charge_ui_pending = FALSE;

/**
 * @brief Human-readable charge-state text for the dashboard label.
 */
static const char *__charge_text(X4PRO_CHARGE_STATE_E st)
{
    switch (st) {
    case X4PRO_CHARGE_CHARGING:
        return "CHARGING";
    case X4PRO_CHARGE_FULL:
        return "FULL";
    default:
        return "idle";
    }
}

/**
 * @brief Charge-state change callback: runs in the battery estimator task —
 *        mirror the state and flag the LVGL thread for a label refresh.
 */
static void __charge_state_cb(X4PRO_CHARGE_STATE_E from, X4PRO_CHARGE_STATE_E to)
{
    (void)from;
    s_charge_state      = to;
    s_charge_ui_pending = TRUE;
}

static int32_t    s_touch_last_x = -1;
static int32_t    s_touch_last_y = -1;
static BOOL_T     s_touch_home_seen;
static BOOL_T     s_touch_pressed_prev;
static BOOL_T     s_touch_home_prev;
static SYS_TIME_T s_touch_down_ms;
static int32_t    s_touch_log_x = -1; /* last move-logged position */
static int32_t    s_touch_log_y = -1;
static SYS_TIME_T s_key_down_ms[3];   /* LEFT / RIGHT / PWR press stamps */

static void __build_dashboard(void);
static void __build_settings_screen(void);
static void __dashboard_refresh_slow(void);
static void __scr_pad_viewable(lv_obj_t *scr);
static void __refresh_keys_quadrant(uint8_t st, BOOL_T home);
static void __fill_gray16_pattern_fb(void);

static void __apply_title_font(lv_obj_t *obj)
{
#if LV_FONT_MONTSERRAT_24
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, LV_PART_MAIN);
#endif
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
}

static void __apply_body_font(lv_obj_t *obj)
{
#if LV_FONT_MONTSERRAT_14
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN);
#endif
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
}

/**
 * @brief Apply the viewable-area padding (asymmetric bezel inset) to a screen.
 * @param[in] scr screen object.
 * @return none
 */
static void __scr_pad_viewable(lv_obj_t *scr)
{
    lv_obj_set_style_pad_left(scr, X4PRO_VIEW_PAD_LEFT, LV_PART_MAIN);
    lv_obj_set_style_pad_right(scr, X4PRO_VIEW_PAD_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_pad_top(scr, X4PRO_VIEW_PAD_TOP, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(scr, X4PRO_VIEW_PAD_BOTTOM, LV_PART_MAIN);
}

static void __screen_reset(lv_obj_t *scr)
{
    lv_obj_clean(scr);
    s_bar_title = NULL;
    s_bar_time  = NULL;
    s_q_bat     = NULL;
    s_q_sd      = NULL;
    s_q_epd     = NULL;
    s_foot_lbl  = NULL;
    s_set_bright_slider = NULL;
    s_set_bright_val    = NULL;
    s_set_warmth_slider = NULL;
    s_set_warmth_val    = NULL;
    s_set_refresh_btn   = NULL;
    (void)memset(s_key_lbl, 0, sizeof(s_key_lbl));
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

static void __style_quad_frame(lv_obj_t *q)
{
    lv_obj_set_style_bg_color(q, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_color(q, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_width(q, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(q, 6, LV_PART_MAIN);
    lv_obj_remove_flag(q, LV_OBJ_FLAG_SCROLLABLE);
}

/* ----------------------------------------------------------- EPD plumbing */

static void __epd_set_pixel(int32_t x, int32_t y, bool white)
{
    uint32_t off;
    uint8_t  mask;

    if (x < 0 || x >= X4PRO_EPD_W || y < 0 || y >= X4PRO_EPD_H) {
        return;
    }

    off  = (uint32_t)y * X4PRO_EPD_STRIDE + (uint32_t)x / 8U;
    mask = (uint8_t)(0x80U >> (unsigned)(x % 8));
    if (white) {
        s_epd_fb[off] |= mask;
    } else {
        s_epd_fb[off] = (uint8_t)(s_epd_fb[off] & (uint8_t)~mask);
    }
}

/**
 * @brief Set a pixel in the portrait view frame: rotates 90 deg CW into the
 *        native landscape EPD RAM (portrait top row = panel right edge).
 */
static void __epd_set_pixel_portrait(int32_t px, int32_t py, bool white)
{
    __epd_set_pixel(py, (X4PRO_EPD_H - 1) - px, white);
}

static bool __rgb565_is_white(lv_color16_t c)
{
    uint32_t r = c.red;
    uint32_t g = c.green;
    uint32_t b = c.blue;

    r = (r * 255U) / 31U;
    g = (g * 255U) / 63U;
    b = (b * 255U) / 31U;
    {
        uint32_t y = (77U * r + 150U * g + 29U * b) >> 8;
        return (y > 140U) ? true : false;
    }
}

static void __x4pro_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int32_t       w  = lv_area_get_width(area);
    int32_t       h  = lv_area_get_height(area);
    lv_color16_t *px = (lv_color16_t *)(void *)px_map;
    int32_t       row;
    int32_t       col;

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            lv_color16_t c     = px[(uint32_t)row * (uint32_t)w + (uint32_t)col];
            bool         white = __rgb565_is_white(c);

                        __epd_set_pixel_portrait(area->x1 + col, area->y1 + row, white);
        }
    }

    s_epd_dirty = TRUE;
    lv_display_flush_ready(disp);
}

static void __epd_push_timer_cb(lv_timer_t *t)
{
    OPERATE_RET rt;

    (void)t;
    if (!s_epd_dirty) {
        return;
    }
    if (s_force_full_once || s_use_full_refresh) {
        rt = board_x4pro_epd_display_full_refresh(s_epd_fb);
        s_force_full_once = FALSE;
    } else {
        rt = board_x4pro_epd_display(s_epd_fb);
        if (OPRT_OK != rt) {
            /* Stalled fast update: re-seed the baseline with one absolute
             * paint so the DU path recovers instead of looping on errors. */
            PR_WARN("[x4pro_demo] fast push failed %d, retrying full", rt);
            rt = board_x4pro_epd_display_full_refresh(s_epd_fb);
        }
    }
    if (OPRT_OK != rt) {
        PR_ERR("[x4pro_demo] epd push failed: %d", rt);
    }
    s_epd_dirty = FALSE;
}

/* -------------------------------------------------------------- touch io */

/**
 * @brief LVGL pointer indev read; single polling point for the GT911 so the
 *        status register is only consumed once. Home key is captured here.
 */
static void __touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    X4PRO_TOUCH_STATE_T ts;
    OPERATE_RET         rt;
    int32_t             px;
    int32_t             py;

    (void)indev;
    rt = board_x4pro_touch_poll(&ts);
    if (OPRT_OK != rt) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if (ts.home) {
        /* GT911 capacitive Home key: force one full refresh. */
        s_touch_home_seen  = TRUE;
        s_force_full_once  = TRUE;
    }

    if (ts.pressed) {
        /* GT911 reports native landscape panel coords; rotate into the
         * portrait view (inverse of the flush-path pixel rotation). */
        px = (X4PRO_EPD_H - 1) - (int32_t)ts.y;
        py = (int32_t)ts.x;

        if (!s_touch_pressed_prev) {
            s_touch_down_ms = tal_system_get_millisecond();
            s_touch_log_x   = px;
            s_touch_log_y   = py;
            PR_NOTICE("[x4pro_demo] touch DOWN: x=%ld y=%ld points=%u%s", (long)px, (long)py,
                      (unsigned)ts.points, ts.home ? " (+Home)" : "");
        } else {
            /* movement trace while held (>=8 px since the last log) */
            int32_t dx = px - s_touch_log_x;
            int32_t dy = py - s_touch_log_y;

            if (dx < -8 || dx > 8 || dy < -8 || dy > 8) {
                s_touch_log_x = px;
                s_touch_log_y = py;
                PR_DEBUG("[x4pro_demo] touch MOVE: x=%ld y=%ld", (long)px, (long)py);
            }
        }
        data->point.x   = px;
        data->point.y   = py;
        data->state     = LV_INDEV_STATE_PRESSED;
        s_touch_last_x  = px;
        s_touch_last_y  = py;
    } else {
        if (s_touch_pressed_prev) {
            PR_NOTICE("[x4pro_demo] touch UP: x=%ld y=%ld (held %lu ms)", (long)s_touch_last_x,
                      (long)s_touch_last_y,
                      (unsigned long)(tal_system_get_millisecond() - s_touch_down_ms));
        }
        data->state = LV_INDEV_STATE_RELEASED;
    }
    if (ts.home && !s_touch_home_prev) {
        PR_NOTICE("[x4pro_demo] Home key DOWN (capacitive, status bit 0x10)");
    } else if (!ts.home && s_touch_home_prev) {
        PR_NOTICE("[x4pro_demo] Home key UP");
    }
    s_touch_pressed_prev = ts.pressed;
    s_touch_home_prev    = ts.home;
}

/* ---------------------------------------------------------------- sd card */

static void __mount_sd_if_possible(void)
{
    OPERATE_RET rt;

    s_sd_mounted = FALSE;
    rt           = board_x4pro_sdcard_mount();
    if (OPRT_OK == rt) {
        s_sd_mounted = TRUE;
    }
}

static void __format_sd_line(char *buf, size_t len)
{
    uint64_t     total_bytes = 0;
    uint64_t     free_bytes  = 0;
    uint64_t     used_bytes  = 0;
    OPERATE_RET  rt;
    const double gib = (double)(1024ULL * 1024ULL * 1024ULL);

    if (!s_sd_mounted) {
        snprintf(buf, len, "SD: not mounted");
        return;
    }

    rt = board_x4pro_sdcard_get_usage(&total_bytes, &free_bytes);
    if (OPRT_OK != rt) {
        snprintf(buf, len, "SD: mounted (no df)");
        return;
    }

    used_bytes = (total_bytes > free_bytes) ? (total_bytes - free_bytes) : 0ULL;
    snprintf(buf, len, "SD: used %.2f / free %.2f (%.2f GB tot)", (double)used_bytes / gib, (double)free_bytes / gib,
             (double)total_bytes / gib);
}

static void __sd_smoke_once(void)
{
    OPERATE_RET rt;
    char         rb[96];
    size_t       br = 0;

    if (!s_sd_mounted) {
        snprintf(s_sd_smoke_msg, sizeof(s_sd_smoke_msg), "Insert a card for FATFS smoke test.");
        return;
    }

    (void)board_x4pro_sdcard_ensure_dir("/x4prolab");
    rt = board_x4pro_sdcard_write_file("/x4prolab/smoke.txt", "x4-pro-lab-ok\n", 14U);
    if (OPRT_OK != rt) {
        snprintf(s_sd_smoke_msg, sizeof(s_sd_smoke_msg), "Write failed: %d", rt);
        return;
    }
    (void)memset(rb, 0, sizeof(rb));
    rt = board_x4pro_sdcard_read_file_to_buffer("/x4prolab/smoke.txt", rb, sizeof(rb), 0U, &br);
    if (OPRT_OK != rt) {
        snprintf(s_sd_smoke_msg, sizeof(s_sd_smoke_msg), "Read failed: %d", rt);
        return;
    }
    snprintf(s_sd_smoke_msg, sizeof(s_sd_smoke_msg), "Write+read OK (%u bytes). /x4prolab/smoke.txt", (unsigned)br);
}

/* -------------------------------------------------------------- dashboard */

static void __refresh_keys_quadrant(uint8_t st, BOOL_T home)
{
    int i;

    if (NULL == s_key_lbl[0]) {
        return;
    }

    for (i = 0; i < 4; i++) {
        BOOL_T on;

        if (NULL == s_key_lbl[i]) {
            continue;
        }
        if (i == 3) {
            on = home;
        } else {
            on = (0U != (st & (1U << (unsigned)i)));
        }
        if (on) {
            lv_obj_set_style_bg_color(s_key_lbl[i], lv_color_hex(0x000000), LV_PART_MAIN);
            lv_obj_set_style_text_color(s_key_lbl[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(s_key_lbl[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_text_color(s_key_lbl[i], lv_color_hex(0x000000), LV_PART_MAIN);
        }
    }
}

static void __dashboard_refresh_slow(void)
{
    char        line[288];
    uint32_t    mv  = 0;
    uint8_t     pct = 0;
    OPERATE_RET rt;
    SYS_TIME_T  now  = tal_system_get_millisecond();
    uint32_t    up_s = (uint32_t)((now - s_boot_ms) / 1000U);

    if (X4PRO_SCR_DASHBOARD != s_screen) {
        return;
    }

    if (NULL != s_bar_time) {
        snprintf(line, sizeof(line), "up %lu s", (unsigned long)up_s);
        lv_label_set_text(s_bar_time, line);
    }

    if (NULL != s_q_bat) {
        rt = board_x4pro_battery_read(&mv, &pct);
        if (OPRT_OK == rt) {
            (void)board_x4pro_battery_get_charge_state(&s_charge_state);
            snprintf(line, sizeof(line), "CW2017 gauge\n%u%%\n%lu mV\nCharge: %s", (unsigned)pct, (unsigned long)mv,
                     __charge_text(s_charge_state));
        } else {
            snprintf(line, sizeof(line), "CW2017 gauge\nread err %d", rt);
        }
        lv_label_set_text(s_q_bat, line);
    }

    if (NULL != s_q_sd) {
        __format_sd_line(line, sizeof(line));
        {
            size_t n = strlen(line);
            if (n < sizeof(line) - 4U) {
                line[n++] = '\n';
                (void)strncpy(line + n, s_sd_smoke_msg, sizeof(line) - n);
                line[sizeof(line) - 1] = '\0';
            }
        }
        lv_label_set_text(s_q_sd, line);
    }

    if (NULL != s_q_epd) {
        snprintf(line, sizeof(line), "Touch: %ld, %ld\nRefresh: %s\nFrontlight: %u%% warm %u%%",
                 (long)s_touch_last_x, (long)s_touch_last_y, s_use_full_refresh ? "FULL" : "FAST",
                 (unsigned)s_fl_brightness, (unsigned)s_fl_warmth);
        lv_label_set_text(s_q_epd, line);
    }
}

static lv_obj_t *__quad_title(lv_obj_t *parent, const char *title)
{
    lv_obj_t *lbl = lv_label_create(parent);

    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_title_font(lbl);
    lv_obj_set_width(lbl, X4PRO_RENDER_W - 24);
    return lbl;
}

static lv_obj_t *__build_screen_base(void)
{
    lv_obj_t *scr = lv_screen_active();

    __screen_reset(scr);
    lv_obj_set_style_layout(scr, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(scr, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    __scr_pad_viewable(scr);
    return scr;
}

static void __build_dashboard(void)
{
    static const char *key_names[4] = {"Left", "Right", "PWR", "Home"};
    lv_obj_t          *scr;
    lv_obj_t          *bar;
    lv_obj_t          *bar_row;
    lv_obj_t          *mid;
    lv_obj_t          *q_bat;
    lv_obj_t          *q_sd;
    lv_obj_t          *q_keys;
    lv_obj_t          *q_epd;
    lv_obj_t          *key_grid;
    lv_obj_t          *foot;
    int32_t            i;
    uint8_t            st0 = 0;

    __sd_smoke_once();

    scr = __build_screen_base();
    s_screen = X4PRO_SCR_DASHBOARD;

    bar = lv_obj_create(scr);
    lv_obj_set_size(bar, X4PRO_RENDER_W, X4PRO_BAR_H);
    lv_obj_set_style_pad_all(bar, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xE8E8E8), LV_PART_MAIN);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    bar_row = lv_obj_create(bar);
    lv_obj_set_width(bar_row, X4PRO_RENDER_W - 16);
    lv_obj_set_height(bar_row, LV_SIZE_CONTENT);
    lv_obj_set_style_layout(bar_row, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(bar_row, LV_FLEX_FLOW_ROW, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(bar_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(bar_row, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_row, 0, LV_PART_MAIN);
    lv_obj_remove_flag(bar_row, LV_OBJ_FLAG_SCROLLABLE);

    s_bar_title = lv_label_create(bar_row);
    lv_label_set_text(s_bar_title, "X4 Pro Hardware Lab");
    lv_obj_set_style_text_color(s_bar_title, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_title_font(s_bar_title);

    s_bar_time = lv_label_create(bar_row);
    lv_label_set_text(s_bar_time, "up 0 s");
    lv_obj_set_style_text_color(s_bar_time, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_body_font(s_bar_time);

    /* 4 stacked full-width boxes (1 col x 4) */
    mid = lv_obj_create(scr);
    lv_obj_set_size(mid, X4PRO_RENDER_W, X4PRO_MID_H);
    lv_obj_set_style_layout(mid, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(mid, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_pad_row(mid, X4PRO_ROW_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(mid, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(mid, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE);

    q_bat = lv_obj_create(mid);
    lv_obj_set_size(q_bat, X4PRO_RENDER_W, X4PRO_ROW_H);
    __style_quad_frame(q_bat);
    lv_obj_set_style_layout(q_bat, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(q_bat, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_pad_row(q_bat, 4, LV_PART_MAIN);
    (void)__quad_title(q_bat, "Battery (CW2017)");
    s_q_bat = lv_label_create(q_bat);
    lv_label_set_text(s_q_bat, "-");
    lv_obj_set_style_text_color(s_q_bat, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_body_font(s_q_bat);
    lv_obj_set_width(s_q_bat, X4PRO_RENDER_W - 16);

    q_sd = lv_obj_create(mid);
    lv_obj_set_size(q_sd, X4PRO_RENDER_W, X4PRO_ROW_H);
    __style_quad_frame(q_sd);
    lv_obj_set_style_layout(q_sd, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(q_sd, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_pad_row(q_sd, 4, LV_PART_MAIN);
    (void)__quad_title(q_sd, "microSD / FATFS");
    s_q_sd = lv_label_create(q_sd);
    lv_label_set_text(s_q_sd, "-");
    lv_obj_set_style_text_color(s_q_sd, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_body_font(s_q_sd);
    lv_obj_set_width(s_q_sd, X4PRO_RENDER_W - 16);

    q_keys = lv_obj_create(mid);
    lv_obj_set_size(q_keys, X4PRO_RENDER_W, X4PRO_ROW_H);
    __style_quad_frame(q_keys);
    lv_obj_set_style_layout(q_keys, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(q_keys, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_pad_row(q_keys, 4, LV_PART_MAIN);
    (void)__quad_title(q_keys, "Keys + Home touch");

    key_grid = lv_obj_create(q_keys);
    lv_obj_remove_flag(key_grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(key_grid, X4PRO_RENDER_W - 16);
    lv_obj_set_style_layout(key_grid, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(key_grid, LV_FLEX_FLOW_ROW_WRAP, LV_PART_MAIN);
    lv_obj_set_style_pad_row(key_grid, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(key_grid, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(key_grid, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(key_grid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(key_grid, 0, LV_PART_MAIN);

    for (i = 0; i < 4; i++) {
        s_key_lbl[i] = lv_label_create(key_grid);
        lv_label_set_text(s_key_lbl[i], key_names[i]);
        lv_obj_set_width(s_key_lbl[i], (X4PRO_RENDER_W - 44) / 4);
        __apply_body_font(s_key_lbl[i]);
        lv_obj_set_style_pad_ver(s_key_lbl[i], 2, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(s_key_lbl[i], 4, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_key_lbl[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_key_lbl[i], lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_text_align(s_key_lbl[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    }

    q_epd = lv_obj_create(mid);
    lv_obj_set_size(q_epd, X4PRO_RENDER_W, X4PRO_ROW_H);
    __style_quad_frame(q_epd);
    lv_obj_set_style_layout(q_epd, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(q_epd, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_pad_row(q_epd, 4, LV_PART_MAIN);
    (void)__quad_title(q_epd, "EPD + touch");
    s_q_epd = lv_label_create(q_epd);
    lv_label_set_text(s_q_epd, "-");
    lv_obj_set_style_text_color(s_q_epd, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_body_font(s_q_epd);
    lv_obj_set_width(s_q_epd, X4PRO_RENDER_W - 16);

    foot = lv_obj_create(scr);
    lv_obj_set_size(foot, X4PRO_RENDER_W, X4PRO_FOOT_H);
    lv_obj_set_style_layout(foot, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(foot, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(foot, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(foot, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(foot, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
    lv_obj_set_style_border_width(foot, 0, LV_PART_MAIN);
    lv_obj_remove_flag(foot, LV_OBJ_FLAG_SCROLLABLE);

    s_foot_lbl = lv_label_create(foot);
    lv_obj_set_width(s_foot_lbl, X4PRO_RENDER_W - 24);
    lv_label_set_long_mode(s_foot_lbl, LV_LABEL_LONG_WRAP);
    lv_label_set_text_fmt(s_foot_lbl, "%s | Right key: settings", PLATFORM_BOARD);
    lv_obj_set_style_text_color(s_foot_lbl, lv_color_hex(0x000000), LV_PART_MAIN);
    /* nudge the hint up inside the 40 px bar so it sits off the bottom edge */
    lv_obj_set_style_translate_y(s_foot_lbl, -15, LV_PART_MAIN);
    __apply_body_font(s_foot_lbl);

    s_hub_slow_tick = 0U;
    __dashboard_refresh_slow();
    if (OPRT_OK == board_x4pro_buttons_get_state(&st0)) {
        __refresh_keys_quadrant(st0, FALSE);
    }
    lv_obj_invalidate(scr);
}

/* ------------------------------------------------------- settings screen */

static void __set_refresh_btn_label(void)
{
    if (NULL != s_set_refresh_btn) {
        lv_label_set_text(s_set_refresh_btn, s_use_full_refresh ? "Refresh: FULL" : "Refresh: FAST");
    }
}

static void __set_val_label(lv_obj_t *lbl, int32_t v)
{
    char line[16];

    if (NULL != lbl) {
        snprintf(line, sizeof(line), "%ld%%", (long)v);
        lv_label_set_text(lbl, line);
    }
}

static void __slider_style(lv_obj_t *sl)
{
    lv_obj_set_width(sl, X4PRO_RENDER_W - 260);
    lv_obj_set_style_bg_color(sl, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, lv_color_hex(0x000000), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, lv_color_hex(0x000000), LV_PART_KNOB);
    lv_obj_set_style_pad_all(sl, 8, LV_PART_KNOB);
}

static void __bright_slider_cb(lv_event_t *e)
{
    int32_t v;

    (void)e;
    if (NULL == s_set_bright_slider) {
        return;
    }
    v                = lv_slider_get_value(s_set_bright_slider);
    s_fl_brightness  = (uint8_t)v;
    (void)board_x4pro_frontlight_set_brightness(s_fl_brightness);
    __set_val_label(s_set_bright_val, v);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        PR_NOTICE("[x4pro_demo] frontlight brightness -> %u%%", (unsigned)s_fl_brightness);
    }
}

static void __warmth_slider_cb(lv_event_t *e)
{
    int32_t v;

    (void)e;
    if (NULL == s_set_warmth_slider) {
        return;
    }
    v            = lv_slider_get_value(s_set_warmth_slider);
    s_fl_warmth  = (uint8_t)v;
    (void)board_x4pro_frontlight_set_warmth(s_fl_warmth);
    __set_val_label(s_set_warmth_val, v);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        PR_NOTICE("[x4pro_demo] frontlight warmth -> %u%%", (unsigned)s_fl_warmth);
    }
}

static void __refresh_toggle_cb(lv_event_t *e)
{
    (void)e;
    s_use_full_refresh = !s_use_full_refresh;
    s_force_full_once  = TRUE; /* show the new mode cleanly once */
    __set_refresh_btn_label();
}

static void __settings_back_cb(lv_event_t *e)
{
    (void)e;
    __build_dashboard();
}

/**
 * @brief One row: caption label + widget + value label.
 */
static lv_obj_t *__settings_row(lv_obj_t *scr, const char *caption)
{
    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_t *lbl;

    lv_obj_set_size(row, X4PRO_RENDER_W, LV_SIZE_CONTENT);
    lv_obj_set_style_layout(row, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(row, LV_FLEX_FLOW_ROW, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(row, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(row, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lbl = lv_label_create(row);
    lv_label_set_text(lbl, caption);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_body_font(lbl);
    lv_obj_set_width(lbl, 120);
    return row;
}

static lv_obj_t *__mono_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_t *lbl = lv_label_create(btn);

    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(btn, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(btn, 10, LV_PART_MAIN);
    __apply_body_font(lbl);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return lbl;
}

static void __build_settings_screen(void)
{
    lv_obj_t *scr;
    lv_obj_t *title;
    lv_obj_t *row;
    lv_obj_t *back_row;

    scr = __build_screen_base();
    s_screen = X4PRO_SCR_SETTINGS;

    title = lv_label_create(scr);
    lv_label_set_text(title, "Display settings");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_title_font(title);
    lv_obj_set_width(title, X4PRO_RENDER_W - 16);
    lv_obj_set_style_pad_bottom(title, 8, LV_PART_MAIN);

    row = __settings_row(scr, "Brightness");
    s_set_bright_slider = lv_slider_create(row);
    lv_slider_set_range(s_set_bright_slider, 0, 100);
    lv_slider_set_value(s_set_bright_slider, (int32_t)s_fl_brightness, LV_ANIM_OFF);
    __slider_style(s_set_bright_slider);
    lv_obj_add_event_cb(s_set_bright_slider, __bright_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    s_set_bright_val = lv_label_create(row);
    lv_obj_set_style_text_color(s_set_bright_val, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_body_font(s_set_bright_val);
    __set_val_label(s_set_bright_val, (int32_t)s_fl_brightness);

    row = __settings_row(scr, "Warmth");
    s_set_warmth_slider = lv_slider_create(row);
    lv_slider_set_range(s_set_warmth_slider, 0, 100);
    lv_slider_set_value(s_set_warmth_slider, (int32_t)s_fl_warmth, LV_ANIM_OFF);
    __slider_style(s_set_warmth_slider);
    lv_obj_add_event_cb(s_set_warmth_slider, __warmth_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    s_set_warmth_val = lv_label_create(row);
    lv_obj_set_style_text_color(s_set_warmth_val, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_body_font(s_set_warmth_val);
    __set_val_label(s_set_warmth_val, (int32_t)s_fl_warmth);

    row = __settings_row(scr, "EPD refresh");
    s_set_refresh_btn = __mono_button(row, "-", __refresh_toggle_cb);
    __set_refresh_btn_label();

    back_row = lv_obj_create(scr);
    lv_obj_set_size(back_row, X4PRO_RENDER_W, LV_SIZE_CONTENT);
    lv_obj_set_style_layout(back_row, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(back_row, LV_FLEX_FLOW_ROW, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(back_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_PART_MAIN);
    lv_obj_set_style_pad_all(back_row, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(back_row, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(back_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(back_row, LV_OBJ_FLAG_SCROLLABLE);

    (void)__mono_button(back_row, "< Back (Left key)", __settings_back_cb);

    lv_obj_invalidate(scr);
}

/* ---------------------------------------------------------------- shutdown */

static void __lvgl_pump(int iterations)
{
    int j;

    for (j = 0; j < iterations; j++) {
        lv_timer_handler();
    }
}

/**
 * @brief Centered two-line shutdown-style screen, painted with one absolute
 *        full refresh so it survives on the e-ink through deep sleep.
 */
static void __paint_shutdown_screen(const char *main_txt, const char *sub_txt)
{
    lv_obj_t *scr;
    lv_obj_t *lbl_main;
    lv_obj_t *lbl_sub;

    s_screen = X4PRO_SCR_SHUTDOWN;

    scr = __build_screen_base();

    lbl_main = lv_label_create(scr);
    lv_label_set_text(lbl_main, main_txt);
    __apply_title_font(lbl_main);
    lv_obj_set_style_text_align(lbl_main, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(lbl_main, X4PRO_RENDER_W - 32);
    lv_obj_set_style_text_color(lbl_main, lv_color_hex(0x000000), LV_PART_MAIN);

    lbl_sub = lv_label_create(scr);
    lv_label_set_text(lbl_sub, sub_txt);
    __apply_body_font(lbl_sub);
    lv_obj_set_style_text_align(lbl_sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(lbl_sub, X4PRO_RENDER_W - 48);
    lv_obj_set_style_text_color(lbl_sub, lv_color_hex(0x000000), LV_PART_MAIN);

    lv_obj_invalidate(scr);
    __lvgl_pump(80);
    (void)board_x4pro_epd_display_full_refresh(s_epd_fb);
    s_epd_dirty = FALSE;
}

/**
 * @brief Hold-to-arm step: pop up a "Powering off" dialog over the current
 *        screen while the key stays pressed; the actual shutdown runs on
 *        key release.
 */
static void __power_off_arm(void)
{
    lv_obj_t *popup;
    lv_obj_t *panel;
    lv_obj_t *lbl;

    s_pwr_off_armed = TRUE;
    PR_NOTICE("[x4pro_demo] power off armed (held %lu ms): release PWR to power off",
              (unsigned long)(tal_system_get_millisecond() - s_pwr_down_ms));

    /* modal dialog on the top layer, above whatever screen is showing */
    popup = lv_obj_create(lv_layer_top());
    lv_obj_set_size(popup, X4PRO_VIEW_W, X4PRO_VIEW_H);
    lv_obj_set_style_bg_color(popup, lv_color_hex(0x000000), LV_PART_MAIN);
    /* 1-bit panel: the flush thresholds luma at 140/255, so the dim needs
     * opa >= ~46 % to render at all; 70 % lands solid black. */
    lv_obj_set_style_bg_opa(popup, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(popup, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(popup, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(popup, 0, LV_PART_MAIN);
    lv_obj_set_style_layout(popup, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(popup, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(popup, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_remove_flag(popup, LV_OBJ_FLAG_SCROLLABLE);
    s_pwr_popup = popup;

    panel = lv_obj_create(popup);
    lv_obj_set_size(panel, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 8, LV_PART_MAIN);
    lv_obj_set_style_layout(panel, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(panel, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(panel, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lbl = lv_label_create(panel);
    lv_label_set_text(lbl, "Powering off");
    __apply_title_font(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), LV_PART_MAIN);

    lbl = lv_label_create(panel);
    lv_label_set_text(lbl, "Release PWR to power off.");
    __apply_body_font(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), LV_PART_MAIN);

    /* absolute paint so the dialog lands even if the DU baseline is stale */
    lv_obj_invalidate(popup);
    __lvgl_pump(80);
    (void)board_x4pro_epd_display_full_refresh(s_epd_fb);
    s_epd_dirty = FALSE;
}

static void __user_power_off_sequence(void)
{
    if (s_power_off_started) {
        return;
    }
    s_power_off_started = TRUE;

    if (NULL != s_status_timer) {
        lv_timer_delete(s_status_timer);
        s_status_timer = NULL;
    }
    if (NULL != s_pwr_popup) {
        lv_obj_delete(s_pwr_popup); /* top-layer child: not cleared by screen rebuild */
        s_pwr_popup = NULL;
    }

    PR_NOTICE("[x4pro_demo] PWR released: powering off");

    /* last full-RAM exercise before sleep: flash the gray16 test pattern */
    PR_NOTICE("[x4pro_demo] power-off test pattern: gray16 bands (full refresh)");
    __fill_gray16_pattern_fb();
    (void)board_x4pro_epd_display_full_refresh(s_epd_fb);
    s_epd_dirty = FALSE;

    /* persistent centered "powered off" frame while in deep sleep */
    __paint_shutdown_screen("Powered off", "Press PWR to wake.");
    tal_system_sleep(400);

    if (s_sd_mounted) {
        (void)board_x4pro_sdcard_unmount();
        s_sd_mounted = FALSE;
    }

    (void)board_x4pro_power_shutdown();
}

/* ------------------------------------------------------------ status loop */

static void __status_timer_cb(lv_timer_t *t)
{
    OPERATE_RET rt_btn;
    uint8_t     st = 0;

    (void)t;
    /* The arm and shutdown paints pump lv_timer_handler, which would re-run
     * this timer mid-callback; keep exactly one instance active. */
    if (s_in_status_cb || s_power_off_started) {
        return;
    }
    s_in_status_cb = TRUE;

    rt_btn = board_x4pro_buttons_get_state(&st);
    if (OPRT_OK != rt_btn) {
        s_in_status_cb = FALSE;
        return;
    }

    /* Armed: the very first thing on release is the power-off, on every
     * screen, before any navigation/dashboard processing below. */
    if (s_pwr_off_armed && 0U == (st & X4PRO_BTN_POWER)) {
        s_in_status_cb = FALSE;
        __user_power_off_sequence();
        return;
    }

    /* edge logging for the physical keys (with held duration on release) */
    if (st != s_prev_btn) {
        static const char *key_names[3] = {"LEFT", "RIGHT", "PWR"};
        uint8_t            changed      = (uint8_t)(st ^ s_prev_btn);
        int                k;

        for (k = 0; k < 3; k++) {
            if (0U != (changed & (1U << (unsigned)k))) {
                if (0U != (st & (1U << (unsigned)k))) {
                    s_key_down_ms[k] = tal_system_get_millisecond();
                    PR_NOTICE("[x4pro_demo] key %s DOWN (GPIO%d, active-LOW)", key_names[k],
                              (k == 0) ? (int)X4PRO_BTN_LEFT_PIN : ((k == 1) ? (int)X4PRO_BTN_RIGHT_PIN
                                                                             : (int)X4PRO_BTN_POWER_PIN));
                } else {
                    PR_NOTICE("[x4pro_demo] key %s UP (held %lu ms)", key_names[k],
                              (unsigned long)(tal_system_get_millisecond() - s_key_down_ms[k]));
                }
            }
        }
    }

    /* Hold power to arm the shutdown; releasing the armed key powers off
     * (handled at the top of this callback). While armed, skip everything
     * else so no screen processing can shadow the release. A short press
     * (< threshold) never powers off. */
    if (0U != (st & X4PRO_BTN_POWER)) {
        /* Real elapsed time, not a tick count: on the dashboard the key
         * quadrant / status labels invalidate often and each resulting EPD
         * push stalls this timer far beyond its 40 ms period, so counting
         * +40 ms per tick drastically undercounts the hold. */
        if (0 == s_pwr_down_ms) {
            s_pwr_down_ms = tal_system_get_millisecond();
        }
        if (!s_pwr_off_armed &&
            (uint32_t)(tal_system_get_millisecond() - s_pwr_down_ms) >= X4PRO_PWR_HOLD_MS) {
            s_in_status_cb = FALSE;
            __power_off_arm();
            return;
        }
    } else {
        s_pwr_down_ms = 0;
    }
    if (s_pwr_off_armed) {
        s_prev_btn     = st;
        s_in_status_cb = FALSE;
        return;
    }

    /* edge-triggered navigation */
    if ((0U != (st & X4PRO_BTN_LEFT)) && (0U == (s_prev_btn & X4PRO_BTN_LEFT))) {
        if (X4PRO_SCR_DASHBOARD != s_screen) {
            __build_dashboard();
        }
    }
    if ((0U != (st & X4PRO_BTN_RIGHT)) && (0U == (s_prev_btn & X4PRO_BTN_RIGHT))) {
        if (X4PRO_SCR_SETTINGS != s_screen) {
            __build_settings_screen();
        }
    }
    s_prev_btn = st;

    if (X4PRO_SCR_DASHBOARD == s_screen) {
        __refresh_keys_quadrant(st, s_touch_home_seen);
        s_touch_home_seen = FALSE;

        /* charge-state transition: refresh the battery quadrant right away
         * instead of waiting for the slow hub tick. */
        if (s_charge_ui_pending) {
            s_charge_ui_pending = FALSE;
            __dashboard_refresh_slow();
        }

        s_hub_slow_tick++;
        if (s_hub_slow_tick >= X4PRO_HUB_SLOW_N) {
            s_hub_slow_tick = 0U;
            __dashboard_refresh_slow();
        }

        /* periodic hardware-lab diagnostics on the serial console */
        s_diag_tick++;
        if (s_diag_tick >= X4PRO_DIAG_TICKS) {
            uint32_t    mv  = 0;
            uint8_t     pct = 0;
            OPERATE_RET rb  = board_x4pro_battery_read(&mv, &pct);
            uint32_t    ups = (uint32_t)((tal_system_get_millisecond() - s_boot_ms) / 1000U);

            s_diag_tick = 0U;
            if (OPRT_OK == rb) {
                PR_NOTICE("[x4pro_demo] diag: up %lu s | bat %u%% %lu mV %s | SD %s | fl bri %u warm %u | scr %s",
                          (unsigned long)ups, (unsigned)pct, (unsigned long)mv, __charge_text(s_charge_state),
                          s_sd_mounted ? "mounted" : "absent", (unsigned)s_fl_brightness,
                          (unsigned)s_fl_warmth,
                          (X4PRO_SCR_SETTINGS == s_screen) ? "settings" : "dashboard");
            } else {
                PR_NOTICE("[x4pro_demo] diag: up %lu s | bat err %d | SD %s | fl bri %u warm %u | scr %s",
                          (unsigned long)ups, rb, s_sd_mounted ? "mounted" : "absent",
                          (unsigned)s_fl_brightness, (unsigned)s_fl_warmth,
                          (X4PRO_SCR_SETTINGS == s_screen) ? "settings" : "dashboard");
            }
        }
    }

    s_in_status_cb = FALSE;
}

/* ------------------------------------------------- boot EPD test patterns */

/**
 * @brief 16-band Bayer4 gray dither, drawn straight into the EPD framebuffer:
 *        validates the full-RAM write path across the whole panel.
 */
static void __fill_gray16_pattern_fb(void)
{
    static const uint8_t s_bayer4[4][4] = {
        {0, 8, 2, 10},
        {12, 4, 14, 6},
        {3, 11, 1, 9},
        {15, 7, 13, 5},
    };
    int32_t  x;
    int32_t  y;
    int32_t  band;
    uint32_t thr;
    uint8_t  m;
    bool     white;

    (void)memset(s_epd_fb, 0xFF, sizeof(s_epd_fb));

    /* portrait frame: 16 vertical bands across the 480 px width */
    for (y = 0; y < X4PRO_VIEW_H; y++) {
        for (x = 0; x < X4PRO_VIEW_W; x++) {
            band = (x * 16) / X4PRO_VIEW_W;
            if (band > 15) {
                band = 15;
            }
            thr = (uint32_t)band * 16U + 16U;
            if (thr > 256U) {
                thr = 256U;
            }
            m     = s_bayer4[(unsigned)x % 4U][(unsigned)y % 4U];
            white = (((uint32_t)m * 16U + 8U) >= thr) ? true : false;
            __epd_set_pixel_portrait(x, y, white);
        }
    }
}

/* ------------------------------------------------------------------ boot */

static void __build_splash_screen(void)
{
    lv_obj_t *scr = __build_screen_base();
    lv_obj_t *hello;
    lv_obj_t *brand;
    lv_obj_t *sub;

    lv_obj_set_style_flex_main_place(scr, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(scr, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_row(scr, 20, LV_PART_MAIN);

    hello = lv_label_create(scr);
    lv_label_set_text(hello, "Hello World");
    lv_obj_set_style_text_color(hello, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_align(hello, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
#if LV_FONT_MONTSERRAT_48
    lv_obj_set_style_text_font(hello, &lv_font_montserrat_48, LV_PART_MAIN);
#elif LV_FONT_MONTSERRAT_36
    lv_obj_set_style_text_font(hello, &lv_font_montserrat_36, LV_PART_MAIN);
#else
    __apply_title_font(hello);
#endif

    brand = lv_label_create(scr);
    lv_label_set_text(brand, "TuyaOpen");
    lv_obj_set_style_text_color(brand, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_align(brand, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
#if LV_FONT_MONTSERRAT_36
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_36, LV_PART_MAIN);
#elif LV_FONT_MONTSERRAT_24
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_24, LV_PART_MAIN);
#else
    __apply_title_font(brand);
#endif

    sub = lv_label_create(scr);
        lv_label_set_text(sub, "XTEINK X4 Pro | ESP32-S3 | EPD 800x480 | GT911");
    lv_obj_set_style_text_color(sub, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    __apply_body_font(sub);
    lv_obj_set_width(sub, X4PRO_RENDER_W - 32);

    lv_obj_invalidate(scr);
}

static void __switch_timer_cb(lv_timer_t *t)
{
    (void)t;
    __build_dashboard();
    s_hub_slow_tick = 0U;
    s_status_timer  = lv_timer_create(__status_timer_cb, X4PRO_INPUT_POLL_MS, NULL);
}

static void __lvgl_thread(void *arg)
{
    SYS_TIME_T last_ms = tal_system_get_millisecond();

    (void)arg;
    for (;;) {
        SYS_TIME_T now = tal_system_get_millisecond();

        if (now >= last_ms) {
            lv_tick_inc((uint32_t)(now - last_ms));
        }
        last_ms = now;
        lv_timer_handler();
        tal_system_sleep(2);
    }
}

void user_main(void)
{
        static uint8_t lv_buf[X4PRO_LV_BUF_BYTES];
    static char    lvgl_thread_name[] = "lvgl_x4pro";
    lv_timer_t    *t_switch;
    lv_indev_t    *indev;
    OPERATE_RET    rt = OPRT_OK;
    int            i;

    (void)rt;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("xteink_x4_pro_lvgl_demo (hardware lab)");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);

    (void)memset(s_epd_fb, 0xFF, sizeof(s_epd_fb));
    s_epd_dirty         = FALSE;
    s_boot_ms           = tal_system_get_millisecond();
    s_status_timer      = NULL;
    s_hub_slow_tick     = 0U;
    s_diag_tick         = 0U;
    s_pwr_down_ms       = 0;
    s_pwr_off_armed     = FALSE;
    s_pwr_popup         = NULL;
    s_power_off_started = FALSE;
    s_in_status_cb      = FALSE;
    s_prev_btn          = 0U;

    /* Hardware bring-up: individual results first, then a compact boot report
     * so the serial console doubles as the hardware test log. */
    {
        OPERATE_RET rt_hw   = board_register_hardware();
        uint32_t    bat_mv  = 0;
        uint8_t     bat_pct = 0;
        OPERATE_RET rt_bat  = board_x4pro_battery_read(&bat_mv, &bat_pct);

        /* charge-state notifications -> dashboard battery quadrant */
        (void)board_x4pro_battery_on_charge_state(__charge_state_cb);
        (void)board_x4pro_battery_get_charge_state(&s_charge_state);

        /* frontlight: adopt driver defaults, then light up at a sane level */
        if (OPRT_OK != board_x4pro_frontlight_get(&s_fl_brightness, &s_fl_warmth)) {
            s_fl_brightness = 50U;
            s_fl_warmth     = 0U;
        }
        (void)board_x4pro_frontlight_set_brightness(s_fl_brightness);
        (void)board_x4pro_frontlight_set_warmth(s_fl_warmth);

        __mount_sd_if_possible();

        {
            uint8_t btn0 = 0;

            (void)board_x4pro_buttons_get_state(&btn0);
            PR_NOTICE("[x4pro_demo]   buttons @boot: 0x%02X (bit0=LEFT GPIO0, bit1=RIGHT GPIO7, bit2=PWR GPIO3)",
                      (unsigned)btn0);
        }

        PR_NOTICE("[x4pro_demo] === hardware boot report ===");
        PR_NOTICE("[x4pro_demo]   rails+buttons+EPD+touch+fl: %s (%d)",
                  (OPRT_OK == rt_hw) ? "PASS" : "FAIL", rt_hw);
        PR_NOTICE("[x4pro_demo]   battery gauge (CW2017):     %s (%d)",
                  (OPRT_OK == rt_bat) ? "PASS" : "FAIL", rt_bat);
        if (OPRT_OK == rt_bat) {
            PR_NOTICE("[x4pro_demo]     charge %u%% (Li-ion curve), %lu mV, state %s", (unsigned)bat_pct,
                      (unsigned long)bat_mv, __charge_text(s_charge_state));
        }
        PR_NOTICE("[x4pro_demo]   microSD:                    %s", s_sd_mounted ? "PASS (mounted)" : "FAIL/absent");
        PR_NOTICE("[x4pro_demo] ============================");
    }

    lv_init();

        s_disp = lv_display_create(X4PRO_VIEW_W, X4PRO_VIEW_H);
    lv_display_set_default(s_disp);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_disp, lv_buf, NULL, (uint32_t)sizeof(lv_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, __x4pro_flush_cb);

    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_disp(indev, s_disp);
    lv_indev_set_read_cb(indev, __touch_read_cb);

    (void)lv_timer_create(__epd_push_timer_cb, X4PRO_EPD_PUSH_MS, NULL);

    /* splash: first frame must always be a full refresh */
    __build_splash_screen();
    for (i = 0; i < 80; i++) {
        lv_timer_handler();
    }
    (void)board_x4pro_epd_display_full_refresh(s_epd_fb);
    s_epd_dirty = FALSE;
    tal_system_sleep(X4PRO_SPLASH_HOLD_MS);

    /* Boot EPD validation paint (structure ported from the working X4 lab
     * demo). lv_tick has not started yet, so no timers fire during the pump
     * below — the pattern lands as exactly one absolute full refresh. */
    PR_NOTICE("[x4pro_demo] test pattern: gray16 Bayer dither bands (full refresh)");
    __fill_gray16_pattern_fb();
    (void)board_x4pro_epd_display_full_refresh(s_epd_fb);
    s_epd_dirty = FALSE;
    tal_system_sleep(X4PRO_GRAY16_HOLD_MS);

    t_switch = lv_timer_create(__switch_timer_cb, 1, NULL);
    lv_timer_set_repeat_count(t_switch, 1);

    {
        THREAD_CFG_T cfg = {0};

        cfg.stackDepth = 1024 * 10;
        cfg.priority   = THREAD_PRIO_1;
        cfg.thrdname   = lvgl_thread_name;
        TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&s_lvgl_thread, NULL, NULL, __lvgl_thread, NULL, &cfg));
    }
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    user_main();
    for (;;) {
        tal_system_sleep(500);
    }
}
#else

static THREAD_HANDLE ty_app_thread = NULL;

static void tuya_app_thread(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    static char  app_thread_name[] = "tuya_app_main";
    THREAD_CFG_T thrd_param;

    (void)memset(&thrd_param, 0, sizeof(thrd_param));
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority   = THREAD_PRIO_1;
    thrd_param.thrdname   = app_thread_name;

    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
