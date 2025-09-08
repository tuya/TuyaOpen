/**
 * @file tdd_camera_dvp.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"

#include "tkl_dvp.h"
#include "tkl_gpio.h"

#include "tdd_camera_dvp.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDD_DVP_SR_CFG_T          sensor;
    TDD_DVP_SR_INTFS_T        intfs;
}CAMERA_DVP_DEV_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static CAMERA_DVP_DEV_T *sg_dvp_dev = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static TUYA_DVP_FRAME_MANAGE_T *__tdd_dvp_frame_manage_malloc(TUYA_FRAME_FMT_E fmt)
{
    TDD_CAMERA_FRAME_T *tdd_frame;
    TUYA_DVP_FRAME_MANAGE_T *dvp_frame;

    tdd_frame = tdl_camera_create_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)sg_dvp_dev, fmt);
    if(NULL == tdd_frame) {
        PR_ERR("tdl_camera_create_tdd_frame failed");
        return NULL;
    }

    if(sizeof(TUYA_DVP_FRAME_MANAGE_T) > sizeof(tdd_frame->rsv)) {
        PR_ERR("rsv buf is small");
        return NULL;
    }

    dvp_frame = (TUYA_DVP_FRAME_MANAGE_T *)(tdd_frame->rsv);

    dvp_frame->frame_fmt = fmt;
    dvp_frame->data      = tdd_frame->frame.data;
    dvp_frame->data_len  = tdd_frame->frame.data_len;

    dvp_frame->arg = (void *)tdd_frame;

    return dvp_frame;
}

static void __tdd_dvp_frame_manage_free(TUYA_DVP_FRAME_MANAGE_T *dvp_frame)
{
    if(NULL == dvp_frame) {
        return;
    }

    tdl_camera_release_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)sg_dvp_dev, (TDD_CAMERA_FRAME_T *)dvp_frame->arg);
}

static OPERATE_RET __tdd_dvp_frame_post_handler(TUYA_DVP_FRAME_MANAGE_T *dvp_frame)
{
    if(NULL == dvp_frame) {
        return OPRT_INVALID_PARM;
    }

    return tdl_camera_post_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)sg_dvp_dev, (TDD_CAMERA_FRAME_T *)dvp_frame->arg);
}

static OPERATE_RET __tdd_camera_dvp_init(uint32_t clk, TDD_CAMERA_OPEN_CFG_T *cfg) 
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_DVP_BASE_CFG_T dvp_base_cfg;

    switch(cfg->out_fmt) {
        case TDL_CAMERA_FMT_YUV422:
            dvp_base_cfg.output_mode = TUYA_DVP_OUTPUT_YUV422;
        break;
        case TDL_CAMERA_FMT_JPEG:
            dvp_base_cfg.output_mode = TUYA_DVP_OUTPUT_JPEG;
        break;
        case TDL_CAMERA_FMT_H264:
            dvp_base_cfg.output_mode = TUYA_DVP_OUTPUT_H264;
        break;
        case TDL_CAMERA_FMT_JPEG_YUV422_BOTH:
            dvp_base_cfg.output_mode = TUYA_DVP_OUTPUT_JPEG_YUV422_BOTH;
        break;
        case TDL_CAMERA_FMT_H264_YUV422_BOTH:
            dvp_base_cfg.output_mode = TUYA_DVP_OUTPUT_H264_YUV422_BOTH;
        break;
        default:
            PR_ERR("unsupported frame format: %d", cfg->out_fmt);
            return OPRT_INVALID_PARM;
    }

    dvp_base_cfg.width     = cfg->width;
    dvp_base_cfg.height    = cfg->height;
    dvp_base_cfg.fps       = cfg->fps;

	TUYA_CALL_ERR_RETURN(tkl_dvp_init(&dvp_base_cfg, clk));

    return OPRT_OK;
}

static OPERATE_RET __tdd_camera_dvp_open(TDD_CAMERA_DEV_HANDLE_T device, TDD_CAMERA_OPEN_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    CAMERA_DVP_DEV_T *dvp_dev = (CAMERA_DVP_DEV_T *)device;
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {.direct = TUYA_GPIO_OUTPUT};

    if (NULL == device || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    sg_dvp_dev = dvp_dev;

    tkl_dvp_frame_assign_cb_register(__tdd_dvp_frame_manage_malloc);
	tkl_dvp_frame_unassign_cb_register(__tdd_dvp_frame_manage_free);
	tkl_dvp_frame_post_cb_register(__tdd_dvp_frame_post_handler);

    if(dvp_dev->sensor.i2c.port < TUYA_I2C_NUM_MAX) {
        TUYA_CALL_ERR_RETURN(tdd_dvp_i2c_init(&dvp_dev->sensor.i2c));
    }

	if (dvp_dev->sensor.pwr.pin < TUYA_GPIO_NUM_MAX) {
		gpio_cfg.level = dvp_dev->sensor.pwr.active_level;
		tkl_gpio_init(dvp_dev->sensor.pwr.pin, &gpio_cfg);
        PR_NOTICE("dvp pwr on:%d", dvp_dev->sensor.pwr.pin);
	}

    if(dvp_dev->intfs.rst) {
        TUYA_CALL_ERR_RETURN(dvp_dev->intfs.rst(&dvp_dev->sensor.rst, dvp_dev->intfs.arg));
    }
    
    TUYA_CALL_ERR_RETURN(__tdd_camera_dvp_init(dvp_dev->sensor.clk, cfg));

    if(dvp_dev->intfs.init) {
        TUYA_CALL_ERR_RETURN(dvp_dev->intfs.init(&dvp_dev->sensor.i2c, dvp_dev->intfs.arg));
    }

    if(dvp_dev->intfs.set_ppi) {
        TUYA_CAMERA_PPI_E ppi = (TUYA_CAMERA_PPI_E)((cfg->width << 16) | cfg->height);
        TUYA_CALL_ERR_RETURN(dvp_dev->intfs.set_ppi(&dvp_dev->sensor.i2c, ppi, cfg->fps, dvp_dev->intfs.arg));
    }

    return rt;
}

static OPERATE_RET __tdd_camera_dvp_close(TDD_CAMERA_DEV_HANDLE_T device)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET tdl_camera_dvp_device_register(char *name, TDD_DVP_SR_CFG_T *sr_cfg, TDD_DVP_SR_INTFS_T *sr_intfs)
{
    CAMERA_DVP_DEV_T *dvp_dev = NULL;
    TDD_CAMERA_DEV_INFO_T dev_info;

    if(NULL == name || NULL == sr_cfg) {
        return OPRT_INVALID_PARM;
    }

    dvp_dev = (CAMERA_DVP_DEV_T *)tal_malloc(sizeof(CAMERA_DVP_DEV_T));
    if(NULL == dvp_dev) {
        return OPRT_MALLOC_FAILED;
    }
    memset(dvp_dev, 0, sizeof(CAMERA_DVP_DEV_T));

    memcpy(&dvp_dev->sensor, sr_cfg,   sizeof(TDD_DVP_SR_CFG_T));
    memcpy(&dvp_dev->intfs,  sr_intfs, sizeof(TDD_DVP_SR_INTFS_T));

    dev_info.type       = TDL_CAMERA_DVP;
    dev_info.max_fps    = sr_cfg->max_fps;
    dev_info.max_width  = sr_cfg->max_width;
    dev_info.max_height = sr_cfg->max_height;
    dev_info.fmt        = sr_cfg->fmt;

    TDD_CAMERA_INTFS_T camera_intfs = {
        .open  = __tdd_camera_dvp_open,
        .close = __tdd_camera_dvp_close,
    };

    return tdl_camera_device_register(name, (TDD_CAMERA_DEV_HANDLE_T)dvp_dev, &camera_intfs, &dev_info);
}