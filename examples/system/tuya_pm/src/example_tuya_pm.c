/**
 * @file example_tuya_pm.c
 * @brief Exercises the power manager (tuya_pm) two ways (PM_DEMO_MODE): HOLD pins one
 *        scheme so its steady-state current can be measured on a power analyzer; DECAY
 *        lets the manager idle-decay down the chain on its own so the descent and its
 *        timing can be observed. Peripherals are registered as real tdl_power rails, so
 *        each scheme reflects true power. CEC_T20 and ULP_ONLINE rely on WiFi power-save
 *        (needs AP association + station mode + BLE off), so they are gated behind WiFi
 *        association via an app-level hold-lock (BLE-off is handled by the built-in
 *        schemes). Set PM_DEMO_MODE / PM_DEMO_HOLD_SCHEME and reflash.
 *
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "string.h"
#include "tal_api.h"
#include "tal_wifi.h"
#include "tkl_output.h"
#include "board_com_api.h"
#include "tuya_pm.h"


/* ---- demo mode (change and reflash) ----
        HOLD : pin PM_DEMO_HOLD_SCHEME and hold it (no decay) - measures the steady-state
               current of one scheme.
        DECAY: don't request anything - let the manager idle-decay down the chain on its
               own (ACTIVE -> CEC_T20 -> ULP_ONLINE -> DEEPSLEEP), one step per the chain's
               min_residency_ms. Watch the scheme-change log to see the descent. ---- */
#define PM_DEMO_MODE_HOLD  0
#define PM_DEMO_MODE_DECAY 1

#ifndef PM_DEMO_MODE
#define PM_DEMO_MODE PM_DEMO_MODE_HOLD
#endif

/* Scheme to pin in HOLD mode (ignored in DECAY mode). */
#ifndef PM_DEMO_HOLD_SCHEME
#define PM_DEMO_HOLD_SCHEME TUYA_PM_ULP_ONLINE
#endif

/* ---- WiFi: fill these in to associate to an AP. Needed for real ULP_ONLINE uA
        (DTIM keep-alive only applies while connected). Empty SSID = stay offline. ---- */
#define PM_DEMO_WIFI_SSID "your-ssid"
#define PM_DEMO_WIFI_PWD  "your-password"

/***********************************************************
***********************variable define**********************
***********************************************************/
/* ---- link gate: CEC_T20/ULP_ONLINE rely on WiFi power-save, which the modem enters
        only once associated to an AP (station mode, BLE off). Modeled as a hold-lock
        pinned at ACTIVE, held while offline and released once WiFi is up. Armed only
        when the scheme under test is a WiFi-PS scheme. ---- */
static TUYA_PM_LOCK_HANDLE sg_link_lock = NULL;
static BOOL_T              sg_link_held = FALSE;

/***********************************************************
***********************function define**********************
***********************************************************/

static const char *__scheme_str(uint8_t s)
{
    switch (s) {
    case TUYA_PM_ACTIVE:     return "ACTIVE";
    case TUYA_PM_CEC_T20:    return "CEC_T20";
    case TUYA_PM_ULP_ONLINE: return "ULP_ONLINE";
    case TUYA_PM_DEEPSLEEP:  return "DEEPSLEEP";
    default:                 return "?";
    }
}

static void __on_pm_change(uint8_t from, uint8_t to, void *arg)
{
    (void)arg;
    PR_NOTICE("[demo] scheme change: %s -> %s", __scheme_str(from), __scheme_str(to));
}

/* Idempotently hold/release the link gate (keeps the lock ref-count balanced). */
static void __link_gate(BOOL_T wifi_up)
{
    if (NULL == sg_link_lock) {
        return;
    }
    if (wifi_up && sg_link_held) {
        tuya_pm_lock_release(sg_link_lock); // associated -> allow descent into the PS schemes
        sg_link_held = FALSE;
    } else if (!wifi_up && !sg_link_held) {
        tuya_pm_lock_acquire(sg_link_lock); // offline -> pin at ACTIVE, block the PS schemes
        sg_link_held = TRUE;
    }
}

static void __wifi_evt(WF_EVENT_E event, void *arg)
{
    (void)arg;
    if (WFE_CONNECTED == event) {
        PR_NOTICE("[demo] wifi connected");
        __link_gate(TRUE);  // AP associated -> release the gate, PS schemes allowed
    } else if (WFE_DISCONNECTED == event) {
        PR_NOTICE("[demo] wifi disconnected");
        __link_gate(FALSE); // lost the AP -> re-take the gate, fall back to ACTIVE
    }
}

/* Associate to the configured AP (blocking, best-effort). Skips if no SSID. */
static void __wifi_connect(void)
{
    WF_STATION_STAT_E st = 0;
    int i;

    if (0 == strlen(PM_DEMO_WIFI_SSID)) {
        PR_NOTICE("[demo] no SSID set -> running offline (no DTIM; CPU sleep only)");
        return;
    }
    tal_wifi_init(__wifi_evt);
    tal_wifi_set_work_mode(WWM_STATION);
    PR_NOTICE("[demo] connecting wifi '%s' ...", PM_DEMO_WIFI_SSID);
    tal_wifi_station_connect((int8_t *)PM_DEMO_WIFI_SSID, (int8_t *)PM_DEMO_WIFI_PWD);

    for (i = 0; i < 40; i++) { // wait up to ~20s for an IP
        tal_system_sleep(500);
        if (OPRT_OK == tal_wifi_station_get_status(&st) && WSS_GOT_IP == st) {
            PR_NOTICE("[demo] wifi got IP");
            __link_gate(TRUE); // release even if no event fired
            return;
        }
    }
    PR_NOTICE("[demo] wifi not connected (status=%d); continuing anyway", st);
}

void user_main(void)
{
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    tal_sw_timer_init();
    board_register_hardware(); // registers the tdl_power "power" device + rails

    /* Explicit descent chain (all four built-ins) so any scheme can be requested;
       min_residency per step is this app's decay-speed choice. */
    static const TUYA_PM_CHAIN_STEP_T chain[] = {
        {TUYA_PM_ACTIVE,     0     },
        {TUYA_PM_CEC_T20,    3000  },
        {TUYA_PM_ULP_ONLINE, 30000 },
        {TUYA_PM_DEEPSLEEP,  300000},
    };
    tuya_pm_set_chain(chain, sizeof(chain) / sizeof(chain[0]));

    if (OPRT_OK != tuya_pm_init("power")) {
        PR_ERR("tuya_pm_init failed");
        while (1) {
            tal_system_sleep(1000);
        }
    }
    tuya_pm_on_change(__on_pm_change, NULL);

    /* Bring WiFi up first and gate descent on it: the online schemes only save power
       once associated to an AP. Pin at ACTIVE (link gate) until WiFi is up. */
    tuya_pm_lock_create("link", TUYA_PM_ACTIVE, &sg_link_lock);
    __link_gate(FALSE);  // block descent until WiFi is up
    __wifi_connect();    // associate (releases the gate on success)

#if PM_DEMO_MODE == PM_DEMO_MODE_HOLD
    /* Pin the chosen scheme and hold it (no decay), then just sit for measurement.
       BLE-off / WiFi-PS / deep sleep are done inside the built-in scheme's enter(). */
    PR_NOTICE(">> holding at %s for power measurement (edit PM_DEMO_HOLD_SCHEME)", __scheme_str(PM_DEMO_HOLD_SCHEME));
    tuya_pm_request(PM_DEMO_HOLD_SCHEME);
#else
    /* No request: let it idle-decay down the chain on its own; watch __on_pm_change for
       each step. Reset the idle timer so decay starts cleanly from ACTIVE now that WiFi
       is up (the link gate held it at ACTIVE while associating). */
    PR_NOTICE(">> idle-decaying down the chain; watch scheme changes (residency per step set by the chain)");
    tuya_pm_activity();
#endif
    tuya_pm_dump();

    while (1) {
        tal_system_sleep(60000);
    }
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    user_main();
    while (1) {
        tal_system_sleep(500);
    }
}
#else
static THREAD_HANDLE ty_app_thread = NULL;

static void tuya_app_thread(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth   = 1024 * 4;
    thrd_param.priority     = THREAD_PRIO_1;
    thrd_param.thrdname     = "tuya_pm_demo";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
