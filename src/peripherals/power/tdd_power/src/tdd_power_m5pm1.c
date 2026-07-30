/**
 * @file tdd_power_m5pm1.c
 * @brief M5PM1 power backend. See tdd_power_m5pm1.h. Battery voltage comes from the
 *        chip's ADC (already in mV); charge state is derived from the power source
 *        plus an optional CHRG-status GPIO. No power domains.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "string.h"
#include "tal_memory.h"
#include "tal_log.h"
#include "tkl_gpio.h"
#include "tdl_power_driver.h"
#include "tdd_power_m5pm1.h"

// M5PM1 GPIO1 (PYG1) is the chip's IRQ output line.
#define M5PM1_IRQ_OUT_GPIO M5PM1_GPIO_NUM_1

typedef struct {
    TDD_POWER_M5PM1_CFG_T cfg;
} POWER_M5PM1_CTX_T;

static OPERATE_RET __m5pm1_battery_get_voltage(TDD_POWER_DEV_HANDLE_T ctx, uint32_t *mv)
{
    uint16_t    v  = 0;
    OPERATE_RET rt = m5pm1_get_battery_voltage(&v);
    (void)ctx;
    if (OPRT_OK != rt) {
        return rt;
    }
    *mv = v;
    return OPRT_OK;
}

// PWR_SRC (0x04) is a bitmap [2:0]: bit0=5VIN, bit1=5VINOUT, bit2=BAT. External 5V is
// present when either input bit is set.
#define M5PM1_PWR_SRC_EXT_MASK 0x03u

// Charge state from external-power presence: plugged in -> CHARGING, on battery ->
// DISCHARGE. This board's CHRG line (PYG0) does not reliably indicate charging vs full
// (it reads "not charging" even while charging a half-full pack), so it isn't used and
// FULL isn't reported.
static OPERATE_RET __m5pm1_charger_get_state(TDD_POWER_DEV_HANDLE_T ctx, TDL_CHG_STATE_E *st)
{
    POWER_M5PM1_CTX_T *c   = (POWER_M5PM1_CTX_T *)ctx;
    M5PM1_PWR_SRC_E    src = M5PM1_PWR_SRC_UNKNOWN;
    BOOL_T             ext = FALSE;

    if (OPRT_OK == m5pm1_get_power_source(&src)) {
        ext = ((uint8_t)src & M5PM1_PWR_SRC_EXT_MASK) ? TRUE : FALSE;
    }
    *st = ext ? TDL_CHG_CHARGING : TDL_CHG_DISCHARGE;

    // Ack the pending 5V insert/remove IRQ so the M5PM1 releases its IRQ line and it can
    // fire again. Runs in the TDL worker / poll path, never in the ISR.
    if (c->cfg.irq_valid) {
        m5pm1_irq_clear_all(M5PM1_IRQ_DOMAIN_SYS);
    }
    return OPRT_OK;
}

// SoC GPIO ISR (hard context): only signal TDL; the M5PM1 I2C read/clear runs in the worker.
static void __m5pm1_irq_isr(void *arg)
{
    tdl_power_charger_irq_notify(arg);
}

static OPERATE_RET __m5pm1_charger_arm_event(TDD_POWER_DEV_HANDLE_T ctx)
{
    POWER_M5PM1_CTX_T   *c = (POWER_M5PM1_CTX_T *)ctx;
    TUYA_GPIO_BASE_CFG_T gi;
    TUYA_GPIO_IRQ_T      irq = {0};

    if (!c->cfg.irq_valid) {
        return OPRT_NOT_SUPPORTED; // IRQ line not wired -> app polls
    }

    // Route M5PM1 GPIO1 to its IRQ-output function.
    m5pm1_gpio_set_func(M5PM1_IRQ_OUT_GPIO, M5PM1_GPIO_FUNC_IRQ);

    // Enable ONLY 5VIN insert/remove (SYS bits 0,1) — that's the plug/unplug we care
    // about. Mask the rest of SYS (5VINOUT is driven as OUTPUT, battery events invalid
    // while charging) and the ENTIRE GPIO domain (PYG4 = BMI270 INT storms it); otherwise
    // the IRQ floods.
    m5pm1_irq_set_mask(M5PM1_IRQ_DOMAIN_SYS, 0x3Cu);
    m5pm1_irq_set_mask(M5PM1_IRQ_DOMAIN_GPIO, 0xFFu);

    // Clear stale status so the line starts released (high).
    m5pm1_irq_clear_all(M5PM1_IRQ_DOMAIN_SYS);
    m5pm1_irq_clear_all(M5PM1_IRQ_DOMAIN_GPIO);

    // M5PM1 IRQ is active-low (held until status cleared) -> pull-up input, falling edge.
    memset(&gi, 0, sizeof(gi));
    gi.direct = TUYA_GPIO_INPUT;
    gi.mode   = TUYA_GPIO_PULLUP;
    gi.level  = TUYA_GPIO_LEVEL_HIGH;
    tkl_gpio_init(c->cfg.irq_pin, &gi);
    irq.mode = TUYA_GPIO_IRQ_FALL;
    irq.cb   = __m5pm1_irq_isr;
    irq.arg  = c;
    tkl_gpio_irq_init(c->cfg.irq_pin, &irq);
    tkl_gpio_irq_enable(c->cfg.irq_pin);
    return OPRT_OK;
}

OPERATE_RET tdd_power_m5pm1_register(const char *name, const TDD_POWER_M5PM1_CFG_T *cfg)
{
    POWER_M5PM1_CTX_T *c = NULL;
    TDL_POWER_INTFS_T  intfs;
    OPERATE_RET        rt = OPRT_OK;

    if (NULL == name || NULL == cfg) {
        return OPRT_INVALID_PARM;
    }

    c = (POWER_M5PM1_CTX_T *)tal_malloc(sizeof(POWER_M5PM1_CTX_T));
    if (NULL == c) {
        return OPRT_MALLOC_FAILED;
    }
    memset(c, 0, sizeof(POWER_M5PM1_CTX_T));
    c->cfg = *cfg;

    // No domain ops: the M5PM1 rails are board infrastructure, not app roles.
    // No battery_get_percent: TDL derives it from voltage + the OCV curve.
    memset(&intfs, 0, sizeof(intfs));
    intfs.battery_get_voltage = __m5pm1_battery_get_voltage;
    intfs.charger_get_state   = __m5pm1_charger_get_state;
    intfs.charger_arm_event   = __m5pm1_charger_arm_event; // NOT_SUPPORTED unless cfg.irq_valid

    rt = tdl_power_register(name, &intfs, &c->cfg.info, c);
    if (OPRT_OK != rt) {
        tal_free(c);
    }
    return rt;
}
