/**
 * @file board_charge_detect_api.c
 * @author Tuya Inc.
 * @brief Implementation of charge detection / battery measurement API for ZECTRIX_NOTE4_TY.
 *
 * Battery ADC and charge-detect interrupt are provided but NOT registered by default
 * (see __board_register_charge_detect() in the main board file). Enable once the ADC
 * channel for P25 and the charger status polarity have been verified on hardware.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tkl_gpio.h"
#include "tkl_adc.h"
#include "tkl_output.h"
#include "board_charge_detect_api.h"

/***********************************************************
***********************variable define**********************
***********************************************************/

static BOARD_CHARGE_DETECT_CB sg_charge_detect_cb          = NULL;
static void                  *sg_charge_detect_arg         = NULL;
static BOOL_T                 sg_charge_detect_initialized = FALSE;

static BOOL_T              sg_battery_adc_initialized = FALSE;
static TUYA_ADC_BASE_CFG_T sg_battery_adc_cfg;

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET board_charge_detect_init(void)
{
    OPERATE_RET          rt = OPRT_OK;
    TUYA_GPIO_BASE_CFG_T cfg = {0};

    if (sg_charge_detect_initialized) {
        return OPRT_OK;
    }

    // CHRG (open-drain, active low): pull-up so an idle/unplugged charger reads HIGH.
    cfg.direct = TUYA_GPIO_INPUT;
    cfg.mode   = TUYA_GPIO_PULLUP;
    cfg.level  = TUYA_GPIO_LEVEL_HIGH;
    rt         = tkl_gpio_init(BOARD_CHARGE_DETECT_PIN, &cfg);
    if (OPRT_OK != rt) {
        return rt;
    }

    // STDBY (active high): pull-down so "not full" reads LOW when no charger present.
    cfg.direct = TUYA_GPIO_INPUT;
    cfg.mode   = TUYA_GPIO_PULLDOWN;
    cfg.level  = TUYA_GPIO_LEVEL_LOW;
    rt         = tkl_gpio_init(BOARD_CHARGE_STDBY_PIN, &cfg);
    if (OPRT_OK != rt) {
        tkl_gpio_deinit(BOARD_CHARGE_DETECT_PIN);
        return rt;
    }

    sg_charge_detect_initialized = TRUE;
    return rt;
}

OPERATE_RET board_charge_detect_register_callback(BOARD_CHARGE_DETECT_CB cb, void *arg)
{
    if (NULL == cb) {
        return OPRT_INVALID_PARM;
    }

    sg_charge_detect_cb  = cb;
    sg_charge_detect_arg = arg;

    return OPRT_OK;
}

OPERATE_RET board_charge_detect_unregister_callback(void)
{
    sg_charge_detect_cb  = NULL;
    sg_charge_detect_arg = NULL;

    return OPRT_OK;
}

OPERATE_RET board_charge_detect_get_state(BOARD_CHARGE_STATE_E *state)
{
    OPERATE_RET       rt = OPRT_OK;
    TUYA_GPIO_LEVEL_E chrg_level;
    TUYA_GPIO_LEVEL_E stdby_level;
    BOOL_T            charging;
    BOOL_T            full;

    if (NULL == state) {
        return OPRT_INVALID_PARM;
    }

    if (!sg_charge_detect_initialized) {
        return OPRT_COM_ERROR;
    }

    rt = tkl_gpio_read(BOARD_CHARGE_DETECT_PIN, &chrg_level);
    if (OPRT_OK != rt) {
        return rt;
    }
    rt = tkl_gpio_read(BOARD_CHARGE_STDBY_PIN, &stdby_level);
    if (OPRT_OK != rt) {
        return rt;
    }

    // Polarity mirrors the ESP32 zectrix board: CHRG asserted (charging) is LOW,
    // STDBY asserted (charge done) is HIGH. STDBY takes priority: once the charger
    // reports "done", CHRG has already released even though power is still present.
    charging = (chrg_level == BOARD_CHARGE_DETECT_ACTIVE_LEVEL) ? TRUE : FALSE;
    full     = (stdby_level == BOARD_CHARGE_FULL_ACTIVE_LEVEL) ? TRUE : FALSE;

    if (full) {
        *state = BOARD_CHARGE_STATE_FULL;
    } else if (charging) {
        *state = BOARD_CHARGE_STATE_PLUGGED;
    } else {
        *state = BOARD_CHARGE_STATE_UNPLUGGED;
    }

    return rt;
}

OPERATE_RET board_charge_detect_enable(void)
{
    if (!sg_charge_detect_initialized) {
        return OPRT_COM_ERROR;
    }

    return tkl_gpio_irq_enable(BOARD_CHARGE_DETECT_PIN);
}

OPERATE_RET board_charge_detect_disable(void)
{
    if (!sg_charge_detect_initialized) {
        return OPRT_COM_ERROR;
    }

    return tkl_gpio_irq_disable(BOARD_CHARGE_DETECT_PIN);
}

OPERATE_RET board_charge_detect_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (!sg_charge_detect_initialized) {
        return OPRT_OK;
    }

    tkl_gpio_irq_disable(BOARD_CHARGE_DETECT_PIN);
    rt = tkl_gpio_deinit(BOARD_CHARGE_DETECT_PIN);

    sg_charge_detect_cb          = NULL;
    sg_charge_detect_arg         = NULL;
    sg_charge_detect_initialized = FALSE;

    return rt;
}

OPERATE_RET board_battery_adc_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (sg_battery_adc_initialized) {
        return OPRT_OK;
    }

    // Config mirrors the official ADC example (examples/peripherals/adc): single
    // channel, 12-bit, continuous mode, inner-sample type, conv_cnt 1. Channel is the
    // T5 ADC channel index (ADC_1 for P25), used both in ch_list and the read call.
    sg_battery_adc_cfg.ch_list.data = 1 << BOARD_BATTERY_ADC_CH;
    sg_battery_adc_cfg.ch_nums      = 1;
    sg_battery_adc_cfg.width        = 12;
    sg_battery_adc_cfg.mode         = TUYA_ADC_CONTINUOUS;
    sg_battery_adc_cfg.type         = TUYA_ADC_INNER_SAMPLE_VOL;
    sg_battery_adc_cfg.conv_cnt     = 1;

    rt = tkl_adc_init(BOARD_BATTERY_ADC_NUM, &sg_battery_adc_cfg);
    if (OPRT_OK != rt) {
        return rt;
    }

    sg_battery_adc_initialized = TRUE;
    return rt;
}

OPERATE_RET board_battery_read_voltage(uint32_t *voltage_mv)
{
    OPERATE_RET rt;
    int32_t     raw     = 0;
    int32_t     raw_max = 0;
    int         n       = 0;
    int         i;
    float       tap_mv;

    if (NULL == voltage_mv) {
        return OPRT_INVALID_PARM;
    }

    if (!sg_battery_adc_initialized) {
        return OPRT_COM_ERROR;
    }

    // Take the PEAK of several reads. The ~755k tap is high-Z, so each SAR conversion
    // only drains the node (readings sag across a burst); the maximum is the freshest,
    // least-disturbed sample and tracks the true tap voltage.
    for (i = 0; i < BOARD_BATTERY_ADC_SAMPLES; i++) {
        raw = 0;
        rt  = tkl_adc_read_single_channel(BOARD_BATTERY_ADC_NUM, BOARD_BATTERY_ADC_CH, &raw);
        if (OPRT_OK == rt) {
            if (raw > raw_max) {
                raw_max = raw;
            }
            n++;
        }
    }
    if (0 == n) {
        return OPRT_COM_ERROR;
    }

    // Linear raw-count -> tap voltage (vendor model V = (raw-LOW)/SPAN + 1.0 V),
    // then scale up across the divider to the battery voltage.
    tap_mv = (((float)(raw_max - BOARD_BATTERY_ADC_CAL_LOW) / (float)BOARD_BATTERY_ADC_CAL_SPAN) + 1.0f) *
             1000.0f;
    if (tap_mv < 0.0f) {
        tap_mv = 0.0f;
    }
    *voltage_mv = (uint32_t)(tap_mv * BOARD_BATTERY_DIVIDER_RATIO);

    return OPRT_OK;
}

/**
 * @brief Ternary-lithium (NMC/NCM / 三元锂) open-circuit-voltage -> SOC curve.
 *
 * Single-cell discharge characteristic for a typical NMC Li-ion cell at light
 * load / near rest. Compared to LiCoO2 the plateau sits a little lower and the
 * usable range extends down to the 3.0 V cutoff. The relation is strongly
 * non-linear (a long flat plateau around 3.6-3.9 V), so a linear 3.0-4.2 V map
 * is wildly inaccurate in the mid range. We interpolate this lookup table
 * instead. Entries MUST be sorted by descending voltage. Voltages are per-cell,
 * in volts.
 */
typedef struct {
    float voltage_v;
    float percent;
} battery_ocv_point_t;

static const battery_ocv_point_t sg_nmc_curve[] = {
    {4.20f, 100.0f}, {4.15f, 95.0f}, {4.10f, 90.0f}, {4.05f, 85.0f}, {4.00f, 80.0f},
    {3.95f, 75.0f},  {3.90f, 70.0f}, {3.86f, 65.0f}, {3.82f, 60.0f}, {3.79f, 55.0f},
    {3.76f, 50.0f},  {3.73f, 45.0f}, {3.71f, 40.0f}, {3.68f, 35.0f}, {3.66f, 30.0f},
    {3.63f, 25.0f},  {3.60f, 20.0f}, {3.55f, 15.0f}, {3.48f, 10.0f}, {3.35f, 5.0f},
    {3.00f, 0.0f},
};

#define BATTERY_CURVE_POINTS (sizeof(sg_nmc_curve) / sizeof(sg_nmc_curve[0]))

static uint8_t __battery_voltage_to_percent(float battery_voltage_v)
{
    uint16_t i;
    float    percent = 0.0f;

    // Clamp to the curve end-points.
    if (battery_voltage_v >= sg_nmc_curve[0].voltage_v) {
        return 100;
    }
    if (battery_voltage_v <= sg_nmc_curve[BATTERY_CURVE_POINTS - 1].voltage_v) {
        return 0;
    }

    // Find the bracketing pair and linearly interpolate SOC within it.
    for (i = 0; i < BATTERY_CURVE_POINTS - 1; i++) {
        float v_hi = sg_nmc_curve[i].voltage_v;
        float v_lo = sg_nmc_curve[i + 1].voltage_v;

        if (battery_voltage_v <= v_hi && battery_voltage_v > v_lo) {
            float p_hi = sg_nmc_curve[i].percent;
            float p_lo = sg_nmc_curve[i + 1].percent;
            percent    = p_lo + (p_hi - p_lo) * ((battery_voltage_v - v_lo) / (v_hi - v_lo));
            break;
        }
    }

    return (uint8_t)(percent + 0.5f);
}

OPERATE_RET board_battery_read_percentage(uint8_t *percentage)
{
    OPERATE_RET rt         = OPRT_OK;
    uint32_t    voltage_mv = 0;

    if (NULL == percentage) {
        return OPRT_INVALID_PARM;
    }

    rt = board_battery_read_voltage(&voltage_mv);
    if (OPRT_OK != rt) {
        return rt;
    }

    *percentage = __battery_voltage_to_percent((float)voltage_mv / 1000.0f);
    return rt;
}

OPERATE_RET board_battery_read(uint32_t *voltage_mv, uint8_t *percentage)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == voltage_mv || NULL == percentage) {
        return OPRT_INVALID_PARM;
    }

    rt = board_battery_read_voltage(voltage_mv);
    if (OPRT_OK != rt) {
        return rt;
    }

    *percentage = __battery_voltage_to_percent((float)(*voltage_mv) / 1000.0f);
    return rt;
}

OPERATE_RET board_battery_adc_deinit(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (!sg_battery_adc_initialized) {
        return OPRT_OK;
    }

    rt                         = tkl_adc_deinit(BOARD_BATTERY_ADC_NUM);
    sg_battery_adc_initialized = FALSE;

    return rt;
}
