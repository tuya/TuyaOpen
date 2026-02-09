/**
 * @file board_charge_detect_api.c
 * @author Tuya Inc.
 * @brief Implementation of charge detection API for TUYA_T5AI_EINK_NFC board.
 *
 * This module provides APIs to detect charging state changes via GPIO interrupt.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tkl_gpio.h"
#include "tkl_adc.h"
#include "tkl_output.h"
#include "board_charge_detect_api.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

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

// /**
//  * @brief Internal interrupt callback for charge detect GPIO
//  */
// static void __charge_detect_irq_cb(void *args)
// {
//     OPERATE_RET          rt = OPRT_OK;
//     TUYA_GPIO_LEVEL_E    level;
//     BOARD_CHARGE_STATE_E state;

//     if (NULL == sg_charge_detect_cb) {
//         return;
//     }

//     // Read the current GPIO level to determine charge state
//     rt = tkl_gpio_read(BOARD_CHARGE_DETECT_PIN, &level);
//     if (OPRT_OK != rt) {
//         return;
//     }

//     // Determine charge state based on GPIO level
//     // Assuming: HIGH = plugged, LOW = unplugged (adjust based on hardware)
//     state = (level == TUYA_GPIO_LEVEL_HIGH) ? BOARD_CHARGE_STATE_PLUGGED : BOARD_CHARGE_STATE_UNPLUGGED;

//     // Call user callback
//     if (sg_charge_detect_cb != NULL) {
//         sg_charge_detect_cb(state, sg_charge_detect_arg);
//     }
// }

// OPERATE_RET board_charge_detect_init(void)
// {
//     OPERATE_RET          rt = OPRT_OK;
//     TUYA_GPIO_BASE_CFG_T gpio_cfg;
//     TUYA_GPIO_IRQ_T      irq_cfg;

//     if (sg_charge_detect_initialized) {
//         return OPRT_OK;
//     }

//     // Initialize GPIO as input with pull-up (adjust based on hardware)
//     gpio_cfg.direct = TUYA_GPIO_INPUT;
//     gpio_cfg.mode   = TUYA_GPIO_PULLUP; // Pull-up to detect falling edge when charger plugs in
//     gpio_cfg.level  = TUYA_GPIO_LEVEL_HIGH;

//     rt = tkl_gpio_init(BOARD_CHARGE_DETECT_PIN, &gpio_cfg);
//     if (OPRT_OK != rt) {
//         PR_ERR("Failed to initialize charge detect GPIO: %d", rt);
//         return rt;
//     }

//     // Try to configure interrupt for falling edge (when charger plugs in, assuming pull-up)
//     // Use FALL edge only as RISE_FALL may not be supported on this platform
//     // If interrupt setup fails, we can still use polling via board_charge_detect_get_state()
//     irq_cfg.mode = TUYA_GPIO_IRQ_FALL; // Detect falling edge (charger plug in)
//     irq_cfg.cb   = __charge_detect_irq_cb;
//     irq_cfg.arg  = NULL;

//     rt = tkl_gpio_irq_init(BOARD_CHARGE_DETECT_PIN, &irq_cfg);
//     if (OPRT_OK != rt) {
//         PR_WARN("Charge detect GPIO interrupt not supported (error %d), will use polling mode", rt);
//         // Continue without interrupt - polling mode is still available via board_charge_detect_get_state()
//         sg_charge_detect_initialized = TRUE;
//         return OPRT_OK; // Return OK so system can continue
//     }

//     // Enable interrupt
//     rt = tkl_gpio_irq_enable(BOARD_CHARGE_DETECT_PIN);
//     if (OPRT_OK != rt) {
//         PR_WARN("Failed to enable charge detect interrupt: %d, will use polling mode", rt);
//         tkl_gpio_irq_disable(BOARD_CHARGE_DETECT_PIN);
//         // Continue without interrupt - polling mode is still available
//         sg_charge_detect_initialized = TRUE;
//         return OPRT_OK; // Return OK so system can continue
//     }

//     sg_charge_detect_initialized = TRUE;
//     return rt;
// }

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
    TUYA_GPIO_LEVEL_E level;

    if (NULL == state) {
        return OPRT_INVALID_PARM;
    }

    if (!sg_charge_detect_initialized) {
        return OPRT_COM_ERROR;
    }

    rt = tkl_gpio_read(BOARD_CHARGE_DETECT_PIN, &level);
    if (OPRT_OK != rt) {
        return rt;
    }

    // Determine charge state based on GPIO level
    // Assuming: HIGH = plugged, LOW = unplugged (adjust based on hardware)
    *state = (level == TUYA_GPIO_LEVEL_HIGH) ? BOARD_CHARGE_STATE_PLUGGED : BOARD_CHARGE_STATE_UNPLUGGED;

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

    // Disable interrupt
    tkl_gpio_irq_disable(BOARD_CHARGE_DETECT_PIN);

    // Deinitialize GPIO
    rt = tkl_gpio_deinit(BOARD_CHARGE_DETECT_PIN);

    // Clear callback
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

    // Configure ADC for battery voltage reading
    // Voltage divider: 2M / 510K / 100R
    // ADC reads at the junction of 510K and 100R
    sg_battery_adc_cfg.ch_list.data = 1 << BOARD_BATTERY_ADC_CH;
    sg_battery_adc_cfg.ch_nums      = 1;
    sg_battery_adc_cfg.width        = 12; // 12-bit ADC
    sg_battery_adc_cfg.mode         = TUYA_ADC_SINGLE;
    sg_battery_adc_cfg.type         = TUYA_ADC_INNER_SAMPLE_VOL;
    sg_battery_adc_cfg.conv_cnt     = 1;
    sg_battery_adc_cfg.ref_vol      = 3300; // 3.3V reference voltage (in mV)

    rt = tkl_adc_init(BOARD_BATTERY_ADC_NUM, &sg_battery_adc_cfg);
    if (OPRT_OK != rt) {
        return rt;
    }

    sg_battery_adc_initialized = TRUE;
    return rt;
}

OPERATE_RET board_battery_read_voltage(uint32_t *voltage_mv)
{
    OPERATE_RET rt                = OPRT_OK;
    int32_t     adc_voltage_mv    = 0;
    float       battery_voltage_v = 0.0f;

    if (NULL == voltage_mv) {
        return OPRT_INVALID_PARM;
    }

    if (!sg_battery_adc_initialized) {
        return OPRT_COM_ERROR;
    }

    // Read ADC voltage in millivolts
    rt = tkl_adc_read_single_channel(BOARD_BATTERY_ADC_NUM, BOARD_BATTERY_ADC_CH, &adc_voltage_mv);
    if (OPRT_OK != rt) {
        return rt;
    }

    // Convert ADC voltage to battery voltage using voltage divider ratio
    // Circuit: VBAT -> 2MΩ -> (junction) -> 510KΩ -> GND, Junction -> 100R -> ADC
    // V_ADC = VBAT * (510K) / (2M + 510K) = VBAT * 0.2032
    // V_battery = V_adc * (2M + 510K) / 510K = V_adc * 4.922
    // Note: 100R is a small series resistor and doesn't affect the divider ratio
    battery_voltage_v = ((float)adc_voltage_mv / 1000.0f) * BOARD_BATTERY_DIVIDER_RATIO;
    *voltage_mv       = (uint32_t)(battery_voltage_v * 1000.0f);

    return rt;
}

OPERATE_RET board_battery_read_percentage(uint8_t *percentage)
{
    OPERATE_RET rt                = OPRT_OK;
    uint32_t    voltage_mv        = 0;
    float       battery_voltage_v = 0.0f;
    float       percent           = 0.0f;

    if (NULL == percentage) {
        return OPRT_INVALID_PARM;
    }

    // Read battery voltage first
    rt = board_battery_read_voltage(&voltage_mv);
    if (OPRT_OK != rt) {
        return rt;
    }

    battery_voltage_v = (float)voltage_mv / 1000.0f;

    // Calculate percentage: LiPo 3.0V (0%) to 4.2V (100%)
    if (battery_voltage_v <= BOARD_BATTERY_VOLTAGE_MIN) {
        percent = 0.0f;
    } else if (battery_voltage_v >= BOARD_BATTERY_VOLTAGE_MAX) {
        percent = 100.0f;
    } else {
        percent = ((battery_voltage_v - BOARD_BATTERY_VOLTAGE_MIN) / BOARD_BATTERY_VOLTAGE_RANGE) * 100.0f;
    }

    *percentage = (uint8_t)percent;
    return rt;
}

OPERATE_RET board_battery_read(uint32_t *voltage_mv, uint8_t *percentage)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == voltage_mv || NULL == percentage) {
        return OPRT_INVALID_PARM;
    }

    // Read voltage first
    rt = board_battery_read_voltage(voltage_mv);
    if (OPRT_OK != rt) {
        return rt;
    }

    // Calculate percentage from voltage
    float battery_voltage_v = (float)(*voltage_mv) / 1000.0f;
    float percent           = 0.0f;

    if (battery_voltage_v <= BOARD_BATTERY_VOLTAGE_MIN) {
        percent = 0.0f;
    } else if (battery_voltage_v >= BOARD_BATTERY_VOLTAGE_MAX) {
        percent = 100.0f;
    } else {
        percent = ((battery_voltage_v - BOARD_BATTERY_VOLTAGE_MIN) / BOARD_BATTERY_VOLTAGE_RANGE) * 100.0f;
    }

    *percentage = (uint8_t)percent;
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
