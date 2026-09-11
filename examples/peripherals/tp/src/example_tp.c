/**
 * @file example_tp.c
 * @brief example_tp module is used to tppad
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tkl_output.h"
#include "tal_api.h"

#include "tdl_tp_manage.h"
#include "tdl_display_manage.h"

#include "board_com_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define EXAMPLE_TP_POINT_NUM_MAX  10    /* Maximum number of tp points */

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_TP_HANDLE_T sg_tdl_tp_hdl = NULL;

/* Background RGB refresh: opens the panel and continuously flushes full frames,
 * so touch reads run against a real refreshing display (same bus contention as
 * lvgl_demo). Without this the tp example never opens the LCD. */
static THREAD_HANDLE sg_flush_thread = NULL;

static void __disp_flush_thread(void *arg)
{
    TDL_DISP_HANDLE_T disp = tdl_disp_find_dev(DISPLAY_NAME);
    if (NULL == disp) {
        PR_ERR("[FLUSH] display %s not found", DISPLAY_NAME);
        return;
    }

    if (OPRT_OK != tdl_disp_dev_open(disp)) {
        PR_ERR("[FLUSH] display open failed");
        return;
    }

    TDL_DISP_DEV_INFO_T info = {0};
    tdl_disp_dev_get_info(disp, &info);

    /* RGB565 full frame */
    uint32_t len = (uint32_t)info.width * info.height * 2;
    TDL_DISP_FRAME_BUFF_T *fb = tdl_disp_create_frame_buff(DISP_FB_TP_PSRAM, len);
    if (NULL == fb) {
        PR_ERR("[FLUSH] frame buff alloc failed");
        return;
    }
    fb->fmt     = info.fmt;
    fb->width   = info.width;
    fb->height  = info.height;
    fb->x_start = 0;
    fb->y_start = 0;
    fb->len     = len;

    PR_NOTICE("[FLUSH] refreshing %dx%d, len=%u", info.width, info.height, len);

    /* create_frame_buff does NOT set free_cb, so __esp_rgb_flush never frees
     * this buffer (free_cb==NULL). Good: we allocate it once and reuse it for
     * every flush. draw_bitmap copies synchronously (768KB CPU memcpy into the
     * scanning fb, returns when done), so repainting after the flush returns is
     * always safe. Use a slow cadence so the flush never starves touch reads. */
    uint16_t color = 0;
    while (1) {
        uint16_t *px = (uint16_t *)fb->frame;
        uint32_t  n  = len / 2;
        for (uint32_t i = 0; i < n; i++) {
            px[i] = color;
        }
        color += 0x0821; /* drift the color so the screen visibly changes */

        tdl_disp_dev_flush(disp, fb);   /* synchronous 768KB copy into scanning fb */
        PR_NOTICE("[FLUSH] alive, color=0x%04x", color);
        tal_system_sleep(1000);         /* 1 Hz repaint: plenty of headroom */
    }
}

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    OPERATE_RET ret = OPRT_OK;
    TDL_TP_POS_T points[EXAMPLE_TP_POINT_NUM_MAX];
    uint8_t point_count = 0;

    /* Basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("========================================");
    PR_NOTICE("    Simple Tp Driver Example");
    PR_NOTICE("========================================");
    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);
    PR_NOTICE("========================================");

    board_register_hardware();

    /* Start background RGB refresh so touch reads share the bus with a live
     * panel (approximates lvgl_demo's contention). */
    THREAD_CFG_T flush_cfg = {0};
    flush_cfg.stackDepth = 1024 * 4;
    flush_cfg.priority   = THREAD_PRIO_2;
    flush_cfg.thrdname   = "disp_flush";
    tal_thread_create_and_start(&sg_flush_thread, NULL, NULL, __disp_flush_thread, NULL, &flush_cfg);

    sg_tdl_tp_hdl = tdl_tp_find_dev(DISPLAY_NAME);
    if (NULL == sg_tdl_tp_hdl) {
        PR_ERR("[COORD] device %s not found", DISPLAY_NAME);
        return;
    }

    ret = tdl_tp_dev_open(sg_tdl_tp_hdl);
    if (ret != OPRT_OK) {
        PR_ERR("[COORD] open failed rt=%d", ret);
        return;
    }
    PR_NOTICE("[INIT] TOUCH open done (GT1151 I2C up)");

    /* Loop to read tp data */
    while (1) {
        ret = tdl_tp_dev_read(sg_tdl_tp_hdl, EXAMPLE_TP_POINT_NUM_MAX, points, &point_count);
        if (OPRT_OK != ret) {
            PR_ERR("[COORD] read failed rt=%d", ret);
            break;
        }

        if (point_count > 0) {
            /* Iterate and print each tp point */
            for (int i = 0; i < point_count; i++) {
                PR_NOTICE("[COORD] idx=%u x=%d y=%d", i, points[i].x, points[i].y);
            }
            /* Additional gesture or tp event handling can be added here */
        } else {
            /* Heartbeat so we can tell "read OK but no touch" from a stuck read.
             * Print sparsely (every ~1s at 50Hz) to avoid flooding. */
            static uint32_t idle = 0;
            if (++idle >= 50) {
                idle = 0;
                PR_NOTICE("[COORD] read ok, no touch (count=0)");
            }
        }

        tal_system_sleep(20);  /* Delay to limit polling frequency (50Hz) */
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