/**
 * @file example_lvgl.c
 * @brief LVGL (Light and Versatile Graphics Library) example for SDK.
 *
 * This file provides an example implementation of using the LVGL library with the Tuya SDK.
 * It demonstrates the initialization and usage of LVGL for graphical user interface (GUI) development.
 * The example covers setting up the display port, initializing LVGL, and running a demo application.
 *
 * The LVGL example aims to help developers understand how to integrate LVGL into their Tuya IoT projects for
 * creating graphical user interfaces on embedded devices. It includes detailed examples of setting up LVGL,
 * handling display updates, and integrating these functionalities within a multitasking environment.
 *
 * @note This example is designed to be adaptable to various Tuya IoT devices and platforms, showcasing fundamental LVGL
 * operations critical for GUI development on embedded systems.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_spi.h"
#include "tkl_system.h"
#include "tkl_display.h"

#include "tuya_lcd_device.h"
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "lv_port_disp.h"
#if defined(LVGL_ENABLE_TOUCH) || defined(LVGL_ENABLE_ENCODER)
#include "lv_port_indev.h"
#endif

/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TKL_DISP_DEVICE_S sg_lcd_dev = {
    .device_id = 0,
    .device_port = TKL_DISP_LCD,
    .device_info = NULL,
};

/***********************************************************
***********************function define**********************
***********************************************************/

static uint32_t lv_tick_get_cb(void)
{
    return (uint32_t)tkl_system_get_millisecond();
}

/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main()
{
    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    // register lcd device
    tuya_lcd_device_register(sg_lcd_dev.device_id);

    lv_init();
    lv_tick_set_cb(lv_tick_get_cb);
    lv_port_disp_init(&sg_lcd_dev);
#ifdef LVGL_ENABLE_TOUCH
    lv_port_indev_init();
#endif

    /*Create a Demo*/
    lv_demo_widgets();

    while (1) {
        lv_timer_handler();
        tal_system_sleep(5);
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
    THREAD_CFG_T thrd_param = {1024 * 4, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif