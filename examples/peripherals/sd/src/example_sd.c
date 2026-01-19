/**
 * @file example_sd.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_fs.h"

#if defined(EBABLE_EXAMPLE_SD_PINMUX) && (EBABLE_EXAMPLE_SD_PINMUX == 1)
#include "tkl_pinmux.h"
#endif

#include "board_com_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TASK_SD_PRIORITY THREAD_PRIO_2
#define TASK_SD_SIZE     4096

#define SDCARD_MOUNT_PATH "/sdcard"
#define RANDOM_FILE_PATH  "/sdcard/random.txt"

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE sg_sd_thrd_hdl;
static char sg_write_buf[128] = {0};
static char sg_read_buf[128] = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
static void __example_sd_test(void)
{
    int random_value = tal_system_get_random(0xFFFFFFFF);

    snprintf(sg_write_buf, sizeof(sg_write_buf), "random value: %d", random_value);

    TUYA_FILE file_hdl = tkl_fopen(RANDOM_FILE_PATH, "w");
    if (NULL == file_hdl) {
        PR_ERR("Open file %s failed", RANDOM_FILE_PATH);
        return;
    }

    uint32_t write_len = strlen(sg_write_buf);
    PR_NOTICE("Write file content: %s", sg_write_buf);
    uint32_t ret_len = tkl_fwrite(sg_write_buf, write_len, file_hdl);
    if (ret_len != write_len) {
        PR_ERR("Write file %s failed: %d", RANDOM_FILE_PATH, ret_len);
    }

    tkl_fclose(file_hdl);
    file_hdl = NULL;

    file_hdl = tkl_fopen(RANDOM_FILE_PATH, "r");
    if (NULL == file_hdl) {
        PR_ERR("open file %s failed: %d", RANDOM_FILE_PATH, ret_len);
        goto __EXIT;
    }

    // read file
    uint32_t read_len = tkl_fread(sg_read_buf, sizeof(sg_read_buf), file_hdl);
    if (read_len <= 0) {
        PR_ERR("read file %s failed: %d", RANDOM_FILE_PATH, read_len);
        goto __EXIT;
    }

    // compare file
    if (strncmp(sg_write_buf, sg_read_buf, read_len) != 0) {
        PR_ERR("---> fail: compare file failed");
    } else {
        PR_NOTICE("---> success: compare file success");
    }

__EXIT:
    // close file
    tkl_fclose(file_hdl);
    file_hdl = NULL;

    return;
}

/**
 * @brief sd task
 *
 * @param[in] param:Task parameters
 * @return none
 */
static void __example_sd_task(void *param)
{
    OPERATE_RET rt = OPRT_OK;
    
#if defined(EBABLE_EXAMPLE_SD_PINMUX) && (EBABLE_EXAMPLE_SD_PINMUX == 1)
    tkl_io_pinmux_config(EXAMPLE_SD_CLK_PIN, TUYA_SDIO_HOST_CLK);
    tkl_io_pinmux_config(EXAMPLE_SD_CMD_PIN, TUYA_SDIO_HOST_CMD);
    tkl_io_pinmux_config(EXAMPLE_SD_D0_PIN, TUYA_SDIO_HOST_D0);
    tkl_io_pinmux_config(EXAMPLE_SD_D1_PIN, TUYA_SDIO_HOST_D1);
    tkl_io_pinmux_config(EXAMPLE_SD_D2_PIN, TUYA_SDIO_HOST_D2);
    tkl_io_pinmux_config(EXAMPLE_SD_D3_PIN, TUYA_SDIO_HOST_D3);
#endif

    TUYA_CALL_ERR_LOG(tkl_fs_mount(SDCARD_MOUNT_PATH, DEV_SDCARD));
    if (rt != OPRT_OK) {
        PR_ERR("Mount SD card failed: %d", rt);
        while (1) {
            TUYA_CALL_ERR_LOG(tkl_fs_mount(SDCARD_MOUNT_PATH, DEV_SDCARD));
            tal_system_sleep(3 * 1000);
        }

    }

    while (1) {
        __example_sd_test();
        tal_system_sleep(3 * 1000);
    }
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    /*hardware register*/
    board_register_hardware();

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    static THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = TASK_SD_SIZE;
    thrd_param.priority = TASK_SD_PRIORITY;
    thrd_param.thrdname = "sd";
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_sd_thrd_hdl, NULL, NULL, __example_sd_task, NULL, &thrd_param));

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