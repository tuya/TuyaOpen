/**
 * @file example_power.c
 * @brief Demonstrates the power abstraction (tdl_power): read battery/charge, query
 *        board-declared thresholds, subscribe to charge events, and switch power
 *        domains by semantic role (e.g. a low-power state). Roles a board does not
 *        have are skipped gracefully, so the same code runs across boards.
 *
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tkl_output.h"

#include "tdl_power_manage.h"
#include "board_com_api.h"

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_POWER_HANDLE sg_pwr = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

static const char *__chg_str(TDL_CHG_STATE_E st)
{
    switch (st) {
    case TDL_CHG_CHARGING:
        return "charging";
    case TDL_CHG_FULL:
        return "full";
    default:
        return "discharging";
    }
}

// Runs in the TDL worker thread (not an ISR), so logging here is safe.
static void __on_charge_event(TDL_CHG_STATE_E state, void *arg)
{
    (void)arg;
    PR_NOTICE("[event] charge state changed -> %s", __chg_str(state));
}

static void __print_status(void)
{
    uint32_t        mv    = 0;
    uint8_t         pct   = 0;
    TDL_CHG_STATE_E st    = TDL_CHG_DISCHARGE;
    BOOL_T          sd_on = FALSE;

    tdl_power_battery_get_voltage(sg_pwr, &mv);   // 0 if the board has no battery
    tdl_power_battery_get_percent(sg_pwr, &pct);
    tdl_power_charger_get_state(sg_pwr, &st);
    tdl_power_domain_get(sg_pwr, TDL_PWR_DOMAIN_SD, &sd_on);

    PR_NOTICE("battery: %u mV, %u%%, %s | SD domain: %s", mv, pct, __chg_str(st), sd_on ? "on" : "off");
}

void user_main(void)
{
    TDL_POWER_INFO_T info = {0};

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    tal_sw_timer_init();

    /* register the board's power device(s) */
    board_register_hardware();

    sg_pwr = tdl_power_find(POWER_NAME);
    if (NULL == sg_pwr) {
        PR_ERR("power device '%s' not found (does this board register one?)", POWER_NAME);
        while (1) {
            tal_system_sleep(1000);
        }
    }

    /* 1) board-declared battery landmarks (facts the app turns into policy) */
    if (OPRT_OK == tdl_power_get_info(sg_pwr, &info)) {
        PR_NOTICE("battery landmarks: full=%umV empty=%umV low=%umV critical=%umV", info.battery.v_full_mv,
                  info.battery.v_empty_mv, info.battery.v_low_mv, info.battery.v_critical_mv);
    }

    /* 2) subscribe to charge-state changes (event-driven; polling still works too) */
    if (OPRT_NOT_SUPPORTED == tdl_power_charger_on_event(sg_pwr, __on_charge_event, NULL)) {
        PR_NOTICE("charger events not supported on this board; will just poll");
    }

    /* 3) power-domain control by role. This set spans several boards; roles the board
          does not have are silently skipped, so one mask works everywhere. */
    uint32_t heavy_rails = TDL_PWR_DOMAIN_CAMERA | TDL_PWR_DOMAIN_SD | TDL_PWR_DOMAIN_CELLULAR |
                           TDL_PWR_DOMAIN_AUDIO | TDL_PWR_DOMAIN_DISPLAY;

    PR_NOTICE(">> entering low-power: cut camera / SD / cellular / audio / display");
    tdl_power_domain_set(sg_pwr, heavy_rails, FALSE);
    tal_system_sleep(3000);

    PR_NOTICE(">> leaving low-power: restore those rails");
    tdl_power_domain_set(sg_pwr, heavy_rails, TRUE);

    /* example low-battery policy built on the board-declared threshold */
    while (1) {
        uint32_t mv = 0;
        __print_status();
        if (OPRT_OK == tdl_power_battery_get_voltage(sg_pwr, &mv) && 0 != info.battery.v_critical_mv &&
            0 != mv && mv < info.battery.v_critical_mv) {
            PR_WARN("battery below critical (%umV < %umV) - app would save state / shut down here", mv,
                    info.battery.v_critical_mv);
        }
        tal_system_sleep(5000);
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
    thrd_param.thrdname     = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
