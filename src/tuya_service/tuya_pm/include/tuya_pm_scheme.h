/**
 * @file tuya_pm_scheme.h
 * @brief tuya_pm - scheme AUTHOR API: include this to DEFINE and REGISTER a custom
 *        power scheme.
 *
 * Application / product code that only *uses* the manager (compose a descent chain,
 * register consumers / locks, request & observe schemes) needs only tuya_pm.h. This
 * header is for the other audience - whoever authors a scheme.
 *
 * A scheme is an object whose enter/exit do the actual power actions by calling the
 * low-level APIs directly (lpmgr / tal_wifi / tuya_ble_deinit / tdl_power); the
 * component wraps nothing. See docs/power_level_management_design.md.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_PM_SCHEME_H__
#define __TUYA_PM_SCHEME_H__

#include "tuya_pm.h" // TUYA_PM_SCHEME_E ids (+ BUILTIN_MAX), tuya_cloud_types

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/* A power scheme = its one-time init + what to do on enter/leave. Built-in schemes are
 * internal instances (referenced by TUYA_PM_SCHEME_E ids); custom schemes are provided
 * by the author and registered. init runs once at tuya_pm_init (only for schemes in the
 * chain); enter/exit run on every transition. All run the mechanism directly (lpmgr /
 * tal_wifi / tuya_ble_deinit / tdl_power) - the component wraps nothing; a scheme carries
 * no declarative flag, its behaviour is exactly whatever init/enter/exit do. Data and ops
 * are kept in separate structs so the ops table can be shared/reused. */

/** A scheme's operations (a vtable). */
typedef struct {
    // one-time setup at tuya_pm_init (may be NULL). power_handle is the bound tdl_power
    // handle (used by the DEEPSLEEP scheme); schemes that don't need it ignore it.
    OPERATE_RET (*init)(void *ctx, void *power_handle);
    OPERATE_RET (*enter)(void *ctx); // entering: do the power actions here
    OPERATE_RET (*exit)(void *ctx);  // leaving (may be NULL)
} TUYA_PM_SCHEME_OPS_T;

/** A scheme: its id plus a pointer to its ops. (The idle-decay threshold is not here -
 *  it is a per-step property of the descent chain, set by the chain user.) */
typedef struct {
    uint8_t                     id;  // built-in = TUYA_PM_SCHEME_E; custom >= BUILTIN_MAX
    const TUYA_PM_SCHEME_OPS_T *ops; // operations (init/enter/exit)
    void                       *ctx; // passed to the ops
} TUYA_PM_SCHEME_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Register a custom scheme into the library. Call BEFORE tuya_pm_init.
 *
 * @param[in] scheme Scheme descriptor (must outlive the component; id must be unique
 *                   and >= TUYA_PM_SCHEME_BUILTIN_MAX).
 *
 * @return OPRT_OK on success; an error code if the id duplicates a built-in or
 *         already-registered scheme, or if called after init.
 */
OPERATE_RET tuya_pm_scheme_register(const TUYA_PM_SCHEME_T *scheme);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_PM_SCHEME_H__ */
