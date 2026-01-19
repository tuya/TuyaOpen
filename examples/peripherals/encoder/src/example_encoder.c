/**
 * @file example_encoder.c
 * @brief Rotary encoder input handling example for the Tuya SDK
 *
 * This file provides an example implementation of rotary encoder input handling using the Tuya SDK.
 * It demonstrates the configuration and usage of rotary encoder peripherals for detecting rotation
 * and button press events. The example shows how to initialize the encoder driver, continuously
 * monitor encoder angle changes, and detect button press events.
 *
 * The encoder example is designed to help developers understand how to integrate rotary encoder
 * functionality into their Tuya-based IoT projects for user interface and control applications.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tkl_output.h"
#include "tal_api.h"

#include "drv_encoder.h"

#include "board_com_api.h"

/***********************************************************
*************************micro define***********************
***********************************************************/

// Encoder pin definitions with fallback defaults
#ifndef DECODER_INPUT_A
#define DECODER_INPUT_A TUYA_GPIO_NUM_4 // Default: GPIO 4
#endif

#ifndef DECODER_INPUT_B
#define DECODER_INPUT_B TUYA_GPIO_NUM_5 // Default: GPIO 5
#endif

#ifndef DECODER_INPUT_P
#define DECODER_INPUT_P TUYA_GPIO_NUM_6 // Default: GPIO 6
#endif

#define ENCODER_POLL_INTERVAL_MS 100 // Poll encoder every 100ms

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static int32_t last_angle = 0;
static uint8_t last_button_state = 0;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Monitor encoder angle changes and button presses
 *
 * This function continuously monitors the encoder for angle changes and button
 * press events. It prints messages when the encoder is rotated or the button
 * is pressed.
 *
 * @return none
 */
static void __encoder_monitor_task(void)
{
    int32_t current_angle = 0;
    uint8_t button_pressed = 0;
    int32_t angle_delta = 0;

    while (1) {
        // Get current encoder angle
        current_angle = encoder_get_angle();

        // Check if angle has changed
        if (current_angle != last_angle) {
            angle_delta = current_angle - last_angle;

            if (angle_delta > 0) {
                PR_NOTICE("Encoder rotated clockwise: angle = %d (delta: +%d)", current_angle, angle_delta);
            } else {
                PR_NOTICE("Encoder rotated counter-clockwise: angle = %d (delta: %d)", current_angle, angle_delta);
            }

            last_angle = current_angle;
        }

        // Check button state
        button_pressed = encoder_get_pressed();

        if (button_pressed && !last_button_state) {
            PR_NOTICE("Encoder button pressed! Current angle: %d", current_angle);
            last_button_state = 1;
        } else if (!button_pressed && last_button_state) {
            PR_NOTICE("Encoder button released");
            last_button_state = 0;
        }

        // Sleep for a short interval
        tal_system_sleep(ENCODER_POLL_INTERVAL_MS);
    }
}

/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
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

    /*hardware register*/
    board_register_hardware();

    PR_NOTICE("Encoder Configuration:");
    PR_NOTICE("- Input A Pin (clockwise):        GPIO %d", DECODER_INPUT_A);
    PR_NOTICE("- Input B Pin (counter-clockwise): GPIO %d", DECODER_INPUT_B);
    PR_NOTICE("- Button Press Pin:                GPIO %d", DECODER_INPUT_P);
    PR_NOTICE("");
    PR_NOTICE("Initializing encoder driver...");

    // Initialize the encoder
    tkl_encoder_init();
    if (rt != OPRT_OK) {
        PR_ERR("Failed to initialize encoder driver, error: %d", rt);
        return;
    }

    PR_NOTICE("Encoder initialized successfully!");
    PR_NOTICE("Starting encoder monitoring...");
    PR_NOTICE("- Rotate the encoder to see angle changes");
    PR_NOTICE("- Press the encoder button to see button events");

    // Get initial angle
    last_angle = encoder_get_angle();
    PR_NOTICE("Initial encoder angle: %d", last_angle);

    // Start monitoring encoder
    __encoder_monitor_task();
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
