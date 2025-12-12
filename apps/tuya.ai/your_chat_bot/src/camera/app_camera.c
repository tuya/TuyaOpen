/**
 * @file app_camera.c
 * @brief app_camera module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include "tal_api.h"

#include "tdl_camera_manage.h"

#include "app_camera.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define APP_CAMERA_FPS    20
#define APP_CAMERA_WIDTH  480
#define APP_CAMERA_HEIGHT 480

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_CAMERA_HANDLE_T sg_camera_hdl = NULL;

static uint8_t    sg_need_capture_jpeg      = 0;
static SEM_HANDLE sg_capture_jpeg_sem       = NULL;
static uint8_t   *sg_camera_jpeg_buffer     = NULL;
static uint32_t   sg_camera_jpeg_buffer_len = 0;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __get_camera_frame_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == frame) {
        return OPRT_INVALID_PARM;
    }

    return rt;
}

static OPERATE_RET __get_camera_jpeg_frame_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == frame) {
        return OPRT_INVALID_PARM;
    }

    if (sg_need_capture_jpeg == 0) {
        return OPRT_OK;
    }

    // free old jpeg buffer
    if (sg_camera_jpeg_buffer) {
        tal_psram_free(sg_camera_jpeg_buffer);
        sg_camera_jpeg_buffer = NULL;
    }

    // allocate new jpeg buffer
    sg_camera_jpeg_buffer = (uint8_t *)tal_psram_malloc(frame->data_len);
    if (NULL == sg_camera_jpeg_buffer) {
        return OPRT_MALLOC_FAILED;
    }

    memcpy(sg_camera_jpeg_buffer, frame->data, frame->data_len);
    sg_camera_jpeg_buffer_len = frame->data_len;

    tal_semaphore_post(sg_capture_jpeg_sem);

    return rt;
}

OPERATE_RET app_camera_jpeg_capture(uint8_t **image_data, uint32_t *image_data_len, uint32_t timeout_ms)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == image_data || NULL == image_data_len) {
        return OPRT_INVALID_PARM;
    }

    sg_need_capture_jpeg      = 1;
    sg_camera_jpeg_buffer_len = 0;
    TUYA_CALL_ERR_RETURN(tal_semaphore_wait(sg_capture_jpeg_sem, timeout_ms));

    if (sg_camera_jpeg_buffer_len == 0) {
        rt = OPRT_TIMEOUT;
        goto __EXIT;
    }

    *image_data     = sg_camera_jpeg_buffer;
    *image_data_len = sg_camera_jpeg_buffer_len;

    PR_DEBUG("capture jpeg buffer len:%d", *image_data_len);

__EXIT:
    // clear capture jpeg flag
    sg_need_capture_jpeg      = 0;
    sg_camera_jpeg_buffer_len = 0;

    return rt;
}

OPERATE_RET app_camera_init(void)
{
    OPERATE_RET      rt = OPRT_OK;
    TDL_CAMERA_CFG_T cfg;

    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&sg_capture_jpeg_sem, 0, 1));
    sg_need_capture_jpeg = 0;

    // find camera device
    sg_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    TUYA_CHECK_NULL_RETURN(sg_camera_hdl, OPRT_NOT_FOUND);

    // init camera config
    memset(&cfg, 0, sizeof(TDL_CAMERA_CFG_T));

    // set camera config
    cfg.fps                  = APP_CAMERA_FPS;
    cfg.width                = APP_CAMERA_WIDTH;
    cfg.height               = APP_CAMERA_HEIGHT;
    cfg.get_frame_cb         = __get_camera_frame_cb;
    cfg.get_encoded_frame_cb = __get_camera_jpeg_frame_cb;

    // Set JPEG encoded
    cfg.out_fmt                           = TDL_CAMERA_FMT_JPEG_YUV422_BOTH;
    cfg.encoded_quality.jpeg_cfg.enable   = 1;
    cfg.encoded_quality.jpeg_cfg.max_size = 25;
    cfg.encoded_quality.jpeg_cfg.min_size = 10;

    TUYA_CALL_ERR_RETURN(tdl_camera_dev_open(sg_camera_hdl, &cfg));

    PR_NOTICE("camera init success");

    return rt;
}

OPERATE_RET app_camera_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == sg_camera_hdl) {
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(tdl_camera_dev_close(sg_camera_hdl));

    return rt;
}