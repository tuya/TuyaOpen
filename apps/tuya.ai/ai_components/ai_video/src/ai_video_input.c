/**
 * @file ai_video_input.c
 * @brief AI video input module implementation
 *
 * This module implements video input processing, including camera frame
 * capture, JPEG encoding, and video display functionality.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tal_api.h"

#include "tuya_ai_client.h"
#include "ai_user_event.h"
#include "tdl_camera_manage.h"

#include "ai_video_input.h"

/***********************************************************
************************macro define************************
***********************************************************/
#if defined(ENBALE_EXT_RAM) && (ENBALE_EXT_RAM == 1)
#define AI_VIDEO_MALLOC tal_psram_malloc
#define AI_VIDEO_FREE   tal_psram_free
#else
#define AI_VIDEO_MALLOC tal_malloc
#define AI_VIDEO_FREE   tal_free
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t     *data;
    uint32_t     len;
    bool         need_capture;
    SEM_HANDLE   sem;
    MUTEX_HANDLE mutex;
} JPEG_FRAME_CAPTURE_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_CAMERA_HANDLE_T    sg_camera_hdl = NULL;
static TDL_CAMERA_CFG_T       sg_camera_cfg;
static DELAYED_WORK_HANDLE    sg_delayed_work = NULL;
static JPEG_FRAME_CAPTURE_T   sg_jpeg_capture;
static AI_VIDEO_DISP_FLUSH_CB sg_disp_flush_cb   = NULL;
static bool                   sg_is_disp_started = false;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __get_raw_frame_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == frame) {
        return OPRT_INVALID_PARM;
    }

    if (false == sg_is_disp_started || NULL == sg_disp_flush_cb) {
        return OPRT_OK;
    }

    sg_disp_flush_cb(frame);

    return rt;
}

static OPERATE_RET __get_jpeg_frame_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == frame) {
        return OPRT_INVALID_PARM;
    }

    if (NULL == sg_jpeg_capture.mutex || false == sg_jpeg_capture.need_capture) {
        return OPRT_OK;
    }

    tal_mutex_lock(sg_jpeg_capture.mutex);

    if (sg_jpeg_capture.need_capture) {
        if (sg_jpeg_capture.data) {
            AI_VIDEO_FREE(sg_jpeg_capture.data);
            sg_jpeg_capture.data = NULL;
        }

        sg_jpeg_capture.data = (uint8_t *)AI_VIDEO_MALLOC(frame->data_len);
        if (sg_jpeg_capture.data) {
            memcpy(sg_jpeg_capture.data, frame->data, frame->data_len);
            sg_jpeg_capture.len          = frame->data_len;
            sg_jpeg_capture.need_capture = FALSE;

            if (sg_jpeg_capture.sem) {
                tal_semaphore_post(sg_jpeg_capture.sem);
            }
            PR_DEBUG("JPEG frame captured, len: %d", frame->data_len);
        } else {
            PR_ERR("Failed to allocate memory for JPEG frame");
            sg_jpeg_capture.len = 0;
        }
    }

    tal_mutex_unlock(sg_jpeg_capture.mutex);

    return rt;
}

static void __video_init_workq(void *args)
{
    OPERATE_RET       rt  = OPRT_OK;
    TDL_CAMERA_CFG_T *cfg = (TDL_CAMERA_CFG_T *)args;

    PR_DEBUG("video -> camera init event");

    /* Find camera device */
    sg_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    if (NULL == sg_camera_hdl) {
        PR_ERR("camera dev %s not found", CAMERA_NAME);
        return;
    }

    TUYA_CALL_ERR_LOG(tdl_camera_dev_open(sg_camera_hdl, cfg));

    return;
}

/**
@brief Initialize AI video input module
@param vi_cfg Video input configuration
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_video_init(AI_VIDEO_CFG_T *vi_cfg)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(vi_cfg, OPRT_INVALID_PARM);

    if (!sg_jpeg_capture.mutex) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&sg_jpeg_capture.mutex));
    }

    if (!sg_jpeg_capture.sem) {
        TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&sg_jpeg_capture.sem, 0, 1));
    }

    sg_disp_flush_cb   = vi_cfg->disp_flush_cb;
    sg_is_disp_started = false;

    /* Set camera config */
    sg_camera_cfg.width  = COMP_AI_VIDEO_WIDTH;
    sg_camera_cfg.height = COMP_AI_VIDEO_HEIGHT;
    sg_camera_cfg.fps    = COMP_AI_VIDEO_FPS;

    sg_camera_cfg.get_frame_cb         = __get_raw_frame_cb;
    sg_camera_cfg.get_encoded_frame_cb = __get_jpeg_frame_cb;

    /* Set JPEG encoded */
    sg_camera_cfg.out_fmt = TDL_CAMERA_FMT_JPEG_YUV422_BOTH;

#if defined(ENABLE_COMP_AI_VIDEO_JPEG_QUALITY) && (ENABLE_COMP_AI_VIDEO_JPEG_QUALITY == 1)
    sg_camera_cfg.encoded_quality.jpeg_cfg.enable   = 1;
    sg_camera_cfg.encoded_quality.jpeg_cfg.max_size = COMP_AI_VIDEO_JPEG_QUALITY_MAX_SIZE;
    sg_camera_cfg.encoded_quality.jpeg_cfg.min_size = COMP_AI_VIDEO_JPEG_QUALITY_MIN_SIZE;
#endif

    TUYA_CALL_ERR_RETURN(tal_workq_init_delayed(WORKQ_SYSTEM, __video_init_workq, &sg_camera_cfg, &sg_delayed_work));
    TUYA_CALL_ERR_RETURN(tal_workq_start_delayed(sg_delayed_work, 500, LOOP_ONCE));

    PR_NOTICE("camera init success");

    return OPRT_OK;
}

/**
@brief Deinitialize AI video input module
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_video_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (sg_jpeg_capture.mutex) {
        tal_mutex_lock(sg_jpeg_capture.mutex);
        if (sg_jpeg_capture.data) {
            AI_VIDEO_FREE(sg_jpeg_capture.data);
            sg_jpeg_capture.data = NULL;
        }
        sg_jpeg_capture.len          = 0;
        sg_jpeg_capture.need_capture = false;
        tal_mutex_unlock(sg_jpeg_capture.mutex);
        tal_mutex_release(sg_jpeg_capture.mutex);
        sg_jpeg_capture.mutex = NULL;
    }

    if (sg_jpeg_capture.sem) {
        tal_semaphore_release(sg_jpeg_capture.sem);
        sg_jpeg_capture.sem = NULL;
    }

    if (NULL == sg_camera_hdl) {
        tdl_camera_dev_close(sg_camera_hdl);
        sg_camera_hdl = NULL;
    }

    return rt;
}

/**
@brief Get JPEG frame from camera
@param image_data Pointer to store image data pointer
@param image_data_len Pointer to store image data length
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_video_get_jpeg_frame(uint8_t **image_data, uint32_t *image_data_len)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == image_data || NULL == image_data_len) {
        return OPRT_INVALID_PARM;
    }

    if (!sg_jpeg_capture.mutex || !sg_jpeg_capture.sem) {
        PR_ERR("JPEG capture not initialized");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_jpeg_capture.mutex);
    sg_jpeg_capture.need_capture = true;
    tal_mutex_unlock(sg_jpeg_capture.mutex);

    PR_DEBUG("Waiting for next JPEG frame...");

    OPERATE_RET ret = tal_semaphore_wait(sg_jpeg_capture.sem, 3000);
    if (ret != OPRT_OK) {
        tal_mutex_lock(sg_jpeg_capture.mutex);
        sg_jpeg_capture.need_capture = false;
        tal_mutex_unlock(sg_jpeg_capture.mutex);
        PR_ERR("Wait for JPEG frame timeout");
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(sg_jpeg_capture.mutex);

    if (!sg_jpeg_capture.data || sg_jpeg_capture.len == 0) {
        tal_mutex_unlock(sg_jpeg_capture.mutex);
        PR_ERR("No valid JPEG frame data");
        return OPRT_COM_ERROR;
    }

    *image_data = (uint8_t *)AI_VIDEO_MALLOC(sg_jpeg_capture.len);
    if (!(*image_data)) {
        tal_mutex_unlock(sg_jpeg_capture.mutex);
        PR_ERR("Failed to allocate memory for JPEG frame");
        return OPRT_MALLOC_FAILED;
    }

    memcpy(*image_data, sg_jpeg_capture.data, sg_jpeg_capture.len);
    *image_data_len = sg_jpeg_capture.len;

    tal_mutex_unlock(sg_jpeg_capture.mutex);

    PR_DEBUG("Get JPEG frame success, len: %d", *image_data_len);

    return rt;
}

/**
@brief Free JPEG image data
@param image_data Pointer to image data pointer
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_video_jpeg_image_free(uint8_t **image_data)
{
    if (image_data == NULL || *image_data == NULL) {
        return OPRT_INVALID_PARM;
    }

    AI_VIDEO_FREE(*image_data);
    *image_data = NULL;

    return OPRT_OK;
}

/**
@brief Start video display
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_video_display_start(void)
{
    AI_NOTIFY_VIDEO_START_T notify = {
        .camera_width  = sg_camera_cfg.width,
        .camera_height = sg_camera_cfg.height,
    };

    ai_user_event_notify(AI_USER_EVT_VIDEO_DISPLAY_START, &notify);

    sg_is_disp_started = true;

    return OPRT_OK;
}

/**
@brief Stop video display
@return OPERATE_RET Operation result
*/
OPERATE_RET ai_video_display_stop(void)
{
    sg_is_disp_started = false;

    ai_user_event_notify(AI_USER_EVT_VIDEO_DISPLAY_END, NULL);

    return OPRT_OK;
}
