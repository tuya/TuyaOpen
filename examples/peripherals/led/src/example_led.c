/**
 * @file example_led.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tkl_output.h"

#include "tdl_led_manage.h"
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
static TDL_LED_HANDLE_T sg_led_hdl = NULL;

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

    tal_sw_timer_init();

    /*hardware register*/
    board_register_hardware();

    sg_led_hdl = tdl_led_find_dev(LED_NAME);
    TUYA_CALL_ERR_LOG(tdl_led_open(sg_led_hdl));

    TUYA_CALL_ERR_LOG(tdl_led_set_status(sg_led_hdl, TDL_LED_ON));
    tal_system_sleep(2000);

    TUYA_CALL_ERR_LOG(tdl_led_set_status(sg_led_hdl, TDL_LED_OFF));
    tal_system_sleep(2000);

    TDL_LED_BLINK_CFG_T blink_cfg = {
        .cnt                    = 5,
        .start_stat             = TDL_LED_ON,
        .first_half_cycle_time  = 300,
        .latter_half_cycle_time = 300,
    };
    TUYA_CALL_ERR_LOG(tdl_led_blink(sg_led_hdl, &blink_cfg));
    PR_NOTICE("LED will blink for 5 times with 300ms half cycle time");

    tal_system_sleep(6000);

    TUYA_CALL_ERR_LOG(tdl_led_flash(sg_led_hdl, 1000));
    PR_NOTICE("LED will flash with 1s half cycle time");

    while (1) {
        tal_system_sleep(2000);
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
    thrd_param.stackDepth   = 1024 * 4;
    thrd_param.priority     = THREAD_PRIO_1;
    thrd_param.thrdname     = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif

/***********************************************************
***********************function define**********************
***********************************************************/
