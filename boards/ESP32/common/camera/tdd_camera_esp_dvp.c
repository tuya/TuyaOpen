/**
 * @file tdd_camera_esp_dvp.c
 * @brief ESP32/ESP32-S3 DVP camera TDD driver.
 *
 * Uses the espressif/esp32-camera IDF component (esp_camera_init / fb_get /
 * fb_return) and bridges it to the TDL camera layer via a dedicated capture
 * task.
 *
 * Supported output formats:
 *   - TDL_CAMERA_FMT_JPEG      : OV2640 hardware JPEG (on-sensor compression)
 *   - TDL_CAMERA_FMT_YUV422    : raw packed YUV422 (YUYV byte order)
 *   - TDL_CAMERA_FMT_JPEG_YUV422_BOTH : JPEG only (sensor can't do both at once)
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

/* This file is only meaningful on targets that have the LCD_CAM peripheral
 * with a camera input (ESP32 / ESP32-S3).  Guard the whole compilation unit
 * so it compiles silently to nothing on other targets (e.g. ESP32-P4 which
 * uses MIPI-CSI instead). */

#include <string.h>

#include "tuya_cloud_types.h"
#include "tuya_error_code.h"
#include "tal_memory.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_thread.h"

#include "tdl_camera_driver.h"
#include "tdd_camera_esp_dvp.h"

#include "esp_camera.h"
#include "driver/ledc.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define DVP_CAP_TASK_STACK       (8192)
#define DVP_FB_COUNT             (2)
#define DVP_JPEG_QUALITY_DEFAULT (12) /* OV2640 JPEG quality 0(best)~63(worst) */

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char                     name[CAMERA_DEV_NAME_MAX_LEN + 1];
    TDD_CAMERA_ESP_DVP_CFG_T cfg;

    volatile bool running;
    THREAD_HANDLE thread;
    bool          inited;

    bool need_raw;
    bool need_encoded;

    uint32_t width;
    uint32_t height;
    uint32_t frame_id;
} DVP_CAMERA_T;

/***********************************************************
***********************variable define**********************
***********************************************************/

/* Lookup table: (width, height) -> esp32-camera framesize_t enum value.
 * Listed from small to large so the first match wins. */
typedef struct {
    uint32_t    w;
    uint32_t    h;
    framesize_t fs;
} fsz_entry_t;

static const fsz_entry_t sg_fsz_tbl[] = {
    {96, 96, FRAMESIZE_96X96},  {160, 120, FRAMESIZE_QQVGA},  {240, 240, FRAMESIZE_240X240},
    {320, 240, FRAMESIZE_QVGA}, {400, 296, FRAMESIZE_CIF},    {480, 320, FRAMESIZE_HVGA},
    {640, 480, FRAMESIZE_VGA},  {800, 600, FRAMESIZE_SVGA},   {1024, 768, FRAMESIZE_XGA},
    {1280, 720, FRAMESIZE_HD},  {1280, 1024, FRAMESIZE_SXGA}, {1600, 1200, FRAMESIZE_UXGA},
};

/***********************************************************
***********************function define**********************
***********************************************************/

static framesize_t __find_framesize(uint32_t w, uint32_t h)
{
    for (size_t i = 0; i < sizeof(sg_fsz_tbl) / sizeof(sg_fsz_tbl[0]); i++) {
        if (sg_fsz_tbl[i].w == w && sg_fsz_tbl[i].h == h) {
            return sg_fsz_tbl[i].fs;
        }
    }
    PR_WARN("DVP: unsupported resolution %ux%u, falling back to QVGA (320x240)", w, h);
    return FRAMESIZE_QVGA;
}

static void __post_raw_frame(DVP_CAMERA_T *dev, camera_fb_t *fb)
{
    TDD_CAMERA_FRAME_T *f = tdl_camera_create_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, TUYA_FRAME_FMT_YUV422);
    if (NULL == f) {
        return;
    }

    uint32_t copy_len = (uint32_t)fb->len;
    if (copy_len > f->frame.data_len) {
        copy_len = f->frame.data_len;
    }
    memcpy(f->frame.data, fb->buf, copy_len);

    f->frame.id              = (uint16_t)(dev->frame_id++);
    f->frame.is_i_frame      = 1;
    f->frame.is_complete     = 1;
    f->frame.fmt             = TUYA_FRAME_FMT_YUV422;
    f->frame.width           = (uint16_t)fb->width;
    f->frame.height          = (uint16_t)fb->height;
    f->frame.data_len        = copy_len;
    f->frame.total_frame_len = copy_len;

    if (tdl_camera_post_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, f) != OPRT_OK) {
        tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, f);
    }
}

static void __post_jpeg_frame(DVP_CAMERA_T *dev, camera_fb_t *fb)
{
    TDD_CAMERA_FRAME_T *f = tdl_camera_create_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, TUYA_FRAME_FMT_JPEG);
    if (NULL == f) {
        return;
    }

    uint32_t copy_len = (uint32_t)fb->len;
    if (copy_len > f->frame.data_len) {
        copy_len = f->frame.data_len;
    }
    memcpy(f->frame.data, fb->buf, copy_len);

    f->frame.id              = (uint16_t)(dev->frame_id);
    f->frame.is_i_frame      = 1;
    f->frame.is_complete     = 1;
    f->frame.fmt             = TUYA_FRAME_FMT_JPEG;
    f->frame.width           = (uint16_t)fb->width;
    f->frame.height          = (uint16_t)fb->height;
    f->frame.data_len        = copy_len;
    f->frame.total_frame_len = copy_len;

    if (tdl_camera_post_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, f) != OPRT_OK) {
        tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, f);
    }
}

static void __dvp_capture_task(void *args)
{
    DVP_CAMERA_T *dev = (DVP_CAMERA_T *)args;

    PR_NOTICE("DVP capture task started");

    while (dev->running) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (NULL == fb) {
            tal_system_sleep(10);
            continue;
        }

        if (dev->need_raw && fb->format == PIXFORMAT_YUV422) {
            __post_raw_frame(dev, fb);
        }

        if (dev->need_encoded && fb->format == PIXFORMAT_JPEG) {
            __post_jpeg_frame(dev, fb);
        }

        esp_camera_fb_return(fb);
    }

    PR_NOTICE("DVP capture task exiting");
    tal_thread_delete(NULL);
}

static OPERATE_RET __tdd_dvp_open(TDD_CAMERA_DEV_HANDLE_T device, TDD_CAMERA_OPEN_CFG_T *cfg)
{
    DVP_CAMERA_T *dev = (DVP_CAMERA_T *)device;

    if (NULL == dev || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }
    if (dev->running) {
        return OPRT_OK;
    }

    if (cfg->out_fmt == TDL_CAMERA_FMT_H264 || cfg->out_fmt == TDL_CAMERA_FMT_H264_YUV422_BOTH) {
        PR_ERR("DVP camera: H264 output is not supported");
        return OPRT_NOT_SUPPORTED;
    }

    dev->need_raw     = (cfg->out_fmt & TDL_IMG_FMT_RAW_MASK) ? true : false;
    dev->need_encoded = (cfg->out_fmt & TDL_IMG_FMT_ENCODED_MASK) ? true : false;

    /* OV2640 can output JPEG or YUV422, but not both simultaneously. */
    pixformat_t pf;
    if (dev->need_encoded) {
        pf = PIXFORMAT_JPEG;
        if (dev->need_raw) {
            PR_WARN("DVP: JPEG+RAW simultaneously not supported; using JPEG only");
            dev->need_raw = false;
        }
    } else {
        pf = PIXFORMAT_YUV422;
    }

    int jpeg_quality = DVP_JPEG_QUALITY_DEFAULT;

    uint32_t    req_w      = cfg->width ? (uint32_t)cfg->width : 320;
    uint32_t    req_h      = cfg->height ? (uint32_t)cfg->height : 240;
    framesize_t frame_size = __find_framesize(req_w, req_h);

    const TDD_CAMERA_ESP_DVP_CFG_T *hw = &dev->cfg;

    camera_config_t cam_cfg = {
        .pin_pwdn      = hw->pin_pwdn,
        .pin_reset     = hw->pin_reset,
        .pin_xclk      = hw->pin_xclk,
        .pin_sccb_sda  = hw->pin_sccb_sda,
        .pin_sccb_scl  = hw->pin_sccb_scl,
        .pin_d7        = hw->pin_d7,
        .pin_d6        = hw->pin_d6,
        .pin_d5        = hw->pin_d5,
        .pin_d4        = hw->pin_d4,
        .pin_d3        = hw->pin_d3,
        .pin_d2        = hw->pin_d2,
        .pin_d1        = hw->pin_d1,
        .pin_d0        = hw->pin_d0,
        .pin_vsync     = hw->pin_vsync,
        .pin_href      = hw->pin_href,
        .pin_pclk      = hw->pin_pclk,
        .xclk_freq_hz  = hw->xclk_freq_hz > 0 ? hw->xclk_freq_hz : 20000000,
        .ledc_timer    = LEDC_TIMER_0,
        .ledc_channel  = LEDC_CHANNEL_0,
        .pixel_format  = pf,
        .frame_size    = frame_size,
        .jpeg_quality  = jpeg_quality,
        .fb_count      = DVP_FB_COUNT,
        .fb_location   = CAMERA_FB_IN_PSRAM,
        .grab_mode     = CAMERA_GRAB_WHEN_EMPTY,
        .sccb_i2c_port = hw->sccb_i2c_port,
    };

    esp_err_t err = esp_camera_init(&cam_cfg);
    if (err != ESP_OK) {
        PR_ERR("esp_camera_init failed: 0x%x", (unsigned)err);
        return OPRT_COM_ERROR;
    }

    dev->inited = true;
    dev->width  = req_w;
    dev->height = req_h;

    dev->running    = true;
    THREAD_CFG_T th = {.stackDepth = DVP_CAP_TASK_STACK, .priority = THREAD_PRIO_2, .thrdname = "dvp_cam"};
    OPERATE_RET  rt = tal_thread_create_and_start(&dev->thread, NULL, NULL, __dvp_capture_task, dev, &th);
    if (rt != OPRT_OK) {
        dev->running = false;
        esp_camera_deinit();
        dev->inited = false;
        return rt;
    }

    PR_NOTICE("DVP camera opened: %ux%u raw=%d enc=%d pixfmt=%d", dev->width, dev->height, dev->need_raw,
              dev->need_encoded, (int)pf);
    return OPRT_OK;
}

static OPERATE_RET __tdd_dvp_close(TDD_CAMERA_DEV_HANDLE_T device)
{
    DVP_CAMERA_T *dev = (DVP_CAMERA_T *)device;

    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }
    if (!dev->running) {
        return OPRT_OK;
    }

    dev->running = false;

    if (dev->thread) {
        tal_thread_delete(dev->thread);
        dev->thread = NULL;
    }
    /* Give the capture task time to exit esp_camera_fb_get() */
    tal_system_sleep(100);

    if (dev->inited) {
        esp_camera_deinit();
        dev->inited = false;
    }

    PR_NOTICE("DVP camera closed");
    return OPRT_OK;
}

OPERATE_RET tdd_camera_esp_dvp_register(const char *name, const TDD_CAMERA_ESP_DVP_CFG_T *cfg)
{
    if (NULL == name || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    DVP_CAMERA_T *dev = (DVP_CAMERA_T *)tal_malloc(sizeof(DVP_CAMERA_T));
    if (NULL == dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dev, 0, sizeof(*dev));
    dev->cfg = *cfg;
    strncpy(dev->name, name, CAMERA_DEV_NAME_MAX_LEN);

    TDD_CAMERA_DEV_INFO_T dev_info = {
        .type       = TDL_CAMERA_DVP,
        .max_fps    = 30,
        .max_width  = 1600,
        .max_height = 1200,
        /* OV2640 over DVP: raw YUV422 or on-sensor hardware JPEG. No H264 encoder. */
        .supported_fmts = TDL_CAMERA_FMT_YUV422 | TDL_CAMERA_FMT_JPEG,
        .yuv_order  = TUYA_YUV422_YUYV,  /* ESP32-S3 LCD_CAM outputs YUYV */
    };
    TDD_CAMERA_INTFS_T intfs = {
        .open  = __tdd_dvp_open,
        .close = __tdd_dvp_close,
    };

    OPERATE_RET rt = tdl_camera_device_register((char *)name, (TDD_CAMERA_DEV_HANDLE_T)dev, &intfs, &dev_info);
    if (rt != OPRT_OK) {
        tal_free(dev);
        return rt;
    }

    PR_NOTICE("registered DVP camera: %s (SCCB SCL=%d SDA=%d)", name, cfg->pin_sccb_scl, cfg->pin_sccb_sda);
    return OPRT_OK;
}
