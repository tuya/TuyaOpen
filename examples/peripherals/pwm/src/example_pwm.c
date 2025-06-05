/**
 * @file example_pwm.c
 * @brief PWM driver example for Tuya IoT projects.
 *
 * This file provides an example implementation of a PWM (Pulse Width Modulation) driver using the Tuya SDK.
 * It demonstrates the configuration and usage of PWM for controlling devices like LEDs or motors by varying the duty
 * cycle of the output signal. The example covers initializing a PWM channel, starting and stopping the PWM signal, and
 * dynamically changing the frequency and duty cycle of the PWM output.
 *
 * The PWM driver example aims to help developers understand how to use PWM in their Tuya IoT projects for applications
 * requiring precise control over the power delivered to devices. It includes detailed examples of setting up PWM
 * configurations, handling PWM operations, and integrating these functionalities within a multitasking environment.
 *
 * @note This example is designed to be adaptable to various Tuya IoT devices and platforms, showcasing fundamental PWM
 * operations critical for IoT device development.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_pwm.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define PWM_DUTY          5000 // 50% duty
#define PWM_FREQUENCY     10000
#define TASK_PWM_PRIORITY THREAD_PRIO_2
#define TASK_PWM_SIZE     4096

/***********************************************************
***********************typedef define***********************
***********************************************************/
#define PWM_ID TUYA_PWM_NUM_0

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE sg_pwm_handle;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief pwm task
 *
 * @param[in] param:Task parameters
 * @return none
 */
static void __example_pwm_task(void *param)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t frequency = PWM_FREQUENCY;
    uint32_t count = 0;

    /*pwm init*/
    TUYA_PWM_BASE_CFG_T pwm_cfg = {
        .duty = PWM_DUTY, /* 1-10000 */
        .frequency = PWM_FREQUENCY,
        .polarity = TUYA_PWM_NEGATIVE,
    };
    TUYA_CALL_ERR_GOTO(tkl_pwm_init(PWM_ID, &pwm_cfg), __EXIT);

    /*start PWM3*/
    TUYA_CALL_ERR_GOTO(tkl_pwm_start(PWM_ID), __EXIT);
    PR_NOTICE("PWM%d start", PWM_ID);

    while (1) {
        /*Frequency, duty cycle settings*/
        TUYA_CALL_ERR_LOG(tkl_pwm_frequency_set(PWM_ID, frequency));
        TUYA_CALL_ERR_LOG(tkl_pwm_start(PWM_ID));
        PR_NOTICE("PWM%d , frequency: %d", PWM_ID, frequency);

        /*close pwm*/
        if (count >= 3) {
            break;
        }
        count++;
        frequency = frequency + 10000;

        tal_system_sleep(2000);
    }

    TUYA_CALL_ERR_LOG(tkl_pwm_stop(PWM_ID));

__EXIT:
    PR_NOTICE("PWM task is finished, will delete");
    tal_thread_delete(sg_pwm_handle);
    return;
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main()
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

    /* a pwm thread */
    THREAD_CFG_T pwm_param = {.priority = TASK_PWM_PRIORITY, .stackDepth = TASK_PWM_SIZE, .thrdname = "pwm_task"};
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_pwm_handle, NULL, NULL, __example_pwm_task, NULL, &pwm_param));

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