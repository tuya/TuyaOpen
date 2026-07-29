/**
 * @file app_battery.c
 * @brief Battery/charge status for the app, over the tdl_power abstraction.
 *        Board hardware (ADC divider, charge GPIO, curve, thresholds) lives in the
 *        board's power registration; this module only polls/reports and owns policy.
 * @version 0.2
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include "app_battery.h"

#include "tal_api.h"
#include "tdl_power_manage.h"

#define GET_BATTERY_TIME_MS (5 * 60 * 1000) // 5 minutes

static TDL_POWER_HANDLE sg_pwr               = NULL;
static TIMER_ID         sg_battery_timer_id  = NULL;
static uint8_t          sg_battery_percentage = 50;
static volatile bool    sg_is_charging       = false;

static void __battery_refresh(void)
{
    uint8_t         pct = 0;
    TDL_CHG_STATE_E st;

    if (OPRT_OK == tdl_power_battery_get_percent(sg_pwr, &pct)) {
        sg_battery_percentage = pct;
    }
    if (OPRT_OK == tdl_power_charger_get_state(sg_pwr, &st)) {
        sg_is_charging = (TDL_CHG_CHARGING == st);
    }
}

static void __battery_timer_cb(TIMER_ID timer_id, void *arg)
{
    __battery_refresh();
}

static void __charge_evt_cb(TDL_CHG_STATE_E state, void *arg)
{
    sg_is_charging = (TDL_CHG_CHARGING == state);
    // TODO: update DP / UI on charge state change
}

OPERATE_RET app_battery_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    sg_pwr = tdl_power_find(POWER_NAME);
    if (NULL == sg_pwr) {
        PR_ERR("power device not found");
        return OPRT_NOT_FOUND;
    }

    // Event-driven charge updates (replaces the old 1.5s polling timer).
    tdl_power_charger_on_event(sg_pwr, __charge_evt_cb, NULL);

    // Periodic battery-level refresh.
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__battery_timer_cb, NULL, &sg_battery_timer_id));
    TUYA_CALL_ERR_RETURN(tal_sw_timer_start(sg_battery_timer_id, GET_BATTERY_TIME_MS, TAL_TIMER_CYCLE));
    tal_sw_timer_trigger(sg_battery_timer_id);

    return OPRT_OK;
}

void app_battery_get_status(uint8_t *percentage, bool *is_charging)
{
    if (NULL != percentage) {
        *percentage = sg_battery_percentage;
    }
    if (NULL != is_charging) {
        *is_charging = sg_is_charging;
    }
}
