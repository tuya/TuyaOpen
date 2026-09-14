/**
 * @file tdd_camera_esp_video_dvp.c
 * @brief ESP32-S31 DVP camera TDD driver using Espressif esp_video.
 *
 * The S31 DVP controller exposes the OV3660 stream through V4L2. This driver
 * copies captured YUV422 frames into the TDL camera frame pool and keeps the
 * V4L2 MMAP buffers queued for continuous capture.
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "tuya_cloud_types.h"
#include "tuya_error_code.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_system.h"
#include "tal_thread.h"

#include "tdl_camera_driver.h"
#include "tdd_camera_esp_video_dvp.h"

#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_cam_sensor_xclk.h"
#include "esp_heap_caps.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "linux/videodev2.h"

#define TAG "tdd_cam_dvp_video"

#define DVP_VIDEO_BUF_COUNT  (3)
#define DVP_VIDEO_TASK_STACK (8192)

typedef struct {
    void *addr;
    size_t len;
} DVP_VIDEO_BUF_T;

typedef struct {
    char name[CAMERA_DEV_NAME_MAX_LEN + 1];
    TDD_CAMERA_ESP_VIDEO_DVP_CFG_T cfg;
    int fd;
    int jpeg_fd;
    volatile bool running;
    THREAD_HANDLE thread;
    DVP_VIDEO_BUF_T bufs[DVP_VIDEO_BUF_COUNT];
    DVP_VIDEO_BUF_T jpeg_buf;
    uint8_t *jpeg_input;
    size_t jpeg_input_len;
    uint8_t buf_count;
    uint32_t width;
    uint32_t height;
    uint32_t pixfmt;
    uint32_t frame_id;
    bool need_raw;
    bool need_encoded;
    bool jpeg_capture_started;
    bool jpeg_output_started;
} DVP_VIDEO_CAMERA_T;

static bool sg_video_inited;
static esp_cam_sensor_xclk_handle_t sg_xclk_handle;

static OPERATE_RET __video_init_once(const TDD_CAMERA_ESP_VIDEO_DVP_CFG_T *cfg)
{
    if (sg_video_inited) {
        return OPRT_OK;
    }

    i2c_master_bus_handle_t i2c_bus = NULL;
    if (i2c_master_get_bus_handle(cfg->i2c_port, &i2c_bus) != ESP_OK || i2c_bus == NULL) {
        PR_ERR("get I2C bus[%d] failed; initialize board audio before camera", cfg->i2c_port);
        return OPRT_COM_ERROR;
    }

    esp_cam_sensor_xclk_config_t xclk_cfg = {
        .ledc_cfg = {
            .timer = LEDC_TIMER_1,
            .clk_cfg = LEDC_AUTO_CLK,
            .channel = LEDC_CHANNEL_2,
            .xclk_freq_hz = cfg->xclk_freq_hz,
            .xclk_pin = cfg->xclk_pin,
        },
    };
    esp_err_t err = esp_cam_sensor_xclk_allocate(ESP_CAM_SENSOR_XCLK_LEDC, &sg_xclk_handle);
    if (err != ESP_OK) {
        PR_ERR("allocate camera XCLK failed: 0x%x", err);
        return OPRT_COM_ERROR;
    }
    err = esp_cam_sensor_xclk_start(sg_xclk_handle, &xclk_cfg);
    if (err != ESP_OK) {
        PR_ERR("start camera XCLK failed: 0x%x", err);
        esp_cam_sensor_xclk_free(sg_xclk_handle);
        sg_xclk_handle = NULL;
        return OPRT_COM_ERROR;
    }

    esp_video_init_dvp_config_t dvp_cfg = {
        .sccb_config = {
            .init_sccb = false,
            .i2c_handle = i2c_bus,
            .freq = cfg->sccb_freq_hz,
        },
        .reset_pin = cfg->reset_pin,
        .pwdn_pin = cfg->pwdn_pin,
        .dvp_pin = {
            .data_width = 8,
            .data_io = {
                cfg->data_io[0], cfg->data_io[1], cfg->data_io[2], cfg->data_io[3],
                cfg->data_io[4], cfg->data_io[5], cfg->data_io[6], cfg->data_io[7],
            },
            .vsync_io = cfg->vsync_io,
            .de_io = cfg->de_io,
            .pclk_io = cfg->pclk_io,
            .xclk_io = cfg->xclk_pin,
        },
        .xclk_freq = cfg->xclk_freq_hz,
    };
    esp_video_init_config_t video_cfg = {
        .dvp = &dvp_cfg,
    };

    err = esp_video_init(&video_cfg);
    if (err != ESP_OK) {
        PR_ERR("esp_video_init(DVP) failed: 0x%x", err);
        return OPRT_COM_ERROR;
    }

    sg_video_inited = true;
    PR_NOTICE("esp_video DVP initialized on I2C%d", cfg->i2c_port);
    return OPRT_OK;
}

static void __release_buffers(DVP_VIDEO_CAMERA_T *dev)
{
    for (uint8_t i = 0; i < dev->buf_count; i++) {
        if (dev->bufs[i].addr != MAP_FAILED && dev->bufs[i].addr != NULL) {
            munmap(dev->bufs[i].addr, dev->bufs[i].len);
            dev->bufs[i].addr = NULL;
        }
        dev->bufs[i].len = 0;
    }
    dev->buf_count = 0;
}

static void __jpeg_close(DVP_VIDEO_CAMERA_T *dev)
{
    int type;

    if (dev->jpeg_fd >= 0) {
        if (dev->jpeg_output_started) {
            type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            (void)ioctl(dev->jpeg_fd, VIDIOC_STREAMOFF, &type);
            dev->jpeg_output_started = false;
        }
        if (dev->jpeg_capture_started) {
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            (void)ioctl(dev->jpeg_fd, VIDIOC_STREAMOFF, &type);
            dev->jpeg_capture_started = false;
        }
    }

    if (dev->jpeg_buf.addr != MAP_FAILED && dev->jpeg_buf.addr != NULL) {
        munmap(dev->jpeg_buf.addr, dev->jpeg_buf.len);
        dev->jpeg_buf.addr = NULL;
    }
    dev->jpeg_buf.len = 0;

    if (dev->jpeg_input != NULL) {
        heap_caps_free(dev->jpeg_input);
        dev->jpeg_input = NULL;
    }
    dev->jpeg_input_len = 0;

    if (dev->jpeg_fd >= 0) {
        close(dev->jpeg_fd);
        dev->jpeg_fd = -1;
    }
}

static OPERATE_RET __jpeg_open(DVP_VIDEO_CAMERA_T *dev)
{
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    int type;

    dev->jpeg_fd = open(ESP_VIDEO_JPEG_DEVICE_NAME, O_RDWR);
    if (dev->jpeg_fd < 0) {
        PR_ERR("open %s failed: errno=%d", ESP_VIDEO_JPEG_DEVICE_NAME, errno);
        return OPRT_COM_ERROR;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width = dev->width;
    fmt.fmt.pix.height = dev->height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
    if (ioctl(dev->jpeg_fd, VIDIOC_S_FMT, &fmt) != 0) {
        PR_ERR("JPEG VIDIOC_S_FMT(output UYVY) failed: errno=%d", errno);
        goto err;
    }

    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_USERPTR;
    if (ioctl(dev->jpeg_fd, VIDIOC_REQBUFS, &req) != 0 || req.count < 1) {
        PR_ERR("JPEG VIDIOC_REQBUFS(output USERPTR) failed: errno=%d count=%u", errno, req.count);
        goto err;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = dev->width;
    fmt.fmt.pix.height = dev->height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    if (ioctl(dev->jpeg_fd, VIDIOC_S_FMT, &fmt) != 0) {
        PR_ERR("JPEG VIDIOC_S_FMT(capture JPEG) failed: errno=%d", errno);
        goto err;
    }

    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(dev->jpeg_fd, VIDIOC_REQBUFS, &req) != 0 || req.count < 1) {
        PR_ERR("JPEG VIDIOC_REQBUFS(capture MMAP) failed: errno=%d count=%u", errno, req.count);
        goto err;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (ioctl(dev->jpeg_fd, VIDIOC_QUERYBUF, &buf) != 0) {
        PR_ERR("JPEG VIDIOC_QUERYBUF failed: errno=%d", errno);
        goto err;
    }
    dev->jpeg_buf.len = buf.length;
    dev->jpeg_buf.addr = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                              dev->jpeg_fd, buf.m.offset);
    if (dev->jpeg_buf.addr == MAP_FAILED) {
        PR_ERR("JPEG mmap failed: errno=%d", errno);
        dev->jpeg_buf.addr = NULL;
        goto err;
    }

    dev->jpeg_input_len = (size_t)dev->width * dev->height * 2;
    dev->jpeg_input = (uint8_t *)heap_caps_aligned_alloc(
        64, dev->jpeg_input_len, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (dev->jpeg_input == NULL) {
        PR_ERR("JPEG input buffer allocation failed: size=%u", (unsigned)dev->jpeg_input_len);
        goto err;
    }

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (ioctl(dev->jpeg_fd, VIDIOC_QBUF, &buf) != 0) {
        PR_ERR("JPEG VIDIOC_QBUF(capture) failed: errno=%d", errno);
        goto err;
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(dev->jpeg_fd, VIDIOC_STREAMON, &type) != 0) {
        PR_ERR("JPEG VIDIOC_STREAMON(capture) failed: errno=%d", errno);
        goto err;
    }
    dev->jpeg_capture_started = true;

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(dev->jpeg_fd, VIDIOC_STREAMON, &type) != 0) {
        PR_ERR("JPEG VIDIOC_STREAMON(output) failed: errno=%d", errno);
        goto err;
    }
    dev->jpeg_output_started = true;

    PR_NOTICE("JPEG M2M opened: %ux%u input=UYVY output=JPEG buffer=%u", dev->width,
              dev->height, (unsigned)dev->jpeg_buf.len);
    return OPRT_OK;

err:
    __jpeg_close(dev);
    return OPRT_COM_ERROR;
}

static OPERATE_RET __request_buffers(DVP_VIDEO_CAMERA_T *dev)
{
    struct v4l2_requestbuffers req = {
        .count = DVP_VIDEO_BUF_COUNT,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };

    if (ioctl(dev->fd, VIDIOC_REQBUFS, &req) != 0 || req.count < 2) {
        PR_ERR("VIDIOC_REQBUFS(MMAP) failed: errno=%d count=%u", errno, req.count);
        return OPRT_COM_ERROR;
    }
    dev->buf_count = req.count < DVP_VIDEO_BUF_COUNT ? req.count : DVP_VIDEO_BUF_COUNT;

    for (uint8_t i = 0; i < dev->buf_count; i++) {
        struct v4l2_buffer buf = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
            .index = i,
        };
        if (ioctl(dev->fd, VIDIOC_QUERYBUF, &buf) != 0) {
            PR_ERR("VIDIOC_QUERYBUF[%u] failed: errno=%d", i, errno);
            return OPRT_COM_ERROR;
        }

        dev->bufs[i].len = buf.length;
        dev->bufs[i].addr = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, buf.m.offset);
        if (dev->bufs[i].addr == MAP_FAILED) {
            PR_ERR("mmap[%u] failed: errno=%d", i, errno);
            dev->bufs[i].addr = NULL;
            return OPRT_COM_ERROR;
        }
        if (ioctl(dev->fd, VIDIOC_QBUF, &buf) != 0) {
            PR_ERR("VIDIOC_QBUF[%u] failed: errno=%d", i, errno);
            return OPRT_COM_ERROR;
        }
    }

    PR_NOTICE("DVP video buffers: %u x %u bytes (MMAP)", dev->buf_count,
              (unsigned)dev->bufs[0].len);
    return OPRT_OK;
}

static void __yuyv_to_uyvy(const uint8_t *src, uint8_t *dst, uint32_t len)
{
    for (uint32_t i = 0; i + 3 < len; i += 4) {
        dst[i]     = src[i + 1];
        dst[i + 1] = src[i];
        dst[i + 2] = src[i + 3];
        dst[i + 3] = src[i + 2];
    }
}

static void __post_raw_frame(DVP_VIDEO_CAMERA_T *dev, const uint8_t *data, uint32_t len)
{
    uint32_t frame_len = dev->width * dev->height * 2;
    TDD_CAMERA_FRAME_T *frame = tdl_camera_create_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev,
                                                              TUYA_FRAME_FMT_YUV422);
    if (frame == NULL) {
        return;
    }
    if (len < frame_len || frame_len > frame->frame.data_len) {
        PR_WARN("camera frame size mismatch: got=%u need=%u buffer=%u", len, frame_len,
                frame->frame.data_len);
        tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, frame);
        return;
    }

    /* Tuya's generic YUV422 contract is UYVY. The S31 DVP commonly exposes
     * YUYV, so normalize it at the driver boundary before publishing the raw
     * frame. This keeps DMA2D and all generic image consumers consistent. */
    if (dev->pixfmt == V4L2_PIX_FMT_YUYV) {
        __yuyv_to_uyvy(data, frame->frame.data, frame_len);
    } else {
        memcpy(frame->frame.data, data, frame_len);
    }
    frame->frame.id = (uint16_t)(dev->frame_id++);
    frame->frame.is_i_frame = 1;
    frame->frame.is_complete = 1;
    frame->frame.fmt = TUYA_FRAME_FMT_YUV422;
    frame->frame.width = (uint16_t)dev->width;
    frame->frame.height = (uint16_t)dev->height;
    frame->frame.data_len = frame_len;
    frame->frame.total_frame_len = frame_len;

    if (tdl_camera_post_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, frame) != OPRT_OK) {
        tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, frame);
    }
}

static void __post_jpeg_frame(DVP_VIDEO_CAMERA_T *dev, const uint8_t *data, uint32_t len)
{
    uint32_t frame_len = dev->width * dev->height * 2;
    struct v4l2_buffer out_buf;
    struct v4l2_buffer cap_buf;

    if (dev->jpeg_fd < 0 || dev->jpeg_input == NULL || dev->jpeg_buf.addr == NULL ||
        len < frame_len || dev->jpeg_input_len < frame_len) {
        return;
    }

    if (dev->pixfmt == V4L2_PIX_FMT_YUYV) {
        __yuyv_to_uyvy(data, dev->jpeg_input, frame_len);
    } else {
        memcpy(dev->jpeg_input, data, frame_len);
    }

    memset(&out_buf, 0, sizeof(out_buf));
    out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    out_buf.memory = V4L2_MEMORY_USERPTR;
    out_buf.index = 0;
    out_buf.m.userptr = (unsigned long)dev->jpeg_input;
    out_buf.length = frame_len;
    out_buf.bytesused = frame_len;
    if (ioctl(dev->jpeg_fd, VIDIOC_QBUF, &out_buf) != 0) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            PR_WARN("JPEG VIDIOC_QBUF(output) failed: errno=%d", errno);
        }
        return;
    }

    memset(&cap_buf, 0, sizeof(cap_buf));
    cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cap_buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(dev->jpeg_fd, VIDIOC_DQBUF, &cap_buf) != 0) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            PR_WARN("JPEG VIDIOC_DQBUF(capture) failed: errno=%d", errno);
        }
        return;
    }

    uint32_t jpeg_len = cap_buf.bytesused;
    if (jpeg_len > 0 && jpeg_len <= dev->jpeg_buf.len) {
        static bool logged_jpeg_sample;
        if (!logged_jpeg_sample && jpeg_len >= 8) {
            const uint8_t *jpeg_data = (const uint8_t *)dev->jpeg_buf.addr;
            PR_NOTICE("JPEG sample: %02x %02x %02x %02x ... %02x %02x %02x %02x, len=%u",
                      jpeg_data[0], jpeg_data[1], jpeg_data[2], jpeg_data[3],
                      jpeg_data[jpeg_len - 4], jpeg_data[jpeg_len - 3],
                      jpeg_data[jpeg_len - 2], jpeg_data[jpeg_len - 1], (unsigned)jpeg_len);
            logged_jpeg_sample = true;
        }
        TDD_CAMERA_FRAME_T *frame = tdl_camera_create_tdd_frame(
            (TDD_CAMERA_DEV_HANDLE_T)dev, TUYA_FRAME_FMT_JPEG);
        if (frame != NULL) {
            if (jpeg_len <= frame->frame.data_len) {
                memcpy(frame->frame.data, dev->jpeg_buf.addr, jpeg_len);
                frame->frame.id = (uint16_t)(dev->frame_id);
                frame->frame.is_i_frame = 1;
                frame->frame.is_complete = 1;
                frame->frame.fmt = TUYA_FRAME_FMT_JPEG;
                frame->frame.width = (uint16_t)dev->width;
                frame->frame.height = (uint16_t)dev->height;
                frame->frame.data_len = jpeg_len;
                frame->frame.total_frame_len = jpeg_len;
                if (tdl_camera_post_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, frame) != OPRT_OK) {
                    tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, frame);
                }
            } else {
                tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, frame);
            }
        }
    }

    memset(&cap_buf, 0, sizeof(cap_buf));
    cap_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cap_buf.memory = V4L2_MEMORY_MMAP;
    cap_buf.index = 0;
    (void)ioctl(dev->jpeg_fd, VIDIOC_QBUF, &cap_buf);

    memset(&out_buf, 0, sizeof(out_buf));
    out_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    out_buf.memory = V4L2_MEMORY_USERPTR;
    out_buf.index = 0;
    (void)ioctl(dev->jpeg_fd, VIDIOC_DQBUF, &out_buf);
}

static void __capture_task(void *args)
{
    DVP_VIDEO_CAMERA_T *dev = (DVP_VIDEO_CAMERA_T *)args;
    bool first_frame = true;

    while (dev->running) {
        struct v4l2_buffer buf = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
        };
        if (ioctl(dev->fd, VIDIOC_DQBUF, &buf) != 0) {
            tal_system_sleep(5);
            continue;
        }

        if (first_frame) {
            first_frame = false;
            PR_NOTICE("DVP first frame: index=%u bytesused=%u", buf.index, (unsigned)buf.bytesused);
        }
        if (buf.index < dev->buf_count && dev->bufs[buf.index].addr != NULL) {
            uint32_t len = buf.bytesused ? buf.bytesused : (uint32_t)dev->bufs[buf.index].len;
            const uint8_t *data = (const uint8_t *)dev->bufs[buf.index].addr;
            if (dev->need_raw) {
                __post_raw_frame(dev, data, len);
            }
            if (dev->need_encoded) {
                __post_jpeg_frame(dev, data, len);
            }
        }
        (void)ioctl(dev->fd, VIDIOC_QBUF, &buf);
    }

    tal_thread_delete(NULL);
}

static OPERATE_RET __camera_open(TDD_CAMERA_DEV_HANDLE_T device, TDD_CAMERA_OPEN_CFG_T *cfg)
{
    DVP_VIDEO_CAMERA_T *dev = (DVP_VIDEO_CAMERA_T *)device;
    OPERATE_RET rt = OPRT_OK;
    if (dev == NULL || cfg == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (dev->running) {
        return OPRT_OK;
    }
    if (cfg->out_fmt & TDL_CAMERA_FMT_H264) {
        PR_ERR("S31 DVP camera does not support H264 output");
        return OPRT_NOT_SUPPORTED;
    }

    dev->need_raw = (cfg->out_fmt & TDL_IMG_FMT_RAW_MASK) != 0;
    dev->need_encoded = (cfg->out_fmt & TDL_IMG_FMT_ENCODED_MASK) != 0;
    if (!dev->need_raw && !dev->need_encoded) {
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(__video_init_once(&dev->cfg));

    dev->fd = open(ESP_VIDEO_DVP_DEVICE_NAME, O_RDWR);
    if (dev->fd < 0) {
        PR_ERR("open %s failed: errno=%d", ESP_VIDEO_DVP_DEVICE_NAME, errno);
        return OPRT_COM_ERROR;
    }

    struct v4l2_format fmt = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    };
    if (ioctl(dev->fd, VIDIOC_G_FMT, &fmt) != 0) {
        PR_ERR("VIDIOC_G_FMT failed: errno=%d", errno);
        goto err;
    }
    if ((fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) &&
        (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_UYVY)) {
        PR_ERR("camera format is not YUV422: 0x%08x", (unsigned)fmt.fmt.pix.pixelformat);
        goto err;
    }
    dev->width = fmt.fmt.pix.width;
    dev->height = fmt.fmt.pix.height;
    dev->pixfmt = fmt.fmt.pix.pixelformat;
    if (dev->width != cfg->width || dev->height != cfg->height) {
        PR_ERR("camera format mismatch: actual=%ux%u requested=%ux%u", dev->width, dev->height,
               cfg->width, cfg->height);
        goto err;
    }

    if (__request_buffers(dev) != OPRT_OK) {
        goto err;
    }

    if (dev->need_encoded && __jpeg_open(dev) != OPRT_OK) {
        goto err;
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(dev->fd, VIDIOC_STREAMON, &type) != 0) {
        PR_ERR("VIDIOC_STREAMON failed: errno=%d", errno);
        goto err;
    }

    dev->running = true;
    THREAD_CFG_T th = {.stackDepth = DVP_VIDEO_TASK_STACK, .priority = THREAD_PRIO_2, .thrdname = "s31_cam"};
    rt = tal_thread_create_and_start(&dev->thread, NULL, NULL, __capture_task, dev, &th);
    if (rt != OPRT_OK) {
        dev->running = false;
        (void)ioctl(dev->fd, VIDIOC_STREAMOFF, &type);
        goto err;
    }

    PR_NOTICE("S31 DVP camera opened: %ux%u %s", dev->width, dev->height,
              dev->pixfmt == V4L2_PIX_FMT_UYVY ? "UYVY" : "YUYV");
    return OPRT_OK;

err:
    __jpeg_close(dev);
    __release_buffers(dev);
    if (dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
    }
    return OPRT_COM_ERROR;
}

static OPERATE_RET __camera_close(TDD_CAMERA_DEV_HANDLE_T device)
{
    DVP_VIDEO_CAMERA_T *dev = (DVP_VIDEO_CAMERA_T *)device;
    if (dev == NULL) {
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
    tal_system_sleep(50);

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    (void)ioctl(dev->fd, VIDIOC_STREAMOFF, &type);
    __jpeg_close(dev);
    __release_buffers(dev);
    close(dev->fd);
    dev->fd = -1;
    return OPRT_OK;
}

OPERATE_RET tdd_camera_esp_video_dvp_register(const char *name,
                                              const TDD_CAMERA_ESP_VIDEO_DVP_CFG_T *cfg)
{
    if (name == NULL || cfg == NULL) {
        return OPRT_INVALID_PARM;
    }

    DVP_VIDEO_CAMERA_T *dev = (DVP_VIDEO_CAMERA_T *)tal_malloc(sizeof(*dev));
    if (dev == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dev, 0, sizeof(*dev));
    dev->fd = -1;
    dev->jpeg_fd = -1;
    dev->cfg = *cfg;
    strncpy(dev->name, name, CAMERA_DEV_NAME_MAX_LEN);

    TDD_CAMERA_DEV_INFO_T dev_info = {
        .type = TDL_CAMERA_DVP,
        .max_fps = 24,
        .max_width = 640,
        .max_height = 480,
        .supported_fmts = TDL_CAMERA_FMT_YUV422 | TDL_CAMERA_FMT_JPEG,
        /* Raw frames are normalized to UYVY in __post_raw_frame(). */
        .yuv_order = TUYA_YUV422_UYVY,
    };
    TDD_CAMERA_INTFS_T intfs = {
        .open = __camera_open,
        .close = __camera_close,
    };
    OPERATE_RET rt = tdl_camera_device_register((char *)name, (TDD_CAMERA_DEV_HANDLE_T)dev, &intfs, &dev_info);
    if (rt != OPRT_OK) {
        tal_free(dev);
        return rt;
    }
    PR_NOTICE("registered S31 DVP camera: %s", name);
    return OPRT_OK;
}
