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

#include "tkl_output.h"

#if defined(ENABLE_SPI) && (ENABLE_SPI)
#include "tdd_pixel_ws2812.h"
#include "tdd_pixel_sm16703p.h"
#include "tdd_pixel_yx1903b.h"
#endif

#include "tdl_pixel_dev_manage.h"
#include "tdl_pixel_color_manage.h"
#include "example_led-pixels.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define LED_PIXELS_TOTAL_NUM         50
#define LED_CHANGE_TIME              800 //ms
#define COLOR_RESOLUION              1000u

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static PIXEL_HANDLE_T sg_pixels_handle = NULL;
static THREAD_HANDLE  sg_pixels_thrd = NULL;

/***********************************************************
*********************** const define ***********************
***********************************************************/
static const PIXEL_COLOR_T cCOLOR_ARR[] = {
    { // red
        .warm = 0,
        .cold = 0,
        .red = COLOR_RESOLUION,
        .green = 0,
        .blue = 0,
    },
    { // green
        .warm = 0,
        .cold = 0,
        .red = 0,
        .green = COLOR_RESOLUION,
        .blue = 0,
    },
    { // blue
        .warm = 0,
        .cold = 0,
        .red = 0,
        .green = 0,
        .blue = COLOR_RESOLUION,
    },
    { // turn off
        .warm = 0,
        .cold = 0,
        .red = 0,
        .green = 0,
        .blue = 0,
    },
};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __example_pixels_task(void *args)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t i = 0, color_num = CNTSOF(cCOLOR_ARR);

    while(1) {
        TUYA_CALL_ERR_GOTO(tdl_pixel_set_single_color(sg_pixels_handle, 0, LED_PIXELS_TOTAL_NUM, (PIXEL_COLOR_T *)&cCOLOR_ARR[i]), __ERROR);

        TUYA_CALL_ERR_GOTO(tdl_pixel_dev_refresh(sg_pixels_handle), __ERROR);

        i = (i + 1) % color_num;
        tal_system_sleep(1000);
        PR_DEBUG("change to color %d", i);
    }

__ERROR:
    PR_ERR("pixels demo error exit");
    tal_thread_delete(sg_pixels_thrd);
    sg_pixels_thrd = NULL;

    return;
}

/**
 * @brief    register hardware 
 *
 * @param[in] : the name of the driver
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET reg_pixels_hardware(char *device_name)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(ENABLE_SPI) && (ENABLE_SPI)
    PIXEL_DRIVER_CONFIG_T dev_init_cfg = {
        .port     = TUYA_SPI_NUM_0,
        .line_seq = RGB_ORDER,
    };

    // Register WS2812 by default. If using other drivers, please replace with other chip driver interfaces 
    TUYA_CALL_ERR_RETURN(tdd_ws2812_driver_register(device_name, &dev_init_cfg));

    // TUYA_CALL_ERR_RETURN(tdd_ws2812_opt_driver_register(device_name, &dev_init_cfg));

    // TUYA_CALL_ERR_RETURN(tdd_ws2814_driver_register(device_name, &dev_init_cfg));

    // TUYA_CALL_ERR_RETURN(tdd_sk6812_driver_register(device_name, &dev_init_cfg));

    // TUYA_CALL_ERR_RETURN(tdd_sm16703p_driver_register(device_name, &dev_init_cfg));

    // TUYA_CALL_ERR_RETURN(tdd_sm16704pk_driver_register(device_name, &dev_init_cfg));

    // TUYA_CALL_ERR_RETURN(tdd_sm16714p_driver_register(device_name, &dev_init_cfg));

    // TUYA_CALL_ERR_RETURN(tdd_yx1903b_driver_register(device_name, &dev_init_cfg));

    return OPRT_OK;
#else 
    return OPRT_NOT_SUPPORTED;
#endif
}

/**
 * @brief    open driver
 *
 * @param[in] : the name of the driver
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET open_pixels_driver(char *device_name)
{
    OPERATE_RET rt = OPRT_OK;

    /*find leds strip pixels device*/
    TUYA_CALL_ERR_RETURN(tdl_pixel_dev_find(device_name, &sg_pixels_handle));

    /*open leds strip pixels device*/
    PIXEL_DEV_CONFIG_T pixels_cfg = {
        .pixel_num        = LED_PIXELS_TOTAL_NUM,
        .pixel_resolution = COLOR_RESOLUION,
    };
    TUYA_CALL_ERR_RETURN(tdl_pixel_dev_open(sg_pixels_handle, &pixels_cfg));

    return OPRT_OK;
}

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

    reg_pixels_hardware("pixel");

    open_pixels_driver("pixel");

    /*create example task*/
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_pixels_thrd, NULL, NULL, __example_pixels_task, NULL, &thrd_param));

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
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif