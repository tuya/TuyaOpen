/**
 * @file tdd_camera_dvp.c
 * @brief DVP (Digital Video Port) camera driver implementation
 *
 * This file implements the DVP camera driver, including frame buffer management,
 * DVP hardware initialization, camera device registration, and frame processing
 * callbacks. It provides the core functionality for DVP camera sensor integration.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
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
    TUYA_DVP_CFG_T            dvp_cfg;
}CAMERA_DVP_DEV_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static CAMERA_DVP_DEV_T *sg_dvp_dev = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Allocate frame buffer for DVP frame management
 * @param fmt Frame format enumeration
 * @return Pointer to DVP frame management structure, or NULL on failure
 */
static TUYA_DVP_FRAME_MANAGE_T *__tdd_dvp_frame_manage_malloc(TUYA_FRAME_FMT_E fmt)
{
    TDD_CAMERA_FRAME_T *tdd_frame;
    TUYA_DVP_FRAME_MANAGE_T *dvp_frame;

    tdd_frame = tdl_camera_create_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)sg_dvp_dev, fmt);
    if(NULL == tdd_frame) {
        dvp_frame = NULL;
        goto __EXIT;
    }

    if(sizeof(TUYA_DVP_FRAME_MANAGE_T) > sizeof(tdd_frame->rsv)) {
        dvp_frame = NULL;
        goto __EXIT;
    }

    dvp_frame = (TUYA_DVP_FRAME_MANAGE_T *)(tdd_frame->rsv);
    memset(dvp_frame, 0x00, sizeof(TUYA_DVP_FRAME_MANAGE_T));

    dvp_frame->frame_fmt = fmt;
    dvp_frame->data      = tdd_frame->frame.data;
    dvp_frame->data_len  = tdd_frame->frame.data_len;

    dvp_frame->arg = (void *)tdd_frame;

__EXIT:
    return dvp_frame;
}

/**
 * @brief Frame post handler for DVP hardware interrupt
 * @brief WARNING: This is hardware interrupt handler for T5, do not use PR_xxx here!
 * @param dvp_frame Pointer to DVP frame management structure
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid
 */
static OPERATE_RET __tdd_dvp_frame_post_handler(TUYA_DVP_FRAME_MANAGE_T *dvp_frame)
{
    TDD_CAMERA_FRAME_T *tdd_frame = NULL;

    if(NULL == dvp_frame || NULL == dvp_frame->arg) {
        return OPRT_INVALID_PARM;
    }

    tdd_frame = (TDD_CAMERA_FRAME_T *)dvp_frame->arg;

    tdd_frame->frame.id              = dvp_frame->frame_id;
    tdd_frame->frame.is_i_frame      = dvp_frame->is_i_frame;
    tdd_frame->frame.is_complete     = dvp_frame->is_frame_complete;
    tdd_frame->frame.width           = dvp_frame->width;
    tdd_frame->frame.height          = dvp_frame->height;
    tdd_frame->frame.data_len        = dvp_frame->data_len;
    tdd_frame->frame.total_frame_len = dvp_frame->total_frame_len;

    return tdl_camera_post_tdd_frame((TDD_CAMERA_DEV_HANDLE_T)sg_dvp_dev, (TDD_CAMERA_FRAME_T *)dvp_frame->arg);
}

/**
 * @brief Initialize DVP camera device
 * @param dev Pointer to DVP camera device structure
 * @param cfg Pointer to camera open configuration structure
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid
 */
static OPERATE_RET __tdd_camera_dvp_init(CAMERA_DVP_DEV_T *dev, TDD_CAMERA_OPEN_CFG_T *cfg) 
{
    OPERATE_RET rt = OPRT_OK;

    if(NULL == dev || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    switch(cfg->out_fmt) {
        case TDL_CAMERA_FMT_YUV422:
            dev->dvp_cfg.output_mode = TUYA_CAMERA_OUTPUT_YUV422;
        break;
        case TDL_CAMERA_FMT_JPEG:
            dev->dvp_cfg.output_mode = TUYA_CAMERA_OUTPUT_JPEG;
        break;
        case TDL_CAMERA_FMT_H264:
            dev->dvp_cfg.output_mode = TUYA_CAMERA_OUTPUT_H264;
        break;
        case TDL_CAMERA_FMT_JPEG_YUV422_BOTH:
            dev->dvp_cfg.output_mode = TUYA_CAMERA_OUTPUT_JPEG_YUV422_BOTH;
        break;
        case TDL_CAMERA_FMT_H264_YUV422_BOTH:
            dev->dvp_cfg.output_mode = TUYA_CAMERA_OUTPUT_H264_YUV422_BOTH;
        break;
        default:
            PR_ERR("unsupported frame format: %d", cfg->out_fmt);
            return OPRT_INVALID_PARM;
    }

    dev->dvp_cfg.width     = cfg->width;
    dev->dvp_cfg.height    = cfg->height;
    dev->dvp_cfg.fps       = cfg->fps;

    memcpy(&dev->dvp_cfg.encoded_quality, &cfg->encoded_quality, sizeof(TUYA_DVP_ENCODED_QUALITY));

    TUYA_CALL_ERR_RETURN(tkl_dvp_init(&dev->dvp_cfg, dev->sensor.usr_cfg.clk));

    return OPRT_OK;
}

/**
 * @brief Open DVP camera device
 * @param device DVP camera device handle
 * @param cfg Pointer to camera open configuration structure
 * @return OPRT_OK on success, error code otherwise
 */
static OPERATE_RET __tdd_camera_dvp_open(TDD_CAMERA_DEV_HANDLE_T device, TDD_CAMERA_OPEN_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    CAMERA_DVP_DEV_T *dvp_dev = (CAMERA_DVP_DEV_T *)device;
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {.direct = TUYA_GPIO_OUTPUT};
    TDD_DVP_SR_USR_CFG_T *p_usr_cfg = NULL;

    if(NULL == device || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    sg_dvp_dev = dvp_dev;
    p_usr_cfg  = &(dvp_dev->sensor.usr_cfg);

    tkl_dvp_frame_assign_cb_register(__tdd_dvp_frame_manage_malloc);
	tkl_dvp_frame_post_cb_register(__tdd_dvp_frame_post_handler);

    if(p_usr_cfg->i2c.port < TUYA_I2C_NUM_MAX) {
        TUYA_CALL_ERR_RETURN(tdd_dvp_i2c_init(&p_usr_cfg->i2c));
    }

	if (p_usr_cfg->pwr.pin < TUYA_GPIO_NUM_MAX) {
		gpio_cfg.level = p_usr_cfg->pwr.active_level;
		tkl_gpio_init(p_usr_cfg->pwr.pin, &gpio_cfg);
        PR_NOTICE("dvp pwr on:%d", p_usr_cfg->pwr.pin);
	}

    if(dvp_dev->intfs.rst) {
        TUYA_CALL_ERR_RETURN(dvp_dev->intfs.rst(&p_usr_cfg->rst, dvp_dev->intfs.arg));
    }
    
    TUYA_CALL_ERR_RETURN(__tdd_camera_dvp_init(dvp_dev, cfg));

    if(dvp_dev->intfs.init) {
        TUYA_CALL_ERR_RETURN(dvp_dev->intfs.init(&p_usr_cfg->i2c, dvp_dev->intfs.arg));
    }

    if(dvp_dev->intfs.set_ppi) {
        TUYA_CAMERA_PPI_E ppi = (TUYA_CAMERA_PPI_E)((cfg->width << 16) | cfg->height);
        TUYA_CALL_ERR_RETURN(dvp_dev->intfs.set_ppi(&p_usr_cfg->i2c, ppi, cfg->fps, dvp_dev->intfs.arg));
    }

    return rt;
}

/**
 * @brief Close DVP camera device
 * @param device DVP camera device handle
 * @return OPRT_NOT_SUPPORTED (function not implemented)
 */
static OPERATE_RET __tdd_camera_dvp_close(TDD_CAMERA_DEV_HANDLE_T device)
{
    return OPRT_NOT_SUPPORTED;
}

/**
 * @brief Register a DVP camera device
 * @param name Camera device name
 * @param sr_cfg Pointer to DVP sensor configuration structure
 * @param sr_intfs Pointer to DVP sensor interface functions structure
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid,
 *         OPRT_MALLOC_FAILED on memory allocation failure
 */
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