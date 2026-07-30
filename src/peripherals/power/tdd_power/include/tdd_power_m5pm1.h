/**
 * @file tdd_power_m5pm1.h
 * @brief M5PM1 power backend: the M5PM1 PMIC (I2C) provides battery voltage (its
 *        own ADC, reported in mV) and charger state (power source + optional CHRG
 *        status GPIO). Power domains are not exposed — the M5PM1 rails are board
 *        infrastructure (SoC/IMU supplies, Grove 5V), not app-switchable roles.
 *        Thin adapter over the m5pm1 chip driver; the chip must be brought up by
 *        the board first (m5pm1_init).
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_POWER_M5PM1_H__
#define __TDD_POWER_M5PM1_H__

#include "tuya_cloud_types.h"
#include "tdl_power_driver.h"
#include "m5pm1_driver.h" // M5PM1_GPIO_NUM_E

#ifdef __cplusplus
extern "C" {
#endif

/** M5PM1-backend board configuration. */
typedef struct {
    // Charge events: the M5PM1 pulls its IRQ output (PYG1) low on 5V insert/remove. Wire
    // irq_pin to the SoC GPIO that receives it for event-driven (plug/unplug) charge
    // state; leave irq_valid FALSE to fall back to polling.
    BOOL_T          irq_valid;
    TUYA_GPIO_NUM_E irq_pin;
    TDL_POWER_INFO_T info; // battery landmarks + OCV curve
} TDD_POWER_M5PM1_CFG_T;

/**
 * @brief Register an M5PM1-backed power device (thin ops adapter).
 *
 * The chip must already be initialized (m5pm1_init) by the board. This exposes the
 * tdl interface over it: battery voltage from the chip, charge state from the power
 * source (+ optional CHRG GPIO). Percentage is derived by the TDL layer from the
 * voltage and the board-declared OCV curve. No power domains.
 *
 * @param[in] name Device name (convention "power").
 * @param[in] cfg  Board configuration (charger wiring + static battery info).
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdd_power_m5pm1_register(const char *name, const TDD_POWER_M5PM1_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_POWER_M5PM1_H__ */
