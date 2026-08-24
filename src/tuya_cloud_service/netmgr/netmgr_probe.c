/**
 * @file netmgr_probe.c
 * @brief The built-in passive reachability backend: it turns the two MQTT
 *        lifecycle events into probe verdicts and emits no traffic of its own.
 *
 * What is in this translation unit, and what is not
 * ------------------------------------------------
 * Only @ref netmgr_probe_backend_mqtt and the three functions it is made of.
 * Everything else declared in netmgr_probe.h - netmgr_probe_report(),
 * netmgr_probe_report_simple(), netmgr_probe_epoch_get(),
 * netmgr_probe_backend_set() and netmgr_probe_stat_get() - needs the per-link
 * accumulators and s_netmgr.lock, so all of it belongs to netmgr.c. This file is
 * strictly a CALLER of that API, which is what keeps it reviewable on its own:
 * two subscribes, two unsubscribes, and one uint32_t of state.
 *
 * The dependency argument, re-checked rather than inherited
 * --------------------------------------------------------
 * The includes are netmgr_probe.h plus tal_event.h and tal_log.h, and that is
 * all. EVENT_MQTT_CONNECTED and EVENT_MQTT_DISCONNECTED are string macros in
 * src/tal_system/include/tal_event_info.h:40-41, which sits BELOW netmgr, so
 * subscribing to them adds no cloud include and no new dependency edge -
 * tal_wifi_ulp does the identical thing at tuya_wifi_ultra_lowpower.c:185-186.
 *
 * The event payload is a tuya_iot_client_t *. This file must never dereference
 * it: doing so would need a cloud header and would close exactly the cycle
 * netmgr_probe.h exists to avoid. Both callbacks therefore ignore @c data
 * entirely, and the fact that they do is the whole reason this module needs zero
 * edits outside itself.
 *
 * Where a verdict really comes from
 * ---------------------------------
 * Tracing the publish sites to their causes matters, because the shape of this
 * file follows from them:
 *
 *   EVENT_MQTT_CONNECTED    tuya_iot.c:427, tail of mqtt_client_connected_on(),
 *                           reached from mqtt_client_connected_cb()
 *                           (mqtt_service.c:295) which runs after CONNACK and
 *                           after the inbound topic subscribe. A byte came back
 *                           from the cloud: positive proof. -> GOOD.
 *
 *   EVENT_MQTT_DISCONNECTED tuya_iot.c:443, tail of mqtt_client_disconnect_on().
 *                           Its ONLY producer is mqtt_client_disconnect()
 *                           (mqtt_client_wrapper.c:233), which invokes
 *                           on_disconnected unconditionally, and that function
 *                           has exactly two callers:
 *                             - mqtt_client_yield() when MQTT_ProcessLoop()
 *                               fails (mqtt_client_wrapper.c:317-319). Keepalive
 *                               timeout or socket error: real evidence.
 *                             - tuya_mqtt_stop() (mqtt_service.c:508). A
 *                               deliberate teardown: no evidence at all.
 *                           -> BAD, as evidence and never as proof.
 *
 * Two consequences of that trace, both load-bearing:
 *
 * 1. A FAILED CONNECT PRODUCES NOTHING. mqtt_client_connect() on error closes
 *    the transporter and returns (mqtt_client_wrapper.c:218-223) without ever
 *    calling on_disconnected. So the reconnect loop in tuya_mqtt_loop() can spin
 *    against a dead WAN forever - back-off, retry, fail - in complete silence.
 *    This backend cannot see it. netmgr_policy_t.verify_timeout_ms is the only
 *    thing that can, which is why netmgr_probe.h calls it the default mechanism
 *    rather than a fallback.
 *
 * 2. EVERY BAD IS PRECEDED BY A GOOD, on any route that still manages to
 *    connect: a session must come up (GOOD, which resets the count) before there
 *    is a session to tear down (BAD). Consecutive BADs from THIS backend are
 *    therefore not the normal case, and probe_bad_threshold is not a countdown
 *    this backend walks down on its own. See the note on the threshold at the
 *    bottom of this comment.
 *
 * Thread context
 * --------------
 * Both callbacks run on the application thread that loops tuya_iot_yield():
 * tuya_iot_yield() STATE_MQTT_YIELD calls tuya_mqtt_loop() inline
 * (tuya_iot.c:911), which calls mqtt_client_yield() (mqtt_service.c:889), and
 * tal_event_publish() dispatches synchronously under the event's own mutex
 * (tal_event.c:386-392). There is no MQTT worker thread anywhere in the path.
 *
 * That thread is also the keepalive pump, so neither callback may do anything
 * slow. Both do the same two things and nothing else: read a uint32_t and call
 * netmgr_probe_report(), which by contract only records and posts. Running a
 * reselect here would put a blocking conn->get(NETCONN_CMD_IP) modem exchange
 * inside the keepalive path.
 *
 * What probe_bad_threshold actually does to this backend
 * -----------------------------------------------------
 * Follow consequence 2 above to its end, because it changes what the threshold
 * is for. On a route that can still reach the broker, the verdict stream is
 * GOOD, BAD, GOOD, BAD - every session start emits a GOOD that resets the count,
 * so bad_count from this backend never reaches 2 there. On a route that cannot
 * reach the broker, the stream is one BAD and then silence, because failed
 * connects emit nothing. Either way this backend on its own does not walk a
 * default threshold of 3 down to DEGRADED, and it is not supposed to: the
 * mechanism that demotes a WAN-less link is verify_timeout_ms, and the threshold
 * is a filter in front of it, not a second path to it.
 *
 * The threshold still has to be greater than 1, and the reason survives the
 * epoch guard above. A teardown caused by netmgr's own handover now carries a
 * stale epoch and is discarded, but a deliberate tuya_mqtt_stop() from anywhere
 * else - STATE_STOP at tuya_iot.c:1108, run_state_reset() at :555,
 * tuya_iot_destroy() at :789 - happens with the route unchanged, so it carries a
 * LIVE epoch and lands as a genuine-looking BAD against a link that is fine.
 * Nothing in this module
 * can tell that apart: mqtt_client_disconnect_on() sees only the fact of a
 * disconnect, and tuya_mqtt_context_t.manual_disconnect is no help even to code
 * that could read it, because tuya_mqtt_stop() sets it AFTER
 * mqtt_client_disconnect() has already published the event (mqtt_service.c:508
 * then :511).
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "netmgr_probe.h"
#include "tal_event.h"
#include "tal_log.h"

/***********************************************************
************************** state ***************************
***********************************************************/

/**
 * The subscriber description. tal_event matches (name, desc, cb) on both
 * subscribe and unsubscribe, so the unsubscribes below only cancel the right
 * nodes as long as this is the same string in all four calls - hence one macro
 * rather than four literals.
 */
#define PROBE_MQTT_SUBSCRIBER "netmgr_probe"

/**
 * The route epoch observed when the CURRENT MQTT session came up, or
 * NETMGR_PROBE_EPOCH_ANY when no session has been observed since start().
 *
 * Why this variable exists at all, when netmgr_probe.h says the passive backend
 * "cannot" hold an epoch and should pass NETMGR_PROBE_EPOCH_ANY: it can, and
 * unguarded attribution here is not the rare race the header describes but a
 * GUARANTEED one, once per switch. The chain is deterministic:
 *
 *   netmgr installs a new route, bumps the epoch, publishes EVENT_LINK_TYPE_CHG
 *     -> __tuya_iot_link_type_change_cb() (tuya_iot.c:870)
 *     -> tuya_iot_reconnect() sets nextstate = STATE_MQTT_RECONNECT
 *     -> the next tuya_iot_yield() runs tuya_mqtt_stop() (tuya_iot.c:1078)
 *     -> mqtt_client_disconnect() -> EVENT_MQTT_DISCONNECTED
 *
 * So netmgr's own handover makes tuya_iot tear down MQTT, and the resulting
 * teardown arrives AFTER the epoch was bumped. Reported as EPOCH_ANY it lands on
 * the link netmgr just switched TO, every single time - a spurious BAD against
 * the one link that has done nothing wrong yet. Self-correcting, as the header
 * says, but only after a round trip, and free to avoid.
 *
 * A route change breaks the socket, so one MQTT session lives entirely within
 * one epoch: the epoch seen at CONNACK is the epoch of the whole session, and a
 * verdict about the session's end belongs to it. That is the value stored here.
 *
 * It is deliberately NOT consumed when a BAD is reported, because a session can
 * be torn down twice: the keepalive death, and then the tuya_mqtt_stop() that
 * netmgr's own switch provokes. Keeping the value until the next CONNECTED gives
 * both the same epoch, so the first is attributed to the route that really
 * failed and the second - by then stale - is discarded. Consuming it would send
 * the second one out as EPOCH_ANY, straight onto the new link, which is the
 * mis-attribution this variable exists to prevent.
 *
 * Storage: written by both callbacks, which are the same thread; reset in
 * start() and stop(), where no callback can be running because
 * tal_event_unsubscribe() serialises against dispatch on the event mutex
 * (tal_event.c:498-500). No lock and no volatile is needed, and adding either
 * would imply a concurrency that does not exist.
 */
static uint32_t s_session_epoch = NETMGR_PROBE_EPOCH_ANY;

/** Whether the two subscriptions are currently installed. */
static BOOL_T s_subscribed = FALSE;

/***********************************************************
******************** function declaration ******************
***********************************************************/

static int         __probe_mqtt_connected_cb(void *data);
static int         __probe_mqtt_disconnected_cb(void *data);
static OPERATE_RET __probe_mqtt_start(void);
static void        __probe_mqtt_stop(void);

/***********************************************************
*********************** the backend ************************
***********************************************************/

const netmgr_probe_backend_t netmgr_probe_backend_mqtt = {
    .name  = "mqtt",
    .start = __probe_mqtt_start,
    .stop  = __probe_mqtt_stop,
};

/***********************************************************
************************ observers *************************
***********************************************************/

/**
 * @brief EVENT_MQTT_CONNECTED: CONNACK arrived, so the active route works.
 *
 * The epoch is read HERE and not inside netmgr_probe_report(), because here is
 * the moment of observation: the CONNACK came in on the route installed right
 * now. Reading it later - or letting netmgr substitute whatever is active when
 * the work item runs - would attribute the proof to a route that never carried
 * it.
 *
 * @param[in] data the publisher's tuya_iot_client_t *, deliberately unused
 *
 * @return OPRT_OK always. _event_node_dispatch() logs a non-OK return and
 *         carries on to the next subscriber, and there is nothing here another
 *         subscriber should be told about.
 */
static int __probe_mqtt_connected_cb(void *data)
{
    (void)data;

    uint32_t epoch = netmgr_probe_epoch_get();

    /* Remember the session's epoch for the teardown that will end it. */
    s_session_epoch = epoch;

    const netmgr_probe_result_t result = {
        .verdict = NETMGR_PROBE_GOOD,
        .source  = NETMGR_PROBE_SRC_MQTT,
        .epoch   = epoch,
    };

    /* Return value ignored on purpose: netmgr_probe_report() answers OPRT_OK for
     * both accepted and deliberately dropped, and a reporter has no recovery for
     * the error cases either - it is stating a fact, not requesting an action. */
    (void)netmgr_probe_report(&result);

    PR_DEBUG("probe mqtt good, epoch %u", (unsigned int)epoch);
    return OPRT_OK;
}

/**
 * @brief EVENT_MQTT_DISCONNECTED: a session ended. Evidence, not proof.
 *
 * Carries the epoch of the session that ended, not a fresh reading, so that a
 * teardown caused by netmgr's own handover is discarded by netmgr as stale
 * instead of being charged to the link that was just selected. See
 * @ref s_session_epoch for why the value is not cleared here.
 *
 * NETMGR_PROBE_EPOCH_ANY is passed when no CONNACK has been observed since
 * start() - a teardown of a session this backend never saw come up. That is the
 * unguarded attribution the header describes, and it is the right answer for it:
 * the module has no better information, and the header's own reasoning applies
 * (one spurious increment, threshold greater than one, cleared by the next GOOD).
 *
 * @param[in] data the publisher's tuya_iot_client_t *, deliberately unused
 *
 * @return OPRT_OK always, as __probe_mqtt_connected_cb()
 */
static int __probe_mqtt_disconnected_cb(void *data)
{
    (void)data;

    const netmgr_probe_result_t result = {
        .verdict = NETMGR_PROBE_BAD,
        .source  = NETMGR_PROBE_SRC_MQTT,
        .epoch   = s_session_epoch,
    };

    (void)netmgr_probe_report(&result);

    PR_DEBUG("probe mqtt bad, epoch %u", (unsigned int)s_session_epoch);
    return OPRT_OK;
}

/***********************************************************
************************ lifecycle *************************
***********************************************************/

/**
 * @brief Begin observing. Two subscribes, nothing allocated, nothing sent.
 *
 * Called from the tail of netmgr_init() with s_netmgr.lock released, so blocking
 * inside tal_event_subscribe() is allowed.
 *
 * The second subscribe's failure path cancels the first. That is not
 * defensiveness for its own sake: netmgr_init() logs a start() failure and
 * CONTINUES, so a half-installed backend would survive the whole init/deinit
 * cycle and would be exactly the leak netconn_wifi.c carried until M2 - the one
 * netmgr_probe.h names as the pattern this module must not repeat.
 *
 * @return OPRT_OK on success, otherwise the tal_event_subscribe() error
 */
static OPERATE_RET __probe_mqtt_start(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (s_subscribed) {
        return OPRT_OK;
    }

    /* No session has been observed yet in this cycle. Set before subscribing, so
     * a CONNACK that lands between the two subscribes cannot be overwritten. */
    s_session_epoch = NETMGR_PROBE_EPOCH_ANY;

    rt = tal_event_subscribe(EVENT_MQTT_CONNECTED, PROBE_MQTT_SUBSCRIBER, __probe_mqtt_connected_cb,
                             SUBSCRIBE_TYPE_NORMAL);
    if (OPRT_OK != rt) {
        PR_ERR("probe mqtt subscribe %s failed, %d", EVENT_MQTT_CONNECTED, rt);
        return rt;
    }

    rt = tal_event_subscribe(EVENT_MQTT_DISCONNECTED, PROBE_MQTT_SUBSCRIBER, __probe_mqtt_disconnected_cb,
                             SUBSCRIBE_TYPE_NORMAL);
    if (OPRT_OK != rt) {
        PR_ERR("probe mqtt subscribe %s failed, %d", EVENT_MQTT_DISCONNECTED, rt);
        tal_event_unsubscribe(EVENT_MQTT_CONNECTED, PROBE_MQTT_SUBSCRIBER, __probe_mqtt_connected_cb);
        return rt;
    }

    s_subscribed = TRUE;
    PR_DEBUG("probe backend mqtt started, passive");
    return OPRT_OK;
}

/**
 * @brief Stop observing and release what start() created, which is two list
 *        nodes inside tal_event.
 *
 * Idempotent, as the contract requires: netmgr_deinit() is idempotent and may
 * run on netmgr_init()'s own error paths, so this can be reached with no
 * matching successful start(). The flag rather than a bare pair of unsubscribes,
 * because tal_event's tolerance of an unknown node is an implementation detail
 * (tal_event.c:253-255 "pretend to success") and not something its header
 * promises.
 *
 * Ordering: the epoch is cleared only AFTER both unsubscribes return. Dispatch
 * runs under the event mutex and tal_event_unsubscribe() takes the same mutex,
 * so once they return no callback is running and none can start - clearing
 * earlier would race a callback still in flight on the yield thread. netmgr
 * calls this with s_netmgr.lock released, so waiting on that mutex here cannot
 * invert against a callback that is inside netmgr_probe_report().
 */
static void __probe_mqtt_stop(void)
{
    if (!s_subscribed) {
        return;
    }
    s_subscribed = FALSE;

    tal_event_unsubscribe(EVENT_MQTT_DISCONNECTED, PROBE_MQTT_SUBSCRIBER, __probe_mqtt_disconnected_cb);
    tal_event_unsubscribe(EVENT_MQTT_CONNECTED, PROBE_MQTT_SUBSCRIBER, __probe_mqtt_connected_cb);

    s_session_epoch = NETMGR_PROBE_EPOCH_ANY;
    PR_DEBUG("probe backend mqtt stopped");
}
