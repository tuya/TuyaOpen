/**
 * @file netmgr_probe.h
 * @brief Reachability verdicts for netmgr: telling "the link is up" apart from
 *        "the link can reach the cloud".
 *
 * The problem
 * -----------
 * __get_active_conn() walks the connection list and returns the first link whose
 * conn->get(NETCONN_CMD_STATUS) says NETMGR_LINK_UP. NETMGR_LINK_UP means an
 * association and an address, nothing more. A device that joins an AP with no
 * WAN - a captive portal, an uplink that is down, a router still negotiating -
 * has a link that is UP by every measure netmgr can see, so on a wifi+4G board,
 * where wifi's default priority (1) beats cellular's (0), the device pins itself
 * to the useless link permanently and the 4G bearer never carries a byte.
 *
 * Fixing that needs one bit netmgr does not have: whether traffic on the active
 * link actually reaches anything. This header is where that bit comes from.
 *
 * Why the default backend sends no packets
 * ---------------------------------------
 * An active probe - a DNS query, a TCP connect to a known host, an ICMP echo -
 * is the obvious implementation and it is the wrong default here. It costs radio
 * time on a schedule netmgr chooses rather than one the product chose, and this
 * tree has two subsystems whose entire purpose is to not do that:
 * src/tal_wifi_ulp/ parks the device between cloud round trips, and src/tuya_pm/
 * budgets the wake-ups. A probe that wakes the radio every N seconds to check a
 * link that the ULP path has deliberately put to sleep is not a feature, it is a
 * regression with a nice name. On a metered cellular bearer it is also billable.
 *
 * The device already performs a reachability test, constantly, for its own
 * reasons: it talks to the cloud. Every MQTT CONNACK is proof the link works;
 * every keepalive timeout is evidence it does not. Reusing that costs nothing and
 * is strictly more informative than a synthetic probe, because it tests the path
 * the device actually needs rather than a proxy for it.
 *
 * So: the default backend is PASSIVE. It observes signals the cloud layer
 * already produces and emits no traffic of its own. An active backend is
 * expressible (see @ref netmgr_probe_backend_t) and is a product's choice, never
 * the default.
 *
 * Why this is a push API and not netmgr subscribing to the cloud
 * ------------------------------------------------------------
 * netmgr sits BELOW tuya_iot. The dependency runs cloud -> netmgr and must keep
 * running that way: tuya_iot.c calls netmgr_conn_set(), tuya_lan.c and ble_mgr.c
 * call netmgr_conn_get(), and netmgr includes no cloud header for connectivity.
 * Having netmgr call tuya_mqtt_connected() or read a tuya_iot_client_t would
 * close that cycle, and the cycle is not hypothetical - netmgr.c already
 * publishes EVENT_LINK_TYPE_CHG outside s_netmgr.lock precisely because
 * tuya_iot's subscriber calls tuya_iot_reconnect() synchronously back into
 * netmgr. Adding a read of cloud state from inside netmgr would put a second,
 * harder edge on that same cycle.
 *
 * netmgr_probe_report() inverts it. Whoever knows a verdict pushes it down;
 * netmgr includes nothing and calls nothing upward. That is also why the API
 * takes a verdict and not a transport handle: netmgr must not be able to ask a
 * question, only to be told an answer.
 *
 * Who calls it, and when
 * ----------------------
 * The default backend, netmgr_probe.c, subscribes to two event names and
 * translates them. Both already exist and both are already published:
 *
 *   EVENT_MQTT_CONNECTED    tuya_iot.c:427, from mqtt_client_connected_on(),
 *                           i.e. after CONNACK and after the inbound topic
 *                           subscribe. -> NETMGR_PROBE_GOOD.
 *   EVENT_MQTT_DISCONNECTED tuya_iot.c:443, from mqtt_client_disconnect_on().
 *                           -> NETMGR_PROBE_BAD.
 *
 * Three properties of those two call sites shape everything below.
 *
 * 1. They are event names from src/tal_system/include/tal_event_info.h, which is
 *    BELOW netmgr. Subscribing to them adds no include of any cloud header and
 *    no new dependency edge - it is the same thing tal_wifi_ulp already does at
 *    tuya_wifi_ultra_lowpower.c:185-186. So the default backend needs ZERO edits
 *    outside this module. That is the whole reason these two were chosen over the
 *    richer signals listed further down.
 *
 * 2. They fire on the application thread that loops tuya_iot_yield(); there is no
 *    MQTT worker thread. tal_event_publish() dispatches synchronously, so the
 *    subscriber runs on that thread. netmgr_probe_report() must therefore behave
 *    like netmgr_notify_link(): record and post, never run the state machine on
 *    the caller's thread. It does, and for the same reason.
 *
 * 3. EVENT_MQTT_DISCONNECTED alone is NOT evidence of an unreachable link. It
 *    fires on a deliberate tuya_mqtt_stop() as well as on a keepalive timeout,
 *    and the callback cannot tell them apart. It also fires during ordinary cloud
 *    maintenance. A single BAD must not demote a link - hence
 *    netmgr_policy_t.probe_bad_threshold, and hence the fact that GOOD is
 *    believed immediately while BAD has to repeat.
 *
 * What the passive default cannot see, and what to do about it
 * -----------------------------------------------------------
 * MQTT only connects AFTER activation. A device joining a WAN-less AP for the
 * first time never reaches CONNACK, so it produces neither GOOD nor BAD and the
 * link sits at NETMGR_LINK_STATE_UNVERIFIED forever. Two mechanisms close that
 * gap, in this order of preference:
 *
 *   (a) netmgr_policy_t.verify_timeout_ms. A link that has been active and
 *       unverified for that long is treated as if it had reported BAD. This needs
 *       no cooperation from any other layer, which is why it is the default
 *       mechanism and why it is on by default.
 *
 *   (b) ATOP. atop_base_request() (atop_base.c:354) is the single funnel every
 *       ATOP request passes through, and it already separates the two cases this
 *       module cares about:
 *
 *         atop_base.c:488-491  http_status != HTTP_CLIENT_SUCCESS
 *                              -> OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR
 *                              -> transport failed -> NETMGR_PROBE_BAD
 *         atop_base.c:502+     a response body arrived and is being parsed
 *                              -> the cloud was reached -> NETMGR_PROBE_GOOD,
 *                                 EVEN IF response->success is false or the
 *                                 errorCode is GATEWAY_NOT_EXISTS
 *
 *       That second rule is the one that is easy to get wrong: an application
 *       level rejection proves reachability. Only a transport failure is
 *       evidence against the link.
 *
 *       (b) is two call sites in atop_base.c, so it is NOT part of the default
 *       backend and it is NOT required for M3 to work. Ship it as its own
 *       reviewable change; it is what makes first-activation over a WAN-less AP
 *       recover in seconds instead of after verify_timeout_ms.
 *
 * Signals deliberately NOT used, with reasons, so nobody re-litigates them:
 *
 *   - PINGRESP. coreMQTT never surfaces it - core_mqtt.c:1186 explicitly skips
 *     the application callback for PINGRESP - so there is no hook without
 *     patching a vendored library.
 *   - PUBACK (mqtt_client_puback_cb, mqtt_service.c:338) and SUBACK
 *     (mqtt_client_subscribed_cb, mqtt_service.c:331, whose body is one
 *     PR_DEBUG). Both are genuine round-trip proof and both are nearly empty
 *     functions, so they are the best FUTURE additions. They are not in the
 *     default because each needs a new event name and an edit to mqtt_service.c,
 *     and CONNACK already covers the transitions netmgr acts on.
 *   - tuya_dev_evt_notify() (src/common/include/dev_evt.h). It brackets
 *     operations with ACTION_BEFORE/ACTION_AFTER and carries no result code, it
 *     has a single callback slot, and that slot is already claimed by the ULP
 *     wake-lock manager. It can say an operation happened, never whether it
 *     succeeded.
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
    /**
     * Nothing is known. Never reported by anyone - it is the value a zeroed
     * struct holds and the value netmgr uses internally for "no verdict yet", so
     * that NETMGR_LINK_STATE_UNVERIFIED and a zeroed accumulator agree.
     */
    NETMGR_PROBE_UNKNOWN = 0,

    /**
     * Traffic reached the cloud. Believed immediately and unconditionally: one
     * GOOD clears any accumulated BAD and moves the link to
     * NETMGR_LINK_STATE_ONLINE.
     *
     * Asymmetry with BAD is intentional. A GOOD is positive proof - a byte came
     * back - and cannot be produced by a broken link. A BAD is the absence of
     * proof, which has many causes that are not the link.
     */
    NETMGR_PROBE_GOOD = 1,

    /**
     * Traffic did not reach the cloud. Evidence, not proof. Counted against
     * netmgr_policy_t.probe_bad_threshold; only when the count reaches the
     * threshold does the link move to NETMGR_LINK_STATE_DEGRADED.
     */
    NETMGR_PROBE_BAD = 2,
} netmgr_probe_verdict_e;

#define NETMGR_PROBE_VERDICT_TO_STR(v)                                                                                 \
    ((v) == NETMGR_PROBE_GOOD      ? "good"                                                                            \
     : (v) == NETMGR_PROBE_BAD     ? "bad"                                                                             \
     : (v) == NETMGR_PROBE_UNKNOWN ? "unknown"                                                                         \
                                   : "invalid")

/**
 * @brief Where a verdict came from, for logs only.
 *
 * netmgr never branches on this. It exists because the single hardest thing about
 * debugging a link that keeps getting demoted is finding out who demoted it, and
 * a trace line that says "wifi degraded, 3 bad from mqtt" answers that
 * immediately.
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
 * netmgr bumps a counter every time it installs a different route - a different
 * active link, or the same link with a different source address. A verdict is
 * about the route that carried the traffic, not about the route that happens to
 * be installed when the report arrives, and those differ whenever a switch races
 * a report.
 *
 * The failure this prevents, concretely: wifi is active and its network is dead;
 * MQTT times out; netmgr's verify timeout has already demoted wifi and switched
 * to cellular; the MQTT disconnect callback then fires and reports BAD. Without
 * an epoch that BAD lands on cellular, which did nothing wrong.
 *
 * A reporter that can hold a value across its own network operation should read
 * the epoch before the operation and pass it back with the verdict; netmgr
 * discards a verdict whose epoch is stale. A reporter that cannot - the default
 * passive backend cannot, because it only sees an event after the fact - passes
 * NETMGR_PROBE_EPOCH_ANY and accepts the attribution window.
 *
 * The window is survivable even unguarded, and it is worth knowing why before
 * anyone builds something more elaborate: a mis-attributed BAD costs at most one
 * spurious increment on the new link's counter, probe_bad_threshold is greater
 * than one, and the next GOOD clears it. It is self-correcting within one MQTT
 * cycle. The epoch is here so that an ACTIVE backend, which can hold state
 * across its own probe, gets exact attribution for free.
 *
 * Never 0 once netmgr_init() has run; 0 is reserved for NETMGR_PROBE_EPOCH_ANY.
 * Wrapping is a non-event: only equality is ever tested.
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
 * Attribution: to the link that was active in @a result->epoch, or to the link
 * active right now when the epoch is NETMGR_PROBE_EPOCH_ANY. A reporter never
 * names a link, because a reporter does not know which link carried its packets
 * - it only knows whether they arrived. Naming one would let a caller demote a
 * link it never used.
 *
 * Thread model, and the reason it is not negotiable: this function records the
 * verdict under s_netmgr.lock and posts the same coalesced work item
 * netmgr_notify_link() posts, so the state machine still runs in exactly one
 * context, the WORKQ_SYSTEM thread. It does NOT evaluate the verdict, does not
 * reselect, does not publish and does not touch the route. That matters
 * concretely: the default backend's callers are on the tuya_iot_yield() thread,
 * which is also the MQTT keepalive pump, and running a reselect there would put a
 * blocking conn->get(NETCONN_CMD_IP) modem exchange inside the keepalive path.
 *
 * Coalescing: verdicts are ACCUMULATED, not overwritten, which is the one place
 * this differs from netmgr_notify_link(). A link report can be dropped in favour
 * of the settled state because the handler re-reads every driver; a verdict
 * cannot, because there is nothing to re-read - the count of consecutive BADs IS
 * the state. So a GOOD arriving before the handler runs still clears the
 * accumulated BADs, and two BADs arriving before the handler runs still count
 * twice.
 *
 * Dropped, returning OPRT_OK, when there is nothing to record it in: before
 * netmgr_init() has seeded the state, once netmgr_deinit() has started, when no
 * link is active, or when probing is disabled by policy. A reporter has no
 * recovery for any of those and must not treat them as errors - it is reporting a
 * fact about the world, not requesting an action.
 *
 * @param[in] result the verdict; NULL, or a verdict of NETMGR_PROBE_UNKNOWN, is
 *                   ignored and answers OPRT_INVALID_PARM
 *
 * @return OPRT_OK when accepted or deliberately dropped. Others on error, please
 *         refer to tuya_error_code.h
 */
OPERATE_RET netmgr_probe_report(const netmgr_probe_result_t *result);

/**
 * @brief Convenience form of netmgr_probe_report() for the common case.
 *
 * Exists so a reporter that has no epoch to offer is one line rather than four,
 * which is what keeps an added call site in atop_base.c small enough to review at
 * a glance.
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
 * netmgr starts the backend at the end of netmgr_init() and stops it at the
 * start of netmgr_deinit(), so a backend may create resources in start() and
 * must release all of them in stop().
 *
 * Instances are `const` and provided by the product. netmgr only reads them, and
 * it holds the pointer for the lifetime of one init/deinit cycle, so the storage
 * must outlive that - a `static const` in the product's translation unit.
 *
 * The hooks are called from netmgr_init()/netmgr_deinit() with s_netmgr.lock
 * RELEASED, under the same rule as conn->open() and conn->close(): they are
 * outward calls and they are allowed to block. A backend may therefore
 * tal_event_subscribe() in start(), which the default one does.
 */
typedef struct {
    /** Short name for logs, e.g. "mqtt". Never NULL. */
    const char *name;

    /**
     * Begin observing. NULL is allowed and means the backend needs no setup.
     *
     * @return OPRT_OK on success. A failure is logged and netmgr_init()
     *         CONTINUES - a device that cannot verify its link must still be
     *         able to use it, and treating this as fatal would turn a
     *         diagnostic feature into a boot failure.
     */
    OPERATE_RET (*start)(void);

    /**
     * Stop observing and release everything start() created. NULL is allowed.
     *
     * Must be idempotent: netmgr_deinit() is idempotent and may run on
     * netmgr_init()'s own error paths, so stop() can be reached without a
     * matching successful start().
     */
    void (*stop)(void);
} netmgr_probe_backend_t;

/**
 * @brief The built-in passive backend: EVENT_MQTT_CONNECTED/DISCONNECTED.
 *
 * Emits no traffic. Its start() is two tal_event_subscribe() calls and its
 * stop() is the two matching unsubscribes, which is also the fix for a pattern
 * this module must not repeat - netconn_wifi.c leaked two subscriptions across
 * every init/deinit cycle until M2.
 *
 * Defined in netmgr_probe.c and installed by netmgr_init() when no product
 * backend was registered, so a build that does nothing gets this one.
 */
extern const netmgr_probe_backend_t netmgr_probe_backend_mqtt;

/**
 * @brief Install a product backend, replacing the built-in one.
 *
 * Must be called before netmgr_init(); afterwards it is rejected with
 * OPRT_COM_ERROR rather than silently ignored. Same discipline, and the same
 * reasoning, as netconn_registry_set_table(): a configuration call that silently
 * reverts to the default is the failure mode these APIs exist to prevent.
 *
 * Passing NULL selects "no probing at all", which is distinct from
 * netmgr_policy_t.probe_enable = FALSE: this removes the backend, that ignores
 * its verdicts. Use the policy flag for a runtime switch and this for a build
 * that must not link the backend at all.
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
 * Takes and releases s_netmgr.lock, so the caller never formats output under it -
 * the same contract as netmgr_link_info_get() in netmgr_priv.h and for the same
 * reason.
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
