#include "tal_api.h"
#include "tkl_output.h"
#include "tal_cli.h"

extern int tflite_main_c_function(int argc, char *argv[]);

static void user_main(void)
{
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    PR_DEBUG("hello world\r\n");

    tflite_main_c_function(0, NULL);

    int cnt = 0;
    while (1) {
        PR_DEBUG("cnt is %d", cnt++);
        tal_system_sleep(1000);
    }
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
    // THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 128*1024;  // 增加到 128KB 以容纳 TensorFlow Lite 对象
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    thrd_param.psram_mode = TRUE;
#endif
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif