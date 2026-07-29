/**
 * @file tdd_power_soc.c
 * @brief SoC power backend: GPIO load-switch domains, ADC divider battery,
 *        GPIO charge-status detection. For boards without an integrated PMIC.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "string.h"
#include "tal_memory.h"
#include "tal_log.h"
#include "tkl_gpio.h"
#include "tkl_adc.h"
#include "tdl_power_driver.h"
#include "tdd_power_soc.h"

// T5 SARADC raw->tap-voltage model V = (raw - LOW)/SPAN + 1.0V. Same chip on all
// T5 boards; a board leaves cal_low/cal_span 0 to use these, overrides if it drifts.
#define SOC_ADC_CAL_LOW_DEFAULT  2469
#define SOC_ADC_CAL_SPAN_DEFAULT 2429

typedef struct {
    TDD_POWER_SOC_CFG_T cfg;          // copied; sub-pointers reference static board data
    uint32_t            batt_filt_mv; // EMA-filtered battery voltage, 0 = not seeded yet
} POWER_SOC_CTX_T;

static TUYA_GPIO_LEVEL_E __level_inv(TUYA_GPIO_LEVEL_E lv)
{
    return (TUYA_GPIO_LEVEL_HIGH == lv) ? TUYA_GPIO_LEVEL_LOW : TUYA_GPIO_LEVEL_HIGH;
}

/* ---- power_domain ---- */

static const TDD_POWER_GPIO_DOMAIN_T *__domain_of(POWER_SOC_CTX_T *c, TDL_POWER_DOMAIN_E role)
{
    for (uint8_t i = 0; i < c->cfg.domain_cnt; i++) {
        if (c->cfg.domains[i].role == role) {
            return &c->cfg.domains[i];
        }
    }
    return NULL;
}

static OPERATE_RET __soc_domain_set(TDD_POWER_DEV_HANDLE_T ctx, TDL_POWER_DOMAIN_E role, BOOL_T on)
{
    POWER_SOC_CTX_T               *c = (POWER_SOC_CTX_T *)ctx;
    const TDD_POWER_GPIO_DOMAIN_T *d = __domain_of(c, role);

    if (NULL == d) {
        return OPRT_NOT_SUPPORTED; // role not on this board -> skipped by TDL
    }
    return tkl_gpio_write(d->pin, on ? d->active_level : __level_inv(d->active_level));
}

static OPERATE_RET __soc_domain_get(TDD_POWER_DEV_HANDLE_T ctx, TDL_POWER_DOMAIN_E role, BOOL_T *on)
{
    POWER_SOC_CTX_T               *c  = (POWER_SOC_CTX_T *)ctx;
    const TDD_POWER_GPIO_DOMAIN_T *d  = __domain_of(c, role);
    TUYA_GPIO_LEVEL_E              lv = TUYA_GPIO_LEVEL_LOW;
    OPERATE_RET                    rt = OPRT_OK;

    if (NULL == d) {
        return OPRT_NOT_SUPPORTED;
    }
    rt = tkl_gpio_read(d->pin, &lv);
    if (OPRT_OK != rt) {
        return rt;
    }
    *on = (lv == d->active_level) ? TRUE : FALSE;
    return OPRT_OK;
}

/* ---- battery (ADC divider) ---- */

static OPERATE_RET __soc_battery_get_voltage(TDD_POWER_DEV_HANDLE_T ctx, uint32_t *mv)
{
    POWER_SOC_CTX_T               *c   = (POWER_SOC_CTX_T *)ctx;
    const TDD_POWER_ADC_BATTERY_T *b   = c->cfg.battery;
    int32_t                        raw = 0, raw_max = 0;
    uint8_t                        got = 0;
    uint16_t                       low, span;
    float                          tap_mv;

    if (NULL == b) {
        return OPRT_NOT_SUPPORTED;
    }

    // Peak of several reads: the high-Z tap sags under each SAR conversion, so the
    // maximum is the freshest, least-disturbed sample.
    for (uint8_t i = 0; i < (b->samples ? b->samples : 1); i++) {
        raw = 0;
        if (OPRT_OK == tkl_adc_read_single_channel(b->adc_num, b->adc_ch, &raw)) {
            if (raw > raw_max) {
                raw_max = raw;
            }
            got++;
        }
    }
    if (0 == got) {
        return OPRT_COM_ERROR;
    }

    low    = b->cal_low ? b->cal_low : SOC_ADC_CAL_LOW_DEFAULT;
    span   = b->cal_span ? b->cal_span : SOC_ADC_CAL_SPAN_DEFAULT;
    tap_mv = (((float)(raw_max - low) / (float)span) + 1.0f) * 1000.0f;
    if (tap_mv < 0.0f) {
        tap_mv = 0.0f;
    }

    // EMA (K=8): even after peak-picking, the raw read still wobbles a few mV, which
    // makes the derived percent bounce (e.g. 57->55->56). Smooth it so the reported
    // voltage/percent settle. Seeded on the first read; converges over ~8 reads.
    uint32_t inst = (uint32_t)(tap_mv * b->divider_ratio);
    if (0 == c->batt_filt_mv) {
        c->batt_filt_mv = inst;
    } else {
        c->batt_filt_mv = (c->batt_filt_mv * 7 + inst) / 8;
    }
    *mv = c->batt_filt_mv;
    return OPRT_OK;
}

/* ---- charger (GPIO status lines) ---- */

static OPERATE_RET __soc_charger_get_state(TDD_POWER_DEV_HANDLE_T ctx, TDL_CHG_STATE_E *st)
{
    POWER_SOC_CTX_T                *c  = (POWER_SOC_CTX_T *)ctx;
    const TDD_POWER_GPIO_CHARGER_T *ch = c->cfg.charger;
    TUYA_GPIO_LEVEL_E               lv;
    BOOL_T                          charging = FALSE, full = FALSE;

    if (NULL == ch) {
        return OPRT_NOT_SUPPORTED;
    }
    if (OPRT_OK == tkl_gpio_read(ch->chrg_pin, &lv)) {
        charging = (lv == ch->chrg_active) ? TRUE : FALSE;
    }
    if (TDD_POWER_PIN_NONE != ch->stdby_pin && OPRT_OK == tkl_gpio_read(ch->stdby_pin, &lv)) {
        full = (lv == ch->stdby_active) ? TRUE : FALSE;
    }

    // STDBY takes priority: once "done" is reported CHRG has already released.
    *st = full ? TDL_CHG_FULL : (charging ? TDL_CHG_CHARGING : TDL_CHG_DISCHARGE);
    return OPRT_OK;
}

static void __soc_charger_isr(void *arg);

// Arm one status line. Some SoCs (e.g. T5) don't support double-edge IRQ, so we pick
// a single edge from the pin's CURRENT level and re-arm the opposite edge on each
// interrupt — catching both plug and unplug. Level read + IRQ config are register-only,
// safe in hard-ISR context (unlike an I2C fuel gauge, which is why the app cb defers).
static void __soc_charger_arm_pin(POWER_SOC_CTX_T *c, TUYA_GPIO_NUM_E pin)
{
    TUYA_GPIO_LEVEL_E lv  = TUYA_GPIO_LEVEL_LOW;
    TUYA_GPIO_IRQ_T   irq = {0};

    tkl_gpio_read(pin, &lv);
    // currently HIGH -> the next change is a falling edge; currently LOW -> rising.
    irq.mode = (TUYA_GPIO_LEVEL_HIGH == lv) ? TUYA_GPIO_IRQ_FALL : TUYA_GPIO_IRQ_RISE;
    irq.cb   = __soc_charger_isr;
    irq.arg  = c;
    tkl_gpio_irq_init(pin, &irq);
    tkl_gpio_irq_enable(pin);
}

// GPIO ISR (hard context): signal TDL (arg is this device's ctx) then re-arm for the
// next transition; the state read + app cb run in the worker.
static void __soc_charger_isr(void *arg)
{
    POWER_SOC_CTX_T                *c  = (POWER_SOC_CTX_T *)arg;
    const TDD_POWER_GPIO_CHARGER_T *ch = c->cfg.charger;

    tdl_power_charger_irq_notify(arg);

    __soc_charger_arm_pin(c, ch->chrg_pin);
    if (TDD_POWER_PIN_NONE != ch->stdby_pin) {
        __soc_charger_arm_pin(c, ch->stdby_pin);
    }
}

static OPERATE_RET __soc_charger_arm_event(TDD_POWER_DEV_HANDLE_T ctx)
{
    POWER_SOC_CTX_T                *c  = (POWER_SOC_CTX_T *)ctx;
    const TDD_POWER_GPIO_CHARGER_T *ch = c->cfg.charger;

    if (NULL == ch) {
        return OPRT_NOT_SUPPORTED;
    }

    __soc_charger_arm_pin(c, ch->chrg_pin);
    if (TDD_POWER_PIN_NONE != ch->stdby_pin) {
        __soc_charger_arm_pin(c, ch->stdby_pin);
    }
    return OPRT_OK;
}

/* ---- registration ---- */

static void __soc_domains_init(POWER_SOC_CTX_T *c)
{
    TUYA_GPIO_BASE_CFG_T gc;

    for (uint8_t i = 0; i < c->cfg.domain_cnt; i++) {
        const TDD_POWER_GPIO_DOMAIN_T *d = &c->cfg.domains[i];
        gc.mode   = TUYA_GPIO_PUSH_PULL;
        gc.direct = TUYA_GPIO_OUTPUT;
        gc.level  = d->default_on ? d->active_level : __level_inv(d->active_level);
        tkl_gpio_init(d->pin, &gc);
        tkl_gpio_write(d->pin, gc.level);
    }
}

static void __soc_battery_init(POWER_SOC_CTX_T *c)
{
    const TDD_POWER_ADC_BATTERY_T *b = c->cfg.battery;
    TUYA_ADC_BASE_CFG_T            ac = {0};

    ac.ch_list.data = 1u << b->adc_ch;
    ac.ch_nums      = 1;
    ac.width        = 12;
    ac.mode         = TUYA_ADC_CONTINUOUS;
    ac.type         = TUYA_ADC_INNER_SAMPLE_VOL;
    ac.conv_cnt     = 1;
    tkl_adc_init(b->adc_num, &ac);
}

// status line as input; idle pull = opposite of the active level
static void __soc_charger_pin_init(TUYA_GPIO_NUM_E pin, TUYA_GPIO_LEVEL_E active)
{
    TUYA_GPIO_BASE_CFG_T gc;
    gc.direct = TUYA_GPIO_INPUT;
    gc.mode   = (TUYA_GPIO_LEVEL_LOW == active) ? TUYA_GPIO_PULLUP : TUYA_GPIO_PULLDOWN;
    gc.level  = __level_inv(active);
    tkl_gpio_init(pin, &gc);
}

OPERATE_RET tdd_power_soc_register(const char *name, const TDD_POWER_SOC_CFG_T *cfg)
{
    POWER_SOC_CTX_T  *c = NULL;
    TDL_POWER_INTFS_T intfs;
    OPERATE_RET       rt = OPRT_OK;

    if (NULL == name || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    c = (POWER_SOC_CTX_T *)tal_malloc(sizeof(POWER_SOC_CTX_T));
    if (NULL == c) {
        return OPRT_MALLOC_FAILED;
    }
    memset(c, 0, sizeof(POWER_SOC_CTX_T));
    c->cfg = *cfg;

    memset(&intfs, 0, sizeof(intfs));

    if (c->cfg.domain_cnt > 0) {
        __soc_domains_init(c);
        intfs.domain_set = __soc_domain_set;
        intfs.domain_get = __soc_domain_get;
    }
    if (NULL != c->cfg.battery) {
        __soc_battery_init(c);
        intfs.battery_get_voltage = __soc_battery_get_voltage; // percent derived by TDL
    }
    if (NULL != c->cfg.charger) {
        __soc_charger_pin_init(c->cfg.charger->chrg_pin, c->cfg.charger->chrg_active);
        if (TDD_POWER_PIN_NONE != c->cfg.charger->stdby_pin) {
            __soc_charger_pin_init(c->cfg.charger->stdby_pin, c->cfg.charger->stdby_active);
        }
        intfs.charger_get_state  = __soc_charger_get_state;
        intfs.charger_arm_event  = __soc_charger_arm_event;
    }

    rt = tdl_power_register(name, &intfs, &c->cfg.info, c);
    if (OPRT_OK != rt) {
        tal_free(c);
    }
    return rt;
}
