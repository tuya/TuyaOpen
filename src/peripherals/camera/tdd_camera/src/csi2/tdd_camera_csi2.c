/**
 * @file tdd_camera_csi2.c
 * @brief MIPI CSI2 camera driver implementation
 *
 * Implements the MIPI CSI2 camera driver, adapting the native CSI2 camera
 * interface into TuyaOpen's unified camera subsystem with TDL_CAMERA_CSI2 type.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tkl_csi2.h"
#include "tkl_gpio.h"

#include "tdd_camera_csi2.h"

typedef struct {
    TDD_CSI2_SR_USR_CFG_T usr_cfg;
    TUYA_DVP_CFG_T        csi_cfg;
    bool                  is_inited;
} CAMERA_CSI2_DEV_T;

static CAMERA_CSI2_DEV_T *sg_csi2_dev = NULL;

static OPERATE_RET __tdd_csi2_setup_callback(void *args)
{
    (void)args;
    return OPRT_OK;
}

static TUYA_DVP_FRAME_MANAGE_T *__tdd_csi2_frame_manage_malloc(TUYA_FRAME_FMT_E fmt, void *args)
{
    TDD_CAMERA_FRAME_T      *tdd_frame;
    TUYA_DVP_FRAME_MANAGE_T *csi_frame;

    tdd_frame = tdl_camera_create_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)sg_csi2_dev, fmt);
    if (NULL == tdd_frame) {
        return NULL;
    }

    if (sizeof(TUYA_DVP_FRAME_MANAGE_T) > sizeof(tdd_frame->rsv)) {
        return NULL;
    }

    csi_frame = (TUYA_DVP_FRAME_MANAGE_T *)(tdd_frame->rsv);
    memset(csi_frame, 0x00, sizeof(TUYA_DVP_FRAME_MANAGE_T));

    csi_frame->frame_fmt = fmt;
    csi_frame->data      = tdd_frame->frame.data;
    csi_frame->data_len  = tdd_frame->frame.data_len;
    csi_frame->arg       = (void *)tdd_frame;

    return csi_frame;
}

static OPERATE_RET __tdd_csi2_frame_post_handler(TUYA_DVP_FRAME_MANAGE_T *csi_frame, void *args)
{
    TDD_CAMERA_FRAME_T *tdd_frame = NULL;

    if (NULL == csi_frame || NULL == csi_frame->arg) {
        return OPRT_INVALID_PARM;
    }

    tdd_frame                        = (TDD_CAMERA_FRAME_T *)csi_frame->arg;
    tdd_frame->frame.id              = csi_frame->frame_id;
    tdd_frame->frame.is_i_frame      = csi_frame->is_i_frame;
    tdd_frame->frame.is_complete     = csi_frame->is_frame_complete;
    tdd_frame->frame.width           = csi_frame->width;
    tdd_frame->frame.height          = csi_frame->height;
    tdd_frame->frame.data_len        = csi_frame->data_len;
    tdd_frame->frame.total_frame_len = csi_frame->total_frame_len;

    return tdl_camera_post_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)sg_csi2_dev, tdd_frame);
}

static OPERATE_RET __tdd_camera_csi2_init(CAMERA_CSI2_DEV_T *dev, TDD_CAMERA_OPEN_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == dev || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    switch (cfg->out_fmt) {
    case TDL_CAMERA_FMT_YUV422:
        dev->csi_cfg.output_mode = TUYA_CAMERA_OUTPUT_YUV422;
        break;
    case TDL_CAMERA_FMT_JPEG:
        dev->csi_cfg.output_mode = TUYA_CAMERA_OUTPUT_JPEG;
        break;
    case TDL_CAMERA_FMT_H264:
        dev->csi_cfg.output_mode = TUYA_CAMERA_OUTPUT_H264;
        break;
    default:
        dev->csi_cfg.output_mode = TUYA_CAMERA_OUTPUT_JPEG;
        break;
    }

    dev->csi_cfg.width                = cfg->width;
    dev->csi_cfg.height               = cfg->height;
    dev->csi_cfg.fps                  = cfg->fps;
    dev->csi_cfg.inter_cfg.sensor_clk = dev->usr_cfg.clk;
    dev->csi_cfg.inter_cfg.setup_cb   = __tdd_csi2_setup_callback;
    dev->csi_cfg.inter_cfg.assign_cb  = __tdd_csi2_frame_manage_malloc;
    dev->csi_cfg.inter_cfg.post_cb    = __tdd_csi2_frame_post_handler;
    dev->csi_cfg.inter_cfg.cb_param   = NULL;

    memcpy(&dev->csi_cfg.encoded_quality, &cfg->encoded_quality, sizeof(TUYA_DVP_ENCODED_QUALITY));

    TUYA_CALL_ERR_RETURN(tkl_csi2_init(&dev->csi_cfg));
    tkl_csi2_stop();

    return OPRT_OK;
}

static OPERATE_RET __tdd_camera_csi2_open(TDD_CAMERA_DEV_HANDLE_T device, TDD_CAMERA_OPEN_CFG_T *cfg)
{
    OPERATE_RET        rt      = OPRT_OK;
    CAMERA_CSI2_DEV_T *csi_dev = (CAMERA_CSI2_DEV_T *)device;

    if (NULL == device || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    sg_csi2_dev = csi_dev;

    if (csi_dev->is_inited) {
        return tkl_csi2_start();
    }

    TUYA_CALL_ERR_RETURN(__tdd_camera_csi2_init(csi_dev, cfg));
    csi_dev->is_inited = true;

    return tkl_csi2_start();
}

static OPERATE_RET __tdd_camera_csi2_close(TDD_CAMERA_DEV_HANDLE_T device)
{
    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }
    return tkl_csi2_stop();
}

OPERATE_RET tdd_camera_csi2_register(char *name, TDD_CSI2_SR_USR_CFG_T *cfg)
{
    CAMERA_CSI2_DEV_T    *csi_dev = NULL;
    TDD_CAMERA_DEV_INFO_T dev_info;

    if (NULL == name || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    csi_dev = (CAMERA_CSI2_DEV_T *)tal_malloc(sizeof(CAMERA_CSI2_DEV_T));
    if (NULL == csi_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(csi_dev, 0, sizeof(CAMERA_CSI2_DEV_T));
    memcpy(&csi_dev->usr_cfg, cfg, sizeof(TDD_CSI2_SR_USR_CFG_T));

    dev_info.type           = TDL_CAMERA_CSI2;
    dev_info.max_fps        = 30;
    dev_info.max_width      = 1920;
    dev_info.max_height     = 1080;
    dev_info.supported_fmts = TDL_CAMERA_FMT_JPEG | TDL_CAMERA_FMT_YUV422;

    TDD_CAMERA_INTFS_T camera_intfs = {
        .open  = __tdd_camera_csi2_open,
        .close = __tdd_camera_csi2_close,
    };

    return tdl_camera_device_register(name, (TDD_CAMERA_DEV_HANDLE_T)csi_dev, &camera_intfs, &dev_info);
}
