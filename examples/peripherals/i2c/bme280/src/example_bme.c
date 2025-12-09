/**
 * @file example_bme.c
 * @brief Example implementation of an I2C driver for Tuya IoT projects.
 *
 * This file provides an example implementation of an I2C driver using the Tuya SDK.
 * It demonstrates the configuration and usage of I2C communication for reading and writing data to an I2C device.
 * The example covers initializing the I2C interface, sending commands to the device, and reading data from the device.
 *
 * The I2C driver example aims to help developers understand how to communicate with I2C devices in Tuya IoT projects.
 * It includes detailed examples of setting up I2C configurations, sending commands, and reading data from I2C devices.
 *
 * @note This example is designed to be adaptable to various Tuya IoT devices and platforms, showcasing fundamental I2C
 * operations that are critical for IoT device development.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_gpio.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"
#include "bme280_driver.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define TASK_GPIO_PRIORITY         THREAD_PRIO_2
#define TASK_GPIO_SIZE             4096

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE sg_i2c_handle;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief sensor_init
 *
 * @param[in] param:none
 * @return none
 */
void sensor_init(void)
{
    PR_DEBUG("app_init");

    /* ----- 1. BME280 Init ----- */
    OPERATE_RET ret = bme280_init();
    if (ret != OPRT_OK) {
        PR_ERR("BME280 init error %d", ret);
    } else {
        PR_DEBUG("BME280 init OK");
    }
    
    /* ----- 2. BME280 Config ----- */
    ret = bme280_config(BME280_OSRS_X2, BME280_OSRS_X2, BME280_OSRS_X1, BME280_FILTER_4);
    if (ret != OPRT_OK) {
        PR_ERR("BME280 config error %d", ret);
    } else {
        PR_DEBUG("BME280 config OK");
    }
}

/**
 * @brief i2c task
 *
 * @param[in] param:Task parameters
 * @return none
 */
static void __example_i2c_task(void *param)
{

    BME280_DATA_T env;
    int32_t temp_x10;
    uint32_t humi_x10;
    uint32_t pressure_hpa_x10;

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

    sensor_init();

    while (1) {
        tal_system_sleep(5000);

        if (bme280_read(&env) == OPRT_OK) {
            temp_x10 = env.temperature_x10;
            humi_x10 = env.humidity_x10;
            pressure_hpa_x10 = env.pressure_hpa_x10;
            PR_DEBUG("🌡️ BME280: T=%d.%d°C, H=%d.%d%%, P=%d.%dhPa",
                    temp_x10/10, temp_x10%10, 
                    humi_x10/10, humi_x10%10,
                    pressure_hpa_x10/10, pressure_hpa_x10%10);
        }

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
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    static THREAD_CFG_T thrd_param = {.priority = TASK_GPIO_PRIORITY, .stackDepth = TASK_GPIO_SIZE, .thrdname = "i2c"};
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_i2c_handle, NULL, NULL, __example_i2c_task, NULL, &thrd_param));

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
