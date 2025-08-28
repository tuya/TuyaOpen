/**
 * @file example_dvp.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"

#include "tkl_video_in.h"
#include "tkl_video_enc.h"

#include "board_com_api.h"
#include "tdl_display_manage.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define CAMERA_WIDTH              480
#define CAMERA_HEIGHT             480

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_DISP_HANDLE_T      sg_tdl_disp_hdl = NULL;
static TDL_DISP_DEV_INFO_T    sg_display_info;
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb_1 = NULL, *sg_p_display_fb_2 = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __dvp_display_frame_cb(TKL_VI_DISP_FRAME_T *p_frame)
{
    static TDL_DISP_FRAME_BUFF_T *target_fb = NULL;

    if(NULL == p_frame) {
        return;
    }

    if(target_fb == sg_p_display_fb_1) {
        target_fb = sg_p_display_fb_2;
    }else {
        target_fb = sg_p_display_fb_1;
    }

    if(p_frame->frame_len > target_fb->len) {
        PR_ERR("frame length %d exceeds buffer length %d", p_frame->frame_len, target_fb->len);
        return;
    }

    memcpy(target_fb->frame, p_frame->pbuf, p_frame->frame_len);

    tdl_disp_dev_flush(sg_tdl_disp_hdl, target_fb);

    return;
}

static OPERATE_RET __dvp_display_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t frame_len = 0;

    memset(&sg_display_info, 0, sizeof(TDL_DISP_DEV_INFO_T));

    sg_tdl_disp_hdl = tdl_disp_find_dev(DISPLAY_NAME);
    if(NULL == sg_tdl_disp_hdl) {
        PR_ERR("display dev %s not found", DISPLAY_NAME);
        return OPRT_NOT_FOUND;
    }

    TUYA_CALL_ERR_RETURN(tdl_disp_dev_get_info(sg_tdl_disp_hdl, &sg_display_info));

    if(sg_display_info.fmt != TUYA_PIXEL_FMT_RGB565) {
        PR_ERR("display pixel format %d not supported", sg_display_info.fmt);
        return OPRT_NOT_SUPPORTED;
    }

    TUYA_CALL_ERR_RETURN(tdl_disp_dev_open(sg_tdl_disp_hdl));
 
    tdl_disp_set_brightness(sg_tdl_disp_hdl, 100); // Set brightness to 100%

    /*create frame buffer*/
    frame_len = CAMERA_WIDTH * CAMERA_HEIGHT * 2; // RGB565 is 2 bytes per pixel
    sg_p_display_fb_1 = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    sg_p_display_fb_2 = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if(NULL == sg_p_display_fb_1 || NULL == sg_p_display_fb_2) {
        tdl_disp_free_frame_buff(sg_p_display_fb_1), sg_p_display_fb_1 = NULL;
        tdl_disp_free_frame_buff(sg_p_display_fb_2), sg_p_display_fb_2 = NULL;
        PR_ERR("create display frame buff failed");
        return OPRT_MALLOC_FAILED;
    }
    sg_p_display_fb_1->fmt    = TUYA_PIXEL_FMT_RGB565;
    sg_p_display_fb_1->width  = CAMERA_WIDTH;
    sg_p_display_fb_1->height = CAMERA_HEIGHT;
    sg_p_display_fb_2->fmt    = TUYA_PIXEL_FMT_RGB565;
    sg_p_display_fb_2->width  = CAMERA_WIDTH;
    sg_p_display_fb_2->height = CAMERA_HEIGHT;

    // sg_p_display_fb_1->fmt    = TUYA_PIXEL_FMT_RGB565;
    // sg_p_display_fb_1->width  = sg_display_info.width;
    // sg_p_display_fb_1->height = sg_display_info.height;
    // sg_p_display_fb_2->fmt    = TUYA_PIXEL_FMT_RGB565;
    // sg_p_display_fb_2->width  = sg_display_info.width;
    // sg_p_display_fb_2->height = sg_display_info.height;


    return OPRT_OK;
}


static OPERATE_RET __example_dvp_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TKL_VI_CONFIG_T vi_config;
    TKL_VI_EXT_CONFIG_T ext_conf;

    ext_conf.type = TKL_VI_EXT_CONF_CAMERA;
    ext_conf.camera.camera_type = TKL_VI_CAMERA_TYPE_DVP;
    ext_conf.camera.fmt = TKL_CODEC_VIDEO_H264;

    ext_conf.camera.power_pin = TUYA_GPIO_NUM_51;
    ext_conf.camera.active_level = TUYA_GPIO_LEVEL_HIGH;
    ext_conf.camera.i2c.clk = TUYA_GPIO_NUM_13;
    ext_conf.camera.i2c.sda = TUYA_GPIO_NUM_15;
    ext_conf.camera.fps = 15;
    ext_conf.camera.height = CAMERA_HEIGHT;
    ext_conf.camera.width = CAMERA_WIDTH;

    vi_config.isp.width  = sg_display_info.width;
    vi_config.isp.height = sg_display_info.height;
    vi_config.isp.fps = 15;

    vi_config.pdata = &ext_conf;

    vi_config.disp_cb = __dvp_display_frame_cb;

    TUYA_CALL_ERR_RETURN(tkl_vi_init(&vi_config, 0));
 
    return rt;
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    /*hardware register*/
    board_register_hardware();

    TUYA_CALL_ERR_LOG(__dvp_display_init());

    TUYA_CALL_ERR_LOG(__example_dvp_init());

    while(1) {
        tal_system_sleep(10*1000);
    }

    return;
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif