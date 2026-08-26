/**
 * @file netmgr_probe.h
 * @brief Reachability verdicts for netmgr: telling "the link is up" apart
 *        from "the link can reach the cloud".
 *
 * NETMGR_LINK_UP means an association and an address, nothing more - a link
 * to a captive portal or a dead uplink is UP by every measure netmgr can
 * see, so on a wifi+4G board that pins the device to a useless link forever.
 * This header supplies the missing bit: does traffic on the active link
 * actually reach anything.
 *
 * The default backend is PASSIVE: it observes signals the cloud layer
 * already produces (MQTT CONNACK / keepalive timeout) instead of sending
 * probes of its own, which would cost radio time on netmgr's schedule
 * rather than the product's - exactly what src/tal_wifi_ulp/ and
 * src/tuya_pm/ exist to avoid, and billable on a metered bearer. An active
 * backend is expressible (@ref netmgr_probe_backend_t) but is a product's
 * choice, never the default.
 *
 * This is a PUSH API, not netmgr subscribing to the cloud, and that is not
 * negotiable: netmgr sits BELOW tuya_iot, and a netmgr-side read of cloud
 * state would add a second, harder re-entrancy edge to one netmgr.c already
 * has. netmgr_probe_report() inverts it - whoever knows a verdict pushes it
 * down - and carries a verdict, never a transport handle: netmgr can be
 * told an answer but never ask a question.
 *
 * EVENT_MQTT_DISCONNECTED alone is NOT evidence of an unreachable link - it
 * fires on a deliberate stop as well as a keepalive timeout, and a callback
 * cannot tell them apart, hence netmgr_policy_t.probe_bad_threshold and GOOD
 * being believed immediately while BAD has to repeat.
 *
 * What the passive default cannot see: MQTT only connects AFTER activation,
 * so a device joining a WAN-less AP the first time produces neither verdict
 * and sits UNVERIFIED forever, until netmgr_policy_t.verify_timeout_ms
 * closes the gap. An ATOP-based backend (a separate, optional addition) can
 * close it faster.
 *
 * Signals deliberately not used, and why: PINGRESP (coreMQTT hides it from
 * the application callback), PUBACK/SUBACK (real proof, but each needs a new
 * event name), tuya_dev_evt_notify() (no result code, and its one callback
 * slot is already claimed by the ULP wake-lock manager).
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __NETMGR_PROBE_H__
#define __NETMGR_PROBE_H__

#include "tuya_cloud_types.h"
#include "netmgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************* verdicts *************************
***********************************************************/

/**
 * @brief What a reporter observed about the path out of the active link.
 */
typedef enum {
    /** Nothing is known - never reported by anyone. The value a zeroed
     *  struct holds and netmgr's internal "no verdict yet", kept in sync
     *  with NETMGR_LINK_STATE_UNVERIFIED. */
    NETMGR_PROBE_UNKNOWN = 0,

    /** Traffic reached the cloud. Believed immediately and unconditionally -
     *  one GOOD clears any accumulated BAD and moves the link to
     *  NETMGR_LINK_STATE_ONLINE. Asymmetric with BAD on purpose: a GOOD is
     *  positive proof a broken link cannot produce, while a BAD is only the
     *  absence of proof, which has many causes that are not the link. */
    NETMGR_PROBE_GOOD = 1,

    /** Traffic did not reach the cloud. Evidence, not proof - counted
     *  against netmgr_policy_t.probe_bad_threshold, and only at the
     *  threshold does the link move to NETMGR_LINK_STATE_DEGRADED. */
    NETMGR_PROBE_BAD = 2,
} netmgr_probe_verdict_e;

#define NETMGR_PROBE_VERDICT_TO_STR(v)                                                                                 \
    ((v) == NETMGR_PROBE_GOOD      ? "good"                                                                            \
     : (v) == NETMGR_PROBE_BAD     ? "bad"                                                                             \
     : (v) == NETMGR_PROBE_UNKNOWN ? "unknown"                                                                         \
                                   : "invalid")

/**
 * @brief Where a verdict came from, for logs only - netmgr never branches on
 *        this. Answers the hardest question when debugging a demoted link:
 *        who demoted it.
 */
typedef enum {
    NETMGR_PROBE_SRC_NONE = 0,
    /** The default passive backend, from EVENT_MQTT_CONNECTED/DISCONNECTED. */
    NETMGR_PROBE_SRC_MQTT = 1,
    /** atop_base_request(), if the optional ATOP reporting is built in. */
    NETMGR_PROBE_SRC_ATOP = 2,
    /** netmgr's own verify_timeout_ms expiring. Synthesised, not reported. */
    NETMGR_PROBE_SRC_TIMEOUT = 3,
    /** A product-supplied backend. */
    NETMGR_PROBE_SRC_CUSTOM = 4,
} netmgr_probe_source_e;

#define NETMGR_PROBE_SRC_TO_STR(s)                                                                                     \
    ((s) == NETMGR_PROBE_SRC_MQTT      ? "mqtt"                                                                        \
     : (s) == NETMGR_PROBE_SRC_ATOP    ? "atop"                                                                        \
     : (s) == NETMGR_PROBE_SRC_TIMEOUT ? "timeout"                                                                     \
     : (s) == NETMGR_PROBE_SRC_CUSTOM  ? "custom"                                                                      \
                                       : "none")

/***********************************************************
************************** epoch ***************************
***********************************************************/

/**
 * @brief "I did not observe an epoch; attribute this to whatever is active now."
 */
#define NETMGR_PROBE_EPOCH_ANY ((uint32_t)0)

/**
 * @brief The route generation a verdict belongs to.
 *
 * netmgr bumps a counter every time it installs a different route. A verdict
 * is about the route that carried the traffic, not the route installed when
 * the report arrives, and those differ whenever a switch races a report:
 * wifi dies, MQTT times out, netmgr's verify timeout demotes wifi and
 * switches to cellular, and only then does the MQTT disconnect callback
 * fire and report BAD - without an epoch that BAD lands on cellular, which
 * did nothing wrong.
 *
 * A reporter able to hold a value across its own network operation should
 * read the epoch before the operation and pass it back with the verdict;
 * netmgr discards a verdict whose epoch is stale. One that cannot passes
 * NETMGR_PROBE_EPOCH_ANY and accepts the attribution window, which is
 * survivable even unguarded: a mis-attributed BAD costs at most one
 * spurious increment, probe_bad_threshold is greater than one, and the next
 * GOOD clears it. The epoch exists so an ACTIVE backend, which can hold
 * state across its own probe, gets exact attribution for free.
 *
 * Never 0 once netmgr_init() has run - 0 is NETMGR_PROBE_EPOCH_ANY. Wrapping
 * is a non-event: only equality is ever tested.
 *
 * @return the current epoch, or 0 when netmgr is not initialised
 */
uint32_t netmgr_probe_epoch_get(void);

/***********************************************************
********************** reporting API ***********************
***********************************************************/

/**
 * @brief One verdict about the path out of the active link.
 */
typedef struct {
    netmgr_probe_verdict_e verdict;
    netmgr_probe_source_e  source;
    /**
     * The epoch this verdict is about, from netmgr_probe_epoch_get(), or
     * NETMGR_PROBE_EPOCH_ANY. See @ref netmgr_probe_epoch_get.
     */
    uint32_t epoch;
} netmgr_probe_result_t;

/**
 * @brief Tell netmgr what a reporter observed. Safe from any context.
 *
 * Attribution: to the link active in @a result->epoch, or the link active
 * right now for NETMGR_PROBE_EPOCH_ANY. A reporter never names a link - it
 * only knows whether its own packets arrived, not which link carried them.
 *
 * Records the verdict under s_netmgr.lock and posts the same coalesced work
 * item netmgr_notify_link() posts, without evaluating, reselecting or
 * touching the route - the default backend's callers are on the
 * tuya_iot_yield()/MQTT-keepalive thread, and a reselect there would put a
 * blocking modem exchange inside the keepalive path.
 *
 * Verdicts ACCUMULATE rather than overwrite, unlike a netmgr_notify_link()
 * link report: there is nothing to re-read later, since the count of
 * consecutive BADs IS the state - a GOOD before the handler runs still
 * clears accumulated BADs, and two BADs before it still count twice.
 *
 * Dropped, returning OPRT_OK, when there is nothing to record it in: before
 * netmgr_init() has seeded state, once netmgr_deinit() has started, when no
 * link is active, or when probing is disabled by policy - a reporter must
 * treat these as facts about the world, not as errors.
 *
 * @param[in] result the verdict; NULL, or a verdict of NETMGR_PROBE_UNKNOWN, is
 *                   ignored and answers OPRT_INVALID_PARM
 *
 * @return OPRT_OK when accepted or deliberately dropped. Others on error, please
 *         refer to tuya_error_code.h
 */
OPERATE_RET netmgr_probe_report(const netmgr_probe_result_t *result);

/**
 * @brief Convenience form of netmgr_probe_report() for the common case: no
 *        epoch to offer.
 *
 * @param[in] verdict what was observed
 * @param[in] source  who observed it, for logs
 *
 * @return as netmgr_probe_report()
 */
OPERATE_RET netmgr_probe_report_simple(netmgr_probe_verdict_e verdict, netmgr_probe_source_e source);

/***********************************************************
*********************** the backend ************************
***********************************************************/

/**
 * @brief A reachability backend: something that produces verdicts.
 *
 * netmgr starts it at the end of netmgr_init() and stops it at the start of
 * netmgr_deinit(), with s_netmgr.lock RELEASED (the same rule as
 * conn->open()/close()), so a backend may block or tal_event_subscribe() in
 * start() and must release everything in stop(). Instances are `const` and
 * provided by the product; netmgr holds the pointer for one whole
 * init/deinit cycle, so the storage - a `static const` in the product's
 * translation unit - must outlive that.
 */
typedef struct {
    /** Short name for logs, e.g. "mqtt". Never NULL. */
    const char *name;

    /**
     * Begin observing. NULL means the backend needs no setup.
     *
     * @return OPRT_OK on success. A failure is logged and netmgr_init()
     *         CONTINUES - a device that cannot verify its link must still be
     *         able to use it.
     */
    OPERATE_RET (*start)(void);

    /**
     * Stop observing and release everything start() created. NULL is
     * allowed. Must be idempotent: netmgr_deinit() is idempotent and may run
     * on netmgr_init()'s own error paths, so stop() can be reached without a
     * matching successful start().
     */
    void (*stop)(void);
} netmgr_probe_backend_t;

/**
 * @brief The built-in passive backend: EVENT_MQTT_CONNECTED/DISCONNECTED.
 *
 * Emits no traffic. Installed by netmgr_init() when no product backend was
 * registered, so a build that selects ENABLE_NETMGR_PROBE and does nothing
 * else gets this one. WITH THAT SYMBOL UNSET THIS DOES NOT EXIST -
 * CMakeLists.txt drops netmgr_probe.c, and netmgr_init()'s only reference
 * carries the same #if. Left unguarded here: a product with its own backend
 * has no reason to care, and a guarded extern would turn a misconfiguration
 * into a confusing "undeclared identifier" instead of a clear link error.
 */
extern const netmgr_probe_backend_t netmgr_probe_backend_mqtt;

/**
 * @brief Install a product backend, replacing the built-in one.
 *
 * Must be called before netmgr_init(); afterwards it is rejected with
 * OPRT_COM_ERROR rather than silently ignored, the same discipline as
 * netconn_registry_set_table().
 *
 * Passing NULL selects "no probing at all", distinct from
 * netmgr_policy_t.probe_enable = FALSE: this removes the backend, that
 * ignores its verdicts. It does not stop netmgr_probe_backend_mqtt from
 * being LINKED, since netmgr_init() still names it in the branch this call
 * steers away from - a build that must not contain it leaves Kconfig's
 * ENABLE_NETMGR_PROBE unset instead.
 *
 * @param[in] backend the backend to install, or NULL for none
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET netmgr_probe_backend_set(const netmgr_probe_backend_t *backend);

/***********************************************************
********************* observability ************************
***********************************************************/

/**
 * @brief What netmgr has accumulated about one link, for the CLI and for logs.
 */
typedef struct {
    /** Last verdict recorded for this link, or NETMGR_PROBE_UNKNOWN. */
    netmgr_probe_verdict_e last;
    /** Who reported it. */
    netmgr_probe_source_e source;
    /** Consecutive BADs since the last GOOD. Reset to 0 by any GOOD. */
    uint8_t bad_count;
    /** Total GOODs and BADs seen, saturating. Diagnostics only. */
    uint16_t good_total;
    uint16_t bad_total;
} netmgr_probe_stat_t;

/**
 * @brief Snapshot the probe accumulator for one link.
 *
 * Takes and releases s_netmgr.lock, so the caller never formats output under
 * it - the same contract as netmgr_link_info_get() in netmgr_priv.h.
 *
 * @param[in]  type a single netmgr_type_e bit
 * @param[out] stat filled on OPRT_OK, untouched otherwise
 *
 * @return OPRT_OK on success, OPRT_NOT_FOUND when @a type is not registered.
 *         Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET netmgr_probe_stat_get(netmgr_type_e type, netmgr_probe_stat_t *stat);

#ifdef __cplusplus
}
#endif

#endif /* __NETMGR_PROBE_H__ */
