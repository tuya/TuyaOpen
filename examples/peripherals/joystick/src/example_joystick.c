/**
 * @file example_joystick.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tkl_output.h"
#include "tal_api.h"

#include "tdl_joystick_manage.h"

#include "board_com_api.h"

/***********************************************************
************************macro define************************
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
static void __stick_function_cb(char *name, TDL_JOYSTICK_TOUCH_EVENT_E event, void *argc)
{
    switch (event) {
    case TDL_JOYSTICK_BUTTON_PRESS_DOWN: {
        PR_NOTICE("%s: press down", name);
    } break;

    case TDL_JOYSTICK_BUTTON_LONG_PRESS_HOLD: {
        PR_NOTICE("%s: press long hold", name);
    } break;

    case TDL_JOYSTICK_UP: {
        PR_NOTICE("%s: up", name);
    } break;

    case TDL_JOYSTICK_LEFT: {
        PR_NOTICE("%s: left", name);
    } break;

    case TDL_JOYSTICK_RIGHT: {
        PR_NOTICE("%s: right", name);
    } break;

    case TDL_JOYSTICK_DOWN: {
        PR_NOTICE("%s: down", name);
    } break;

    case TDL_JOYSTICK_LONG_UP: {
        PR_NOTICE("%s: long up", name);
    } break;

    case TDL_JOYSTICK_LONG_DOWN: {
        PR_NOTICE("%s: long down", name);
    } break;

    case TDL_JOYSTICK_LONG_LEFT: {
        PR_NOTICE("%s: long left", name);
    } break;

    case TDL_JOYSTICK_LONG_RIGHT: {
        PR_NOTICE("%s: long right", name);
    } break;

    default:
        break;
    }
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main(void)
{
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

    TDL_JOYSTICK_CFG_T joystick_cfg = {
        .button_cfg = {.long_start_valid_time = 3000,
                       .long_keep_timer = 1000,
                       .button_debounce_time = 50,
                       .button_repeat_valid_count = 2,
                       .button_repeat_valid_time = 50},
        .adc_cfg =
            {
                .adc_max_val = 8192,        /* adc max value */
                .adc_min_val = 0,           /* adc min value */
                .normalized_range = 10,     /* normalized range ±10 */
                .sensitivity = 2,           /* sensitivity should < normalized range */
            },
    };

    TDL_JOYSTICK_HANDLE sg_joystick_hdl = NULL;

    tdl_joystick_create(JOYSTICK_NAME, &joystick_cfg, &sg_joystick_hdl);

    tdl_joystick_event_register(sg_joystick_hdl, TDL_JOYSTICK_BUTTON_PRESS_DOWN, __stick_function_cb);
    tdl_joystick_event_register(sg_joystick_hdl, TDL_JOYSTICK_BUTTON_LONG_PRESS_HOLD, __stick_function_cb);
    tdl_joystick_event_register(sg_joystick_hdl, TDL_JOYSTICK_UP, __stick_function_cb);
    tdl_joystick_event_register(sg_joystick_hdl, TDL_JOYSTICK_DOWN, __stick_function_cb);
    tdl_joystick_event_register(sg_joystick_hdl, TDL_JOYSTICK_LEFT, __stick_function_cb);
    tdl_joystick_event_register(sg_joystick_hdl, TDL_JOYSTICK_RIGHT, __stick_function_cb);
    tdl_joystick_event_register(sg_joystick_hdl, TDL_JOYSTICK_LONG_UP, __stick_function_cb);
    tdl_joystick_event_register(sg_joystick_hdl, TDL_JOYSTICK_LONG_DOWN, __stick_function_cb);
    tdl_joystick_event_register(sg_joystick_hdl, TDL_JOYSTICK_LONG_LEFT, __stick_function_cb);
    tdl_joystick_event_register(sg_joystick_hdl, TDL_JOYSTICK_LONG_RIGHT, __stick_function_cb);

    // int adc_value[2] = {0}; /* adc value array, x and y */

    while(1) {
        // tdl_joystick_get_raw_xy(sg_joystick_hdl, &adc_value[0], &adc_value[1]);
        // PR_DEBUG("raw value_X = %d, value_Y = %d", adc_value[0], adc_value[1]);
        // tdl_joystick_calibrated_xy(sg_joystick_hdl,  &adc_value[0], &adc_value[1]);
        // PR_DEBUG("cali  value_X = %d, value_Y = %d", adc_value[0], adc_value[1]);
        tal_system_sleep(1000);
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