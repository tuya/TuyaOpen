/**
 * @file example_wifi_ps.c
 * @brief WiFi power save - the radio sleeps between beacons instead of listening all the time.
 *
 * An associated station does not have to keep its receiver on. It tells the access point it is
 * going to doze, and the access point holds any traffic for it until the next DTIM beacon. The
 * station wakes on that beacon, collects whatever was buffered, and goes back down. Raising the
 * DTIM interval means fewer wakeups and less current, at the cost of the extra latency of
 * anything waiting for the next one.
 *
 * All of that is one call: tkl_wifi_set_lp_mode(TRUE, dtim).
 *
 * The example therefore does the only two things it has to - join an access point, because power
 * save is a property of an association and does nothing without one, then turn it on. The
 * heartbeat afterwards is there so a sleeping device and a hung one do not look alike.
 *
 * This is the low power path for a device that stays connected. A device that never joins a
 * network takes a different one entirely - see cpu_sleep and cpu_deep_sleep next door.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_wifi.h"
#include "tkl_sleep.h"
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

static void __wifi_event_cb(WF_EVENT_E event, void *arg)
{
    (void)arg;

    PR_DEBUG("wifi event %d", event);
}

/**
 * @brief join the access point and wait for an address
 *
 * Power save is negotiated with the access point, so there is nothing to enable until the
 * station is associated. Returning early here rather than enabling it anyway keeps a failure to
 * connect from looking like a failure of power save.
 */
static OPERATE_RET __join_ap(void)
{
    WF_STATION_STAT_E stat = WSS_IDLE;
    OPERATE_RET rt;
    int waited_ms = 0;

    TUYA_CALL_ERR_RETURN(tkl_wifi_init(__wifi_event_cb));
    TUYA_CALL_ERR_RETURN(tkl_wifi_set_work_mode(WWM_STATION));

    PR_NOTICE("connecting to \"%s\" ...", EXAMPLE_WIFI_SSID);
    TUYA_CALL_ERR_RETURN(tkl_wifi_station_connect((const int8_t *)EXAMPLE_WIFI_SSID,
                                                  (const int8_t *)EXAMPLE_WIFI_PASSWORD));

    while (waited_ms < EXAMPLE_WIFI_CONNECT_TIMEOUT_S * 1000) {
        if (tkl_wifi_station_get_status(&stat) == OPRT_OK && stat == WSS_GOT_IP) {
            PR_NOTICE("connected");
            return OPRT_OK;
        }
        tal_system_sleep(500);
        waited_ms += 500;
    }

    PR_ERR("could not connect (status %d) - power save needs an association, giving up", stat);
    return OPRT_COM_ERROR;
}

static void __example_wifi_ps_task(void *arg)
{
    OPERATE_RET rt;

    (void)arg;

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);

    if (__join_ap() != OPRT_OK) {
        return;
    }

    /* A connected device cannot hand wifi back, but bluetooth is another matter: a stack that
     * is up holds its own claim on the power manager, and one claim is enough to keep the
     * core awake no matter what the radio is doing. AN150 starts its own low power recipe
     * with ble_disable for exactly this reason. */
#if defined(ENABLE_BLUETOOTH) && (ENABLE_BLUETOOTH == 1)
    PR_NOTICE("ble  stack deinit -> %d", tkl_ble_stack_deinit(0));
#endif

    /* The whole feature. A higher dtim buys lower current and costs latency: traffic held by the
     * access point waits for the next interval before it is collected. */
#if defined(EXAMPLE_WIFI_PS) && (EXAMPLE_WIFI_PS == 1)
    rt = tkl_wifi_set_lp_mode(TRUE, EXAMPLE_WIFI_PS_DTIM);
    PR_NOTICE("wifi power save on, dtim %d -> %d", EXAMPLE_WIFI_PS_DTIM, rt);
    if (rt != OPRT_OK) {
        PR_WARN("this platform has no wifi power save wired up; the radio stays awake");
    }
#else
    /* Asked for rather than simply skipped. Platforms tend to switch power save on by
     * themselves once the interface is a station - on GD32VW553 the wifi manager does it the
     * moment the vif becomes STA - so leaving the call out would measure power save against
     * itself. */
    rt = tkl_wifi_set_lp_mode(FALSE, 0);
    PR_NOTICE("wifi power save off -> %d", rt);
#endif

    /* The other half, and not optional: a dozing radio leaves the cpu with nothing to do
     * between beacons, and the core and its clocks are the larger share of what is left -
     * without this the radio saving is easy to mistake for no saving at all. */
    rt = tkl_cpu_sleep_mode_set(TRUE, TUYA_CPU_SLEEP);
    PR_NOTICE("cpu sleep         -> %d", rt);

    /* Something has to come out of the console, or a device that is dozing between beacons and
     * one that has hung look exactly alike. Measure the supply current here - that is the only
     * thing that shows whether the radio is actually sleeping. */
    while (1) {
        tal_system_sleep(5000);
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
    thrd_param.thrdname   = "wifi_ps";
    TUYA_CALL_ERR_LOG(
        tal_thread_create_and_start(&sg_task_handle, NULL, NULL, __example_wifi_ps_task, NULL, &thrd_param));

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
