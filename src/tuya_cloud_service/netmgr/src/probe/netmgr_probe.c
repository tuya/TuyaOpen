/**
 * @file netmgr_probe.c
 * @brief The built-in passive reachability backend: it turns the two MQTT
 *        lifecycle events into probe verdicts and emits no traffic of its own.
 *
 * Only @ref netmgr_probe_backend_mqtt and the three functions it is made of;
 * everything else declared in netmgr_probe.h needs s_netmgr.lock and belongs
 * to netmgr.c. Compiled only when Kconfig's ENABLE_NETMGR_PROBE is selected
 * (CMakeLists.txt drops the file, matching the #if around netmgr.c's
 * reference to @ref netmgr_probe_backend_mqtt); no #if inside this file
 * itself, since a guard here would silently build with the symbol missing
 * and no explanation. Includes are netmgr_probe.h plus tal_event.h and
 * tal_log.h - EVENT_MQTT_CONNECTED/DISCONNECTED sit BELOW netmgr in
 * tal_event_info.h, so subscribing adds no cloud include. Both callbacks
 * ignore the tuya_iot_client_t * payload entirely; dereferencing it would
 * need a cloud header and close the cycle netmgr_probe.h exists to avoid.
 *
 * Where a verdict really comes from, traced because the shape of this file
 * follows from it: EVENT_MQTT_CONNECTED publishes only after CONNACK and the
 * inbound topic subscribe - a byte came back, positive proof -> GOOD.
 * EVENT_MQTT_DISCONNECTED's only producer runs from exactly two callers: a
 * failed MQTT_ProcessLoop() (keepalive timeout or socket error - real
 * evidence) and a deliberate tuya_mqtt_stop() (no evidence at all) -> BAD,
 * evidence and never proof.
 *
 * Two consequences, both load-bearing. (1) A FAILED CONNECT PRODUCES
 * NOTHING: a failing mqtt_client_connect() never calls on_disconnected, so
 * the reconnect loop can spin against a dead WAN in complete silence - only
 * netmgr_policy_t.verify_timeout_ms can see it, which is why netmgr_probe.h
 * calls that the default mechanism rather than a fallback. (2) EVERY BAD IS
 * PRECEDED BY A GOOD on any route that still connects at all, so
 * consecutive BADs from THIS backend are not the normal case, and it alone
 * does not walk a live route's threshold down to DEGRADED.
 *
 * Both callbacks run on the application thread that loops tuya_iot_yield()
 * (also the keepalive pump) - see netmgr_probe_report()'s doc in
 * netmgr_probe.h for why that means recording and posting only, nothing
 * slower.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "netmgr_probe.h"
#include "tal_event.h"
#include "tal_log.h"

/***********************************************************
************************** state ***************************
***********************************************************/

/** tal_event matches (name, desc, cb) on subscribe and unsubscribe, so this
 *  must be the same string in all four calls below - hence one macro. */
#define PROBE_MQTT_SUBSCRIBER "netmgr_probe"

/**
 * The route epoch observed when the CURRENT MQTT session came up, or
 * NETMGR_PROBE_EPOCH_ANY when no session has been observed since start().
 *
 * Why this exists, when netmgr_probe.h says the passive backend "cannot"
 * hold an epoch and should pass EPOCH_ANY: unguarded attribution here is
 * not the header's rare race but a GUARANTEED one, once per switch -
 * netmgr's own handover tears MQTT down AFTER the epoch is already bumped,
 * so EPOCH_ANY would land on the link netmgr just switched TO every time. A
 * route change breaks the socket, so one MQTT session lives entirely within
 * one epoch: the epoch seen at CONNACK is the session's, and its end
 * belongs to it.
 *
 * Deliberately NOT consumed on a BAD: a session can be torn down twice (the
 * keepalive death, then the tuya_mqtt_stop() netmgr's own switch provokes),
 * and keeping the value until the next CONNECTED gives both the same
 * epoch, so the second - by then stale - is discarded instead of landing on
 * the new link.
 *
 * NOT solved: a tuya_mqtt_stop() unrelated to netmgr's handover (app
 * shutdown, a manual reconnect) fires with the route unchanged, so it
 * carries a LIVE epoch and lands as a genuine-looking BAD against a link
 * that is fine - nothing here can tell that apart from real evidence, which
 * is why probe_bad_threshold being greater than 1 matters even with this
 * guard.
 *
 * Written by both callbacks on the same thread; reset in start() and
 * stop(), where no callback can be running because
 * tal_event_unsubscribe() serialises against dispatch on the event mutex -
 * no lock or volatile needed.
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
 * The epoch is read HERE, not inside netmgr_probe_report(): this is the
 * moment of observation, and reading it later - or letting netmgr
 * substitute whatever is active when the work item runs - would attribute
 * the proof to a route that never carried it.
 *
 * @param[in] data the publisher's tuya_iot_client_t *, deliberately unused
 * @return OPRT_OK always - nothing here another subscriber needs to know.
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

    /* Return ignored: a reporter has no recovery for any outcome here either
     * way - it is stating a fact, not requesting an action. */
    (void)netmgr_probe_report(&result);

    PR_DEBUG("probe mqtt good, epoch %u", (unsigned int)epoch);
    return OPRT_OK;
}

/**
 * @brief EVENT_MQTT_DISCONNECTED: a session ended. Evidence, not proof.
 *
 * Carries the epoch of the session that ended, not a fresh reading, so a
 * teardown from netmgr's own handover is discarded as stale instead of
 * charged to the link just selected - see @ref s_session_epoch for why the
 * value is not cleared here. EPOCH_ANY when no CONNACK has been observed
 * since start(): a teardown of a session this backend never saw come up.
 *
 * @param[in] data the publisher's tuya_iot_client_t *, deliberately unused
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
 *        Called from the tail of netmgr_init() with s_netmgr.lock released,
 *        so blocking inside tal_event_subscribe() is allowed.
 *
 * The second subscribe's failure path cancels the first - not defensiveness
 * for its own sake: netmgr_init() logs a start() failure and CONTINUES, so a
 * half-installed backend would survive the whole init/deinit cycle, the
 * same leak netconn_wifi.c carried until M2.
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
 * @brief Stop observing and release what start() created: two list nodes
 *        inside tal_event. Idempotent, guarded with @ref s_subscribed rather
 *        than a bare pair of unsubscribes, since tal_event's tolerance of an
 *        unknown node is an implementation detail, not a header promise.
 *
 * The epoch is cleared only AFTER both unsubscribes return: dispatch and
 * tal_event_unsubscribe() share the event mutex, so once they return no
 * callback is running or can start - clearing earlier would race one still
 * in flight on the yield thread. netmgr calls this with s_netmgr.lock
 * released, so waiting on that mutex here cannot invert against a callback
 * inside netmgr_probe_report().
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
