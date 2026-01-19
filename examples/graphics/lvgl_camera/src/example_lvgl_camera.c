/**
 * @file example_lvgl_camera.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tkl_output.h"

#include "tdl_display_manage.h"
#include "tdl_camera_manage.h"
#include "tdl_button_manage.h"

#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_vendor.h"

#include "board_com_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define DISPLAY_FRAME_BUFF_NUM 2

#define EXAMPLE_CAMERA_FPS    15
#define EXAMPLE_CAMERA_WIDTH  480
#define EXAMPLE_CAMERA_HEIGHT 480
/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static bool sg_is_display_camera = false;
static THREAD_HANDLE sg_lvgl_thrd = NULL;
static TDL_CAMERA_HANDLE_T sg_camera_hdl = NULL;
static TDL_DISP_HANDLE_T sg_tdl_disp_hdl = NULL;
static TDL_DISP_DEV_INFO_T sg_display_info;
static TDL_FB_MANAGE_HANDLE_T sg_fb_manage = NULL;
static uint8_t sg_display_fb_num = DISPLAY_FRAME_BUFF_NUM;
static TDL_BUTTON_HANDLE sg_button_hdl = NULL;
/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __get_camera_jpeg_frame_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    return frame ? OPRT_OK : OPRT_INVALID_PARM;
}

static OPERATE_RET __get_camera_raw_frame_cb(TDL_CAMERA_HANDLE_T hdl, TDL_CAMERA_FRAME_T *frame)
{
    OPERATE_RET rt = OPRT_OK;
    TDL_DISP_FRAME_BUFF_T *target_fb = NULL, *rotat_fb = NULL, *convert_fb = NULL;

    if(false == sg_is_display_camera) {
        return OPRT_OK;
    }

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

static OPERATE_RET __example_camera_display_init(void)
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
        TUYA_CALL_ERR_LOG(tdl_disp_fb_manage_add(sg_fb_manage,sg_display_info.fmt, width, height));
    }

    return OPRT_OK;
}

static OPERATE_RET __example_camera_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Find and open camera device
    sg_camera_hdl = tdl_camera_find_dev(CAMERA_NAME);
    TUYA_CHECK_NULL_RETURN(sg_camera_hdl, OPRT_COM_ERROR);

    // Configure camera
    TDL_CAMERA_CFG_T cfg = {
        .fps                      = EXAMPLE_CAMERA_FPS,
        .width                    = EXAMPLE_CAMERA_WIDTH,
        .height                   = EXAMPLE_CAMERA_HEIGHT,
        .get_frame_cb             = __get_camera_raw_frame_cb,
        .get_encoded_frame_cb     = __get_camera_jpeg_frame_cb,
        .out_fmt                  = TDL_CAMERA_FMT_JPEG_YUV422_BOTH,
        .encoded_quality.jpeg_cfg = {.enable = 1, .max_size = 25, .min_size = 10},
    };

    TUYA_CALL_ERR_LOG(tdl_camera_dev_open(sg_camera_hdl, &cfg));

    return rt;
}

static void __example_lvgl_task(void *arg)
{
    (void)arg;

    uint32_t timer_count = 0;

    lv_vendor_init(DISPLAY_NAME);

    lv_vendor_start(THREAD_PRIO_0, 1024 * 8);

    // lock display, because this task is not lvgl task
    lv_vendor_disp_lock();
    lv_obj_t *screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(screen, lv_color_white(), LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text_fmt(label, "Hello World! %u", timer_count);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_vendor_disp_unlock();

    while (1) {
        timer_count++;
        lv_vendor_disp_lock();
        lv_label_set_text_fmt(label, "Hello World! %u", timer_count);
        lv_vendor_disp_unlock();

        tal_system_sleep(1000);
    }

    return;
}

static OPERATE_RET __example_lvgl_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    THREAD_CFG_T thrd_hdl = {0};
    thrd_hdl.stackDepth   = 1024 * 4;
    thrd_hdl.priority     = THREAD_PRIO_0;
    thrd_hdl.thrdname     = "example_lvgl_task";

    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_lvgl_thrd, NULL, NULL,\
                                                  __example_lvgl_task, NULL, &thrd_hdl));

    return rt;
}

static void __example_button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    (void)name;
    (void)argc;

    switch (event) {
    case TDL_BUTTON_PRESS_SINGLE_CLICK: {
        if (false == sg_is_display_camera) {
            disp_disable_update();
            sg_is_display_camera = true;
        } else {
            sg_is_display_camera = false;
            disp_enable_update();
        }
    } break;
    default: {
        PR_DEBUG("button event: %d not register", event);
    } break;
    }

    return;
}

static OPERATE_RET __example_button_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TDL_BUTTON_CFG_T button_cfg;

    memset(&button_cfg, 0, sizeof(TDL_BUTTON_CFG_T));
    button_cfg.long_start_valid_time = 3000;
    button_cfg.long_keep_timer       = 1000;
    button_cfg.button_debounce_time  = 50;

    TUYA_CALL_ERR_LOG(tdl_button_create(BUTTON_NAME, &button_cfg, &sg_button_hdl));

    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_SINGLE_CLICK, __example_button_function_cb);

    return rt;
}
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    tal_sw_timer_init();
    tal_workq_init();

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

    TUYA_CALL_ERR_LOG(__example_button_init());

    TUYA_CALL_ERR_LOG(__example_lvgl_init());

    TUYA_CALL_ERR_LOG(__example_camera_init());

    TUYA_CALL_ERR_LOG(__example_camera_display_init());

    while (1) {
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
        int psram_free = tal_psram_get_free_heap_size();
        int sram_free = tal_system_get_free_heap_size();

        PR_DEBUG("psram free: %d, sram free: %d", psram_free, sram_free);
#else 
        int sram_free = tal_system_get_free_heap_size();

        PR_DEBUG("sram free: %d", sram_free);
#endif

        tal_system_sleep(3 * 1000);
    }

    return;
}

#if OPERATING_SYSTEM == SYSTEM_LINUX

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
void main(int argc, char *argv[])
{
    user_main();
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
    THREAD_CFG_T thrd_param;
    
    memset(&thrd_param, 0, sizeof(THREAD_CFG_T));
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";

    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif