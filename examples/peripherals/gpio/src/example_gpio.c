/**
 * @file example_gpio.c
 * @brief GPIO driver example for Tuya IoT projects.
 *
 * This file provides an example implementation of a GPIO (General Purpose Input/Output) driver using the Tuya SDK.
 * It demonstrates the configuration and usage of GPIO pins for input, output, and interrupt-driven events.
 * The example covers initializing GPIO pins in different modes, writing to output pins, reading from input pins, and
 * setting up interrupts on GPIO pins to handle external events.
 *
 * The GPIO driver example aims to help developers understand how to control hardware peripherals and manage
 * event-driven programming in Tuya IoT projects. It includes detailed examples of setting up GPIO configurations,
 * handling interrupts with callbacks, and integrating these functionalities within a task-based application structure.
 *
 * @note This example is designed to be adaptable to various Tuya IoT devices and platforms, showcasing fundamental GPIO
 * operations that are critical for IoT device development.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_gpio.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define EXAMPLE_IRQ_MODE       TUYA_GPIO_IRQ_RISE

#define TASK_GPIO_PRIORITY     THREAD_PRIO_2
#define TASK_GPIO_SIZE         4096

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE sg_gpio_handle;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief interrupt callback function
 *
 * @param[in] args:parameters
 * @return none
 */
static void __gpio_irq_callback(void *args)
{
    /* Both TAL_PR_ and PR_ have locks in these two types of printing and should not be used in interrupts. */
    tkl_log_output("\r\n------------ GPIO IRQ Callbcak ------------\r\n");
}

/**
 * @brief gpio task
 *
 * @param[in] param:Task parameters
 * @return none
 */
static void __example_gpio_task(void *param)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t i = 0;
    TUYA_GPIO_LEVEL_E read_level = 0;

    /*GPIO output init*/
    TUYA_GPIO_BASE_CFG_T out_pin_cfg = {
        .mode = TUYA_GPIO_PUSH_PULL, .direct = TUYA_GPIO_OUTPUT, .level = TUYA_GPIO_LEVEL_LOW};
    TUYA_CALL_ERR_LOG(tkl_gpio_init(EXAMPLE_OUTPUT_PIN, &out_pin_cfg));

    /*GPIO input init*/
    TUYA_GPIO_BASE_CFG_T in_pin_cfg = {
        .mode = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
    };
    TUYA_CALL_ERR_LOG(tkl_gpio_init(EXAMPLE_INPUT_PIN, &in_pin_cfg));

    /*GPIO irq init*/
    TUYA_CALL_ERR_LOG(tkl_gpio_init(EXAMPLE_IRQ_PIN, &in_pin_cfg));
    TUYA_GPIO_IRQ_T irq_cfg = {
        .cb = __gpio_irq_callback,
        .arg = NULL,
        .mode = EXAMPLE_IRQ_MODE,
    };
    TUYA_CALL_ERR_LOG(tkl_gpio_irq_init(EXAMPLE_IRQ_PIN, &irq_cfg));

    /*irq enable*/
    TUYA_CALL_ERR_LOG(tkl_gpio_irq_enable(EXAMPLE_IRQ_PIN));

    while (1) {
        /* GPIO output */
        if (i == 0) {
            tkl_gpio_write(EXAMPLE_OUTPUT_PIN, TUYA_GPIO_LEVEL_HIGH);
            PR_DEBUG("pin output high");
        } else {
            tkl_gpio_write(EXAMPLE_OUTPUT_PIN, TUYA_GPIO_LEVEL_LOW);
            PR_DEBUG("pin output low");
        }
        i = i ^ 1;

        /* GPIO read */
        TUYA_CALL_ERR_LOG(tkl_gpio_read(EXAMPLE_INPUT_PIN, &read_level));
        if (read_level == 1) {
            PR_DEBUG("GPIO read high level");
        } else {
            PR_DEBUG("GPIO read low level");
        }

        tal_system_sleep(2000);
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
    thrd_param.stackDepth = TASK_GPIO_SIZE;
    thrd_param.priority = TASK_GPIO_PRIORITY;
    thrd_param.thrdname = "gpio";
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_gpio_handle, NULL, NULL, __example_gpio_task, NULL, &thrd_param));

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
