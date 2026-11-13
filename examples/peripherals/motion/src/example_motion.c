/**
 * @file example_motion.c
 * @brief Motion sensor example.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tkl_output.h"
#include "tdl_motion_manage.h"
#include "board_com_api.h"

void user_main(void)
{
    TDL_MOTION_HANDLE motion;
    TDL_MOTION_OPEN_CFG_T open_cfg = {
        .sample_cfg = {
            .accel_range = TDL_MOTION_ACCEL_RANGE_2G,
            .gyro_range = TDL_MOTION_GYRO_RANGE_2000DPS,
            .sample_rate_hz = 100U,
            .low_power = false,
        },
        /* Desk Buddy uses polling; other boards may enable TDD IRQ events. */
        .events_enabled = false,
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

    motion = tdl_motion_find_dev(MOTION_NAME);
    if (motion == NULL) {
        PR_ERR("motion device %s not found", MOTION_NAME);
        return;
    }

    rt = tdl_motion_open(&motion, &open_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("motion open failed: %d", rt);
        return;
    }

    while (1) {
        TDL_MOTION_DATA_T data;

        rt = tdl_motion_read(motion, &data);
        if (rt == OPRT_OK && data.accel_valid) {
            PR_NOTICE("motion accel: x=%.2f y=%.2f z=%.2f m/s2",
                      data.accel_mps2.x, data.accel_mps2.y, data.accel_mps2.z);
        } else {
            PR_ERR("motion read failed: %d", rt);
        }
        /* Match the configured 100 Hz ODR: one new sample every 10 ms. */
        tal_system_sleep(10);
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
        .thrdname = "motion_app",
    };

    tal_thread_create_and_start(&sg_app_thread, NULL, NULL,
                                __app_thread, NULL, &thread_cfg);
}
#endif
