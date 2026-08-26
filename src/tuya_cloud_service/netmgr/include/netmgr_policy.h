/**
 * @file netmgr_policy.h
 * @brief Which link carries the traffic, and why - the decision netmgr currently
 *        makes by walking a linked list.
 *
 * Provides the policy input/output contract for link selection: a tunable
 * netmgr_policy_t, a flat snapshot a ranking function can consume without
 * touching netmgr's live state, and a hook to replace the built-in ranking.
 * netmgr's linked list stays in registration order; ranking happens here, from
 * a snapshot, on every decision - which is what lets a runtime NETCONN_CMD_PRI
 * change actually take effect, since there is no cached order left to disagree
 * with it.
 *
 * This module decides; netmgr.c owns everything else - driver reports, all
 * debounce/grace timing, probe accumulation, retry deadlines, the route push,
 * events, the LAN gate. See docs/netmgr/extension_guide.md §1 for the full
 * division of labour. Holding no timer, lock or driver access is why
 * netmgr_policy_select() is the one function here allowed to run with
 * s_netmgr.lock held.
 *
 * Every deadline in the module - debounce, grace, dwell, probe timeout,
 * revalidation - is served by one shared tal_sw_timer in netmgr.c rather than
 * one per link; netmgr_select_out_t.recheck_ms is how a ranking function
 * contributes its own deadline to that fold without a timer of its own.
 *
 * probe_enable, probe_demote and min_dwell_ms default to off/zero unless a
 * board selects ENABLE_NETMGR_PROBE, NETMGR_PROBE_DEMOTE or
 * NETMGR_POLICY_MIN_DWELL_MS in src/tuya_cloud_service/Kconfig - see
 * NETMGR_POLICY_DEFAULT_INIT below. See docs/netmgr/release_notes.md §1 for
 * the neutrality argument this default rests on.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __NETMGR_POLICY_H__
#define __NETMGR_POLICY_H__

#include "tuya_cloud_types.h"
#include "netmgr.h"
#include "netconn_registry.h"
#include "netmgr_retry.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
********************* link state machine *******************
***********************************************************/

/**
 * @brief What netmgr knows about one link, layered on top of what the driver
 *        reports. conn->get(NETCONN_CMD_STATUS) is unchanged - it still answers
 *        only NETMGR_LINK_UP/DOWN out of base.status; everything richer here is
 *        netmgr-side, derived from that input plus probe verdicts plus
 *        deadlines, since a driver has no way to know whether its link reaches
 *        the cloud and no business knowing policy timings either.
 *
 * DOWN is 0, so a zeroed slot is DOWN - same convention netmgr_conn_base_t.status
 * already follows.
 */
typedef enum {
    /** No association, no address, nothing pending. */
    NETMGR_LINK_STATE_DOWN = 0,

    /** An attempt is in flight. Only reachable on NETCONN_CTRL_MANAGED links
     *  (see netconn_registry.h). Maps to NETMGR_LINK_DOWN publicly. */
    NETMGR_LINK_STATE_CONNECTING = 1,

    /**
     * L3 is up (association + address), reachability UNKNOWN. Entered the
     * moment a driver reports NETMGR_LINK_UP; with probing disabled it is the
     * only up-state that ever exists. Ranks alongside ONLINE, not below it -
     * a passive probe can only observe the ACTIVE link, so ranking this lower
     * would deadlock: it could never be selected to earn a verdict.
     */
    NETMGR_LINK_STATE_UNVERIFIED = 2,

    /** L3 is up and something reached the cloud through it. See netmgr_probe.h. */
    NETMGR_LINK_STATE_ONLINE = 3,

    /**
     * L3 is up but netmgr has evidence traffic does NOT get through
     * (probe_bad_threshold consecutive BAD verdicts, or verify_timeout_ms with
     * no verdict). Still maps to NETMGR_LINK_UP publicly - it is a RANKING
     * signal, not a liveness one: reporting DOWN would stop the retry that
     * could clear it and would flap every is_connected consumer in the tree.
     */
    NETMGR_LINK_STATE_DEGRADED = 4,

    /** Down, waiting out a back-off before the next attempt or revalidation -
     *  distinct from DOWN because a deadline is pending. Maps to
     *  NETMGR_LINK_DOWN publicly. */
    NETMGR_LINK_STATE_BACKOFF = 5,
} netmgr_link_state_e;

#define NETMGR_LINK_STATE_TO_STR(s)                                                                                    \
    ((s) == NETMGR_LINK_STATE_DOWN         ? "down"                                                                    \
     : (s) == NETMGR_LINK_STATE_CONNECTING ? "connecting"                                                              \
     : (s) == NETMGR_LINK_STATE_UNVERIFIED ? "unverified"                                                              \
     : (s) == NETMGR_LINK_STATE_ONLINE     ? "online"                                                                  \
     : (s) == NETMGR_LINK_STATE_DEGRADED   ? "degraded"                                                                \
     : (s) == NETMGR_LINK_STATE_BACKOFF    ? "backoff"                                                                 \
                                           : "unknown")

/**
 * @brief Can this state carry traffic? The eligibility floor, and the single
 *        definition of it: a link that passes may be selected, a link that
 *        does not may not, whatever its priority and whatever the pin says.
 *        DEGRADED passes - see its note.
 */
#define NETMGR_LINK_STATE_IS_UP(s)                                                                                     \
    ((s) == NETMGR_LINK_STATE_UNVERIFIED || (s) == NETMGR_LINK_STATE_ONLINE || (s) == NETMGR_LINK_STATE_DEGRADED)

/**
 * @brief Is netmgr's evidence against this link, for ranking purposes?
 *
 * Exactly one state, but named rather than compared inline, because the whole
 * neutrality argument rests on there being exactly one and on it never being
 * reachable with probing off.
 */
#define NETMGR_LINK_STATE_IS_SUSPECT(s) ((s) == NETMGR_LINK_STATE_DEGRADED)

/**
 * @brief The internal state, collapsed onto the public netmgr_status_e.
 *
 *   DOWN, CONNECTING, BACKOFF     -> NETMGR_LINK_DOWN
 *   UNVERIFIED, ONLINE, DEGRADED  -> NETMGR_LINK_UP
 *
 * conn->get(NETCONN_CMD_STATUS) is unaffected - it is answered by the driver
 * from base.status and never consults this enum. netmgr_conn_get(NETCONN_AUTO,
 * NETCONN_CMD_STATUS) can now resolve to a different link than before, but its
 * range is still just these two values. NETMGR_LINK_UP_SWITH is a property of a
 * TRANSITION, not a link state, so it is produced from netmgr_event.h's terms
 * and never appears here. With probing off, DEGRADED is unreachable, so the
 * internal machine has exactly UNVERIFIED and ONLINE as up-states, both mapping
 * to UP - bit-identical to pre-M3 behaviour.
 */
#define NETMGR_LINK_STATE_TO_STATUS(s) (NETMGR_LINK_STATE_IS_UP(s) ? NETMGR_LINK_UP : NETMGR_LINK_DOWN)

/***********************************************************
************************ parameters ************************
***********************************************************/

/**
 * @brief Tunable selection behaviour.
 *
 * One struct rather than a command per knob, so a product sets its whole policy
 * in one atomic call rather than racing a reselect between two related writes.
 *
 * Deliberately NOT reachable through netmgr_conn_set(): a policy is a property of
 * the DEVICE, not of a link, and every NETCONN_CMD_* is addressed to one link.
 */
typedef struct {
    /**
     * A link must report up continuously for this long before it becomes
     * eligible, to suppress a flapping link taking the route for a moment.
     *
     * Unit: ms. DEFAULT 0 - eligible on the first report.
     */
    uint32_t up_debounce_ms;

    /**
     * When the ACTIVE link reports down, hold it active - and hold its route -
     * for this long before reselecting, so a brief drop does not tear down the
     * MQTT session. Applies only to the active link.
     *
     * Consumed entirely by netmgr.c by holding the link's netmgr_link_state_e
     * up, so unlike up_debounce_ms it has no visible field in
     * netmgr_link_view_t or netmgr_policy_select_default().
     *
     * Unit: ms. DEFAULT 0 - reselect immediately.
     */
    uint32_t down_grace_ms;

    /**
     * After a switch, do not switch again for this long. Never traps the device
     * on a dead link: dwell is only consulted while the active link is still
     * eligible - the moment it stops being eligible, dwell is abandoned rather
     * than leaving the device with no network to honour a hysteresis parameter.
     *
     * Unit: ms. DEFAULT NETMGR_POLICY_DEFAULT_MIN_DWELL_MS, which is 0 (no
     * dwell) unless the board set NETMGR_POLICY_MIN_DWELL_MS in Kconfig; that
     * symbol only appears under ENABLE_NETMGR_PROBE, since demotion is what
     * starts the oscillation below.
     *
     * A MULTI-LINK PRODUCT SHOULD SET THIS. A wifi+4G board whose wifi AP has
     * lost its WAN will otherwise oscillate at the default of 0:
     *
     *   1. wifi is demoted to DEGRADED (verify_timeout_ms elapses with no
     *      verdict) and the route moves to cellular;
     *   2. @ref revalidate promotes wifi back to NETMGR_LINK_STATE_UNVERIFIED
     *      after 30 s - a passive probe can only judge the ACTIVE link, so
     *      re-trying wifi is the only way to learn it recovered;
     *   3. UNVERIFIED shares the not-suspect tier with ONLINE (see
     *      NETMGR_LINK_STATE_UNVERIFIED for why separating them would
     *      deadlock), so wifi's higher priority wins and the route moves back;
     *   4. verify_timeout_ms elapses again and it returns to step 1.
     *
     * Each swing tears down and re-establishes the MQTT session. The
     * revalidation table (30, 60, 120, 300, 600 s, last entry repeating) damps
     * the period to roughly one cycle every twelve minutes, but min_dwell_ms is
     * what damps the early, fast cycles: it holds a switch until the dwell
     * expires without ever preventing one. A board that would rather not
     * re-verify at all can set @ref revalidate to a non-NULL table with count
     * 0, at the cost of never discovering that wifi came back.
     *
     * This warning is duplicated, in full, in NETMGR_POLICY_MIN_DWELL_MS's
     * Kconfig help, since the person who needs it is flipping
     * ENABLE_NETMGR_PROBE in menuconfig and is not reading this header. If the
     * two ever disagree, this one is the specification.
     */
    uint32_t min_dwell_ms;

    /**
     * TRUE: a higher-ranked link takes over as soon as it becomes eligible.
     * FALSE: sticky - once a link is active it keeps the route while it stays
     * eligible, and a reselect only happens when it stops being eligible.
     *
     * DEFAULT TRUE.
     */
    BOOL_T preempt;

    /**
     * Master switch for netmgr_probe.h. FALSE: verdicts are dropped,
     * verify_timeout_ms is never armed, and NETMGR_LINK_STATE_DEGRADED is
     * unreachable. DEFAULT FALSE, from Kconfig ENABLE_NETMGR_PROBE.
     *
     * Safe to turn ON: the default (passive) backend emits no packets, so
     * there's no traffic/power/billing cost. A BAD verdict only affects
     * ranking when another link exists to prefer, so a single-link board is
     * provably unaffected. On a multi-link board the changed ranking is the
     * intended fix - e.g. a wifi+4G device on a WAN-less AP moves to cellular
     * instead of pinning to wifi forever.
     */
    BOOL_T probe_enable;

    /**
     * TRUE: a DEGRADED link ranks below every non-suspect eligible link.
     * FALSE: verdicts still accumulate and are visible to the CLI and to
     * netmgr_probe_stat_get(), but do not affect ranking.
     *
     * DEFAULT FALSE; TRUE only when ENABLE_NETMGR_PROBE and NETMGR_PROBE_DEMOTE
     * are BOTH selected - cannot be TRUE while probe_enable is FALSE
     * (NETMGR_POLICY_DEFAULT_INIT enforces this by nesting the two tests).
     * Split from probe_enable so a product can deploy observability before the
     * behaviour change.
     */
    BOOL_T probe_demote;

    /**
     * TRUE: on entering DEGRADED, a MANAGED link is also dropped and re-dialled.
     *
     * DEFAULT FALSE, and must stay false by default: on a single-link wifi
     * product, a cloud-side outage correctly reports BAD, but re-dialling a
     * perfectly healthy association would turn an invisible cloud problem into
     * a visible device one. Re-association fixes a broken ASSOCIATION;
     * DEGRADED is not evidence of one.
     */
    BOOL_T probe_reconnect;

    /**
     * Consecutive NETMGR_PROBE_BAD verdicts needed to move a link to DEGRADED.
     * Any NETMGR_PROBE_GOOD resets the count.
     *
     * DEFAULT 3. Must be greater than 1: a DELIBERATE tuya_mqtt_stop() (e.g. on
     * switch or teardown) fires EVENT_MQTT_DISCONNECTED exactly as a keepalive
     * timeout does, and the two cannot be told apart at that layer, so a
     * threshold of 1 would demote a perfectly healthy link on a routine
     * teardown. Three, against the MQTT layer's own reconnect back-off, is on
     * the order of half a minute of sustained failure before it fires.
     *
     * ENFORCED, not merely requested: any value below
     * NETMGR_PROBE_BAD_THRESHOLD_MIN is raised to it where the count is
     * compared - covering both an unset 0 and a product that explicitly writes
     * 1. netmgr_policy_get() still returns whatever was stored, unrewritten;
     * the floor is applied only at the comparison.
     *
     * WHICH BACKEND THIS SERVES: an ACTIVE backend, one that can emit several
     * BADs in a row with no GOOD in between. With the default PASSIVE backend
     * every BAD is followed by a session-establishment GOOD that zeroes the
     * count, so the stream rarely if ever reaches 2 - that is not a bug to go
     * looking for. Passive demotion instead goes through @ref
     * verify_timeout_ms, on the path ONLINE --(BAD)--> UNVERIFIED
     * --(timeout)--> DEGRADED; it is which mechanism covers which backend, not
     * a gap.
     */
    uint8_t probe_bad_threshold;

    /**
     * How long a link may stay ACTIVE and UNVERIFIED before netmgr synthesises
     * a BAD of its own (source NETMGR_PROBE_SRC_TIMEOUT). Only runs for the
     * active link - a passive backend can only observe that one; a standby
     * link stays UNVERIFIED indefinitely and that is correct, not a gap.
     *
     * Catches what the passive backend can't see: FIRST ACTIVATION (MQTT never
     * connects, so neither EVENT_MQTT_CONNECTED nor EVENT_MQTT_DISCONNECTED
     * fires), and a verified link whose WAN then dies (a BAD verdict drops
     * ONLINE back to UNVERIFIED, re-arming this timer, because the reconnect
     * that follows closes its transporter silently with no on_disconnected
     * callback). With the default passive backend, this parameter - not
     * probe_bad_threshold - is what most demotions actually go through. The
     * fallback to UNVERIFIED is not itself a demotion (UNVERIFIED and ONLINE
     * share the not-suspect tier); a GOOD inside the window returns the link to
     * ONLINE with nothing having happened.
     *
     * Unit: ms. DEFAULT 120000 (2 min), sized above one full activation
     * attempt (token get, ATOP activate, endpoint update, MQTT connect) on a
     * slow link. 0 disables it.
     */
    uint32_t verify_timeout_ms;

    /**
     * Back-off between attempts to re-verify a DEGRADED link - rate-limited
     * because promoting it back to UNVERIFIED moves the route and makes
     * tuya_iot reconnect.
     *
     * DEFAULT netmgr_retry_table_revalidate, {30, 60, 120, 300, 600} seconds
     * (last entry repeats). NETMGR_POLICY_DEFAULT_INIT sets this to
     * `{NULL, 0}` as a SENTINEL resolved in netmgr.c, NOT a literal empty
     * table:
     *
     *   entry == NULL              use netmgr_retry_table_revalidate
     *   entry != NULL, count  > 0  use the product's table
     *   entry != NULL, count == 0  never re-verify
     *
     * To disable revalidation, point entry at any non-NULL array with count 0
     * - a zeroed field means "use the default", not "off".
     *
     * netmgr_retry_fail() reads an empty table the OTHER way (arms at `now`,
     * due immediately) for its other consumer, wifi association back-off,
     * where "no table" must mean "retry at once"; netmgr.c never calls it for
     * a count-0 revalidation table, so the two readings do not collide.
     */
    netmgr_retry_table_t revalidate;

    /**
     * TRUE: publish NETMGR_LINK_UP_SWITH in EVENT_LINK_STATUS_CHG when the
     * aggregate moves from one up link to a different up link without an
     * intervening down.
     *
     * DEFAULT FALSE. The enumerator has never been produced before this
     * module, and known consumers disagree about what it means: several -
     * e.g. the chat-bot UI's wifi indicator - compare
     * `status == NETMGR_LINK_UP`, which reads UP_SWITH as DISCONNECTED, so
     * turning this on can make an existing app report "wifi disconnected" on a
     * healthy handover. Products wanting a handover signal should prefer
     * EVENT_NETMGR_CHG instead, which has no legacy consumers to break.
     */
    BOOL_T emit_up_switch;
} netmgr_policy_t;

/**
 * @brief Where NETMGR_POLICY_DEFAULT_INIT's three Kconfig-derived defaults
 *        come from: ENABLE_NETMGR_PROBE, NETMGR_PROBE_DEMOTE and
 *        NETMGR_POLICY_MIN_DWELL_MS (src/tuya_cloud_service/Kconfig), reached
 *        via the build-generated tuya_kconfig.h.
 *
 * Tested with `defined(...) && (... == 1)`, not a bare defined(): this header
 * is on the public include path and can be included from a translation unit
 * that never saw tuya_kconfig.h (undefined must mean OFF), and
 * `#define ENABLE_NETMGR_PROBE 0` must also mean OFF.
 *
 * probe_demote's test is NESTED inside probe_enable's rather than standing
 * beside it: demotion with probing off would rank links on evidence never
 * collected, so this header enforces that itself rather than trusting every
 * definer to respect Kconfig's own nesting of NETMGR_PROBE_DEMOTE.
 */
#if defined(ENABLE_NETMGR_PROBE) && (ENABLE_NETMGR_PROBE == 1)
#define NETMGR_POLICY_DEFAULT_PROBE_ENABLE TRUE
#if defined(NETMGR_PROBE_DEMOTE) && (NETMGR_PROBE_DEMOTE == 1)
#define NETMGR_POLICY_DEFAULT_PROBE_DEMOTE TRUE
#else
#define NETMGR_POLICY_DEFAULT_PROBE_DEMOTE FALSE
#endif
#else
#define NETMGR_POLICY_DEFAULT_PROBE_ENABLE FALSE
#define NETMGR_POLICY_DEFAULT_PROBE_DEMOTE FALSE
#endif

#if defined(NETMGR_POLICY_MIN_DWELL_MS)
#define NETMGR_POLICY_DEFAULT_MIN_DWELL_MS NETMGR_POLICY_MIN_DWELL_MS
#else
#define NETMGR_POLICY_DEFAULT_MIN_DWELL_MS 0
#endif

/**
 * @brief The shipped default: the pre-M3 behaviour, plus a priority sort that
 *        actually works. See docs/netmgr/release_notes.md §1 for the full
 *        neutrality argument: every timing is 0 and both probe flags are
 *        FALSE unless the board selected otherwise, so with nothing selected
 *        no deadline is ever armed and ranking reduces to "highest pri,
 *        registration order breaking ties".
 *
 * probe_bad_threshold, verify_timeout_ms and revalidate stay literals - they
 * only do anything while probe_enable is TRUE, so a Kconfig symbol for each
 * would be a knob with no effect in a default build; netmgr_policy_set() is
 * the full-fidelity interface for a product that needs different values.
 *
 * Designated initialisers, so a field added later defaults to 0 rather than
 * silently shifting an existing one.
 */
/**
 * @brief The floor under netmgr_policy_t.probe_bad_threshold.
 *
 * Two, because one is the value at which a single mis-attributed NETMGR_PROBE_BAD
 * demotes a healthy link, and netmgr_probe.h's whole argument for tolerating
 * mis-attribution is that it cannot. Applied where the count is compared, so a
 * product that sets 0 or 1 gets a working device rather than a link that drops on
 * the first deliberate tuya_mqtt_stop().
 */
#define NETMGR_PROBE_BAD_THRESHOLD_MIN 2

#define NETMGR_POLICY_DEFAULT_INIT                                                                                     \
    {                                                                                                                  \
        .up_debounce_ms = 0, .down_grace_ms = 0, .min_dwell_ms = NETMGR_POLICY_DEFAULT_MIN_DWELL_MS, .preempt = TRUE,  \
        .probe_enable = NETMGR_POLICY_DEFAULT_PROBE_ENABLE, .probe_demote = NETMGR_POLICY_DEFAULT_PROBE_DEMOTE,        \
        .probe_reconnect = FALSE, .probe_bad_threshold = 3, .verify_timeout_ms = 120000, .revalidate = {NULL, 0},      \
        .emit_up_switch = FALSE,                                                                                       \
    }

/**
 * @brief Read the policy in force.
 *
 * @param[out] policy filled on OPRT_OK, untouched otherwise
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET netmgr_policy_get(netmgr_policy_t *policy);

/**
 * @brief Install a policy. Legal at any time, including before netmgr_init().
 *
 * Unlike netconn_registry_set_table() this is NOT latched - a policy is
 * tuning, not topology, and a product may reasonably change it at runtime
 * (e.g. lengthen dwell after provisioning, disable probing during an OTA).
 * Takes effect at the next reselect, which netmgr schedules immediately.
 *
 * @a policy is copied; @a policy->revalidate.entry is NOT - that array must
 * outlive netmgr, same rule as netmgr_retry_table_t states.
 *
 * @param[in] policy the policy to install; NULL restores
 *                   NETMGR_POLICY_DEFAULT_INIT
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET netmgr_policy_set(const netmgr_policy_t *policy);

/***********************************************************
******************** the selection hook ********************
***********************************************************/

/**
 * @brief One candidate link, as the ranking function sees it. A flat snapshot,
 *        not a pointer into netmgr's live state, so a replacement ranking
 *        function cannot reach the connection list, call a driver, or
 *        deadlock.
 */
typedef struct {
    netmgr_type_e type;

    /** Live conn->pri (a NETCONN_CMD_PRI set can have moved it since
     *  registration), read fresh every pass - the fix for the NETCONN_CMD_PRI
     *  defect: no cached order to disagree with it. Higher wins. */
    uint8_t pri;

    /** Registration order, 0-based (index into s_netmgr.report[]). Secondary
     *  tie-break when pri is equal, preserving M2's rule. Lower wins. */
    uint32_t reg_index;

    netmgr_link_state_e  state;
    netconn_caps_t       caps;
    netconn_ctrl_level_e ctrl;

    /** When up_debounce_ms will have elapsed for this link, same base as
     *  netmgr_select_in_t.now_ms; 0 means eligible now. Computed by netmgr.c;
     *  a replacement ranking function is free to ignore it. */
    uint32_t eligible_at_ms;
} netmgr_link_view_t;

/**
 * @brief Everything the ranking function is given.
 */
typedef struct {
    /** The candidates, in registration order. Never NULL when count > 0. */
    const netmgr_link_view_t *links;
    uint32_t                  count;

    /** Currently active link, or NETCONN_AUTO when there is none. */
    netmgr_type_e active;

    /** Manual override from netmgr_policy_pin(), or NETCONN_AUTO. */
    netmgr_type_e pinned;

    /** Monotonic milliseconds; the base every *_ms field here shares. */
    uint32_t now_ms;

    /** When the active link became active (== now_ms if none is active).
     *  With policy.min_dwell_ms, the whole dwell computation - dwell needs no
     *  state of its own. */
    uint32_t active_since_ms;

    /** The policy in force, so the hook needs no second call to read it. */
    netmgr_policy_t policy;
} netmgr_select_in_t;

/**
 * @brief What the ranking function decided.
 */
typedef struct {
    /**
     * The link that should be active, or NETCONN_AUTO for "no link can carry
     * traffic" (a legitimate answer - what s_netmgr.active already holds when
     * nothing is up). Must be one of the types in @ref netmgr_select_in_t.links
     * and satisfy NETMGR_LINK_STATE_IS_UP() - netmgr VALIDATES this and falls
     * back to the built-in ranking, with an error log, if a hook returns
     * something it cannot honour.
     */
    netmgr_type_e choice;

    /** Ask again in this many milliseconds, or 0 for "no deadline of my own".
     *  netmgr folds this into its single shared deadline alongside its own
     *  pending ones - the mechanism a ranking function uses to implement
     *  hysteresis without owning a timer. */
    uint32_t recheck_ms;
} netmgr_select_out_t;

/**
 * @brief A product's replacement for the built-in ranking.
 *
 * Called with s_netmgr.lock HELD. A hook must therefore be pure arithmetic over
 * @a in: it must not call netmgr_conn_get() or netmgr_conn_set()
 * (self-deadlock on a non-recursive mutex), must not publish, must not block,
 * and must not call any tal_* entry point that might. Everything it could
 * legitimately want is already in @a in, which is why @a in is a snapshot and
 * not a set of accessors.
 *
 * @param[in]  in  the candidates and the context
 * @param[out] out the decision; pre-initialised to
 *                 {NETCONN_AUTO, 0} before the call
 * @param[in]  ctx the pointer handed to netmgr_policy_select_cb_set()
 */
typedef void (*netmgr_policy_select_cb_t)(const netmgr_select_in_t *in, netmgr_select_out_t *out, void *ctx);

/**
 * @brief Replace the built-in ranking - the extension point for logic the
 *        parameters can't express (a signal-strength threshold, a
 *        time-of-day preference, a "never use cellular while charging"
 *        rule). NULL restores the built-in ranking.
 *
 * A function pointer rather than a Kconfig weak symbol - see
 * netconn_registry.h's note on netconn_registry_set_table() for why a weak
 * default is the wrong tool here.
 *
 * @param[in] cb  the ranking function, or NULL for the built-in one
 * @param[in] ctx opaque, passed back on every call; netmgr never dereferences it
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET netmgr_policy_select_cb_set(netmgr_policy_select_cb_t cb, void *ctx);

/**
 * @brief The built-in ranking, exposed so a hook can delegate to it instead of
 *        reimplementing the tie-break rules for the cases it doesn't want to
 *        change. Also what netmgr falls back to when a hook returns an
 *        unusable choice.
 *
 * The rule, in full:
 *
 *   1. discard every link failing NETMGR_LINK_STATE_IS_UP(), and every link
 *      whose eligible_at_ms is still in the future - those two are the
 *      eligibility floor and nothing overrides them, pin included;
 *   2. if in->pinned is eligible, choose it and stop - a pin outranks every
 *      automatic consideration below;
 *   3. partition the survivors into two tiers: NOT-SUSPECT
 *      (UNVERIFIED, ONLINE) and SUSPECT (DEGRADED). Any not-suspect link beats
 *      every suspect one. One tier when policy.probe_demote is FALSE;
 *   4. within a tier, highest pri wins; equal pri breaks to lowest reg_index -
 *      M2's tie-break, unchanged;
 *   5. if the winner is not the current active link, and the active link is
 *      still eligible, and policy.preempt is FALSE, keep the active link -
 *      EXCEPT when the active link is itself SUSPECT and a not-suspect
 *      alternative exists, in which case stickiness is abandoned and the switch
 *      goes ahead. Without this carve-out, preempt FALSE would also disable
 *      fail-over: DEGRADED passes the eligibility floor, so a sticky policy
 *      would stay on a WAN-less wifi link forever instead of falling back to
 *      cellular. Gated on policy.probe_demote, since with demotion off
 *      DEGRADED is not a ranking signal and must not break stickiness either;
 *   6. if the winner is not the current active link, and the active link is
 *      still eligible, and min_dwell_ms has not elapsed since
 *      active_since_ms, keep the active link and set recheck_ms to the
 *      remaining dwell. Shares rule 5's precondition, so an active link that
 *      has stopped being eligible never wins either of them - neither
 *      stickiness nor dwell can strand the device on a dead link;
 *   7. set recheck_ms to the nearest future eligible_at_ms among the links
 *      discarded by rule 1 for debounce, so a debouncing link is reconsidered
 *      when it ripens.
 *
 * UNVERIFIED and ONLINE share a tier because separating them would deadlock a
 * passive probe - see NETMGR_LINK_STATE_UNVERIFIED. SUSPECT is a demotion, not
 * an exclusion: a degraded link is still the best thing available when it is
 * the only thing available.
 *
 * @param[in]  in  as netmgr_policy_select_cb_t
 * @param[out] out as netmgr_policy_select_cb_t
 */
void netmgr_policy_select_default(const netmgr_select_in_t *in, netmgr_select_out_t *out);

/**
 * @brief Rank the candidates: the installed hook if there is one, else the
 *        built-in rule - and VALIDATE whatever comes back. Defined in
 *        netmgr.c, not netmgr_policy.c, because validation needs the live
 *        candidate set that only netmgr.c has.
 *
 * What is validated:
 *
 *   - NETCONN_AUTO is accepted at face value - a legitimate answer meaning "no
 *     link should carry traffic";
 *   - anything else must appear in @a in->links AND satisfy
 *     NETMGR_LINK_STATE_IS_UP() - the same eligibility floor rule 1 of
 *     netmgr_policy_select_default() enforces, which nothing overrides;
 *   - a choice failing either test is logged and REPLACED by
 *     netmgr_policy_select_default(), which also discards the hook's
 *     recheck_ms.
 *
 * Debounce is deliberately NOT enforced against a hook: eligible_at_ms is given
 * to the hook as an input, and a product ranking is entitled to ignore its own
 * hysteresis - a debouncing link is still up, so honouring such a choice cannot
 * route traffic over a dead link.
 *
 * @param[in]  in  the candidates and the context
 * @param[out] out the decision; pre-initialised to {NETCONN_AUTO, 0} here, so a
 *                 hook that writes nothing has answered "no link"
 */
void netmgr_policy_select(const netmgr_select_in_t *in, netmgr_select_out_t *out);

/***********************************************************
********************** manual override *********************
***********************************************************/

/**
 * @brief Pin the active link, or release the pin. Backs `netmgr switch <link>`
 *        in netmgr_cli.c.
 *
 * Semantics:
 *
 *   - a pin outranks priority, tiering, stickiness and dwell;
 *   - a pin does NOT override the eligibility floor. Pinning a link that is
 *     down cannot make traffic leave through it, so the pin is REMEMBERED and
 *     takes effect if and when the link comes up; netmgr_policy_pin() reports
 *     which of the two happened;
 *   - a pin does not dial. There is no generic "connect" verb in
 *     netmgr_conn_config_type_e, so `netmgr switch wifi` on a down wifi arms
 *     the pin and nothing else - use `netmgr wifi up <ssid>` to actually
 *     connect;
 *   - a pin survives link events, reselects and policy changes. It is cleared
 *     only by netmgr_policy_pin(NETCONN_AUTO) and by netmgr_deinit().
 *
 * @param[in] type a single netmgr_type_e bit to pin, or NETCONN_AUTO to release
 *
 * @return OPRT_OK when the pin is armed AND the link is eligible now.
 *         OPRT_RESOURCE_NOT_READY when the pin is armed but the link cannot carry
 *         traffic yet - not a failure, the pin is set.
 *         OPRT_NOT_FOUND when @a type is not registered in this build.
 *         Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET netmgr_policy_pin(netmgr_type_e type);

/**
 * @brief Read the pin.
 *
 * @param[out] type NETCONN_AUTO when nothing is pinned
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET netmgr_policy_pin_get(netmgr_type_e *type);

/***********************************************************
********************* state inspection *********************
***********************************************************/

/**
 * @brief Read the internal state of one link. For the CLI and diagnostics.
 *
 * Deliberately NOT routed through netmgr_conn_get(NETCONN_CMD_STATUS): that
 * command's contract is the public two-valued status and must keep answering
 * exactly that. A caller wanting the richer value asks for it by name.
 *
 * Takes and releases s_netmgr.lock.
 *
 * @param[in]  type  a single netmgr_type_e bit, or NETCONN_AUTO for the active
 *                   link
 * @param[out] state filled on OPRT_OK, untouched otherwise
 *
 * @return OPRT_OK on success, OPRT_NOT_FOUND when @a type is not registered or
 *         NETCONN_AUTO resolves to nothing. Others on error, please refer to
 *         tuya_error_code.h
 */
OPERATE_RET netmgr_link_state_get(netmgr_type_e type, netmgr_link_state_e *state);

#ifdef __cplusplus
}
#endif

#endif /* __NETMGR_POLICY_H__ */
