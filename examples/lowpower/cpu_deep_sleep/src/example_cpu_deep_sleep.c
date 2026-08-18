/**
 * @file example_cpu_deep_sleep.c
 * @brief Deep sleep and the wakeup sources that survive it.
 *
 * Deep sleep is the mode where most of the chip is powered down: clocks stop,
 * peripherals lose their state, and only a small always-on island keeps running. That
 * island is what limits the choice of wakeup source - typically an RTC alarm and a
 * handful of dedicated wakeup pins, not any old interrupt.
 *
 * The two halves are arming what may wake the device (tkl_wakeup_source_set) and then
 * asking the cpu to go down (tkl_cpu_sleep_mode_set with TUYA_CPU_DEEP_SLEEP).
 *
 * The reset reason printed at start-up tells you how the board got here, which is the
 * easiest way to tell a fresh power-up apart from a wake that went all the way round
 * through reset.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_sleep.h"
#include "tkl_system.h"
#include "tkl_wakeup.h"
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
static volatile uint32_t sg_rtc_fired = 0;

/***********************************************************
***********************function define**********************
***********************************************************/

#if defined(EXAMPLE_WAKEUP_RTC) && (EXAMPLE_WAKEUP_RTC == 1)
/**
 * @brief called from the rtc alarm interrupt
 *
 * Keep it short: this runs in interrupt context, so it only records the event and
 * lets the task below do the printing.
 */
static void __rtc_wakeup_cb(TUYA_RTC_NUM_E port, void *args)
{
    (void)port;
    (void)args;

    sg_rtc_fired++;
}

static OPERATE_RET __arm_rtc_wakeup(void)
{
    TUYA_WAKEUP_SOURCE_BASE_CFG_T cfg = {0};

    cfg.source                        = TUYA_WAKEUP_SOURCE_RTC;
    cfg.wakeup_para.rtc_param.RTC_num = TUYA_RTC_NUM_0;
    cfg.wakeup_para.rtc_param.mode    = TUYA_RTC_MODE_ONCE;
    cfg.wakeup_para.rtc_param.ms      = EXAMPLE_WAKEUP_RTC_SECONDS * 1000;
    cfg.wakeup_para.rtc_param.cb      = __rtc_wakeup_cb;
    cfg.wakeup_para.rtc_param.args    = NULL;

    return tkl_wakeup_source_set(&cfg);
}
#endif

#if defined(EXAMPLE_WAKEUP_GPIO) && (EXAMPLE_WAKEUP_GPIO == 1)
static OPERATE_RET __arm_gpio_wakeup(void)
{
    TUYA_WAKEUP_SOURCE_BASE_CFG_T cfg = {0};

    cfg.source                          = TUYA_WAKEUP_SOURCE_GPIO;
    cfg.wakeup_para.gpio_param.gpio_num = (TUYA_GPIO_NUM_E)EXAMPLE_WAKEUP_GPIO_PIN;
    cfg.wakeup_para.gpio_param.level    = (TUYA_GPIO_WAKE_TYPE_E)EXAMPLE_WAKEUP_GPIO_EDGE;

    return tkl_wakeup_source_set(&cfg);
}
#endif

/**
 * @brief hand back the radios this application has no use for
 *
 * Deep sleep removes power from most of the chip, so a radio stack that is still up has
 * every reason to object - and on some platforms it does, by telling the power manager to
 * keep the core alive on its behalf, silently, with every call still returning success.
 * Whether there is a radio to hand back is a property of the build, so there is nothing
 * here for the user to configure.
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

static void __example_cpu_deep_sleep_task(void *arg)
{
    OPERATE_RET rt = OPRT_OK;
    char *reset_desc = NULL;
    TUYA_RESET_REASON_E reason;

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

#if defined(EXAMPLE_WAKEUP_GPIO) && (EXAMPLE_WAKEUP_GPIO == 1)
    rt = __arm_gpio_wakeup();
    PR_NOTICE("gpio wakeup  pin %d edge %d -> %d", EXAMPLE_WAKEUP_GPIO_PIN, EXAMPLE_WAKEUP_GPIO_EDGE, rt);
#endif

#if defined(EXAMPLE_WAKEUP_RTC) && (EXAMPLE_WAKEUP_RTC == 1)
    rt = __arm_rtc_wakeup();
    PR_NOTICE("rtc  wakeup  in %d s -> %d", EXAMPLE_WAKEUP_RTC_SECONDS, rt);
#endif

    __radios_off();

    rt = tkl_cpu_sleep_mode_set(TRUE, TUYA_CPU_DEEP_SLEEP);
    PR_NOTICE("enter deep sleep -> %d", rt);
    if (rt != OPRT_OK) {
        PR_WARN("this platform has no deep sleep mode wired up; staying awake");
    }

    /* Something has to come out of the console, or a device that is sleeping and a device
     * that has hung look exactly alike. Deliberately no counter: platforms that resume in
     * place would count on and on while platforms that wake through reset would restart
     * from one every time, and that difference says nothing about how well either slept.
     * Measure the supply current here to see what the mode is actually worth. */
    while (1) {
        tal_system_sleep(2000);

#if defined(EXAMPLE_WAKEUP_RTC) && (EXAMPLE_WAKEUP_RTC == 1)
        if (sg_rtc_fired) {
            sg_rtc_fired = 0;
            PR_NOTICE("rtc alarm fired");
        }
#endif
        PR_DEBUG("awake");
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
    thrd_param.thrdname   = "cpu_deep_sleep";
    TUYA_CALL_ERR_LOG(
        tal_thread_create_and_start(&sg_task_handle, NULL, NULL, __example_cpu_deep_sleep_task, NULL, &thrd_param));

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
