/**
 * @file example_i2c_scan.c
 * @brief Example implementation of an I2C scan for Tuya IoT projects.
 *
 * This file provides an example implementation of an I2C scan using the Tuya SDK.
 * It demonstrates how to scan the I2C bus for connected devices by iterating through possible
 * 7-bit addresses and checking for device presence.
 *
 * @note This example is designed to be adaptable to various Tuya IoT devices and platforms,
 * showcasing fundamental I2C operations that are critical for IoT device development.
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
#define TASK_GPIO_PRIORITY THREAD_PRIO_2
#define TASK_GPIO_SIZE     4096

#define SCAN_TEST_SIZE 0
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

OPERATE_RET __i2c_scan()
{
    OPERATE_RET op_ret = OPRT_COM_ERROR;
    uint8_t i2c_addr = 0;
    uint8_t dev_num = 0;
    for (i2c_addr = 0X00; i2c_addr <= 0X7F; i2c_addr++) {
        uint8_t data_buf[1] = {0};
        if (OPRT_OK == tkl_i2c_master_send(EXAMPLE_I2C_PORT, i2c_addr, data_buf, SCAN_TEST_SIZE, TRUE)) {
            dev_num++;
            if (dev_num >= i2c_addr) {
                op_ret = OPRT_INVALID_PARM;
                continue;
            }
            PR_NOTICE("i2c device found at address: 0x%02X", i2c_addr);
            op_ret = OPRT_OK;
        }
    }
    return op_ret;
}
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

    if(TUYA_I2C_NUM_0 == EXAMPLE_I2C_PORT) {
        tkl_io_pinmux_config(EXAMPLE_I2C_SCL_PIN, TUYA_IIC0_SCL);
        tkl_io_pinmux_config(EXAMPLE_I2C_SDA_PIN, TUYA_IIC0_SDA);
    } else if(TUYA_I2C_NUM_1 == EXAMPLE_I2C_PORT) {
        tkl_io_pinmux_config(EXAMPLE_I2C_SCL_PIN, TUYA_IIC1_SCL);
        tkl_io_pinmux_config(EXAMPLE_I2C_SDA_PIN, TUYA_IIC1_SDA);
    }else if(TUYA_I2C_NUM_3 == EXAMPLE_I2C_PORT) {
        tkl_io_pinmux_config(EXAMPLE_I2C_SCL_PIN, TUYA_IIC2_SCL);
        tkl_io_pinmux_config(EXAMPLE_I2C_SDA_PIN, TUYA_IIC2_SDA);
    }

    /*i2c init*/
    cfg.role = TUYA_IIC_MODE_MASTER;
    cfg.speed = TUYA_IIC_BUS_SPEED_100K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    op_ret = tkl_i2c_init(EXAMPLE_I2C_PORT, &cfg);
    if (OPRT_OK != op_ret) {
        PR_ERR("i2c init fail, err<%d>!", op_ret);
    }

    while (1) {
        if (OPRT_OK != __i2c_scan()) {
            PR_ERR("i2c can not find any 7bits address device, please check : \r\n\
                           1、device connection \r\n\
                           2、device power supply \r\n\
                           3、device is good \r\n\
                           4、SCL/SDA pinmux \r\n\
                           5、SCL/SDA pull-up resistor \r\n\
                           6、device support bus speed \r\n");
        }
        tal_system_sleep(1000);
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