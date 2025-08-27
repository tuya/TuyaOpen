/**
 * @file example_touch.c
 * @brief Simplified Touch driver example for SDK.
 *
 * This file provides a simplified example implementation of Touch driver using the Tuya SDK.
 * It focuses on basic multi-channel touch monitoring without complex timer functionality.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_touch.h"

/***********************************************************
*************************micro define***********************
***********************************************************/

#define TEST_TOUCH_CHANNEL_MASK   0xCF3F            // Exclude channels 6,7,12,13
#define SINGLE_TOUCH_CHANNEL_MASK (1 << 1 | 1 << 3) // Single channel test uses channel 1
#define TOUCH_DEMO                1
/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Touch event callback function
 *
 * @param[in] channel Touch channel number
 * @param[in] event Touch event type
 * @param[in] arg User parameter
 */
#if TOUCH_DEMO
static void touch_event_callback(UINT32_T channel, TUYA_TOUCH_EVENT_E event, VOID *arg)
{
    switch (event) {
    case TUYA_TOUCH_EVENT_DOWN:
        PR_NOTICE("*** TOUCH EVENT PRESSD DOWN *** Channel %d", channel);
        break;

    case TUYA_TOUCH_EVENT_UP:
        PR_NOTICE("*** TOUCH EVENT RELEASED UP *** Channel %d", channel);
        break;

    case TUYA_TOUCH_EVENT_LONG_PRESS:
        PR_NOTICE("*** TOUCH EVENT LONG PRESSED *** Channel %d", channel);
        break;

    default:
        break;
    }
}
#endif
/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
#if TOUCH_DEMO
    OPERATE_RET rt = OPRT_OK;
    TUYA_TOUCH_CONFIG_T touch_config;
#endif
    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("========================================");
    PR_NOTICE("    Simple Touch Driver Example");
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

#if TOUCH_DEMO
    touch_config.sensitivity_level = TUYA_TOUCH_SENSITIVITY_LEVEL_1;
    touch_config.detect_threshold = TUYA_TOUCH_DETECT_THRESHOLD_6;
    touch_config.detect_range = TUYA_TOUCH_DETECT_RANGE_8PF;
    TUYA_CALL_ERR_LOG(tkl_touch_init(SINGLE_TOUCH_CHANNEL_MASK, &touch_config));
    TUYA_CALL_ERR_LOG(tkl_touch_register_callback(SINGLE_TOUCH_CHANNEL_MASK, touch_event_callback, NULL));

#else
    cli_touch_single_channel_calib_mode_test_cmd();
#endif

    while (1) {
        tal_system_sleep(50);
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
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif