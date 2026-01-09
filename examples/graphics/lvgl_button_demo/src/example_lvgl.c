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

#include "lvgl.h"
#include "demos/lv_demos.h"
#include "examples/lv_examples.h"
#include "lv_vendor.h"
#include "board_com_api.h"
/***********************************************************
*************************micro define***********************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

/* 
 * Change background color between blue and white
 * Each button click toggles the background color once
 */
 void button_event_cb(lv_event_t * event)
 {
    (void) event;
     static bool is_blue = false;
     
     // Get the screen object
     lv_obj_t * screen = lv_screen_active();
     
     if (is_blue) {
         // Switch to white background
         lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFFFFF), 0);
         is_blue = false;
     } else {
         // Switch to blue background
         lv_obj_set_style_bg_color(screen, lv_color_hex(0x0000FF), 0);
         is_blue = true;
     }
 }
 
 void lvgl_demo_button()
 {
     // Create a button
     lv_obj_t * btn = lv_btn_create(lv_screen_active());
     lv_obj_set_size(btn, 120, 50);
     lv_obj_center(btn);
     
     lv_obj_t * label = lv_label_create(btn);
     lv_label_set_text(label, "Button");
     lv_obj_center(label);
 
     lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, NULL);
 }

/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    /*hardware register*/
    board_register_hardware();

    lv_vendor_init(DISPLAY_NAME);

    lv_vendor_disp_lock();

    lvgl_demo_button();
    
    lv_vendor_disp_unlock();

    lv_vendor_start(5, 1024*8);
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
    THREAD_CFG_T thrd_param = {1024 * 4, 4, "tuya_app_main",0};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif