/**
 * @file tdd_camera_esp_csi.c
 * @brief MIPI-CSI camera driver (esp_video / V4L2 + P4 hardware JPEG) for ESP32-P4.
 *
 * Flow:
 *   OV5647 (RAW8) --CSI--> P4 ISP --> YUV422 --> /dev/video0 (V4L2)
 *      capture task: VIDIOC_DQBUF -> post YUV422 raw frame (preview)
 *                                 -> (throttled) hw-JPEG encode -> post JPEG frame (capture)
 *                                 -> VIDIOC_QBUF
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "tuya_cloud_types.h"
#include "tuya_error_code.h"
/* Include the specific TAL headers (not tal_api.h) to avoid pulling in
 * tal_security.h -> tkl_hash.h, which is not on this component's include path. */
#include "tal_memory.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_thread.h"

#include "tdl_camera_driver.h"
#include "tdd_camera_esp_csi.h"

#include "linux/videodev2.h"
#include "esp_video_init.h"
#include "driver/jpeg_encode.h"
#include "driver/i2c_master.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"  /* esp_cache_get_alignment() */

/* ESP-IDF (newlib) has no sys/mman.h, so the camera uses V4L2 USERPTR buffers
 * allocated in PSRAM (cache-line aligned) instead of MMAP. */

/***********************************************************
************************macro define************************
***********************************************************/
#define TAG "tdd_cam_csi"

#ifndef ESP_VIDEO_MIPI_CSI_DEVICE_NAME
#define ESP_VIDEO_MIPI_CSI_DEVICE_NAME "/dev/video0"
#endif

/* V4L2 capture pixel format. The ESP32-P4 ISP advertises this fourcc as
 * V4L2_PIX_FMT_YUV422P ("422P"), but the data is actually *packed* YUV422
 * (COLOR_PIXEL_YUV422 = 16bpp, Y per pixel + U/V per 2 pixels interleaved), NOT
 * planar. The two consumers:
 *   - hw JPEG: same COLOR_PIXEL_YUV422 convention as the ISP -> feed packed data
 *              straight through (no reorder; guarantees correct photo colors).
 *   - ai_ui preview: tal_image_*_yuv422_to_rgb565() reads packed UYVY. */
#define CSI_CAPTURE_PIXFMT      V4L2_PIX_FMT_YUV422P

/* ISP packed YUV422 byte order. Empirically the ESP32-P4 ISP outputs UYVY (the
 * captured photo is correct with a straight pass-through to the JPEG encoder, and
 * a YUYV->UYVY swap for preview produced luma/chroma-swapped green/purple output).
 * UYVY also matches what the ai_ui SW converter reads, so preview is a pass-through.
 * If preview colors ever look red<->blue swapped, set this to 1. */
#define CSI_ISP_YUV422_IS_YUYV  (0)

#define CSI_BUF_COUNT           (3)
#define CSI_JPEG_QUALITY        (80)
/* Encode JPEG at most every N captured frames. Photos are taken on demand and the
 * app waits up to 3s for one, so there is no need to burn the JPEG engine at full
 * preview frame-rate. */
#define CSI_JPEG_ENCODE_EVERY_N (3)

#define CSI_CAPTURE_TASK_STACK  (8192)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char                      name[CAMERA_DEV_NAME_MAX_LEN + 1];
    TDD_CAMERA_ESP_CSI_CFG_T  cfg;

    int                       fd;
    volatile bool             running;
    THREAD_HANDLE             thread;

    uint32_t                  width;   /* actual capture width  */
    uint32_t                  height;  /* actual capture height */
    uint32_t                  pixfmt;  /* actual V4L2 fourcc    */

    uint8_t                  *bufs[CSI_BUF_COUNT];
    size_t                    buf_size;
    uint8_t                   buf_count;
    size_t                    cache_align;

    bool                      need_raw;
    bool                      need_encoded;

    jpeg_encoder_handle_t     jpeg_enc;
    uint8_t                  *jpeg_in;     /* aligned YUV422 input  */
    size_t                    jpeg_in_cap;
    uint8_t                  *jpeg_out;    /* aligned JPEG output    */
    size_t                    jpeg_out_cap;

    uint32_t                  frame_id;
} CSI_CAMERA_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static bool sg_esp_video_inited = false;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __esp_video_init_once(const TDD_CAMERA_ESP_CSI_CFG_T *cfg)
{
    if (sg_esp_video_inited) {
        return OPRT_OK;
    }

    i2c_master_bus_handle_t i2c_bus = NULL;
    if (i2c_master_get_bus_handle(cfg->i2c_port, &i2c_bus) != ESP_OK || NULL == i2c_bus) {
        PR_ERR("get I2C bus[%d] handle failed (touch/audio must init the bus first)", cfg->i2c_port);
        return OPRT_COM_ERROR;
    }

    esp_video_init_csi_config_t csi_cfg[] = {
        {
            .sccb_config = {
                .init_sccb  = false,
                .i2c_handle = i2c_bus,
                .freq       = cfg->sccb_freq_hz ? cfg->sccb_freq_hz : 100000,
            },
            .reset_pin = cfg->reset_pin,
            .pwdn_pin  = cfg->pwdn_pin,
        },
    };
    esp_video_init_config_t init_cfg = {
        .csi = csi_cfg,
    };

    esp_err_t err = esp_video_init(&init_cfg);
    if (err != ESP_OK) {
        PR_ERR("esp_video_init failed: 0x%x", err);
        return OPRT_COM_ERROR;
    }

    sg_esp_video_inited = true;
    PR_NOTICE("esp_video_init ok (CSI sccb on I2C%d)", cfg->i2c_port);
    return OPRT_OK;
}

static void __log_supported_formats(int fd)
{
    struct v4l2_fmtdesc desc;
    for (int i = 0;; i++) {
        memset(&desc, 0, sizeof(desc));
        desc.index = i;
        desc.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(fd, VIDIOC_ENUM_FMT, &desc) != 0) {
            break;
        }
        uint32_t f = desc.pixelformat;
        PR_NOTICE("  CSI fmt[%d]: '%c%c%c%c' (%s)", i,
                  (char)(f & 0xff), (char)((f >> 8) & 0xff),
                  (char)((f >> 16) & 0xff), (char)((f >> 24) & 0xff), desc.description);
    }
}

static OPERATE_RET __csi_set_format(CSI_CAMERA_T *dev, uint16_t want_w, uint16_t want_h)
{
    struct v4l2_format fmt;

    __log_supported_formats(dev->fd);

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(dev->fd, VIDIOC_G_FMT, &fmt) != 0) {
        PR_ERR("VIDIOC_G_FMT failed: %d", errno);
        return OPRT_COM_ERROR;
    }
    PR_NOTICE("CSI native fmt: %ux%u fourcc=0x%08x", fmt.fmt.pix.width, fmt.fmt.pix.height,
              (unsigned)fmt.fmt.pix.pixelformat);

    /* Keep the sensor/ISP native resolution; only switch the pixel format to YUV422.
     * Build a FRESH v4l2_format with just type/width/height/pixelformat: reusing the
     * struct from G_FMT keeps RGB565's bytesperline/sizeimage, which are inconsistent
     * with planar 422P and make S_FMT fail with EINVAL. (Matches the esp_video demo.) */
    (void)want_w;
    (void)want_h;
    uint32_t native_w = fmt.fmt.pix.width;
    uint32_t native_h = fmt.fmt.pix.height;

    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = native_w;
    fmt.fmt.pix.height      = native_h;
    fmt.fmt.pix.pixelformat = CSI_CAPTURE_PIXFMT;
    if (ioctl(dev->fd, VIDIOC_S_FMT, &fmt) != 0) {
        PR_ERR("VIDIOC_S_FMT(422P) failed: %d", errno);
        return OPRT_COM_ERROR;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(dev->fd, VIDIOC_G_FMT, &fmt) != 0) {
        PR_ERR("VIDIOC_G_FMT(after set) failed: %d", errno);
        return OPRT_COM_ERROR;
    }

    dev->width  = fmt.fmt.pix.width;
    dev->height = fmt.fmt.pix.height;
    dev->pixfmt = fmt.fmt.pix.pixelformat;
    PR_NOTICE("CSI capture fmt: %ux%u fourcc=0x%08x", dev->width, dev->height, (unsigned)dev->pixfmt);

    return OPRT_OK;
}

static OPERATE_RET __csi_request_buffers(CSI_CAMERA_T *dev)
{
    struct v4l2_requestbuffers req;

    /* cache-line alignment for PSRAM so esp_cache_msync() can invalidate the
     * CSI-DMA-written frames before the CPU reads them. */
    dev->cache_align = 64;
    (void)esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &dev->cache_align);
    if (0 == dev->cache_align) {
        dev->cache_align = 64;
    }

    memset(&req, 0, sizeof(req));
    req.count  = CSI_BUF_COUNT;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;

    if (ioctl(dev->fd, VIDIOC_REQBUFS, &req) != 0) {
        PR_ERR("VIDIOC_REQBUFS(USERPTR) failed: %d", errno);
        return OPRT_COM_ERROR;
    }
    dev->buf_count = (req.count < CSI_BUF_COUNT) ? req.count : CSI_BUF_COUNT;

    for (int i = 0; i < dev->buf_count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_USERPTR;
        buf.index  = i;

        if (ioctl(dev->fd, VIDIOC_QUERYBUF, &buf) != 0) {
            PR_ERR("VIDIOC_QUERYBUF[%d] failed: %d", i, errno);
            return OPRT_COM_ERROR;
        }

        /* Buffer size: driver-reported length, fall back to YUV422 size; round
         * up to the cache line so msync covers the whole buffer. */
        size_t want = buf.length ? buf.length : ((size_t)dev->width * dev->height * 2);
        size_t size = (want + dev->cache_align - 1) & ~(dev->cache_align - 1);

        dev->bufs[i] = (uint8_t *)heap_caps_aligned_calloc(dev->cache_align, 1, size, MALLOC_CAP_SPIRAM);
        if (NULL == dev->bufs[i]) {
            PR_ERR("alloc USERPTR buf[%d] (%u bytes) failed", i, (unsigned)size);
            return OPRT_MALLOC_FAILED;
        }
        dev->buf_size = size;

        buf.m.userptr = (unsigned long)dev->bufs[i];
        buf.length    = size;
        if (ioctl(dev->fd, VIDIOC_QBUF, &buf) != 0) {
            PR_ERR("VIDIOC_QBUF[%d] failed: %d", i, errno);
            return OPRT_COM_ERROR;
        }
    }

    PR_NOTICE("CSI buffers: %d x %u bytes (USERPTR PSRAM, align=%u)",
              dev->buf_count, (unsigned)dev->buf_size, (unsigned)dev->cache_align);
    return OPRT_OK;
}

static OPERATE_RET __csi_jpeg_engine_init(CSI_CAMERA_T *dev)
{
    jpeg_encode_engine_cfg_t eng_cfg = {
        .intr_priority = 0,
        .timeout_ms    = 1000,
    };
    if (jpeg_new_encoder_engine(&eng_cfg, &dev->jpeg_enc) != ESP_OK) {
        PR_ERR("jpeg_new_encoder_engine failed");
        dev->jpeg_enc = NULL;
        return OPRT_COM_ERROR;
    }

    size_t in_need  = (size_t)dev->width * dev->height * 2; /* YUV422 packed */
    size_t out_need = (size_t)dev->width * dev->height;     /* generous JPEG ceiling */

    jpeg_encode_memory_alloc_cfg_t in_mem  = {.buffer_direction = JPEG_ENC_ALLOC_INPUT_BUFFER};
    jpeg_encode_memory_alloc_cfg_t out_mem = {.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER};

    dev->jpeg_in  = (uint8_t *)jpeg_alloc_encoder_mem(in_need, &in_mem, &dev->jpeg_in_cap);
    dev->jpeg_out = (uint8_t *)jpeg_alloc_encoder_mem(out_need, &out_mem, &dev->jpeg_out_cap);
    if (NULL == dev->jpeg_in || NULL == dev->jpeg_out) {
        PR_ERR("jpeg_alloc_encoder_mem failed (in=%p out=%p)", dev->jpeg_in, dev->jpeg_out);
        return OPRT_MALLOC_FAILED;
    }

    PR_NOTICE("JPEG engine ready (in=%u out=%u)", (unsigned)dev->jpeg_in_cap, (unsigned)dev->jpeg_out_cap);
    return OPRT_OK;
}

static void __csi_jpeg_engine_deinit(CSI_CAMERA_T *dev)
{
    if (dev->jpeg_enc) {
        jpeg_del_encoder_engine(dev->jpeg_enc);
        dev->jpeg_enc = NULL;
    }
    if (dev->jpeg_in) {
        free(dev->jpeg_in);
        dev->jpeg_in = NULL;
    }
    if (dev->jpeg_out) {
        free(dev->jpeg_out);
        dev->jpeg_out = NULL;
    }
    dev->jpeg_in_cap = dev->jpeg_out_cap = 0;
}

/* Copy packed YUV422, swapping every byte pair so the output is UYVY.
 * YUYV (Y0 U Y1 V) <-> UYVY (U Y0 V Y1) differ only by swapping each 16-bit unit. */
static void __yuyv_to_uyvy(const uint8_t *src, uint8_t *dst, uint32_t bytes)
{
    for (uint32_t i = 0; i + 1 < bytes; i += 2) {
        dst[i]     = src[i + 1];
        dst[i + 1] = src[i];
    }
}

static void __post_raw_frame(CSI_CAMERA_T *dev, const uint8_t *data, uint32_t len)
{
    uint32_t packed_len = dev->width * dev->height * 2;

    TDD_CAMERA_FRAME_T *f = tdl_camera_create_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, TUYA_FRAME_FMT_YUV422);
    if (NULL == f) {
        return;
    }
    if (len < packed_len || packed_len > f->frame.data_len) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            PR_WARN("raw frame size mismatch: in=%u need=%u buf=%u", len, packed_len, f->frame.data_len);
        }
        tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, f);
        return;
    }

    /* ai_ui preview converter expects packed UYVY */
#if CSI_ISP_YUV422_IS_YUYV
    __yuyv_to_uyvy(data, f->frame.data, packed_len);
#else
    memcpy(f->frame.data, data, packed_len);
#endif

    f->frame.id              = (uint16_t)(dev->frame_id++);
    f->frame.is_i_frame      = 1;
    f->frame.is_complete     = 1;
    f->frame.fmt             = TUYA_FRAME_FMT_YUV422;
    f->frame.width           = dev->width;
    f->frame.height          = dev->height;
    f->frame.data_len        = packed_len;
    f->frame.total_frame_len = packed_len;

    if (tdl_camera_post_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, f) != OPRT_OK) {
        tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, f);
    }
}

static void __post_jpeg_frame(CSI_CAMERA_T *dev, const uint8_t *yuv, uint32_t yuv_len)
{
    uint32_t packed_len = dev->width * dev->height * 2;

    if (NULL == dev->jpeg_enc || yuv_len < packed_len || packed_len > dev->jpeg_in_cap) {
        return;
    }

    /* The ISP packed YUV422 already matches the hw JPEG COLOR_PIXEL_YUV422
     * convention, so copy it straight into the (DMA-aligned) encoder input. */
    memcpy(dev->jpeg_in, yuv, packed_len);

    jpeg_encode_cfg_t enc_cfg = {
        .width        = dev->width,
        .height       = dev->height,
        .src_type     = JPEG_ENCODE_IN_FORMAT_YUV422,
        .sub_sample   = JPEG_DOWN_SAMPLING_YUV422,
        .image_quality = CSI_JPEG_QUALITY,
    };

    uint32_t out_len = 0;
    esp_err_t err = jpeg_encoder_process(dev->jpeg_enc, &enc_cfg, dev->jpeg_in, packed_len,
                                         dev->jpeg_out, dev->jpeg_out_cap, &out_len);
    if (err != ESP_OK || 0 == out_len) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            PR_WARN("jpeg_encoder_process failed: 0x%x len=%u", err, out_len);
        }
        return;
    }

    TDD_CAMERA_FRAME_T *f = tdl_camera_create_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, TUYA_FRAME_FMT_JPEG);
    if (NULL == f) {
        return;
    }
    if (out_len > f->frame.data_len) {
        tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, f);
        return;
    }
    memcpy(f->frame.data, dev->jpeg_out, out_len);
    f->frame.id              = (uint16_t)(dev->frame_id);
    f->frame.is_i_frame      = 1;
    f->frame.is_complete     = 1;
    f->frame.fmt             = TUYA_FRAME_FMT_JPEG;
    f->frame.width           = dev->width;
    f->frame.height          = dev->height;
    f->frame.data_len        = out_len;
    f->frame.total_frame_len = out_len;

    if (tdl_camera_post_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, f) != OPRT_OK) {
        tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, f);
    }
}

static void __csi_capture_task(void *args)
{
    CSI_CAMERA_T *dev = (CSI_CAMERA_T *)args;
    uint32_t cnt = 0;
    uint32_t dqbuf_err = 0;
    bool     first_ok = false;

    while (dev->running) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_USERPTR;  /* must match REQBUFS memory type */

        if (ioctl(dev->fd, VIDIOC_DQBUF, &buf) != 0) {
            if ((dqbuf_err++ % 200) == 0) {
                PR_WARN("VIDIOC_DQBUF failed: errno=%d (cnt=%u)", errno, (unsigned)dqbuf_err);
            }
            tal_system_sleep(5);
            continue;
        }

        if (!first_ok) {
            first_ok = true;
            PR_NOTICE("CSI first frame: index=%u bytesused=%u", buf.index, (unsigned)buf.bytesused);
        }

        if (buf.index < dev->buf_count && dev->bufs[buf.index]) {
            uint8_t *data = dev->bufs[buf.index];
            uint32_t len  = buf.bytesused ? buf.bytesused : (uint32_t)dev->buf_size;

            /* CSI DMA wrote the frame to PSRAM behind the CPU cache; invalidate
             * the whole (cache-aligned) buffer before reading/encoding it. */
            (void)esp_cache_msync(data, dev->buf_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C);

            if (dev->need_raw) {
                __post_raw_frame(dev, data, len);
            }
            if (dev->need_encoded && (cnt % CSI_JPEG_ENCODE_EVERY_N) == 0) {
                __post_jpeg_frame(dev, data, len);
            }
        }

        /* USERPTR: restore the user pointer/length before re-queuing. */
        buf.m.userptr = (unsigned long)dev->bufs[buf.index];
        buf.length    = dev->buf_size;
        (void)ioctl(dev->fd, VIDIOC_QBUF, &buf);
        cnt++;
    }

    tal_thread_delete(NULL);
}

static void __csi_teardown(CSI_CAMERA_T *dev)
{
    if (dev->fd >= 0) {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        (void)ioctl(dev->fd, VIDIOC_STREAMOFF, &type);
    }
    for (int i = 0; i < dev->buf_count; i++) {
        if (dev->bufs[i]) {
            heap_caps_free(dev->bufs[i]);
            dev->bufs[i] = NULL;
        }
    }
    dev->buf_count = 0;
    dev->buf_size  = 0;
    __csi_jpeg_engine_deinit(dev);
    if (dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
    }
}

static OPERATE_RET __tdd_csi_open(TDD_CAMERA_DEV_HANDLE_T device, TDD_CAMERA_OPEN_CFG_T *cfg)
{
    CSI_CAMERA_T *dev = (CSI_CAMERA_T *)device;
    OPERATE_RET   rt  = OPRT_OK;
    int           type;

    if (NULL == dev || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }
    if (dev->running) {
        return OPRT_OK;
    }

    dev->need_raw     = (cfg->out_fmt & TDL_IMG_FMT_RAW_MASK) ? true : false;
    dev->need_encoded = (cfg->out_fmt & TDL_IMG_FMT_ENCODED_MASK) ? true : false;
    if (cfg->out_fmt == TDL_CAMERA_FMT_H264 || cfg->out_fmt == TDL_CAMERA_FMT_H264_YUV422_BOTH) {
        PR_ERR("CSI camera does not support H264");
        return OPRT_NOT_SUPPORTED;
    }

    TUYA_CALL_ERR_RETURN(__esp_video_init_once(&dev->cfg));

    dev->fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDWR);
    if (dev->fd < 0) {
        PR_ERR("open %s failed: %d", ESP_VIDEO_MIPI_CSI_DEVICE_NAME, errno);
        return OPRT_COM_ERROR;
    }

    rt = __csi_set_format(dev, cfg->width, cfg->height);
    if (rt != OPRT_OK) {
        goto err;
    }
    rt = __csi_request_buffers(dev);
    if (rt != OPRT_OK) {
        goto err;
    }
    if (dev->need_encoded) {
        rt = __csi_jpeg_engine_init(dev);
        if (rt != OPRT_OK) {
            goto err;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(dev->fd, VIDIOC_STREAMON, &type) != 0) {
        PR_ERR("VIDIOC_STREAMON failed: %d", errno);
        rt = OPRT_COM_ERROR;
        goto err;
    }

    dev->running = true;
    THREAD_CFG_T th = {CSI_CAPTURE_TASK_STACK, THREAD_PRIO_2, "csi_cam"};
    rt = tal_thread_create_and_start(&dev->thread, NULL, NULL, __csi_capture_task, dev, &th);
    if (rt != OPRT_OK) {
        dev->running = false;
        goto err;
    }

    PR_NOTICE("CSI camera opened: %ux%u raw=%d enc=%d", dev->width, dev->height,
              dev->need_raw, dev->need_encoded);
    return OPRT_OK;

err:
    __csi_teardown(dev);
    return rt;
}

static OPERATE_RET __tdd_csi_close(TDD_CAMERA_DEV_HANDLE_T device)
{
    CSI_CAMERA_T *dev = (CSI_CAMERA_T *)device;
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
    /* give the capture task a moment to exit its ioctl before tearing down */
    tal_system_sleep(50);

    __csi_teardown(dev);
    return OPRT_OK;
}

OPERATE_RET tdd_camera_esp_csi_register(const char *name, const TDD_CAMERA_ESP_CSI_CFG_T *cfg)
{
    if (NULL == name || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    CSI_CAMERA_T *dev = (CSI_CAMERA_T *)tal_malloc(sizeof(CSI_CAMERA_T));
    if (NULL == dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dev, 0, sizeof(*dev));
    dev->fd  = -1;
    dev->cfg = *cfg;
    strncpy(dev->name, name, CAMERA_DEV_NAME_MAX_LEN);

    TDD_CAMERA_DEV_INFO_T dev_info = {
        .type       = TDL_CAMERA_DVP,  /* generic; apps just look up by name */
        .max_fps    = 50,
        .max_width  = 1920,
        .max_height = 1280,
        .fmt        = TUYA_FRAME_FMT_YUV422,
    };
    TDD_CAMERA_INTFS_T intfs = {
        .open  = __tdd_csi_open,
        .close = __tdd_csi_close,
    };

    OPERATE_RET rt = tdl_camera_device_register((char *)name, (TDD_CAMERA_DEV_HANDLE_T)dev, &intfs, &dev_info);
    if (rt != OPRT_OK) {
        tal_free(dev);
        return rt;
    }

    PR_NOTICE("registered CSI camera: %s (I2C%d)", name, cfg->i2c_port);
    return OPRT_OK;
}
