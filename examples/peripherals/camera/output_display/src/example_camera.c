/**
 * @file example_camera.c
 * @brief Camera preview on the board display.
 *
 * Opens the camera in YUV422, converts each frame to the panel's RGB565
 * framebuffer and flushes it. On Jieli the vendor delivers frames from its own
 * raw_flow_task; the conversion and flush are handed to a worker thread so the
 * vendor task only copies the frame.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include <string.h>
#include <stdio.h>

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_memory.h"

#include "board_com_api.h"

#include "tdl_display_manage.h"
#include "tdl_camera_manage.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
/* Panel-only diagnostic: ignore camera data and draw a fixed horizontal
 * band pattern, flushing continuously. If a moving split line still shows on
 * this static picture the artifact is in the panel/flush path, not in the
 * camera frame assembly. */
#define EXAMPLE_PANEL_TEST 0

/* Camera-data diagnostic: after the first converted frame, stop updating the
 * panel. A seam that stays put on the frozen picture lives in one assembled
 * frame (frame stitching); a seam that still moves is a display effect. */
#define EXAMPLE_FREEZE_AFTER_FIRST 0

/* Three buffers so the frame being scanned out by the panel is never the
 * frame the converter is drawing into next. Two buffers were enough for the
 * 9 fps DVP preview, but at the SPI path's ~4 fps the panel has time to
 * re-scan the same buffer while it is filled again, which showed up as a
 * horizontal split that moved. */
#define DISPLAY_FRAME_BUFF_NUM 3

/* Bring-up bisect switch: 1 fills the frame buffer with a solid colour and
 * skips the YUV422 conversion, so a crash here isolates the display/EMI flush
 * path from the camera frame read. 0 is the normal example. */
#define EXAMPLE_SOLID_COLOR_TEST 0

/* Display-only bring-up: fill and flush one frame buffer from user_main before
 * the camera is opened. This separates the panel/EMI path from the camera
 * callback thread. */
#define EXAMPLE_DISPLAY_ONLY 0

/* The vendor delivers frames on its own raw_flow_task (8 KB stack). Doing the
 * YUV422->RGB565 conversion and the panel flush there hangs/faults on this
 * board, while the identical calls work on a Tuya thread. Hand each frame to a
 * worker instead: the callback only copies, the worker converts and flushes. */
#define EXAMPLE_DISPLAY_WORKER 0

/* Page test: numbered pages, one per candidate memory layout for the vendor's
 * 614400-byte YUV422 frame, so the operator can name the page that looks
 * right instead of recalling a fast sequence. */
#define EXAMPLE_PAGE_TEST 0

/* The final preview architecture: the vendor callback only caches + copies a
 * frame into a static buffer, and the Tuya main thread converts and flushes it
 * in a loop. No worker thread and no conversion on raw_flow_task. */
#define EXAMPLE_PREVIEW_LOOP 1

/* Single-frame bring-up test, entirely on the Tuya main thread: the camera
 * callback only copies one real frame (after cache maintenance), then
 * user_main converts and flushes it. No worker thread, no conversion on the
 * vendor raw_flow_task. */
#define EXAMPLE_CAMTEST 0

#if defined(EXAMPLE_CAMERA_IMG_ROTATION_0) && (EXAMPLE_CAMERA_IMG_ROTATION_0 == 1)
#define EXAMPLE_CAMERA_IMG_ROTATION TUYA_DISPLAY_ROTATION_0
#elif defined(EXAMPLE_CAMERA_IMG_ROTATION_90) && (EXAMPLE_CAMERA_IMG_ROTATION_90 == 1)
#define EXAMPLE_CAMERA_IMG_ROTATION TUYA_DISPLAY_ROTATION_90
#elif defined(EXAMPLE_CAMERA_IMG_ROTATION_180) && (EXAMPLE_CAMERA_IMG_ROTATION_180 == 1)
#define EXAMPLE_CAMERA_IMG_ROTATION TUYA_DISPLAY_ROTATION_180
#elif defined(EXAMPLE_CAMERA_IMG_ROTATION_270) && (EXAMPLE_CAMERA_IMG_ROTATION_270 == 1)
#define EXAMPLE_CAMERA_IMG_ROTATION TUYA_DISPLAY_ROTATION_270
#else
#define EXAMPLE_CAMERA_IMG_ROTATION TUYA_DISPLAY_ROTATION_0
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_DISP_HANDLE_T      sg_tdl_disp_hdl = NULL;
static TDL_DISP_DEV_INFO_T    sg_display_info;
static TDL_FB_MANAGE_HANDLE_T sg_fb_manage = NULL;

static TDL_CAMERA_HANDLE_T sg_tdl_camera_hdl = NULL;
static TUYA_YUV422_ORDER_E sg_yuv_order      = TUYA_YUV422_UYVY;

#if EXAMPLE_DISPLAY_WORKER
static SEM_HANDLE    sg_frame_sem  = NULL;
static THREAD_HANDLE sg_worker_hdl = NULL;
/* Static so the frame buffer can never alias the worker stack that the RTOS
 * allocates later from the same heap. */
static uint8_t          sg_yuv_pending[EXAMPLE_CAMERA_WIDTH * EXAMPLE_CAMERA_HEIGHT * 2] __attribute__((aligned(32)));
static uint32_t         sg_yuv_size      = sizeof(sg_yuv_pending);
static volatile uint8_t sg_pending_ready = 0;
static volatile uint8_t sg_worker_busy   = 0;
#endif

/***********************************************************
***********************function define**********************
***********************************************************/
/* YUV422 byte orders. The vendor frame is a packed 4-byte group per two
 * pixels; the sensor/ISP decides which byte is Y0/U/Y1/V. */
#define YUV_ORDER_UYVY 0
#define YUV_ORDER_YUYV 1
#define YUV_ORDER_YVYU 2
#define YUV_ORDER_VYUY 3

#define COLORBAR_TEST 0

static inline uint16_t __rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (uint16_t)(b >> 3));
}

static inline uint16_t __bswap16(uint16_t v)
{
    return (uint16_t)((v >> 8) | (v << 8));
}

static inline uint16_t __yuv_to_rgb565(uint8_t y, uint8_t u, uint8_t v, int swap)
{
    int32_t c = (int32_t)y - 16;
    int32_t d = (int32_t)u - 128;
    int32_t e = (int32_t)v - 128;
    int32_t r = (298 * c + 409 * e + 128) >> 8;
    int32_t g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int32_t b = (298 * c + 516 * d + 128) >> 8;

    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    uint16_t rgb = __rgb565((uint8_t)r, (uint8_t)g, (uint8_t)b);
    return swap ? __bswap16(rgb) : rgb;
}

/* The per-pixel integer conversion above costs three multiplies and a shift
 * per component. On the AC791 that is ~330 ns per pixel, i.e. 51 ms for the
 * 320x480 preview - the single largest cost in the loop. Replace it with
 * per-component tables: the Y term depends on Y only, and the U/V terms on U
 * or V only, so two pixels that share a chroma pair need five table reads
 * each and no multiplies at all. Rounding is folded into the tables; the
 * result differs from the exact formula by at most one LSB. */
static int16_t s_ycoef[256];
static int16_t s_rv[256];
static int16_t s_gu[256];
static int16_t s_gv[256];
static int16_t s_bu[256];
static int     s_lut_ready;

static void __yuv_lut_init(void)
{
    if (s_lut_ready) {
        return;
    }
    for (int i = 0; i < 256; i++) {
        int e      = i - 128;
        s_ycoef[i] = (int16_t)((298 * (i - 16)) >> 8);
        s_rv[i]    = (int16_t)((409 * e) >> 8);
        s_gu[i]    = (int16_t)((-100 * e) >> 8);
        s_gv[i]    = (int16_t)((-208 * e) >> 8);
        s_bu[i]    = (int16_t)((516 * e) >> 8);
    }
    s_lut_ready = 1;
}

static inline __attribute__((always_inline)) uint16_t __clamp_pack(int r, int g, int b, int swap)
{
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    uint16_t rgb = __rgb565((uint8_t)r, (uint8_t)g, (uint8_t)b);
    return swap ? __bswap16(rgb) : rgb;
}

static inline __attribute__((always_inline)) void __yuv_pair_to_rgb565(uint8_t y0, uint8_t y1, uint8_t u, uint8_t v,
                                                                       uint16_t *o0, uint16_t *o1, int swap)
{
    int c0 = s_ycoef[y0];
    int c1 = s_ycoef[y1];
    int rv = s_rv[v];
    int gu = s_gu[u];
    int gv = s_gv[v];
    int bu = s_bu[u];

    *o0 = __clamp_pack(c0 + rv, c0 + gu + gv, c0 + bu, swap);
    *o1 = __clamp_pack(c1 + rv, c1 + gu + gv, c1 + bu, swap);
}

/* Crop the left dst_w pixels of every source row (src_stride = src_w * 2) into
 * a dst_stride = dst_w * 2 RGB565 framebuffer. */
/* Packed YUYV fast path: the per-pair switch on `order` used to run inside the
 * pixel loop. Hoisting it out and walking the source with a flat pointer is
 * the shape the compiler turns into a tight loop on this MIPS core. */
static void __yuyv_crop_to_rgb565(const uint8_t *src, int src_w, int src_h, uint8_t *dst, int dst_w, int dst_h,
                                  int swap)
{
    int w = (dst_w < src_w) ? dst_w : src_w;
    int h = (dst_h < src_h) ? dst_h : src_h;

    __yuv_lut_init();

    for (int y = 0; y < h; y++) {
        const uint8_t *row = src + (size_t)y * (size_t)src_w * 2u;
        uint16_t      *out = (uint16_t *)(dst + (size_t)y * (size_t)dst_w * 2u);

        for (int x = 0; x < w; x += 2) {
            __yuv_pair_to_rgb565(row[0], row[2], row[1], row[3], &out[x], &out[x + 1], swap);
            row += 4;
        }
    }
}

static void __yuv422_crop_to_rgb565(const uint8_t *src, int src_w, int src_h, uint8_t *dst, int dst_w, int dst_h,
                                    int order, int swap)
{
    int w = (dst_w < src_w) ? dst_w : src_w;
    int h = (dst_h < src_h) ? dst_h : src_h;

    __yuv_lut_init();

    if (order == YUV_ORDER_YUYV) {
        __yuyv_crop_to_rgb565(src, src_w, src_h, dst, dst_w, dst_h, swap);
        return;
    }

    for (int y = 0; y < h; y++) {
        const uint8_t *row = src + (size_t)y * (size_t)src_w * 2u;
        uint16_t      *out = (uint16_t *)(dst + (size_t)y * (size_t)dst_w * 2u);

        for (int x = 0; x < w; x += 2) {
            uint8_t y0, y1, u, v;
            switch (order) {
            case YUV_ORDER_YUYV:
                y0 = row[0];
                u  = row[1];
                y1 = row[2];
                v  = row[3];
                break;
            case YUV_ORDER_YVYU:
                y0 = row[0];
                v  = row[1];
                y1 = row[2];
                u  = row[3];
                break;
            case YUV_ORDER_VYUY:
                v  = row[0];
                y0 = row[1];
                u  = row[2];
                y1 = row[3];
                break;
            case YUV_ORDER_UYVY:
            default:
                u  = row[0];
                y0 = row[1];
                v  = row[2];
                y1 = row[3];
                break;
            }
            row += 4;
            __yuv_pair_to_rgb565(y0, y1, u, v, &out[x], &out[x + 1], swap);
        }
    }
}

/* Planar YUV422P (the layout the SDK's own yuv422p_* helpers consume) to
 * RGB565 with nearest-neighbour scaling, so the source size can be lowered for
 * frame rate without changing the panel geometry. The column map is
 * precomputed because the divide per pixel is far too slow at 320x480. */
static uint16_t s_sx_map[480];
static int      s_sx_map_sw = -1;
static int      s_sx_map_dw = -1;

static void __yuv422p_scale_to_rgb565(const uint8_t *src, int src_w, int src_h, uint8_t *dst, int dst_w, int dst_h,
                                      int swap)
{
    size_t         y_size  = (size_t)src_w * (size_t)src_h;
    const uint8_t *u_plane = src + y_size;
    const uint8_t *v_plane = src + y_size + y_size / 2u;
    int            dw      = (dst_w < 480) ? dst_w : 480;

    if (s_sx_map_sw != src_w || s_sx_map_dw != dst_w) {
        for (int x = 0; x < dw; x++) {
            s_sx_map[x] = (uint16_t)((x * src_w) / dst_w);
        }
        s_sx_map_sw = src_w;
        s_sx_map_dw = dst_w;
    }

    for (int y = 0; y < dst_h; y++) {
        int            sy   = (y * src_h) / dst_h;
        const uint8_t *yrow = src + (size_t)sy * (size_t)src_w;
        const uint8_t *urow = u_plane + (size_t)sy * (size_t)(src_w / 2);
        const uint8_t *vrow = v_plane + (size_t)sy * (size_t)(src_w / 2);
        uint16_t      *out  = (uint16_t *)(dst + (size_t)y * (size_t)dst_w * 2u);

        for (int x = 0; x < dw; x++) {
            int sx = s_sx_map[x];
            out[x] = __yuv_to_rgb565(yrow[sx], urow[sx >> 1], vrow[sx >> 1], swap);
        }
    }
}

static const uint8_t s_digit3x5[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};

/* Draw the page number in the top-left corner, plus a row-varying band at the
 * bottom (vertical gradient and a diagonal), so a wrong stride or window shows
 * up immediately instead of hiding behind flat-colour bars. */
static void __draw_page_marker(uint8_t *fb, int w, int h, int page, int swap)
{
    uint16_t *out = (uint16_t *)fb;
    for (int y = 6; y < 48 && y < h; y++) {
        for (int x = 6; x < 78 && x < w; x++) {
            out[(size_t)y * w + x] = swap ? __bswap16(0x0000) : 0x0000;
        }
    }

    int digits[2];
    int nd    = 1;
    digits[0] = page % 10;
    if (page >= 10) {
        digits[1] = digits[0];
        digits[0] = (page / 10) % 10;
        nd        = 2;
    }

    for (int d = 0; d < nd; d++) {
        int ox = 12 + d * 30;
        for (int row = 0; row < 5; row++) {
            for (int col = 0; col < 3; col++) {
                if (s_digit3x5[digits[d]][row] & (1 << (2 - col))) {
                    for (int sy = 0; sy < 6; sy++) {
                        for (int sx = 0; sx < 6; sx++) {
                            int px = ox + col * 6 + sx;
                            int py = 12 + row * 6 + sy;
                            if (px < w && py < h) {
                                uint16_t c               = 0xFFFF;
                                out[(size_t)py * w + px] = swap ? __bswap16(c) : c;
                            }
                        }
                    }
                }
            }
        }
    }

    int band = 60;
    if (band > h) {
        band = h;
    }
    for (int y = h - band; y < h; y++) {
        int      level = ((y - (h - band)) * 255) / (band - 1);
        uint16_t c     = __rgb565((uint8_t)level, (uint8_t)level, (uint8_t)level);
        for (int x = 0; x < w; x++) {
            out[(size_t)y * w + x] = swap ? __bswap16(c) : c;
        }
        int diag = ((y - (h - band)) * 4) % w;
        for (int dx = 0; dx < 4; dx++) {
            out[(size_t)y * w + ((diag + dx) % w)] = swap ? __bswap16(0xF800) : 0xF800;
        }
    }
}
#if COLORBAR_TEST
static const uint8_t s_bar_yuv[8][3] = {
    {235, 128, 128}, /* white   */
    {210, 16, 146},  /* yellow  */
    {170, 166, 16},  /* cyan    */
    {145, 54, 34},   /* green   */
    {106, 202, 222}, /* magenta */
    {81, 90, 240},   /* red     */
    {41, 240, 110},  /* blue    */
    {16, 128, 128},  /* black   */
};

static const uint16_t s_bar_rgb[8] = {
    0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F, 0x0000,
};

static void __fill_rgb_bars(uint8_t *fb, int w, int h, int swap)
{
    uint16_t *out = (uint16_t *)fb;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int      bar           = (x * 8) / w;
            uint16_t c             = s_bar_rgb[bar];
            out[(size_t)y * w + x] = swap ? __bswap16(c) : c;
        }
    }
}

/* Write a packed YUYV frame: eight bars on top, a per-row gradient with a
 * diagonal below, so a wrong source stride breaks the gradient/diagonal. */
static void __fill_yuv_bars(uint8_t *buf, int w, int h)
{
    int split = (h * 2) / 3;

    for (int y = 0; y < h; y++) {
        uint8_t *row = buf + (size_t)y * (size_t)w * 2u;
        for (int x = 0; x < w; x += 2) {
            uint8_t yy;
            uint8_t uu = 128;
            uint8_t vv = 128;

            if (y < split) {
                int bar = ((x < 320) ? (x / 40) : ((x - 320) / 40)) & 7;
                yy      = s_bar_yuv[bar][0];
                uu      = s_bar_yuv[bar][1];
                vv      = s_bar_yuv[bar][2];
            } else {
                yy       = (uint8_t)(16 + ((y - split) * 219) / (h - split - 1));
                int diag = ((y - split) * 4) % w;
                if (x == diag || x == diag + 1) {
                    yy = 235;
                }
            }
            row[x * 2 + 0] = yy;
            row[x * 2 + 1] = uu;
            row[x * 2 + 2] = yy;
            row[x * 2 + 3] = vv;
        }
    }
}

static void __colorbar_test(void)
{
    printf("[BARTEST] begin\n");
    uint8_t *yuv = tkl_system_malloc(640u * 480u * 2u);
    if (yuv == NULL) {
        printf("[BARTEST] no yuv buffer\n");
        return;
    }

    for (int swap = 0; swap < 2; swap++) {
        TDL_DISP_FRAME_BUFF_T *fb = tdl_disp_get_free_fb(sg_fb_manage);
        if (fb == NULL) {
            break;
        }
        printf("[BARTEST] rgb565 bars swap=%d\n", swap);
        __fill_rgb_bars(fb->frame, fb->width, fb->height, swap);
        tdl_disp_dev_flush(sg_tdl_disp_hdl, fb);
        tal_system_sleep(2000);
    }

    __fill_yuv_bars(yuv, 640, 480);
    static const char *names[4] = {"UYVY", "YUYV", "YVYU", "VYUY"};
    for (int order = 0; order < 4; order++) {
        for (int swap = 0; swap < 2; swap++) {
            TDL_DISP_FRAME_BUFF_T *fb = tdl_disp_get_free_fb(sg_fb_manage);
            if (fb == NULL) {
                break;
            }
            printf("[BARTEST] yuv bars order=%s swap=%d\n", names[order], swap);
            __yuv422_crop_to_rgb565(yuv, 640, 480, fb->frame, fb->width, fb->height, order, swap);
            tdl_disp_dev_flush(sg_tdl_disp_hdl, fb);
            tal_system_sleep(2000);
        }
    }

    tkl_system_free(yuv);
    printf("[BARTEST] done\n");
}
#endif

#if EXAMPLE_PREVIEW_LOOP
#if defined(JIELI_SELECTED_CHIP_WL82)
extern void flushinv_dcache(void *ptr, int len);
#endif

static SEM_HANDLE        sg_preview_sem      = NULL;
static volatile uint32_t sg_preview_len      = 0;
static volatile uint8_t  sg_preview_ready    = 0;
static uint32_t          sg_preview_frames   = 0;
static uint32_t          sg_preview_start_ms = 0;
static uint32_t          sg_window_start_ms  = 0;
static uint32_t          sg_conv_ms_sum      = 0;
static uint32_t          sg_conv_only_sum    = 0;
static uint32_t          sg_wait_sem_sum     = 0;
static uint32_t          sg_wait_fb_sum      = 0;
static uint32_t          sg_conv_ms_max      = 0;
/* Sized for the largest source the DVP path can deliver. Frames are still
 * validated against this size in the callback, because the converter derives
 * its plane offsets from the frame dimensions. */
static uint8_t           sg_preview_buf[640 * 480 * 2] __attribute__((aligned(32)));
static volatile uint8_t *sg_preview_ptr = NULL;
static volatile uint32_t sg_preview_w   = 0;
static volatile uint32_t sg_preview_h   = 0;

static OPERATE_RET __preview_frame_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    (void)hdl;

    if (sg_preview_ready || frame->data == NULL || frame->data_len == 0) {
        return OPRT_OK;
    }
    /* The converter derives the U/V offsets from width*height, so a frame that
     * does not fit the copy buffer (or is shorter than it claims) would read
     * past it. Drop such a frame instead of rendering garbage. */
    {
        uint32_t need = (uint32_t)frame->width * (uint32_t)frame->height * 2u;
        if (frame->width == 0 || frame->height == 0 || need > sizeof(sg_preview_buf) || frame->data_len < need) {
            return OPRT_OK;
        }
    }
    uint32_t len = frame->data_len;
    if (len > sizeof(sg_preview_buf)) {
        len = sizeof(sg_preview_buf);
    }
#if EXAMPLE_PAGE_TEST
    {
        static uint32_t cb_count;
        if (cb_count < 2) {
            printf("[PAGE] cb data=%p len=%u\n", frame->data, (unsigned)len);
            cb_count++;
        }
    }
#endif
#if defined(JIELI_SELECTED_CHIP_WL82)
    /* frame->data is the TDL pool frame that tkl_dvp already filled; this
     * invalidate is defensive and keeps the copy from reading stale lines. */
    flushinv_dcache(frame->data, (int)frame->data_len);
#endif
#if EXAMPLE_PAGE_TEST
    memcpy(sg_preview_buf, frame->data, len);
    sg_preview_len = len;
#else
    /* Copy the frame on the vendor thread: reading the vendor buffer from
     * another thread while the ISP refills it ended in a device drop, so the
     * copy stays even though it costs an extra pass over SDRAM. */
    memcpy(sg_preview_buf, frame->data, len);
    sg_preview_ptr = sg_preview_buf;
    sg_preview_len = len;
    sg_preview_w   = frame->width;
    sg_preview_h   = frame->height;
#endif
    sg_preview_ready = 1;
    tal_semaphore_post(sg_preview_sem);

    return OPRT_OK;
}

static OPERATE_RET __preview_run(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&sg_preview_sem, 0, 1));

    sg_tdl_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    if (sg_tdl_camera_hdl == NULL) {
        printf("[PREVIEW] camera dev %s not found\n", CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    TDL_CAMERA_DEV_INFO_T cam_info = {0};
    tdl_camera_dev_get_info(sg_tdl_camera_hdl, &cam_info);
    printf("[PREVIEW] camera info max=%ux%u yuv_order=%d\n", cam_info.max_width, cam_info.max_height,
           cam_info.yuv_order);

    TDL_CAMERA_CFG_T cfg;
    memset(&cfg, 0, sizeof(TDL_CAMERA_CFG_T));
    cfg.fps          = EXAMPLE_CAMERA_FPS;
    cfg.width        = EXAMPLE_CAMERA_WIDTH;
    cfg.height       = EXAMPLE_CAMERA_HEIGHT;
    cfg.out_fmt      = TDL_CAMERA_FMT_YUV422;
    cfg.get_frame_cb = __preview_frame_cb;

    TUYA_CALL_ERR_RETURN(tdl_camera_dev_open(sg_tdl_camera_hdl, &cfg));
    printf("[PREVIEW] camera opened, loop start\n");
    __yuv_lut_init();
    sg_preview_start_ms = (uint32_t)tal_system_get_millisecond();
    sg_window_start_ms  = sg_preview_start_ms;

    while (1) {
#if EXAMPLE_PANEL_TEST
        {
            TDL_DISP_FRAME_BUFF_T *fb = tdl_disp_get_free_fb(sg_fb_manage);

            if (fb != NULL) {
                uint16_t *p = (uint16_t *)fb->frame;

                for (int y = 0; y < (int)fb->height; y++) {
                    uint16_t c = ((y / 20) & 1) ? 0xF800 : 0x07E0;

                    for (int x = 0; x < (int)fb->width; x++) {
                        p[(size_t)y * fb->width + (size_t)x] = c;
                    }
                }
                tdl_disp_dev_flush(sg_tdl_disp_hdl, fb);
            }
            tal_system_sleep(50);
            continue;
        }
#endif
        uint32_t wait_start = (uint32_t)tal_system_get_millisecond();

        if (tal_semaphore_wait(sg_preview_sem, 1000) != OPRT_OK) {
            printf("[PREVIEW] no frame yet\n");
            continue;
        }
        if (!sg_preview_ready) {
            continue;
        }

        uint32_t t_sem = (uint32_t)tal_system_get_millisecond();

        TDL_DISP_FRAME_BUFF_T *fb = tdl_disp_get_free_fb(sg_fb_manage);
        if (fb == NULL) {
            printf("[PREVIEW] no free fb\n");
            sg_preview_ready = 0;
            continue;
        }
        {
            uint32_t t = (uint32_t)tal_system_get_millisecond();

            sg_wait_sem_sum += t_sem - wait_start;
            sg_wait_fb_sum += t - t_sem;
        }

        uint32_t conv_start = (uint32_t)tal_system_get_millisecond();

#if defined(JIELI_SELECTED_CHIP_WL82) && defined(JIELI_CAMERA_PATH_SPI) && (JIELI_CAMERA_PATH_SPI == 1)
        /* SPI GC0310: the vendor 2-wire stream carries packed YUYV, 1280 bytes
         * per 640-pixel line (spi_video.h calls it yuyv422). */
        __yuv422_crop_to_rgb565(sg_preview_ptr ? sg_preview_ptr : sg_preview_buf,
                                sg_preview_w ? (int)sg_preview_w : EXAMPLE_CAMERA_WIDTH,
                                sg_preview_h ? (int)sg_preview_h : EXAMPLE_CAMERA_HEIGHT, fb->frame, fb->width,
                                fb->height, YUV_ORDER_YUYV, sg_display_info.is_swap);
#elif defined(JIELI_SELECTED_CHIP_WL82)
        /* DVP: planar YUV422P, read straight from the vendor buffer. */
        __yuv422p_scale_to_rgb565(sg_preview_ptr ? sg_preview_ptr : sg_preview_buf,
                                  sg_preview_w ? (int)sg_preview_w : EXAMPLE_CAMERA_WIDTH,
                                  sg_preview_h ? (int)sg_preview_h : EXAMPLE_CAMERA_HEIGHT, fb->frame, fb->width,
                                  fb->height, sg_display_info.is_swap);
#else
        /* Other platforms deliver packed YUYV here; keep the packed converter
         * so this example still works on the ESP32 config. */
        __yuv422_crop_to_rgb565(sg_preview_buf, EXAMPLE_CAMERA_WIDTH, EXAMPLE_CAMERA_HEIGHT, fb->frame, fb->width,
                                fb->height, YUV_ORDER_YUYV, sg_display_info.is_swap);
#endif
        {
            uint32_t conv_ms = (uint32_t)tal_system_get_millisecond() - conv_start;

            sg_conv_only_sum += conv_ms;
        }
        tdl_disp_dev_flush(sg_tdl_disp_hdl, fb);
        sg_preview_ready = 0;

        sg_preview_frames++;
        {
            uint32_t spent = (uint32_t)tal_system_get_millisecond() - conv_start;
            sg_conv_ms_sum += spent;
            if (spent > sg_conv_ms_max) {
                sg_conv_ms_max = spent;
            }
        }
#if EXAMPLE_FREEZE_AFTER_FIRST
        printf("[PREVIEW] freeze: first frame on screen, updates stopped\n");
        while (1) {
            tal_system_sleep(1000);
        }
#endif
        if ((sg_preview_frames % 30u) == 0u) {
            uint32_t now = (uint32_t)tal_system_get_millisecond();
            uint32_t win = now - sg_window_start_ms;
            printf("[PREVIEW] fps=%u frames=%u conv+flush=%ums conv_only=%ums max=%ums sem=%ums fb=%ums src=%ux%u "
                   "len=%u\n",
                   (unsigned)((sg_preview_frames * 1000u) / (win ? win : 1u)), (unsigned)sg_preview_frames,
                   (unsigned)(sg_conv_ms_sum / (sg_preview_frames ? sg_preview_frames : 1u)),
                   (unsigned)(sg_conv_only_sum / (sg_preview_frames ? sg_preview_frames : 1u)),
                   (unsigned)sg_conv_ms_max, (unsigned)(sg_wait_sem_sum / (sg_preview_frames ? sg_preview_frames : 1u)),
                   (unsigned)(sg_wait_fb_sum / (sg_preview_frames ? sg_preview_frames : 1u)), (unsigned)sg_preview_w,
                   (unsigned)sg_preview_h, (unsigned)sg_preview_len);
            sg_conv_ms_sum     = 0;
            sg_conv_only_sum   = 0;
            sg_wait_sem_sum    = 0;
            sg_wait_fb_sum     = 0;
            sg_conv_ms_max     = 0;
            sg_preview_frames  = 0;
            sg_window_start_ms = now;
        }
    }
}

#if EXAMPLE_PAGE_TEST
/* The vendor delivers 614400 bytes for 640x480 YUV422, a size that fits
 * several memory layouts: packed YUYV/YVYU, planar YUV422P (Y + U + V) or
 * semi-planar (Y + interleaved UV). The length alone cannot tell them apart,
 * and the wrong assumption shows up as colour blocks. One numbered page per
 * layout lets the operator name the continuous one. */
#define YUV_LAYOUT_PACKED_YUYV 0
#define YUV_LAYOUT_PACKED_YVYU 1
#define YUV_LAYOUT_PLANAR_UV   2
#define YUV_LAYOUT_PLANAR_VU   3
#define YUV_LAYOUT_SEMI_UV     4
#define YUV_LAYOUT_SEMI_VU     5
#define YUV_LAYOUT_NUM         6

/* 320-column planar YUV422P crop, fed to the vendor converter. The panel is
 * 320x480, so the left half of the 640x480 frame is re-packed row by row: the
 * vendor helper assumes the plane stride matches the width it is given. */
static uint8_t sg_planar_crop[(EXAMPLE_CAMERA_WIDTH / 2) * EXAMPLE_CAMERA_HEIGHT * 2] __attribute__((aligned(32)));

#if defined(JIELI_SELECTED_CHIP_WL82)
/* common_lib.a: be = 1 stores RGB565 big-endian, matching is_swap=1. */
extern void yuv422p_quto_rgb565(unsigned char *yuvBuffer_in, unsigned char *rgbBuffer_out, int width, int height,
                                char be);
#endif

static void __crop_planar_left_half(const uint8_t *src, int src_w, int src_h, uint8_t *dst)
{
    int            dst_w = src_w / 2;
    size_t         src_y = (size_t)src_w * (size_t)src_h;
    size_t         dst_y = (size_t)dst_w * (size_t)src_h;
    const uint8_t *src_u = src + src_y;
    const uint8_t *src_v = src + src_y + src_y / 2u;
    uint8_t       *dst_u = dst + dst_y;
    uint8_t       *dst_v = dst + dst_y + dst_y / 2u;

    for (int y = 0; y < src_h; y++) {
        memcpy(dst + (size_t)y * (size_t)dst_w, src + (size_t)y * (size_t)src_w, (size_t)dst_w);
        memcpy(dst_u + (size_t)y * (size_t)(dst_w / 2), src_u + (size_t)y * (size_t)(src_w / 2), (size_t)(dst_w / 2));
        memcpy(dst_v + (size_t)y * (size_t)(dst_w / 2), src_v + (size_t)y * (size_t)(src_w / 2), (size_t)(dst_w / 2));
    }
}

static void __yuv422_layout_to_rgb565(const uint8_t *src, int src_w, int src_h, uint8_t *dst, int dst_w, int dst_h,
                                      int layout, int swap)
{
    int            w        = (dst_w < src_w) ? dst_w : src_w;
    int            h        = (dst_h < src_h) ? dst_h : src_h;
    size_t         y_size   = (size_t)src_w * (size_t)src_h;
    const uint8_t *y_plane  = src;
    const uint8_t *u_plane  = src + y_size;
    const uint8_t *v_plane  = src + y_size + y_size / 2u;
    const uint8_t *uv_plane = src + y_size;

    if (layout == YUV_LAYOUT_PLANAR_VU) {
        v_plane = src + y_size;
        u_plane = src + y_size + y_size / 2u;
    }

    for (int y = 0; y < h; y++) {
        const uint8_t *prow  = src + (size_t)y * (size_t)src_w * 2u;
        const uint8_t *yrow  = y_plane + (size_t)y * (size_t)src_w;
        const uint8_t *urow  = u_plane + ((size_t)y * (size_t)src_w) / 2u;
        const uint8_t *vrow  = v_plane + ((size_t)y * (size_t)src_w) / 2u;
        const uint8_t *uvrow = uv_plane + (size_t)y * (size_t)src_w;
        uint16_t      *out   = (uint16_t *)(dst + (size_t)y * (size_t)dst_w * 2u);

        for (int x = 0; x < w; x += 2) {
            uint8_t y0, y1, u, v;

            switch (layout) {
            case YUV_LAYOUT_PACKED_YUYV:
                y0 = prow[0];
                u  = prow[1];
                y1 = prow[2];
                v  = prow[3];
                prow += 4;
                break;
            case YUV_LAYOUT_PACKED_YVYU:
                y0 = prow[0];
                v  = prow[1];
                y1 = prow[2];
                u  = prow[3];
                prow += 4;
                break;
            case YUV_LAYOUT_PLANAR_UV:
            case YUV_LAYOUT_PLANAR_VU:
                y0 = yrow[x];
                y1 = yrow[x + 1];
                u  = urow[x >> 1];
                v  = vrow[x >> 1];
                break;
            case YUV_LAYOUT_SEMI_VU:
                y0 = yrow[x];
                y1 = yrow[x + 1];
                v  = uvrow[x];
                u  = uvrow[x + 1];
                break;
            case YUV_LAYOUT_SEMI_UV:
            default:
                y0 = yrow[x];
                y1 = yrow[x + 1];
                u  = uvrow[x];
                v  = uvrow[x + 1];
                break;
            }
            out[x]     = __yuv_to_rgb565(y0, u, v, swap);
            out[x + 1] = __yuv_to_rgb565(y1, u, v, swap);
        }
    }
}

static OPERATE_RET __page_test_run(void)
{
    OPERATE_RET        rt                            = OPRT_OK;
    static const char *s_layout_name[YUV_LAYOUT_NUM] = {"packed-YUYV", "packed-YVYU", "planar-YUV",
                                                        "planar-YVU",  "semi-UV",     "semi-VU"};

    /* Open the camera before allocating anything else. The previous page test
     * allocated a 600 KB bar buffer first, freed it and then opened the camera;
     * its first camera frame crashed in os_sem_post. Keep this order identical
     * to the stable preview build, whose heap held only the display buffers and
     * the vendor video buffers. */
    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&sg_preview_sem, 0, 1));

    sg_tdl_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    if (sg_tdl_camera_hdl == NULL) {
        printf("[PAGE] camera dev %s not found\n", CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    TDL_CAMERA_CFG_T cfg;
    memset(&cfg, 0, sizeof(TDL_CAMERA_CFG_T));
    cfg.fps          = EXAMPLE_CAMERA_FPS;
    cfg.width        = EXAMPLE_CAMERA_WIDTH;
    cfg.height       = EXAMPLE_CAMERA_HEIGHT;
    cfg.out_fmt      = TDL_CAMERA_FMT_YUV422;
    cfg.get_frame_cb = __preview_frame_cb;
    TUYA_CALL_ERR_RETURN(tdl_camera_dev_open(sg_tdl_camera_hdl, &cfg));
    printf("[PAGE] camera opened, pages 11-16 swap=%d\n", sg_display_info.is_swap);

    for (int page = 11; page <= 16; page++) {
        const char *name = NULL;

        if (page <= 12) {
            name = (page == 11) ? "vendor-422p-be1" : "vendor-422p-be0";
        } else {
            name = s_layout_name[YUV_LAYOUT_PLANAR_UV + (page - 13)];
        }

        printf("[PAGE] %d wait %s\n", page, name);
        if (tal_semaphore_wait(sg_preview_sem, 5000) != OPRT_OK) {
            printf("[PAGE] %d no frame\n", page);
            continue;
        }
        printf("[PAGE] %d frame len=%u\n", page, (unsigned)sg_preview_len);

        TDL_DISP_FRAME_BUFF_T *fb = tdl_disp_get_free_fb(sg_fb_manage);
        if (fb == NULL) {
            printf("[PAGE] %d no free fb\n", page);
            sg_preview_ready = 0;
            continue;
        }

        printf("[PAGE] %d convert %s\n", page, name);
        if (page <= 12) {
            /* Primary path: the SDK's own planar YUV422P -> RGB565 converter.
             * Re-pack the left 320 columns first so the helper's implicit
             * stride (width) matches the source planes. */
            __crop_planar_left_half(sg_preview_buf, EXAMPLE_CAMERA_WIDTH, EXAMPLE_CAMERA_HEIGHT, sg_planar_crop);
            yuv422p_quto_rgb565(sg_planar_crop, fb->frame, EXAMPLE_CAMERA_WIDTH / 2, EXAMPLE_CAMERA_HEIGHT,
                                (page == 11) ? 1 : 0);
        } else {
            int layout = YUV_LAYOUT_PLANAR_UV + (page - 13);
            __yuv422_layout_to_rgb565(sg_preview_buf, EXAMPLE_CAMERA_WIDTH, EXAMPLE_CAMERA_HEIGHT, fb->frame, fb->width,
                                      fb->height, layout, sg_display_info.is_swap);
        }
        __draw_page_marker(fb->frame, fb->width, fb->height, page, sg_display_info.is_swap);
        printf("[PAGE] %d flush\n", page);
        tdl_disp_dev_flush(sg_tdl_disp_hdl, fb);
        sg_preview_ready = 0;
        printf("[PAGE] %d camera %s\n", page, name);
        tal_system_sleep(5000);
    }

    printf("[PAGE] done\n");
    return OPRT_OK;
}
#endif
#endif

#if EXAMPLE_DISPLAY_WORKER
#if defined(JIELI_SELECTED_CHIP_WL82)
/* Written by the vendor ISP DMA; drop stale lines before the CPU copies it. */
extern void flushinv_dcache(void *ptr, int len);
#endif

static void __display_worker(void *arg)
{
    (void)arg;

    printf("[DISP_WORKER] thread running\n");
    while (1) {
        if (tal_semaphore_wait(sg_frame_sem, SEM_WAIT_FOREVER) != OPRT_OK) {
            continue;
        }
        sg_pending_ready = 0;
        sg_worker_busy   = 1;

        TDL_DISP_FRAME_BUFF_T *fb = tdl_disp_get_free_fb(sg_fb_manage);
        if (fb != NULL) {
            printf("[DISP_WORKER] convert src=%p %ux%u dst=%p %ux%u order=%d swap=%d\n", sg_yuv_pending,
                   EXAMPLE_CAMERA_WIDTH, EXAMPLE_CAMERA_HEIGHT, fb->frame, fb->width, fb->height, YUV_ORDER_YUYV,
                   sg_display_info.is_swap);
            __yuv422_crop_to_rgb565(sg_yuv_pending, EXAMPLE_CAMERA_WIDTH, EXAMPLE_CAMERA_HEIGHT, fb->frame, fb->width,
                                    fb->height, YUV_ORDER_YUYV, sg_display_info.is_swap);
            printf("[DISP_WORKER] convert done, flush\n");
            tdl_disp_dev_flush(sg_tdl_disp_hdl, fb);
            printf("[DISP_WORKER] flush done\n");
        }

        sg_worker_busy = 0;
    }
}
#endif
static OPERATE_RET __get_camera_raw_frame_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    OPERATE_RET rt = OPRT_OK;
#if !EXAMPLE_DISPLAY_WORKER
    TDL_DISP_FRAME_BUFF_T *convert_fb = NULL;
#endif

#if EXAMPLE_DISPLAY_WORKER
    /* This runs on the vendor raw_flow_task: keep it to a copy and a post. */
    if (sg_yuv_pending == NULL || sg_worker_busy || sg_pending_ready || frame->data == NULL ||
        frame->data_len > sg_yuv_size) {
        return OPRT_OK;
    }
    {
        static uint32_t cb_cnt;
        if (cb_cnt < 3) {
            printf("[DISP_WORKER] cb data=%p len=%u\n", frame->data, frame->data_len);
            cb_cnt++;
        }
    }
#if defined(JIELI_SELECTED_CHIP_WL82)
    flushinv_dcache(frame->data, (int)frame->data_len);
#endif
    memcpy(sg_yuv_pending, frame->data, frame->data_len);
    sg_pending_ready = 1;
    tal_semaphore_post(sg_frame_sem);
    return OPRT_OK;
#else
    convert_fb = tdl_disp_get_free_fb(sg_fb_manage);
    TUYA_CHECK_NULL_RETURN(convert_fb, OPRT_COM_ERROR);

    {
        static uint32_t dbg_cnt;
        if (dbg_cnt < 4) {
            printf("[disp-dbg] frame data=%p wh=%ux%u len=%u fb=%p fbwh=%ux%u fblen=%u in_order=%d\n", frame->data,
                   frame->width, frame->height, frame->data_len, convert_fb->frame, convert_fb->width,
                   convert_fb->height, convert_fb->len, sg_yuv_order);
            dbg_cnt++;
        }
    }

#if EXAMPLE_SOLID_COLOR_TEST
    /* RGB565 red, byte order swapped so both halves show the colour. */
    memset(convert_fb->frame, 0xF8, convert_fb->len);
    (void)frame;
    printf("[disp-dbg] solid fill done\n");
#else
    TUYA_CALL_ERR_LOG(tdl_disp_convert_yuv422_to_fb(frame->data, frame->width, frame->height, convert_fb,
                                                    sg_display_info.is_swap, EXAMPLE_CAMERA_IMG_ROTATION,
                                                    sg_yuv_order));
    printf("[disp-dbg] convert done\n");
#endif

    tdl_disp_dev_flush(sg_tdl_disp_hdl, convert_fb);
    printf("[disp-dbg] flush returned\n");

    return rt;
#endif
}

#if defined(JIELI_SELECTED_CHIP_WL82)
/* The vendor ISP writes the overlay buffer; drop stale CPU cache lines before
 * reading it. Declared here because the example does not pull vendor headers. */
extern void flushinv_dcache(void *ptr, int len);
#endif

#if EXAMPLE_CAMTEST
static SEM_HANDLE       sg_camtest_sem = NULL;
static uint8_t         *sg_camtest_buf = NULL;
static uint32_t         sg_camtest_len = 0;
static volatile uint8_t sg_camtest_got = 0;

static OPERATE_RET __camtest_frame_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    (void)hdl;

    if (sg_camtest_got || frame->data == NULL || frame->data_len == 0 || sg_camtest_buf == NULL) {
        return OPRT_OK;
    }

    printf("[CAMTEST] cb frame data=%p len=%u\n", frame->data, frame->data_len);
#if defined(JIELI_SELECTED_CHIP_WL82)
    flushinv_dcache(frame->data, (int)frame->data_len);
    printf("[CAMTEST] cb cache invalidated\n");
#endif
    uint32_t len = frame->data_len;
    if (len > (uint32_t)EXAMPLE_CAMERA_WIDTH * EXAMPLE_CAMERA_HEIGHT * 2u) {
        len = (uint32_t)EXAMPLE_CAMERA_WIDTH * EXAMPLE_CAMERA_HEIGHT * 2u;
    }
    memcpy(sg_camtest_buf, frame->data, len);
    sg_camtest_len = len;
    sg_camtest_got = 1;
    printf("[CAMTEST] cb copied %u bytes\n", len);
    tal_semaphore_post(sg_camtest_sem);

    return OPRT_OK;
}
#endif

static OPERATE_RET __display_init(void)
{
    OPERATE_RET rt     = OPRT_OK;
    uint16_t    width  = EXAMPLE_CAMERA_WIDTH;
    uint16_t    height = EXAMPLE_CAMERA_HEIGHT;

    memset(&sg_display_info, 0, sizeof(TDL_DISP_DEV_INFO_T));

    sg_tdl_disp_hdl = tdl_disp_find_dev(DISPLAY_NAME);
    if (NULL == sg_tdl_disp_hdl) {
        PR_ERR("display dev %s not found", DISPLAY_NAME);
        return OPRT_NOT_FOUND;
    }

    TUYA_CALL_ERR_RETURN(tdl_disp_dev_get_info(sg_tdl_disp_hdl, &sg_display_info));
    printf("[disp-dbg] info %ux%u fmt=%d swap=%d\n", sg_display_info.width, sg_display_info.height, sg_display_info.fmt,
           sg_display_info.is_swap);

    if (sg_display_info.fmt != TUYA_PIXEL_FMT_RGB565 && sg_display_info.fmt != TUYA_PIXEL_FMT_MONOCHROME) {
        PR_ERR("display pixel format %d not supported", sg_display_info.fmt);
        return OPRT_NOT_SUPPORTED;
    }

    TUYA_CALL_ERR_RETURN(tdl_disp_dev_open(sg_tdl_disp_hdl));

    tdl_disp_set_brightness(sg_tdl_disp_hdl, 100); // Set brightness to 100%

    TUYA_CALL_ERR_RETURN(tdl_disp_fb_manage_init(&sg_fb_manage));

    /*create frame buffer*/
    width  = sg_display_info.width;
    height = sg_display_info.height;

    for (uint8_t i = 0; i < DISPLAY_FRAME_BUFF_NUM; i++) {
        TUYA_CALL_ERR_LOG(tdl_disp_fb_manage_add(sg_fb_manage, sg_display_info.fmt, width, height));
    }
    printf("[disp-dbg] fb add %ux%u fmt=%d num=%d\n", width, height, sg_display_info.fmt, DISPLAY_FRAME_BUFF_NUM);

#if EXAMPLE_DISPLAY_ONLY
    {
        TDL_DISP_FRAME_BUFF_T *fb = tdl_disp_get_free_fb(sg_fb_manage);
        if (fb == NULL) {
            printf("[disp-dbg] selftest: no free fb\n");
            return OPRT_COM_ERROR;
        }
        printf("[disp-dbg] selftest fb=%p len=%u\n", fb->frame, fb->len);
        fb->frame[0] = 0xF8;
        printf("[disp-dbg] selftest first byte written\n");
        memset(fb->frame + 1, 0xF8, fb->len - 1);
        printf("[disp-dbg] selftest fill done\n");
        tdl_disp_dev_flush(sg_tdl_disp_hdl, fb);
        printf("[disp-dbg] selftest flush returned\n");

        /* Exercise the conversion itself on this thread (4 KB stack) with a
         * synthetic 640x480 YUV422 frame, so the camera callback thread is not
         * part of the experiment. */
        uint8_t *yuv = tkl_system_malloc(640 * 480 * 2);
        if (yuv != NULL) {
            for (uint32_t i = 0; i < 640u * 480u * 2u; i++) {
                yuv[i] = (uint8_t)(i & 0xff);
            }
            fb = tdl_disp_get_free_fb(sg_fb_manage);
            if (fb != NULL) {
                printf("[disp-dbg] selftest convert start fb=%p len=%u\n", fb->frame, fb->len);
                tdl_disp_convert_yuv422_to_fb(yuv, 640, 480, fb, false, TUYA_DISPLAY_ROTATION_0, TUYA_YUV422_UYVY);
                printf("[disp-dbg] selftest convert done\n");
                tdl_disp_dev_flush(sg_tdl_disp_hdl, fb);
                printf("[disp-dbg] selftest convert flush returned\n");
            }
            tkl_system_free(yuv);
        } else {
            printf("[disp-dbg] selftest no yuv buffer\n");
        }
    }
#endif

    return OPRT_OK;
}

#if EXAMPLE_CAMTEST
static OPERATE_RET __camtest_run(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&sg_camtest_sem, 0, 1));

    sg_camtest_buf = tkl_system_malloc((uint32_t)EXAMPLE_CAMERA_WIDTH * EXAMPLE_CAMERA_HEIGHT * 2u);
    if (sg_camtest_buf == NULL) {
        printf("[CAMTEST] no memory for the frame copy\n");
        return OPRT_MALLOC_FAILED;
    }

    sg_tdl_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    if (sg_tdl_camera_hdl == NULL) {
        printf("[CAMTEST] camera dev %s not found\n", CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    TDL_CAMERA_DEV_INFO_T cam_info = {0};
    tdl_camera_dev_get_info(sg_tdl_camera_hdl, &cam_info);
    sg_yuv_order = TUYA_YUV422_YUYV;

    TDL_CAMERA_CFG_T cfg;
    memset(&cfg, 0, sizeof(TDL_CAMERA_CFG_T));
    cfg.fps          = EXAMPLE_CAMERA_FPS;
    cfg.width        = EXAMPLE_CAMERA_WIDTH;
    cfg.height       = EXAMPLE_CAMERA_HEIGHT;
    cfg.out_fmt      = TDL_CAMERA_FMT_YUV422;
    cfg.get_frame_cb = __camtest_frame_cb;

    printf("[CAMTEST] opening camera %ux%u@%u yuv_order=%d\n", EXAMPLE_CAMERA_WIDTH, EXAMPLE_CAMERA_HEIGHT,
           EXAMPLE_CAMERA_FPS, sg_yuv_order);
    TUYA_CALL_ERR_RETURN(tdl_camera_dev_open(sg_tdl_camera_hdl, &cfg));
    printf("[CAMTEST] camera opened, waiting for one frame\n");

    rt = tal_semaphore_wait(sg_camtest_sem, 5000);
    printf("[CAMTEST] wait rt=%d got=%u len=%u\n", rt, sg_camtest_got, sg_camtest_len);

    tdl_camera_dev_close(sg_tdl_camera_hdl);
    printf("[CAMTEST] camera closed\n");

    if (!sg_camtest_got) {
        printf("[CAMTEST] no frame within 5 s\n");
        return OPRT_TIMEOUT;
    }

    TDL_DISP_FRAME_BUFF_T *fb = tdl_disp_get_free_fb(sg_fb_manage);
    if (fb == NULL) {
        printf("[CAMTEST] no free display fb\n");
        return OPRT_COM_ERROR;
    }

    printf("[CAMTEST] convert begin fb=%p len=%u\n", fb->frame, fb->len);
    tdl_disp_convert_yuv422_to_fb(sg_camtest_buf, EXAMPLE_CAMERA_WIDTH, EXAMPLE_CAMERA_HEIGHT, fb,
                                  sg_display_info.is_swap, EXAMPLE_CAMERA_IMG_ROTATION, sg_yuv_order);
    printf("[CAMTEST] convert done, flush\n");
    tdl_disp_dev_flush(sg_tdl_disp_hdl, fb);
    printf("[CAMTEST] flush done\n");

    return OPRT_OK;
}
#endif

static OPERATE_RET __camera_init(void)
{
    OPERATE_RET      rt = OPRT_OK;
    TDL_CAMERA_CFG_T cfg;

    memset(&cfg, 0, sizeof(TDL_CAMERA_CFG_T));

    sg_tdl_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    if (NULL == sg_tdl_camera_hdl) {
        PR_ERR("camera dev %s not found", CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    /* Get YUV byte order from camera driver info */
    TDL_CAMERA_DEV_INFO_T cam_info = {0};
    tdl_camera_dev_get_info(sg_tdl_camera_hdl, &cam_info);
    /* The vendor logs GC2145 as YUYV while the TDD reports UYVY; the image on
     * the panel is colour-swapped with the TDD default, so pin YUYV here. */
    sg_yuv_order = TUYA_YUV422_YUYV;
    printf("[DISP_WORKER] cam_info.yuv_order=%d, using YUYV\n", cam_info.yuv_order);

    cfg.fps     = EXAMPLE_CAMERA_FPS;
    cfg.width   = EXAMPLE_CAMERA_WIDTH;
    cfg.height  = EXAMPLE_CAMERA_HEIGHT;
    cfg.out_fmt = TDL_CAMERA_FMT_YUV422;

    cfg.get_frame_cb = __get_camera_raw_frame_cb;

#if EXAMPLE_DISPLAY_WORKER
    sg_yuv_size    = (uint32_t)EXAMPLE_CAMERA_WIDTH * EXAMPLE_CAMERA_HEIGHT * 2u;
    sg_yuv_pending = tkl_system_malloc(sg_yuv_size);
    if (sg_yuv_pending == NULL) {
        PR_ERR("no memory for the %u-byte display frame", (unsigned)sg_yuv_size);
        return OPRT_MALLOC_FAILED;
    }
    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&sg_frame_sem, 0, 1));

    THREAD_CFG_T worker_cfg = {0};
    worker_cfg.stackDepth   = 8192;
    worker_cfg.priority     = THREAD_PRIO_3;
    worker_cfg.thrdname     = "camera_disp";
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_worker_hdl, NULL, NULL, __display_worker, NULL, &worker_cfg));
#endif

    TUYA_CALL_ERR_RETURN(tdl_camera_dev_open(sg_tdl_camera_hdl, &cfg));

    PR_NOTICE("camera init success");

    return OPRT_OK;
}

/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    printf("[PREVIEW] init\n");

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    /*hardware register*/
    board_register_hardware();

    TUYA_CALL_ERR_LOG(__display_init());

#if COLORBAR_TEST
    __colorbar_test();
#endif

#if EXAMPLE_PAGE_TEST
    TUYA_CALL_ERR_LOG(__page_test_run());
#elif EXAMPLE_PREVIEW_LOOP
    TUYA_CALL_ERR_LOG(__preview_run());
#elif EXAMPLE_CAMTEST
    TUYA_CALL_ERR_LOG(__camtest_run());
#elif !EXAMPLE_DISPLAY_ONLY
    TUYA_CALL_ERR_LOG(__camera_init());
#else
    printf("[disp-dbg] display-only build, camera not opened\n");
#endif

    while (1) {
        tal_system_sleep(1000);
    }
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    (void)arg;

    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
