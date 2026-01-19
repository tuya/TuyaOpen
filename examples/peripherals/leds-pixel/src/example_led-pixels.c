/**
 * @file example_led-pixels.c
 * @brief LED pixel driver example for SDK.
 *
 * This file provides an example implementation of LED pixel control using the Tuya SDK.
 * It demonstrates the configuration and usage of various LED pixel types (WS2812, SK6812, SM16703P, etc.)
 * for creating colorful lighting effects. The example focuses on setting up pixel drivers, managing color
 * sequences, and controlling LED strips with different timing patterns for various applications.
 *
 * The LED pixel driver example is designed to help developers understand how to integrate LED pixel
 * functionality into their Tuya-based IoT projects, enabling dynamic lighting control and visual effects
 * for smart lighting applications.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */
#include "tal_system.h"
#include "tal_api.h"
#include <math.h>

#include "tkl_output.h"

#include "tdl_pixel_dev_manage.h"
#include "tdl_pixel_color_manage.h"

#include "board_com_api.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define LED_PIXELS_TOTAL_NUM 1024
#define LED_CHANGE_TIME      800 // ms
#define COLOR_RESOLUTION     1000
#define COLOR_VAL            10
/***********************************************************
***********************typedef define***********************
***********************************************************/

/*********************************************************** 
***********************variable define**********************
***********************************************************/
static PIXEL_HANDLE_T sg_pixels_handle = NULL;

/***********************************************************
*********************** const define ***********************
***********************************************************/
static const PIXEL_COLOR_T cCOLOR_ARR[] = {
                                            {
                                               // red
                                               .warm = 0,
                                               .cold = 0,
                                               .red = COLOR_VAL,
                                               .green = 0,
                                               .blue = 0,
                                           },
                                           {
                                               // green
                                               .warm = 0,
                                               .cold = 0,
                                               .red = 0,
                                               .green = COLOR_VAL,
                                               .blue = 0,
                                           },
                                           {
                                               // blue
                                               .warm = 0,
                                               .cold = 0,
                                               .red = 0,
                                               .green = 0,
                                               .blue = COLOR_VAL,
                                           }
};

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief user_main
 *
 * @return none
 */
void user_main()
{
    OPERATE_RET rt = OPRT_OK;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    board_register_hardware();

    /*find leds strip pixels device*/
    TUYA_CALL_ERR_LOG(tdl_pixel_dev_find(LEDS_PIXEL_NAME, &sg_pixels_handle));

    /*open leds strip pixels device*/
    PIXEL_DEV_CONFIG_T pixels_cfg = {
        .pixel_num = LED_PIXELS_TOTAL_NUM,
        .pixel_resolution = COLOR_RESOLUTION,
    };
    TUYA_CALL_ERR_LOG(tdl_pixel_dev_open(sg_pixels_handle, &pixels_cfg));

    while(1) {
        for(uint32_t i = 0; i<CNTSOF(cCOLOR_ARR); i++) {
            tdl_pixel_set_single_color_all(sg_pixels_handle, (PIXEL_COLOR_T *)&cCOLOR_ARR[i]);
            tdl_pixel_dev_refresh(sg_pixels_handle);

            tal_system_sleep(500);
        }
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
    (void)arg;
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    /*create example task*/
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";

    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif