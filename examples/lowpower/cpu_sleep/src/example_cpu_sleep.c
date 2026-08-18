/**
 * @file example_cpu_sleep.c
 * @brief TUYA_CPU_SLEEP - low power with the state kept, execution resuming in place.
 *
 * What this mode promises is that nothing is lost: ram, peripheral registers and any
 * network association survive, and the core carries on from the instruction it stopped
 * on. Nothing is reinitialised, because nothing went away.
 *
 * What it does not promise is any particular depth. How much a platform actually shuts
 * off varies - some stop the core alone and leave every clock running, others halt the
 * pll and the crystal too. On GD32VW553 it is the latter, which is why the figure below
 * is far lower than "just stopping the core" would suggest, and also why the name of this
 * example is TUYA_CPU_SLEEP rather than anything describing a depth.
 *
 * TUYA_CPU_DEEP_SLEEP is a different thing entirely, not a deeper setting of this one: it
 * discards the state and comes back through reset. See the cpu_deep_sleep example.
 *
 * The point here is that a device can sit in low power and still respond immediately: the
 * software timer below keeps ticking the whole time.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_sleep.h"
#include "tkl_system.h"
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "tkl_wifi.h"
#endif
#if defined(ENABLE_BLUETOOTH) && (ENABLE_BLUETOOTH == 1)
#include "tkl_bluetooth.h"
#endif

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE sg_task_handle = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief hand back the radios this application has no use for
 *
 * A radio stack that is up but idle still tells the power manager it needs the cpu, and on
 * some platforms that alone is enough to stop the core ever sleeping - silently, with every
 * call still returning success. Whether there is a radio to hand back is a property of the
 * build, so there is nothing here for the user to configure.
 */
static void __radios_off(void)
{
#if defined(ENABLE_BLUETOOTH) && (ENABLE_BLUETOOTH == 1)
    PR_NOTICE("ble  stack deinit -> %d", tkl_ble_stack_deinit(0));
#endif
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    PR_NOTICE("wifi power down   -> %d", tkl_wifi_set_work_mode(WWM_POWERDOWN));
#endif
}

static void __example_cpu_sleep_task(void *arg)
{
    OPERATE_RET rt = OPRT_OK;
    char *reset_desc = NULL;
    TUYA_RESET_REASON_E reason;
    uint32_t tick = 0;

    (void)arg;

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);

    reason = tkl_system_get_reset_reason(&reset_desc);
    PR_NOTICE("Reset reason:        %d (%s)", reason, reset_desc ? reset_desc : "unknown");

    __radios_off();

    rt = tkl_cpu_sleep_mode_set(TRUE, TUYA_CPU_SLEEP);
    PR_NOTICE("enter cpu sleep -> %d", rt);
    if (rt != OPRT_OK) {
        PR_WARN("this platform has no cpu sleep mode wired up; staying awake");
    }

    /* The core spends the gap between these ticks asleep, yet the counter keeps
     * advancing and the log keeps coming out: state was never lost. Measure the supply
     * current here and compare it against the deepsleep example. */
    while (1) {
        tal_system_sleep(2000);
        tick++;

        PR_DEBUG("alive %d", tick);
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

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    static THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority   = THREAD_PRIO_1;
    thrd_param.thrdname   = "cpu_sleep";
    TUYA_CALL_ERR_LOG(
        tal_thread_create_and_start(&sg_task_handle, NULL, NULL, __example_cpu_sleep_task, NULL, &thrd_param));

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

void tuya_app_main(void)
{
    user_main();
}
#endif
