/**
 * @file example_camera.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
#include "tkl_dma2d.h"
#endif

#include "board_com_api.h"

#include "tdl_display_manage.h"
#include "tdl_camera_manage.h"
/***********************************************************
*************************micro define***********************
***********************************************************/
#define CAMERA_FPS                20
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
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb = NULL;

static TDL_CAMERA_HANDLE_T sg_tdl_camera_hdl = NULL;

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
static TKL_DMA2D_FRAME_INFO_T sg_in_frame = {0};
static TKL_DMA2D_FRAME_INFO_T sg_out_frame = {0};
static SEM_HANDLE sg_convert_sem;
#endif
/***********************************************************
***********************function define**********************
***********************************************************/
#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
static void __dma2d_irq_cb(TUYA_DMA2D_IRQ_E type, VOID_T *args)
{
    if(sg_convert_sem) {
        tal_semaphore_post(sg_convert_sem);
    }
}

static OPERATE_RET __dma2d_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&sg_convert_sem, 0, 1));

    TUYA_DMA2D_BASE_CFG_T dma2d_cfg = {
        .cb = __dma2d_irq_cb, 
        .arg=NULL,
    };

    return tkl_dma2d_init(&dma2d_cfg);
}
#endif

OPERATE_RET __get_camera_raw_frame_cb(TDL_CAMERA_HANDLE_T hdl,  TDL_CAMERA_FRAME_T *frame)
{
#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
    sg_in_frame.type = TUYA_FRAME_FMT_YUV422;
    sg_in_frame.width = CAMERA_WIDTH;
    sg_in_frame.height = CAMERA_HEIGHT;
    sg_in_frame.axis.x_axis = 0;
    sg_in_frame.axis.y_axis = 0;
    sg_in_frame.width_cp = 0;
    sg_in_frame.height_cp = 0;
    sg_in_frame.pbuf = frame->data;

    sg_out_frame.type = TUYA_FRAME_FMT_RGB565;
    sg_out_frame.width = sg_p_display_fb->width;
    sg_out_frame.height = sg_p_display_fb->height;
    sg_out_frame.axis.x_axis = 0;
    sg_out_frame.axis.y_axis = 0;
    sg_out_frame.width_cp = 0;
    sg_out_frame.height_cp = 0;
    sg_out_frame.pbuf = sg_p_display_fb->frame;

    tkl_dma2d_convert(&sg_in_frame, &sg_out_frame);

    tal_semaphore_wait_forever(sg_convert_sem);

    tdl_disp_dev_flush(sg_tdl_disp_hdl, sg_p_display_fb);
#endif

    return OPRT_OK;
}

static OPERATE_RET __display_init(void)
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
    sg_p_display_fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if(NULL == sg_p_display_fb) {
        PR_ERR("create display frame buff failed");
        return OPRT_MALLOC_FAILED;
    }
    sg_p_display_fb->fmt    = TUYA_PIXEL_FMT_RGB565;
    sg_p_display_fb->width  = CAMERA_WIDTH;
    sg_p_display_fb->height = CAMERA_HEIGHT;

    return OPRT_OK;
}

static OPERATE_RET __camera_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_CAMERA_CFG_T cfg;

    sg_tdl_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    if(NULL == sg_tdl_camera_hdl) {
        PR_ERR("camera dev %s not found", CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    cfg.fps     = CAMERA_FPS;
    cfg.width   = CAMERA_WIDTH;
    cfg.height  = CAMERA_HEIGHT;
    cfg.out_fmt = TDL_CAMERA_FMT_YUV422;
    cfg.get_frame_cb = __get_camera_raw_frame_cb;

    TUYA_CALL_ERR_RETURN(tdl_camera_dev_open(sg_tdl_camera_hdl, &cfg));

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

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
    TUYA_CALL_ERR_LOG(__dma2d_init());
#endif

    TUYA_CALL_ERR_LOG(__display_init());

    TUYA_CALL_ERR_LOG(__camera_init());

    while(1) {
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
    (void) arg;

    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {1024 * 4, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
