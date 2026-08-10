/**
 * @file tdl_power_driver.h
 * @brief Power driver-registration interface (for board/TDD developers).
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDL_POWER_DRIVER_H__
#define __TDL_POWER_DRIVER_H__

#include "tuya_cloud_types.h"
#include "tdl_power_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Backend-private device handle (opaque driver-layer token; distinct from the
 *  application-facing TDL_POWER_HANDLE). Passed to every op and back via notify. */
typedef void *TDD_POWER_DEV_HANDLE_T;

/**
 * @brief Backend capability ops. NULL op = capability absent (-> OPRT_NOT_SUPPORTED),
 * except battery_get_percent whose absence makes TDL derive percent from voltage.
 * Domain ops take a single role; TDL splits an app mask into per-domain calls.
 *
 * charger_arm_event only sets up the hardware IRQ; on fire the backend's ISR calls
 * tdl_power_charger_irq_notify(ctx) with its own ctx. TDL owns the worker thread that
 * then reads charger_get_state and invokes the app callback (so the callback and any
 * I2C read run in thread context, never in the ISR).
 */
typedef struct {
    OPERATE_RET (*domain_set)(TDD_POWER_DEV_HANDLE_T ctx, TDL_POWER_DOMAIN_E domain, BOOL_T on);
    OPERATE_RET (*domain_get)(TDD_POWER_DEV_HANDLE_T ctx, TDL_POWER_DOMAIN_E domain, BOOL_T *on);
    OPERATE_RET (*battery_get_voltage)(TDD_POWER_DEV_HANDLE_T ctx, uint32_t *mv);
    OPERATE_RET (*battery_get_percent)(TDD_POWER_DEV_HANDLE_T ctx, uint8_t *pct); // only if HW fuel gauge
    OPERATE_RET (*charger_get_state)(TDD_POWER_DEV_HANDLE_T ctx, TDL_CHG_STATE_E *st);
    OPERATE_RET (*charger_arm_event)(TDD_POWER_DEV_HANDLE_T ctx); // set up HW IRQ -> notify on fire
    OPERATE_RET (*enter_deepsleep)(TDD_POWER_DEV_HANDLE_T ctx, uint32_t timer_wake_ms);
} TDL_POWER_INTFS_T;

/**
 * @brief Register the (single) power device.
 *
 * @param[in] name  Device name (convention "power").
 * @param[in] intfs Runtime capability ops (must outlive the device).
 * @param[in] info  Static device facts, copied by the TDL layer (may be NULL).
 * @param[in] ctx   Backend-private context passed back into every op (may be NULL).
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tdl_power_register(const char *name, const TDL_POWER_INTFS_T *intfs,
                               const TDL_POWER_INFO_T *info, TDD_POWER_DEV_HANDLE_T ctx);

/**
 * @brief Hand a charge-state-change event to the TDL worker. ISR-safe; meant to be
 *        called from a backend's charger IRQ. TDL then re-reads charger state and
 *        invokes the app callback in thread context.
 *
 * @param[in] ctx The backend's own context (the same one it registered / was armed
 *                with). Identifies which device fired — a driver-layer token, not the
 *                application handle.
 */
void tdl_power_charger_irq_notify(TDD_POWER_DEV_HANDLE_T ctx);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_POWER_DRIVER_H__ */
