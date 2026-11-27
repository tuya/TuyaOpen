/**
 * @file tdd_camera_uvc.c
 * @brief UVC (USB video class) camera driver implementation
 *
 * The T5AI UVC stack (bk_uvc + multimedia) runs on the media core together
 * with the application. This driver opens the camera through it and feeds the
 * MJPEG frames into the TDL frame pool, either as they are (JPEG output) or
 * decoded to YUV422 on the fly, so a UVC camera is used like any other
 * tdl_camera device.
 *
 * The read-frame callback runs on the media transfer task, which only has
 * about 1.5 KB of stack, and the frame is released once the callback returns.
 * In decode mode the callback therefore only hands the frame to the decode
 * thread of this driver and waits for it.
 *
 * @version 1.0
 * @date 2026-11-27
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved
 */

#include <string.h>

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_gpio.h"
#include "tkl_jpeg_codec.h"

#include "tdl_camera_driver.h"
#include "tdd_camera_uvc.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define UVC_MEDIA_PORT        1 /* camera index in the media stack */
#define UVC_START_ATTEMPT_NUM 3 /* tries before the open fails */

#define UVC_POWER_STABLE_MS 200  /* settle time after the camera power is switched on */
#define UVC_POWER_CYCLE_MS  500  /* power off time between two start attempts */
#define UVC_START_DELAY_MS  1000 /* the camera may still be booting after enumeration */

#define UVC_DECODE_TASK_STACK 6144 /* jpeg header parser and decoder call chain */
#define UVC_DECODE_TASK_PRIO  THREAD_PRIO_1
#define UVC_DECODE_WAIT_MS    2000 /* upper bound for decoding one frame */

#define UVC_ERR_LOG_PERIOD_MS 5000 /* frame errors repeat at frame rate: one log line per period */

#define UVC_MAX_FPS    30
#define UVC_MAX_WIDTH  1920
#define UVC_MAX_HEIGHT 1080

/***********************************************************
***********************typedef define***********************
***********************************************************/
/* The T5AI UVC stack (bk_uvc + multimedia) has no TKL wrapper, and its headers
 * pull in the platform build config that application builds do not have. The
 * pieces of its ABI used here are re-declared below, they must stay identical
 * to t5_os ap/include/driver/media_types.h. The platform is built with short
 * enums, so its enum members are one byte wide: an int-sized member shifts
 * every member after it. The *_layout_ok typedefs break the build when a
 * struct layout drifts.
 */
typedef int bk_err_t;

#define BK_OK   0
#define BK_FAIL (-1)

typedef void *camera_handle_t;

typedef enum {
    UNKNOW_CAMERA = 0,
    DVP_CAMERA,
    UVC_CAMERA,
    NET_CAMERA,
} camera_type_t;

typedef enum {
    ROTATE_NONE = 0,
} media_rotate_t;

typedef enum {
    FPS0  = 0,
    FPS5  = (1 << 0),
    FPS10 = (1 << 1),
    FPS15 = (1 << 2),
    FPS20 = (1 << 3),
    FPS25 = (1 << 4),
    FPS30 = (1 << 5),
} frame_fps_t;

typedef enum {
    IMAGE_UNKNOW = 0,
    IMAGE_YUV    = (1 << 0),
    IMAGE_MJPEG  = (1 << 2),
    IMAGE_H264   = (1 << 3),
} image_format_t;

typedef struct frame_buffer_t frame_buffer_t;

typedef struct {
    void (*free)(frame_buffer_t *frame_buffer);
} frame_buffer_callback_t;

struct frame_buffer_t {
    uint32_t                      flag;
    uint8_t                      *frame;
    uint32_t                      size;
    uint8_t                       frame_crc;
    uint8_t                       type; /* camera_type_t */
    uint8_t                       fmt;  /* pixel_format_t */
    uint8_t                       crc;
    uint32_t                      timestamp;
    uint16_t                      width;
    uint16_t                      height;
    uint32_t                      length;
    uint32_t                      sequence;
    uint32_t                      h264_type;
    const frame_buffer_callback_t *cb;
};

typedef char frame_buffer_layout_ok[(sizeof(frame_buffer_t) == 40) ? 1 : -1];

typedef void (*frame_cb_t)(frame_buffer_t *frame);

typedef struct {
    uint8_t  type;   /* camera_type_t */
    uint16_t port;
    uint16_t format;
    uint16_t width;
    uint16_t height;
    uint32_t fps;
    uint8_t  rotate; /* media_rotate_t */
} media_camera_device_t;

typedef char media_camera_device_layout_ok[(sizeof(media_camera_device_t) == 20) ? 1 : -1];

/* MJPEG frame on loan from the media transfer task, valid until the decode thread is done with it */
typedef struct {
    const uint8_t *mjpeg;
    uint32_t      length;
    uint32_t      seq; /* frame sequence number, becomes the id of the posted frame */
    uint32_t      id;  /* request id for the completion handshake */
} CAMERA_UVC_DEC_REQ_T;

/* The media stack delivers the frames through one global callback, so there is one UVC device */
typedef struct {
    TDD_CAMERA_UVC_CFG_T cfg;
    camera_handle_t      handle;
    bool                 is_open;
    bool                 yuv_dec_mode; /* decode MJPEG to YUV422 on the fly, selected by the open format */
    CAMERA_UVC_DEC_REQ_T dec_req;      /* request handed to the decode thread */
    QUEUE_HANDLE         dec_queue;    /* carries CAMERA_UVC_DEC_REQ_T *, a NULL request stops the thread */
    SEM_HANDLE           dec_done;     /* posted once a request has been decoded */
    SEM_HANDLE           dec_exit;     /* posted when the decode thread has left its loop */
    THREAD_HANDLE        dec_thread;
    bool                 dec_busy;     /* a request is queued or being decoded */
    uint32_t             req_id;
    uint32_t             done_id;
    uint32_t             err_log_tick; /* time of the last rate limited error log */
} CAMERA_UVC_DEV_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static CAMERA_UVC_DEV_T *sg_uvc_dev = NULL;

/***********************************************************
********************function declaration********************
***********************************************************/
/* T5AI multimedia component (media_app.h), see the ABI note above. */
extern bk_err_t media_app_camera_open(camera_handle_t *handle, media_camera_device_t *device);
extern bk_err_t media_app_camera_close(camera_handle_t *handle);
extern bk_err_t media_app_register_read_frame_callback(image_format_t fmt, frame_cb_t cb);
extern bk_err_t media_app_unregister_read_frame_callback(void);

/* Stream start is split from open in the T5AI UVC stack: bk_uvc_init() only
 * enumerates the device, the ISO stream has to be started explicitly. */
extern bk_err_t bk_uvc_camera_start(camera_handle_t *handle, media_camera_device_t *device);

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Map a frame rate to the frame_fps_t value of the media stack.
 *
 * @param[in] fps Requested frame rate.
 *
 * @return The frame_fps_t value of the frame rate, FPS15 when it is not one of the supported rates.
 */
static frame_fps_t __tdd_uvc_fps_map(uint16_t fps)
{
    frame_fps_t media_fps = FPS15;

    switch (fps) {
    case 5: {
        media_fps = FPS5;
        break;
    }
    case 10: {
        media_fps = FPS10;
        break;
    }
    case 20: {
        media_fps = FPS20;
        break;
    }
    case 25: {
        media_fps = FPS25;
        break;
    }
    case 30: {
        media_fps = FPS30;
        break;
    }
    default: {
        break;
    }
    }

    return media_fps;
}

/**
 * @brief Rate limit the error logs of the frame path.
 *
 * Frame errors repeat with every frame, which would flood the log.
 *
 * @param[in] uvc_dev UVC device.
 *
 * @return true when a log line may be printed, false within UVC_ERR_LOG_PERIOD_MS of the previous one.
 */
static bool __tdd_uvc_log_allowed(CAMERA_UVC_DEV_T *uvc_dev)
{
    uint32_t now = (uint32_t)tal_system_get_millisecond();

    if ((now - uvc_dev->err_log_tick) < UVC_ERR_LOG_PERIOD_MS) {
        return false;
    }
    uvc_dev->err_log_tick = now;

    return true;
}

/**
 * @brief Handle one MJPEG frame of the media transfer task.
 *
 * The frame is released once this callback returns and the task only has
 * about 1.5 KB of stack. In decode mode the frame is therefore handed to the
 * decode thread, which works on it while this callback waits. In JPEG mode
 * the frame is copied into a node of the TDL frame pool.
 *
 * @param[in] frame Frame on loan from the media stack.
 *
 * @return none
 */
static void __tdd_uvc_frame_cb(frame_buffer_t *frame)
{
    CAMERA_UVC_DEV_T *uvc_dev = sg_uvc_dev;
    TDD_CAMERA_FRAME_T *tdd_frame = NULL;
    CAMERA_UVC_DEC_REQ_T *req = NULL;
    uint32_t copy_len = 0;

    if (NULL == frame || NULL == frame->frame || 0 == frame->length) {
        return;
    }

    if (NULL == uvc_dev || !uvc_dev->is_open) {
        return;
    }

    if (uvc_dev->yuv_dec_mode) {
        /* The decode thread is still running after a timeout, leave its frame alone. */
        if (uvc_dev->dec_busy) {
            return;
        }

        req               = &uvc_dev->dec_req;
        req->mjpeg        = frame->frame;
        req->length       = frame->length;
        req->seq          = frame->sequence;
        req->id           = ++uvc_dev->req_id;
        uvc_dev->dec_busy = true;

        if (OPRT_OK != tal_queue_post(uvc_dev->dec_queue, &req, 0)) {
            uvc_dev->dec_busy = false;
            return;
        }

        /* The completion of an earlier request must not release this frame. */
        while (uvc_dev->done_id != req->id) {
            if (OPRT_OK != tal_semaphore_wait(uvc_dev->dec_done, UVC_DECODE_WAIT_MS)) {
                break;
            }
        }

        return;
    }

    /* JPEG mode: pass the MJPEG frame on as it is. */
    tdd_frame = tdl_camera_create_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)uvc_dev, TUYA_FRAME_FMT_JPEG);
    if (NULL == tdd_frame) {
        return; /* Every node is still held by the consumer, drop the frame. */
    }

    copy_len = frame->length;
    if (copy_len > tdd_frame->frame.data_len) {
        if (__tdd_uvc_log_allowed(uvc_dev)) {
            PR_WARN("uvc frame of %u bytes truncated to the %u bytes of the frame pool node",
                    (unsigned)copy_len, (unsigned)tdd_frame->frame.data_len);
        }
        copy_len = tdd_frame->frame.data_len;
    }

    memcpy(tdd_frame->frame.data, frame->frame, copy_len);

    tdd_frame->frame.fmt             = TUYA_FRAME_FMT_JPEG;
    tdd_frame->frame.data_len        = copy_len;
    tdd_frame->frame.total_frame_len = copy_len;
    tdd_frame->frame.id              = frame->sequence;
    tdd_frame->frame.width           = frame->width;
    tdd_frame->frame.height          = frame->height;
    tdd_frame->frame.is_i_frame      = 1;
    tdd_frame->frame.is_complete     = 1;

    if (OPRT_OK != tdl_camera_post_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)uvc_dev, tdd_frame)) {
        tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)uvc_dev, tdd_frame);
    }
}

/**
 * @brief Get the frame size from the SOF segment of a JPEG image.
 *
 * Only the size is needed before the hardware decode, so the header is read
 * here instead of going through the software jpeg parser of the platform.
 *
 * @param[in] jpg JPEG data.
 * @param[in] len Length of the JPEG data.
 * @param[out] width Frame width.
 * @param[out] height Frame height.
 *
 * @return true when the size was found, false otherwise.
 */
static bool __tdd_uvc_jpeg_size(const uint8_t *jpg, uint32_t len, uint16_t *width, uint16_t *height)
{
    uint32_t pos = 2;

    if (len < 4 || jpg[0] != 0xFF || jpg[1] != 0xD8) {
        return false;
    }

    while ((pos + 4) <= len) {
        uint8_t marker = jpg[pos + 1];
        uint32_t seg_len = ((uint32_t)jpg[pos + 2] << 8) | jpg[pos + 3];

        if (jpg[pos] != 0xFF || 0xFF == marker) {
            pos++; /* fill byte */
            continue;
        }

        if (0x01 == marker || (marker >= 0xD0 && marker <= 0xD7)) {
            pos += 2; /* standalone marker, no payload */
            continue;
        }

        if (seg_len < 2 || (pos + 2 + seg_len) > len) {
            return false;
        }

        /* SOF0..SOF15, except DHT (C4), JPG (C8) and DAC (CC) */
        if (marker >= 0xC0 && marker <= 0xCF && 0xC4 != marker && 0xC8 != marker && 0xCC != marker) {
            if (seg_len < 8) {
                return false;
            }
            *height = ((uint16_t)jpg[pos + 5] << 8) | jpg[pos + 6];
            *width  = ((uint16_t)jpg[pos + 7] << 8) | jpg[pos + 8];
            return (0 != *width && 0 != *height);
        }

        if (0xDA == marker) {
            return false; /* scan data reached without a frame header */
        }

        pos += 2 + seg_len;
    }

    return false;
}

/**
 * @brief Decode one MJPEG frame to YUV422 and post it to the TDL frame queue.
 *
 * The decoded frame is written into a node of the TDL frame pool. The node is
 * released here when it could not be posted, otherwise it belongs to the
 * consumer.
 *
 * @param[in] uvc_dev UVC device.
 * @param[in] req Decode request holding the MJPEG frame on loan.
 *
 * @return none
 */
static void __tdd_uvc_decode_frame(CAMERA_UVC_DEV_T *uvc_dev, const CAMERA_UVC_DEC_REQ_T *req)
{
    TKL_JPEG_CODEC_INFO_T jpeg_info = {0};
    TDD_CAMERA_FRAME_T *tdd_frame = NULL;
    OPERATE_RET rt = OPRT_OK;
    uint32_t yuv_len = 0;

    /* The camera decides the real size, the open request only selects the stream. */
    if (!__tdd_uvc_jpeg_size(req->mjpeg, req->length, &jpeg_info.out_width, &jpeg_info.out_height)) {
        if (__tdd_uvc_log_allowed(uvc_dev)) {
            PR_ERR("no jpeg frame header in a %u byte frame", (unsigned)req->length);
        }
        return;
    }
    jpeg_info.in_size = req->length;
    yuv_len           = (uint32_t)jpeg_info.out_width * jpeg_info.out_height * 2;

    /* Any other size makes the platform fall back to its software decoder. */
    if (0 != jpeg_info.out_width % 32 || 0 != jpeg_info.out_height % 8) {
        if (__tdd_uvc_log_allowed(uvc_dev)) {
            PR_WARN("uvc frame %ux%u dropped: width must be a multiple of 32 and height of 8",
                    (unsigned)jpeg_info.out_width, (unsigned)jpeg_info.out_height);
        }
        return;
    }

    tdd_frame = tdl_camera_create_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)uvc_dev, TUYA_FRAME_FMT_YUV422);
    if (NULL == tdd_frame) {
        return; /* Every node is still held by the consumer, drop the frame. */
    }

    if (yuv_len > tdd_frame->frame.data_len) {
        if (__tdd_uvc_log_allowed(uvc_dev)) {
            PR_WARN("uvc frame %ux%u needs %u bytes, the frame pool node holds %u",
                    (unsigned)jpeg_info.out_width, (unsigned)jpeg_info.out_height, (unsigned)yuv_len,
                    (unsigned)tdd_frame->frame.data_len);
        }
        goto __EXIT;
    }

    /* The decoder only reads the frame, it stays owned by the media stack. */
    rt = tkl_jpeg_codec_convert((uint8_t *)req->mjpeg, tdd_frame->frame.data, &jpeg_info, JPEG_DEC_OUT_YUV422);
    if (OPRT_OK != rt) {
        if (__tdd_uvc_log_allowed(uvc_dev)) {
            PR_ERR("jpeg decode failed: %d", rt);
        }
        goto __EXIT;
    }

    tdd_frame->frame.fmt             = TUYA_FRAME_FMT_YUV422;
    tdd_frame->frame.id              = req->seq;
    tdd_frame->frame.width           = jpeg_info.out_width;
    tdd_frame->frame.height          = jpeg_info.out_height;
    tdd_frame->frame.data_len        = yuv_len;
    tdd_frame->frame.total_frame_len = yuv_len;
    tdd_frame->frame.is_complete     = 1;

    if (OPRT_OK != tdl_camera_post_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)uvc_dev, tdd_frame)) {
        if (__tdd_uvc_log_allowed(uvc_dev)) {
            PR_ERR("uvc frame post failed");
        }
        goto __EXIT;
    }

    return;

__EXIT:
    tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)uvc_dev, tdd_frame);
}

/**
 * @brief Decode the MJPEG frames handed over by the frame callback.
 *
 * @param[in] arg UVC device.
 *
 * @return none
 */
static void __tdd_uvc_decode_task(void *arg)
{
    CAMERA_UVC_DEV_T *uvc_dev = (CAMERA_UVC_DEV_T *)arg;
    CAMERA_UVC_DEC_REQ_T *req = NULL;

    while (1) {
        if (OPRT_OK != tal_queue_fetch(uvc_dev->dec_queue, &req, SEM_WAIT_FOREVER)) {
            continue;
        }

        if (NULL == req) {
            break; /* stop request */
        }

        __tdd_uvc_decode_frame(uvc_dev, req);

        uvc_dev->done_id  = req->id;
        uvc_dev->dec_busy = false;
        tal_semaphore_post(uvc_dev->dec_done);
    }

    tal_semaphore_post(uvc_dev->dec_exit);
}

/**
 * @brief Stop the decode thread and free its resources.
 *
 * @param[in] uvc_dev UVC device.
 *
 * @return none
 */
static void __tdd_uvc_decode_stop(CAMERA_UVC_DEV_T *uvc_dev)
{
    CAMERA_UVC_DEC_REQ_T *stop_req = NULL;

    if (NULL != uvc_dev->dec_thread) {
        /* A NULL request makes the thread leave its loop. */
        if ((OPRT_OK != tal_queue_post(uvc_dev->dec_queue, &stop_req, UVC_DECODE_WAIT_MS)) ||
            (OPRT_OK != tal_semaphore_wait(uvc_dev->dec_exit, UVC_DECODE_WAIT_MS))) {
            PR_ERR("uvc decode thread did not stop, its resources are kept");
            return;
        }

        if (OPRT_OK != tal_thread_delete(uvc_dev->dec_thread)) {
            PR_ERR("uvc decode thread delete failed");
        }
        uvc_dev->dec_thread = NULL;
    }

    if (NULL != uvc_dev->dec_queue) {
        tal_queue_free(uvc_dev->dec_queue);
        uvc_dev->dec_queue = NULL;
    }
    if (NULL != uvc_dev->dec_done) {
        tal_semaphore_release(uvc_dev->dec_done);
        uvc_dev->dec_done = NULL;
    }
    if (NULL != uvc_dev->dec_exit) {
        tal_semaphore_release(uvc_dev->dec_exit);
        uvc_dev->dec_exit = NULL;
    }
}

/**
 * @brief Create the decode thread and its resources.
 *
 * @param[in] uvc_dev UVC device.
 *
 * @return OPRT_OK on success, others on failure.
 */
static OPERATE_RET __tdd_uvc_decode_start(CAMERA_UVC_DEV_T *uvc_dev)
{
    OPERATE_RET rt = OPRT_OK;
    THREAD_CFG_T thread_cfg = {UVC_DECODE_TASK_STACK, UVC_DECODE_TASK_PRIO, "uvc_dec_task", 0};

    TUYA_CALL_ERR_GOTO(tal_queue_create_init(&uvc_dev->dec_queue, sizeof(CAMERA_UVC_DEC_REQ_T *), 1), __ERR);
    TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&uvc_dev->dec_done, 0, 1), __ERR);
    TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&uvc_dev->dec_exit, 0, 1), __ERR);
    TUYA_CALL_ERR_GOTO(tal_thread_create_and_start(&uvc_dev->dec_thread, NULL, NULL, \
                                                   __tdd_uvc_decode_task, uvc_dev, &thread_cfg), __ERR);

    uvc_dev->req_id    = 0;
    uvc_dev->done_id   = 0;
    uvc_dev->dec_busy  = false;

    return OPRT_OK;

__ERR:
    __tdd_uvc_decode_stop(uvc_dev);

    return rt;
}

/**
 * @brief Switch the camera power gpio.
 *
 * @param[in] uvc_dev UVC device.
 * @param[in] on Power the camera on or off.
 *
 * @return none
 */
static void __tdd_uvc_power_set(CAMERA_UVC_DEV_T *uvc_dev, bool on)
{
    TUYA_GPIO_LEVEL_E level = uvc_dev->cfg.pwr.active_level;
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .direct = TUYA_GPIO_OUTPUT,
    };

    if (!on) {
        level = (TUYA_GPIO_LEVEL_HIGH == level) ? TUYA_GPIO_LEVEL_LOW : TUYA_GPIO_LEVEL_HIGH;
    }
    gpio_cfg.level = level;

    if (OPRT_OK != tkl_gpio_init(uvc_dev->cfg.pwr.pin, &gpio_cfg)) {
        PR_ERR("camera power gpio %d init failed", uvc_dev->cfg.pwr.pin);
    }
}

/**
 * @brief Unsubscribe from the frames and close the camera in the media stack.
 *
 * @param[in] uvc_dev UVC device.
 *
 * @return none
 */
static void __tdd_uvc_media_close(CAMERA_UVC_DEV_T *uvc_dev)
{
    media_app_unregister_read_frame_callback();

    if (NULL != uvc_dev->handle) {
        media_app_camera_close(&uvc_dev->handle);
        uvc_dev->handle = NULL;
    }
}

/**
 * @brief Subscribe to the MJPEG frames and start the UVC stream.
 *
 * @param[in] uvc_dev UVC device.
 * @param[in] media_dev Stream parameters, the same as for the open call.
 *
 * @return BK_OK on success, the camera is closed again on failure.
 */
static bk_err_t __tdd_uvc_stream_start(CAMERA_UVC_DEV_T *uvc_dev, media_camera_device_t *media_dev)
{
    bk_err_t ret = media_app_register_read_frame_callback(IMAGE_MJPEG, __tdd_uvc_frame_cb);

    if (BK_OK != ret) {
        PR_ERR("register read frame callback failed: %d", ret);
    } else {
        /* Opening only enumerated the device, this starts the ISO stream. */
        ret = bk_uvc_camera_start(&uvc_dev->handle, media_dev);
        if (BK_OK != ret) {
            PR_ERR("uvc stream start failed: %d", ret);
        }
    }

    if (BK_OK != ret) {
        __tdd_uvc_media_close(uvc_dev);
    }

    return ret;
}

/**
 * @brief Open UVC camera device.
 *
 * @param[in] device UVC camera device handle.
 * @param[in] cfg Pointer to camera open configuration structure.
 *
 * @return OPRT_OK on success, others on failure.
 */
static OPERATE_RET __tdd_camera_uvc_open(TDD_CAMERA_DEV_HANDLE_T device, TDD_CAMERA_OPEN_CFG_T *cfg)
{
    CAMERA_UVC_DEV_T *uvc_dev = (CAMERA_UVC_DEV_T *)device;
    bk_err_t ret = BK_FAIL;
    media_camera_device_t media_dev = {0};
    OPERATE_RET rt = OPRT_OK;

    if (NULL == device || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    if (uvc_dev->is_open) {
        return OPRT_OK;
    }

    /* A UVC camera delivers MJPEG: pass it through, or decode it to YUV422 on the fly. */
    if (cfg->out_fmt & TDL_CAMERA_FMT_YUV422) {
        uvc_dev->yuv_dec_mode = true;
    } else if (cfg->out_fmt & TDL_CAMERA_FMT_JPEG) {
        uvc_dev->yuv_dec_mode = false;
    } else {
        PR_ERR("unsupported output format: %d", cfg->out_fmt);
        return OPRT_NOT_SUPPORTED;
    }

    /* Optional power switch: a camera fed straight from the USB connector leaves pwr.pin unset. */
    if (uvc_dev->cfg.pwr.pin < TUYA_GPIO_NUM_MAX) {
        __tdd_uvc_power_set(uvc_dev, true);
        tal_system_sleep(UVC_POWER_STABLE_MS);
    }

    media_dev.type   = UVC_CAMERA;
    media_dev.port   = UVC_MEDIA_PORT;
    media_dev.format = IMAGE_MJPEG;
    media_dev.width  = cfg->width;
    media_dev.height = cfg->height;
    media_dev.fps    = __tdd_uvc_fps_map(cfg->fps);
    media_dev.rotate = ROTATE_NONE;

    /* Opening enumerates the USB device. A camera that stopped answering after a
     * failed start may stay that way until it is powered again. */
    for (uint8_t attempt = 1; attempt <= UVC_START_ATTEMPT_NUM; attempt++) {
        if (attempt > 1 && uvc_dev->cfg.pwr.pin < TUYA_GPIO_NUM_MAX) {
            __tdd_uvc_power_set(uvc_dev, false);
            tal_system_sleep(UVC_POWER_CYCLE_MS);
            __tdd_uvc_power_set(uvc_dev, true);
            tal_system_sleep(UVC_POWER_STABLE_MS);
        }

        ret = media_app_camera_open(&uvc_dev->handle, &media_dev);
        if (BK_OK == ret) {
            /* The camera may still be booting after enumeration. */
            tal_system_sleep(UVC_START_DELAY_MS);
            ret = __tdd_uvc_stream_start(uvc_dev, &media_dev);
        } else {
            PR_ERR("uvc camera open failed: %d, attempt %d of %d", ret, attempt, UVC_START_ATTEMPT_NUM);
            uvc_dev->handle = NULL;
        }

        if (BK_OK == ret) {
            break;
        }
    }

    if (BK_OK != ret) {
        PR_ERR("uvc camera start failed, check that a camera is plugged in");
        return OPRT_COM_ERROR;
    }

    if (uvc_dev->yuv_dec_mode) {
        TUYA_CALL_ERR_GOTO(__tdd_uvc_decode_start(uvc_dev), __ERR);
        TUYA_CALL_ERR_GOTO(tkl_jpeg_codec_init(), __ERR);
    }

    uvc_dev->is_open = true;
    PR_NOTICE("uvc camera open success, %ux%u@%dfps, out fmt: 0x%x",
              (unsigned)cfg->width, (unsigned)cfg->height, (unsigned)cfg->fps, (unsigned)cfg->out_fmt);

    return OPRT_OK;

__ERR:
    __tdd_uvc_decode_stop(uvc_dev);
    __tdd_uvc_media_close(uvc_dev);

    return rt;
}

/**
 * @brief Close UVC camera device.
 *
 * @param[in] device UVC camera device handle.
 *
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid.
 */
static OPERATE_RET __tdd_camera_uvc_close(TDD_CAMERA_DEV_HANDLE_T device)
{
    CAMERA_UVC_DEV_T *uvc_dev = (CAMERA_UVC_DEV_T *)device;
    OPERATE_RET rt = OPRT_OK;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }

    if (!uvc_dev->is_open) {
        return OPRT_OK;
    }

    uvc_dev->is_open = false;

    /* media_app_camera_close() waits for the media transfer task, so no frame is on loan afterwards */
    __tdd_uvc_media_close(uvc_dev);

    if (uvc_dev->yuv_dec_mode) {
        __tdd_uvc_decode_stop(uvc_dev);
        TUYA_CALL_ERR_LOG(tkl_jpeg_codec_deinit());
        uvc_dev->yuv_dec_mode = false;
    }

    if (uvc_dev->cfg.pwr.pin < TUYA_GPIO_NUM_MAX) {
        __tdd_uvc_power_set(uvc_dev, false);
    }

    PR_NOTICE("uvc camera close success");

    return rt;
}

/**
 * @brief Register a UVC camera device.
 *
 * @param[in] name Camera device name.
 * @param[in] cfg Pointer to UVC camera configuration structure, NULL when the camera is powered directly.
 *
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid,
 *         OPRT_MALLOC_FAILED on memory allocation failure, OPRT_COM_ERROR when a
 *         UVC camera is registered already.
 */
OPERATE_RET tdd_camera_uvc_register(char *name, TDD_CAMERA_UVC_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    CAMERA_UVC_DEV_T *uvc_dev = NULL;
    TDD_CAMERA_DEV_INFO_T dev_info = {0};
    TDD_CAMERA_INTFS_T camera_intfs = {
        .open  = __tdd_camera_uvc_open,
        .close = __tdd_camera_uvc_close,
    };

    if (NULL == name) {
        return OPRT_INVALID_PARM;
    }

    /* The media stack delivers the frames through one global callback. */
    if (NULL != sg_uvc_dev) {
        PR_ERR("only one uvc camera is supported");
        return OPRT_COM_ERROR;
    }

    uvc_dev = (CAMERA_UVC_DEV_T *)tal_malloc(sizeof(CAMERA_UVC_DEV_T));
    if (NULL == uvc_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(uvc_dev, 0, sizeof(CAMERA_UVC_DEV_T));

    uvc_dev->cfg.pwr.pin = TUYA_GPIO_NUM_MAX;
    if (NULL != cfg) {
        uvc_dev->cfg = *cfg;
    }

    dev_info.type           = TDL_CAMERA_UVC;
    dev_info.max_fps        = UVC_MAX_FPS;
    dev_info.max_width      = UVC_MAX_WIDTH;
    dev_info.max_height     = UVC_MAX_HEIGHT;
    /* MJPEG passed through, or decoded to YUV422 (YUYV byte order) on the fly */
    dev_info.supported_fmts = TDL_CAMERA_FMT_JPEG_YUV422_BOTH;
    dev_info.yuv_order      = TUYA_YUV422_YUYV;

    rt = tdl_camera_device_register(name, (TDD_CAMERA_DEV_HANDLE_T)uvc_dev, &camera_intfs, &dev_info);
    if (OPRT_OK != rt) {
        PR_ERR("uvc device register failed: %d", rt);
        tal_free(uvc_dev);
        return rt;
    }

    sg_uvc_dev = uvc_dev;

    return OPRT_OK;
}
