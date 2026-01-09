/**
 * @file tuya_main.c
 * @brief Main application entry for hello world example
 *
 * This file provides the main entry point for the hello world example.
 * It initializes LVGL and the UI components.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"

#if defined(ENABLE_LIBLVGL) && (ENABLE_LIBLVGL == 1)
#include "lvgl.h"
#include "lv_vendor.h"
#include "board_com_api.h"
#include "ui_hello_world.h"
#endif

/***********************************************************
*********************** macro define ***********************
***********************************************************/

/***********************************************************
********************** typedef define **********************
***********************************************************/

/***********************************************************
********************** variable define *********************
***********************************************************/

/***********************************************************
********************** function define *********************
***********************************************************/

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

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

#if defined(ENABLE_LIBLVGL) && (ENABLE_LIBLVGL == 1)
    /* hardware register */
    board_register_hardware();

    /* Initialize LVGL vendor */
    lv_vendor_init(DISPLAY_NAME);

    lv_vendor_disp_lock();

    /* Initialize hello world UI */
    ui_hello_world_init();

    lv_vendor_disp_unlock();

    /* Start LVGL task */
    lv_vendor_start(5, 1024 * 8);

    PR_NOTICE("LVGL hello world example started");
#else
    PR_ERR("LVGL is not enabled. Please enable CONFIG_ENABLE_LIBLVGL in app_default.config");
#endif
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

