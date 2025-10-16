/**
 * @file app_main.c
 * @brief MicroPython minimal application for T5AI
 */

#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tkl_output.h"

#ifdef ENABLE_MICROPYTHON
/* MicroPython header */
#include "micropython.h"
#endif

#ifndef PROJECT_NAME
#define PROJECT_NAME "mpy_minimal"
#endif

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.0.0"
#endif

#define APP_TASK_PRIORITY       (THREAD_PRIO_2)
#define APP_TASK_STACK_SIZE     (4096)

/**
 * @brief user defined log output callback
 *
 * @param str log string
 * @return void
 */
static void user_log_output_cb(const char *str)
{
    tkl_log_output(str);
}

static void app_task(void *arg)
{
    /* Wait a bit for system to stabilize */
    tal_system_sleep(2000);

    PR_NOTICE("========================================");
    PR_NOTICE("   Minimal Boot Firmware Started");
    PR_NOTICE("========================================");

#ifdef ENABLE_MICROPYTHON
    /* Initialize MicroPython */
    PR_NOTICE("Initializing MicroPython...");
    if (micropython_init() != 0) {
        PR_ERR("Failed to initialize MicroPython");
        PR_NOTICE("Continuing without MicroPython...");
    } else {
        PR_NOTICE("MicroPython initialized successfully");
        PR_NOTICE("REPL should be available on UART0 (115200 baud)");
    }
#endif

    /* Main loop */
    while (1) {
        PR_DEBUG("Sleep 1 seconds ...");
        tal_system_sleep(1000); /* Sleep 1 seconds */
    }
}

/**
 * @brief Application main entry
 */
void tuya_app_main(void)
{
    OPERATE_RET rt = OPRT_OK;
    THREAD_HANDLE app_thread = NULL;

    /* Initialize log system first with user callback */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, user_log_output_cb);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    /* Create application task */
    THREAD_CFG_T thread_cfg = {
        .stackDepth = APP_TASK_STACK_SIZE,
        .priority = APP_TASK_PRIORITY,
        .thrdname = "mpy_main"
    };

    rt = tal_thread_create_and_start(&app_thread, NULL, NULL, app_task, NULL, &thread_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to create app task: %d", rt);
        return;
    }
}
