/**
 * @file example_display.c
 * @brief example_display module is used to demonstrate the usage of display peripherals.
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"

#include "tdl_display_manage.h"
#include "tdl_display_draw.h"
#include "board_com_api.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDL_DISP_HANDLE_T       hdl;
    TDL_DISP_DEV_INFO_T     info;
    TDL_DISP_FRAME_BUFF_T  *fb;
}DISP_CTRL_T;
/***********************************************************
***********************variable define**********************
***********************************************************/
static DISP_CTRL_T      sg_disp_ctrl;

#if defined(DISPLAY_NAME_2)
static DISP_CTRL_T      sg_disp_ctrl_2;
#endif

/***********************************************************
***********************function define**********************
***********************************************************/
static uint32_t __disp_get_random_color(uint32_t range)
{
    return tal_system_get_random(range);
}

static OPERATE_RET __disp_init(char *dev_name, DISP_CTRL_T *disp_ctrl)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t bytes_per_pixel = 0, pixels_per_byte = 0, bpp = 0;
    uint32_t frame_len = 0;

    memset(disp_ctrl, 0, sizeof(DISP_CTRL_T));

    disp_ctrl->hdl = tdl_disp_find_dev(dev_name);
    if(NULL == disp_ctrl->hdl) {
        PR_ERR("display dev %s not found", dev_name);
        return OPRT_COM_ERROR;
    }

    rt = tdl_disp_dev_get_info(disp_ctrl->hdl, &disp_ctrl->info);
    if(rt != OPRT_OK) {
        PR_ERR("get display dev info failed, rt: %d", rt);
        return rt;
    }

    rt = tdl_disp_dev_open(disp_ctrl->hdl);
    if(rt != OPRT_OK) {
        PR_ERR("open display dev failed, rt: %d", rt);
        return rt;
    }

    tdl_disp_set_brightness(disp_ctrl->hdl, 100); // Set brightness to 100%

    /*get frame len*/
    bpp = tdl_disp_get_fmt_bpp(disp_ctrl->info.fmt);
    if (bpp == 0) {
        PR_ERR("Unsupported pixel format: %d", disp_ctrl->info.fmt);
        return OPRT_COM_ERROR;
    }
    if(bpp < 8) {
        pixels_per_byte = 8 / bpp; // Calculate pixels per byte
        frame_len = (disp_ctrl->info.width + pixels_per_byte - 1) / pixels_per_byte * disp_ctrl->info.height;
    }else {
        bytes_per_pixel = (bpp+7) / 8; // Calculate bytes per pixel
        frame_len = disp_ctrl->info.width * disp_ctrl->info.height * bytes_per_pixel;
    }

    /*create frame buffer*/
    disp_ctrl->fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if(NULL == disp_ctrl->fb) {
        PR_ERR("create display frame buff failed");
        return OPRT_COM_ERROR;
    }
    disp_ctrl->fb->x_start = 0;
    disp_ctrl->fb->y_start = 0;
    disp_ctrl->fb->fmt    = disp_ctrl->info.fmt;
    disp_ctrl->fb->width  = disp_ctrl->info.width;
    disp_ctrl->fb->height = disp_ctrl->info.height;

    return OPRT_OK;
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
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    /*hardware register*/
    board_register_hardware();

    TUYA_CALL_ERR_LOG(__disp_init(DISPLAY_NAME, &sg_disp_ctrl));

#if defined(DISPLAY_NAME_2)
    TUYA_CALL_ERR_LOG(__disp_init(DISPLAY_NAME_2, &sg_disp_ctrl_2));   
#endif

    while(1) {
        tdl_disp_draw_fill_full(sg_disp_ctrl.fb, __disp_get_random_color(0xFFFFFFFF), sg_disp_ctrl.info.is_swap);

        tdl_disp_dev_flush(sg_disp_ctrl.hdl, sg_disp_ctrl.fb);

#if defined(DISPLAY_NAME_2)
        tdl_disp_draw_fill_full(sg_disp_ctrl_2.fb, __disp_get_random_color(0xFFFFFFFF), sg_disp_ctrl_2.info.is_swap);

        tdl_disp_dev_flush(sg_disp_ctrl_2.hdl, sg_disp_ctrl_2.fb);
#endif

        tal_system_sleep(1000);
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
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif