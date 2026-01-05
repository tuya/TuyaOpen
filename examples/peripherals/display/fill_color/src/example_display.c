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


/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_DISP_HANDLE_T      sg_tdl_disp_hdl = NULL;
static TDL_DISP_DEV_INFO_T    sg_display_info;
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb = NULL;
/***********************************************************
***********************function define**********************
***********************************************************/
static uint32_t __disp_get_random_color(uint32_t range)
{
    return tal_system_get_random(range);
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t bytes_per_pixel = 0, pixels_per_byte = 0, bpp = 0;
    uint32_t frame_len = 0;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    /*hardware register*/
    board_register_hardware();

    memset(&sg_display_info, 0, sizeof(TDL_DISP_DEV_INFO_T));

    sg_tdl_disp_hdl = tdl_disp_find_dev(DISPLAY_NAME);
    if(NULL == sg_tdl_disp_hdl) {
        PR_ERR("display dev %s not found", DISPLAY_NAME);
        return;
    }

    rt = tdl_disp_dev_get_info(sg_tdl_disp_hdl, &sg_display_info);
    if(rt != OPRT_OK) {
        PR_ERR("get display dev info failed, rt: %d", rt);
        return;
    }

    rt = tdl_disp_dev_open(sg_tdl_disp_hdl);
    if(rt != OPRT_OK) {
        PR_ERR("open display dev failed, rt: %d", rt);
        return;
    }

    tdl_disp_set_brightness(sg_tdl_disp_hdl, 100); // Set brightness to 100%

    /*get frame len*/
    bpp = tdl_disp_get_fmt_bpp(sg_display_info.fmt);
    if (bpp == 0) {
        PR_ERR("Unsupported pixel format: %d", sg_display_info.fmt);
        return;
    }
    if(bpp < 8) {
        pixels_per_byte = 8 / bpp; // Calculate pixels per byte
        frame_len = (sg_display_info.width + pixels_per_byte - 1) / pixels_per_byte * sg_display_info.height;
    }else {
        bytes_per_pixel = (bpp+7) / 8; // Calculate bytes per pixel
        frame_len = sg_display_info.width * sg_display_info.height * bytes_per_pixel;
    }

    /*create frame buffer*/
    sg_p_display_fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if(NULL == sg_p_display_fb) {
        PR_ERR("create display frame buff failed");
        return;
    }
    sg_p_display_fb->x_start = 0;
    sg_p_display_fb->y_start = 0;
    sg_p_display_fb->fmt    = sg_display_info.fmt;
    sg_p_display_fb->width  = sg_display_info.width;
    sg_p_display_fb->height = sg_display_info.height;


    while(1) {
        tdl_disp_draw_fill_full(sg_p_display_fb, __disp_get_random_color(0xFFFFFFFF), sg_display_info.is_swap);

        tdl_disp_dev_flush(sg_tdl_disp_hdl, sg_p_display_fb);

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
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif