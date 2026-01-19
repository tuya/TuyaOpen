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
#include "tkl_adc.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define EXAMPLE_ADC_PORT             TUYA_ADC_NUM_0
#define EXAMPLE_ADC_CHANNEL          2
#define EXAMPLE_ADC_MODE             TUYA_ADC_CONTINUOUS
#define EXAMPLE_ADC_TYPE             TUYA_ADC_INNER_SAMPLE_VOL

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TUYA_ADC_BASE_CFG_T sg_adc_cfg = {
    .ch_list.data = 1 << EXAMPLE_ADC_CHANNEL,
    .ch_nums      = 1, // adc Number of channel lists
    .width        = 12,
    .mode         = EXAMPLE_ADC_MODE,
    .type         = EXAMPLE_ADC_TYPE,
    .conv_cnt     = 1,
};

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
    int32_t adc_value = 0;

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

    /* ADC 0 channel 2 init */
    TUYA_CALL_ERR_GOTO(tkl_adc_init(EXAMPLE_ADC_PORT, &sg_adc_cfg), __EXIT);

    TUYA_CALL_ERR_LOG(tkl_adc_read_single_channel(EXAMPLE_ADC_PORT, EXAMPLE_ADC_CHANNEL, &adc_value));
    PR_DEBUG("ADC%d value = %d", EXAMPLE_ADC_PORT, adc_value);

    TUYA_CALL_ERR_LOG(tkl_adc_deinit(EXAMPLE_ADC_PORT));

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