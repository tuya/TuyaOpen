/**
 * @file example_encoder.c
 * @brief Rotary encoder example.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tkl_output.h"
#include "tdl_encoder_manage.h"
#include "board_com_api.h"

static void __encoder_event_cb(TDL_ENCODER_HANDLE handle,
                               const TDL_ENCODER_EVENT_T *event,
                               void *user_data)
{
    (void)handle;
    (void)user_data;

    switch (event->type) {
        case TDL_ENCODER_EVENT_ROTATE:
            PR_NOTICE("encoder rotate: delta=%d position=%d",
                      event->delta, event->position);
            break;
        case TDL_ENCODER_EVENT_BUTTON_DOWN:
            PR_NOTICE("encoder button down");
            break;
        case TDL_ENCODER_EVENT_BUTTON_UP:
            PR_NOTICE("encoder button up");
            break;
        case TDL_ENCODER_EVENT_BUTTON_CLICK:
            PR_NOTICE("encoder button click");
            break;
        case TDL_ENCODER_EVENT_BUTTON_DOUBLE_CLICK:
            PR_NOTICE("encoder button double click");
            break;
        case TDL_ENCODER_EVENT_BUTTON_LONG_START:
            PR_NOTICE("encoder button long press");
            break;
        case TDL_ENCODER_EVENT_BUTTON_LONG_HOLD:
            PR_NOTICE("encoder button long hold");
            break;
        default:
            break;
    }
}

void user_main(void)
{
    TDL_ENCODER_HANDLE encoder;
    TDL_ENCODER_CFG_T cfg = {
        .click_ms = 300U,
        .long_press_ms = 1500U,
        .long_hold_ms = 100U,
    };
    OPERATE_RET rt;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);

    rt = board_register_hardware();
    if (rt != OPRT_OK) {
        PR_ERR("board hardware register failed: %d", rt);
        return;
    }

    encoder = tdl_encoder_find_dev(ENCODER_NAME);
    if (encoder == NULL) {
        PR_ERR("encoder %s not found", ENCODER_NAME);
        return;
    }

    rt = tdl_encoder_set_callback(encoder, __encoder_event_cb, NULL);
    if (rt != OPRT_OK) {
        PR_ERR("encoder callback register failed: %d", rt);
        return;
    }

    rt = tdl_encoder_open(&encoder, &cfg);
    if (rt != OPRT_OK) {
        PR_ERR("encoder open failed: %d", rt);
        return;
    }

    PR_NOTICE("encoder started");
    while (1) {
        tal_system_sleep(1000);
    }
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    user_main();
}
#else
static THREAD_HANDLE sg_app_thread;

static void __app_thread(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(sg_app_thread);
    sg_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thread_cfg = {
        .stackDepth = 4096,
        .priority = THREAD_PRIO_1,
        .thrdname = "encoder_app",
    };

    tal_thread_create_and_start(&sg_app_thread, NULL, NULL,
                                __app_thread, NULL, &thread_cfg);
}
#endif
