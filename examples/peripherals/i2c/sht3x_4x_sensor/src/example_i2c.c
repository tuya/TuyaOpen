/**
 * @file example_i2c.c
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
 * @brief i2c task
 *
 * @param[in] param:Task parameters
 * @return none
 */
static void __example_i2c_task(void *param)
{
    OPERATE_RET op_ret = OPRT_OK;
    TUYA_IIC_BASE_CFG_T cfg;

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

    tkl_io_pinmux_config(EXAMPLE_I2C_SCL_PIN, TUYA_IIC0_SCL);
    tkl_io_pinmux_config(EXAMPLE_I2C_SDA_PIN, TUYA_IIC0_SDA);

    /*i2c init*/
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    op_ret = tkl_i2c_init(EXAMPLE_I2C_PORT, &cfg);
    if (OPRT_OK != op_ret) {
        PR_ERR("i2c init fail, err<%d>!", op_ret);
    }

    while (1) {
        tal_system_sleep(2000);

#if defined (EXAMPLE_I2C_SENSOR_SHT3X) && (EXAMPLE_I2C_SENSOR_SHT3X ==1)
        uint16_t temp = 0;
        uint16_t humi = 0;
        extern OPERATE_RET sht3x_read_temp_humi(int port, uint16_t *temp, uint16_t *humi);

        op_ret = sht3x_read_temp_humi(EXAMPLE_I2C_PORT, &temp, &humi);
        if (op_ret != OPRT_OK) {
            PR_ERR("sht3x read fail, err<%d>!", op_ret);
            continue;
        }
        PR_INFO("sht3x temp:%d.%d, humi:%d.%d", temp / 1000, temp % 1000, humi / 1000, humi % 1000);

#elif defined (EXAMPLE_I2C_SENSOR_SHT4X) && (EXAMPLE_I2C_SENSOR_SHT4X ==1)
        uint16_t temp = 0;
        uint16_t humi = 0;
        extern OPERATE_RET sht4x_read_temp_humi(int port, uint16_t *temp, uint16_t *humi);

        op_ret = sht4x_read_temp_humi(EXAMPLE_I2C_PORT, &temp, &humi);
        if (op_ret != OPRT_OK) {
            PR_ERR("sht4x read fail, err<%d>!", op_ret);
            continue;
        }
        PR_INFO("sht4x temp:%d.%d, humi:%d.%d", temp / 1000, temp % 1000, humi / 1000, humi % 1000);
#endif

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

    static THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = TASK_GPIO_SIZE;
    thrd_param.priority = TASK_GPIO_PRIORITY;
    thrd_param.thrdname = "i2c";
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
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif