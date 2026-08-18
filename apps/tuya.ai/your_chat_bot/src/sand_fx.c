/**
 * @file sand_fx.c
 * @brief Gravity sand/water effect using SH3001 accelerometer
 * @version 0.3
 * @date 2026-08-11
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */

#include "sand_fx.h"

#include "tal_api.h"
#include "board_com_api.h"

#if (defined(ENABLE_AI_CHAT_GUI_OLED) && (ENABLE_AI_CHAT_GUI_OLED == 1)) || \
    (defined(BOARD_CHOICE_NICEMCU_T5_0_96ISP) && (BOARD_CHOICE_NICEMCU_T5_0_96ISP == 1))

#include "lvgl.h"
#include "lv_vendor.h"

#include <string.h>
#include <math.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#if defined(BOARD_CHOICE_NICEMCU_T5_0_96ISP) && (BOARD_CHOICE_NICEMCU_T5_0_96ISP == 1)
#define SAND_W            160
#define SAND_H            80
#define SAND_CELL         2
#define SAND_COUNT        180
#define SAND_BG_RGB565    0x18C3
#define SAND_FG_RGB565    0xFE69
#define SAND_FG2_RGB565   0xFD20
/* Accel -> logical screen (+sx right, +sy down). ROTATION_270 flips vs 90°. */
#define SAND_MAP_SX(ax, ay) (-(ay))
#define SAND_MAP_SY(ax, ay) ( (ax))
#else
#define SAND_W            128
#define SAND_H            64
#define SAND_CELL         2
#define SAND_COUNT        110
#define SAND_BG_RGB565    0xFFFF
#define SAND_FG_RGB565    0x0000
#define SAND_FG2_RGB565   0x0000
#define SAND_MAP_SX(ax, ay) (-(ay))
#define SAND_MAP_SY(ax, ay) ( (ax))
#endif

#define SAND_GW           (SAND_W / SAND_CELL)
#define SAND_GH           (SAND_H / SAND_CELL)
#define SAND_TICK_MS      40

/* ---------------------------------------------------------------------------
 * Types
 * --------------------------------------------------------------------------- */
typedef struct {
    int16_t x;
    int16_t y;
    uint8_t alive;
} SAND_GRAIN_T;

/* ---------------------------------------------------------------------------
 * File-scope variables
 * --------------------------------------------------------------------------- */
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_canvas = NULL;
static lv_color_t *s_cbuf = NULL;
static lv_timer_t *s_timer = NULL;
static SAND_GRAIN_T s_grains[SAND_COUNT];
static uint8_t s_occ[SAND_GH][SAND_GW];
static float s_gx = 0.0f;
static float s_gy = 1.0f;
static uint8_t s_active = 0;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Clear occupancy grid
 * @return none
 */
static void __sand_clear_occ(void)
{
    memset(s_occ, 0, sizeof(s_occ));
}

/**
 * @brief Test if grid cell is empty and in bounds
 * @param[in] x cell x
 * @param[in] y cell y
 * @return true if free
 */
static bool __sand_free(int x, int y)
{
    if (x < 0 || y < 0 || x >= SAND_GW || y >= SAND_GH) {
        return false;
    }
    return (0 == s_occ[y][x]);
}

/**
 * @brief Initialize grains piled near bottom-center
 * @return none
 */
static void __sand_reset_grains(void)
{
    uint16_t i = 0;
    int x = SAND_GW / 6;
    int y = SAND_GH - 2;
    int x_max = SAND_GW - SAND_GW / 6;

    __sand_clear_occ();
    memset(s_grains, 0, sizeof(s_grains));

    for (i = 0; i < SAND_COUNT; i++) {
        while (y >= 0) {
            if (__sand_free(x, y)) {
                s_grains[i].x = (int16_t)x;
                s_grains[i].y = (int16_t)y;
                s_grains[i].alive = 1;
                s_occ[y][x] = 1;
                break;
            }
            x++;
            if (x >= x_max) {
                x = SAND_GW / 6;
                y--;
            }
        }
        x++;
        if (x >= x_max) {
            x = SAND_GW / 6;
            y--;
        }
    }
}

/**
 * @brief Update gravity vector from SH3001
 * @return none
 */
static void __sand_update_gravity(void)
{
    sh3001_dev_t *dev = board_sh3001_get_dev();
    float ax = 0.0f, ay = 0.0f, az = 0.0f;
    float sx = 0.0f, sy = 0.0f, mag = 0.0f;

    if (NULL == dev) {
        s_gx = 0.0f;
        s_gy = 1.0f;
        return;
    }

    if (OPRT_OK != sh3001_read_accel(dev, &ax, &ay, &az)) {
        return;
    }

#if defined(BOARD_CHOICE_NICEMCU_T5_0_96ISP) && (BOARD_CHOICE_NICEMCU_T5_0_96ISP == 1)
    /* Chip X zero is ~45° from horizontal; remap so flat → board X≈0 */
    {
        float bx, by, bz;
        board_sh3001_map_accel_to_board(ax, ay, az, &bx, &by, &bz);
        ax = bx;
        ay = by;
        az = bz;
    }
#endif

    sx = SAND_MAP_SX(ax, ay);
    sy = SAND_MAP_SY(ax, ay);
    mag = sqrtf(sx * sx + sy * sy);
    if (mag < 0.15f) {
        return;
    }

    s_gx = sx / mag;
    s_gy = sy / mag;
}

/**
 * @brief Quantize gravity to primary step and optional diagonal
 * @param[out] dx primary x step
 * @param[out] dy primary y step
 * @param[out] sx slide x
 * @param[out] sy slide y
 * @return none
 */
static void __sand_gravity_steps(int8_t *dx, int8_t *dy, int8_t *sx, int8_t *sy)
{
    float ax = fabsf(s_gx);
    float ay = fabsf(s_gy);

    *dx = 0;
    *dy = 0;
    *sx = 0;
    *sy = 0;

    if (ax >= ay) {
        *dx = (s_gx >= 0.0f) ? 1 : -1;
        if (ay > 0.35f * ax) {
            *dy = (s_gy >= 0.0f) ? 1 : -1;
        }
        *sx = *dx;
        *sy = (s_gy >= 0.0f) ? 1 : -1;
        if (0 == *dy) {
            *sy = (*sy == 0) ? 1 : *sy;
        }
    } else {
        *dy = (s_gy >= 0.0f) ? 1 : -1;
        if (ax > 0.35f * ay) {
            *dx = (s_gx >= 0.0f) ? 1 : -1;
        }
        *sy = *dy;
        *sx = (s_gx >= 0.0f) ? 1 : -1;
        if (0 == *dx) {
            *sx = (*sx == 0) ? 1 : *sx;
        }
    }
}

/**
 * @brief One physics step for all grains
 * @return none
 */
static void __sand_step(void)
{
    int8_t dx = 0, dy = 0, sx = 0, sy = 0;
    int i = 0;
    int pass = 0;

    __sand_update_gravity();
    __sand_gravity_steps(&dx, &dy, &sx, &sy);

    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < SAND_COUNT; i++) {
            int x = 0, y = 0;
            int nx = 0, ny = 0;
            int moved = 0;

            if (0 == s_grains[i].alive) {
                continue;
            }

            x = s_grains[i].x;
            y = s_grains[i].y;
            s_occ[y][x] = 0;

            nx = x + dx;
            ny = y + dy;
            if (__sand_free(nx, ny)) {
                moved = 1;
            } else if (__sand_free(x + sx, y + sy)) {
                nx = x + sx;
                ny = y + sy;
                moved = 1;
            } else if (__sand_free(x + sx, y + dy)) {
                nx = x + sx;
                ny = y + dy;
                moved = 1;
            } else if (__sand_free(x + dx, y + sy)) {
                nx = x + dx;
                ny = y + sy;
                moved = 1;
            } else if (__sand_free(x - sx, y + dy)) {
                nx = x - sx;
                ny = y + dy;
                moved = 1;
            } else if (__sand_free(x + dx, y - sy)) {
                nx = x + dx;
                ny = y - sy;
                moved = 1;
            }

            if (moved) {
                s_grains[i].x = (int16_t)nx;
                s_grains[i].y = (int16_t)ny;
                s_occ[ny][nx] = 1;
            } else {
                s_occ[y][x] = 1;
            }
        }
    }
}

/**
 * @brief Redraw canvas from occupancy grid
 * @return none
 */
static void __sand_draw(void)
{
    int x = 0, y = 0, cx = 0, cy = 0;
    uint16_t *px = NULL;
    uint32_t i = 0;
    uint32_t n = (uint32_t)SAND_W * SAND_H;

    if ((NULL == s_canvas) || (NULL == s_cbuf)) {
        return;
    }

    px = (uint16_t *)s_cbuf;
    for (i = 0; i < n; i++) {
        px[i] = SAND_BG_RGB565;
    }

    for (y = 0; y < SAND_GH; y++) {
        for (x = 0; x < SAND_GW; x++) {
            uint16_t color = 0;

            if (0 == s_occ[y][x]) {
                continue;
            }
            color = ((x + y) & 1) ? SAND_FG_RGB565 : SAND_FG2_RGB565;
            for (cy = 0; cy < SAND_CELL; cy++) {
                for (cx = 0; cx < SAND_CELL; cx++) {
                    int xx = x * SAND_CELL + cx;
                    int yy = y * SAND_CELL + cy;
                    px[yy * SAND_W + xx] = color;
                }
            }
        }
    }

    lv_obj_invalidate(s_canvas);
}

/**
 * @brief LVGL timer: physics + draw when active
 * @param[in] t timer
 * @return none
 */
static void __sand_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (0 == s_active) {
        return;
    }
    __sand_step();
    __sand_draw();
}

/**
 * @brief Create sand screen resources (must hold disp lock)
 * @return OPRT_OK on success
 */
static OPERATE_RET __sand_ui_create(void)
{
    uint32_t buf_bytes = (uint32_t)SAND_W * SAND_H * sizeof(lv_color_t);

    if (NULL != s_screen) {
        return OPRT_OK;
    }

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    s_cbuf = (lv_color_t *)tal_psram_malloc(buf_bytes);
#else
    s_cbuf = (lv_color_t *)tal_malloc(buf_bytes);
#endif
    if (NULL == s_cbuf) {
        PR_ERR("sand: canvas buf alloc failed (%u)", (unsigned)buf_bytes);
        return OPRT_MALLOC_FAILED;
    }
    memset(s_cbuf, 0, buf_bytes);

    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x182838), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);

    s_canvas = lv_canvas_create(s_screen);
    lv_canvas_set_buffer(s_canvas, s_cbuf, SAND_W, SAND_H, LV_COLOR_FORMAT_RGB565);
    lv_obj_center(s_canvas);

    __sand_reset_grains();
    __sand_draw();

    s_timer = lv_timer_create(__sand_timer_cb, SAND_TICK_MS, NULL);
    lv_timer_pause(s_timer);
    s_active = 0;

    PR_NOTICE("sand: ui ready (%dx%d, %d grains)", SAND_W, SAND_H, SAND_COUNT);
    return OPRT_OK;
}

/**
 * @brief Create sand screen (timer paused)
 * @return OPRT_OK on success
 */
OPERATE_RET sand_fx_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL != s_screen) {
        return OPRT_OK;
    }

    lv_vendor_disp_lock();
    rt = __sand_ui_create();
    lv_vendor_disp_unlock();
    return rt;
}

/**
 * @brief Show sand screen and resume physics
 * @return OPRT_OK on success
 */
OPERATE_RET sand_fx_enter(void)
{
    if (NULL == s_screen) {
        if (OPRT_OK != sand_fx_init()) {
            return OPRT_COM_ERROR;
        }
    }

    lv_vendor_disp_lock();
    lv_screen_load(s_screen);
    s_active = 1;
    if (NULL != s_timer) {
        lv_timer_resume(s_timer);
    }
    lv_vendor_disp_unlock();
    return OPRT_OK;
}

/**
 * @brief Pause physics
 * @return none
 */
void sand_fx_leave(void)
{
    s_active = 0;
    lv_vendor_disp_lock();
    if (NULL != s_timer) {
        lv_timer_pause(s_timer);
    }
    lv_vendor_disp_unlock();
}

/**
 * @brief Get sand LVGL screen object
 * @return screen pointer, or NULL
 */
void *sand_fx_get_screen(void)
{
    return (void *)s_screen;
}

/**
 * @brief Legacy: init + enter immediately
 * @return OPRT_OK on success
 */
OPERATE_RET sand_fx_start(void)
{
    OPERATE_RET rt = sand_fx_init();
    if (OPRT_OK != rt) {
        return rt;
    }
    return sand_fx_enter();
}

#else /* sand not enabled */

OPERATE_RET sand_fx_init(void)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET sand_fx_enter(void)
{
    return OPRT_NOT_SUPPORTED;
}

void sand_fx_leave(void)
{
}

void *sand_fx_get_screen(void)
{
    return NULL;
}

OPERATE_RET sand_fx_start(void)
{
    return OPRT_NOT_SUPPORTED;
}

#endif
