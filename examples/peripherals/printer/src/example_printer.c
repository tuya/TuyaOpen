/**
 * @file example_printer.c
 * @brief General-purpose thermal printer example for TuyaOpen
 * @version 0.5
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 * Demonstrates the TDL printer API against any registered printer driver:
 *   - MTP02-DXD  (RAW protocol, no text support)
 *   - DP48A      (ESC/POS protocol, UTF-8 → GBK text + bitmap)
 *
 * The example runs two back-to-back demos:
 *   1. Text demo  — prints a header line; skipped with a notice on RAW drivers.
 *   2. Image demo — decodes the built-in JPEG and prints it as a 1-bit bitmap,
 *                   scaling down automatically to fit the printer's paper width.
 *
 * The printer driver is registered by board_register_hardware() using the name
 * defined by CONFIG_PRINTER_NAME.  No TDD-specific header is needed here.
 */

#include "tal_api.h"
#include "tkl_output.h"

#include "tdl_printer_manage.h"
#include "tal_image_jpeg_codec.h"
#include "board_com_api.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * Built-in test image (defined in test_flower_jpeg.c)
 * --------------------------------------------------------------------------- */
extern const uint8_t  test_flower_jpeg[];
extern const uint32_t TEST_FLOWER_JPEG_SIZE;

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static TDL_PRINTER_HANDLE sg_printer_hdl;

/* ---------------------------------------------------------------------------
 * Printer event callback
 * --------------------------------------------------------------------------- */
static void __printer_event_cb(TDL_PRINTER_HANDLE handle, TDL_PRINTER_EVENT_E event,
                                void *data, void *arg)
{
    (void)handle; (void)data; (void)arg;
    switch (event) {
    case TDL_PRINTER_EVENT_PAPER_OUT:   PR_WARN("EVENT: paper out");         break;
    case TDL_PRINTER_EVENT_PAPER_IN:    PR_NOTICE("EVENT: paper loaded");    break;
    case TDL_PRINTER_EVENT_OVERHEATED:  PR_WARN("EVENT: head overheated");   break;
    case TDL_PRINTER_EVENT_TEMP_NORMAL: PR_NOTICE("EVENT: temperature normal"); break;
    case TDL_PRINTER_EVENT_ERROR:       PR_ERR("EVENT: printer error");      break;
    default: break;
    }
}

/* ---------------------------------------------------------------------------
 * Demo 1: text printing
 *
 * tdl_printer_send_text() returns OPRT_NOT_SUPPORTED on RAW-protocol drivers
 * (e.g. MTP02).  We treat that as "not available" and continue.
 * --------------------------------------------------------------------------- */
static OPERATE_RET __demo_text(void)
{
    OPERATE_RET rt;

    rt = tdl_printer_send_text(sg_printer_hdl, "TuyaOpen Printer Example\n");
    if (rt == OPRT_NOT_SUPPORTED) {
        PR_NOTICE("text printing not supported by this driver (RAW protocol) — skipping");
        return OPRT_OK;
    }
    if (rt != OPRT_OK) {
        PR_ERR("send_text failed: %d", rt);
        return rt;
    }

    /* Chinese text: auto-converted UTF-8 → GBK on ESC/POS drivers */
    rt = tdl_printer_send_text(sg_printer_hdl, "涂鸦智能 打印测试\n");
    if (rt != OPRT_OK) {
        PR_ERR("send_text (Chinese) failed: %d", rt);
        return rt;
    }

    PR_NOTICE("text demo OK");
    return OPRT_OK;
}

/* ---------------------------------------------------------------------------
 * Demo 2: JPEG → 1-bit bitmap
 *
 * The image is scaled down to the printer's paper width so that it fits
 * regardless of original resolution.  For printers that report dots_per_line=0
 * (should not happen for well-configured drivers) the image is printed at its
 * original width.
 *
 * tdl_printer_start() / tdl_printer_end() are called around the bitmap send.
 * They return OPRT_NOT_SUPPORTED on ESC/POS drivers — that is expected and
 * harmless (the ESC/POS driver handles framing internally).
 * --------------------------------------------------------------------------- */
static OPERATE_RET __demo_image(uint32_t print_width)
{
    OPERATE_RET rt;

    /* Read JPEG dimensions */
    TAL_IMAGE_JPEG_INFO_T info = {0};
    rt = tal_image_jpeg_get_info(test_flower_jpeg, TEST_FLOWER_JPEG_SIZE, &info);
    if (rt != OPRT_OK) {
        PR_ERR("jpeg_get_info failed: %d", rt);
        return rt;
    }
    PR_NOTICE("JPEG source: %ux%u", info.width, info.height);

    /* Scale to fit printer width */
    uint16_t out_w, out_h;
    if (print_width > 0 && info.width > (uint16_t)print_width) {
        out_w = (uint16_t)print_width;
        out_h = (uint16_t)((uint32_t)info.height * print_width / info.width);
    } else {
        out_w = info.width;
        out_h = info.height;
    }
    if (out_h == 0) out_h = 1;
    PR_NOTICE("output size: %ux%u", out_w, out_h);

    /* Allocate 1-bit bitmap buffer */
    uint32_t bmp_size = ((uint32_t)(out_w + 7u) / 8u) * out_h;
    uint8_t *bmp_buf = (uint8_t *)tal_malloc(bmp_size);
    if (bmp_buf == NULL) {
        PR_ERR("malloc %u bytes for bitmap failed", bmp_size);
        return OPRT_MALLOC_FAILED;
    }

    /* Decode JPEG → gray (with nearest-neighbor downscale) → Floyd-Steinberg 1-bit */
    TAL_IMAGE_JPEG_OUTPUT_T dec = {
        .out_buf      = bmp_buf,
        .out_buf_size = bmp_size,
        .out_width    = out_w,
        .out_height   = out_h,
    };
    PR_NOTICE("decoding + dithering ...");
    rt = tal_image_jpeg_decode_bitmap(test_flower_jpeg, TEST_FLOWER_JPEG_SIZE, &dec, 128);
    if (rt != OPRT_OK) {
        PR_ERR("jpeg_decode_bitmap failed: %d", rt);
        tal_free(bmp_buf);
        return rt;
    }

    /* start() is required by RAW drivers; ESC/POS drivers return NOT_SUPPORTED — ignore */
    tdl_printer_start(sg_printer_hdl);

    PR_NOTICE("printing bitmap ...");
    rt = tdl_printer_send_bitmap(sg_printer_hdl, 0, out_w, out_h, bmp_buf);
    tal_free(bmp_buf);

    /* end() mirrors start() */
    tdl_printer_end(sg_printer_hdl);

    if (rt != OPRT_OK) {
        PR_ERR("send_bitmap failed: %d", rt);
        return rt;
    }

    PR_NOTICE("image demo OK");
    return OPRT_OK;
}

/* ---------------------------------------------------------------------------
 * Main demo task
 * --------------------------------------------------------------------------- */
static void __example_printer_task(void)
{
    OPERATE_RET rt;

    /* Find printer registered by board_register_hardware() */
    rt = tdl_printer_find(PRINTER_NAME, &sg_printer_hdl);
    if (rt != OPRT_OK) {
        PR_ERR("printer \"%s\" not found (rt=%d). "
               "Check CONFIG_PRINTER_NAME and board registration.", PRINTER_NAME, rt);
        return;
    }

    /* Open with status event monitoring */
    TDL_PRINTER_OPEN_PARAM_T open_param = {
        .event_cb         = __printer_event_cb,
        .event_cb_arg     = NULL,
        .poll_interval_ms = 0,      /* use Kconfig default */
    };
    rt = tdl_printer_open(sg_printer_hdl, &open_param);
    if (rt != OPRT_OK) {
        PR_ERR("printer open failed: %d", rt);
        return;
    }

    /* Query hardware geometry — used to scale the image */
    TDL_PRINTER_DEV_INFO_T dev_info = {0};
    tdl_printer_get_dev_info(sg_printer_hdl, &dev_info);
    PR_NOTICE("printer ready: dots_per_line=%u  bytes_per_line=%u",
              dev_info.dots_per_line, dev_info.bytes_per_line);

    /* --- Demo 1: text --- */
    PR_NOTICE("=== text demo ===");
    rt = __demo_text();
    if (rt != OPRT_OK) {
        PR_ERR("text demo failed: %d", rt);
    }

    /* --- Demo 2: image --- */
    PR_NOTICE("=== image demo ===");
    rt = __demo_image(dev_info.dots_per_line);
    if (rt != OPRT_OK) {
        PR_ERR("image demo failed: %d", rt);
    }

    /* Feed paper to the tear position */
    tdl_printer_paper_feed(sg_printer_hdl, 40);

    tdl_printer_close(sg_printer_hdl);
    PR_NOTICE("printer closed — example done");
}

/* ---------------------------------------------------------------------------
 * Application entry points
 * --------------------------------------------------------------------------- */
void user_main(void)
{
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Project:     %s  v%s", PROJECT_NAME, PROJECT_VERSION);
    PR_NOTICE("Build:       %s", __DATE__);
    PR_NOTICE("TuyaOpen:    %s  (%s)", OPEN_VERSION, OPEN_COMMIT);
    PR_NOTICE("Board:       %s  (%s)", PLATFORM_BOARD, PLATFORM_COMMIT);

    board_register_hardware();

    __example_printer_task();

    while (1) {
        tal_system_sleep(1000);
    }
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();
    while (1) { tal_system_sleep(500); }
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
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 6;   /* image decode needs extra stack */
    thrd_param.priority   = THREAD_PRIO_1;
    thrd_param.thrdname   = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
