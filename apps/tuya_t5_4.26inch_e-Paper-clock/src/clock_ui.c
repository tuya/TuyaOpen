/**
 * @file clock_ui.c
 * @brief UI rendering for the 4.26" e-Paper clock example.
 *
 * This file implements the framebuffer rendering logic for the e-Paper clock,
 * including time display and status layout.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */
#include <stdio.h>
#include <string.h>

#include "clock_ui.h"

#include "EPD_4in26.h"
#include "GUI_Paint.h"

static UWORD g_fg = BLACK;
static UWORD g_bg = WHITE;

static void __ui_theme_apply(const clock_ui_state_t *state)
{
    // Default to light theme for safety.
    clock_theme_t theme = CLOCK_THEME_LIGHT;
    if (state) {
        theme = state->theme;
    }

    if (theme == CLOCK_THEME_DARK) {
        g_fg = WHITE;
        g_bg = BLACK;
    } else {
        g_fg = BLACK;
        g_bg = WHITE;
    }
}

static const char *weekday_str(int wday)
{
    static const char *names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (wday < 0 || wday > 6) {
        return "?";
    }
    return names[wday];
}

static void draw_string_clipped(UWORD x, UWORD y, UWORD max_w, const char *s, sFONT *font)
{
    if (!s || !font || max_w == 0) {
        return;
    }

    UWORD max_chars = (UWORD)(max_w / font->Width);
    if (max_chars == 0) {
        return;
    }

    char buf[64];
    size_t n = strlen(s);
    if (n > (size_t)max_chars) {
        n = (size_t)max_chars;
    }
    if (n >= sizeof(buf)) {
        n = sizeof(buf) - 1;
    }
    memcpy(buf, s, n);
    buf[n] = '\0';

    Paint_DrawString_EN(x, y, buf, font, g_fg, g_bg);
}

static void draw_string_centered_clipped(UWORD x, UWORD y, UWORD w, const char *s, sFONT *font)
{
    if (!s || !font || w == 0) {
        return;
    }

    UWORD max_chars = (UWORD)(w / font->Width);
    if (max_chars == 0) {
        return;
    }

    char buf[96];
    size_t n = strlen(s);
    if (n > (size_t)max_chars) {
        n = (size_t)max_chars;
    }
    if (n >= sizeof(buf)) {
        n = sizeof(buf) - 1;
    }
    memcpy(buf, s, n);
    buf[n] = '\0';

    UWORD text_w = (UWORD)(strlen(buf) * font->Width);
    UWORD start_x = x;
    if (w > text_w) {
        start_x = x + (w - text_w) / 2;
    }
    Paint_DrawString_EN(start_x, y, buf, font, g_fg, g_bg);
}

static void fill_rect(UWORD x, UWORD y, UWORD w, UWORD h, UWORD color)
{
    if (w == 0 || h == 0) {
        return;
    }
    Paint_DrawRectangle(x, y, x + w - 1, y + h - 1, color, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}

static void draw_7seg_digit(UWORD x, UWORD y, int digit, UWORD w, UWORD h, UWORD t)
{
    // Segment map: a b c d e f g
    static const uint8_t segs[10] = {
        0b1111110, // 0
        0b0110000, // 1
        0b1101101, // 2
        0b1111001, // 3
        0b0110011, // 4
        0b1011011, // 5
        0b1011111, // 6
        0b1110000, // 7
        0b1111111, // 8
        0b1111011, // 9
    };

    if (digit < 0 || digit > 9) {
        return;
    }

    uint8_t m = segs[digit];
    UWORD half = h / 2;
    UWORD mid_y = y + half - (t / 2);
    UWORD vert_h = half - t - (t / 2);

    // a (top)
    if (m & 0b1000000) {
        fill_rect(x + t, y, w - 2 * t, t, g_fg);
    }
    // b (upper right)
    if (m & 0b0100000) {
        fill_rect(x + w - t, y + t, t, vert_h, g_fg);
    }
    // c (lower right)
    if (m & 0b0010000) {
        fill_rect(x + w - t, mid_y + t, t, vert_h, g_fg);
    }
    // d (bottom)
    if (m & 0b0001000) {
        fill_rect(x + t, y + h - t, w - 2 * t, t, g_fg);
    }
    // e (lower left)
    if (m & 0b0000100) {
        fill_rect(x, mid_y + t, t, vert_h, g_fg);
    }
    // f (upper left)
    if (m & 0b0000010) {
        fill_rect(x, y + t, t, vert_h, g_fg);
    }
    // g (middle)
    if (m & 0b0000001) {
        fill_rect(x + t, mid_y, w - 2 * t, t, g_fg);
    }
}

static void clock_big_layout_get(UWORD base_x, UWORD base_y, UWORD *out_start_x, UWORD *out_start_y)
{
    // Layout: D gap D gap : gap D gap D  => 4 digits + 4 gaps + colon
    UWORD total_w = EPD_CLOCK_DIGIT_W * 4 + EPD_CLOCK_DIGIT_GAP * 4 + EPD_CLOCK_COLON_W;
    UWORD start_x = base_x + (EPD_CLOCK_TIME_W > total_w ? (EPD_CLOCK_TIME_W - total_w) / 2 : 0);
    UWORD start_y = base_y + EPD_CLOCK_TIME_INNER_TOP;

    if (out_start_x) {
        *out_start_x = start_x;
    }
    if (out_start_y) {
        *out_start_y = start_y;
    }
}

static void draw_time_big(UWORD x, UWORD y, int hour, int min, BOOL_T blank_hour_tens)
{
    UWORD start_x = 0;
    UWORD start_y = 0;
    UWORD colon_w = EPD_CLOCK_COLON_W;
    UWORD colon_dot = EPD_CLOCK_COLON_DOT;
    UWORD gap = EPD_CLOCK_DIGIT_GAP;

    clock_big_layout_get(x, y, &start_x, &start_y);

    int h1 = (hour / 10) % 10;
    int h2 = hour % 10;
    int m1 = (min / 10) % 10;
    int m2 = min % 10;

    UWORD cur_x = start_x;
    if (!blank_hour_tens || h1 != 0) {
        draw_7seg_digit(cur_x, start_y, h1, EPD_CLOCK_DIGIT_W, EPD_CLOCK_DIGIT_H, EPD_CLOCK_DIGIT_THICK);
    }
    cur_x += EPD_CLOCK_DIGIT_W + gap;
    draw_7seg_digit(cur_x, start_y, h2, EPD_CLOCK_DIGIT_W, EPD_CLOCK_DIGIT_H, EPD_CLOCK_DIGIT_THICK);
    cur_x += EPD_CLOCK_DIGIT_W + gap;

    // Colon
    UWORD colon_x = cur_x + (colon_w - colon_dot) / 2;
    UWORD colon_y1 = start_y + EPD_CLOCK_DIGIT_H / 3;
    UWORD colon_y2 = start_y + (EPD_CLOCK_DIGIT_H * 2) / 3;
    fill_rect(colon_x, colon_y1, colon_dot, colon_dot, g_fg);
    fill_rect(colon_x, colon_y2, colon_dot, colon_dot, g_fg);
    cur_x += colon_w + gap;

    draw_7seg_digit(cur_x, start_y, m1, EPD_CLOCK_DIGIT_W, EPD_CLOCK_DIGIT_H, EPD_CLOCK_DIGIT_THICK);
    cur_x += EPD_CLOCK_DIGIT_W + gap;
    draw_7seg_digit(cur_x, start_y, m2, EPD_CLOCK_DIGIT_W, EPD_CLOCK_DIGIT_H, EPD_CLOCK_DIGIT_THICK);
}

BOOL_T clock_ui_validate_layout(void)
{
    // Screen bounds
    if (EPD_CLOCK_DATE_X + EPD_CLOCK_DATE_W > EPD_4in26_WIDTH) {
        return FALSE;
    }
    if (EPD_CLOCK_TIME_X + EPD_CLOCK_TIME_W > EPD_4in26_WIDTH) {
        return FALSE;
    }
    if (EPD_CLOCK_STAT_X + EPD_CLOCK_STAT_W > EPD_4in26_WIDTH) {
        return FALSE;
    }
    if (EPD_CLOCK_DATE_Y + EPD_CLOCK_DATE_H > EPD_4in26_HEIGHT) {
        return FALSE;
    }
    if (EPD_CLOCK_TIME_Y + EPD_CLOCK_TIME_H > EPD_4in26_HEIGHT) {
        return FALSE;
    }
    if (EPD_CLOCK_STAT_Y + EPD_CLOCK_STAT_H > EPD_4in26_HEIGHT) {
        return FALSE;
    }

    // Partial refresh (1bpp) requires 8-pixel alignment in X/Width.
    if ((EPD_CLOCK_DATE_X % 8) != 0 || (EPD_CLOCK_DATE_W % 8) != 0) {
        return FALSE;
    }
    if ((EPD_CLOCK_TIME_X % 8) != 0 || (EPD_CLOCK_TIME_W % 8) != 0) {
        return FALSE;
    }
    if ((EPD_CLOCK_STAT_X % 8) != 0 || (EPD_CLOCK_STAT_W % 8) != 0) {
        return FALSE;
    }

    // No overlap between time and status columns.
    if (EPD_CLOCK_TIME_X + EPD_CLOCK_TIME_W > EPD_CLOCK_STAT_X) {
        return FALSE;
    }

    // Big time must fit within time region width.
    UWORD total_w = EPD_CLOCK_DIGIT_W * 4 + EPD_CLOCK_DIGIT_GAP * 4 + EPD_CLOCK_COLON_W;
    if (total_w > EPD_CLOCK_TIME_W) {
        return FALSE;
    }

    return TRUE;
}

static void clock_ui_draw_header(const clock_ui_state_t *state)
{
    char line[96];

    if (state && state->time_synced) {
        snprintf(line, sizeof(line), "%04d-%02d-%02d %s",
                 state->local.tm_year + 1900, state->local.tm_mon + 1, state->local.tm_mday, weekday_str(state->local.tm_wday));
        draw_string_centered_clipped(EPD_CLOCK_DATE_X, EPD_CLOCK_DATE_Y + 10, EPD_CLOCK_DATE_W, line, &Font20);
        return;
    }

    if (state && state->net.link != NETMGR_LINK_DOWN) {
        snprintf(line, sizeof(line), "WiFi OK - waiting time sync (default 08:00)");
    } else {
        snprintf(line, sizeof(line), "Configuring network... (default 08:00)");
    }
    draw_string_centered_clipped(EPD_CLOCK_DATE_X, EPD_CLOCK_DATE_Y + 14, EPD_CLOCK_DATE_W, line, &Font16);
}

static void clock_ui_draw_status(const clock_ui_state_t *state)
{
    char line[96];
    const UWORD left = EPD_CLOCK_STAT_X + 10;
    const UWORD max_w = EPD_CLOCK_STAT_W - 20;

    Paint_DrawRectangle(EPD_CLOCK_STAT_X, EPD_CLOCK_STAT_Y, EPD_CLOCK_STAT_X + EPD_CLOCK_STAT_W - 1,
                        EPD_CLOCK_STAT_Y + EPD_CLOCK_STAT_H - 1, g_fg, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

    snprintf(line, sizeof(line), "WiFi: %s", state ? NETMGR_STATUS_TO_STR(state->net.link) : "unknown");
    draw_string_clipped(left, EPD_CLOCK_STAT_Y + 12, max_w, line, &Font16);

    snprintf(line, sizeof(line), "SSID: %s", (state && state->net.ssid[0]) ? state->net.ssid : "-");
    draw_string_clipped(left, EPD_CLOCK_STAT_Y + 36, max_w, line, &Font12);

    snprintf(line, sizeof(line), "IP: %s", (state && state->net.ip[0]) ? state->net.ip : "-");
    draw_string_clipped(left, EPD_CLOCK_STAT_Y + 54, max_w, line, &Font12);

    snprintf(line, sizeof(line), "Cloud: %s", (state && state->cloud_connected) ? "ON" : "OFF");
    draw_string_clipped(left, EPD_CLOCK_STAT_Y + 78, max_w, line, &Font12);

    const char *time_state = "DEFAULT";
    if (state && state->time_synced) {
        if (state->time_src == CLOCK_TIME_SRC_CLOUD) {
            time_state = "CLOUD";
        } else if (state->time_src == CLOCK_TIME_SRC_NTP) {
            time_state = "NTP";
        } else {
            time_state = "OK";
        }
    }
    snprintf(line, sizeof(line), "Time: %s", time_state);
    draw_string_clipped(left, EPD_CLOCK_STAT_Y + 96, max_w, line, &Font12);
}

void clock_ui_render(uint8_t *framebuffer, const clock_ui_state_t *state)
{
    if (!framebuffer) {
        return;
    }

    __ui_theme_apply(state);

    Paint_NewImage((UBYTE *)framebuffer, EPD_4in26_WIDTH, EPD_4in26_HEIGHT, ROTATE_0, g_bg);
    Paint_Clear(g_bg);

    // Frame
    Paint_DrawRectangle(0, 0, EPD_4in26_WIDTH - 1, EPD_4in26_HEIGHT - 1, g_fg, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);
    Paint_DrawLine(0, EPD_CLOCK_TIME_Y - 10, EPD_4in26_WIDTH - 1, EPD_CLOCK_TIME_Y - 10, g_fg, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(EPD_CLOCK_STAT_X, EPD_CLOCK_TIME_Y - 10, EPD_CLOCK_STAT_X, EPD_4in26_HEIGHT - 2, g_fg, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    clock_ui_draw_header(state);

    // Time
    int hour = state ? state->local.tm_hour : 8;
    int min = state ? state->local.tm_min : 0;

    BOOL_T blank_hour_tens = FALSE;
    if (state && state->time_mode == CLOCK_TIME_MODE_12H) {
        int h = hour % 12;
        if (h == 0) {
            h = 12;
        }
        hour = h;
        blank_hour_tens = TRUE;
    }
    draw_time_big(EPD_CLOCK_TIME_X, EPD_CLOCK_TIME_Y, hour, min, blank_hour_tens);

    // Status
    clock_ui_draw_status(state);

    Paint_DrawString_EN(20, EPD_4in26_HEIGHT - 20, "Tuya e-Paper Clock", &Font12, g_fg, g_bg);
}

