/**
 * @file tdd_power_axp2101.h
 * @brief AXP2101 power backend: one external PMIC (I2C) provides all three
 *        capabilities (domains via LDO/DCDC channels, battery via the on-chip
 *        fuel gauge, charger state). For boards with an AXP2101, in contrast to
 *        tdd_power_soc. Domain struct is TDD_POWER_AXP_<capability>_T.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_POWER_AXP2101_H__
#define __TDD_POWER_AXP2101_H__

#include "tuya_cloud_types.h"
#include "tdl_power_driver.h"
#include "axp2101_driver.h" // XPowersPowerChannel_t

#ifdef __cplusplus
extern "C" {
#endif

/** Sentinel for "AXP IRQ line not wired" (no charger events). */
#ifndef TDD_POWER_PIN_NONE
#define TDD_POWER_PIN_NONE TUYA_GPIO_NUM_MAX
#endif

/** One app-visible AXP domain: role -> channel (+ boot voltage/state). */
typedef struct {
    TDL_POWER_DOMAIN_E    role;
    XPowersPowerChannel_t channel;    // e.g. XPOWERS_ALDO3
    uint16_t              default_mv;  // boot voltage (0 = leave)
    BOOL_T                default_on;  // boot enable state
} TDD_POWER_AXP_DOMAIN_T;

/** A board rail on an AXP channel with no app role (core supply, or explicitly off).
 *  Set once at boot, not exposed to the app. Core rails must be listed on=TRUE and are
 *  never blanket-disabled, so core power never glitches during bring-up. */
typedef struct {
    XPowersPowerChannel_t channel;
    uint16_t              mv; // boot voltage (0 = leave)
    BOOL_T                on; // boot enable state
} TDD_POWER_AXP_RAIL_T;

/** AXP charge configuration (board/battery specific values). */
typedef struct {
    xpower_apx2101_vbus_vol_limit_t  vbus_vol_limit;
    xpowers_axp2101_vbus_cur_limit_t vbus_cur_limit;
    uint16_t                         sys_power_down_mv; // 0 = leave
    xpowers_prechg_t                 precharge_curr;
    xpowers_axp2101_chg_iterm_t      term_curr;
    xpowers_axp2101_chg_curr_t       const_curr;
    xpowers_axp2101_chg_vol_t        target_vol;
    xpowers_chg_led_mode_t           led_mode;
} TDD_POWER_AXP_CHARGE_T;

/** AXP2101-backend board configuration. The backend brings the chip fully up from this
 *  (init + measurements + charge + rails/domains + power-key); battery/charger come from
 *  the chip itself. Only truly board-specific non-AXP wiring (e.g. a 4G reset GPIO)
 *  stays in the board. */
typedef struct {
    const TDD_POWER_AXP_DOMAIN_T *domains; uint8_t domain_cnt; // app rails (role -> channel)
    const TDD_POWER_AXP_RAIL_T   *rails;   uint8_t rail_cnt;   // no-role rails (core / off)
    TDD_POWER_AXP_CHARGE_T        charge;
    xpowers_press_on_time_t       pekey_on;
    xpowers_press_off_time_t      pekey_off;
    TUYA_GPIO_NUM_E               irq_pin; // AXP IRQ line; TDD_POWER_PIN_NONE if unwired
    TDL_POWER_INFO_T              info;     // battery landmarks
} TDD_POWER_AXP2101_CFG_T;

/**
 * @brief Register an AXP2101-backed power device (thin ops adapter).
 *
 * The chip itself must already be brought up (init, channel voltages, charge
 * config) by the board's own AXP setup — this only exposes the tdl interface
 * over it (role->channel domain control, chip fuel gauge, charge state).
 *
 * @param[in] name Device name (convention "power").
 * @param[in] cfg  Board configuration (role->channel map + static info).
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdd_power_axp2101_register(const char *name, const TDD_POWER_AXP2101_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_POWER_AXP2101_H__ */
