/**
 * @file example_tp.c
 * @brief example_tp module is used to tppad
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tkl_output.h"
#include "tal_api.h"

#include "tdl_tp_manage.h"

#include "board_com_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define EXAMPLE_TP_POINT_NUM_MAX  10    /* Maximum number of tp points */

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_TP_HANDLE_T sg_tdl_tp_hdl = NULL;

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
    OPERATE_RET ret = OPRT_OK;
    TDL_TP_POS_T points[EXAMPLE_TP_POINT_NUM_MAX];
    uint8_t point_count = 0;

    /* Basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("========================================");
    PR_NOTICE("    Simple Tp Driver Example");
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

    sg_tdl_tp_hdl = tdl_tp_find_dev(DISPLAY_NAME);
    if (NULL == sg_tdl_tp_hdl) {
        PR_ERR("[COORD] device %s not found", DISPLAY_NAME);
        return;
    }

    ret = tdl_tp_dev_open(sg_tdl_tp_hdl);
    if (ret != OPRT_OK) {
        PR_ERR("[COORD] open failed rt=%d", ret);
        return;
    }

    /* Loop to read tp data */
    while (1) {
        ret = tdl_tp_dev_read(sg_tdl_tp_hdl, EXAMPLE_TP_POINT_NUM_MAX, points, &point_count);
        if (OPRT_OK != ret) {
            PR_ERR("[COORD] read failed rt=%d", ret);
            break;
        }

        if (point_count > 0) {
            /* Iterate and print each tp point */
            for (int i = 0; i < point_count; i++) {
                PR_DEBUG("[COORD] idx=%u x=%d y=%d", i, points[i].x, points[i].y);
            }
            /* Additional gesture or tp event handling can be added here */
        }

        tal_system_sleep(20);  /* Delay to limit polling frequency (50Hz) */
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
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif