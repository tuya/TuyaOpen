/**
 * @file tuya_pm_schemes.c
 * @brief Built-in power schemes for tuya_pm: ACTIVE / CEC_T20 / ULP_ONLINE / DEEPSLEEP.
 *
 * Each scheme's enter() runs the actual mechanism directly (WiFi power-save, BLE off,
 * deep sleep). All ecosystem-specific dependencies (lpmgr / tal_wifi / ble_mgr /
 * tdl_power) live in THIS file, so the scheduler core (tuya_pm.c) stays generic and
 * includes no ecosystem headers.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_log.h"
#include "tal_sleep.h"
#include "tal_wifi.h"
#include "tuya_pm.h"
#include "tuya_pm_scheme.h"  // (public) TUYA_PM_SCHEME_T
#include "tuya_pm_internal.h" // (internal) declares tuya_pm_get_preset_schemes

#if defined(ENABLE_POWER)
#include "tdl_power_manage.h"
#include "tdl_power_types.h"
#endif

#if defined(ENABLE_WIFI_ULTRA_LOWPOWER)
#include "lpmgr.h"
#include "tuya_wifi_ultra_lowpower.h"
#endif

#if defined(ENABLE_BT_SERVICE) && (ENABLE_BT_SERVICE == 1)
#include "ble_mgr.h"
#endif

/***********************************************************
********************function declaration********************
***********************************************************/
static void        __ble_off(void);
static void        __ps_off(void);
static void        __ps_on(uint8_t dtim);
static void        __ensure_backend(void);
static OPERATE_RET __ps_init(void *ctx, void *power_handle);
static OPERATE_RET __deepsleep_init(void *ctx, void *power_handle);
static OPERATE_RET __active_enter(void *ctx);
static OPERATE_RET __cec_t20_enter(void *ctx);
static OPERATE_RET __ulp_online_enter(void *ctx);
static OPERATE_RET __deepsleep_enter(void *ctx);

/***********************************************************
***********************variable define**********************
***********************************************************/
static void *s_pwr = NULL; // bound tdl_power handle for DEEPSLEEP (NULL if none)
#if defined(ENABLE_WIFI_ULTRA_LOWPOWER)
static BOOL_T s_ulp_full_hold = FALSE; // holding the lpmgr full-speed (DISABLE) lock
#endif
static BOOL_T s_backend_up = FALSE;    // shared WiFi-PS/CPU backend brought up once (guarded)

/* ops (vtable) per scheme - { init, enter, exit } - kept separate from the data. */
static const TUYA_PM_SCHEME_OPS_T cACTIVE_OPS     = {NULL,      __active_enter,     NULL};
static const TUYA_PM_SCHEME_OPS_T cCEC_T20_OPS    = {__ps_init, __cec_t20_enter,    NULL};
static const TUYA_PM_SCHEME_OPS_T cULP_ONLINE_OPS = {__ps_init, __ulp_online_enter, NULL};
static const TUYA_PM_SCHEME_OPS_T cDEEPSLEEP_OPS  = {__deepsleep_init, __deepsleep_enter, NULL};

/* { id, ops, ctx }, indexed by TUYA_PM_SCHEME_E id. */
static const TUYA_PM_SCHEME_T cBUILTIN_TBL[TUYA_PM_SCHEME_BUILTIN_MAX] = {
    [TUYA_PM_ACTIVE]     = {TUYA_PM_ACTIVE,     &cACTIVE_OPS,     NULL},
    [TUYA_PM_CEC_T20]    = {TUYA_PM_CEC_T20,    &cCEC_T20_OPS,    NULL},
    [TUYA_PM_ULP_ONLINE] = {TUYA_PM_ULP_ONLINE, &cULP_ONLINE_OPS, NULL},
    [TUYA_PM_DEEPSLEEP]  = {TUYA_PM_DEEPSLEEP,  &cDEEPSLEEP_OPS,  NULL},
};

/***********************************************************
***********************function define**********************
***********************************************************/

/* Tear down the BLE controller: a hard prerequisite for WiFi power-save on
 * shared-antenna dual-mode modems (on T5 the controller is powered by the CP core
 * at boot). No-op on builds without tuya BT. */
static void __ble_off(void)
{
#if defined(ENABLE_BT_SERVICE) && (ENABLE_BT_SERVICE == 1)
    tuya_ble_deinit();
#endif
}

/* WiFi-PS + CPU-sleep backend, selected at compile time. */
#if defined(ENABLE_WIFI_ULTRA_LOWPOWER)
static void __ps_off(void)
{
    if (!s_ulp_full_hold) {
        lpmgr_register(TY_LP_DISABLE); // force full speed
        s_ulp_full_hold = TRUE;
    }
}
static void __ps_on(uint8_t dtim)
{
    if (s_ulp_full_hold) {
        lpmgr_unregister(TY_LP_DISABLE);
        s_ulp_full_hold = FALSE;
    }
    if (dtim) {
        lpmgr_set_lps_dtim(dtim);
    }
}
#else
static void __ps_off(void)
{
    tal_cpu_acquire_wakelock(TAL_WAKELOCK_APP_BASE); // hold awake
    tal_wifi_lp_disable();
}
static void __ps_on(uint8_t dtim)
{
    tal_cpu_release_wakelock(TAL_WAKELOCK_APP_BASE); // allow sleep
    if (dtim) {
        tal_wifi_set_lps_dtim(dtim);
    }
    tal_wifi_lp_enable();
}
#endif

/* Bring up the shared WiFi-PS / CPU-sleep backend once (guarded), so a chain with no
 * PS scheme never starts it. Called from the PS schemes' init. */
static void __ensure_backend(void)
{
    if (s_backend_up) {
        return;
    }
    s_backend_up = TRUE;
#if defined(ENABLE_WIFI_ULTRA_LOWPOWER)
    tuya_wifi_ulp_init();           // event-driven ULP framework
    lpmgr_register(TY_LP_APP_USED); // enable app low-power participation
#else
    tal_cpu_set_lp_mode(TRUE);      // global CPU LP so non-ACTIVE schemes can sleep
#endif
}

/* One-time init shared by the WiFi-PS schemes (CEC_T20 / ULP_ONLINE). */
static OPERATE_RET __ps_init(void *ctx, void *power_handle)
{
    (void)ctx;
    (void)power_handle; // PS schemes don't need the tdl_power handle
    __ensure_backend();
    return OPRT_OK;
}

/* One-time init for DEEPSLEEP: store the tdl_power handle for enter(). */
static OPERATE_RET __deepsleep_init(void *ctx, void *power_handle)
{
    (void)ctx;
    s_pwr = power_handle;
    return OPRT_OK;
}

static OPERATE_RET __active_enter(void *ctx)
{
    (void)ctx;
    __ps_off();
    return OPRT_OK;
}

static OPERATE_RET __cec_t20_enter(void *ctx)
{
    (void)ctx;
    __ble_off();
    __ps_on(1); // DTIM1: online standby, power aligned to CEC Title 20
    return OPRT_OK;
}

static OPERATE_RET __ulp_online_enter(void *ctx)
{
    (void)ctx;
    __ble_off();
    __ps_on(10); // DTIM10: ultra-low-power online keep-alive
    return OPRT_OK;
}

/* Enter real deep sleep via tdl_power (owns wakeup sources + the CPU power-down; does
 * not return on success - waking reboots). Returns only if unavailable/refused, in
 * which case the scheduler holds at this (deepest online) level. */
static OPERATE_RET __deepsleep_enter(void *ctx)
{
    (void)ctx;
    __ble_off();
#if defined(ENABLE_POWER)
    if (NULL != s_pwr) {
        tdl_power_enter_deepsleep((TDL_POWER_HANDLE)s_pwr, 0);
        PR_WARN("[pm] deep sleep refused by power device; holding online low-power");
        return OPRT_NOT_SUPPORTED;
    }
    PR_WARN("[pm] DEEPSLEEP requested but no power device bound; degrading to online");
#else
    PR_WARN("[pm] DEEPSLEEP requested but ENABLE_POWER is off; degrading to online");
#endif
    return OPRT_NOT_SUPPORTED;
}

const TUYA_PM_SCHEME_T *tuya_pm_get_preset_schemes(uint8_t *cnt)
{
    if (cnt) {
        *cnt = TUYA_PM_SCHEME_BUILTIN_MAX;
    }
    return cBUILTIN_TBL;
}
