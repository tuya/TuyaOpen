/**
 * @file netmgr_policy.h
 * @brief Which link carries the traffic, and why - the decision netmgr currently
 *        makes by walking a linked list.
 *
 * What this replaces
 * ------------------
 * All of link selection in netmgr today is __get_active_conn():
 *
 *     while (cur_conn) {
 *         cur_conn->get(NETCONN_CMD_STATUS, &netmgr_status);
 *         if (netmgr_status == NETMGR_LINK_UP) { active_type = cur_conn->type; break; }
 *         cur_conn = cur_conn->next;
 *     }
 *
 * "The first link in list order that is up." Three consequences, all of them
 * defects rather than choices:
 *
 *   - the order is baked into the CONTAINER. __netmgr_conn_register() inserts by
 *     descending priority once, at registration, and nothing ever re-sorts. So
 *     changing conn->pri at runtime cannot change the outcome, and the comment in
 *     netconn_wifi_set() - "set pri will cause status change to reneg the active
 *     connection" - describes an intent the code does not implement;
 *   - "up" is the only predicate, so a link with an address and no route to the
 *     internet outranks a working one at lower priority. On wifi+4G that pins the
 *     device to a dead AP forever;
 *   - the decision is instantaneous and memoryless, so a link that flaps moves
 *     the route, the MQTT session and every subscriber with it, on every flap.
 *
 * The shape of the fix
 * --------------------
 * Order stops being a property of the container. netmgr keeps its linked list -
 * netmgr_conn_base_t.next is public and netmgr_link_info_at() iterates it - but
 * the list becomes REGISTRATION ORDER, and ranking happens here, from a snapshot,
 * every time a decision is needed. That is what makes NETCONN_CMD_PRI work: there
 * is no sorted structure left to go stale.
 *
 * The tie-break M2 established survives verbatim. netconn_table.c records that
 * two links with equal priority are ordered by registration alone, and calls
 * preserving that the difference between provably and probably behaviour neutral.
 * @ref netmgr_link_view_t therefore carries reg_index, and the default ranking
 * uses it as the secondary key - so equal-priority links still come out in
 * registration order, and they do so because the rule is written down rather than
 * because an insertion loop happens to be stable.
 *
 * Division of labour with netmgr.c
 * --------------------------------
 * This module decides. netmgr.c knows everything else. The seam is deliberate and
 * it is what makes the M4 degenerate build a two-line function.
 *
 *   netmgr.c owns:  driver reports and the per-link state machine below,
 *                   including all debounce and grace TIMING; probe verdict
 *                   accumulation; retry deadlines; the single shared
 *                   tal_sw_timer; the route push; the events; the LAN gate.
 *   this module:    given a snapshot of link states, name the link that should
 *                   be active, and say when to ask again.
 *
 * So this module has no timer, no lock, no driver access, and no state except the
 * parameters and the pin. It cannot block and it cannot re-enter netmgr. That is
 * why netmgr_policy_select() is the one thing in the module that IS allowed to be
 * called with s_netmgr.lock held.
 *
 * The single shared deadline
 * --------------------------
 * Debounce, grace, dwell, probe timeout and revalidation are all deadlines, and
 * they are all served by ONE tal_sw_timer in netmgr.c, armed to the nearest one
 * pending across every link. Not one timer per link, and the reason is not
 * frugality with the ~48 bytes a TIMER_T costs:
 *
 *   - tal_sw_timer.c has no count limit, so a per-link timer would not be
 *     rejected, it would just be worse;
 *   - every timer in the process shares one "sys_timer" thread at THREAD_PRIO_0
 *     and __timer_dispatch() runs the callbacks serially. More timers means more
 *     wake-ups of that one thread for the same information, which is exactly the
 *     cost the ULP path is built to avoid;
 *   - the deadlines are not independent. A reselect triggered by any of them
 *     re-evaluates all of them, so N timers would produce N wake-ups where one
 *     suffices.
 *
 * The callback stays on the sys_timer thread, which is NOT the WORKQ_SYSTEM
 * thread the M2 state machine runs on. So the callback does exactly what the LAN
 * timer callback does and nothing more: check the gate, post the coalesced notify
 * work item, return. The state machine keeps running in one context. Nothing here
 * adds a concurrency source.
 *
 * Net timer count for M3: two before (the LAN poll timer and the wifi reconnect
 * timer), two after (the shared deadline and the wifi reconnect timer). The LAN
 * poll timer is deleted because the LAN gate becomes event-driven, which pays for
 * the new one.
 *
 * Behaviour neutrality, structurally
 * ----------------------------------
 * With NETMGR_POLICY_DEFAULT_INIT every timing parameter is 0. That is not a
 * tuning choice, it is the neutrality argument: at zero there are no deadlines to
 * arm, so the shared timer is never started, the debounce and grace branches
 * short-circuit to "now", and netmgr_policy_select() reduces to
 *
 *     the eligible link with the highest pri, registration order breaking ties
 *
 * which is __get_active_conn() with a priority sort that actually works. A
 * product that wants hysteresis sets a parameter; a product that does nothing
 * gets today's timing.
 *
 * M3 shipped one default that was NOT neutral - probing - on the argument that
 * it is neutral on a single-link board and corrective on a multi-link one. M4
 * turned it off. NETMGR_POLICY_DEFAULT_INIT now derives probe_enable,
 * probe_demote and min_dwell_ms from Kconfig (ENABLE_NETMGR_PROBE,
 * NETMGR_PROBE_DEMOTE, NETMGR_POLICY_MIN_DWELL_MS in
 * src/tuya_cloud_service/Kconfig), and every one of them is off or zero unless a
 * board asks for it. So the neutrality argument above now covers the WHOLE
 * default rather than all of it but one: with nothing selected,
 * NETMGR_LINK_STATE_DEGRADED is unreachable, no deadline is ever armed, and the
 * shared timer is never started. A product opts into the behaviour change
 * explicitly, on the board where it is a fix. See netmgr_policy_t.probe_enable
 * for the argument that opting in is safe - it is unchanged, it is just no
 * longer an argument about a default.
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
 * @brief What netmgr knows about one link. Internal; see the mapping below.
 *
 * This is netmgr's own state, layered on top of what the driver reports. The
 * drivers are NOT changed: conn->get(NETCONN_CMD_STATUS) keeps answering
 * NETMGR_LINK_UP or NETMGR_LINK_DOWN out of base.status, exactly as today, and
 * that remains the L2/L3 input to the machine below. Everything richer is
 * netmgr-side, derived from that input plus probe verdicts plus deadlines.
 *
 * Keeping it out of the drivers is what makes the whole thing additive. A driver
 * has no way to know whether its link reaches the cloud, no way to know what the
 * policy timings are, and no business knowing either.
 *
 * DOWN is 0 so a zeroed slot is DOWN, which is the same convention
 * netmgr_conn_base_t.status already follows.
 */
typedef enum {
    /**
     * No association, no address, nothing pending. The driver says
     * NETMGR_LINK_DOWN and netmgr has no reason to expect that to change on its
     * own.
     */
    NETMGR_LINK_STATE_DOWN = 0,

    /**
     * An attempt is in flight. Only reachable on NETCONN_CTRL_MANAGED links,
     * because it is the only control level with an attempt to make - see
     * netconn_registry.h. netmgr enters it when it observes a MANAGED driver
     * begin a connect, and leaves it on the next driver report or when the
     * attempt deadline expires.
     *
     * Maps to NETMGR_LINK_DOWN publicly: a link that is dialling cannot carry
     * traffic, and a caller asking "is my network up" must not be told yes.
     */
    NETMGR_LINK_STATE_CONNECTING = 1,

    /**
     * L3 is up - association and an address - and reachability is UNKNOWN.
     *
     * This is the state every link enters the moment its driver reports
     * NETMGR_LINK_UP, and with probing disabled it is the only up-state that ever
     * exists. That is why it must be fully eligible for selection and must rank
     * alongside ONLINE rather than below it: a passive probe can only ever
     * observe the ACTIVE link, so a link that is never selected can never be
     * verified. Ranking UNVERIFIED below ONLINE would be a deadlock - the
     * higher-priority link would wait for a verdict it can only earn by being
     * selected.
     */
    NETMGR_LINK_STATE_UNVERIFIED = 2,

    /**
     * L3 is up and something reached the cloud through it. See netmgr_probe.h.
     */
    NETMGR_LINK_STATE_ONLINE = 3,

    /**
     * L3 is up and netmgr has evidence that traffic does NOT get through:
     * probe_bad_threshold consecutive BAD verdicts, or verify_timeout_ms elapsed
     * with no verdict at all.
     *
     * Still maps to NETMGR_LINK_UP publicly, and that is the single most
     * load-bearing decision in this header. Three reasons, any one of them
     * sufficient:
     *
     *   - the link IS up. It has an address, it carries LAN traffic, and
     *     tuya_lan.c reads its address through
     *     netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_IP);
     *   - reporting DOWN would stop the retry that discovers recovery. The iot
     *     state machine's STATE_MQTT_RECONNECT branches on
     *     client->config.network_check(), and every app in this tree wires that
     *     to netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS). Answering DOWN
     *     sends it to STATE_NETWORK_RECONNECT to poll for a link that is already
     *     there, and it stops trying to reach the cloud - so the verdict that
     *     would clear the degradation is never produced. The mapping would make
     *     the state permanent;
     *   - it would flap every consumer. Twelve app-level helpers compute
     *     `status != NETMGR_LINK_DOWN` into an is_connected flag that drives
     *     audio, UI and chat sessions.
     *
     * DEGRADED is a RANKING signal, not a liveness signal. It says "prefer
     * something else if there is something else", and on a single-link board
     * there never is - which is why turning probing on is neutral there, and
     * why ENABLE_NETMGR_PROBE's help text tells a single-link board not to
     * bother.
     */
    NETMGR_LINK_STATE_DEGRADED = 4,

    /**
     * Down, and waiting out a back-off before the next attempt or the next
     * revalidation. Distinct from DOWN because netmgr HAS a pending deadline for
     * it, so the shared timer has something to arm and the CLI has something to
     * print. Maps to NETMGR_LINK_DOWN publicly.
     */
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
 * @brief Can this state carry traffic?
 *
 * The eligibility floor, and the single definition of it. A link that passes may
 * be selected; a link that does not, may not, whatever its priority and whatever
 * the pin says. DEGRADED passes - see its note.
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
 * The complete mapping. There is no third public value to map onto:
 * NETMGR_LINK_UP_SWITH is not a link state, it is a property of a TRANSITION of
 * the aggregate, so it is produced in netmgr_event.h's terms and never here.
 *
 *   DOWN, CONNECTING, BACKOFF                -> NETMGR_LINK_DOWN
 *   UNVERIFIED, ONLINE, DEGRADED             -> NETMGR_LINK_UP
 *
 * Every existing reader of a status keeps its meaning:
 *
 *   - conn->get(NETCONN_CMD_STATUS) is untouched. It is answered by the driver
 *     from base.status and never consults this enum, so all 20 call sites of
 *     netmgr_conn_get(..., NETCONN_CMD_STATUS, ...) - ble_mgr.c, cli_cmd.c,
 *     tuya_svc_netmgr.c and seventeen app helpers - see the same two values they
 *     see today;
 *   - netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS) resolves AUTO to
 *     s_netmgr.active and then asks that driver, so its RANGE is unchanged. Which
 *     link AUTO resolves to can differ, because that is the entire point of M3,
 *     but only when probing has produced a BAD verdict and an alternative exists;
 *   - s_netmgr.status, the payload of EVENT_LINK_STATUS_CHG, stays in
 *     {DOWN, UP, UP_SWITH} and is derived through this macro.
 *
 * Note what the mapping buys: with probing off, DEGRADED is unreachable, so the
 * internal machine has exactly the two up-states UNVERIFIED and ONLINE, both of
 * which map to UP, and the aggregate is bit-identical to today.
 */
#define NETMGR_LINK_STATE_TO_STATUS(s) (NETMGR_LINK_STATE_IS_UP(s) ? NETMGR_LINK_UP : NETMGR_LINK_DOWN)

/***********************************************************
************************ parameters ************************
***********************************************************/

/**
 * @brief Tunable selection behaviour.
 *
 * One struct rather than a command per knob, so a product sets its whole policy
 * in one call and so netmgr.c reads a consistent set - a per-knob API would let a
 * reselect run between two related writes.
 *
 * Deliberately NOT reachable through netmgr_conn_set(): a policy is a property of
 * the DEVICE, not of a link, and every NETCONN_CMD_* is addressed to one link.
 * Adding NETCONN_CMD_POLICY would make "which link's policy" a question with no
 * answer.
 */
typedef struct {
    /**
     * A link must report up continuously for this long before it becomes
     * eligible. Suppresses a flapping link taking the route for a moment.
     *
     * DEFAULT 0: eligible on the first report, as today.
     */
    uint32_t up_debounce_ms;

    /**
     * When the ACTIVE link reports down, hold it active - and hold its route -
     * for this long before reselecting, so a brief drop does not tear down the
     * MQTT session.
     *
     * Applies only to the active link. A standby link that drops has nothing to
     * hold on to.
     *
     * Consumed entirely by netmgr.c and INVISIBLE to
     * netmgr_policy_select_default(): grace is expressed by holding the link's
     * netmgr_link_state_e up for the duration, so by the time a snapshot is built
     * the grace has already had its effect and there is nothing left for the
     * ranking to read. Do not look for it in the seven rules below - it is not
     * missing, it is upstream. Same division of labour as up_debounce_ms, which
     * does surface, but only as the already-computed
     * netmgr_link_view_t.eligible_at_ms.
     *
     * DEFAULT 0: reselect immediately, as today.
     */
    uint32_t down_grace_ms;

    /**
     * After a switch, do not switch again for this long.
     *
     * Never traps the device on a dead link: dwell is only consulted when the
     * active link is still eligible. The moment it stops being eligible the
     * dwell is abandoned, because the alternative is having no network in order
     * to honour a hysteresis parameter.
     *
     * DEFAULT NETMGR_POLICY_MIN_DWELL_MS, which is 0 - no dwell, as today -
     * unless the board selected a value in Kconfig. The symbol only appears
     * under ENABLE_NETMGR_PROBE, because the oscillation described below is what
     * it exists to damp and that oscillation needs demotion to start it.
     *
     * A MULTI-LINK PRODUCT SHOULD SET THIS. Concretely, a wifi+4G board whose wifi
     * AP has lost its WAN will oscillate at the default of 0, and the mechanism is
     * inherent to passive probing rather than a defect:
     *
     *   1. wifi is demoted to DEGRADED (verify_timeout_ms elapses with no verdict)
     *      and the route moves to cellular. Correct, and the whole point of M3;
     *   2. @ref revalidate promotes wifi back to NETMGR_LINK_STATE_UNVERIFIED after
     *      30 s. It has to: a passive probe can only ever judge the ACTIVE link, so
     *      the only way to find out whether wifi recovered is to use it again;
     *   3. UNVERIFIED shares the not-suspect tier with ONLINE (see
     *      NETMGR_LINK_STATE_UNVERIFIED for why separating them would deadlock), so
     *      wifi's higher priority wins and the route moves back;
     *   4. verify_timeout_ms elapses again and it returns to step 1.
     *
     * Each swing tears down and re-establishes the MQTT session, twice per cycle,
     * because __tuya_iot_link_type_change_cb() calls tuya_iot_reconnect(). The
     * revalidation table damps the period - 30, 60, 120, 300 then 600 s plus
     * verify_timeout_ms - so it settles at roughly one cycle every twelve minutes
     * and stays there. Bounded, but visible.
     *
     * min_dwell_ms is the knob that damps the early, fast end of that sequence: it
     * holds a switch until the dwell expires without ever preventing one, so the
     * first few cycles stop being the noisiest. A board that would rather not
     * re-verify at all sets @ref revalidate to a non-NULL entry with count 0, at
     * the cost of never discovering that wifi came back.
     *
     * The same warning is repeated, in full, in NETMGR_POLICY_MIN_DWELL_MS's
     * Kconfig help. Deliberate duplication: the person who needs it is the one
     * flipping ENABLE_NETMGR_PROBE in menuconfig, and that person is not reading
     * this header. If the two ever disagree, this one is the specification.
     */
    uint32_t min_dwell_ms;

    /**
     * TRUE: a higher-ranked link takes over as soon as it becomes eligible.
     * FALSE: sticky - once a link is active it keeps the route while it stays
     * eligible, and a reselect only happens when it stops being eligible.
     *
     * DEFAULT TRUE. This is today's behaviour: __get_active_conn() recomputes
     * from scratch on every event and has no memory of what was active.
     */
    BOOL_T preempt;

    /**
     * Master switch for everything in netmgr_probe.h. FALSE means verdicts are
     * dropped, verify_timeout_ms is not armed, and NETMGR_LINK_STATE_DEGRADED is
     * unreachable.
     *
     * DEFAULT FALSE, from Kconfig ENABLE_NETMGR_PROBE. M3 shipped this TRUE;
     * M4 turned it off so that a build which selects nothing behaves exactly as
     * it did before the probe existed, and so that the opt-in is a deliberate
     * act by a product that knows it has two links.
     *
     * The argument below is unchanged and is still the reason the switch is
     * offered - it says why turning it ON is safe. What it does not support is
     * making it a default: "neutral" in it is a statement about a single-link
     * board, and this tree contains boards of both kinds. The argument:
     *
     *   - the default backend emits no packets, so there is no traffic, power or
     *     billing cost to turn on. It observes MQTT transitions the device
     *     already produces;
     *   - the ONLY effect of a BAD verdict is that DEGRADED loses to a
     *     non-suspect link during ranking. On a single-link board there is no
     *     other link, so the ranking has one candidate and the outcome is
     *     identical whatever its state. Every wifi-only and ethernet-only
     *     product in this tree is therefore untouched, provably, not
     *     approximately;
     *   - on a multi-link board the changed outcome IS the bug being fixed. A
     *     wifi+4G device on a WAN-less AP moves to cellular instead of pinning
     *     to wifi forever.
     *
     * So: neutral where there is nothing to choose, corrective where there is.
     * FALSE is nonetheless the shipped value, because it is the one that needs
     * no argument at all - and it remains the single flag that restores M2
     * behaviour exactly, which is now what a default build gets.
     */
    BOOL_T probe_enable;

    /**
     * TRUE: a DEGRADED link ranks below every non-suspect eligible link.
     * FALSE: verdicts are still accumulated and still visible to the CLI and to
     * netmgr_probe_stat_get(), but they do not affect ranking.
     *
     * DEFAULT FALSE, and TRUE only when ENABLE_NETMGR_PROBE and
     * NETMGR_PROBE_DEMOTE are BOTH selected. It cannot be TRUE while
     * probe_enable is FALSE - that would be a policy ranking links on evidence
     * it never collects - and NETMGR_POLICY_DEFAULT_INIT enforces that by
     * nesting the tests rather than trusting Kconfig's own nesting.
     * NETMGR_PROBE_DEMOTE defaults y once probing is on, because a board that
     * asked for the probe asked for the fix.
     *
     * Split from probe_enable so a product can deploy the observability first
     * and the behaviour change second, which is the only way to get field data
     * before committing.
     */
    BOOL_T probe_demote;

    /**
     * TRUE: on entering DEGRADED, a MANAGED link is also dropped and re-dialled.
     *
     * DEFAULT FALSE, and it must stay false by default. Consider a single-link
     * wifi product during a cloud-side outage: probing correctly reports BAD, the
     * link goes DEGRADED, and with this flag set netmgr would bounce a perfectly
     * healthy association - repeatedly, for the duration of an outage it cannot
     * do anything about. That converts an invisible cloud problem into a visible
     * device problem. Re-association is a remedy for a broken ASSOCIATION, and
     * DEGRADED is not evidence of one.
     */
    BOOL_T probe_reconnect;

    /**
     * Consecutive NETMGR_PROBE_BAD verdicts needed to move a link to DEGRADED.
     * Any NETMGR_PROBE_GOOD resets the count.
     *
     * DEFAULT 3. Must be greater than 1, and the reason is one specific class of
     * false BAD: a DELIBERATE tuya_mqtt_stop() that does not move the route.
     * EVENT_MQTT_DISCONNECTED fires on it exactly as it does on a keepalive
     * timeout, and mqtt_client_disconnect_on() cannot tell them apart. Three of
     * those call sites matter - STATE_STOP (tuya_iot.c:1108), run_state_reset()
     * (:555) and tuya_iot_destroy() (:789) - and the last two are gated on
     * tuya_mqtt_connected(), so they fire with a CURRENT epoch and therefore
     * survive the staleness check that catches switch-induced teardowns. They look
     * like genuine evidence against a perfectly healthy link. A threshold above 1
     * is what absorbs them. Three, against the MQTT layer's own 1 s-to-8 s
     * reconnect back-off plus its hardcoded +10 s sleep, is on the order of half a
     * minute of sustained failure.
     *
     * WHICH BACKEND THIS SERVES. The threshold is a knob for an ACTIVE backend,
     * one that can emit several BADs in a row with no GOOD in between. With the
     * default PASSIVE backend it is very nearly unreachable, and a reader should
     * not go looking for a bug in that: every BAD needs a session teardown and
     * every session establishment publishes a GOOD that zeroes the count, so the
     * stream is GOOD/BAD/GOOD/BAD and never reaches 2. Passive demotion therefore
     * happens through @ref verify_timeout_ms instead, on the path
     * ONLINE --(BAD)--> UNVERIFIED --(timeout)--> DEGRADED. That is not a
     * degradation of the design, it is which mechanism covers which backend.
     *
     * 0 is treated as 1.
     */
    uint8_t probe_bad_threshold;

    /**
     * How long a link may stay ACTIVE and UNVERIFIED before netmgr synthesises a
     * BAD of its own (source NETMGR_PROBE_SRC_TIMEOUT).
     *
     * Only runs for the active link, because a passive backend can only observe
     * the active link. A standby link stays UNVERIFIED indefinitely and that is
     * correct, not a gap: it has produced no evidence either way.
     *
     * This is the mechanism that fixes the WAN-less AP case with no cooperation
     * from any other layer, and the case it uniquely covers is
     * FIRST ACTIVATION - MQTT never connects on an unactivated device, so neither
     * EVENT_MQTT_CONNECTED nor EVENT_MQTT_DISCONNECTED ever fires and the passive
     * backend is silent.
     *
     * It also covers the OTHER, commoner case, and this is the more important half
     * in the field: a link that WAS verified and whose WAN then dies. A link at
     * NETMGR_LINK_STATE_ONLINE that receives a BAD falls back to UNVERIFIED, which
     * re-arms this timer. Without that fallback the link stayed ONLINE forever -
     * one BAD is below @ref probe_bad_threshold, every later reconnect attempt is
     * silent because mqtt_client_connect() closes its transporter and returns
     * without calling on_disconnected (mqtt_client_wrapper.c:218-223), and this
     * timeout did not apply because the link was ONLINE rather than UNVERIFIED. So
     * with the default passive backend this parameter, not the threshold, is what
     * every demotion goes through.
     *
     * The fallback is not itself a demotion: UNVERIFIED and ONLINE share the
     * not-suspect tier, so it changes no ranking and moves no route. It only
     * restarts the clock, and a GOOD inside the window puts the link back to
     * ONLINE with nothing having happened.
     *
     * DEFAULT 120000 (2 min). Sized above one full activation attempt on a slow
     * link: token get, ATOP activate, endpoint update and MQTT connect, each of
     * which the iot state machine already retries with 1 s polls. Too short and a
     * slow-but-working link is demoted; too long and the fix takes minutes.
     *
     * 0 disables it.
     */
    uint32_t verify_timeout_ms;

    /**
     * Back-off between attempts to re-verify a DEGRADED link.
     *
     * Re-verification is not passive and cannot be: promoting the link back to
     * UNVERIFIED makes it win ranking again, which moves the route, which makes
     * tuya_iot reconnect. So it is rate-limited by a table rather than retried
     * freely.
     *
     * DEFAULT netmgr_retry_table_revalidate, {30, 60, 120, 300, 600} seconds.
     * The last entry repeating is what bounds the cost of a link that never
     * recovers.
     *
     * HOW THE DEFAULT IS EXPRESSED, and it matters because the two obvious
     * readings differ. NETMGR_POLICY_DEFAULT_INIT sets `.revalidate = {NULL, 0}`,
     * and read as a literal table that is a table of no entries. So the sentinel
     * is resolved at the point of use, in netmgr.c:
     *
     *   entry == NULL              use netmgr_retry_table_revalidate, i.e. the
     *                              default this field documents. The `{NULL, 0}`
     *                              in the initialiser means "unset", not "empty"
     *   entry != NULL, count  > 0  use the product's table
     *   entry != NULL, count == 0  never re-verify
     *
     * A product that wants revalidation OFF therefore points entry at any non-NULL
     * array and sets count to 0. Without that split the documented default would
     * be unreachable and the shipped default would be its opposite: a link demoted
     * once could never recover for the life of the boot, which is the failure a
     * revalidation table exists to prevent.
     *
     * Never re-verifying is still a legitimate choice for a board whose secondary
     * link is strictly a fallback - it just has to be said explicitly.
     *
     * Note that netmgr_retry_fail() reads an empty table the other way, arming at
     * `now` so the retry is due immediately. That is correct for its other
     * consumer, the wifi association back-off, where "no table" has to mean "retry
     * at once"; netmgr.c does not call it for a count-0 revalidation table, so the
     * two readings do not collide. netmgr_retry.h records the same split.
     */
    netmgr_retry_table_t revalidate;

    /**
     * TRUE: publish NETMGR_LINK_UP_SWITH in EVENT_LINK_STATUS_CHG when the
     * aggregate moves from one up link to a different up link without an
     * intervening down.
     *
     * DEFAULT FALSE, and the reason is concrete rather than cautious. The enum
     * value has never been produced, so no consumer has ever been tested against
     * it, and the tree splits into two camps that disagree about what it means:
     *
     *   - eleven app helpers compute `status != NETMGR_LINK_DOWN`, which reads
     *     UP_SWITH as connected. Correct;
     *   - several sites compare `status == NETMGR_LINK_UP`, which reads UP_SWITH
     *     as DISCONNECTED.
     *
     * The consumer that settles it is
     * apps/tuya.ai/your_chat_bot/src/display2/app_ui_helper.c:88-90. It subscribes
     * to EVENT_LINK_STATUS_CHG, dereferences the payload CORRECTLY - unlike the
     * miscast consumers noted in netmgr_event.h - and then computes
     *
     *     uint8_t connected = (net_status == NETMGR_LINK_UP) ? 1 : 0;
     *
     * feeding ui_setting_wifi_update() and SYSTEM_MSG_WIFI_DISCONNECTED. So
     * enabling this flag makes the chat-bot UI display "wifi disconnected" on
     * every link handover. examples/protocols/{https,http,mqtt}_client compare the
     * same way.
     *
     * An earlier draft of this note cited tuya_svc_netmgr.c:37 instead, on the
     * grounds that it maps anything that is not NETMGR_LINK_UP to
     * NETWORK_STATUS_OFFLINE for tuya_ai_client.c. That citation is WRONG and is
     * corrected here rather than dropped, because a wrong citation is worse than
     * none - it makes the next reader believe the check was done. That site reads
     * netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, ...), which resolves AUTO
     * to the active link and then asks the DRIVER, whose base.status only ever
     * holds NETMGR_LINK_DOWN or NETMGR_LINK_UP. It cannot observe UP_SWITH at all,
     * so it cannot be broken by this flag. The conclusion is unchanged; only the
     * evidence is.
     *
     * Worse, today a handover publishes NO status event at all: s_netmgr.status
     * stays NETMGR_LINK_UP, status_chg stays FALSE, and only EVENT_LINK_TYPE_CHG
     * fires. So enabling this ADDS an event that has never existed, to
     * subscribers that have never seen it. That is a behaviour change with a
     * known-broken consumer set, and it does not belong in a default.
     *
     * The enumerator is nonetheless now REACHABLE, which is what
     * examples/multimedia/audio_player/music/src/tuya_app_main.c:100 has been
     * testing for since it was written. See netmgr_event.h for the defect in that
     * particular consumer, which has to be fixed before the flag is any use to
     * it.
     *
     * Products wanting the richer signal should prefer EVENT_NETMGR_CHG, which
     * carries the handover explicitly and has no legacy consumers to break.
     */
    BOOL_T emit_up_switch;
} netmgr_policy_t;

/**
 * @brief Where the three Kconfig-selected defaults come from, and what they are
 *        when nothing selected them.
 *
 * The symbols are ENABLE_NETMGR_PROBE, NETMGR_PROBE_DEMOTE and
 * NETMGR_POLICY_MIN_DWELL_MS, declared in src/tuya_cloud_service/Kconfig beside
 * ENABLE_CELLULAR. They reach this header through the build-generated
 * tuya_kconfig.h, which every platform's tuya_cloud_types.h pulls in via
 * tuya_iot_config.h - so the `#include "tuya_cloud_types.h"` at the top of this
 * file is enough to make them visible and nothing here needs a second include.
 * That is the same route tal_network_register.h documents for ENABLE_LIBLWIP.
 *
 * They are nevertheless all tested with defined(), because this header is on the
 * PUBLIC include path: an app, an example or an out-of-tree component may include
 * it from a translation unit that never sees tuya_kconfig.h, and a header that
 * only compiles inside the SDK's own build is a header that breaks somebody's
 * build later. Undefined therefore means OFF here, silently and on purpose.
 *
 * `== 1` rather than a bare defined() is the lesson netconn_table.c records
 * about ENABLE_CELLULAR: kconfiglib emits nothing at all for an unset bool, so
 * defined() alone would be sufficient for a Kconfig-generated symbol - but a
 * board or an app that hand-writes `#define ENABLE_NETMGR_PROBE 0` to mean off
 * would then turn it ON. Both spellings of off work here.
 *
 * The probe_demote test is NESTED inside the probe_enable one rather than
 * standing beside it, and the nesting is load-bearing: demotion with probing off
 * is a policy that ranks links on evidence it never collects. Kconfig already
 * prevents that by putting NETMGR_PROBE_DEMOTE under `if (ENABLE_NETMGR_PROBE)`,
 * but this header holds the invariant on its own, for the hand-defining caller
 * above.
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
 * @brief The shipped default policy: the behaviour netmgr had before M3, plus a
 *        priority sort that actually works.
 *
 * Every timing is 0 and both probe flags are FALSE unless the board selected
 * otherwise, which is the neutrality argument at the top of this file made
 * concrete. Traced through the code rather than asserted:
 *
 *   - up_debounce_ms, down_grace_ms and min_dwell_ms are 0, so netmgr.c writes 0
 *     into netmgr_report_t.eligible_at_ms and .grace_at_ms and this module's
 *     recheck_ms stays 0;
 *   - verify_at_ms is armed only under `pol.probe_enable`, so with probing off it
 *     is 0 too;
 *   - the revalidation deadline is armed only when a link enters
 *     NETMGR_LINK_STATE_DEGRADED, which needs a probe verdict, so it never arms;
 *   - those four plus recheck_ms are the entire input to netmgr.c's deadline
 *     fold, and 0 means "contributes nothing" throughout. The shared timer is
 *     therefore stopped, not started, on every pass;
 *   - DEGRADED being unreachable also empties the SUSPECT tier of
 *     netmgr_policy_select_default(), so the ranking is exactly `highest pri,
 *     registration order breaking ties`.
 *
 * The three non-literal fields are the three a product opts into. The rest stay
 * literals deliberately: probe_bad_threshold, verify_timeout_ms and revalidate
 * only do anything while probe_enable is TRUE, so a Kconfig symbol for each
 * would be a knob with no effect in a default build. A product that needs them
 * away from the documented values calls netmgr_policy_set(), which is the
 * full-fidelity interface - and revalidate is a table of pointers that a Kconfig
 * int could not express in any case.
 *
 * Designated initialisers, so a field added later defaults to 0 rather than
 * silently shifting an existing one.
 */
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
 * Unlike netconn_registry_set_table() this is NOT latched, because a policy is
 * tuning rather than topology: a product may reasonably lengthen its dwell after
 * provisioning finishes, or disable probing while an OTA runs. A change takes
 * effect at the next reselect, which netmgr schedules immediately, so the caller
 * does not have to wait for a link event.
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
 * @brief One candidate link, as the ranking function sees it.
 *
 * A flat snapshot, not a pointer into netmgr's live state, so a replacement
 * ranking function cannot reach the connection list, cannot call a driver and
 * cannot deadlock. Everything it is allowed to know is in here.
 */
typedef struct {
    netmgr_type_e type;

    /**
     * Live conn->pri, which a NETCONN_CMD_PRI set can have moved since
     * registration. Higher wins. Reading it here per pass is the fix for the
     * NETCONN_CMD_PRI defect: there is no cached order to disagree with it.
     */
    uint8_t pri;

    /**
     * Position in registration order, 0-based, which is the index into
     * s_netmgr.report[]. The secondary ranking key, preserving M2's tie-break
     * exactly. Lower wins.
     */
    uint32_t reg_index;

    netmgr_link_state_e  state;
    netconn_caps_t       caps;
    netconn_ctrl_level_e ctrl;

    /**
     * When up_debounce_ms will have elapsed for this link, in the same base as
     * netmgr_select_in_t.now_ms. 0 means eligible now.
     *
     * Computed by netmgr.c, which owns the up-timestamps, and honoured here. That
     * split is what keeps all timing in one place while leaving the ranking
     * function free to ignore debounce if a product's ranking has its own idea.
     */
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

    /**
     * When the current active link became active. Equals now_ms when there is no
     * active link. This plus policy.min_dwell_ms is the whole dwell computation,
     * which is why dwell needs no state of its own.
     */
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
     * traffic". NETCONN_AUTO is a legitimate answer and netmgr handles it - it is
     * what s_netmgr.active already holds when nothing is up.
     *
     * Must be one of the types in @ref netmgr_select_in_t.links and must satisfy
     * NETMGR_LINK_STATE_IS_UP(). netmgr VALIDATES this rather than trusting it,
     * and falls back to the built-in ranking with an error log if a hook returns
     * something it cannot honour - a product-supplied hook must not be able to
     * route traffic over a link that is down.
     */
    netmgr_type_e choice;

    /**
     * Ask again in this many milliseconds, or 0 for "no deadline of my own".
     *
     * This is how a ranking function gets timing without owning a timer. netmgr
     * folds it into the single shared deadline alongside its own pending ones. It
     * is what the default ranking uses to come back when a dwell or a debounce
     * expires, and it is the only mechanism a replacement needs in order to
     * implement any hysteresis it likes.
     */
    uint32_t recheck_ms;
} netmgr_select_out_t;

/**
 * @brief A product's replacement for the built-in ranking.
 *
 * Called with s_netmgr.lock HELD, which is the constraint that shapes what it may
 * do. The locking contract at the top of netmgr.c forbids driver callbacks and
 * tal_event_publish() under the lock, and a hook is subject to the same rule for
 * the same reasons. So a hook must be pure arithmetic over @a in: it must not
 * call netmgr_conn_get() or netmgr_conn_set() (self-deadlock on a non-recursive
 * mutex), must not publish, must not block, and must not call any tal_* entry
 * point that might. Everything it could legitimately want is already in @a in,
 * which is why @a in is a snapshot and not a set of accessors.
 *
 * @param[in]  in  the candidates and the context
 * @param[out] out the decision; pre-initialised to
 *                 {NETCONN_AUTO, 0} before the call
 * @param[in]  ctx the pointer handed to netmgr_policy_select_cb_set()
 */
typedef void (*netmgr_policy_select_cb_t)(const netmgr_select_in_t *in, netmgr_select_out_t *out, void *ctx);

/**
 * @brief Replace the built-in ranking.
 *
 * The extension point that lets a board express something the parameters cannot -
 * a signal-strength threshold, a time-of-day preference, a "never use cellular
 * while charging" rule. Passing NULL restores the built-in ranking.
 *
 * A function pointer rather than a Kconfig weak symbol, for the reason
 * netconn_registry.h records about netconn_registry_set_table(): a weak default
 * only loses to a strong definition in an archive member the linker already had
 * cause to pull in, which is exactly the failure that silently reverts a board to
 * defaults.
 *
 * @param[in] cb  the ranking function, or NULL for the built-in one
 * @param[in] ctx opaque, passed back on every call; netmgr never dereferences it
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET netmgr_policy_select_cb_set(netmgr_policy_select_cb_t cb, void *ctx);

/**
 * @brief The built-in ranking, exposed so a hook can delegate to it.
 *
 * A hook that only wants to override one case should not have to reimplement the
 * tie-break rules to handle the rest. Calling this is how it defers, and it is
 * also how netmgr recovers when a hook returns an unusable choice.
 *
 * The rule, in full:
 *
 *   1. discard every link failing NETMGR_LINK_STATE_IS_UP(), and every link
 *      whose eligible_at_ms is still in the future - those two are the
 *      eligibility floor and nothing overrides them, pin included;
 *   2. if in->pinned is eligible, choose it and stop. A pin is an operator
 *      instruction and outranks every automatic consideration below;
 *   3. partition the survivors into two tiers: NOT-SUSPECT
 *      (UNVERIFIED, ONLINE) and SUSPECT (DEGRADED). Any not-suspect link beats
 *      every suspect one. When policy.probe_demote is FALSE there is one tier;
 *   4. within a tier, highest pri wins; equal pri breaks to lowest reg_index.
 *      That is M2's tie-break, unchanged;
 *   5. if the winner is not the current active link, and the active link is
 *      still eligible, and policy.preempt is FALSE, keep the active link -
 *      EXCEPT when the active link is itself SUSPECT and a not-suspect
 *      alternative exists, in which case stickiness is abandoned and the switch
 *      goes ahead. The carve-out is not a refinement, it is what keeps rule 5
 *      from silently disabling the whole refactor: DEGRADED passes the
 *      eligibility floor, so without it a product that set preempt FALSE for
 *      route stability would also lose fail-over - a wifi link on a WAN-less AP
 *      would go DEGRADED, stay active because it is sticky, and the cellular link
 *      it was meant to fall back to would never be selected. The reading of
 *      stickiness this enforces: "do not hop between links that are equally
 *      good", never "ignore that the current link is broken". Gated on
 *      policy.probe_demote, because with demotion off DEGRADED is by definition
 *      not a ranking signal, so it must not break stickiness either - one flag,
 *      one meaning;
 *   6. if the winner is not the current active link, and the active link is
 *      still eligible, and min_dwell_ms has not elapsed since
 *      active_since_ms, keep the active link and set recheck_ms to the
 *      remaining dwell. Note the shared precondition of 5 and 6: an active
 *      link that has stopped being eligible never wins either of them, so
 *      neither stickiness nor dwell can strand the device on a dead link;
 *   7. set recheck_ms to the nearest future eligible_at_ms among the links
 *      discarded by rule 1 for debounce, so a debouncing link is reconsidered
 *      when it ripens.
 *
 * Why UNVERIFIED and ONLINE share a tier is argued at
 * NETMGR_LINK_STATE_UNVERIFIED and it is the subtlest rule here: separating them
 * deadlocks a passive probe.
 *
 * Why the SUSPECT tier is a demotion and not an exclusion: a degraded link is
 * still the best thing available when it is the only thing available. Excluding
 * it would answer NETCONN_AUTO, push src_ip 0, and publish NETMGR_LINK_DOWN for a
 * device that has a working LAN and a temporarily unreachable cloud.
 *
 * @param[in]  in  as netmgr_policy_select_cb_t
 * @param[out] out as netmgr_policy_select_cb_t
 */
void netmgr_policy_select_default(const netmgr_select_in_t *in, netmgr_select_out_t *out);

/**
 * @brief Rank the candidates: the installed hook if there is one, else the
 *        built-in rule - and VALIDATE whatever comes back.
 *
 * The one entry point netmgr.c uses, and the function the note at the top of this
 * header means when it says netmgr_policy_select() "is the one thing in the module
 * that IS allowed to be called with s_netmgr.lock held". It was missing from this
 * header, which left the dispatch unnamed although both halves of it were
 * specified: netmgr_policy_select_cb_set() promises a replaceable ranking and
 * netmgr_select_out_t.choice promises the answer is validated rather than trusted.
 *
 * Defined in netmgr.c and not in netmgr_policy.c, for the reason that file's
 * comment gives about netmgr_policy_select_cb_set(): validating a choice needs the
 * live candidate set, which only netmgr.c has.
 *
 * What is validated, exactly:
 *
 *   - NETCONN_AUTO is accepted at face value. It is a legitimate answer and means
 *     "no link should carry traffic";
 *   - anything else must appear in @a in->links AND satisfy
 *     NETMGR_LINK_STATE_IS_UP(). That is the eligibility floor of rule 1, the one
 *     rule nothing overrides - not the pin, and so not a hook either;
 *   - a choice that fails either test is logged and REPLACED by
 *     netmgr_policy_select_default(), which also discards the hook's recheck_ms,
 *     since a decision netmgr could not honour carries no deadline worth keeping.
 *
 * Debounce is deliberately NOT enforced against a hook: eligible_at_ms is given to
 * the hook as an input and a product ranking is entitled to ignore its own
 * hysteresis. A debouncing link is up, so honouring such a choice cannot route
 * traffic over a dead link, which is the property the validation exists to protect.
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
 * @brief Pin the active link, or release the pin.
 *
 * Backs `netmgr switch <link>` in netmgr_cli.c, which is an explicit "not
 * implemented yet, manual link selection lands in M3" today.
 *
 * Semantics, chosen so the command is honest about what it can and cannot do:
 *
 *   - a pin outranks priority, tiering, stickiness and dwell. An operator asking
 *     for a link has more context than any of them, including the right to use a
 *     link netmgr believes is degraded;
 *   - a pin does NOT override the eligibility floor. Pinning a link that is down
 *     cannot make traffic leave through it, so the pin is REMEMBERED and takes
 *     effect if and when the link comes up. netmgr_policy_pin() reports which of
 *     the two happened so the CLI can say so;
 *   - a pin does not dial. There is no generic "connect" verb in
 *     netmgr_conn_config_type_e and M3 deliberately does not add one, so
 *     `netmgr switch wifi` on a down wifi arms the pin and nothing else - use
 *     `netmgr wifi up <ssid>`. Adding a verb would mean every driver grows an
 *     arm for it and two of the three can only answer OPRT_NOT_SUPPORTED,
 *     because OBSERVE and SUSTAINED links have nothing to dial with;
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
 * @brief Read the internal state of one link.
 *
 * For the CLI and for diagnostics. Deliberately NOT routed through
 * netmgr_conn_get(NETCONN_CMD_STATUS): that command's contract is the public
 * two-valued status and it must keep answering exactly that. A caller wanting the
 * richer value asks for it by name, so no existing caller can receive a value it
 * has never seen.
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
