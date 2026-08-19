/**
 * @file example_xteink_x4_lvgl.c
 * @brief X4 LVGL lab: TuyaOpen splash, one-screen dashboard (menu bar + 4 quadrants + footer), deep sleep.
 * @version 0.3
 * @date 2026-05-04
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tal_system.h"
#include "tkl_output.h"

#include "lvgl.h"

#include "board_com_api.h"
#include "board_config.h"
#include "xteink_x4_buttons.h"

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

#define X4_PWR_HOLD_MS 3000U

#define X4_PWR_OFF_FLASH_UPDATES  5U
#define X4_PWR_OFF_FLASH_PAUSE_MS 240U

#define X4_EPD_W        ((int32_t)X4_EPD_WIDTH)
#define X4_EPD_H        ((int32_t)X4_EPD_HEIGHT)
#define X4_EPD_STRIDE   (X4_EPD_W / 8U)
#define X4_EPD_BUF_SIZE (X4_EPD_STRIDE * X4_EPD_H)

#define X4_LV_DRAW_LINES 24
#define X4_LV_BUF_PIXELS (X4_EPD_W * X4_LV_DRAW_LINES)
#define X4_LV_BUF_BYTES  (X4_LV_BUF_PIXELS * (int)sizeof(lv_color16_t))

#define X4_SPLASH_HOLD_MS    2800U
#define X4_GRAY16_HOLD_MS    1000U
#define X4_BULLSEYE_HOLD_MS  800U

/** Drawable region inside panel (CrossPoint viewable margins; board_config.h). */
#define X4_RENDER_W ((int32_t)X4_PANEL_VIEWABLE_WIDTH)
#define X4_RENDER_H ((int32_t)X4_PANEL_VIEWABLE_HEIGHT)

#define X4_BAR_H   84
#define X4_FOOT_H  64
#define X4_QUAD_GAP 3
#define X4_MID_H   (X4_RENDER_H - X4_BAR_H - X4_FOOT_H)
#define X4_QUAD_H  ((X4_MID_H - X4_QUAD_GAP) / 2)
#define X4_QUAD_W  ((X4_RENDER_W - X4_QUAD_GAP) / 2)

/* Input / display cadence (example-only; avoids slow 500 ms + 350 ms loops). */
#define X4_INPUT_POLL_MS 40U
#define X4_EPD_PUSH_MS   100U
/* On hub, refresh battery/SD/uptime every N polls; key bitmask updates every poll. */
#define X4_HUB_SLOW_N 5U

static uint8_t         s_epd_fb[X4_EPD_BUF_SIZE];
static lv_display_t   *s_disp;
static volatile BOOL_T s_epd_dirty;
static uint32_t        s_boot_ms;

static lv_obj_t *s_bar_title;
static lv_obj_t *s_bar_sub;
static lv_obj_t *s_bar_time;
static lv_obj_t *s_q_bat;
static lv_obj_t *s_q_sd;
static lv_obj_t *s_key_lbl[7];
static lv_obj_t *s_q_about;
static lv_obj_t *s_foot_lbl;

static BOOL_T        s_sd_mounted;
static THREAD_HANDLE s_lvgl_thread;

static lv_timer_t *s_status_timer;
static uint32_t    s_pwr_hold_ms;
static BOOL_T      s_power_off_started;

static int8_t    s_pwroff_cd = -1;
static uint16_t  s_pwroff_cd_ms_acc;
static lv_obj_t *s_lbl_cd_num;

static uint8_t s_hub_slow_tick;

static char s_sd_smoke_msg[192];

static void __build_dashboard(void);
static void __build_splash_screen(void);
static void __dashboard_refresh_slow(void);
static void __scr_pad_viewable(lv_obj_t *scr);
static void __dashboard_bar_keys(uint8_t st);

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
 * @brief Apply CrossPoint-style viewable-area padding to a screen (asymmetric bezel inset).
 * @param[in] scr screen object.
 * @return none
 */
static void __scr_pad_viewable(lv_obj_t *scr)
{
    lv_obj_set_style_pad_left(scr, (int32_t)X4_PANEL_VIEWABLE_LEFT_PX, LV_PART_MAIN);
    lv_obj_set_style_pad_right(scr, (int32_t)X4_PANEL_VIEWABLE_RIGHT_PX, LV_PART_MAIN);
    lv_obj_set_style_pad_top(scr, (int32_t)X4_PANEL_VIEWABLE_TOP_PX, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(scr, (int32_t)X4_PANEL_VIEWABLE_BOTTOM_PX, LV_PART_MAIN);
}

static void __style_quad_frame(lv_obj_t *q)
{
    lv_obj_set_style_bg_color(q, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_color(q, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_width(q, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(q, 6, LV_PART_MAIN);
    lv_obj_remove_flag(q, LV_OBJ_FLAG_SCROLLABLE);
}

static void __epd_set_pixel(int32_t x, int32_t y, bool white)
{
    uint32_t off;
    uint8_t  mask;

    if (x < 0 || x >= X4_EPD_W || y < 0 || y >= X4_EPD_H) {
        return;
    }

    off  = (uint32_t)y * X4_EPD_STRIDE + (uint32_t)x / 8U;
    mask = (uint8_t)(0x80U >> (unsigned)(x % 8));
    if (white) {
        s_epd_fb[off] |= mask;
    } else {
        s_epd_fb[off] = (uint8_t)(s_epd_fb[off] & (uint8_t)~mask);
    }
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

static void __x4_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
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

            __epd_set_pixel(area->x1 + col, area->y1 + row, white);
        }
    }

    s_epd_dirty = TRUE;
    lv_display_flush_ready(disp);
    (void)disp;
}

static void __epd_push_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_epd_dirty) {
        (void)board_x4_epd_display(s_epd_fb);
        s_epd_dirty = FALSE;
    }
}

static void __build_checker_screen(void)
{
    int32_t cols = 16;
    int32_t rows = 10;
    int32_t tw   = X4_RENDER_W / cols;
    int32_t th   = X4_RENDER_H / rows;
    int32_t cx;
    int32_t cy;

    lv_obj_t *scr = lv_screen_active();

    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    __scr_pad_viewable(scr);

    for (cy = 0; cy < rows; cy++) {
        for (cx = 0; cx < cols; cx++) {
            lv_obj_t *tile = lv_obj_create(scr);

            lv_obj_set_size(tile, tw, th);
            lv_obj_set_pos(tile, cx * tw, cy * th);
            lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
            if (((cx + cy) & 1) == 0) {
                lv_obj_set_style_bg_color(tile, lv_color_hex(0x000000), LV_PART_MAIN);
            } else {
                lv_obj_set_style_bg_color(tile, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            }
            lv_obj_set_style_border_width(tile, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(tile, 0, LV_PART_MAIN);
        }
    }

    lv_obj_invalidate(scr);
}

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

    for (y = 0; y < X4_EPD_H; y++) {
        for (x = 0; x < X4_EPD_W; x++) {
            band = (x * 16) / X4_EPD_W;
            if (band > 15) {
                band = 15;
            }
            thr = (uint32_t)band * 16U + 16U;
            if (thr > 256U) {
                thr = 256U;
            }
            m     = s_bayer4[(unsigned)x % 4U][(unsigned)y % 4U];
            white = (((uint32_t)m * 16U + 8U) >= thr) ? true : false;
            __epd_set_pixel(x, y, white);
        }
    }
}

static void __fill_fb_bullseye(void)
{
    int32_t cx = X4_EPD_W / 2;
    int32_t cy = X4_EPD_H / 2;
    int32_t x;
    int32_t y;

    (void)memset(s_epd_fb, 0xFF, sizeof(s_epd_fb));

    for (y = 0; y < X4_EPD_H; y++) {
        for (x = 0; x < X4_EPD_W; x++) {
            int32_t  dx   = x - cx;
            int32_t  dy   = y - cy;
            uint32_t d2   = (uint32_t)(dx * dx + dy * dy);
            uint32_t ring = d2 / (uint32_t)(38 * 38);
            bool     white  = ((ring & 1U) == 0U) ? true : false;

            __epd_set_pixel(x, y, white);
        }
    }
}

static void __mount_sd_if_possible(void)
{
    OPERATE_RET rt = OPRT_OK;

    s_sd_mounted = FALSE;
    rt           = board_x4_sdcard_mount();
    if (OPRT_OK == rt) {
        s_sd_mounted = TRUE;
    }
}

static void __format_sd_line(char *buf, size_t len)
{
    uint64_t     total_bytes = 0;
    uint64_t     free_bytes  = 0;
    uint64_t     used_bytes  = 0;
    OPERATE_RET  rt          = OPRT_OK;
    double       total_gb;
    double       free_gb;
    double       used_gb;
    const double gib = (double)(1024ULL * 1024ULL * 1024ULL);

    if (!s_sd_mounted) {
        snprintf(buf, len, "SD: not mounted");
        return;
    }

    rt = board_x4_sdcard_get_usage(&total_bytes, &free_bytes);
    if (OPRT_OK != rt) {
        snprintf(buf, len, "SD: mounted (no df)");
        return;
    }

    used_bytes = (total_bytes > free_bytes) ? (total_bytes - free_bytes) : 0ULL;
    total_gb   = (double)total_bytes / gib;
    free_gb    = (double)free_bytes / gib;
    used_gb    = (double)used_bytes / gib;

    snprintf(buf, len, "SD: used %.2f / free %.2f (%.2f GB tot)", used_gb, free_gb, total_gb);
}

static void __dashboard_bar_keys(uint8_t st)
{
    char line[64];

    if (NULL == s_bar_sub) {
        return;
    }

    snprintf(line, sizeof(line), "Keys 0x%02X  (single-screen lab)", (unsigned)st);
    lv_label_set_text(s_bar_sub, line);
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

    (void)board_x4_sdcard_ensure_dir("/x4lab");
    rt = board_x4_sdcard_write_file("/x4lab/smoke.txt", "x4-lab-ok\n", 9U);
    if (OPRT_OK != rt) {
        snprintf(s_sd_smoke_msg, sizeof(s_sd_smoke_msg), "Write failed: %d", rt);
        return;
    }
    (void)memset(rb, 0, sizeof(rb));
    rt = board_x4_sdcard_read_file_to_buffer("/x4lab/smoke.txt", rb, sizeof(rb), 0U, &br);
    if (OPRT_OK != rt) {
        snprintf(s_sd_smoke_msg, sizeof(s_sd_smoke_msg), "Read failed: %d", rt);
        return;
    }
    snprintf(s_sd_smoke_msg, sizeof(s_sd_smoke_msg), "Write+read OK (%u bytes). Path /x4lab/smoke.txt", (unsigned)br);
}

static void __dashboard_refresh_slow(void)
{
    char        line[288];
    uint32_t    mv   = 0;
    uint8_t     pct  = 0;
    OPERATE_RET rt   = OPRT_OK;
    bool        chg  = false;
    SYS_TIME_T  now  = tal_system_get_millisecond();
    uint32_t    up_s = (uint32_t)((now - s_boot_ms) / 1000U);

    if (NULL != s_bar_time) {
        snprintf(line, sizeof(line), "up %lu s", (unsigned long)up_s);
        lv_label_set_text(s_bar_time, line);
    }

    if (NULL != s_q_bat) {
        rt = board_x4_battery_read(&mv, &pct);
        if (OPRT_OK == rt) {
            (void)board_x4_charge_sense_get(&chg);
            snprintf(line, sizeof(line), "Battery / ADC\n%s %u%%\n%lu mV\nCharge GPIO: %s", chg ? "Charging" : "Idle",
                     (unsigned)pct, (unsigned long)mv, chg ? "HIGH" : "LOW");
        } else {
            snprintf(line, sizeof(line), "Battery / ADC\nread err %d", rt);
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
}

static void __refresh_keys_quadrant(uint8_t st)
{
    int i;

    if (NULL == s_key_lbl[0]) {
        return;
    }

    for (i = 0; i < 7; i++) {
        if (NULL == s_key_lbl[i]) {
            continue;
        }
        if (0U != (st & (1U << (unsigned)i))) {
            lv_obj_set_style_bg_color(s_key_lbl[i], lv_color_hex(0x000000), LV_PART_MAIN);
            lv_obj_set_style_text_color(s_key_lbl[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(s_key_lbl[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_text_color(s_key_lbl[i], lv_color_hex(0x000000), LV_PART_MAIN);
        }
    }
}

static void __clear_screen_ptrs(void)
{
    int i;

    s_bar_title = NULL;
    s_bar_sub   = NULL;
    s_bar_time  = NULL;
    s_q_bat     = NULL;
    s_q_sd      = NULL;
    s_q_about   = NULL;
    s_foot_lbl  = NULL;
    for (i = 0; i < 7; i++) {
        s_key_lbl[i] = NULL;
    }
}

static lv_obj_t *__quad_title(lv_obj_t *parent, const char *title)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_title_font(lbl);
    lv_obj_set_width(lbl, X4_QUAD_W - 24);
    return lbl;
}

static void __build_splash_screen(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *hello;
    lv_obj_t *brand;
    lv_obj_t *sub;

    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_layout(scr, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(scr, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(scr, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(scr, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_row(scr, 20, LV_PART_MAIN);
    __scr_pad_viewable(scr);

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
    lv_label_set_text(sub, "XTEINK X4 | ESP32-C3 | SSD1677 800x480");
    lv_obj_set_style_text_color(sub, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    __apply_body_font(sub);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(sub, X4_RENDER_W - 32);

    lv_obj_invalidate(scr);
}

static void __build_dashboard(void)
{
    static const char *key_names[7] = {"Back", "OK", "Left", "Right", "Up", "Down", "PWR"};
    lv_obj_t          *scr;
    lv_obj_t          *bar;
    lv_obj_t          *bar_row;
    lv_obj_t          *mid;
    lv_obj_t          *row1;
    lv_obj_t          *row2;
    lv_obj_t          *q_tl;
    lv_obj_t          *q_tr;
    lv_obj_t          *q_bl;
    lv_obj_t          *q_br;
    lv_obj_t          *stripe_row;
    lv_obj_t          *key_grid;
    lv_obj_t          *foot;
    int32_t            i;
    int32_t            bar_w;
    uint8_t            st0 = 0;

    __sd_smoke_once();

    scr = lv_screen_active();
    __clear_screen_ptrs();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_layout(scr, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(scr, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    __scr_pad_viewable(scr);

    bar = lv_obj_create(scr);
    lv_obj_set_size(bar, X4_RENDER_W, X4_BAR_H);
    lv_obj_set_style_layout(bar, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(bar, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_pad_row(bar, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xE8E8E8), LV_PART_MAIN);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    bar_row = lv_obj_create(bar);
    lv_obj_set_width(bar_row, X4_RENDER_W - 16);
    lv_obj_set_height(bar_row, LV_SIZE_CONTENT);
    lv_obj_set_style_layout(bar_row, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(bar_row, LV_FLEX_FLOW_ROW, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(bar_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(bar_row, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_row, 0, LV_PART_MAIN);
    lv_obj_remove_flag(bar_row, LV_OBJ_FLAG_SCROLLABLE);

    s_bar_title = lv_label_create(bar_row);
    lv_label_set_text(s_bar_title, "TuyaOpen + XTEInk X4 | Demo App Hardware Func Test");
    lv_obj_set_style_text_color(s_bar_title, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_title_font(s_bar_title);

    s_bar_time = lv_label_create(bar_row);
    lv_label_set_text(s_bar_time, "up 0 s");
    lv_obj_set_style_text_color(s_bar_time, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_body_font(s_bar_time);

    // s_bar_sub = lv_label_create(bar);
    // lv_label_set_text(s_bar_sub, "Keys -");
    // lv_obj_set_width(s_bar_sub, X4_RENDER_W - 16);
    // lv_obj_set_style_text_color(s_bar_sub, lv_color_hex(0x101010), LV_PART_MAIN);
    // __apply_body_font(s_bar_sub);

    mid = lv_obj_create(scr);
    lv_obj_set_size(mid, X4_RENDER_W, X4_MID_H);
    lv_obj_set_style_layout(mid, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(mid, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_pad_row(mid, X4_QUAD_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(mid, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(mid, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE);

    row1 = lv_obj_create(mid);
    lv_obj_set_size(row1, X4_RENDER_W, X4_QUAD_H);
    lv_obj_set_style_layout(row1, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(row1, LV_FLEX_FLOW_ROW, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row1, X4_QUAD_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row1, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(row1, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(row1, LV_OBJ_FLAG_SCROLLABLE);

    row2 = lv_obj_create(mid);
    lv_obj_set_size(row2, X4_RENDER_W, X4_QUAD_H);
    lv_obj_set_style_layout(row2, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(row2, LV_FLEX_FLOW_ROW, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row2, X4_QUAD_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row2, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(row2, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_remove_flag(row2, LV_OBJ_FLAG_SCROLLABLE);

    q_tl = lv_obj_create(row1);
    lv_obj_set_size(q_tl, X4_QUAD_W, X4_QUAD_H);
    __style_quad_frame(q_tl);
    lv_obj_set_style_layout(q_tl, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(q_tl, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_pad_row(q_tl, 4, LV_PART_MAIN);
    (void)__quad_title(q_tl, "Power / battery");
    s_q_bat = lv_label_create(q_tl);
    lv_label_set_text(s_q_bat, "-");
    lv_obj_set_style_text_color(s_q_bat, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_body_font(s_q_bat);
    lv_obj_set_width(s_q_bat, X4_QUAD_W - 16);

    q_tr = lv_obj_create(row1);
    lv_obj_set_size(q_tr, X4_QUAD_W, X4_QUAD_H);
    __style_quad_frame(q_tr);
    lv_obj_set_style_layout(q_tr, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(q_tr, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_pad_row(q_tr, 4, LV_PART_MAIN);
    (void)__quad_title(q_tr, "microSD / FATFS");
    s_q_sd = lv_label_create(q_tr);
    lv_label_set_text(s_q_sd, "-");
    lv_obj_set_style_text_color(s_q_sd, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_body_font(s_q_sd);
    lv_obj_set_width(s_q_sd, X4_QUAD_W - 16);

    q_bl = lv_obj_create(row2);
    lv_obj_set_size(q_bl, X4_QUAD_W, X4_QUAD_H);
    __style_quad_frame(q_bl);
    lv_obj_set_style_layout(q_bl, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(q_bl, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_pad_row(q_bl, 2, LV_PART_MAIN);
    (void)__quad_title(q_bl, "Keys (ADC ladder)");

    key_grid = lv_obj_create(q_bl);
    lv_obj_remove_flag(key_grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(key_grid, X4_QUAD_W - 16);
    lv_obj_set_style_layout(key_grid, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(key_grid, LV_FLEX_FLOW_ROW_WRAP, LV_PART_MAIN);
    lv_obj_set_style_pad_row(key_grid, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(key_grid, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(key_grid, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(key_grid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(key_grid, 0, LV_PART_MAIN);

    for (i = 0; i < 7; i++) {
        char line[40];
        snprintf(line, sizeof(line), "%u %s", (unsigned)i, key_names[i]);
        s_key_lbl[i] = lv_label_create(key_grid);
        lv_label_set_text(s_key_lbl[i], line);
        lv_obj_set_width(s_key_lbl[i], (X4_QUAD_W - 28) / 2);
        __apply_body_font(s_key_lbl[i]);
        lv_obj_set_style_pad_ver(s_key_lbl[i], 2, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(s_key_lbl[i], 4, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_key_lbl[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_key_lbl[i], lv_color_hex(0x000000), LV_PART_MAIN);
    }

    q_br = lv_obj_create(row2);
    lv_obj_set_size(q_br, X4_QUAD_W, X4_QUAD_H);
    __style_quad_frame(q_br);
    lv_obj_set_style_layout(q_br, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(q_br, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_pad_row(q_br, 4, LV_PART_MAIN);
    (void)__quad_title(q_br, "EPD / SPI + about");
    stripe_row = lv_obj_create(q_br);
    lv_obj_remove_flag(stripe_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(stripe_row, X4_QUAD_W - 16, 56);
    lv_obj_set_style_layout(stripe_row, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(stripe_row, LV_FLEX_FLOW_ROW, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(stripe_row, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_column(stripe_row, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(stripe_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(stripe_row, 0, LV_PART_MAIN);
    bar_w = (X4_QUAD_W - 40) / 10;
    if (bar_w < 8) {
        bar_w = 8;
    }
    for (i = 0; i < 10; i++) {
        lv_obj_t *b = lv_obj_create(stripe_row);
        lv_obj_set_size(b, bar_w, 44);
        lv_obj_set_style_bg_color(b, lv_color_hex((i & 1) ? 0x000000 : 0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(b, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    }
    s_q_about = lv_label_create(q_br);
    lv_label_set_text(s_q_about,
                       "SSD1677 soft-SPI\n"
                       "Built " __DATE__ "\n"
                       "Long PWR 3s -> sleep");
    lv_obj_set_style_text_color(s_q_about, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_body_font(s_q_about);
    lv_obj_set_width(s_q_about, X4_QUAD_W - 16);

    foot = lv_obj_create(scr);
    lv_obj_set_size(foot, X4_RENDER_W, X4_FOOT_H);
    lv_obj_set_style_layout(foot, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(foot, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(foot, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(foot, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(foot, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
    lv_obj_set_style_border_width(foot, 0, LV_PART_MAIN);
    lv_obj_remove_flag(foot, LV_OBJ_FLAG_SCROLLABLE);

    s_foot_lbl = lv_label_create(foot);
    lv_obj_set_width(s_foot_lbl, X4_RENDER_W - 24);
    lv_label_set_long_mode(s_foot_lbl, LV_LABEL_LONG_WRAP);
    lv_label_set_text_fmt(s_foot_lbl, "%s | %s", PLATFORM_BOARD, PROJECT_NAME);
    lv_obj_set_style_text_color(s_foot_lbl, lv_color_hex(0x000000), LV_PART_MAIN);
    __apply_body_font(s_foot_lbl);

    s_hub_slow_tick = 0U;
    __dashboard_refresh_slow();
    if (OPRT_OK == board_x4_buttons_get_state(&st0)) {
        __dashboard_bar_keys(st0);
        __refresh_keys_quadrant(st0);
    }
    lv_obj_invalidate(scr);
}

static void __pwroff_begin_countdown(void)
{
    lv_obj_t *scr;
    lv_obj_t *lbl_title;

    s_pwr_hold_ms      = 0U;
    s_pwroff_cd        = 3;
    s_pwroff_cd_ms_acc = 0U;

    scr = lv_screen_active();
    lv_obj_clean(scr);
    __clear_screen_ptrs();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_layout(scr, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(scr, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(scr, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(scr, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_row(scr, 16, LV_PART_MAIN);
    __scr_pad_viewable(scr);

    lbl_title = lv_label_create(scr);
    lv_label_set_text(lbl_title, "Shutting down");
    __apply_title_font(lbl_title);
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_width(lbl_title, X4_RENDER_W - 32);

    s_lbl_cd_num = lv_label_create(scr);
    lv_label_set_text(s_lbl_cd_num, "3");
#if LV_FONT_MONTSERRAT_36
    lv_obj_set_style_text_font(s_lbl_cd_num, &lv_font_montserrat_36, LV_PART_MAIN);
#elif LV_FONT_MONTSERRAT_24
    lv_obj_set_style_text_font(s_lbl_cd_num, &lv_font_montserrat_24, LV_PART_MAIN);
#endif
    lv_obj_set_style_text_align(s_lbl_cd_num, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_cd_num, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_width(s_lbl_cd_num, X4_RENDER_W - 32);

    lv_obj_invalidate(scr);
}

static void __lvgl_pump(int iterations)
{
    int j;

    for (j = 0; j < iterations; j++) {
        lv_timer_handler();
    }
}

static void __epd_sync_from_lvgl(void)
{
    __lvgl_pump(120);
    s_epd_dirty = TRUE;
    (void)board_x4_epd_display(s_epd_fb);
    s_epd_dirty = FALSE;
    __lvgl_pump(40);
}

static void __styled_powered_off(lv_obj_t *scr, lv_obj_t *main, lv_obj_t *sub, bool invert)
{
    if (invert) {
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_text_color(main, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_color(sub, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_color(main, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_text_color(sub, lv_color_hex(0x000000), LV_PART_MAIN);
    }
}

static void __flash_powered_off_screen(lv_obj_t *scr, lv_obj_t *lbl_main, lv_obj_t *lbl_sub)
{
    uint32_t pass;

    __styled_powered_off(scr, lbl_main, lbl_sub, false);
    lv_obj_invalidate(scr);
    __lvgl_pump(80);
    __epd_sync_from_lvgl();

    for (pass = 0U; pass < X4_PWR_OFF_FLASH_UPDATES; pass++) {
        bool invert = (((unsigned)pass + 1U) & 1U) != 0U;

        __styled_powered_off(scr, lbl_main, lbl_sub, invert);
        lv_obj_invalidate(scr);
        __epd_sync_from_lvgl();
        tal_system_sleep(X4_PWR_OFF_FLASH_PAUSE_MS);
    }

    __styled_powered_off(scr, lbl_main, lbl_sub, false);
    lv_obj_invalidate(scr);
    __epd_sync_from_lvgl();
}

static void __user_power_off_sequence(void)
{
    lv_obj_t *scr;
    lv_obj_t *lbl_main;
    lv_obj_t *lbl_sub;

    if (s_power_off_started) {
        return;
    }
    s_power_off_started = TRUE;
    s_pwroff_cd         = -1;
    s_pwroff_cd_ms_acc  = 0U;
    s_lbl_cd_num        = NULL;

    if (NULL != s_status_timer) {
        lv_timer_delete(s_status_timer);
        s_status_timer = NULL;
    }

    scr = lv_screen_active();
    lv_obj_clean(scr);
    __clear_screen_ptrs();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_layout(scr, LV_LAYOUT_FLEX, LV_PART_MAIN);
    lv_obj_set_style_flex_flow(scr, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
    lv_obj_set_style_flex_main_place(scr, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_flex_cross_place(scr, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_row(scr, 16, LV_PART_MAIN);
    __scr_pad_viewable(scr);

    lbl_main = lv_label_create(scr);
    lv_label_set_text(lbl_main, "DEEP SLEEP");
    __apply_title_font(lbl_main);
    lv_obj_set_style_text_align(lbl_main, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(lbl_main, X4_RENDER_W - 32);
    lv_label_set_long_mode(lbl_main, LV_LABEL_LONG_WRAP);

    lbl_sub = lv_label_create(scr);
    lv_label_set_text(lbl_sub, "Hold PWR 3s after wake to run.\nRelease sooner to stay asleep.");
    __apply_body_font(lbl_sub);
    lv_obj_set_style_text_align(lbl_sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(lbl_sub, X4_RENDER_W - 48);
    lv_label_set_long_mode(lbl_sub, LV_LABEL_LONG_WRAP);

    lv_obj_invalidate(scr);
    __flash_powered_off_screen(scr, lbl_main, lbl_sub);

    __lvgl_pump(60);
    s_epd_dirty = TRUE;
    (void)board_x4_epd_display_full_refresh(s_epd_fb);
    s_epd_dirty = FALSE;
    tal_system_sleep(400);

    if (s_sd_mounted) {
        (void)board_x4_sdcard_unmount();
        s_sd_mounted = FALSE;
    }

    (void)board_x4_power_shutdown();
}

static void __status_timer_cb(lv_timer_t *t)
{
    OPERATE_RET rt_btn = OPRT_OK;
    uint8_t     st     = 0;
    uint32_t    period_ms;

    if (s_power_off_started) {
        return;
    }

    (void)t;
    period_ms = X4_INPUT_POLL_MS;

    if (s_pwroff_cd >= 0) {
        s_pwroff_cd = -1;
        s_lbl_cd_num = NULL;
    }

    rt_btn = board_x4_buttons_get_state(&st);
    if (OPRT_OK != rt_btn) {
        s_pwr_hold_ms   = 0U;
        s_hub_slow_tick = 0U;
        __dashboard_refresh_slow();
        return;
    }

    if (0U != (st & X4_BTN_POWER)) {
        s_pwr_hold_ms += period_ms;
    } else {
        s_pwr_hold_ms = 0U;
    }
    if (s_pwr_hold_ms >= X4_PWR_HOLD_MS) {
        __user_power_off_sequence();
        return;
    }

    __dashboard_bar_keys(st);
    __refresh_keys_quadrant(st);
    s_hub_slow_tick++;
    if (s_hub_slow_tick >= X4_HUB_SLOW_N) {
        s_hub_slow_tick = 0U;
        __dashboard_refresh_slow();
    }
}

static void __switch_timer_cb(lv_timer_t *t)
{
    (void)t;
    __build_dashboard();
    s_hub_slow_tick = 0U;
    s_status_timer  = lv_timer_create(__status_timer_cb, X4_INPUT_POLL_MS, NULL);
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
    static uint8_t lv_buf[X4_LV_BUF_BYTES];
    static char    lvgl_thread_name[] = "lvgl_x4";
    lv_timer_t    *t_epd;
    lv_timer_t    *t_switch;
    OPERATE_RET    rt = OPRT_OK;
    int            i;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("xteink_x4_lvgl_demo (hardware lab)");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);

    (void)memset(s_epd_fb, 0xFF, sizeof(s_epd_fb));
    s_epd_dirty         = FALSE;
    s_boot_ms           = tal_system_get_millisecond();
    s_status_timer      = NULL;
    s_hub_slow_tick     = 0U;
    s_pwr_hold_ms       = 0U;
    s_power_off_started = FALSE;
    s_pwroff_cd         = -1;
    s_pwroff_cd_ms_acc  = 0U;
    s_lbl_cd_num        = NULL;
    TUYA_CALL_ERR_LOG(board_register_hardware());
    {
        X4_WAKEUP_CLASS_E cls;

        if (OPRT_OK == board_x4_sleep_classify_wakeup(&cls)) {
            if (X4_WAKEUP_CLASS_AFTER_USB_POWER == cls) {
                PR_NOTICE("X4: wake classified as USB power boot -> deep sleep (CrossPoint policy)");
                (void)board_x4_power_shutdown();
            } else if (X4_WAKEUP_CLASS_POWER_BUTTON == cls) {
                rt = board_x4_power_verify_gpio_wake((uint32_t)X4_PWR_HOLD_MS, FALSE);
                TUYA_CALL_ERR_LOG(rt);
            }
        }
    }
    __mount_sd_if_possible();

    lv_init();

    s_disp = lv_display_create(X4_EPD_W, X4_EPD_H);
    lv_display_set_default(s_disp);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_disp, lv_buf, NULL, (uint32_t)sizeof(lv_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, __x4_flush_cb);

    t_epd = lv_timer_create(__epd_push_timer_cb, X4_EPD_PUSH_MS, NULL);
    (void)t_epd;

    __build_splash_screen();
    for (i = 0; i < 80; i++) {
        lv_timer_handler();
    }
    (void)board_x4_epd_display_full_refresh(s_epd_fb);
    s_epd_dirty = FALSE;
    tal_system_sleep((uint32_t)X4_SPLASH_HOLD_MS);

    __build_checker_screen();
    for (i = 0; i < 24; i++) {
        lv_timer_handler();
    }
    (void)board_x4_epd_display_full_refresh(s_epd_fb);
    s_epd_dirty = FALSE;
    tal_system_sleep(400);

    __fill_gray16_pattern_fb();
    (void)board_x4_epd_display_full_refresh(s_epd_fb);
    s_epd_dirty = FALSE;
    tal_system_sleep((uint32_t)X4_GRAY16_HOLD_MS);

    __fill_fb_bullseye();
    (void)board_x4_epd_display_full_refresh(s_epd_fb);
    s_epd_dirty = FALSE;
    tal_system_sleep((uint32_t)X4_BULLSEYE_HOLD_MS);

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
