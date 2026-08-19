/**
 * @file xteink_x4_pro_battery.c
 * @brief Xteink X4 Pro CW2017 battery fuel gauge driver.
 * @version 0.1
 * @date 2026-08-18
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 *
 * @note Register map, reset and profile flow recovered from the X4 Pro OEM
 *       firmware (XTEink::Cw2017PowerHal) via the FreeInk SDK BatteryMonitor
 *       (freeink-sdk/libs/hardware/BatteryMonitor/src/BatteryMonitor.cpp).
 *       The CW2017 reports 0% until a matching 80-byte BATINFO battery
 *       profile is resident, so init verifies and uploads it before any SoC
 *       read means anything. The gauge shares the touch I2C bus (SDA39/SCL38,
 *       the adapter reuses the bus handle across drivers).
 *
 *       Reported percentage: FreeInk's standard 1S Li-ion rest-voltage notch
 *       curve (BatteryMonitor::percentageFromMillivolts, with its hysteresis
 *       overload) applied to the gauge's VCELL. This is immediately correct
 *       at boot (the SoC register needs profile-learning time, and the cell
 *       sits just under 4.20 V straight off the charger, which the midpoint
 *       rounding still reports as 100 %). The raw gauge SoC is logged at
 *       init as a cross-check.
 *
 *       Charge state: the X4 Pro has no charger IC and no identified VBUS
 *       sense pin (FreeInk bring-up), so a low-rate poller task estimates it
 *       from the VCELL slope — sustained climb = charging, CV plateau near
 *       4.20 V = full, relaxation = idle — and notifies subscribers.
 */
#include "xteink_x4_pro_battery.h"

#include "board_config.h"
#include "tal_log.h"
#include "tal_system.h"
#include "tal_thread.h"
#include "tkl_i2c.h"
#include "tkl_pinmux.h"
#include <string.h>

#define CW2017_REG_VERSION 0x00 /* running state when (ver & 0xFD) == 0x0D */
#define CW2017_REG_VCELL_H 0x02 /* 14-bit VCELL, big-endian over 0x02/0x03 */
#define CW2017_REG_SOC     0x04 /* integer percent */
#define CW2017_REG_MODE    0x08 /* soft-reset / sleep control */
#define CW2017_REG_CONFIG  0x0B /* bit7 = profile-loaded / update-enable */
#define CW2017_REG_BATINFO 0x10 /* 80-byte profile spans 0x10..0x5F */

/* The exact BATINFO profile the OEM uploads (app1 table @ DROM 0x3c5d8d00).
 * Battery-model specific: it is the profile for the X4 Pro's cell. */
static const uint8_t s_batinfo[80] = {
    0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0xbf, 0xb5, 0xb4, 0xa4, 0x9c, 0xeb, 0xe2,
    0xdf, 0xe5, 0xca, 0xa0, 0x8a, 0x62, 0x53, 0x48, 0x40, 0x3a, 0x32, 0xb1, 0xae, 0xda, 0xb5, 0xff,
    0xff, 0xff, 0xe8, 0xdb, 0xd9, 0xd6, 0xd4, 0xd2, 0xd0, 0xcb, 0xc3, 0xbc, 0x9e, 0x87, 0x7b, 0x71,
    0x72, 0x7c, 0x8c, 0xa3, 0xb7, 0xc8, 0xa5, 0x4f, 0x00, 0x00, 0xab, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23};

#define TAG "x4pro_batt"

static BOOL_T s_batt_inited = FALSE;

/* Standard 1S Li-ion / LiPo (4.20 V) rest-voltage discharge curve, one entry
 * per 10 % notch — ported verbatim from FreeInk BatteryMonitor. The curve is
 * deliberately not resampled any finer: between 20 % and 60 % the whole span
 * is ~130 mV, so a tenth of a volt-step is already below the noise of the
 * gauge. The 0 % anchor is 3.45 V rather than the cell's protection cut-off:
 * below that the pack falls off a cliff and the remaining runtime is minutes,
 * so reporting it as empty is honest; it also leaves headroom for the sag
 * under an e-ink refresh, the heaviest load this device draws. */
static const uint16_t s_liion_notch_mv[11] = {
    3450, /*   0% */
    3680, /*  10% */
    3740, /*  20% */
    3770, /*  30% */
    3790, /*  40% */
    3820, /*  50% */
    3870, /*  60% */
    3920, /*  70% */
    3980, /*  80% */
    4060, /*  90% */
    4200, /* 100% */
};

/* A notch change has to clear the boundary by this much before it is accepted.
 * The 20-40 % band is only 50 mV wide, so without a deadband a few millivolts
 * of sampler noise would swap the reading back and forth between page turns.
 * Kept well under the narrowest half-segment (10 mV) so no notch can become a
 * trap. */
#define BATT_NOTCH_HYSTERESIS_MV 8

/* >100 marks "no usable history yet" for the hysteresis path. */
static uint16_t s_pct_prev = 101U;

/* ------------------------------------------------- charge state estimator */

#define BATT_CHG_POLL_MS      2000U /* VCELL sample cadence */
#define BATT_CHG_CLIMB_MV     10U   /* sustained rise above anchor -> charging */
#define BATT_CHG_FULL_MV      4190U /* CV top-off plateau level */
#define BATT_CHG_EXIT_DROP_MV 6U    /* dip below the plateau -> off charger */
#define BATT_CHG_CONFIRM_N    3U    /* consecutive agreeing samples to latch */

static X4PRO_CHARGE_STATE_E  s_charge_state = X4PRO_CHARGE_IDLE;
static X4PRO_CHARGE_STATE_CB s_charge_cb    = NULL;
static int32_t               s_vema_mv      = 0; /* EMA-smoothed VCELL, 0 = none */
static int32_t               s_anchor_mv    = 0; /* reference level per state */
static uint8_t               s_confirm      = 0U;
static THREAD_HANDLE         s_chg_thread   = NULL;

/**
 * @brief Set up the shared I2C bus (idempotent — the adapter reuses an
 *        already-created bus handle on the port).
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __bus_init(void)
{
    OPERATE_RET         rt = OPRT_OK;
    TUYA_IIC_BASE_CFG_T cfg;

    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config(X4PRO_I2C_PIN_SDA, TUYA_IIC0_SDA));
    TUYA_CALL_ERR_RETURN(tkl_io_pinmux_config(X4PRO_I2C_PIN_SCL, TUYA_IIC0_SCL));

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.role       = TUYA_IIC_MODE_MASTER;
    cfg.speed      = TUYA_IIC_BUS_SPEED_400K;
    cfg.addr_width = TUYA_IIC_ADDRESS_7BIT;

    return tkl_i2c_init(X4PRO_I2C_PORT, &cfg);
}

/**
 * @brief Read one gauge register.
 * @param[in] reg register address.
 * @param[out] val non-NULL receives the byte.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __read_reg8(uint8_t reg, uint8_t *val)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tkl_i2c_master_send(X4PRO_I2C_PORT, X4PRO_BATT_GAUGE_I2C_ADDR, &reg, 1, FALSE));
    return tkl_i2c_master_receive(X4PRO_I2C_PORT, X4PRO_BATT_GAUGE_I2C_ADDR, val, 1, FALSE);
}

/**
 * @brief Write one gauge register.
 * @param[in] reg register address.
 * @param[in] val value byte.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __write_reg8(uint8_t reg, uint8_t val)
{
    uint8_t pkt[2] = {reg, val};

    return tkl_i2c_master_send(X4PRO_I2C_PORT, X4PRO_BATT_GAUGE_I2C_ADDR, pkt, sizeof(pkt), FALSE);
}

/**
 * @brief Soft-reset: MODE 0xF0 -> 0x30 -> 0x00, 20 ms apart (OEM sequence).
 */
static void __gauge_reset(void)
{
    (void)__write_reg8(CW2017_REG_MODE, 0xF0);
    tal_system_sleep(20);
    (void)__write_reg8(CW2017_REG_MODE, 0x30);
    tal_system_sleep(20);
    (void)__write_reg8(CW2017_REG_MODE, 0x00);
    tal_system_sleep(20);
}

/**
 * @brief Make sure a valid BATINFO profile is resident.
 *
 * Wakes/resets the gauge when it is not running, then uploads + enables the
 * profile only if the resident bytes do not already match (the OEM leaves it
 * resident across warm boots, so this is usually a verify-only no-op).
 *
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __ensure_profile(void)
{
    OPERATE_RET rt    = OPRT_OK;
    uint8_t     ver   = 0;
    uint8_t     cfg   = 0;
    BOOL_T      match = TRUE;
    uint8_t     i;
    uint8_t     tries;

    TUYA_CALL_ERR_RETURN(__read_reg8(CW2017_REG_VERSION, &ver));
    if ((ver & 0xFDU) != 0x0DU) {
        __gauge_reset();
    }

    if (OPRT_OK == __read_reg8(CW2017_REG_CONFIG, &cfg) && (cfg & 0x80U)) {
        for (i = 0; i < (uint8_t)sizeof(s_batinfo); i++) {
            uint8_t b = 0;
            if (OPRT_OK != __read_reg8((uint8_t)(CW2017_REG_BATINFO + i), &b) || b != s_batinfo[i]) {
                match = FALSE;
                break;
            }
        }
        if (match) {
            return OPRT_OK; /* correct profile already loaded */
        }
    }

    for (i = 0; i < (uint8_t)sizeof(s_batinfo); i++) {
        TUYA_CALL_ERR_RETURN(__write_reg8((uint8_t)(CW2017_REG_BATINFO + i), s_batinfo[i]));
    }
    TUYA_CALL_ERR_RETURN(__write_reg8(CW2017_REG_CONFIG, 0x80)); /* update-enable */
    tal_system_sleep(20);
    __gauge_reset();

    /* Bounded polling (~1 s cap) for the SoC to become valid. */
    for (tries = 0; tries < 50U; tries++) {
        uint8_t soc = 0;
        if (OPRT_OK == __read_reg8(CW2017_REG_SOC, &soc) && soc <= 100U) {
            break;
        }
        tal_system_sleep(20);
    }

    PR_DEBUG("[" TAG "] CW2017 profile uploaded");
    return OPRT_OK;
}

/**
 * @brief Map a cell voltage onto the notch curve, rounding at each segment
 *        midpoint (FreeInk percentageFromMillivolts).
 *
 * Rounding at the midpoint (instead of flooring) lets a cell resting just
 * below 4.20 V straight off the charger still read 100 %.
 *
 * @param[in] mv cell voltage in millivolts.
 * @return Percentage snapped to a 10 % notch, 0..100.
 */
static uint8_t __pct_from_mv(uint16_t mv)
{
    uint8_t i;

    if (mv >= s_liion_notch_mv[10]) {
        return 100U;
    }
    for (i = 10U; i > 0U; i--) {
        uint16_t boundary = (uint16_t)((s_liion_notch_mv[i - 1U] + s_liion_notch_mv[i]) / 2U);
        if (mv >= boundary) {
            return (uint8_t)(i * 10U);
        }
    }
    return 0U;
}

/**
 * @brief Notch lookup with 8 mV deadband against the previous reading
 *        (FreeInk percentageFromMillivolts(mv, previousPercent)).
 *
 * Re-runs the lookup with the sample biased back toward the notch being left;
 * the move is only accepted if it still crosses. History is kept in-module so
 * every consumer (UI, diagnostics) sees one stable value.
 *
 * @param[in] mv cell voltage in millivolts.
 * @return Stable percentage snapped to a 10 % notch, 0..100.
 */
static uint8_t __pct_from_mv_hyst(uint16_t mv)
{
    uint8_t  notch     = __pct_from_mv(mv);
    uint16_t prev_notch;
    int32_t  biased;
    uint8_t  result;

    if (s_pct_prev > 100U) {
        s_pct_prev = notch; /* no usable history */
        return notch;
    }

    prev_notch = (uint16_t)((s_pct_prev / 10U) * 10U);
    if (notch == prev_notch) {
        return notch;
    }

    biased = (int32_t)mv + ((notch > prev_notch) ? -(int32_t)BATT_NOTCH_HYSTERESIS_MV : (int32_t)BATT_NOTCH_HYSTERESIS_MV);
    if (biased < 0) {
        biased = 0;
    } else if (biased > 65535) {
        biased = 65535;
    }

    result     = (__pct_from_mv((uint16_t)biased) == notch) ? notch : (uint8_t)prev_notch;
    s_pct_prev = result;
    return result;
}

/**
 * @brief Read the 14-bit VCELL register pair.
 * @param[out] mv non-NULL receives the cell voltage in millivolts.
 * @return OPRT_OK on success, error code on failure.
 */
static OPERATE_RET __read_vcell(uint32_t *mv)
{
    OPERATE_RET rt = OPRT_OK;
    uint8_t     hi = 0;
    uint8_t     lo = 0;
    uint16_t    raw14;

    TUYA_CALL_ERR_RETURN(__read_reg8(CW2017_REG_VCELL_H, &hi));
    TUYA_CALL_ERR_RETURN(__read_reg8((uint8_t)(CW2017_REG_VCELL_H + 1U), &lo));
    raw14 = (uint16_t)(((uint16_t)(hi & 0x3FU) << 8) | lo);
    *mv   = ((uint32_t)raw14 * 5U + 8U) >> 4; /* OEM formula, ~0.3125 mV/LSB */
    return OPRT_OK;
}

/**
 * @brief Human-readable charge-state name for logging.
 */
static const char *__charge_name(X4PRO_CHARGE_STATE_E st)
{
    switch (st) {
    case X4PRO_CHARGE_CHARGING:
        return "charging";
    case X4PRO_CHARGE_FULL:
        return "full";
    default:
        return "idle";
    }
}

/**
 * @brief Feed one VCELL sample into the charge-state machine.
 *
 * The EMA (alpha 1/8) plus the N-consecutive-sample latch absorb the ~100 mV
 * sag an e-ink refresh injects, so only a real charger transition sticks.
 *
 * @param[in] mv fresh VCELL sample in millivolts.
 */
static void __charge_sample(uint32_t mv)
{
    X4PRO_CHARGE_STATE_E next = s_charge_state;

    if (0 == s_vema_mv) {
        s_vema_mv   = (int32_t)mv; /* seed the filter and the anchor */
        s_anchor_mv = (int32_t)mv;
        return;
    }
    s_vema_mv += ((int32_t)mv - s_vema_mv) / 8;

    switch (s_charge_state) {
    case X4PRO_CHARGE_IDLE:
        if (s_vema_mv >= s_anchor_mv + (int32_t)BATT_CHG_CLIMB_MV) {
            next = X4PRO_CHARGE_CHARGING;
        } else if (s_vema_mv < s_anchor_mv) {
            s_anchor_mv = s_vema_mv; /* follow the relaxation downward */
        }
        break;
    case X4PRO_CHARGE_CHARGING:
        if (s_vema_mv >= (int32_t)BATT_CHG_FULL_MV) {
            next = X4PRO_CHARGE_FULL;
        } else if (s_vema_mv <= s_anchor_mv) {
            next = X4PRO_CHARGE_IDLE; /* charger pulled: cell relaxes back */
        }
        break;
    case X4PRO_CHARGE_FULL:
        if (s_vema_mv <= (int32_t)(BATT_CHG_FULL_MV - BATT_CHG_EXIT_DROP_MV)) {
            next = X4PRO_CHARGE_IDLE; /* off the charger, CV rail released */
        }
        break;
    default:
        break;
    }

    if (next == s_charge_state) {
        s_confirm = 0U;
        return;
    }
    if (++s_confirm < BATT_CHG_CONFIRM_N) {
        return; /* not yet sustained */
    }

    s_confirm   = 0U;
    s_anchor_mv = s_vema_mv;
    PR_NOTICE("[" TAG "] charge state: %s -> %s (VCELL ema %ld mV)", __charge_name(s_charge_state),
              __charge_name(next), (long)s_vema_mv);
    {
        X4PRO_CHARGE_STATE_E from = s_charge_state;

        s_charge_state = next;
        if (NULL != s_charge_cb) {
            s_charge_cb(from, next);
        }
    }
}

/**
 * @brief Estimator poller: samples VCELL every BATT_CHG_POLL_MS forever.
 */
static void __charge_task(void *args)
{
    (void)args;
    for (;;) {
        uint32_t mv = 0;

        if (OPRT_OK == __read_vcell(&mv)) {
            __charge_sample(mv);
        }
        tal_system_sleep(BATT_CHG_POLL_MS);
    }
}

OPERATE_RET xteink_x4_pro_battery_get_charge_state(X4PRO_CHARGE_STATE_E *state)
{
    if (NULL == state) {
        return OPRT_INVALID_PARM;
    }
    *state = s_charge_state;
    return OPRT_OK;
}

OPERATE_RET xteink_x4_pro_battery_on_charge_state(X4PRO_CHARGE_STATE_CB cb)
{
    s_charge_cb = cb;
    return OPRT_OK;
}

OPERATE_RET xteink_x4_pro_battery_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_batt_inited) {
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(__bus_init());

    /* Gauge present? */
    if (OPRT_OK != tkl_i2c_master_send(X4PRO_I2C_PORT, X4PRO_BATT_GAUGE_I2C_ADDR, NULL, 0, FALSE)) {
        PR_ERR("[" TAG "] CW2017 not answering at 0x%02X", X4PRO_BATT_GAUGE_I2C_ADDR);
        return OPRT_COM_ERROR;
    }

    TUYA_CALL_ERR_RETURN(__ensure_profile());

    s_batt_inited = TRUE;

    /* Cross-check: log the raw gauge SoC once next to the curve-derived value
     * so a miscalibrated BATINFO profile stays visible in the boot report. */
    {
        uint32_t mv  = 0;
        uint8_t  soc = 0;

        if ((OPRT_OK == __read_vcell(&mv)) && (OPRT_OK == __read_reg8(CW2017_REG_SOC, &soc))) {
            PR_NOTICE("[" TAG "] CW2017 gauge ready at 0x%02X (raw SoC %u%%, VCELL %lu mV -> curve %u%%)",
                      X4PRO_BATT_GAUGE_I2C_ADDR, (unsigned)soc, (unsigned long)mv,
                      (unsigned)__pct_from_mv_hyst((uint16_t)mv));
        } else {
            PR_NOTICE("[" TAG "] CW2017 gauge ready at 0x%02X", X4PRO_BATT_GAUGE_I2C_ADDR);
        }
    }

    /* Start the charge-state estimator (non-fatal: percentage still works). */
    if (NULL == s_chg_thread) {
        static char  chg_name[] = "x4pro_chg";
        THREAD_CFG_T cfg        = {0};

        cfg.stackDepth = 1024 * 3;
        cfg.priority   = THREAD_PRIO_1;
        cfg.thrdname   = chg_name;
        if (OPRT_OK != tal_thread_create_and_start(&s_chg_thread, NULL, NULL, __charge_task, NULL, &cfg)) {
            s_chg_thread = NULL;
            PR_WARN("[" TAG "] charge estimator task not started");
        }
    }
    return OPRT_OK;
}

OPERATE_RET xteink_x4_pro_battery_read(uint32_t *voltage_mv, uint8_t *percentage)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t    mv = 0;

    if (NULL == voltage_mv && NULL == percentage) {
        return OPRT_INVALID_PARM;
    }
    if (!s_batt_inited) {
        return OPRT_COM_ERROR;
    }

    TUYA_CALL_ERR_RETURN(__read_vcell(&mv));

    if (NULL != percentage) {
        *percentage = __pct_from_mv_hyst((uint16_t)mv);
    }
    if (NULL != voltage_mv) {
        *voltage_mv = mv;
    }

    return OPRT_OK;
}
