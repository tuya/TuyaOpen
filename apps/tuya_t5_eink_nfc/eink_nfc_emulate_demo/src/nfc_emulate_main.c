/**
 * @file nfc_emulate_main.c
 * @brief NFC Forum Tag Type 2 Emulation - Main entry point for Tuya platform
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"

static THREAD_HANDLE example_thrd_hdl = NULL; // NFC example thread handle
extern int           nfc_demo_all(void);

static void example_task(void *args)
{
    while (1) {
        nfc_demo_all();
        tal_system_sleep(20);
    }
}
/**
 * @brief user_main - NFC emulation entry point
 */
void user_main(void)
{
    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("====================================");
    PR_NOTICE("NFC Forum Tag Type 2 Emulation Demo");
    PR_NOTICE("====================================");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("====================================");

    /* thread create and start */
    const THREAD_CFG_T thread_cfg = {
        .thrdname   = "NFC_example_task",
        .stackDepth = 4096,
        .priority   = THREAD_PRIO_2,
    };
    tal_thread_create_and_start(&example_thrd_hdl, NULL, NULL, example_task, NULL, &thread_cfg);
}

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
 * @brief  NFC task thread
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
