/**
 * @file example_camera.c
 * @brief Camera capture module: initialization, frame callback, JPEG capture
 *
 * Raw YUV422 frames are forwarded to a registered callback for LVGL canvas
 * rendering instead of flushing directly to the display hardware.
 *
 * @version 0.2
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "example_camera.h"
#include "tal_api.h"
#include "board_com_api.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define CAMERA_FPS                  15
#define CAMERA_CAPTURE_TIMEOUT_MS   3000

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static TDL_CAMERA_HANDLE_T         s_camera_hdl = NULL;

static BOOL_T                      s_preview_on = FALSE;
static EXAMPLE_CAMERA_FRAME_CB_T   s_frame_cb = NULL;

static SEM_HANDLE                  s_capture_sem = NULL;
static MUTEX_HANDLE                s_capture_mutex = NULL;
static BOOL_T                      s_need_capture = FALSE;
static uint8_t                    *s_jpeg_buf = NULL;
static uint32_t                    s_jpeg_len = 0;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */

/**
 * @brief JPEG frame callback from camera driver
 * @param[in] hdl Camera handle
 * @param[in] frame JPEG encoded frame
 * @return OPRT_OK on success
 */
static OPERATE_RET __camera_jpeg_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    (void)hdl;

    if (frame == NULL || frame->data == NULL || frame->data_len == 0) {
        return OPRT_OK;
    }

    tal_mutex_lock(s_capture_mutex);
    if (!s_need_capture) {
        tal_mutex_unlock(s_capture_mutex);
        return OPRT_OK;
    }
    s_need_capture = FALSE;
    tal_mutex_unlock(s_capture_mutex);

    uint8_t *buf = (uint8_t *)Malloc(frame->data_len);
    if (buf == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memcpy(buf, frame->data, frame->data_len);

    tal_mutex_lock(s_capture_mutex);
    if (s_jpeg_buf != NULL) {
        Free(s_jpeg_buf);
    }
    s_jpeg_buf = buf;
    s_jpeg_len = frame->data_len;
    tal_mutex_unlock(s_capture_mutex);

    tal_semaphore_post(s_capture_sem);

    return OPRT_OK;
}

/**
 * @brief Raw YUV422 frame callback: forward to registered display callback
 * @param[in] hdl Camera handle
 * @param[in] frame YUV422 raw frame
 * @return OPRT_OK on success
 */
static OPERATE_RET __camera_raw_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    (void)hdl;

    if (!s_preview_on || frame == NULL || frame->data == NULL) {
        return OPRT_OK;
    }

    if (s_frame_cb != NULL) {
        s_frame_cb(frame->data, frame->width, frame->height);
    }

    return OPRT_OK;
}

/**
 * @brief Initialize camera with JPEG + YUV dual output
 * @return OPRT_OK on success
 */
OPERATE_RET example_camera_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    s_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    TUYA_CHECK_NULL_RETURN(s_camera_hdl, OPRT_NOT_FOUND);

    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&s_capture_sem, 0, 1));
    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&s_capture_mutex));

    TDL_CAMERA_CFG_T cfg = {
        .fps                      = CAMERA_FPS,
        .width                    = EXAMPLE_CAMERA_WIDTH,
        .height                   = EXAMPLE_CAMERA_HEIGHT,
        .get_frame_cb             = __camera_raw_cb,
        .get_encoded_frame_cb     = __camera_jpeg_cb,
        .out_fmt                  = TDL_CAMERA_FMT_JPEG_YUV422_BOTH,
        .encoded_quality.jpeg_cfg = {.enable = 1, .max_size = 25, .min_size = 10},
    };

    TUYA_CALL_ERR_RETURN(tdl_camera_dev_open(s_camera_hdl, &cfg));

    return rt;
}

/**
 * @brief Enable or disable camera live preview
 * @param[in] enable TRUE to forward frames to callback, FALSE to stop
 * @return none
 */
void example_camera_preview_enable(BOOL_T enable)
{
    s_preview_on = enable;
}

/**
 * @brief Register YUV422 frame callback for preview display
 * @param[in] cb Callback invoked with each raw frame (called from camera driver context)
 * @return none
 */
void example_camera_set_frame_cb(EXAMPLE_CAMERA_FRAME_CB_T cb)
{
    s_frame_cb = cb;
}

/**
 * @brief Capture one JPEG frame synchronously (blocks until frame arrives or timeout)
 * @param[out] data Receives Malloc'd JPEG buffer (caller must Free)
 * @param[out] len JPEG data length in bytes
 * @return OPRT_OK on success
 */
OPERATE_RET example_camera_capture_jpeg(uint8_t **data, uint32_t *len)
{
    if (data == NULL || len == NULL) {
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(s_capture_mutex);
    s_need_capture = TRUE;
    tal_mutex_unlock(s_capture_mutex);

    OPERATE_RET rt = tal_semaphore_wait(s_capture_sem, CAMERA_CAPTURE_TIMEOUT_MS);
    if (rt != OPRT_OK) {
        PR_ERR("capture timeout");
        return rt;
    }

    tal_mutex_lock(s_capture_mutex);
    *data = s_jpeg_buf;
    *len  = s_jpeg_len;
    s_jpeg_buf = NULL;
    s_jpeg_len = 0;
    tal_mutex_unlock(s_capture_mutex);

    return OPRT_OK;
}

void example_camera_release_jpeg(uint8_t *data)
{
    Free(data);
}