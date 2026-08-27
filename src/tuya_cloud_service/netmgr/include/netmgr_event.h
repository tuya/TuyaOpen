/**
 * @file netmgr_event.h
 * @brief EVENT_NETMGR_CHG: one event that describes one complete netmgr
 *        change, with a reason - unlike EVENT_LINK_TYPE_CHG and
 *        EVENT_LINK_STATUS_CHG, which publish independently from three
 *        branches of one if-chain and so give a subscriber neither WHY, nor
 *        one fact for a simultaneous change, nor the value BEFORE it.
 *
 * Not a migration: the two legacy events keep their name, payload type,
 * pointer-to-a-stack-local convention and publish conditions unchanged.
 * EVENT_NETMGR_CHG publishes IN ADDITION and AFTER them, in this fixed
 * order, so a subscriber that re-enters netmgr synchronously - tuya_iot's
 * does - still does so exactly as today:
 *
 *   1. EVENT_LINK_TYPE_CHG    if the active link changed
 *   2. EVENT_LINK_STATUS_CHG  if the public status changed
 *   3. EVENT_NETMGR_CHG       if either changed, or if the reason is worth
 *                             reporting on its own (see @ref netmgr_change_t)
 *
 * All three publish with s_netmgr.lock RELEASED, from locals - see netmgr.c's
 * locking contract: subscribers re-enter netmgr and the mutex is not
 * portably recursive.
 *
 * All three payloads are POINTERS to a value, not the value itself, valid
 * only for the callback's duration - see @ref netmgr_change_t for the full
 * lifetime contract, and docs/netmgr/release_notes.md §2.3 for two real
 * subscribers that miscast the pointer and silently never saw the network
 * come up.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __NETMGR_EVENT_H__
#define __NETMGR_EVENT_H__

#include "tuya_cloud_types.h"
#include "netmgr.h"
#include "netmgr_policy.h"

/* For EVENT_LINK_TYPE_CHG / EVENT_LINK_STATUS_CHG, and for EVENT_NETMGR_CHG
 * itself - every event name in this tree is meant to end up centralised
 * there, where the CLI and the apps grep for it. */
#include "tal_event_info.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The new event's name. Guarded so this header is self-sufficient
 *        today and defers once the central definition lands in
 *        tal_event_info.h - do not remove the guard when adding that.
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
 * One value per distinguishable cause, so a log line or cloud-side
 * diagnostic can name the cause without guessing. When several causes
 * coincide in one pass, the reason reported is the one that changed the
 * OUTCOME - netmgr coalesces reports, so a pass routinely has more than one
 * input and only one of them decided anything.
 */
typedef enum {
    /** Placeholder for a zeroed struct. Never published. */
    NETMGR_CHG_REASON_NONE = 0,

    /**
     * netmgr_init() completed and seeded the state. Published even when
     * nothing "changed" so a subscriber that starts later has a way to learn
     * the initial value other than polling.
     */
    NETMGR_CHG_REASON_INIT = 1,

    /** A driver reported its link up, and that link became eligible. */
    NETMGR_CHG_REASON_LINK_UP = 2,

    /** A driver reported its link down. */
    NETMGR_CHG_REASON_LINK_DOWN = 3,

    /**
     * A probe verdict moved a link to NETMGR_LINK_STATE_ONLINE. Worth its own
     * value because it is the transition that ENDS an outage, and a trace
     * needs to show it as clearly as the one that starts it.
     */
    NETMGR_CHG_REASON_PROBE_GOOD = 4,

    /**
     * probe_bad_threshold consecutive BAD verdicts moved a link to
     * NETMGR_LINK_STATE_DEGRADED - what a wifi+4G device reports when it
     * gives up on a WAN-less AP.
     */
    NETMGR_CHG_REASON_PROBE_BAD = 5,

    /**
     * netmgr_policy_t.verify_timeout_ms elapsed with no verdict at all.
     * Distinguished from PROBE_BAD on purpose: BAD means something tried and
     * failed, TIMEOUT means nothing ever tried, and the remedies differ.
     */
    NETMGR_CHG_REASON_PROBE_TIMEOUT = 6,

    /** A revalidation deadline expired and a DEGRADED link is being retried. */
    NETMGR_CHG_REASON_REVALIDATE = 7,

    /**
     * conn->pri changed under netmgr_conn_set(NETCONN_CMD_PRI) and the new
     * order changed the winner.
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
     * The active link kept its identity but its address changed - a DHCP
     * renew, a cellular redial, or a NETCONN_CMD_IP set.
     *
     * Published although neither the active type nor the public status
     * changed, because the ROUTE changed: __netmgr_push_route() installed a
     * new src_ip and every socket opened from here on binds to it - a case
     * the legacy if-chain takes no branch for and publishes nothing for.
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
 * LIFETIME. A pointer to a stack local in the publishing function, valid
 * only for the synchronous callback's duration - never NULL, but a
 * subscriber that needs the value later must COPY it.
 *
 * CONTEXT. Published from WORKQ_SYSTEM except for NETMGR_CHG_REASON_INIT and
 * NETMGR_CHG_REASON_DEINIT, from whatever thread called
 * netmgr_init()/netmgr_deinit(). A subscriber must not call netmgr_deinit()
 * from here - forbidden from WORKQ_SYSTEM by netmgr.h's contract, and a
 * subscriber generally cannot know it isn't on that thread.
 */
typedef struct {
    /** Why. See @ref netmgr_change_reason_e. */
    netmgr_change_reason_e reason;

    /**
     * The link the change is ABOUT, which is not always the new active link -
     * e.g. the link demoted for PROBE_BAD, or the link that dropped for
     * LINK_DOWN. NETCONN_AUTO when not attributable to one link (POLICY,
     * UNPINNED).
     */
    netmgr_type_e subject;

    /** Active link before this change; NETCONN_AUTO when there was none. */
    netmgr_type_e old_active;
    /** Active link after; NETCONN_AUTO when there is none. */
    netmgr_type_e new_active;

    /**
     * Public status before and after - the same two values
     * EVENT_LINK_STATUS_CHG carries, so a subscriber can move to this event
     * without changing how it interprets them.
     */
    netmgr_status_e old_status;
    netmgr_status_e new_status;

    /**
     * Internal state of @a new_active, the detail the public status cannot
     * express - tells NETMGR_LINK_STATE_ONLINE from NETMGR_LINK_STATE_DEGRADED
     * when new_status is NETMGR_LINK_UP for both. NETMGR_LINK_STATE_DOWN when
     * new_active is NETCONN_AUTO.
     */
    netmgr_link_state_e new_state;

    /**
     * Route generation after this change; see netmgr_probe_epoch_get(). Lets
     * a subscriber correlate this event with a verdict it is about to
     * report, and tell "the route moved" from "re-announced unchanged".
     */
    uint32_t epoch;

    /**
     * Source address now installed on the data plane, in network byte order,
     * or 0 for none - the value __netmgr_push_route() just wrote, so a
     * subscriber sees the address its next outbound socket will bind to
     * without a second call into netmgr. It cannot make one anyway: it runs
     * synchronously inside the publish, and netmgr_conn_get() would take
     * s_netmgr.lock, which the publisher may be about to retake.
     */
    uint32_t src_ip;

    /**
     * TRUE when this is a handover: both old_active and new_active were up,
     * they differ, and there was no down state between them - the same
     * condition NETMGR_LINK_UP_SWITCH (netmgr.h) represents in
     * EVENT_LINK_STATUS_CHG (see netmgr_policy_t.emit_up_switch), but
     * reported here unconditionally. Prefer this field over enabling that
     * flag to reach a consumer: at least one waiting consumer
     * (examples/multimedia/audio_player/music/src/tuya_app_main.c) casts the
     * EVENT_LINK_STATUS_CHG payload POINTER straight to the enum instead of
     * dereferencing it, so enabling the flag would fix nothing there anyway
     * - see docs/netmgr/release_notes.md §2.3.
     */
    BOOL_T handover;
} netmgr_change_t;

#ifdef __cplusplus
}
#endif

#endif /* __NETMGR_EVENT_H__ */
