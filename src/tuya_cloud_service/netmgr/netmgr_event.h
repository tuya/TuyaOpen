/**
 * @file netmgr_event.h
 * @brief One event that describes one complete change, with a reason.
 *
 * What is wrong with the two events we have
 * -----------------------------------------
 * __netmgr_reselect() publishes EVENT_LINK_TYPE_CHG with a netmgr_type_e and
 * EVENT_LINK_STATUS_CHG with a netmgr_status_e, independently, from three
 * branches of one if-chain. A subscriber therefore cannot answer any of the
 * questions it actually has:
 *
 *   - WHY did this happen? A link came up, a link went down, a priority changed,
 *     a probe failed, an operator pinned a link - all four arrive as the same
 *     two bytes. This is the gap that matters most in the field, because
 *     "the device switched to 4G" and "the device switched to 4G because wifi
 *     stopped reaching the cloud" are different tickets;
 *   - WHAT changed, as one fact? A switch from wifi to wired publishes
 *     EVENT_LINK_TYPE_CHG only, because s_netmgr.status stayed NETMGR_LINK_UP
 *     and status_chg stayed FALSE. A simultaneous change publishes two events,
 *     and a subscriber that reacts to the first sees state that the second is
 *     about to correct;
 *   - what was it BEFORE? Neither payload carries the old value, so a subscriber
 *     that needs the transition rather than the level has to keep its own shadow
 *     copy - and every one of the seven EVENT_LINK_STATUS_CHG subscribers in this
 *     tree that needs one, has one.
 *
 * The two legacy events keep firing, unchanged
 * -------------------------------------------
 * This is not a migration and there is no deprecation. EVENT_LINK_TYPE_CHG has
 * one subscriber (tuya_iot.c:929, whose handler calls tuya_iot_reconnect()) and
 * EVENT_LINK_STATUS_CHG has seven across apps and examples. Both keep the same
 * name, the same payload type, the same pointer-to-a-stack-local convention and
 * the same publish conditions. EVENT_NETMGR_CHG is published IN ADDITION, and
 * after them, so that:
 *
 *   - a subscriber that re-enters netmgr synchronously - which tuya_iot's does -
 *     does so in exactly the order it does today;
 *   - a new subscriber that wants the whole picture can ignore the legacy pair
 *     entirely rather than correlating three events.
 *
 * Publish order, fixed and part of the contract:
 *
 *   1. EVENT_LINK_TYPE_CHG    if the active link changed
 *   2. EVENT_LINK_STATUS_CHG  if the public status changed
 *   3. EVENT_NETMGR_CHG       if either changed, or if the reason is worth
 *                             reporting on its own (see @ref netmgr_change_t)
 *
 * All three are published with s_netmgr.lock RELEASED, from locals, for the
 * reason the locking contract in netmgr.c states: subscribers re-enter netmgr and
 * the mutex is not portably recursive.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __NETMGR_EVENT_H__
#define __NETMGR_EVENT_H__

#include "tuya_cloud_types.h"
#include "netmgr.h"
#include "netmgr_policy.h"

/* For EVENT_LINK_TYPE_CHG / EVENT_LINK_STATUS_CHG, and for EVENT_NETMGR_CHG
 * itself. Every event name in this tree is centralised there and the CLI and the
 * apps grep it, so the new one is added there too rather than being defined here
 * where nobody would look for it. That is a one-line addition to
 * src/tal_system/include/tal_event_info.h:
 *
 *     #define EVENT_NETMGR_CHG "netmgr.chg" // netmgr active link or status change
 *
 * "netmgr.chg" is 10 characters, inside the EVENT_NAME_MAX_LEN of 16 that
 * tal_event.h imposes. */
#include "tal_event_info.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The new event's name.
 *
 * Guarded so that this header is self-sufficient TODAY and defers automatically
 * once the line above lands in tal_event_info.h - the implementation adds it
 * there and this definition then evaporates, with no second edit here and no
 * window in which the two disagree. Do not remove the guard when adding the
 * central definition: it is what makes the two orderings equivalent, so the
 * netmgr change and the tal_system change can be reviewed and merged
 * independently.
 */
#ifndef EVENT_NETMGR_CHG
#define EVENT_NETMGR_CHG "netmgr.chg" // netmgr active link or status change
#endif

/***********************************************************
************************* reasons **************************
***********************************************************/

/**
 * @brief Why netmgr re-evaluated, and what tipped the decision.
 *
 * One value per distinguishable cause, because the point of the field is that a
 * log line or a cloud-side diagnostic can name the cause without guessing. When
 * several causes coincide in one pass the reason reported is the one that changed
 * the OUTCOME - netmgr coalesces reports, so a pass routinely has more than one
 * input and only one of them decided anything.
 */
typedef enum {
    /** Placeholder for a zeroed struct. Never published. */
    NETMGR_CHG_REASON_NONE = 0,

    /**
     * netmgr_init() completed and seeded the state. Published even when nothing
     * "changed", because there was no prior state to change from and a
     * subscriber that starts later otherwise has no way to learn the initial
     * value except by polling.
     */
    NETMGR_CHG_REASON_INIT = 1,

    /** A driver reported its link up, and that link became eligible. */
    NETMGR_CHG_REASON_LINK_UP = 2,

    /** A driver reported its link down. */
    NETMGR_CHG_REASON_LINK_DOWN = 3,

    /**
     * A probe verdict moved a link to NETMGR_LINK_STATE_ONLINE. Worth its own
     * value because it is the transition that ENDS an outage, and an operator
     * looking at a trace needs to see it as clearly as the one that starts it.
     */
    NETMGR_CHG_REASON_PROBE_GOOD = 4,

    /**
     * probe_bad_threshold consecutive BAD verdicts moved a link to
     * NETMGR_LINK_STATE_DEGRADED. THE reason M3 exists: this is what a wifi+4G
     * device reports when it gives up on a WAN-less AP.
     */
    NETMGR_CHG_REASON_PROBE_BAD = 5,

    /**
     * netmgr_policy_t.verify_timeout_ms elapsed with no verdict at all.
     * Distinguished from PROBE_BAD on purpose: BAD means something tried and
     * failed, TIMEOUT means nothing ever tried, and the remedies differ - the
     * second usually means the device never got as far as an activation attempt.
     */
    NETMGR_CHG_REASON_PROBE_TIMEOUT = 6,

    /** A revalidation deadline expired and a DEGRADED link is being retried. */
    NETMGR_CHG_REASON_REVALIDATE = 7,

    /**
     * conn->pri changed under netmgr_conn_set(NETCONN_CMD_PRI) and the new order
     * changed the winner.
     *
     * This reason was unreachable before M3 in the strongest sense: the drivers
     * fired base.event_cb() on a priority set, so a pass ran, but selection
     * walked a list that had been sorted once at registration and never re-sorted,
     * so the outcome could not change. netconn_wifi_set()'s comment - "set pri
     * will cause status change to reneg the active connection" - described this
     * reason before there was any code that could produce it.
     */
    NETMGR_CHG_REASON_PRI_CHANGED = 8,

    /** netmgr_policy_pin() armed a pin and it took effect. */
    NETMGR_CHG_REASON_PINNED = 9,

    /** netmgr_policy_pin(NETCONN_AUTO) released a pin. */
    NETMGR_CHG_REASON_UNPINNED = 10,

    /** up_debounce_ms elapsed and a link became eligible. */
    NETMGR_CHG_REASON_DEBOUNCE = 11,

    /** down_grace_ms elapsed and the active link was finally released. */
    NETMGR_CHG_REASON_GRACE = 12,

    /** min_dwell_ms elapsed and a deferred switch went ahead. */
    NETMGR_CHG_REASON_DWELL = 13,

    /** netmgr_policy_set() installed a policy whose ranking picks differently. */
    NETMGR_CHG_REASON_POLICY = 14,

    /**
     * The active link kept its identity but its address changed - a DHCP renew, a
     * cellular redial, or a NETCONN_CMD_IP set.
     *
     * Published although neither the active type nor the public status changed,
     * because the ROUTE changed: __netmgr_push_route() installed a new src_ip and
     * every socket opened from here on binds to it. A subscriber holding a long
     * lived socket needs to know, and today nothing tells it - this case takes no
     * branch of the if-chain in __netmgr_reselect() and therefore publishes
     * nothing at all.
     */
    NETMGR_CHG_REASON_ADDR_CHANGED = 15,

    /** netmgr_deinit() tore everything down. Published before the links close. */
    NETMGR_CHG_REASON_DEINIT = 16,
} netmgr_change_reason_e;

#define NETMGR_CHG_REASON_TO_STR(r)                                                                                    \
    ((r) == NETMGR_CHG_REASON_INIT            ? "init"                                                                 \
     : (r) == NETMGR_CHG_REASON_LINK_UP       ? "link_up"                                                              \
     : (r) == NETMGR_CHG_REASON_LINK_DOWN     ? "link_down"                                                            \
     : (r) == NETMGR_CHG_REASON_PROBE_GOOD    ? "probe_good"                                                           \
     : (r) == NETMGR_CHG_REASON_PROBE_BAD     ? "probe_bad"                                                            \
     : (r) == NETMGR_CHG_REASON_PROBE_TIMEOUT ? "probe_timeout"                                                        \
     : (r) == NETMGR_CHG_REASON_REVALIDATE    ? "revalidate"                                                           \
     : (r) == NETMGR_CHG_REASON_PRI_CHANGED   ? "pri_changed"                                                          \
     : (r) == NETMGR_CHG_REASON_PINNED        ? "pinned"                                                               \
     : (r) == NETMGR_CHG_REASON_UNPINNED      ? "unpinned"                                                             \
     : (r) == NETMGR_CHG_REASON_DEBOUNCE      ? "debounce"                                                             \
     : (r) == NETMGR_CHG_REASON_GRACE         ? "grace"                                                                \
     : (r) == NETMGR_CHG_REASON_DWELL         ? "dwell"                                                                \
     : (r) == NETMGR_CHG_REASON_POLICY        ? "policy"                                                               \
     : (r) == NETMGR_CHG_REASON_ADDR_CHANGED  ? "addr_changed"                                                         \
     : (r) == NETMGR_CHG_REASON_DEINIT        ? "deinit"                                                               \
                                              : "none")

/***********************************************************
************************* payload **************************
***********************************************************/

/**
 * @brief The payload of EVENT_NETMGR_CHG: one complete change.
 *
 * LIFETIME. Like both legacy events, this is published as a pointer to a stack
 * local in the publishing function, and tal_event_publish() dispatches
 * synchronously on the publisher's thread. So the pointer is valid for the
 * duration of the callback and not one instruction longer. A subscriber that
 * needs the value later must COPY it. This is spelled out because none of the
 * seven existing EVENT_LINK_STATUS_CHG subscribers copies its payload, and they
 * are correct only by accident of the synchronous dispatch.
 *
 * NULLABILITY. The pointer is never NULL. Subscribers should still check, and
 * exactly one in the tree does - tuya_iot.c's __tuya_iot_link_type_change_cb().
 *
 * CONTEXT. Published from the WORKQ_SYSTEM thread in every case except the
 * NETMGR_CHG_REASON_INIT and NETMGR_CHG_REASON_DEINIT publishes, which come from
 * whatever thread called netmgr_init() or netmgr_deinit(). A subscriber must not
 * call netmgr_deinit() - the contract in netmgr.h forbids calling it from
 * WORKQ_SYSTEM, and a subscriber generally cannot know it is not on that thread.
 */
typedef struct {
    /** Why. See @ref netmgr_change_reason_e. */
    netmgr_change_reason_e reason;

    /**
     * The link the change is ABOUT, which is not always the new active link. For
     * NETMGR_CHG_REASON_PROBE_BAD it is the link that was demoted; for
     * NETMGR_CHG_REASON_LINK_DOWN the link that dropped. NETCONN_AUTO when the
     * change is not attributable to one link (POLICY, UNPINNED).
     */
    netmgr_type_e subject;

    /** Active link before this change; NETCONN_AUTO when there was none. */
    netmgr_type_e old_active;
    /** Active link after; NETCONN_AUTO when there is none. */
    netmgr_type_e new_active;

    /**
     * Public status before and after - the same two values EVENT_LINK_STATUS_CHG
     * carries, so a subscriber can move to this event without changing how it
     * interprets them.
     */
    netmgr_status_e old_status;
    netmgr_status_e new_status;

    /**
     * Internal state of @a new_active, which is the detail the public status
     * cannot express. This is where a subscriber learns the difference between
     * "up" and "up and proven", i.e. tells NETMGR_LINK_STATE_ONLINE from
     * NETMGR_LINK_STATE_DEGRADED when new_status is NETMGR_LINK_UP for both.
     *
     * NETMGR_LINK_STATE_DOWN when new_active is NETCONN_AUTO.
     */
    netmgr_link_state_e new_state;

    /**
     * Route generation after this change; see netmgr_probe_epoch_get(). Lets a
     * subscriber correlate this event with a verdict it is about to report, and
     * lets it tell "the route moved" from "the same route was re-announced".
     */
    uint32_t epoch;

    /**
     * Source address now installed on the data plane, in network byte order, or 0
     * for none. The value __netmgr_push_route() just wrote, so a subscriber sees
     * the same address its next outbound socket will bind to without a second
     * call into netmgr - which matters because it cannot make one: it is running
     * synchronously inside the publish, and netmgr_conn_get() would take
     * s_netmgr.lock the publisher has already released but may be about to retake.
     */
    uint32_t src_ip;

    /**
     * TRUE when this is a handover: both old_active and new_active were up, they
     * differ, and there was no down state between them.
     *
     * The precise condition under which NETMGR_LINK_UP_SWITH is published in
     * EVENT_LINK_STATUS_CHG when netmgr_policy_t.emit_up_switch is set. It is
     * reported here unconditionally, whatever that flag says, so a subscriber can
     * detect a handover WITHOUT the legacy event having to change - which is the
     * recommended way to consume it, given the compat problem that flag documents.
     */
    BOOL_T handover;
} netmgr_change_t;

/***********************************************************
*************** notes on NETMGR_LINK_UP_SWITH **************
***********************************************************/

/* Implementing the third enumerator, and the consumer that has been waiting for
 * it
 * ===========================================================================
 * NETMGR_LINK_UP_SWITH has existed in netmgr.h since the type was written and has
 * never been produced by any code path. s_netmgr.status is only ever assigned
 * from __get_netmgr_status(), which copies a driver's base.status, and no driver
 * has any arm that writes anything but NETMGR_LINK_UP or NETMGR_LINK_DOWN.
 *
 * M3 makes it reachable. The condition is @ref netmgr_change_t.handover:
 *
 *     old_status is up AND new_status is up AND old_active != new_active
 *
 * i.e. a handover with no intervening down, which is exactly what the
 * enumerator's own comment says - "network was connected but connection
 * changed".
 *
 * It is gated behind netmgr_policy_t.emit_up_switch, default FALSE, and that
 * header explains why at length: six sites in the tree compare
 * `status == NETMGR_LINK_UP` and would read UP_SWITH as offline, one of them
 * (tuya_svc_netmgr.c:37) mapping it directly to NETWORK_STATUS_OFFLINE for the
 * AI client. Publishing it by default would take the AI apps offline on every
 * handover.
 *
 * The one consumer that asked for it cannot use it yet, and this is worth
 * stating in the header rather than in a commit message, because it will
 * otherwise be read as an M3 failure. In
 * examples/multimedia/audio_player/music/src/tuya_app_main.c:97-100:
 *
 *     OPERATE_RET __link_status_cb(void *data)
 *     {
 *         netmgr_status_e status = (netmgr_status_e)data;
 *         if (NETMGR_LINK_UP == status || NETMGR_LINK_UP_SWITH == status) {
 *
 * That casts the POINTER to the enum instead of dereferencing it. netmgr
 * publishes &pub_status, so `status` holds a stack address, which is never 0, 1
 * or 2 - so is_network_connected is unconditionally false and always has been,
 * for every status, not only for UP_SWITH. Enabling emit_up_switch does nothing
 * for this consumer. The one-line fix is
 *
 *     netmgr_status_e status = *(netmgr_status_e *)data;
 *
 * and it belongs to whoever owns that example; it is listed in the M3 design as a
 * separate, trivial, independently reviewable change. The same miscast appears
 * once more, partially, at
 * examples/protocols/mqtt_client/src/examples_mqtt_client.c:155, where the
 * PR_DEBUG logs the pointer and the very next line dereferences correctly.
 */

#ifdef __cplusplus
}
#endif

#endif /* __NETMGR_EVENT_H__ */
