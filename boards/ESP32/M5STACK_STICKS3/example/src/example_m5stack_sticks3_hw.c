/**
 * @file example_m5stack_sticks3_hw.c
 * @brief M5Stack StickS3 board hardware bring-up example.
 * @version 0.4
 * @date 2026-07-27
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * Home: 3-D rotating cube.
 * BTN1 (K1) click: cycle through screens Home→IMU→Mic→IR→SysInfo.
 * BTN2 (K2) click: on IR screen, run loopback test.
 */
#include "tuya_cloud_types.h"

#include "board_com_api.h"
#include "board_bmi270_api.h"
#include "board_config.h"
#include "tal_api.h"
#include "tdl_audio_manage.h"
#include "tdl_button_manage.h"
#include "tdl_display_draw.h"
#include "tdl_display_manage.h"
#include "tkl_gpio.h"
#include "tkl_output.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define EXAMPLE_BOOT_DELAY_MS      (5000U)
#define EXAMPLE_SAMPLE_RATE        (16000U)
#define EXAMPLE_BEEP_DURATION_MS   (150U)
#define EXAMPLE_CHUNK_SAMPLES      (512U)
#define EXAMPLE_BEEP_FREQ          (1000U)

/* RGB565 colours.  DISPLAY_COLOR_INVERT is true on StickS3 so the panel
 * applies INVON; the names below describe what the user actually sees. */
#define COL_BLACK   0x0000
#define COL_WHITE   0xFFFF
#define COL_A_QUA   0xF800  /* appears blue  */
#define COL_A_GRN   0x07E0  /* appears green */
#define COL_A_RED   0x001F  /* appears red   */
#define COL_A_YEL   0x07FF  /* appears cyan  */
#define COL_A_CYN   0xFFE0  /* appears yellow*/
#define COL_A_MAG   0xF81F  /* appears magenta*/
#define COL_GREY    0x4208  /* dim grey       */

/* Bar geometry for the IMU screen (135×240 portrait). */
#define BAR_H            22
#define BAR_GAP          14
#define BAR_MAX_LEN      ((DISPLAY_WIDTH / 2) - 3)
#define BAR_CENTER_X     (DISPLAY_WIDTH / 2)

/* Refresh interval for live screens (ms). */
#define IMU_REFRESH_MS      100
#define SYSINFO_REFRESH_MS  1000
#define IR_REFRESH_MS       200
#define MIC_REFRESH_MS      100

/* Waveform buffer size for mic screen. */
#define MIC_WAVE_LEN        68

/* ---------------------------------------------------------------------------
 * Types
 * --------------------------------------------------------------------------- */
typedef enum {
    SCREEN_HOME = 0,
    SCREEN_IMU,
    SCREEN_SYSINFO,
    SCREEN_IR,
    SCREEN_MIC,
} screen_state_t;

/* ---------------------------------------------------------------------------
 * 5×7 bitmap font (standard ASCII subset)
 * Each char = 5 columns, 7 rows (bit 6 = row 0 / top).
 * Index: 0=space, 1-10=digits, 11-36=A-Z, 37='-', 38='.', 39=':', 40='+'
 * --------------------------------------------------------------------------- */
static const uint8_t s_font5x7[41][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* space */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */ {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */ {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */ {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */ {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */ {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */ {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */ {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */ {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x51,0x51,0x72}, /* G */ {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */ {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */ {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */ {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */ {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */ {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */ {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */ {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* W */ {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */ {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x08,0x08,0x08,0x08,0x08}, /* - */ {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x00,0x36,0x36,0x00,0x00}, /* : */ {0x08,0x08,0x3E,0x08,0x08}, /* + */
};

static const uint8_t *__font_lookup(char c)
{
    if (c >= '0' && c <= '9') return s_font5x7[c - '0' + 1];
    if (c >= 'A' && c <= 'Z') return s_font5x7[c - 'A' + 11];
    switch (c) {
    case ' ': return s_font5x7[0];
    case '-': return s_font5x7[37];
    case '.': return s_font5x7[38];
    case ':': return s_font5x7[39];
    case '+': return s_font5x7[40];
    default:   return s_font5x7[0];
    }
}

/* Draw a single character at (x,y) with given scale (1=5×7, 2=10×14, …) */
static void __draw_char(TDL_DISP_FRAME_BUFF_T *fb, uint16_t x, uint16_t y,
                        char c, uint16_t color, uint8_t scale)
{
    const uint8_t *g = __font_lookup(c);
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (uint8_t row = 0; row < 7; row++) {
            if (bits & (1 << (6 - row))) {
                TDL_DISP_RECT_T r = {
                    .x0 = x + col * scale,
                    .y0 = y + row * scale,
                    .x1 = x + col * scale + scale - 1,
                    .y1 = y + row * scale + scale - 1,
                };
                tdl_disp_draw_fill(fb, &r, color, false);
            }
        }
    }
}

/* Draw a text string at (x,y) with given scale. */
static void __draw_text(TDL_DISP_FRAME_BUFF_T *fb, uint16_t x, uint16_t y,
                        const char *str, uint16_t color, uint8_t scale)
{
    uint16_t cx = x;
    for (const char *p = str; *p; p++) {
        __draw_char(fb, cx, y, *p, color, scale);
        cx += (5 + 1) * scale;
    }
}

/* Bresenham line drawing on the frame buffer. */
static void __draw_line(TDL_DISP_FRAME_BUFF_T *fb, int x0, int y0,
                        int x1, int y1, uint16_t color)
{
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        if (x0 >= 0 && x0 < DISPLAY_WIDTH && y0 >= 0 && y0 < DISPLAY_HEIGHT)
            tdl_disp_draw_point(fb, (uint16_t)x0, (uint16_t)y0, color, false);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

/* Format a float as sign+int.dec without using %f (embedded-safe). */
static void __fmt_float(char *buf, int bufsz, const char *prefix,
                        float val, int dec)
{
    int scale = 1;
    for (int i = 0; i < dec; i++) scale *= 10;
    int v = (int)(val * (float)scale);
    int neg = v < 0;
    if (neg) v = -v;
    int whole = v / scale;
    int frac  = v % scale;
    snprintf(buf, bufsz, "%s%c%d.%0*d", prefix, neg ? '-' : '+', whole, dec, frac);
}

/* ---------------------------------------------------------------------------
 * 3-D cube wireframe
 * --------------------------------------------------------------------------- */
typedef struct { float x, y, z; } vec3_t;

static const vec3_t s_cube_v[8] = {
    {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
    {-1,-1, 1},{1,-1, 1},{1,1, 1},{-1,1, 1},
};
static const int s_cube_e[12][2] = {
    {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},
    {6,7},{7,4},{0,4},{1,5},{2,6},{3,7},
};

static void __draw_cube(TDL_DISP_FRAME_BUFF_T *fb, int cx, int cy,
                        float scale, float ax, float ay, float az,
                        uint16_t color)
{
    vec3_t p[8];
    float cxa = cosf(ax), sxa = sinf(ax);
    float cya = cosf(ay), sya = sinf(ay);
    float cza = cosf(az), sza = sinf(az);

    for (int i = 0; i < 8; i++) {
        vec3_t v = s_cube_v[i];
        /* X rotation */
        float y = v.y * cxa - v.z * sxa;
        float z = v.y * sxa + v.z * cxa;
        v.y = y; v.z = z;
        /* Y rotation */
        float x = v.x * cya + v.z * sya;
        z = -v.x * sya + v.z * cya;
        v.x = x; v.z = z;
        /* Z rotation */
        x = v.x * cza - v.y * sza;
        y = v.x * sza + v.y * cza;
        v.x = x; v.y = y;
        p[i] = v;
    }

    /* Perspective project */
    int proj[8][2];
    float f = 4.0f;
    for (int i = 0; i < 8; i++) {
        float zz = f + p[i].z;
        if (zz < 0.1f) zz = 0.1f;
        proj[i][0] = cx + (int)(p[i].x * scale * f / zz);
        proj[i][1] = cy + (int)(p[i].y * scale * f / zz);
    }

    for (int i = 0; i < 12; i++) {
        __draw_line(fb, proj[s_cube_e[i][0]][0], proj[s_cube_e[i][0]][1],
                    proj[s_cube_e[i][1]][0], proj[s_cube_e[i][1]][1], color);
    }
}

/* ---------------------------------------------------------------------------
 * File-scope variables
 * --------------------------------------------------------------------------- */
static THREAD_HANDLE s_app_thread  = NULL;
static const int16_t  s_sine_1khz[16] = {
    0, 10716, 19800, 25868, 28000, 25868, 19800, 10716,
    0, -10716, -19800, -25868, -28000, -25868, -19800, -10716,
};

static volatile screen_state_t s_screen = SCREEN_HOME;
static TDL_DISP_HANDLE_T       s_disp_hdl = NULL;

/* Mic data (written by audio callback, read by render thread). */
static volatile uint16_t s_mic_rms = 0;
static volatile bool     s_mic_active = false;
static volatile uint32_t s_mic_frames = 0;
static int16_t           s_mic_wave[MIC_WAVE_LEN];

/* IR loopback test results (set by K2, displayed by render). */
static volatile bool s_ir_tested   = false;
static volatile bool s_ir_det_hi    = false;
static volatile bool s_ir_det_lo    = false;
static volatile bool s_ir_pass      = false;

/* Screen count for K1 cycling. */
#define SCREEN_COUNT  (SCREEN_SYSINFO + 1)

/* ---------------------------------------------------------------------------
 * Display helpers
 * --------------------------------------------------------------------------- */
static TDL_DISP_FRAME_BUFF_T *__alloc_fb(void)
{
    TDL_DISP_FRAME_BUFF_T *fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM,
                                                           DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
    if (fb == NULL) {
        PR_ERR("fb alloc failed");
        return NULL;
    }
    fb->x_start = 0;
    fb->y_start = 0;
    fb->width   = DISPLAY_WIDTH;
    fb->height  = DISPLAY_HEIGHT;
    fb->fmt     = TUYA_PIXEL_FMT_RGB565;
    return fb;
}

static void __flush_and_free(TDL_DISP_FRAME_BUFF_T *fb)
{
    if (fb == NULL || s_disp_hdl == NULL) return;
    tdl_disp_dev_flush(s_disp_hdl, fb);
    tdl_disp_free_frame_buff(fb);
}

static void __draw_signed_bar(TDL_DISP_FRAME_BUFF_T *fb, uint16_t y, uint16_t h,
                               float value, float max_val, uint16_t color)
{
    TDL_DISP_RECT_T ctr = {
        .x0 = BAR_CENTER_X, .y0 = y,
        .x1 = BAR_CENTER_X, .y1 = y + h - 1,
    };
    tdl_disp_draw_fill(fb, &ctr, COL_GREY, false);

    if (max_val <= 0.0f || value == 0.0f) return;

    float ratio = value / max_val;
    if (ratio > 1.0f)  ratio = 1.0f;
    if (ratio < -1.0f) ratio = -1.0f;

    int16_t len = (int16_t)(ratio * (float)BAR_MAX_LEN);
    TDL_DISP_RECT_T rect = {0};

    if (len > 0) {
        rect.x0 = BAR_CENTER_X;       rect.y0 = y;
        rect.x1 = BAR_CENTER_X + len; rect.y1 = y + h - 1;
    } else if (len < 0) {
        rect.x0 = BAR_CENTER_X + len; rect.y0 = y;
        rect.x1 = BAR_CENTER_X;       rect.y1 = y + h - 1;
    }
    if (rect.x0 != rect.x1) {
        tdl_disp_draw_fill(fb, &rect, color, false);
    }
}

/* ---------------------------------------------------------------------------
 * Screen renderers
 * --------------------------------------------------------------------------- */
/**
 * @brief Home screen — full-screen 3-D rotating cube on black background.
 */
static void __render_home(void)
{
    TDL_DISP_FRAME_BUFF_T *fb = __alloc_fb();
    if (fb == NULL) return;

    tdl_disp_draw_fill_full(fb, COL_BLACK, false);

    uint32_t t = tal_system_get_millisecond();
    float ax = (float)t * 0.001f;
    float ay = (float)t * 0.0015f;
    float az = (float)t * 0.0007f;
    __draw_cube(fb, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, 40.0f,
                ax, ay, az, COL_A_GRN);

    __flush_and_free(fb);
}

/**
 * @brief IMU screen — 3-D rotating cube on top, text XYZ values below.
 */
static void __render_imu(void)
{
    TDL_DISP_FRAME_BUFF_T *fb = __alloc_fb();
    if (fb == NULL) return;

    tdl_disp_draw_fill_full(fb, COL_BLACK, false);

    bmi270_dev_t *imu = board_bmi270_get_handle();
    bmi270_sensor_data_t d = {0};
    bool ok = (imu && imu->initialized && board_bmi270_read_data(imu, &d) == OPRT_OK);

    if (ok) {
        /* 3-D cube: time-based rotation + gyro influence */
        uint32_t t = tal_system_get_millisecond();
        float ax = (float)t * 0.001f + d.gyr_x * 0.01f;
        float ay = (float)t * 0.0015f + d.gyr_y * 0.01f;
        float az = (float)t * 0.0007f + d.gyr_z * 0.01f;
        __draw_cube(fb, DISPLAY_WIDTH / 2, 50, 22.0f, ax, ay, az, COL_A_GRN);

        /* Text values below cube */
        char buf[20];
        __fmt_float(buf, sizeof(buf), "AX:", d.acc_x, 2);
        __draw_text(fb, 5, 105, buf, COL_A_RED, 2);
        __fmt_float(buf, sizeof(buf), "AY:", d.acc_y, 2);
        __draw_text(fb, 5, 125, buf, COL_A_GRN, 2);
        __fmt_float(buf, sizeof(buf), "AZ:", d.acc_z, 2);
        __draw_text(fb, 5, 145, buf, COL_A_QUA, 2);

        __fmt_float(buf, sizeof(buf), "GX:", d.gyr_x, 1);
        __draw_text(fb, 5, 170, buf, COL_A_YEL, 2);
        __fmt_float(buf, sizeof(buf), "GY:", d.gyr_y, 1);
        __draw_text(fb, 5, 190, buf, COL_A_CYN, 2);
        __fmt_float(buf, sizeof(buf), "GZ:", d.gyr_z, 1);
        __draw_text(fb, 5, 210, buf, COL_A_MAG, 2);
    } else {
        __draw_text(fb, 10, 100, "IMU FAIL", COL_A_RED, 3);
    }

    __flush_and_free(fb);
}

/**
 * @brief System info screen — text + bars.
 */
static void __render_sysinfo(void)
{
    TDL_DISP_FRAME_BUFF_T *fb = __alloc_fb();
    if (fb == NULL) return;

    tdl_disp_draw_fill_full(fb, COL_BLACK, false);

    __draw_text(fb, 15, 5, "SYS INFO", COL_A_GRN, 2);

    /* Heap bar */
    uint32_t free_h = (uint32_t)tal_system_get_free_heap_size();
    char buf[20];
    snprintf(buf, sizeof(buf), "HEAP:%d", (int)free_h);
    __draw_text(fb, 5, 30, buf, COL_A_GRN, 2);
    __draw_signed_bar(fb, 55, BAR_H, (float)free_h / (400.0f * 1024.0f), 1.0f, COL_A_GRN);

    /* Uptime bar */
    uint32_t upt_s = (uint32_t)(tal_system_get_millisecond() / 1000);
    snprintf(buf, sizeof(buf), "UP:%dS", (int)upt_s);
    __draw_text(fb, 5, 100, buf, COL_A_QUA, 2);
    __draw_signed_bar(fb, 125, BAR_H, (float)(upt_s % 60) / 60.0f, 1.0f, COL_A_QUA);

    /* BMI270 status */
    bmi270_dev_t *imu = board_bmi270_get_handle();
    if (imu && imu->initialized) {
        __draw_text(fb, 5, 170, "IMU:OK", COL_A_GRN, 2);
    } else {
        __draw_text(fb, 5, 170, "IMU:FAIL", COL_A_RED, 2);
    }

    __flush_and_free(fb);
}

/**
 * @brief IR loopback screen — shows prompt or last test result.
 *
 * K2 (BTN2) triggers the actual TX/RX test.  The screen displays
 * "PRESS K2" until tested, then shows the results.
 */
static void __render_ir(void)
{
    TDL_DISP_FRAME_BUFF_T *fb = __alloc_fb();
    if (fb == NULL) return;

    tdl_disp_draw_fill_full(fb, COL_BLACK, false);

    __draw_text(fb, 20, 5, "IR TEST", COL_A_GRN, 2);

    if (!s_ir_tested) {
        __draw_text(fb, 10, 100, "PRESS K2", COL_A_YEL, 3);
        __flush_and_free(fb);
        return;
    }

    /* Show last test results */
    __draw_signed_bar(fb, 40,  BAR_H, 0.7f, 1.0f, COL_A_GRN);
    __draw_signed_bar(fb, 80,  BAR_H, s_ir_det_hi ? 0.7f : -0.7f, 1.0f,
                       s_ir_det_hi ? COL_A_GRN : COL_A_RED);
    __draw_signed_bar(fb, 120, BAR_H, s_ir_det_lo ? 0.7f : -0.7f, 1.0f,
                       s_ir_det_lo ? COL_A_GRN : COL_A_RED);
    __draw_text(fb, 20, 160, s_ir_pass ? "PASS" : "FAIL",
               s_ir_pass ? COL_A_GRN : COL_A_RED, 3);

    __flush_and_free(fb);
}

/**
 * @brief Run the IR loopback test (called from K2 button callback).
 */
static void __ir_run_test(void)
{
    /* Phase 1: TX HIGH → expect RX LOW (IR detected) */
    tkl_gpio_write((TUYA_GPIO_NUM_E)IR_TX_IO, TUYA_GPIO_LEVEL_HIGH);
    tal_system_sleep(5);
    TUYA_GPIO_LEVEL_E rx_hi;
    tkl_gpio_read((TUYA_GPIO_NUM_E)IR_RX_IO, &rx_hi);
    s_ir_det_hi = (rx_hi == TUYA_GPIO_LEVEL_LOW);

    /* Phase 2: TX LOW → expect RX HIGH (idle) */
    tkl_gpio_write((TUYA_GPIO_NUM_E)IR_TX_IO, TUYA_GPIO_LEVEL_LOW);
    tal_system_sleep(5);
    TUYA_GPIO_LEVEL_E rx_lo;
    tkl_gpio_read((TUYA_GPIO_NUM_E)IR_RX_IO, &rx_lo);
    s_ir_det_lo = (rx_lo == TUYA_GPIO_LEVEL_HIGH);

    s_ir_pass    = s_ir_det_hi && s_ir_det_lo;
    s_ir_tested = true;

    PR_NOTICE("IR: TX_H->RX=%s(det=%d) TX_L->RX=%s(idle=%d) %s",
              rx_hi == TUYA_GPIO_LEVEL_HIGH ? "H" : "L", s_ir_det_hi,
              rx_lo == TUYA_GPIO_LEVEL_HIGH ? "H" : "L", s_ir_det_lo,
              s_ir_pass ? "PASS" : "FAIL");
}

/**
 * @brief Mic RMS screen — level bar + live waveform.
 */
static void __render_mic(void)
{
    TDL_DISP_FRAME_BUFF_T *fb = __alloc_fb();
    if (fb == NULL) return;

    tdl_disp_draw_fill_full(fb, COL_BLACK, false);

    __draw_text(fb, 30, 5, "MIC", COL_A_GRN, 2);

    if (!s_mic_active) {
        __draw_text(fb, 10, 100, "NO MIC", COL_A_RED, 2);
        __flush_and_free(fb);
        return;
    }

    /* RMS level bar (vertical, centred, 26px wide, 80px tall) */
    uint16_t rms = s_mic_rms;
    /* Log-ish scaling: 0-2000 → 0-80px */
    float norm = (float)rms / 2000.0f;
    if (norm > 1.0f) norm = 1.0f;
    uint16_t bar_h = (uint16_t)(norm * 80.0f);

    /* Bar background */
    TDL_DISP_RECT_T bg = { .x0 = 54, .y0 = 30, .x1 = 80, .y1 = 110 };
    tdl_disp_draw_fill(fb, &bg, COL_GREY, false);

    /* Bar fill (from bottom up) */
    if (bar_h > 0) {
        TDL_DISP_RECT_T bar = { .x0 = 56, .y0 = 110 - bar_h, .x1 = 78, .y1 = 110 };
        tdl_disp_draw_fill(fb, &bar, COL_A_GRN, false);
    }

    /* RMS numeric */
    char buf[16];
    snprintf(buf, sizeof(buf), "RMS:%d", (int)rms);
    __draw_text(fb, 25, 120, buf, COL_A_YEL, 2);

    /* Waveform (y=150-235, 68 samples across 135px) */
    uint16_t wave_cy = 192;
    uint16_t wave_h  = 40;

    /* Center reference line */
    __draw_line(fb, 0, wave_cy, DISPLAY_WIDTH - 1, wave_cy, COL_GREY);

    for (int i = 0; i < MIC_WAVE_LEN; i++) {
        int x = i * 2;
        if (x >= DISPLAY_WIDTH) x = DISPLAY_WIDTH - 1;
        int16_t s = s_mic_wave[i];
        int h = (int)((float)s / 32768.0f * (float)wave_h);
        if (h >= 0) {
            __draw_line(fb, x, wave_cy, x, wave_cy + h, COL_A_CYN);
        } else {
            __draw_line(fb, x, wave_cy + h, x, wave_cy, COL_A_CYN);
        }
    }

    __flush_and_free(fb);
}

/* ---------------------------------------------------------------------------
 * Button
 * --------------------------------------------------------------------------- */
static const char *__button_event_name(TDL_BUTTON_TOUCH_EVENT_E event)
{
    switch (event) {
    case TDL_BUTTON_PRESS_DOWN:         return "PRESS_DOWN";
    case TDL_BUTTON_PRESS_UP:           return "PRESS_UP";
    case TDL_BUTTON_PRESS_SINGLE_CLICK: return "SINGLE_CLICK";
    case TDL_BUTTON_PRESS_DOUBLE_CLICK: return "DOUBLE_CLICK";
    case TDL_BUTTON_PRESS_REPEAT:       return "REPEAT";
    case TDL_BUTTON_LONG_PRESS_START:   return "LONG_PRESS_START";
    case TDL_BUTTON_LONG_PRESS_HOLD:    return "LONG_PRESS_HOLD";
    case TDL_BUTTON_RECOVER_PRESS_UP:   return "RECOVER_PRESS_UP";
    default:                             return "UNKNOWN";
    }
}

static void __button_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    PR_NOTICE("key: %s event=%s", name, __button_event_name(event));

    if (event != TDL_BUTTON_PRESS_SINGLE_CLICK) {
        return;
    }

#if defined(BUTTON_NAME)
    /* K1: cycle through screens */
    if (strcmp(name, BUTTON_NAME) == 0) {
        s_screen = (screen_state_t)((s_screen + 1) % SCREEN_COUNT);
        PR_NOTICE("-> screen %d", (int)s_screen);
        return;
    }
#endif

#if defined(BUTTON_NAME_2)
    /* K2: on IR screen, run loopback test */
    if (strcmp(name, BUTTON_NAME_2) == 0) {
        if (s_screen == SCREEN_IR) {
            __ir_run_test();
            PR_NOTICE("IR test triggered by K2");
        }
        return;
    }
#endif

    (void)argc;
}

static void __register_button_events(TDL_BUTTON_HANDLE handle)
{
    tdl_button_event_register(handle, TDL_BUTTON_PRESS_DOWN,          __button_cb);
    tdl_button_event_register(handle, TDL_BUTTON_PRESS_UP,            __button_cb);
    tdl_button_event_register(handle, TDL_BUTTON_PRESS_SINGLE_CLICK, __button_cb);
    tdl_button_event_register(handle, TDL_BUTTON_PRESS_DOUBLE_CLICK, __button_cb);
    tdl_button_event_register(handle, TDL_BUTTON_PRESS_REPEAT,       __button_cb);
    tdl_button_event_register(handle, TDL_BUTTON_LONG_PRESS_START,   __button_cb);
    tdl_button_event_register(handle, TDL_BUTTON_LONG_PRESS_HOLD,    __button_cb);
    tdl_button_event_register(handle, TDL_BUTTON_RECOVER_PRESS_UP,    __button_cb);
}

static OPERATE_RET __open_buttons(void)
{
    OPERATE_RET       rt = OPRT_OK;
    TDL_BUTTON_HANDLE hdl = NULL;
    TDL_BUTTON_CFG_T  cfg = {
        .long_start_valid_time     = 800,
        .long_keep_timer           = 1000,
        .button_debounce_time      = 50,
        .button_repeat_valid_count = 3,
        .button_repeat_valid_time  = 500,
    };

    tdl_button_set_task_stack_size(4096);

#if defined(BUTTON_NAME)
    TUYA_CALL_ERR_RETURN(tdl_button_create(BUTTON_NAME, &cfg, &hdl));
    __register_button_events(hdl);
    TUYA_CALL_ERR_LOG(tdl_button_set_ready_flag(BUTTON_NAME, TRUE));
    PR_NOTICE("BTN1 ready: %s", BUTTON_NAME);
#endif

#if defined(BUTTON_NAME_2)
    hdl = NULL;
    TUYA_CALL_ERR_RETURN(tdl_button_create(BUTTON_NAME_2, &cfg, &hdl));
    __register_button_events(hdl);
    TUYA_CALL_ERR_LOG(tdl_button_set_ready_flag(BUTTON_NAME_2, TRUE));
    PR_NOTICE("BTN2 ready: %s", BUTTON_NAME_2);
#endif

    return rt;
}

/* ---------------------------------------------------------------------------
 * Audio
 * --------------------------------------------------------------------------- */
static void __mic_cb(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status,
                     uint8_t *data, uint32_t len)
{
    (void)status;
    if (type != TDL_AUDIO_FRAME_FORMAT_PCM || data == NULL || len < 2) return;

    int16_t *s = (int16_t *)data;
    uint32_t n = len / sizeof(int16_t);

    /* Compute RMS */
    int64_t sum_sq = 0;
    for (uint32_t i = 0; i < n; i++) {
        int32_t v = s[i];
        sum_sq += (int64_t)v * v;
    }
    if (n > 0) {
        float rms = sqrtf((float)(sum_sq / (int64_t)n));
        if (rms > 65535.0f) rms = 65535.0f;
        s_mic_rms = (uint16_t)rms;
        s_mic_active = true;
        s_mic_frames++;
    }

    /* Downsample to waveform buffer */
    uint32_t step = n / MIC_WAVE_LEN;
    if (step == 0) step = 1;
    for (uint32_t i = 0; i < MIC_WAVE_LEN && i * step < n; i++) {
        s_mic_wave[i] = s[i * step];
    }
}

/**
 * @brief Play a single short beep and keep mic streaming open.
 */
static OPERATE_RET __play_beep(void)
{
    OPERATE_RET        rt = OPRT_OK;
    TDL_AUDIO_HANDLE_T audio_hdl = NULL;
    int16_t           *pcm = NULL;
    uint32_t beep_s = EXAMPLE_SAMPLE_RATE * EXAMPLE_BEEP_DURATION_MS / 1000U;

    rt = tdl_audio_find(AUDIO_CODEC_NAME, &audio_hdl);
    if (rt != OPRT_OK) {
        PR_NOTICE("audio skipped: %s not registered", AUDIO_CODEC_NAME);
        return OPRT_OK;
    }

    rt = tdl_audio_open(audio_hdl, __mic_cb);
    if (rt != OPRT_OK) {
        PR_NOTICE("audio skipped: open failed rt:%d", rt);
        return OPRT_OK;
    }
    TUYA_CALL_ERR_LOG(tdl_audio_volume_set(audio_hdl, 50));

    pcm = (int16_t *)tal_malloc(EXAMPLE_CHUNK_SAMPLES * sizeof(int16_t));
    if (pcm == NULL) return OPRT_MALLOC_FAILED;

    /* Single beep: steady tone at EXAMPLE_BEEP_FREQ */
    PR_NOTICE("beep %d Hz %d ms", EXAMPLE_BEEP_FREQ, EXAMPLE_BEEP_DURATION_MS);
    uint32_t phase = 0;
    uint32_t inc   = (uint32_t)((uint64_t)EXAMPLE_BEEP_FREQ * 16 * 65536 /
                                 EXAMPLE_SAMPLE_RATE);
    uint32_t written = 0;
    while (written < beep_s) {
        uint32_t chunk = beep_s - written;
        if (chunk > EXAMPLE_CHUNK_SAMPLES) chunk = EXAMPLE_CHUNK_SAMPLES;
        for (uint32_t i = 0; i < chunk; i++) {
            pcm[i] = s_sine_1khz[(phase >> 16) & 0xF];
            phase += inc;
        }
        rt = tdl_audio_play(audio_hdl, (uint8_t *)pcm,
                            chunk * sizeof(int16_t));
        if (rt != OPRT_OK) {
            PR_ERR("audio play failed rt:%d", rt);
            break;
        }
        written += chunk;
    }

    tal_free(pcm);
    PR_NOTICE("beep done");
    return rt;
}

/* ---------------------------------------------------------------------------
 * Main
 * --------------------------------------------------------------------------- */
static void __print_app_info(void)
{
    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);
}

void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_system_sleep(EXAMPLE_BOOT_DELAY_MS);
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    __print_app_info();

    PR_NOTICE("board_register_hardware begin");
    rt = board_register_hardware();
    if (rt != OPRT_OK) {
        PR_ERR("board_register_hardware failed rt:%d", rt);
    } else {
        PR_NOTICE("board_register_hardware success");
    }

    /* Open display */
    s_disp_hdl = tdl_disp_find_dev(DISPLAY_NAME);
    if (s_disp_hdl == NULL) {
        PR_ERR("display not found: %s", DISPLAY_NAME);
    } else {
        TUYA_CALL_ERR_LOG(tdl_disp_dev_open(s_disp_hdl));
        PR_NOTICE("display opened: %s", DISPLAY_NAME);
    }

    __render_home();

    TUYA_CALL_ERR_LOG(__open_buttons());
    TUYA_CALL_ERR_LOG(__play_beep());

    PR_NOTICE("demo ready — K1:cycle screens  K2:IR test");

    /* Main loop — refresh the active screen */
    screen_state_t prev = SCREEN_HOME;
    for (;;) {
        screen_state_t cur = s_screen;

        /* Home: continuous (cube animation)
         * IMU:  continuous (live data)
         * Mic:  continuous (live waveform)
         * IR:   only redraw on screen change or after K2 test
         * Sys:  continuous (uptime) */
        bool need_redraw = (cur != prev) ||
                           cur == SCREEN_HOME ||
                           cur == SCREEN_IMU  ||
                           cur == SCREEN_MIC  ||
                           cur == SCREEN_SYSINFO;
        if (need_redraw) {
            switch (cur) {
            case SCREEN_HOME:
                __render_home();
                tal_system_sleep(IMU_REFRESH_MS);
                break;
            case SCREEN_IMU:
                __render_imu();
                tal_system_sleep(IMU_REFRESH_MS);
                break;
            case SCREEN_SYSINFO:
                __render_sysinfo();
                tal_system_sleep(SYSINFO_REFRESH_MS);
                break;
            case SCREEN_IR:
                __render_ir();
                tal_system_sleep(IR_REFRESH_MS);
                break;
            case SCREEN_MIC:
                __render_mic();
                tal_system_sleep(MIC_REFRESH_MS);
                break;
            }
            prev = cur;
        } else {
            tal_system_sleep(200);
        }
    }
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    user_main();
}
#else
static void __app_thread(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(s_app_thread);
    s_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {
        .stackDepth = 1024 * 6,
        .priority   = THREAD_PRIO_1,
        .thrdname   = "sticks3_hw",
    };
    tal_thread_create_and_start(&s_app_thread, NULL, NULL, __app_thread, NULL, &thrd_param);
}
#endif
