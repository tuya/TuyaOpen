/**
 * @file example_camera.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"

#include "board_com_api.h"

#include "tdl_display_manage.h"
#include "tdl_camera_manage.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define DISPLAY_FRAME_BUFF_NUM 2

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_DISP_HANDLE_T sg_tdl_disp_hdl = NULL;
static TDL_DISP_DEV_INFO_T sg_display_info;
static TDL_FB_MANAGE_HANDLE_T sg_fb_manage = NULL;
static uint8_t sg_display_fb_num = DISPLAY_FRAME_BUFF_NUM;

static TDL_CAMERA_HANDLE_T sg_tdl_camera_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __get_camera_raw_frame_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_DISP_FRAME_BUFF_T *target_fb = NULL, *rotat_fb = NULL, *convert_fb = NULL;


    convert_fb = tdl_disp_get_free_fb(sg_fb_manage);
    TUYA_CHECK_NULL_RETURN(convert_fb, OPRT_COM_ERROR);

    TUYA_CALL_ERR_LOG(tdl_disp_convert_yuv422_to_framebuffer(frame->data,\
                                                             frame->width,\
                                                             frame->height, \
                                                             convert_fb));

    if (sg_display_info.rotation != TUYA_DISPLAY_ROTATION_0) {
        rotat_fb = tdl_disp_get_free_fb(sg_fb_manage);
        TUYA_CHECK_NULL_RETURN(rotat_fb, OPRT_COM_ERROR);
        
        tdl_disp_draw_rotate(sg_display_info.rotation, convert_fb, rotat_fb, sg_display_info.is_swap);
        if(convert_fb->free_cb) {
            convert_fb->free_cb(convert_fb);
        }

        target_fb = rotat_fb;
    } else {
        if (true == sg_display_info.is_swap) {
            tdl_disp_dev_rgb565_swap((uint16_t *)convert_fb->frame, convert_fb->len / 2);
        }
        target_fb = convert_fb;
    }

    tdl_disp_dev_flush(sg_tdl_disp_hdl, target_fb);

    return rt;
}

static OPERATE_RET __display_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint16_t width = EXAMPLE_CAMERA_WIDTH;
    uint16_t height = EXAMPLE_CAMERA_HEIGHT;

    memset(&sg_display_info, 0, sizeof(TDL_DISP_DEV_INFO_T));

    sg_tdl_disp_hdl = tdl_disp_find_dev(DISPLAY_NAME);
    if (NULL == sg_tdl_disp_hdl) {
        PR_ERR("display dev %s not found", DISPLAY_NAME);
        return OPRT_NOT_FOUND;
    }

    TUYA_CALL_ERR_RETURN(tdl_disp_dev_get_info(sg_tdl_disp_hdl, &sg_display_info));

    if (sg_display_info.fmt != TUYA_PIXEL_FMT_RGB565 && sg_display_info.fmt != TUYA_PIXEL_FMT_MONOCHROME) {
        PR_ERR("display pixel format %d not supported", sg_display_info.fmt);
        return OPRT_NOT_SUPPORTED;
    }

    TUYA_CALL_ERR_RETURN(tdl_disp_dev_open(sg_tdl_disp_hdl));

    tdl_disp_set_brightness(sg_tdl_disp_hdl, 100); // Set brightness to 100%

    TUYA_CALL_ERR_RETURN(tdl_disp_fb_manage_init(&sg_fb_manage));

    /*create frame buffer*/
    if (sg_display_info.rotation != TUYA_DISPLAY_ROTATION_0) {
        sg_display_fb_num = DISPLAY_FRAME_BUFF_NUM + 1;
    }else {
        sg_display_fb_num = DISPLAY_FRAME_BUFF_NUM;
    }

    width  = sg_display_info.width;
    height = sg_display_info.height;

    for(uint8_t i=0; i<sg_display_fb_num; i++) {
        TUYA_CALL_ERR_LOG(tdl_disp_fb_manage_add(sg_fb_manage, sg_display_info.fmt, width, height));
    }

    return OPRT_OK;
}

static OPERATE_RET __camera_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_CAMERA_CFG_T cfg;

    memset(&cfg, 0, sizeof(TDL_CAMERA_CFG_T));

    sg_tdl_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    if (NULL == sg_tdl_camera_hdl) {
        PR_ERR("camera dev %s not found", CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    cfg.fps = EXAMPLE_CAMERA_FPS;
    cfg.width = EXAMPLE_CAMERA_WIDTH;
    cfg.height = EXAMPLE_CAMERA_HEIGHT;
    cfg.out_fmt = TDL_CAMERA_FMT_YUV422;

    cfg.get_frame_cb = __get_camera_raw_frame_cb;

    TUYA_CALL_ERR_RETURN(tdl_camera_dev_open(sg_tdl_camera_hdl, &cfg));

    PR_NOTICE("camera init success");

    return OPRT_OK;
}

/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    /*hardware register*/
    board_register_hardware();

    TUYA_CALL_ERR_LOG(__display_init());

    TUYA_CALL_ERR_LOG(__camera_init());

    while (1) {
        tal_system_sleep(1000);
    }
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
    (void)arg;

    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
