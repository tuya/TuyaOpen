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
#include "tdl_display_draw.h"
#include "tdl_camera_manage.h"
/***********************************************************
*************************micro define***********************
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
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb_1 = NULL;
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb_2 = NULL;
static TDL_DISP_FRAME_BUFF_T *sg_p_display_fb_rotat = NULL;

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

OPERATE_RET __get_camera_raw_frame_rgb565_cb(TDL_CAMERA_HANDLE_T hdl,  TDL_CAMERA_FRAME_T *frame)
{
    OPERATE_RET rt = OPRT_OK;

    if(NULL == sg_p_display_fb) {
        return OPRT_COM_ERROR;
    }

#if defined(ENABLE_DMA2D) && (ENABLE_DMA2D == 1)
    TDL_DISP_FRAME_BUFF_T *target_fb = NULL;

    sg_in_frame.type = TUYA_FRAME_FMT_YUV422;
    sg_in_frame.width = frame->width;
    sg_in_frame.height = frame->height;
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

    TUYA_CALL_ERR_RETURN(tkl_dma2d_convert(&sg_in_frame, &sg_out_frame));

    TUYA_CALL_ERR_RETURN(tal_semaphore_wait(sg_convert_sem, 100));

    if(sg_display_info.rotation != TUYA_DISPLAY_ROTATION_0) {
        tdl_disp_draw_rotate(sg_display_info.rotation, sg_p_display_fb, sg_p_display_fb_rotat, sg_display_info.is_swap);
        target_fb = sg_p_display_fb_rotat;
    }else {
        if(true == sg_display_info.is_swap) {
            tdl_disp_dev_rgb565_swap((uint16_t *)sg_p_display_fb->frame, sg_p_display_fb->len/2);
        }
        target_fb = sg_p_display_fb;
    }
    
    tdl_disp_dev_flush(sg_tdl_disp_hdl, target_fb);

    sg_p_display_fb = (sg_p_display_fb == sg_p_display_fb_1) ? \
                      sg_p_display_fb_2 : sg_p_display_fb_1;
#endif

    return rt;
}

/**
 * @brief Converts YUV422 image to binary image using luminance threshold.
 *
 * @param yuv422_data Pointer to YUV422 input data (YUYV format).
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 * @param binary_data Pointer to output binary data buffer.
 * @param threshold Luminance threshold (0-255), pixels >= threshold become white (1).
 * @return int 0 on success, -1 on failure.
 */
int yuv422_to_binary(uint8_t *yuv422_data, int width, int height, 
                     uint8_t *binary_data, uint8_t threshold)
{
    if (!yuv422_data || !binary_data || width <= 0 || height <= 0) {
        return -1;
    }
    
    int binary_stride = (width + 7) / 8; // Bytes per row for packed binary data
    
    // Clear binary output buffer
    memset(binary_data, 0, binary_stride * height);
    
    // Process YUV422 data (YUYV format: Y0 U Y1 V)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Calculate YUV422 data index
            int yuv_index = y * width * 2 + (x / 2) * 4 + (x % 2) * 2;
            uint8_t luminance = yuv422_data[yuv_index]; // Y component
            
            // Apply threshold
            if (luminance >= threshold) {
                // Set corresponding bit in binary data
                int binary_index = y * binary_stride + x / 8;
                int bit_position = x % 8;
                binary_data[binary_index] |= (1 << bit_position);
            }
        }
    }
    
    return 0;
}

int yuv422_to_binary_adaptive(uint8_t *yuv422_data, int width, int height, 
                              uint8_t *binary_data)
{
    if (!yuv422_data || !binary_data || width <= 0 || height <= 0) {
        return -1;
    }
    
    // Calculate average luminance for adaptive threshold
    uint32_t luminance_sum = 0;
    int total_pixels = width * height;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int yuv_index = y * width * 2 + (x / 2) * 4 + (x % 2) * 2;
            luminance_sum += yuv422_data[yuv_index];
        }
    }
    
    uint8_t avg_threshold = luminance_sum / total_pixels;
    
    // Apply adaptive threshold
    return yuv422_to_binary(yuv422_data, width, height, binary_data, avg_threshold);
}

OPERATE_RET __get_camera_raw_frame_mono_cb(TDL_CAMERA_HANDLE_T hdl,  TDL_CAMERA_FRAME_T *frame)
{  
    TDL_DISP_FRAME_BUFF_T *target_fb = NULL;

    if(NULL == hdl || NULL == frame) {
        return OPRT_INVALID_PARM;
    }

    if(NULL == sg_p_display_fb) {
        return OPRT_COM_ERROR;
    }

    yuv422_to_binary_adaptive(frame->data, frame->width, frame->height, sg_p_display_fb->frame);
    
    if(sg_display_info.rotation != TUYA_DISPLAY_ROTATION_0) {
        tdl_disp_draw_rotate(sg_display_info.rotation, sg_p_display_fb, sg_p_display_fb_rotat, sg_display_info.is_swap);
        target_fb = sg_p_display_fb_rotat;
    }else {
        target_fb = sg_p_display_fb;
    }

    tdl_disp_dev_flush(sg_tdl_disp_hdl, target_fb);

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

    if(sg_display_info.fmt != TUYA_PIXEL_FMT_RGB565 &&\
       sg_display_info.fmt != TUYA_PIXEL_FMT_MONOCHROME) {
         PR_ERR("display pixel format %d not supported", sg_display_info.fmt);
        return OPRT_NOT_SUPPORTED;
    }

    TUYA_CALL_ERR_RETURN(tdl_disp_dev_open(sg_tdl_disp_hdl));
 
    tdl_disp_set_brightness(sg_tdl_disp_hdl, 100); // Set brightness to 100%

    /*create frame buffer*/
    if(sg_display_info.fmt == TUYA_PIXEL_FMT_MONOCHROME ) {
        frame_len = (EXAMPLE_CAMERA_WIDTH + 7) / 8 * EXAMPLE_CAMERA_HEIGHT;
    } else {
        frame_len = EXAMPLE_CAMERA_WIDTH * EXAMPLE_CAMERA_HEIGHT * 2; // RGB565 is 2 bytes per pixel
    }
    sg_p_display_fb_1 = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if(NULL == sg_p_display_fb_1) {
        PR_ERR("create display frame buff failed");
        return OPRT_MALLOC_FAILED;
    }
    sg_p_display_fb_1->fmt    = sg_display_info.fmt;
    sg_p_display_fb_1->width  = EXAMPLE_CAMERA_WIDTH;
    sg_p_display_fb_1->height = EXAMPLE_CAMERA_HEIGHT;


    sg_p_display_fb_2 = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
    if(NULL == sg_p_display_fb_2) {
        PR_ERR("create display frame buff failed");
        return OPRT_MALLOC_FAILED;
    }
    sg_p_display_fb_2->fmt    = sg_display_info.fmt;
    sg_p_display_fb_2->width  = EXAMPLE_CAMERA_WIDTH;
    sg_p_display_fb_2->height = EXAMPLE_CAMERA_HEIGHT;

    if(sg_display_info.rotation != TUYA_DISPLAY_ROTATION_0) {
        sg_p_display_fb_rotat = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, frame_len);
        if(NULL == sg_p_display_fb_rotat) {
            PR_ERR("create display frame buff failed");
            return OPRT_MALLOC_FAILED;
        }
        sg_p_display_fb_rotat->fmt    = sg_display_info.fmt;
        sg_p_display_fb_rotat->width  = EXAMPLE_CAMERA_WIDTH;
        sg_p_display_fb_rotat->height = EXAMPLE_CAMERA_HEIGHT;

    } 

    sg_p_display_fb = sg_p_display_fb_1;

    return OPRT_OK;
}

static OPERATE_RET __camera_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_CAMERA_CFG_T cfg;

    memset(&cfg, 0, sizeof(TDL_CAMERA_CFG_T));

    sg_tdl_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    if(NULL == sg_tdl_camera_hdl) {
        PR_ERR("camera dev %s not found", CAMERA_NAME);
        return OPRT_NOT_FOUND;
    }

    cfg.fps     = EXAMPLE_CAMERA_FPS;
    cfg.width   = EXAMPLE_CAMERA_WIDTH;
    cfg.height  = EXAMPLE_CAMERA_HEIGHT;
    cfg.out_fmt = TDL_CAMERA_FMT_YUV422;

    if(sg_display_info.fmt == TUYA_PIXEL_FMT_MONOCHROME) {
        cfg.get_frame_cb =  __get_camera_raw_frame_mono_cb;
    }else {
        cfg.get_frame_cb =  __get_camera_raw_frame_rgb565_cb;
    }

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
