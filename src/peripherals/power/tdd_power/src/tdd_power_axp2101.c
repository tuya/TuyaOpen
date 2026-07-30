/**
 * @file tdd_power_axp2101.c
 * @brief AXP2101 power backend: maps power capabilities onto the AXP2101 chip
 *        driver (LDO/DCDC channels as domains, on-chip fuel gauge, charge state).
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "string.h"
#include "tal_memory.h"
#include "tal_log.h"
#include "tkl_gpio.h"
#include "tdl_power_driver.h"
#include "tdd_power_axp2101.h"
#include "axp2101_driver.h"

typedef struct {
    TDD_POWER_AXP2101_CFG_T cfg; // copied; domains points to static board data
} POWER_AXP_CTX_T;

static const TDD_POWER_AXP_DOMAIN_T *__domain_of(POWER_AXP_CTX_T *c, TDL_POWER_DOMAIN_E role)
{
    for (uint8_t i = 0; i < c->cfg.domain_cnt; i++) {
        if (c->cfg.domains[i].role == role) {
            return &c->cfg.domains[i];
        }
    }
    return NULL;
}

static OPERATE_RET __axp_domain_set(TDD_POWER_DEV_HANDLE_T ctx, TDL_POWER_DOMAIN_E role, BOOL_T on)
{
    POWER_AXP_CTX_T              *c = (POWER_AXP_CTX_T *)ctx;
    const TDD_POWER_AXP_DOMAIN_T *d = __domain_of(c, role);
    bool                         ok;

    if (NULL == d) {
        return OPRT_NOT_SUPPORTED;
    }
    ok = on ? axp2101_enablePowerOutput(d->channel) : axp2101_disablePowerOutput(d->channel);
    return ok ? OPRT_OK : OPRT_COM_ERROR;
}

static OPERATE_RET __axp_domain_get(TDD_POWER_DEV_HANDLE_T ctx, TDL_POWER_DOMAIN_E role, BOOL_T *on)
{
    POWER_AXP_CTX_T              *c = (POWER_AXP_CTX_T *)ctx;
    const TDD_POWER_AXP_DOMAIN_T *d = __domain_of(c, role);

    if (NULL == d) {
        return OPRT_NOT_SUPPORTED;
    }
    *on = axp2101_isPowerChannelEnable(d->channel) ? TRUE : FALSE;
    return OPRT_OK;
}

static OPERATE_RET __axp_battery_get_voltage(TDD_POWER_DEV_HANDLE_T ctx, uint32_t *mv)
{
    (void)ctx;
    *mv = axp2101_getBattVoltage();
    return OPRT_OK;
}

static OPERATE_RET __axp_battery_get_percent(TDD_POWER_DEV_HANDLE_T ctx, uint8_t *pct)
{
    int p = axp2101_getBatteryPercent(); // chip fuel gauge

    (void)ctx;
    if (p < 0) {
        return OPRT_COM_ERROR;
    }
    *pct = (uint8_t)(p > 100 ? 100 : p);
    return OPRT_OK;
}

static OPERATE_RET __axp_charger_get_state(TDD_POWER_DEV_HANDLE_T ctx, TDL_CHG_STATE_E *st)
{
    (void)ctx;
    if (axp2101_isCharging()) {
        *st = TDL_CHG_CHARGING;
    } else if (axp2101_isVbusIn()) {
        *st = TDL_CHG_FULL; // plugged in but not charging -> charge complete
    } else {
        *st = TDL_CHG_DISCHARGE;
    }
    // Reading the state also acks any pending charge IRQ (so the AXP IRQ line
    // releases and can fire again). Runs in the TDL worker / app thread, not the ISR.
    axp2101_clearIrqStatus();
    return OPRT_OK;
}

// GPIO ISR (hard context): only signal TDL (arg is this device's ctx); the AXP I2C
// read runs in the worker.
static void __axp_irq_isr(void *arg)
{
    tdl_power_charger_irq_notify(arg);
}

static OPERATE_RET __axp_charger_arm_event(TDD_POWER_DEV_HANDLE_T ctx)
{
    POWER_AXP_CTX_T     *c = (POWER_AXP_CTX_T *)ctx;
    TUYA_GPIO_BASE_CFG_T gi;
    TUYA_GPIO_IRQ_T      irq = {0};

    if (TDD_POWER_PIN_NONE == c->cfg.irq_pin) {
        return OPRT_NOT_SUPPORTED; // AXP IRQ line not wired on this board
    }

    axp2101_enableIRQ(XPOWERS_AXP2101_BAT_CHG_START_IRQ | XPOWERS_AXP2101_BAT_CHG_DONE_IRQ |
                      XPOWERS_AXP2101_VBUS_INSERT_IRQ | XPOWERS_AXP2101_VBUS_REMOVE_IRQ);
    axp2101_clearIrqStatus();

    // AXP IRQ output is open-drain active-low -> pull-up input, falling edge.
    gi.direct = TUYA_GPIO_INPUT;
    gi.mode   = TUYA_GPIO_PULLUP;
    gi.level  = TUYA_GPIO_LEVEL_HIGH;
    tkl_gpio_init(c->cfg.irq_pin, &gi);
    irq.mode = TUYA_GPIO_IRQ_FALL;
    irq.cb   = __axp_irq_isr;
    irq.arg  = c;
    tkl_gpio_irq_init(c->cfg.irq_pin, &irq);
    tkl_gpio_irq_enable(c->cfg.irq_pin);
    return OPRT_OK;
}

OPERATE_RET tdd_power_axp2101_register(const char *name, const TDD_POWER_AXP2101_CFG_T *cfg)
{
    POWER_AXP_CTX_T  *c = NULL;
    TDL_POWER_INTFS_T intfs;
    OPERATE_RET       rt = OPRT_OK;

    if (NULL == name || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    // Bring the chip up (the driver uses its built-in I2C: TUYA_I2C_NUM_0 / 0x34).
    TUYA_CALL_ERR_RETURN(axp2101_init());

    // Measurements the battery/charge reads depend on.
    axp2101_disableTSPinMeasure();
    axp2101_enableBattDetection();
    axp2101_enableVbusVoltageMeasure();
    axp2101_enableBattVoltageMeasure();
    axp2101_enableSystemVoltageMeasure();
    axp2101_enableTemperatureMeasure();

    // Charge configuration.
    axp2101_setVbusVoltageLimit(cfg->charge.vbus_vol_limit);
    axp2101_setVbusCurrentLimit(cfg->charge.vbus_cur_limit);
    if (0 != cfg->charge.sys_power_down_mv) {
        axp2101_setSysPowerDownVoltage(cfg->charge.sys_power_down_mv);
    }
    axp2101_setPrechargeCurr(cfg->charge.precharge_curr);
    axp2101_setChargerTerminationCurr(cfg->charge.term_curr);
    axp2101_setChargerConstantCurr(cfg->charge.const_curr);
    axp2101_setChargeTargetVoltage(cfg->charge.target_vol);
    axp2101_enableCellbatteryCharge();
    axp2101_enableChargingLed();
    axp2101_setChargingLedMode(cfg->charge.led_mode);

    // No-role rails (core supplies + explicit-off). Never blanket-disable: core rails
    // are listed on=TRUE so they stay up throughout — no core-power glitch.
    for (uint8_t i = 0; i < cfg->rail_cnt; i++) {
        const TDD_POWER_AXP_RAIL_T *r = &cfg->rails[i];
        if (0 != r->mv) {
            axp2101_setPowerChannelVoltage(r->channel, r->mv);
        }
        if (r->on) {
            axp2101_enablePowerOutput(r->channel);
        } else {
            axp2101_disablePowerOutput(r->channel);
        }
    }
    // App-visible domains: boot voltage + state.
    for (uint8_t i = 0; i < cfg->domain_cnt; i++) {
        const TDD_POWER_AXP_DOMAIN_T *d = &cfg->domains[i];
        if (0 != d->default_mv) {
            axp2101_setPowerChannelVoltage(d->channel, d->default_mv);
        }
        if (d->default_on) {
            axp2101_enablePowerOutput(d->channel);
        } else {
            axp2101_disablePowerOutput(d->channel);
        }
    }

    // Power-key press timing.
    axp2101_setPowerKeyPressOnTime(cfg->pekey_on);
    axp2101_setPowerKeyPressOffTime(cfg->pekey_off);

    c = (POWER_AXP_CTX_T *)tal_malloc(sizeof(POWER_AXP_CTX_T));
    if (NULL == c) {
        return OPRT_MALLOC_FAILED;
    }
    memset(c, 0, sizeof(POWER_AXP_CTX_T));
    c->cfg = *cfg;

    memset(&intfs, 0, sizeof(intfs));
    intfs.domain_set          = __axp_domain_set;
    intfs.domain_get          = __axp_domain_get;
    intfs.battery_get_voltage = __axp_battery_get_voltage;
    intfs.battery_get_percent = __axp_battery_get_percent; // chip gauge overrides derivation
    intfs.charger_get_state   = __axp_charger_get_state;
    intfs.charger_arm_event   = __axp_charger_arm_event; // no-op (NOT_SUPPORTED) unless cfg.irq_pin wired

    rt = tdl_power_register(name, &intfs, &c->cfg.info, c);
    if (OPRT_OK != rt) {
        tal_free(c);
    }
    return rt;
}
