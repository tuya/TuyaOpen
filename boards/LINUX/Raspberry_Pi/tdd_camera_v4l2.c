/**
 * @file tdd_camera_v4l2.c
 * @brief V4L2 (UVC) camera driver implementation for Linux
 *
 * This file implements the V4L2 camera driver adapter on Linux, including:
 * - V4L2 device open/start/stop/close via TKL layer
 * - Frame capture thread (dequeue/queue)
 * - Frame buffer copy and posting to TDL camera manager
 * - Camera device registration for upper-layer discovery
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tdd_camera_v4l2.h"

#include "tal_api.h"
#include "tuya_cloud_types.h"
#include "tuya_error_code.h"

#include "tdl_camera_driver.h"

#include "camera/tkl_camera_v4l2.h"
#include "jpeg_codec/tkl_jpeg_codec.h"

#define V4L2_DEVNODE_MAX_LEN 128

typedef struct {
    char devnode[V4L2_DEVNODE_MAX_LEN];

    TKL_CAMERA_V4L2_HANDLE_T tkl_hdl;

    THREAD_HANDLE thread;
    volatile bool running;

    uint16_t width;
    uint16_t height;
    uint16_t fps;

    TKL_CAMERA_V4L2_PIXFMT_E pixfmt;

    bool need_raw;
    bool need_encoded;
    TUYA_FRAME_FMT_E encoded_post_fmt;

    bool jpeg_codec_inited;
    uint32_t frame_id;
} CAMERA_V4L2_DEV_T;

static void __camera_v4l2_capture_task(void *args)
{
    CAMERA_V4L2_DEV_T *dev = (CAMERA_V4L2_DEV_T *)args;
    if (!dev) {
        return;
    }

    while (dev->running) {
        uint8_t *v4l2_data = NULL;
        uint32_t v4l2_len = 0;
        uint32_t v4l2_index = 0;

        OPERATE_RET rt = tkl_camera_v4l2_dequeue(dev->tkl_hdl, &v4l2_data, &v4l2_len, &v4l2_index);
        if (rt != OPRT_OK) {
            tal_system_sleep(10);
            continue;
        }

        // 1) Post encoded JPEG (MJPEG) frame directly if requested.
        if (dev->need_encoded && dev->pixfmt == TKL_CAMERA_V4L2_PIXFMT_MJPEG) {
            TDD_CAMERA_FRAME_T *enc_frame = tdl_camera_create_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, dev->encoded_post_fmt);
            if (enc_frame) {
                if (v4l2_len <= enc_frame->frame.data_len) {
                    memcpy(enc_frame->frame.data, v4l2_data, v4l2_len);

                    enc_frame->frame.id = (uint16_t)(dev->frame_id++);
                    enc_frame->frame.is_i_frame = 1;
                    enc_frame->frame.is_complete = 1;
                    enc_frame->frame.width = dev->width;
                    enc_frame->frame.height = dev->height;
                    enc_frame->frame.data_len = v4l2_len;
                    enc_frame->frame.total_frame_len = v4l2_len;

                    rt = tdl_camera_post_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, enc_frame);
                    if (rt != OPRT_OK) {
                        tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, enc_frame);
                    }
                } else {
                    static bool warned = false;
                    if (!warned) {
                        warned = true;
                        PR_WARN("v4l2 jpeg frame too large: %u > %u, drop", v4l2_len, enc_frame->frame.data_len);
                    }
                    tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, enc_frame);
                }
            }
        }

        // 2) Post raw YUV422.
        if (dev->need_raw && dev->pixfmt == TKL_CAMERA_V4L2_PIXFMT_YUYV) {
            TDD_CAMERA_FRAME_T *raw_frame = tdl_camera_create_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, TUYA_FRAME_FMT_YUV422);
            if (raw_frame) {
                if (v4l2_len <= raw_frame->frame.data_len) {
                    memcpy(raw_frame->frame.data, v4l2_data, v4l2_len);

                    raw_frame->frame.id = (uint16_t)(dev->frame_id++);
                    raw_frame->frame.is_i_frame = 1;
                    raw_frame->frame.is_complete = 1;
                    raw_frame->frame.width = dev->width;
                    raw_frame->frame.height = dev->height;
                    raw_frame->frame.data_len = v4l2_len;
                    raw_frame->frame.total_frame_len = v4l2_len;

                    rt = tdl_camera_post_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, raw_frame);
                    if (rt != OPRT_OK) {
                        tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, raw_frame);
                    }
                } else {
                    tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, raw_frame);
                }
            }
        }

        // 3) Decode MJPEG to YUV422(UYVY) and post raw frame if requested.
        if (dev->need_raw && dev->pixfmt == TKL_CAMERA_V4L2_PIXFMT_MJPEG) {
            TDD_CAMERA_FRAME_T *raw_frame = tdl_camera_create_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, TUYA_FRAME_FMT_YUV422);
            if (raw_frame) {
                TKL_JPEG_CODEC_INFO_T info;
                memset(&info, 0, sizeof(info));
                if (tkl_jpeg_codec_img_info_get(v4l2_data, v4l2_len, &info) == OPRT_OK) {
                    const uint32_t out_len = (uint32_t)info.out_width * (uint32_t)info.out_height * 2;
                    if (out_len <= raw_frame->frame.data_len) {
                        info.in_size = v4l2_len;
                        if (tkl_jpeg_codec_convert(v4l2_data, raw_frame->frame.data, &info, JPEG_DEC_OUT_YUV422) == OPRT_OK) {
                            raw_frame->frame.id = (uint16_t)(dev->frame_id++);
                            raw_frame->frame.is_i_frame = 1;
                            raw_frame->frame.is_complete = 1;
                            raw_frame->frame.width = info.out_width;
                            raw_frame->frame.height = info.out_height;
                            raw_frame->frame.data_len = out_len;
                            raw_frame->frame.total_frame_len = out_len;

                            rt = tdl_camera_post_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, raw_frame);
                            if (rt != OPRT_OK) {
                                tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, raw_frame);
                            }
                        } else {
                            tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, raw_frame);
                        }
                    } else {
                        tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, raw_frame);
                    }
                } else {
                    tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)dev, raw_frame);
                }
            }
        }

        (void)tkl_camera_v4l2_queue(dev->tkl_hdl, v4l2_index);
    }
}

static OPERATE_RET __tdd_camera_v4l2_open(TDD_CAMERA_DEV_HANDLE_T device, TDD_CAMERA_OPEN_CFG_T *cfg)
{
    CAMERA_V4L2_DEV_T *dev = (CAMERA_V4L2_DEV_T *)device;
    if (dev == NULL || cfg == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (dev->running) {
        return OPRT_OK;
    }

    bool need_raw = (cfg->out_fmt & TDL_IMG_FMT_RAW_MASK) ? true : false;
    bool need_encoded = (cfg->out_fmt & TDL_IMG_FMT_ENCODED_MASK) ? true : false;

    if (cfg->out_fmt == TDL_CAMERA_FMT_H264 || cfg->out_fmt == TDL_CAMERA_FMT_H264_YUV422_BOTH) {
        PR_ERR("V4L2 camera H264 output not supported");
        return OPRT_NOT_SUPPORTED;
    }

    dev->need_raw = need_raw;
    dev->need_encoded = need_encoded;
    dev->encoded_post_fmt = TUYA_FRAME_FMT_JPEG;
    dev->jpeg_codec_inited = false;

    dev->width = cfg->width;
    dev->height = cfg->height;
    dev->fps = cfg->fps;

    TKL_CAMERA_V4L2_CFG_T v4l2_cfg = {0};
    v4l2_cfg.devnode = dev->devnode;
    v4l2_cfg.width = cfg->width;
    v4l2_cfg.height = cfg->height;
    v4l2_cfg.fps = cfg->fps;
    v4l2_cfg.buffer_count = 4;

    OPERATE_RET rt = OPRT_OK;

    // Prefer native RAW (YUYV) when only RAW is requested.
    // If device doesn't support YUYV, fallback to MJPEG and decode to RAW in software.
    if (need_raw && !need_encoded) {
        v4l2_cfg.pixfmt = TKL_CAMERA_V4L2_PIXFMT_YUYV;
        rt = tkl_camera_v4l2_open(&dev->tkl_hdl, &v4l2_cfg);
        if (rt != OPRT_OK) {
            PR_WARN("tkl_camera_v4l2_open(YUYV) failed: %d, fallback to MJPEG+decode", rt);
            v4l2_cfg.pixfmt = TKL_CAMERA_V4L2_PIXFMT_MJPEG;
            rt = tkl_camera_v4l2_open(&dev->tkl_hdl, &v4l2_cfg);
            if (rt == OPRT_OK) {
                if (tkl_jpeg_codec_init() == OPRT_OK) {
                    dev->jpeg_codec_inited = true;
                } else {
                    PR_WARN("tkl_jpeg_codec_init failed, raw decode may fail");
                }
            }
        }
    } else {
        // For JPEG/BOTH mode, capture MJPEG and (optionally) decode to YUV422 in software.
        v4l2_cfg.pixfmt = (need_encoded || cfg->out_fmt == TDL_CAMERA_FMT_JPEG || (need_raw && need_encoded)) ?
                              TKL_CAMERA_V4L2_PIXFMT_MJPEG :
                              TKL_CAMERA_V4L2_PIXFMT_YUYV;
        rt = tkl_camera_v4l2_open(&dev->tkl_hdl, &v4l2_cfg);
        if (rt == OPRT_OK && need_raw && v4l2_cfg.pixfmt == TKL_CAMERA_V4L2_PIXFMT_MJPEG) {
            if (tkl_jpeg_codec_init() == OPRT_OK) {
                dev->jpeg_codec_inited = true;
            } else {
                PR_WARN("tkl_jpeg_codec_init failed, raw decode may fail");
            }
        }
    }

    dev->pixfmt = v4l2_cfg.pixfmt;

    if (rt != OPRT_OK) {
        PR_ERR("tkl_camera_v4l2_open failed: %d", rt);
        return rt;
    }

    rt = tkl_camera_v4l2_start(dev->tkl_hdl);
    if (rt != OPRT_OK) {
        PR_ERR("tkl_camera_v4l2_start failed: %d", rt);
        (void)tkl_camera_v4l2_close(dev->tkl_hdl);
        dev->tkl_hdl = NULL;
        if (dev->jpeg_codec_inited) {
            (void)tkl_jpeg_codec_deinit();
            dev->jpeg_codec_inited = false;
        }
        return rt;
    }

    dev->running = true;
    THREAD_CFG_T thread_cfg = {8192, THREAD_PRIO_1, "v4l2_cam"};
    rt = tal_thread_create_and_start(&dev->thread, NULL, NULL, __camera_v4l2_capture_task, dev, &thread_cfg);
    if (rt != OPRT_OK) {
        dev->running = false;
        (void)tkl_camera_v4l2_stop(dev->tkl_hdl);
        (void)tkl_camera_v4l2_close(dev->tkl_hdl);
        dev->tkl_hdl = NULL;
        if (dev->jpeg_codec_inited) {
            (void)tkl_jpeg_codec_deinit();
            dev->jpeg_codec_inited = false;
        }
        return rt;
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_camera_v4l2_close(TDD_CAMERA_DEV_HANDLE_T device)
{
    CAMERA_V4L2_DEV_T *dev = (CAMERA_V4L2_DEV_T *)device;
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

    if (dev->tkl_hdl) {
        (void)tkl_camera_v4l2_stop(dev->tkl_hdl);
        (void)tkl_camera_v4l2_close(dev->tkl_hdl);
        dev->tkl_hdl = NULL;
    }

    if (dev->jpeg_codec_inited) {
        (void)tkl_jpeg_codec_deinit();
        dev->jpeg_codec_inited = false;
    }

    return OPRT_OK;
}

OPERATE_RET tdd_camera_v4l2_register(const char *name, const char *devnode)
{
    if (name == NULL || devnode == NULL) {
        return OPRT_INVALID_PARM;
    }

    CAMERA_V4L2_DEV_T *dev = (CAMERA_V4L2_DEV_T *)tal_malloc(sizeof(CAMERA_V4L2_DEV_T));
    if (dev == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dev, 0, sizeof(*dev));

    strncpy(dev->devnode, devnode, sizeof(dev->devnode) - 1);

    TDD_CAMERA_DEV_INFO_T dev_info = {0};
    dev_info.type = TDL_CAMERA_DVP; /* Keep existing type for compatibility with apps expecting a camera device */
    dev_info.max_fps = 60;
    dev_info.max_width = 1920;
    dev_info.max_height = 1080;
    dev_info.fmt = TUYA_FRAME_FMT_YUV422;

    TDD_CAMERA_INTFS_T intfs = {
        .open = __tdd_camera_v4l2_open,
        .close = __tdd_camera_v4l2_close,
    };

    OPERATE_RET rt = tdl_camera_device_register((char *)name, (TDD_CAMERA_DEV_HANDLE_T)dev, &intfs, &dev_info);
    if (rt != OPRT_OK) {
        tal_free(dev);
        return rt;
    }

    PR_INFO("registered V4L2 camera: name=%s devnode=%s", name, devnode);
    return OPRT_OK;
}
