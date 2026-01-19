/**
 * @file example_adc.c
 * @brief ADC driver example for SDK.
 *
 * This file provides an example implementation of an ADC (Analog-to-Digital Converter) driver using the Tuya SDK.
 * It demonstrates the configuration and usage of an ADC channel to read analog values and convert them to digital
 * format. The example focuses on setting up a single ADC channel, configuring its properties such as sampling width and
 * mode, and initializing the ADC with these settings for continuous data conversion.
 *
 * The ADC driver example is designed to help developers understand how to integrate ADC functionality into their
 * Tuya-based IoT projects, ensuring accurate analog signal measurement and conversion for various applications.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_flash.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define EXAMPLE_TEST_DATA "tuyaopen flash example test data"

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
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t read_buf[256] = {0};

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

    TUYA_FLASH_BASE_INFO_T flash_info = {0};
    TUYA_CALL_ERR_GOTO(tkl_flash_get_one_type_info(TUYA_FLASH_TYPE_USER0, &flash_info), __EXIT);

    PR_DEBUG("Flash USER0 type info: partition num=%d", flash_info.partition_num);
    for (uint32_t i = 0; i < flash_info.partition_num; i++) {
        PR_DEBUG("Partition %d: start_addr=0x%08x, size=%d/KB, block_size=%d",
                 i,
                 flash_info.partition[i].start_addr,
                 flash_info.partition[i].size / 1024,
                 flash_info.partition[i].block_size);
    }    

    if(0 == flash_info.partition_num) {
        PR_ERR("Flash USER0 partition not found!");
        goto __EXIT;
    }

    TUYA_CALL_ERR_GOTO(tkl_flash_erase(flash_info.partition[0].start_addr, flash_info.partition[0].size), __EXIT);
    tal_system_sleep(200);     
    TUYA_CALL_ERR_GOTO(tkl_flash_write(flash_info.partition[0].start_addr, (const uint8_t *)EXAMPLE_TEST_DATA,\
                                       strlen(EXAMPLE_TEST_DATA)), __EXIT);
    PR_NOTICE("write data: %s", EXAMPLE_TEST_DATA);
    tal_system_sleep(200);                               
    TUYA_CALL_ERR_GOTO(tkl_flash_read(flash_info.partition[0].start_addr, read_buf,\
                                      strlen(EXAMPLE_TEST_DATA)), __EXIT);
    PR_NOTICE("read data: %s", read_buf);

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