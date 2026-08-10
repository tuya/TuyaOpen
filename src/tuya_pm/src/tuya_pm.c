/**
 * @file tuya_pm.c
 * @brief Device power manager (v3) - the scheduler core.
 *
 * A pure scheduler over a library of power "schemes": it arbitrates the effective
 * scheme, decays on idle along a descent chain, and calls each scheme's enter/exit on
 * transitions. It implements NO mechanism itself - the mechanism (WiFi power-save, BLE
 * off, deep sleep) lives in the schemes (tuya_pm_schemes.c) so this file includes no
 * ecosystem headers (lpmgr / tal_wifi / ble).
 *
 * Depth is defined by position in the descent chain (index 0 = shallowest). Internally
 * everything runs in chain-index space; the public API converts between scheme id and
 * chain index. effective = min(idle_target, floor of every hold-lock); idle decay stops
 * at the chain tail, and the low-battery lifeboat can force a separate escape scheme.
 * Consumers follow the effective scheme (suspend high-priority-first on descend,
 * resume low-priority-first on ascend) and do not take part in arbitration.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "string.h"
#include "tuya_error_code.h"
#include "tal_memory.h"
#include "tal_log.h"
#include "tal_mutex.h"
#include "tal_sw_timer.h"
#include "tuya_list.h"
#include "tuya_pm.h"
#include "tuya_pm_scheme.h"  // (public) TUYA_PM_SCHEME_T + tuya_pm_scheme_register
#include "tuya_pm_internal.h" // (internal) tuya_pm_get_preset_schemes

#if defined(ENABLE_POWER)
#include "tdl_power_manage.h"
#include "tdl_power_types.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define PM_NAME_LEN     16
#define PM_MAX_SCHEMES  12
#define PM_BATT_POLL_MS 30000

/***********************************************************
***********************typedef define***********************
***********************************************************/
/* one hold-lock */
typedef struct {
    LIST_HEAD node;
    char      name[PM_NAME_LEN];
    uint8_t   floor_idx; // shallowest chain index allowed while held
    int       cnt;
} PM_LOCK_T;

/* one managed peripheral (consumer) */
typedef struct {
    LIST_HEAD node;
    char      name[PM_NAME_LEN];
    uint8_t   min_idx;   // powered up to this chain index (inclusive)
    uint8_t   priority;  // higher suspends first; resume in reverse
    OPERATE_RET (*suspend)(void *arg);
    OPERATE_RET (*resume)(void *arg);
    void     *arg;
    BOOL_T    powered;      // current tracked power state
    BOOL_T    is_rail;      // rail-helper consumer (gates a tdl_power domain)
    uint32_t  domain_mask;  // rail only
} PM_CONSUMER_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static BOOL_T       s_inited = FALSE;
static MUTEX_HANDLE s_mutex  = NULL;
static TIMER_ID     s_decay_timer = NULL;
static LIST_HEAD    s_lock_list = {&s_lock_list, &s_lock_list};
static LIST_HEAD    s_consumer_list = {&s_consumer_list, &s_consumer_list};
static void        *s_pwr = NULL; // bound tdl_power handle (rail helper), NULL if none

/* scheme library (built-in + registered) and descent chain (ordered scheme ids) */
static const TUYA_PM_SCHEME_T *s_lib[PM_MAX_SCHEMES];
static uint8_t                 s_lib_cnt = 0;
static uint8_t                 s_chain[PM_MAX_SCHEMES];     // scheme ids, shallow -> deep
static uint32_t                s_chain_res[PM_MAX_SCHEMES]; // per-step min_residency_ms
static uint8_t                 s_chain_len = 0;

/* pre-init staging (register / chain are called before init) */
static const TUYA_PM_SCHEME_T *s_custom[PM_MAX_SCHEMES];
static uint8_t                 s_custom_cnt = 0;
static TUYA_PM_CHAIN_STEP_T    s_chain_pending[PM_MAX_SCHEMES];
static uint8_t                 s_chain_pending_len = 0;
static BOOL_T                  s_chain_set = FALSE;

static uint8_t  s_lifeboat_id = 0;       // battery lifeboat target scheme id

static uint8_t  s_idle_target_idx = 0;   // chain index the idle decay drives toward
static uint8_t  s_current_idx     = 0;   // current effective chain index

static TUYA_PM_CHANGE_CB s_change_cb  = NULL;
static void             *s_change_arg = NULL;

static BOOL_T s_force_lifeboat = FALSE;   // low-battery lifeboat active, overrides everything

#if defined(ENABLE_POWER)
static TUYA_PM_BATTERY_POLICY_T s_batt_pending;          // staged by tuya_pm_set_battery (pre-init)
static BOOL_T              s_batt_pending_set = FALSE;
static TUYA_PM_BATTERY_POLICY_T s_battery;
static BOOL_T              s_has_battery = FALSE;
static TUYA_PM_LOCK_HANDLE s_charge_lock = NULL;  // held while charging (charging_holds_active)
static BOOL_T              s_charge_held = FALSE;
static BOOL_T              s_charging    = FALSE;  // latest charge state
static TIMER_ID            s_batt_timer  = NULL;   // periodic voltage poll for the lifeboat
#endif

/***********************************************************
********************** helpers *****************************
***********************************************************/

static const char *__id_name(uint8_t id)
{
    switch (id) {
    case TUYA_PM_ACTIVE:     return "ACTIVE";
    case TUYA_PM_CEC_T20:    return "CEC_T20";
    case TUYA_PM_ULP_ONLINE: return "ULP_ONLINE";
    case TUYA_PM_DEEPSLEEP:  return "DEEPSLEEP";
    default:                 return "custom";
    }
}

/* scheme id -> chain index, or -1 if not in the chain */
static int __idx_of(uint8_t id)
{
    uint8_t i;
    for (i = 0; i < s_chain_len; i++) {
        if (s_chain[i] == id) {
            return (int)i;
        }
    }
    return -1;
}

/* scheme id -> scheme descriptor, or NULL if not in the library */
static const TUYA_PM_SCHEME_T *__scheme_of(uint8_t id)
{
    uint8_t i;
    for (i = 0; i < s_lib_cnt; i++) {
        if (s_lib[i]->id == id) {
            return s_lib[i];
        }
    }
    return NULL;
}

/* effective scheme id at a depth: chain positions map to chain ids; depth == chain_len
 * is the battery lifeboat (whose scheme may be outside the chain). */
static uint8_t __id_at(uint8_t depth)
{
    return (depth >= s_chain_len) ? s_lifeboat_id : s_chain[depth];
}

/***********************************************************
******************** consumer suspend/resume ***************
***********************************************************/

static void __do_suspend(PM_CONSUMER_T *c)
{
    PR_DEBUG("[pm] suspend consumer '%s'", c->name);
#if defined(ENABLE_POWER)
    if (c->is_rail) {
        if (s_pwr) {
            tdl_power_domain_set((TDL_POWER_HANDLE)s_pwr, c->domain_mask, FALSE);
        }
        return;
    }
#endif
    if (c->suspend) {
        c->suspend(c->arg);
    }
}

static void __do_resume(PM_CONSUMER_T *c)
{
    PR_DEBUG("[pm] resume consumer '%s'", c->name);
#if defined(ENABLE_POWER)
    if (c->is_rail) {
        if (s_pwr) {
            tdl_power_domain_set((TDL_POWER_HANDLE)s_pwr, c->domain_mask, TRUE);
        }
        return;
    }
#endif
    if (c->resume) {
        c->resume(c->arg);
    }
}

/** Suspend consumers no longer powered at idx, highest priority first. Call unlocked. */
static void __consumers_suspend(uint8_t idx)
{
    for (;;) {
        LIST_HEAD     *pos  = NULL;
        PM_CONSUMER_T *c    = NULL;
        PM_CONSUMER_T *pick = NULL;
        int            best = -1;

        tal_mutex_lock(s_mutex);
        tuya_list_for_each(pos, &s_consumer_list) {
            c = tuya_list_entry(pos, PM_CONSUMER_T, node);
            if (c->powered && idx > c->min_idx && (int)c->priority > best) {
                pick = c;
                best = c->priority;
            }
        }
        if (pick) {
            pick->powered = FALSE; // mark under lock so it is not re-picked
        }
        tal_mutex_unlock(s_mutex);

        if (NULL == pick) {
            break;
        }
        __do_suspend(pick);
    }
}

/** Resume consumers powered again at idx, lowest priority first. Call unlocked. */
static void __consumers_resume(uint8_t idx)
{
    for (;;) {
        LIST_HEAD     *pos  = NULL;
        PM_CONSUMER_T *c    = NULL;
        PM_CONSUMER_T *pick = NULL;
        int            best = 0x7fffffff;

        tal_mutex_lock(s_mutex);
        tuya_list_for_each(pos, &s_consumer_list) {
            c = tuya_list_entry(pos, PM_CONSUMER_T, node);
            if (!c->powered && idx <= c->min_idx && (int)c->priority < best) {
                pick = c;
                best = c->priority;
            }
        }
        if (pick) {
            pick->powered = TRUE;
        }
        tal_mutex_unlock(s_mutex);

        if (NULL == pick) {
            break;
        }
        __do_resume(pick);
    }
}

/** Add a consumer and bring it to the right state for the current level. */
static void __consumer_add_reconcile(PM_CONSUMER_T *pc)
{
    BOOL_T should_power;

    tal_mutex_lock(s_mutex);
    tuya_list_add_tail(&pc->node, &s_consumer_list);
    should_power = (s_current_idx <= pc->min_idx);
    pc->powered  = should_power;
    tal_mutex_unlock(s_mutex);

    if (!should_power) {
        __do_suspend(pc); // current level is already deeper than it supports
    }
}

/***********************************************************
********************** apply / arbitration *****************
***********************************************************/

/** Apply the effective chain index: run the scheme's enter(). Call unlocked. */
static void __apply(uint8_t depth)
{
    uint8_t                 id = __id_at(depth);
    const TUYA_PM_SCHEME_T *s  = __scheme_of(id);

    if (s && s->ops && s->ops->enter) {
        s->ops->enter(s->ctx);
    }
    PR_DEBUG("[pm] apply -> %s", __id_name(id));
}

/** Arbitrate (must hold s_mutex): min(idle_target, every lock floor). Returns a depth in
 *  [0, chain_len]; chain_len means the battery lifeboat (deeper than the chain tail). */
static uint8_t __arbitrate_locked(void)
{
    LIST_HEAD *pos   = NULL;
    PM_LOCK_T *lk    = NULL;
    uint8_t    floor = (s_chain_len > 0) ? (uint8_t)(s_chain_len - 1) : 0; // no lock = chain tail
    uint8_t    t;

    if (s_force_lifeboat) {
        return s_chain_len; // lifeboat depth (deeper than the chain tail)
    }

    tuya_list_for_each(pos, &s_lock_list) {
        lk = tuya_list_entry(pos, PM_LOCK_T, node);
        if (lk->cnt > 0 && lk->floor_idx < floor) {
            floor = lk->floor_idx;
        }
    }

    t = s_idle_target_idx;
    if (floor < t) {
        t = floor; // locks pull the level shallower
    }
    return t;
}

/** Recompute the effective level; on change sequence consumers + scheme and fire cb. */
static void __recompute_apply(void)
{
    uint8_t newi, oldi;

    tal_mutex_lock(s_mutex);
    newi = __arbitrate_locked();
    oldi = s_current_idx;
    if (newi == oldi) {
        tal_mutex_unlock(s_mutex);
        return;
    }
    s_current_idx = newi;
    tal_mutex_unlock(s_mutex);

    if (newi > oldi) {
        // descending (deeper): consumers tear down first, then the scheme
        __consumers_suspend(newi);
        __apply(newi);
    } else {
        // ascending (shallower): scheme first, then consumers restore
        __apply(newi);
        __consumers_resume(newi);
    }

    if (s_change_cb) {
        s_change_cb(__id_at(oldi), __id_at(newi), s_change_arg);
    }
    PR_INFO("[pm] %s -> %s", __id_name(__id_at(oldi)), __id_name(__id_at(newi)));
}

/***********************************************************
********************** idle decay **************************
***********************************************************/

static void __arm_decay(void)
{
    uint8_t  next;
    uint32_t interval;

    tal_mutex_lock(s_mutex);
    next = (uint8_t)(s_idle_target_idx + 1);
    tal_mutex_unlock(s_mutex);

    if (next >= s_chain_len) {
        return; // idle decay stops at the chain tail
    }
    interval = s_chain_res[next]; // per-step residency (set by the chain user)
    if (0 == interval) {
        interval = 1; // 0 = decay right away
    }
    tal_sw_timer_start(s_decay_timer, interval, TAL_TIMER_ONCE);
}

static void __decay_cb(TIMER_ID id, void *arg)
{
    BOOL_T stepped = FALSE;
    (void)id;
    (void)arg;

    tal_mutex_lock(s_mutex);
    if (s_chain_len > 0 && s_idle_target_idx < (uint8_t)(s_chain_len - 1)) {
        s_idle_target_idx = (uint8_t)(s_idle_target_idx + 1);
        stepped = TRUE;
    }
    tal_mutex_unlock(s_mutex);

    if (stepped) {
        __recompute_apply();
        __arm_decay();
    }
}

/***********************************************************
********************** battery policy ***********************
***********************************************************/

#if defined(ENABLE_POWER)
/* Charge event: hold ACTIVE while charging; charging also clears the lifeboat. */
static void __on_charge(TDL_CHG_STATE_E st, void *arg)
{
    BOOL_T charging = (st == TDL_CHG_CHARGING || st == TDL_CHG_FULL);
    (void)arg;

    if (charging == s_charging) {
        return;
    }
    s_charging = charging;
    PR_NOTICE("[pm] charge -> %s", charging ? "charging" : "discharge");
    if (!s_has_battery) {
        return;
    }
    if (s_battery.charging_holds_active && s_charge_lock) {
        if (charging && !s_charge_held) {
            tuya_pm_lock_acquire(s_charge_lock);
            s_charge_held = TRUE;
        } else if (!charging && s_charge_held) {
            tuya_pm_lock_release(s_charge_lock);
            s_charge_held = FALSE;
        }
    }
    if (charging && s_force_lifeboat) {
        s_force_lifeboat = FALSE; // plugged in -> voltage recovering, leave lifeboat
        __recompute_apply();
    }
}

/* Periodic voltage poll: drop into (or out of) the low-battery lifeboat. */
static void __batt_poll_cb(TIMER_ID id, void *arg)
{
    uint32_t mv = 0;
    BOOL_T   low;
    (void)id;
    (void)arg;

    if (!s_has_battery || 0 == s_battery.lifeboat_below_mv || NULL == s_pwr) {
        return;
    }
    if (s_charging) { // on external power: never lifeboat (nothing to save)
        if (s_force_lifeboat) {
            s_force_lifeboat = FALSE;
            __recompute_apply();
        }
        return;
    }
    if (OPRT_OK != tdl_power_battery_get_voltage((TDL_POWER_HANDLE)s_pwr, &mv) || 0 == mv) {
        return;
    }
    low = (mv < s_battery.lifeboat_below_mv);
    if (low != s_force_lifeboat) {
        s_force_lifeboat = low;
        PR_NOTICE("[pm] battery %umV -> lifeboat %s", mv, low ? "engaged" : "cleared");
        __recompute_apply();
    }
}
#endif /* ENABLE_POWER */

/***********************************************************
**************** public: scheme library / chain ************
***********************************************************/

OPERATE_RET tuya_pm_scheme_register(const TUYA_PM_SCHEME_T *scheme)
{
    uint8_t i;

    if (s_inited) {
        return OPRT_COM_ERROR; // library is read-only after init
    }
    if (NULL == scheme || scheme->id < TUYA_PM_SCHEME_BUILTIN_MAX ||
        NULL == scheme->ops || NULL == scheme->ops->enter) {
        return OPRT_INVALID_PARM; // custom ids must be >= BUILTIN_MAX and have ops->enter
    }
    for (i = 0; i < s_custom_cnt; i++) {
        if (s_custom[i]->id == scheme->id) {
            return OPRT_INVALID_PARM; // duplicate id
        }
    }
    if (s_custom_cnt >= (PM_MAX_SCHEMES - TUYA_PM_SCHEME_BUILTIN_MAX)) {
        return OPRT_EXCEED_UPPER_LIMIT;
    }
    s_custom[s_custom_cnt++] = scheme;
    return OPRT_OK;
}

OPERATE_RET tuya_pm_set_chain(const TUYA_PM_CHAIN_STEP_T *steps, uint8_t n)
{
    uint8_t i;

    if (s_inited) {
        return OPRT_COM_ERROR; // chain is read-only after init
    }
    if (NULL == steps || 0 == n || n > PM_MAX_SCHEMES) {
        return OPRT_INVALID_PARM;
    }
    for (i = 0; i < n; i++) {
        s_chain_pending[i] = steps[i];
    }
    s_chain_pending_len = n;
    s_chain_set         = TRUE;
    return OPRT_OK;
}

OPERATE_RET tuya_pm_set_battery(const TUYA_PM_BATTERY_POLICY_T *batt)
{
#if defined(ENABLE_POWER)
    if (s_inited) {
        return OPRT_COM_ERROR; // read-only after init
    }
    if (NULL == batt) {
        return OPRT_INVALID_PARM;
    }
    s_batt_pending     = *batt;
    s_batt_pending_set = TRUE;
    return OPRT_OK;
#else
    (void)batt;
    return OPRT_NOT_SUPPORTED;
#endif
}

/***********************************************************
********************** public: init ************************
***********************************************************/

/* Assemble the scheme library (built-ins + registered) and validate the chain. */
static OPERATE_RET __build_library_and_chain(void)
{
    uint8_t                 bcnt = 0;
    const TUYA_PM_SCHEME_T *bt   = tuya_pm_get_preset_schemes(&bcnt);
    uint8_t                 i;

    s_lib_cnt = 0;
    for (i = 0; i < bcnt && s_lib_cnt < PM_MAX_SCHEMES; i++) {
        s_lib[s_lib_cnt++] = &bt[i];
    }
    for (i = 0; i < s_custom_cnt && s_lib_cnt < PM_MAX_SCHEMES; i++) {
        s_lib[s_lib_cnt++] = s_custom[i];
    }

    if (s_chain_set) {
        s_chain_len = 0;
        for (i = 0; i < s_chain_pending_len; i++) {
            if (NULL == __scheme_of(s_chain_pending[i].scheme_id)) {
                PR_ERR("[pm] chain references unknown scheme id %d", s_chain_pending[i].scheme_id);
                return OPRT_INVALID_PARM;
            }
            s_chain[s_chain_len]     = s_chain_pending[i].scheme_id;
            s_chain_res[s_chain_len] = s_chain_pending[i].min_residency_ms;
            s_chain_len++;
        }
    } else {
        s_chain[0]     = TUYA_PM_ACTIVE; // default: no auto-decay
        s_chain_res[0] = 0;
        s_chain_len    = 1;
    }
    return OPRT_OK;
}

OPERATE_RET tuya_pm_init(const char *power_dev_name)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_inited) {
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&s_mutex));
    INIT_LIST_HEAD(&s_lock_list);
    INIT_LIST_HEAD(&s_consumer_list);

    rt = __build_library_and_chain();
    if (OPRT_OK != rt) {
        tal_mutex_release(s_mutex);
        s_mutex = NULL;
        return rt;
    }

    s_idle_target_idx = 0;
    s_current_idx     = 0;

#if defined(ENABLE_POWER)
    if (power_dev_name) {
        s_pwr = tdl_power_find(power_dev_name);
        if (NULL == s_pwr) {
            PR_NOTICE("[pm] power device '%s' not found; rail consumers disabled", power_dev_name);
        }
    }
#endif

    rt = tal_sw_timer_create(__decay_cb, NULL, &s_decay_timer);
    if (OPRT_OK != rt) {
        tal_mutex_release(s_mutex);
        s_mutex = NULL;
        return rt;
    }

    s_inited = TRUE;

    {
        /* one-time init for each scheme in the chain; pass the deep-sleep power handle.
           the shared WiFi-PS/CPU backend comes up lazily inside the PS schemes' init. */
        uint8_t k;
        for (k = 0; k < s_chain_len; k++) {
            const TUYA_PM_SCHEME_T *s = __scheme_of(s_chain[k]);
            if (s && s->ops && s->ops->init) {
                s->ops->init(s->ctx, s_pwr);
            }
        }
    }
    __apply(0);      // start at the chain head (ACTIVE)
    __arm_decay();

#if defined(ENABLE_POWER)
    /* Battery policy: charging holds ACTIVE; low voltage engages the lifeboat. */
    if (s_batt_pending_set && s_pwr) {
        TDL_CHG_STATE_E st = TDL_CHG_DISCHARGE;

        s_battery     = s_batt_pending;
        s_has_battery = TRUE;
        s_lifeboat_id = s_battery.lifeboat_scheme;

        /* if the lifeboat scheme is outside the chain, init it here too (e.g. to bind the
           deep-sleep power handle); chain schemes were already init'd above. */
        if (s_battery.lifeboat_below_mv && __idx_of(s_lifeboat_id) < 0) {
            const TUYA_PM_SCHEME_T *ls = __scheme_of(s_lifeboat_id);
            if (ls && ls->ops && ls->ops->init) {
                ls->ops->init(ls->ctx, s_pwr);
            }
        }

        if (s_battery.charging_holds_active) {
            tuya_pm_lock_create("charging", TUYA_PM_ACTIVE, &s_charge_lock);
        }
        tdl_power_charger_on_event((TDL_POWER_HANDLE)s_pwr, __on_charge, NULL);
        if (OPRT_OK == tdl_power_charger_get_state((TDL_POWER_HANDLE)s_pwr, &st)) {
            __on_charge(st, NULL); // seed with the current charge state
        }
        if (s_battery.lifeboat_below_mv &&
            OPRT_OK == tal_sw_timer_create(__batt_poll_cb, NULL, &s_batt_timer)) {
            tal_sw_timer_start(s_batt_timer, PM_BATT_POLL_MS, TAL_TIMER_CYCLE);
            __batt_poll_cb(NULL, NULL); // check immediately
        }
    }
#endif

    PR_NOTICE("[pm] init ok, chain_len=%d tail=%s", s_chain_len,
              (s_chain_len > 0) ? __id_name(s_chain[s_chain_len - 1]) : "?");
    return OPRT_OK;
}

/***********************************************************
********************** public: locks ***********************
***********************************************************/

OPERATE_RET tuya_pm_lock_create(const char *name, uint8_t floor, TUYA_PM_LOCK_HANDLE *out)
{
    PM_LOCK_T *lk = NULL;
    int        fi;

    if (!s_inited) {
        return OPRT_RESOURCE_NOT_READY;
    }
    fi = __idx_of(floor);
    if (NULL == out || fi < 0) {
        return OPRT_INVALID_PARM; // floor must be a scheme in the chain
    }
    lk = (PM_LOCK_T *)tal_malloc(sizeof(PM_LOCK_T));
    if (NULL == lk) {
        return OPRT_MALLOC_FAILED;
    }
    memset(lk, 0, sizeof(PM_LOCK_T));
    if (name) {
        strncpy(lk->name, name, PM_NAME_LEN - 1);
    }
    lk->floor_idx = (uint8_t)fi;
    lk->cnt       = 0;

    tal_mutex_lock(s_mutex);
    tuya_list_add(&lk->node, &s_lock_list);
    tal_mutex_unlock(s_mutex);

    *out = (TUYA_PM_LOCK_HANDLE)lk;
    return OPRT_OK;
}

OPERATE_RET tuya_pm_lock_acquire(TUYA_PM_LOCK_HANDLE lkh)
{
    PM_LOCK_T *lk = (PM_LOCK_T *)lkh;

    if (!s_inited || NULL == lk) {
        return OPRT_INVALID_PARM;
    }
    tal_mutex_lock(s_mutex);
    lk->cnt++;
    tal_mutex_unlock(s_mutex);
    __recompute_apply();
    return OPRT_OK;
}

OPERATE_RET tuya_pm_lock_release(TUYA_PM_LOCK_HANDLE lkh)
{
    PM_LOCK_T *lk = (PM_LOCK_T *)lkh;

    if (!s_inited || NULL == lk) {
        return OPRT_INVALID_PARM;
    }
    tal_mutex_lock(s_mutex);
    if (lk->cnt > 0) {
        lk->cnt--;
    }
    tal_mutex_unlock(s_mutex);
    __recompute_apply();
    return OPRT_OK;
}

OPERATE_RET tuya_pm_activity(void)
{
    if (!s_inited) {
        return OPRT_RESOURCE_NOT_READY;
    }
    tal_mutex_lock(s_mutex);
    s_idle_target_idx = 0; // chain head
    tal_mutex_unlock(s_mutex);

    tal_sw_timer_stop(s_decay_timer);
    __recompute_apply();
    __arm_decay();
    return OPRT_OK;
}

OPERATE_RET tuya_pm_request(uint8_t scheme_id)
{
    int di;

    if (!s_inited) {
        return OPRT_RESOURCE_NOT_READY;
    }
    di = __idx_of(scheme_id);
    if (di < 0) {
        return OPRT_INVALID_PARM; // not in the chain
    }
    tal_sw_timer_stop(s_decay_timer); // explicit hold: no auto-decay
    tal_mutex_lock(s_mutex);
    s_idle_target_idx = (uint8_t)di;
    tal_mutex_unlock(s_mutex);
    __recompute_apply();
    return OPRT_OK;
}

/***********************************************************
******************** public: consumers *********************
***********************************************************/

OPERATE_RET tuya_pm_consumer_register(const TUYA_PM_CONSUMER_T *c, TUYA_PM_CONSUMER_HANDLE *out)
{
    PM_CONSUMER_T *pc = NULL;
    int            mi;

    if (!s_inited) {
        return OPRT_RESOURCE_NOT_READY;
    }
    if (NULL == c || NULL == out) {
        return OPRT_INVALID_PARM;
    }
    mi = __idx_of(c->min_powered_level);
    if (mi < 0) {
        return OPRT_INVALID_PARM; // min must be a scheme in the chain
    }
    pc = (PM_CONSUMER_T *)tal_malloc(sizeof(PM_CONSUMER_T));
    if (NULL == pc) {
        return OPRT_MALLOC_FAILED;
    }
    memset(pc, 0, sizeof(PM_CONSUMER_T));
    if (c->name) {
        strncpy(pc->name, c->name, PM_NAME_LEN - 1);
    }
    pc->min_idx  = (uint8_t)mi;
    pc->priority = c->priority;
    pc->suspend  = c->suspend;
    pc->resume   = c->resume;
    pc->arg      = c->arg;
    pc->is_rail  = FALSE;

    __consumer_add_reconcile(pc);
    *out = (TUYA_PM_CONSUMER_HANDLE)pc;
    return OPRT_OK;
}

OPERATE_RET tuya_pm_consumer_register_rail(const char *name, uint32_t domain_mask,
                                           uint8_t min_powered_level,
                                           uint8_t priority, TUYA_PM_CONSUMER_HANDLE *out)
{
#if defined(ENABLE_POWER)
    PM_CONSUMER_T *pc = NULL;
    int            mi;

    if (!s_inited) {
        return OPRT_RESOURCE_NOT_READY;
    }
    mi = __idx_of(min_powered_level);
    if (NULL == out || mi < 0) {
        return OPRT_INVALID_PARM;
    }
    if (NULL == s_pwr) {
        return OPRT_RESOURCE_NOT_READY; // no tdl_power device bound
    }
    pc = (PM_CONSUMER_T *)tal_malloc(sizeof(PM_CONSUMER_T));
    if (NULL == pc) {
        return OPRT_MALLOC_FAILED;
    }
    memset(pc, 0, sizeof(PM_CONSUMER_T));
    if (name) {
        strncpy(pc->name, name, PM_NAME_LEN - 1);
    }
    pc->min_idx     = (uint8_t)mi;
    pc->priority    = priority;
    pc->is_rail     = TRUE;
    pc->domain_mask = domain_mask;

    __consumer_add_reconcile(pc);
    *out = (TUYA_PM_CONSUMER_HANDLE)pc;
    return OPRT_OK;
#else
    (void)name; (void)domain_mask; (void)min_powered_level; (void)priority; (void)out;
    return OPRT_NOT_SUPPORTED;
#endif
}

OPERATE_RET tuya_pm_consumer_unregister(TUYA_PM_CONSUMER_HANDLE h)
{
    PM_CONSUMER_T *pc = (PM_CONSUMER_T *)h;

    if (!s_inited || NULL == pc) {
        return OPRT_INVALID_PARM;
    }
    tal_mutex_lock(s_mutex);
    tuya_list_del(&pc->node);
    tal_mutex_unlock(s_mutex);

    if (!pc->powered) {
        __do_resume(pc); // restore hardware before dropping the tracking
    }
    tal_free(pc);
    return OPRT_OK;
}

/***********************************************************
******************** public: observe ***********************
***********************************************************/

uint8_t tuya_pm_current(void)
{
    return (s_chain_len > 0) ? __id_at(s_current_idx) : TUYA_PM_ACTIVE;
}

OPERATE_RET tuya_pm_on_change(TUYA_PM_CHANGE_CB cb, void *arg)
{
    s_change_cb  = cb;
    s_change_arg = arg;
    return OPRT_OK;
}

void tuya_pm_dump(void)
{
    LIST_HEAD     *pos = NULL;
    PM_LOCK_T     *lk  = NULL;
    PM_CONSUMER_T *c   = NULL;
    uint8_t        i;

    if (!s_inited) {
        PR_NOTICE("[pm] not inited");
        return;
    }
    PR_NOTICE("[pm] cur=%s idle_target=%s lifeboat=%s (chain_len=%d)",
              __id_name(__id_at(s_current_idx)), __id_name(s_chain[s_idle_target_idx]),
              s_force_lifeboat ? __id_name(s_lifeboat_id) : "-", s_chain_len);
    PR_NOTICE("[pm]   chain:");
    for (i = 0; i < s_chain_len; i++) {
        PR_NOTICE("[pm]     [%d] %s", i, __id_name(s_chain[i]));
    }
    tal_mutex_lock(s_mutex);
    tuya_list_for_each(pos, &s_lock_list) {
        lk = tuya_list_entry(pos, PM_LOCK_T, node);
        PR_NOTICE("[pm]   lock '%s' floor=%s cnt=%d", lk->name, __id_name(s_chain[lk->floor_idx]), lk->cnt);
    }
    tuya_list_for_each(pos, &s_consumer_list) {
        c = tuya_list_entry(pos, PM_CONSUMER_T, node);
        PR_NOTICE("[pm]   consumer '%s' min=%s pri=%d powered=%d%s", c->name,
                  __id_name(s_chain[c->min_idx]), c->priority, c->powered, c->is_rail ? " (rail)" : "");
    }
    tal_mutex_unlock(s_mutex);
}
