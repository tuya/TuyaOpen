/**
 * @file tdd_power_soc.h
 * @brief SoC power backend: power via on-chip peripherals (GPIO load-switch domains,
 *        GPIO charge-status lines, ADC battery divider). For boards without a PMIC.
 *        Structs are classified by mechanism: TDD_POWER_<mechanism>_<capability>_T.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_POWER_SOC_H__
#define __TDD_POWER_SOC_H__

#include "tuya_cloud_types.h"
#include "tdl_power_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Sentinel for an unused charger status pin (e.g. no STDBY line -> never FULL). */
#define TDD_POWER_PIN_NONE TUYA_GPIO_NUM_MAX

/* ---- GPIO mechanism ---- */

/** One GPIO load-switch domain: role -> pin. */
typedef struct {
    TDL_POWER_DOMAIN_E role;
    TUYA_GPIO_NUM_E    pin;
    TUYA_GPIO_LEVEL_E  active_level; // level that enables the rail
    BOOL_T             default_on;   // initial state at registration
} TDD_POWER_GPIO_DOMAIN_T;

/** GPIO charge-status detection (1 or 2 status lines). */
typedef struct {
    TUYA_GPIO_NUM_E   chrg_pin;     // charging line
    TUYA_GPIO_LEVEL_E chrg_active;
    TUYA_GPIO_NUM_E   stdby_pin;    // charge-done line; TDD_POWER_PIN_NONE if absent
    TUYA_GPIO_LEVEL_E stdby_active;
} TDD_POWER_GPIO_CHARGER_T;

/** A GPIO deep-sleep wakeup source (used by enter_deepsleep). */
typedef struct {
    uint32_t gpio_num;     // wakeup GPIO
    uint8_t  active_level; // level that wakes (0 or 1)
} TDD_POWER_GPIO_WAKESRC_T;

/* ---- ADC mechanism ---- */

/** ADC voltage-divider battery sampling (landmarks/curve live in info.battery). */
typedef struct {
    TUYA_ADC_NUM_E adc_num;
    uint8_t        adc_ch;
    float          divider_ratio; // VBAT = V_adc * divider_ratio
    uint16_t       cal_low;       // two-point cal: raw at ~1.0V (0 = T5 default)
    uint16_t       cal_span;      // two-point cal: raw counts/V (0 = T5 default)
    uint8_t        samples;       // reads per measurement; the peak is taken
} TDD_POWER_ADC_BATTERY_T;

/* ---- board aggregate (unused capabilities left NULL/0) ---- */

typedef struct {
    const TDD_POWER_GPIO_DOMAIN_T  *domains;
    uint8_t                         domain_cnt;
    const TDD_POWER_ADC_BATTERY_T  *battery; // NULL = no battery
    const TDD_POWER_GPIO_CHARGER_T *charger; // NULL = no charge detection
    const TDD_POWER_GPIO_WAKESRC_T *wakesrc; // deep-sleep GPIO wake sources (NULL = none)
    uint8_t                         wakesrc_cnt;
    TDL_POWER_INFO_T                info;    // device-level static facts
} TDD_POWER_SOC_CFG_T;

/**
 * @brief Register a SoC-backed power device.
 *
 * @param[in] name Device name (convention "power").
 * @param[in] cfg  Board configuration.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdd_power_soc_register(const char *name, const TDD_POWER_SOC_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_POWER_SOC_H__ */
