/**
 * @file tuya_main.c
 * @brief Desk Buddy hardware self-test application entry.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tkl_output.h"
#include "board_com_api.h"
#include "lv_vendor.h"

#include "desk_buddy_test.h"
#include "desk_buddy_ui.h"

void user_main(void)
{
    OPERATE_RET rt;

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);

    tal_sw_timer_init();

    rt = board_register_hardware();
    if (rt != OPRT_OK) {
        PR_ERR("board_register_hardware failed: %d", rt);
        return;
    }

    rt = desk_buddy_test_init();
    if (rt != OPRT_OK) {
        PR_ERR("desk_buddy_test_init failed: %d", rt);
        return;
    }

    lv_vendor_init(DISPLAY_NAME);

    lv_vendor_disp_lock();
    desk_buddy_ui_create();
    lv_vendor_disp_unlock();

    lv_vendor_start(5, 8192);
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    user_main();

    while (1) {
        tal_system_sleep(500U);
    }
}
#else
static THREAD_HANDLE s_app_thread = NULL;

static void __app_thread(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(s_app_thread);
    s_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thread_cfg = {
        .stackDepth = 4096U,
        .priority = THREAD_PRIO_1,
        .thrdname = "tuya_app_main",
    };

    tal_thread_create_and_start(&s_app_thread, NULL, NULL, __app_thread,
                                NULL, &thread_cfg);
}
#endif
