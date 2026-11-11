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
    bool                      is_inited;
}CAMERA_DVP_DEV_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static CAMERA_DVP_DEV_T *sg_dvp_dev = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
OPERATE_RET __tdd_dvp_setup_callback(void *args)
{
    (void)args;
    return OPRT_OK;
}

/**
 * @brief Allocate frame buffer for DVP frame management
 * @param fmt Frame format enumeration
 * @return Pointer to DVP frame management structure, or NULL on failure
 */
static TUYA_DVP_FRAME_MANAGE_T *__tdd_dvp_frame_manage_malloc(TUYA_FRAME_FMT_E fmt, void *args)
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
static OPERATE_RET __tdd_dvp_frame_post_handler(TUYA_DVP_FRAME_MANAGE_T *dvp_frame, void *args)
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
    dev->dvp_cfg.sync_polarity = dev->sensor.sync_polarity;
    dev->dvp_cfg.inter_cfg.sensor_clk = dev->sensor.usr_cfg.clk;
    dev->dvp_cfg.inter_cfg.setup_cb = __tdd_dvp_setup_callback;
    dev->dvp_cfg.inter_cfg.assign_cb = __tdd_dvp_frame_manage_malloc;
    dev->dvp_cfg.inter_cfg.post_cb = __tdd_dvp_frame_post_handler;
    dev->dvp_cfg.inter_cfg.cb_param = NULL;

    memcpy(&dev->dvp_cfg.encoded_quality, &cfg->encoded_quality, sizeof(TUYA_DVP_ENCODED_QUALITY));

    TUYA_CALL_ERR_RETURN(tkl_dvp_init(&dev->dvp_cfg));
    tkl_dvp_stop(); /* output is open()'s to enable */

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
    TDD_DVP_SR_USR_CFG_T *p_usr_cfg = NULL;

    if (NULL == device || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    sg_dvp_dev = dvp_dev;
    p_usr_cfg = &(dvp_dev->sensor.usr_cfg);

    if (dvp_dev->is_inited) {
        return tkl_dvp_start();
    }

    if (p_usr_cfg->i2c.port < TUYA_I2C_NUM_MAX) {
        TUYA_CALL_ERR_RETURN(tdd_dvp_i2c_init(&p_usr_cfg->i2c));
    }

    /* tkl_dvp_init() owns DVP MCLK through inter_cfg.sensor_clk. Start it
     * before releasing the sensor reset so the sensor sees a stable clock. */
    rt = __tdd_camera_dvp_init(dvp_dev, cfg);
    if (rt != OPRT_OK) {
        return rt;
    }

    if (dvp_dev->intfs.rst) {
        rt = dvp_dev->intfs.rst(&p_usr_cfg->pwr, &p_usr_cfg->rst,
                                dvp_dev->intfs.arg);
        if (rt != OPRT_OK) {
            goto __OPEN_FAIL;
        }
    }

    if (dvp_dev->intfs.init) {
        rt = dvp_dev->intfs.init(&p_usr_cfg->i2c, dvp_dev->intfs.arg);
        if (rt != OPRT_OK) {
            goto __OPEN_FAIL;
        }
    }

    if (dvp_dev->intfs.set_ppi) {
        TUYA_CAMERA_PPI_E ppi = (TUYA_CAMERA_PPI_E)((cfg->width << 16) | cfg->height);
        rt = dvp_dev->intfs.set_ppi(&p_usr_cfg->i2c, ppi, cfg->fps,
                                    dvp_dev->intfs.arg);
        if (rt != OPRT_OK) {
            goto __OPEN_FAIL;
        }
    }

    rt = tkl_dvp_start();
    if (rt != OPRT_OK) {
        goto __OPEN_FAIL;
    }

    dvp_dev->is_inited = true;
    return OPRT_OK;

__OPEN_FAIL:
    (void)tkl_dvp_deinit();
    return rt;
}

/**
 * @brief Close DVP camera device
 * @param device DVP camera device handle
 * @return OPRT_OK on success, error code otherwise
 */
static OPERATE_RET __tdd_camera_dvp_close(TDD_CAMERA_DEV_HANDLE_T device)
{
    CAMERA_DVP_DEV_T *dvp_dev = (CAMERA_DVP_DEV_T *)device;
    TDD_DVP_SR_USR_CFG_T *p_usr_cfg = NULL;
    OPERATE_RET rt = OPRT_OK;

    if (NULL == device) {
        return OPRT_INVALID_PARM;
    }

    p_usr_cfg = &dvp_dev->sensor.usr_cfg;
    rt = tkl_dvp_deinit();

    if (p_usr_cfg->rst.pin < TUYA_GPIO_NUM_MAX &&
        p_usr_cfg->rst.active_level != TUYA_GPIO_LEVEL_NONE) {
        tkl_gpio_write(p_usr_cfg->rst.pin, p_usr_cfg->rst.active_level);
    }
    dvp_dev->is_inited = false;
    return rt;
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
    dev_info.yuv_order  = sr_cfg->yuv_order;
    /* Platform DVP encoder accepts raw YUV422, hardware JPEG and H264
     * (see __tdd_camera_dvp_open output_mode switch). */
    dev_info.supported_fmts = TDL_CAMERA_FMT_YUV422 | TDL_CAMERA_FMT_JPEG | TDL_CAMERA_FMT_H264;

    TDD_CAMERA_INTFS_T camera_intfs = {
        .open  = __tdd_camera_dvp_open,
        .close = __tdd_camera_dvp_close,
    };

    return tdl_camera_device_register(name, (TDD_CAMERA_DEV_HANDLE_T)dvp_dev, &camera_intfs, &dev_info);
}
