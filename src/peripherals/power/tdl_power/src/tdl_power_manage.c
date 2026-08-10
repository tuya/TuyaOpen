/**
 * @file tdl_power_manage.c
 * @brief TDL power device. A device aggregates one or more backend "contributors"
 *        (each a TDD registration), so a single "power" device can span mixed
 *        mechanisms (e.g. AXP2101 channels + SoC GPIO rails). App operations are
 *        fanned out to the contributors; the one that owns a role/capability handles
 *        it, the others return OPRT_NOT_SUPPORTED and are skipped.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "string.h"
#include "tal_memory.h"
#include "tal_log.h"
#include "tal_semaphore.h"
#include "tal_thread.h"
#include "tuya_list.h"
#include "tdl_power_driver.h"
#include "tdl_power_manage.h"

#define POWER_NAME_LEN 16

/* one backend registration */
typedef struct {
    LIST_HEAD              node;
    TDL_POWER_INTFS_T      intfs;
    TDD_POWER_DEV_HANDLE_T ctx;
    TDL_POWER_INFO_T       info;
    volatile uint8_t       chg_pending; // set by notify (ISR), drained by the worker
} POWER_CONTRIB_T;

/* one power device = a list of contributors + the app charger callback */
typedef struct {
    LIST_HEAD        node;
    char             name[POWER_NAME_LEN];
    LIST_HEAD        contribs;
    TDL_CHG_EVENT_CB chg_cb;
    void            *chg_arg;
} POWER_DEV_T;

static LIST_HEAD s_power_list = {&s_power_list, &s_power_list};

static SEM_HANDLE    s_chg_sem  = NULL;
static THREAD_HANDLE s_chg_thrd = NULL;

/***********************************************************
********************* registry / lookup ********************
***********************************************************/

static POWER_DEV_T *__power_find(const char *name)
{
    LIST_HEAD   *pos = NULL;
    POWER_DEV_T *dev = NULL;

    tuya_list_for_each(pos, &s_power_list) {
        dev = tuya_list_entry(pos, POWER_DEV_T, node);
        if (0 == strcmp(dev->name, name)) {
            return dev;
        }
    }
    return NULL;
}

OPERATE_RET tdl_power_register(const char *name, const TDL_POWER_INTFS_T *intfs,
                               const TDL_POWER_INFO_T *info, TDD_POWER_DEV_HANDLE_T ctx)
{
    POWER_DEV_T     *dev = NULL;
    POWER_CONTRIB_T *cb  = NULL;

    if (NULL == name || NULL == intfs) {
        return OPRT_INVALID_PARM;
    }

    dev = __power_find(name);
    if (NULL == dev) { // first contributor -> create the device
        dev = (POWER_DEV_T *)tal_malloc(sizeof(POWER_DEV_T));
        if (NULL == dev) {
            return OPRT_MALLOC_FAILED;
        }
        memset(dev, 0, sizeof(POWER_DEV_T));
        strncpy(dev->name, name, POWER_NAME_LEN - 1);
        INIT_LIST_HEAD(&dev->contribs);
        tuya_list_add(&dev->node, &s_power_list);
    }

    cb = (POWER_CONTRIB_T *)tal_malloc(sizeof(POWER_CONTRIB_T));
    if (NULL == cb) {
        return OPRT_MALLOC_FAILED;
    }
    memset(cb, 0, sizeof(POWER_CONTRIB_T));
    cb->intfs = *intfs;
    if (NULL != info) {
        cb->info = *info;
    }
    cb->ctx = ctx;
    tuya_list_add(&cb->node, &dev->contribs);
    return OPRT_OK;
}

TDL_POWER_HANDLE tdl_power_find(const char *name)
{
    if (NULL == name) {
        return NULL;
    }
    return (TDL_POWER_HANDLE)__power_find(name);
}

/***********************************************************
*********************** power_domain ***********************
***********************************************************/

OPERATE_RET tdl_power_domain_set(TDL_POWER_HANDLE h, uint32_t domain_mask, BOOL_T on)
{
    POWER_DEV_T *dev = (POWER_DEV_T *)h;
    LIST_HEAD   *pos = NULL;
    OPERATE_RET  rt  = OPRT_OK;

    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }

    for (uint8_t b = 0; b < 32; b++) {
        uint32_t bit = 1u << b;
        if (0 == (domain_mask & bit)) {
            continue;
        }
        /* hand this role to whichever contributor owns it */
        tuya_list_for_each(pos, &dev->contribs) {
            POWER_CONTRIB_T *c = tuya_list_entry(pos, POWER_CONTRIB_T, node);
            if (NULL == c->intfs.domain_set) {
                continue;
            }
            OPERATE_RET r = c->intfs.domain_set(c->ctx, (TDL_POWER_DOMAIN_E)bit, on);
            if (OPRT_NOT_SUPPORTED == r) {
                continue; // not this contributor's role
            }
            if (OPRT_OK != r) {
                rt = r;
            }
            break; // owned (ok or hard error)
        }
    }
    return rt;
}

OPERATE_RET tdl_power_domain_get(TDL_POWER_HANDLE h, TDL_POWER_DOMAIN_E domain, BOOL_T *on)
{
    POWER_DEV_T *dev = (POWER_DEV_T *)h;
    LIST_HEAD   *pos = NULL;

    if (NULL == dev || NULL == on) {
        return OPRT_INVALID_PARM;
    }
    tuya_list_for_each(pos, &dev->contribs) {
        POWER_CONTRIB_T *c = tuya_list_entry(pos, POWER_CONTRIB_T, node);
        if (NULL == c->intfs.domain_get) {
            continue;
        }
        OPERATE_RET r = c->intfs.domain_get(c->ctx, domain, on);
        if (OPRT_NOT_SUPPORTED != r) {
            return r;
        }
    }
    return OPRT_NOT_SUPPORTED;
}

/***********************************************************
************************** battery *************************
***********************************************************/

/* percent derivation used when no contributor has a hardware fuel gauge */
static uint8_t __percent_from_voltage(const TDL_POWER_BATTERY_INFO_T *b, uint32_t mv)
{
    if (b->curve && b->curve_cnt >= 2) {
        const TDL_POWER_OCV_PT_T *c = b->curve;
        uint8_t                   n = b->curve_cnt;
        if (mv <= c[0].mv) {
            return c[0].pct;
        }
        if (mv >= c[n - 1].mv) {
            return c[n - 1].pct;
        }
        for (uint8_t i = 0; i < n - 1; i++) {
            if (mv <= c[i + 1].mv) {
                uint32_t lo = c[i].mv, hi = c[i + 1].mv;
                int      plo = c[i].pct, phi = c[i + 1].pct;
                return (uint8_t)(plo + (int)((mv - lo) * (phi - plo) / (hi - lo)));
            }
        }
        return c[n - 1].pct;
    }

    if (b->v_full_mv <= b->v_empty_mv) {
        return 0;
    }
    if (mv <= b->v_empty_mv) {
        return 0;
    }
    if (mv >= b->v_full_mv) {
        return 100;
    }
    return (uint8_t)((mv - b->v_empty_mv) * 100 / (b->v_full_mv - b->v_empty_mv));
}

/* the contributor that provides the battery capability (voltage op), if any */
static POWER_CONTRIB_T *__battery_contrib(POWER_DEV_T *dev)
{
    LIST_HEAD *pos = NULL;
    tuya_list_for_each(pos, &dev->contribs) {
        POWER_CONTRIB_T *c = tuya_list_entry(pos, POWER_CONTRIB_T, node);
        if (NULL != c->intfs.battery_get_voltage || NULL != c->intfs.battery_get_percent) {
            return c;
        }
    }
    return NULL;
}

OPERATE_RET tdl_power_battery_get_voltage(TDL_POWER_HANDLE h, uint32_t *mv)
{
    POWER_DEV_T     *dev = (POWER_DEV_T *)h;
    POWER_CONTRIB_T *c   = NULL;

    if (NULL == dev || NULL == mv) {
        return OPRT_INVALID_PARM;
    }
    c = __battery_contrib(dev);
    if (NULL == c || NULL == c->intfs.battery_get_voltage) {
        return OPRT_NOT_SUPPORTED;
    }
    return c->intfs.battery_get_voltage(c->ctx, mv);
}

OPERATE_RET tdl_power_battery_get_percent(TDL_POWER_HANDLE h, uint8_t *pct)
{
    POWER_DEV_T     *dev = (POWER_DEV_T *)h;
    POWER_CONTRIB_T *c   = NULL;
    uint32_t         mv  = 0;
    OPERATE_RET      rt  = OPRT_OK;

    if (NULL == dev || NULL == pct) {
        return OPRT_INVALID_PARM;
    }
    c = __battery_contrib(dev);
    if (NULL == c) {
        return OPRT_NOT_SUPPORTED;
    }
    if (NULL != c->intfs.battery_get_percent) {
        return c->intfs.battery_get_percent(c->ctx, pct); // hardware gauge
    }
    rt = c->intfs.battery_get_voltage(c->ctx, &mv);
    if (OPRT_OK != rt) {
        return rt;
    }
    *pct = __percent_from_voltage(&c->info.battery, mv); // TDL derives
    return OPRT_OK;
}

OPERATE_RET tdl_power_get_info(TDL_POWER_HANDLE h, TDL_POWER_INFO_T *info)
{
    POWER_DEV_T     *dev = (POWER_DEV_T *)h;
    POWER_CONTRIB_T *c   = NULL;
    LIST_HEAD       *pos = NULL;

    if (NULL == dev || NULL == info) {
        return OPRT_INVALID_PARM;
    }
    c = __battery_contrib(dev); // the battery owner carries the landmarks
    if (NULL == c) {            // no battery -> fall back to the first contributor
        tuya_list_for_each(pos, &dev->contribs) {
            c = tuya_list_entry(pos, POWER_CONTRIB_T, node);
            break;
        }
    }
    if (NULL == c) {
        return OPRT_NOT_FOUND;
    }
    *info = c->info;
    return OPRT_OK;
}

/***********************************************************
************************** charger *************************
***********************************************************/

OPERATE_RET tdl_power_charger_get_state(TDL_POWER_HANDLE h, TDL_CHG_STATE_E *st)
{
    POWER_DEV_T *dev = (POWER_DEV_T *)h;
    LIST_HEAD   *pos = NULL;

    if (NULL == dev || NULL == st) {
        return OPRT_INVALID_PARM;
    }
    tuya_list_for_each(pos, &dev->contribs) {
        POWER_CONTRIB_T *c = tuya_list_entry(pos, POWER_CONTRIB_T, node);
        if (NULL != c->intfs.charger_get_state) {
            return c->intfs.charger_get_state(c->ctx, st);
        }
    }
    return OPRT_NOT_SUPPORTED;
}

static void __chg_worker(void *arg)
{
    LIST_HEAD      *dpos = NULL, *cpos = NULL;
    POWER_DEV_T    *dev  = NULL;
    POWER_CONTRIB_T *c   = NULL;
    TDL_CHG_STATE_E st;

    (void)arg;
    for (;;) {
        if (OPRT_OK != tal_semaphore_wait(s_chg_sem, SEM_WAIT_FOREVER)) {
            continue;
        }
        tuya_list_for_each(dpos, &s_power_list) {
            dev = tuya_list_entry(dpos, POWER_DEV_T, node);
            tuya_list_for_each(cpos, &dev->contribs) {
                c = tuya_list_entry(cpos, POWER_CONTRIB_T, node);
                if (!c->chg_pending) {
                    continue;
                }
                c->chg_pending = 0;
                if (NULL != dev->chg_cb && NULL != c->intfs.charger_get_state &&
                    OPRT_OK == c->intfs.charger_get_state(c->ctx, &st)) {
                    dev->chg_cb(st, dev->chg_arg);
                }
            }
        }
    }
}

void tdl_power_charger_irq_notify(TDD_POWER_DEV_HANDLE_T ctx)
{
    LIST_HEAD *dpos = NULL, *cpos = NULL;

    // Mark the firing contributor (identified by its own ctx) pending, wake the worker.
    tuya_list_for_each(dpos, &s_power_list) {
        POWER_DEV_T *dev = tuya_list_entry(dpos, POWER_DEV_T, node);
        tuya_list_for_each(cpos, &dev->contribs) {
            POWER_CONTRIB_T *c = tuya_list_entry(cpos, POWER_CONTRIB_T, node);
            if (c->ctx == ctx) {
                c->chg_pending = 1;
                if (NULL != s_chg_sem) {
                    tal_semaphore_post(s_chg_sem);
                }
                return;
            }
        }
    }
}

OPERATE_RET tdl_power_charger_on_event(TDL_POWER_HANDLE h, TDL_CHG_EVENT_CB cb, void *arg)
{
    POWER_DEV_T *dev   = (POWER_DEV_T *)h;
    LIST_HEAD   *pos   = NULL;
    OPERATE_RET  rt    = OPRT_OK;
    BOOL_T       armed = FALSE;

    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }

    dev->chg_cb  = cb;
    dev->chg_arg = arg;

    if (NULL == s_chg_sem) { // lazily start the shared worker
        THREAD_CFG_T tcfg = {.stackDepth = 2048, .priority = THREAD_PRIO_1, .thrdname = "pwr_chg"};
        TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&s_chg_sem, 0, 1));
        TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&s_chg_thrd, NULL, NULL, __chg_worker, NULL, &tcfg));
    }

    /* arm every contributor that can raise charger events */
    tuya_list_for_each(pos, &dev->contribs) {
        POWER_CONTRIB_T *c = tuya_list_entry(pos, POWER_CONTRIB_T, node);
        if (NULL != c->intfs.charger_arm_event && OPRT_OK == c->intfs.charger_arm_event(c->ctx)) {
            armed = TRUE;
        }
    }
    return armed ? OPRT_OK : OPRT_NOT_SUPPORTED;
}

/***********************************************************
*********************** deep sleep *************************
***********************************************************/

OPERATE_RET tdl_power_enter_deepsleep(TDL_POWER_HANDLE h, uint32_t timer_wake_ms)
{
    POWER_DEV_T *dev = (POWER_DEV_T *)h;
    LIST_HEAD   *pos = NULL;

    if (NULL == dev) {
        return OPRT_INVALID_PARM;
    }
    /* hand off to whichever backend implements deep sleep (SoC, PMIC, ...) */
    tuya_list_for_each(pos, &dev->contribs) {
        POWER_CONTRIB_T *c = tuya_list_entry(pos, POWER_CONTRIB_T, node);
        if (NULL != c->intfs.enter_deepsleep) {
            return c->intfs.enter_deepsleep(c->ctx, timer_wake_ms);
        }
    }
    return OPRT_NOT_SUPPORTED;
}
