/**
 * @file example_button.c
 * @brief button example for SDK.
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

#include "tdd_button_gpio.h"
#include "tdl_button_manage.h"

#include "tal_api.h"

#include "tkl_output.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define APP_BUTTON_NAME "app_button"

// T2 board button pin
// #define APP_BUTTON_PIN          TUYA_GPIO_NUM_7

// T5 board button pin
#define APP_BUTTON_PIN TUYA_GPIO_NUM_12

// #define APP_BUTTON_MODE_IRQ     1
#define APP_BUTTON_MODE_SCAN 1

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_BUTTON_HANDLE sg_button_hdl = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

static void __button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    switch (event) {
    case TDL_BUTTON_PRESS_DOWN: {
        PR_NOTICE("%s: single click", name);
    } break;

    case TDL_BUTTON_LONG_PRESS_START: {
        PR_NOTICE("%s: long press", name);
    } break;

    default:
        break;
    }
}

/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main()
{
    OPERATE_RET rt = OPRT_OK;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    // button register
    BUTTON_GPIO_CFG_T button_hw_cfg = {
        .pin = APP_BUTTON_PIN,
#if (defined(APP_BUTTON_MODE_SCAN) && APP_BUTTON_MODE_SCAN)
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = TUYA_GPIO_PULLUP,
        .level = TUYA_GPIO_LEVEL_LOW,
#elif (defined(APP_BUTTON_MODE_IRQ) && APP_BUTTON_MODE_IRQ)
        .mode = BUTTON_IRQ_MODE,
        .level = TUYA_GPIO_LEVEL_HIGH,
        .pin_type.irq_edge = TUYA_GPIO_IRQ_FALL,
#endif
    };
    TUYA_CALL_ERR_GOTO(tdd_gpio_button_register(APP_BUTTON_NAME, &button_hw_cfg), __EXIT);

    // button create
    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 50};
    TUYA_CALL_ERR_GOTO(tdl_button_create(APP_BUTTON_NAME, &button_cfg, &sg_button_hdl), __EXIT);

    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_DOWN, __button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_LONG_PRESS_START, __button_function_cb);

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
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif