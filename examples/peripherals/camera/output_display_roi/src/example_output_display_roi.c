/**
 * @file example_output_display_roi.c
 * @brief Low-memory camera-to-display preview using ROI bands.
 *
 * A YUV422 camera frame is converted one source band at a time, nearest-neighbor
 * scaled into a small RGB565 output band, and flushed directly to a display ROI.
 * The example does not allocate a full-screen framebuffer.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "board_com_api.h"
#include "tdl_camera_manage.h"
#include "tdl_display_manage.h"

#define CAMERA_WIDTH                EXAMPLE_CAMERA_WIDTH
#define CAMERA_HEIGHT               EXAMPLE_CAMERA_HEIGHT
#define OUTPUT_X                    EXAMPLE_CAMERA_OUTPUT_X
#define OUTPUT_Y                    EXAMPLE_CAMERA_OUTPUT_Y
#define OUTPUT_WIDTH                EXAMPLE_CAMERA_OUTPUT_WIDTH
#define OUTPUT_HEIGHT               EXAMPLE_CAMERA_OUTPUT_HEIGHT
#define OUTPUT_BAND_LINES           EXAMPLE_CAMERA_BAND_LINES
#define SOURCE_BAND_LINES_MAX \
    ((OUTPUT_BAND_LINES * CAMERA_HEIGHT + OUTPUT_HEIGHT - 1U) / OUTPUT_HEIGHT)

static TDL_DISP_HANDLE_T sg_display;
static TDL_DISP_DEV_INFO_T sg_display_info;
static TDL_CAMERA_HANDLE_T sg_camera;
static TUYA_YUV422_ORDER_E sg_yuv_order = TUYA_YUV422_UYVY;
static uint32_t sg_frame_count;

static uint8_t sg_source_band[CAMERA_WIDTH * SOURCE_BAND_LINES_MAX * 2U]
    __attribute__((aligned(32)));
static uint8_t sg_output_band[OUTPUT_WIDTH * OUTPUT_BAND_LINES * 2U]
    __attribute__((aligned(32)));

static void __display_clear_black(void)
{
    uint32_t bytes_per_line = sg_display_info.width * 2U;
    uint32_t lines = sizeof(sg_output_band) / bytes_per_line;

    if (lines == 0U) {
        PR_WARN("output band is smaller than one display line");
        return;
    }

    memset(sg_output_band, 0, sizeof(sg_output_band));

    for (uint32_t y = 0U; y < sg_display_info.height; y += lines) {
        uint32_t current_lines = sg_display_info.height - y;
        TDL_DISP_FRAME_BUFF_T fb = {
            .type = DISP_FB_TP_SRAM,
            .fmt = TUYA_PIXEL_FMT_RGB565,
            .x_start = 0U,
            .y_start = (uint16_t)y,
            .width = sg_display_info.width,
            .height = (uint16_t)(current_lines > lines ? lines : current_lines),
            .len = bytes_per_line * (current_lines > lines ? lines : current_lines),
            .frame = sg_output_band,
        };

        if (tdl_disp_dev_flush(sg_display, &fb) != OPRT_OK) {
            return;
        }
    }
}

static void __scale_band(const uint16_t *source, uint16_t *output,
                         uint32_t source_y, uint32_t output_y,
                         uint32_t output_lines)
{
    for (uint32_t row = 0U; row < output_lines; ++row) {
        uint32_t source_row = ((output_y + row) * CAMERA_HEIGHT) / OUTPUT_HEIGHT;
        source_row -= source_y;

        for (uint32_t col = 0U; col < OUTPUT_WIDTH; ++col) {
            uint32_t source_col = (col * CAMERA_WIDTH) / OUTPUT_WIDTH;
            output[row * OUTPUT_WIDTH + col] =
                source[source_row * CAMERA_WIDTH + source_col];
        }
    }
}

static OPERATE_RET __camera_frame_cb(TDL_CAMERA_HANDLE_T camera,
                                     TDL_CAMERA_FRAME_T *frame)
{
    uint32_t output_y = 0U;
    uint8_t y_min = 0xFFU;
    uint8_t y_max = 0U;
    uint32_t y_sum = 0U;
    uint32_t y_count = 0U;
    uint32_t frame_start_ms;
    uint32_t frame_interval_ms = 0U;
    static uint32_t last_frame_ms;

    (void)camera;

    if (frame == NULL || frame->data == NULL ||
        frame->width != CAMERA_WIDTH || frame->height != CAMERA_HEIGHT ||
        frame->data_len < (uint32_t)CAMERA_WIDTH * CAMERA_HEIGHT * 2U) {
        return OPRT_INVALID_PARM;
    }

    if (!frame->is_complete) {
        return OPRT_OK;
    }

    frame_start_ms = (uint32_t)tal_system_get_millisecond();
    if (last_frame_ms != 0U) {
        frame_interval_ms = frame_start_ms - last_frame_ms;
    }
    last_frame_ms = frame_start_ms;

    ++sg_frame_count;
    if (sg_frame_count <= 3U) {
        for (uint32_t y = 0U; y < CAMERA_HEIGHT; ++y) {
            const uint8_t *row = frame->data + y * CAMERA_WIDTH * 2U;
            for (uint32_t x = 0U; x < CAMERA_WIDTH; x += 2U) {
                uint8_t luma = row[x * 2U];
                y_min = luma < y_min ? luma : y_min;
                y_max = luma > y_max ? luma : y_max;
                y_sum += luma;
                ++y_count;
            }
        }
        PR_NOTICE("frame %u raw Y min=%u max=%u mean=%u order=%d swap=%d",
                  (unsigned)sg_frame_count, y_min, y_max,
                  y_count ? (unsigned)(y_sum / y_count) : 0U,
                  sg_yuv_order, sg_display_info.is_swap);
    }

    while (output_y < OUTPUT_HEIGHT) {
        uint32_t output_lines = OUTPUT_HEIGHT - output_y;
        uint32_t source_y;
        uint32_t source_end;
        uint32_t source_lines;
        TDL_DISP_FRAME_BUFF_T source_fb;
        TDL_DISP_FRAME_BUFF_T output_fb;
        OPERATE_RET rt;

        if (output_lines > OUTPUT_BAND_LINES) {
            output_lines = OUTPUT_BAND_LINES;
        }

        source_y = (output_y * CAMERA_HEIGHT) / OUTPUT_HEIGHT;
        source_end = ((output_y + output_lines) * CAMERA_HEIGHT +
                      OUTPUT_HEIGHT - 1U) / OUTPUT_HEIGHT;
        source_lines = source_end - source_y;
        if (source_lines == 0U || source_lines > SOURCE_BAND_LINES_MAX) {
            return OPRT_INVALID_PARM;
        }

        source_fb = (TDL_DISP_FRAME_BUFF_T){
            .type = DISP_FB_TP_SRAM,
            .fmt = TUYA_PIXEL_FMT_RGB565,
            .width = CAMERA_WIDTH,
            .height = (uint16_t)source_lines,
            .len = CAMERA_WIDTH * source_lines * 2U,
            .frame = sg_source_band,
        };

        rt = tdl_disp_convert_yuv422_to_fb(
            frame->data + source_y * CAMERA_WIDTH * 2U,
            CAMERA_WIDTH, (uint16_t)source_lines, &source_fb,
            sg_display_info.is_swap, TUYA_DISPLAY_ROTATION_0, sg_yuv_order);
        if (rt != OPRT_OK) {
            return rt;
        }

        if (sg_frame_count <= 3U && output_y == 0U) {
            const uint16_t *pixels = (const uint16_t *)source_fb.frame;
            uint32_t rgb_min = 0xFFFFU;
            uint32_t rgb_max = 0U;
            uint32_t white = 0U;
            for (uint32_t i = 0U; i < CAMERA_WIDTH * source_lines; ++i) {
                uint16_t pixel = pixels[i];
                rgb_min = pixel < rgb_min ? pixel : rgb_min;
                rgb_max = pixel > rgb_max ? pixel : rgb_max;
                white += pixel == 0xFFFFU;
            }
            PR_NOTICE("frame %u RGB565 min=0x%04X max=0x%04X white=%u/%u",
                      (unsigned)sg_frame_count, (unsigned)rgb_min,
                      (unsigned)rgb_max, (unsigned)white,
                      (unsigned)(CAMERA_WIDTH * source_lines));
        }

        __scale_band((const uint16_t *)source_fb.frame,
                     (uint16_t *)sg_output_band,
                     source_y, output_y, output_lines);

        output_fb = (TDL_DISP_FRAME_BUFF_T){
            .type = DISP_FB_TP_SRAM,
            .fmt = TUYA_PIXEL_FMT_RGB565,
            .x_start = OUTPUT_X,
            .y_start = (uint16_t)(OUTPUT_Y + output_y),
            .width = OUTPUT_WIDTH,
            .height = (uint16_t)output_lines,
            .len = OUTPUT_WIDTH * output_lines * 2U,
            .frame = sg_output_band,
        };
        rt = tdl_disp_dev_flush(sg_display, &output_fb);
        if (rt != OPRT_OK) {
            PR_ERR("roi flush failed: %d", rt);
            return rt;
        }

        output_y += output_lines;
    }

    if (sg_frame_count <= 5U || (sg_frame_count % 30U) == 0U) {
        PR_NOTICE("frame %u interval=%ums callback=%ums",
                  (unsigned)sg_frame_count, (unsigned)frame_interval_ms,
                  (unsigned)((uint32_t)tal_system_get_millisecond() - frame_start_ms));
    }

    return OPRT_OK;
}

static OPERATE_RET __display_init(void)
{
    OPERATE_RET rt;

    sg_display = tdl_disp_find_dev(DISPLAY_NAME);
    if (sg_display == NULL) {
        PR_ERR("display %s not found", DISPLAY_NAME);
        return OPRT_NOT_FOUND;
    }

    rt = tdl_disp_dev_get_info(sg_display, &sg_display_info);
    if (rt != OPRT_OK) {
        return rt;
    }

    if (sg_display_info.fmt != TUYA_PIXEL_FMT_RGB565 ||
        OUTPUT_X + OUTPUT_WIDTH > sg_display_info.width ||
        OUTPUT_Y + OUTPUT_HEIGHT > sg_display_info.height) {
        PR_ERR("invalid display ROI %ux%u at (%u,%u) for %ux%u %d",
               OUTPUT_WIDTH, OUTPUT_HEIGHT, OUTPUT_X, OUTPUT_Y,
               sg_display_info.width, sg_display_info.height,
               sg_display_info.fmt);
        return OPRT_INVALID_PARM;
    }

    rt = tdl_disp_dev_open(sg_display);
    if (rt != OPRT_OK) {
        return rt;
    }

    (void)tdl_disp_set_brightness(sg_display, 100U);
    __display_clear_black();

    PR_NOTICE("display ROI %ux%u at (%u,%u), band %u lines",
              OUTPUT_WIDTH, OUTPUT_HEIGHT, OUTPUT_X, OUTPUT_Y,
              OUTPUT_BAND_LINES);
    return OPRT_OK;
}

static OPERATE_RET __camera_init(void)
{
    TDL_CAMERA_DEV_INFO_T info = {0};
    TDL_CAMERA_CFG_T cfg = {0};
    OPERATE_RET rt;

    sg_camera = tdl_camera_find_dev(EXAMPLE_CAMERA_NAME);
    if (sg_camera == NULL) {
        PR_ERR("camera %s not found", EXAMPLE_CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    rt = tdl_camera_dev_get_info(sg_camera, &info);
    if (rt != OPRT_OK) {
        return rt;
    }
    sg_yuv_order = info.yuv_order;

    cfg.fps = EXAMPLE_CAMERA_FPS;
    cfg.width = CAMERA_WIDTH;
    cfg.height = CAMERA_HEIGHT;
    cfg.out_fmt = TDL_CAMERA_FMT_YUV422;
    cfg.get_frame_cb = __camera_frame_cb;

    rt = tdl_camera_dev_open(sg_camera, &cfg);
    if (rt == OPRT_OK) {
        PR_NOTICE("camera %s opened at %ux%u @ %u fps",
                  EXAMPLE_CAMERA_NAME, CAMERA_WIDTH, CAMERA_HEIGHT, EXAMPLE_CAMERA_FPS);
    }

    return rt;
}

void user_main(void)
{
    OPERATE_RET rt;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    board_register_hardware();

    rt = __display_init();
    if (rt != OPRT_OK) {
        PR_ERR("display init failed: %d", rt);
        return;
    }

    rt = __camera_init();
    if (rt != OPRT_OK) {
        PR_ERR("camera init failed: %d", rt);
        return;
    }

    while (1) {
        tal_system_sleep(1000U);
    }
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    user_main();
    while (1) {
        tal_system_sleep(500U);
    }
}
#else
static THREAD_HANDLE sg_app_thread;

static void __app_thread(void *arg)
{
    (void)arg;

    user_main();
    tal_thread_delete(sg_app_thread);
    sg_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T cfg = {
        .stackDepth = 1024 * 4,
        .priority = THREAD_PRIO_1,
        .thrdname = "camera_roi",
    };

    tal_thread_create_and_start(&sg_app_thread, NULL, NULL, __app_thread,
                                NULL, &cfg);
}
#endif
