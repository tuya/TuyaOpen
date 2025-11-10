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
#include "tdl_touch_driver.h"
#include "board_com_api.h"
#include "tdl_touch_manage.h"

/***********************************************************
*************************micro define***********************
***********************************************************/

#define MULTI_TOUCH_CHANNEL_MASK  0xCF3F // Exclude channels 6,7,12,13
#define USER_TOUCH_ID             1
#define SINGLE_TOUCH_CHANNEL_MASK (1 << USER_TOUCH_ID) // Single channel test uses channel 1

#define TOUCH_EXAMPLE_MODE_SIMPLE   1   /* Read filtered single-channel raw capacitance */
#define TOUCH_EXAMPLE_MODE_COORD    2   /* Read multi-point coordinates from touch screen */

#define TOUCH_EXAMPLE_MODE TOUCH_EXAMPLE_MODE_COORD

#ifndef TOUCH_EXAMPLE_MODE
#define TOUCH_EXAMPLE_MODE TOUCH_EXAMPLE_MODE_SIMPLE
#endif

#define TOUCH_SUPPORT_MAX_NUM 3    /* Maximum number of touch points supported in coordinate mode */

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDD_TOUCH_DEV_HANDLE_T sg_tdl_touch_hdl = NULL;

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
static void touch_event_callback(UINT32_T channel, TUYA_TOUCH_EVENT_E event, VOID *arg)
{
    switch (event) {
    case TUYA_TOUCH_EVENT_PRESSED:
        PR_NOTICE("*** TOUCH EVENT PRESSD DOWN *** Channel %d", channel);
        break;

    case TUYA_TOUCH_EVENT_RELEASED:
        PR_NOTICE("*** TOUCH EVENT RELEASED UP *** Channel %d", channel);
        break;
    case TUYA_TOUCH_EVENT_LONG_PRESS:
        PR_NOTICE("*** TOUCH EVENT LONG PRESSED *** Channel %d", channel);
        break;
    default:
        break;
    }
}

/**
 * @brief run_coord_mode
 *
 * @param[in] none
 * @return none
 */
static void run_coord_mode(void)
{
    OPERATE_RET ret = OPRT_OK;
    TDL_TOUCH_POS_T points[10];  /* Support up to 10 touch points */
    uint8_t point_count = 0;

    sg_tdl_touch_hdl = tdl_touch_find_dev(DISPLAY_NAME);
    if (NULL == sg_tdl_touch_hdl) {
        PR_ERR("[COORD] device %s not found", DISPLAY_NAME);
        return;
    }

    ret = tdl_touch_dev_open(sg_tdl_touch_hdl);
    if (ret != OPRT_OK) {
        PR_ERR("[COORD] open failed rt=%d", ret);
        return;
    }

    /* Loop to read touch data */
    while (1) {
        ret = tdl_touch_dev_read(sg_tdl_touch_hdl, TOUCH_SUPPORT_MAX_NUM, points, &point_count);
        if (OPRT_OK != ret) {
            PR_ERR("[COORD] read failed rt=%d", ret);
            break;
        }

        if (point_count > 0) {
            /* Iterate and print each touch point */
            for (int i = 0; i < point_count; i++) {
                PR_DEBUG("[COORD] idx=%u x=%d y=%d", i, points[i].x, points[i].y);
            }
            /* Additional gesture or touch event handling can be added here */
        }

        tal_system_sleep(20);  /* Delay to limit polling frequency (50Hz) */
    }

    /* Close device when no longer needed */
    tdl_touch_dev_close(sg_tdl_touch_hdl);
    sg_tdl_touch_hdl = NULL;
}

/**
 * @brief run_simple_mode
 *
 * @param[in] none
 * @return none
 */
static void run_simple_mode(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_TOUCH_CONFIG_T touch_config;
    float median_value = 0.0f;

    touch_config.sensitivity_level = TUYA_TOUCH_SENSITIVITY_LEVEL_3;
    touch_config.detect_threshold = TUYA_TOUCH_DETECT_THRESHOLD_6;
    touch_config.detect_range = TUYA_TOUCH_DETECT_RANGE_8PF;
    touch_config.threshold.touch_static_noise_threshold = 0.7f;
    touch_config.threshold.touch_filter_update_threshold = 0.6f;
    touch_config.threshold.touch_detect_threshold = 0.4f;
    touch_config.threshold.touch_variance_threshold = 0.1f;

    /* Initialize touch channel and register event callback */
    TUYA_CALL_ERR_LOG(tkl_touch_init(SINGLE_TOUCH_CHANNEL_MASK, &touch_config));
    TUYA_CALL_ERR_LOG(tkl_touch_register_callback(SINGLE_TOUCH_CHANNEL_MASK, touch_event_callback, NULL));

    while (1) {
        rt = tkl_touch_get_single_average_filter_value(USER_TOUCH_ID, &median_value);
        if (rt == OPRT_OK) {
            PR_DEBUG("[SIMPLE] touch channel [%d] cap value: %f", USER_TOUCH_ID, median_value);
        } else {
            PR_ERR("[SIMPLE] read failed rt=%d", rt);
        }
        tal_system_sleep(500);
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
    /* Basic init */
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

    board_register_hardware();

    if (TOUCH_EXAMPLE_MODE == TOUCH_EXAMPLE_MODE_SIMPLE)
        run_simple_mode();
    else if (TOUCH_EXAMPLE_MODE == TOUCH_EXAMPLE_MODE_COORD)
        run_coord_mode();
    else
        PR_ERR("Invalid TOUCH_EXAMPLE_MODE");
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