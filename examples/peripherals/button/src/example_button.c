/**
 * @file example_button.c
 * @brief Button input handling example for the Tuya SDK
 *
 * This file provides an example implementation of button input handling using the Tuya SDK.
 * It demonstrates the configuration and usage of button peripherals for detecting user interactions
 * including single clicks, long presses, and other button events. The example shows how to register
 * button drivers, configure button properties, and handle button events through callback functions.
 *
 * The button example is designed to help developers understand how to integrate button functionality
 * into their Tuya-based IoT projects for user interface and control applications.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tkl_output.h"
#include "tal_api.h"

#include "tdl_button_manage.h"

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

static void __button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    switch (event) {
    case TDL_BUTTON_PRESS_DOWN: {
        PR_NOTICE("%s: single click", name);
    } break;

    case TDL_BUTTON_LONG_PRESS_START: {
        PR_NOTICE("%s: long press", name);
    } break;

    default:
        break;
    }
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
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

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

    // button create
    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 500};
    TDL_BUTTON_HANDLE button_hdl = NULL;

    TUYA_CALL_ERR_GOTO(tdl_button_create(BUTTON_NAME, &button_cfg, &button_hdl), __EXIT);

    tdl_button_event_register(button_hdl, TDL_BUTTON_PRESS_DOWN, __button_function_cb);
    tdl_button_event_register(button_hdl, TDL_BUTTON_LONG_PRESS_START, __button_function_cb);

#if defined(BUTTON_NAME_2)
    TDL_BUTTON_HANDLE button_hdl_2 = NULL;

    TUYA_CALL_ERR_GOTO(tdl_button_create(BUTTON_NAME_2, &button_cfg, &button_hdl_2), __EXIT);

    tdl_button_event_register(button_hdl_2, TDL_BUTTON_PRESS_DOWN, __button_function_cb);
    tdl_button_event_register(button_hdl_2, TDL_BUTTON_LONG_PRESS_START, __button_function_cb);
#endif

#if defined(BUTTON_NAME_3)
    TDL_BUTTON_HANDLE button_hdl_3 = NULL;

    TUYA_CALL_ERR_GOTO(tdl_button_create(BUTTON_NAME_3, &button_cfg, &button_hdl_3), __EXIT);

    tdl_button_event_register(button_hdl_3, TDL_BUTTON_PRESS_DOWN, __button_function_cb);
    tdl_button_event_register(button_hdl_3, TDL_BUTTON_LONG_PRESS_START, __button_function_cb);
#endif

#if defined(BUTTON_NAME_4)
    TDL_BUTTON_HANDLE button_hdl_4 = NULL;

    TUYA_CALL_ERR_GOTO(tdl_button_create(BUTTON_NAME_4, &button_cfg, &button_hdl_4), __EXIT);

    tdl_button_event_register(button_hdl_4, TDL_BUTTON_PRESS_DOWN, __button_function_cb);
    tdl_button_event_register(button_hdl_4, TDL_BUTTON_LONG_PRESS_START, __button_function_cb);
#endif

__EXIT:
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
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif