/**
 * @file tuya_pm.h
 * @brief Device power manager (v3) - a "power-scheme library + scheduler".
 *
 * The core is a pure scheduler: it arbitrates which scheme is effective, decays on
 * idle along a descent chain, and calls each scheme's enter/exit on transitions. It
 * implements no mechanism itself. A "scheme" is an object whose enter/exit do the
 * actual power actions (WiFi power-save, BLE off, deep sleep) by calling the low-level
 * APIs directly. Built-in schemes (ACTIVE/CEC_T20/ULP_ONLINE/DEEPSLEEP) live inside the
 * component; developers may register their own schemes and compose a custom chain.
 *
 * Two audiences, two headers:
 *  - scheme USERS (this header): compose the descent chain, set the product policy,
 *    register consumers/locks, request & observe schemes.
 *  - scheme AUTHORS (tuya_pm_scheme.h): define and register a custom scheme.
 * See docs/power_level_management_design.md.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __TUYA_PM_H__
#define __TUYA_PM_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/**
 * @brief Built-in scheme ids (shallow to deep). Custom scheme ids start at
 *        TUYA_PM_SCHEME_BUILTIN_MAX and are chosen by the author.
 *
 * @note CEC_T20/ULP_ONLINE save power via WiFi power-save, which the modem enters only
 *       when associated to an AP in station mode with BLE off (shared-antenna dual-mode
 *       modules cannot enter PS otherwise). The built-in CEC_T20/ULP_ONLINE schemes turn
 *       BLE off in their enter(); the application must guarantee the association (see
 *       the example's link gate).
 */
typedef enum {
    TUYA_PM_ACTIVE = 0,         // full speed, online
    TUYA_PM_CEC_T20,            // online standby; power aligned to CEC Title 20 network-standby
    TUYA_PM_ULP_ONLINE,         // ultra-low-power online: DTIM keep-alive, stays cloud-reachable (uA)
    TUYA_PM_DEEPSLEEP,          // offline deep sleep, external wakeup
    TUYA_PM_SCHEME_BUILTIN_MAX, // custom scheme ids start here
} TUYA_PM_SCHEME_E;

/* Defining and registering a custom scheme (TUYA_PM_SCHEME_T, tuya_pm_scheme_register)
 * is the scheme-author API - see tuya_pm_scheme.h. Application code only uses the ids
 * above to compose a chain / request / gate. */

/** One step of the descent chain: a scheme and how long idle before decaying into it. */
typedef struct {
    uint8_t  scheme_id;        // built-in via TUYA_PM_SCHEME_E, custom via its id
    uint32_t min_residency_ms; // idle must be >= this before decaying into this step
                               // (ignored for the chain head; 0 = decay right away)
} TUYA_PM_CHAIN_STEP_T;

/** Battery policy (optional). */
typedef struct {
    uint8_t  lifeboat_scheme;    // low-battery escape: scheme id to force (e.g. TUYA_PM_DEEPSLEEP);
                                 // may be outside the descent chain
    uint16_t lifeboat_below_mv;  // force lifeboat_scheme below this voltage (0 = off)
    BOOL_T   charging_holds_active; // pin to the shallowest scheme while charging
} TUYA_PM_BATTERY_POLICY_T;

/** Hold-lock handle. */
typedef void *TUYA_PM_LOCK_HANDLE;

/** Scheme-change callback (ids). */
typedef void (*TUYA_PM_CHANGE_CB)(uint8_t from, uint8_t to, void *arg);

/** Consumer handle. */
typedef void *TUYA_PM_CONSUMER_HANDLE;

/** A managed peripheral (consumer) registered against the power manager. */
typedef struct {
    const char *name;
    uint8_t     min_powered_level; // powered up to this scheme (inclusive); suspend if deeper
    uint8_t     priority;          // higher suspends first; resume in reverse
    OPERATE_RET (*suspend)(void *arg); // tear down before a deeper scheme (thread ctx, may block)
    OPERATE_RET (*resume)(void *arg);  // restore when back at <= min scheme
    void        *arg;
} TUYA_PM_CONSUMER_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/* ---- descent chain (optional) ---- */

/**
 * @brief Compose the descent chain: an ordered array of steps, shallow to deep. The
 *        order is BOTH the depth order and the idle-decay path, and the chain also
 *        defines which schemes exist for arbitration - only schemes in the chain may be
 *        referenced by request / lock floor / consumer min. Idle decay stops at the tail;
 *        each step's min_residency_ms is how long idle before descending into it (the
 *        chain user's decay-speed choice).
 *
 *        Required to enable auto-decay: if not called, the chain is just { ACTIVE } and
 *        the device never auto-decays (stays ACTIVE). Call BEFORE tuya_pm_init.
 *
 * @param[in] steps Ordered chain steps.
 * @param[in] n     Number of steps.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tuya_pm_set_chain(const TUYA_PM_CHAIN_STEP_T *steps, uint8_t n);

/* ---- init & battery ---- */

/**
 * @brief Set the battery policy (charging holds / low-battery lifeboat). Optional; needs
 *        a tdl_power device (bound in tuya_pm_init). Call BEFORE tuya_pm_init.
 *
 * @param[in] batt Battery policy; copied internally.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tuya_pm_set_battery(const TUYA_PM_BATTERY_POLICY_T *batt);

/**
 * @brief Initialize the power manager. Register schemes / compose the chain / set the
 *        battery policy first; after init the library and chain are read-only.
 *
 * @param[in] power_dev_name tdl_power device to bind (rails / battery / deep sleep);
 *                           NULL if the product has no tdl_power device.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tuya_pm_init(const char *power_dev_name);

/* ---- dynamic hold-lock (refcount floor) ---- */

/**
 * @brief Create a hold-lock. While held, the effective scheme is no deeper than floor.
 *
 * @param[in]  name  Debug name (may be NULL).
 * @param[in]  floor Shallowest floor scheme id allowed while held (e.g. TUYA_PM_ACTIVE
 *                   = stay awake).
 * @param[out] out   Lock handle.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tuya_pm_lock_create(const char *name, uint8_t floor, TUYA_PM_LOCK_HANDLE *out);

/**
 * @brief Acquire a hold-lock (refcount++). The same handle stacks: each acquire
 *        increments the count, so every acquire must be paired with a release. The lock
 *        is active (clamps the effective scheme no deeper than floor) while count > 0.
 *        Callers wanting boolean on/off semantics should guard against double-acquire.
 *
 * @param[in] lk Lock handle.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tuya_pm_lock_acquire(TUYA_PM_LOCK_HANDLE lk);

/**
 * @brief Release a hold-lock (refcount--, floored at 0).
 *
 * @param[in] lk Lock handle.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tuya_pm_lock_release(TUYA_PM_LOCK_HANDLE lk);

/* ---- activity / request ---- */

/**
 * @brief Report activity: reset the idle-decay timer and bounce back to the chain head.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tuya_pm_activity(void);

/**
 * @brief Request and hold a specific scheme; stops idle decay until the next request
 *        or activity. Useful to pin a scheme (e.g. for power measurement). Still
 *        clamped by hold-locks. The scheme must be in the chain.
 *
 * @param[in] scheme_id Desired scheme id.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tuya_pm_request(uint8_t scheme_id);

/* ---- observe / debug ---- */

/**
 * @brief Get the current effective scheme id.
 *
 * @return The current scheme id.
 */
uint8_t tuya_pm_current(void);

/**
 * @brief Register a callback invoked on effective-scheme changes.
 *
 * @param[in] cb  Callback function.
 * @param[in] arg User argument passed to the callback.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tuya_pm_on_change(TUYA_PM_CHANGE_CB cb, void *arg);

/**
 * @brief Print the effective scheme, descent chain, all lock holders and consumer
 *        states (debug).
 */
void tuya_pm_dump(void);

/* ---- consumer (managed peripheral, follows the scheme via suspend/resume) ---- */

/**
 * @brief Register a managed peripheral (consumer).
 *
 * @param[in]  c   Consumer descriptor (must outlive the registration).
 * @param[out] out Consumer handle.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tuya_pm_consumer_register(const TUYA_PM_CONSUMER_T *c, TUYA_PM_CONSUMER_HANDLE *out);

/**
 * @brief Unregister a managed peripheral.
 *
 * @param[in] h Consumer handle.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tuya_pm_consumer_unregister(TUYA_PM_CONSUMER_HANDLE h);

/**
 * @brief Convenience: register a trivial consumer that just gates a tdl_power domain.
 *
 * @param[in]  name              Debug name (may be NULL).
 * @param[in]  domain_mask       OR of TDL_POWER_DOMAIN_E roles to gate.
 * @param[in]  min_powered_level Powered up to this scheme (inclusive); gated off if deeper.
 * @param[in]  priority          Suspend order (higher first; resume in reverse).
 * @param[out] out               Consumer handle.
 *
 * @return OPRT_OK on success, or an appropriate error code on failure.
 */
OPERATE_RET tuya_pm_consumer_register_rail(const char *name, uint32_t domain_mask,
                                           uint8_t min_powered_level,
                                           uint8_t priority, TUYA_PM_CONSUMER_HANDLE *out);

/* Deep-sleep wakeup sources are a board hardware fact declared via tdl_power and armed
 * by tdl_power_enter_deepsleep() (called from the built-in DEEPSLEEP scheme's enter);
 * tuya_pm has no wakeup-source API of its own. */

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_PM_H__ */
