/**
 * @file example_os_kv.c
 * @brief Demonstrates key-value (KV) storage operations using Tuya's OS abstraction layer.
 *
 * This file provides an example implementation of using Tuya's key-value (KV) storage system.
 * It includes basic operations such as initializing the KV system, writing data to the KV database, reading data back,
 * and deleting keys from the database. The example is structured to showcase how to use the Tuya KV API to perform
 * common KV storage operations, which are essential for IoT applications that need to store configuration settings or
 * other persistent data across reboots.
 *
 * Key features demonstrated in this example:
 * - Initialization of the KV system with custom configuration.
 * - Writing binary data to the KV database.
 * - Reading data back from the KV database.
 * - Deleting keys from the KV database.
 * - Error handling and resource management in KV operations.
 *
 * This example is designed to be easily adaptable to various Tuya IoT applications, providing a starting point for
 * developers to integrate KV storage into their projects.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define KEY_NAME "my_key"

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief user_main
 *
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;
    int i;
    uint8_t *read_buf = NULL;
    size_t read_len;
    BOOL_T is_exist = FALSE;

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

    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });

    PR_NOTICE("------ kv common example start ------");

    /* Write "my_key" value to kv database */
    uint8_t write_buf[] = {0x00, 0x01, 0x02, 0x03};
    TUYA_CALL_ERR_GOTO(tal_kv_set(KEY_NAME, write_buf, CNTSOF(write_buf)), __EXIT);

    /* Output the values written to the KV database */
    PR_NOTICE("kv flash write data:");
    for (i = 0; i < CNTSOF(write_buf); i++) {
        PR_DEBUG_RAW("0x%02x ", write_buf[i]);
    }
    PR_DEBUG_RAW("\r\n\r\n");

    /* read "my_key" value from kv database */
    TUYA_CALL_ERR_GOTO(tal_kv_get(KEY_NAME, &read_buf, &read_len), __EXIT);

    PR_NOTICE("read len %d, read data:", read_len);
    for (i = 0; i < read_len; i++) {
        PR_DEBUG_RAW("0x%02x ", read_buf[i]);
    }
    PR_DEBUG_RAW("\r\n\r\n");

__EXIT:
    /*free data*/
    if (NULL != read_buf) {
        TUYA_CALL_ERR_LOG(tal_kv_free(read_buf));
        read_buf = NULL;
    }

    /*delete key*/
    rt = tal_kv_del(KEY_NAME);
    if (OPRT_OK != rt) {
        PR_ERR("err<%d>,delete fail!", rt);
    } else {
        PR_NOTICE("my_key is deleted in kv database");
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