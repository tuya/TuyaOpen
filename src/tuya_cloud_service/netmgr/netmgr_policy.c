/**
 * @file netmgr_policy.c
 * @brief The policy layer: parameter storage, the manual pin, and the built-in
 *        ranking that replaces __get_active_conn().
 *
 * Scope of this translation unit
 * ------------------------------
 * netmgr_policy.h describes a module with "no timer, no lock, no driver access,
 * and no state except the parameters and the pin". This file is exactly that and
 * nothing more:
 *
 *   - @ref s_policy      the installed netmgr_policy_t;
 *   - @ref s_pin         the manual override;
 *   - netmgr_policy_select_default(), a pure function of its arguments.
 *
 * Two declarations in netmgr_policy.h are deliberately NOT defined here, because
 * both need state this module is forbidden to hold:
 *
 *   - netmgr_link_state_get() reads the per-link state machine and its own
 *     contract says it "takes and releases s_netmgr.lock", so it belongs to
 *     netmgr.c, which owns both;
 *   - netmgr_policy_select_cb_set() stores the replacement ranking hook. The
 *     hook is invoked under s_netmgr.lock and its return value has to be
 *     VALIDATED against the live candidate set before it is honoured, so the
 *     pointer lives with the code that does both. It is also, pointedly, absent
 *     from the header's own list of this module's state.
 *
 * Why there is no lock in here
 * ----------------------------
 * netmgr_policy_get() and netmgr_policy_pin_get() are the two entry points
 * netmgr.c calls while it is BUILDING netmgr_select_in_t, which it does with
 * s_netmgr.lock held. If either took a lock of its own there would be a second
 * lock in the netmgr call graph, ordered against the first, for the sake of two
 * struct copies. So they are lock-free by construction and safe to call from
 * under s_netmgr.lock - that property is load-bearing, not incidental.
 *
 * The cost is a torn read if netmgr_policy_set() runs concurrently with a
 * reselect on another thread: netmgr_policy_t is larger than a word, so the copy
 * is not atomic. That is accepted rather than overlooked. Installing a policy is
 * a control-plane action - a board does it once at start-up, or an app does it
 * when provisioning finishes - and the worst outcome is one reselect that mixes
 * an old timing with a new one, after which netmgr.c reselects again on the next
 * event with the settled value. The pin is a single enum and its store is
 * naturally atomic on every architecture this SDK targets, so the pin has no
 * such window.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "netmgr_policy.h"
#include "netmgr.h"
#include "netconn_registry.h"

/* For netmgr_reselect_request(), the one thing this module needs from netmgr.c
 * beyond netmgr_link_state_get(). It is the seam that lets netmgr_policy_set() and
 * netmgr_policy_pin() keep the halves of their own contracts that say a change
 * "takes effect at the next reselect, which netmgr schedules immediately" -
 * without this module owning a timer or a work queue, which the header forbids. */
#include "netmgr_priv.h"

#include "tal_api.h"

/***********************************************************
************************** state ***************************
***********************************************************/

/** The defaults, kept as an object so netmgr_policy_set(NULL) can restore them. */
static const netmgr_policy_t c_policy_default = NETMGR_POLICY_DEFAULT_INIT;

/** The policy in force. Initialised to the defaults so a caller that never calls
 * netmgr_policy_set() still gets the documented behaviour. */
static netmgr_policy_t s_policy = NETMGR_POLICY_DEFAULT_INIT;

/** The manual override, NETCONN_AUTO when nothing is pinned. */
static netmgr_type_e s_pin = NETCONN_AUTO;

/***********************************************************
********************** ranking helpers *********************
***********************************************************/

/**
 * @brief How long until @a view clears its up-debounce; 0 means eligible now.
 *
 * netmgr_link_view_t.eligible_at_ms shares netmgr_select_in_t.now_ms's base,
 * which is a 32-bit monotonic millisecond counter and therefore wraps every ~49
 * days. Comparing the two with `<` would, for the one tick either side of the
 * wrap, declare a ripe link unripe by 49 days and arm the shared timer for that
 * long. The signed difference is wrap-correct for any real interval, so it is
 * what is used here and in @ref __dwell_remain_ms.
 */
static uint32_t __eligible_in_ms(const netmgr_link_view_t *view, uint32_t now_ms)
{
    int32_t remain = 0;

    /* Documented sentinel: netmgr.c writes 0 when there is no debounce to wait
     * out, which is every link when up_debounce_ms is 0. */
    if (0 == view->eligible_at_ms) {
        return 0;
    }

    remain = (int32_t)(view->eligible_at_ms - now_ms);

    return (remain > 0) ? (uint32_t)remain : 0;
}

/**
 * @brief How much of min_dwell_ms is left, 0 when it has elapsed.
 *
 * active_since_ms is never in the future - netmgr.c sets it to now_ms when there
 * is no active link - so the unsigned difference is the elapsed time and is
 * correct across the counter wrap without a signed cast.
 */
static uint32_t __dwell_remain_ms(const netmgr_select_in_t *in)
{
    uint32_t elapsed = (uint32_t)(in->now_ms - in->active_since_ms);

    if (elapsed >= in->policy.min_dwell_ms) {
        return 0;
    }

    return in->policy.min_dwell_ms - elapsed;
}

/**
 * @brief Rule 4: does @a cand rank strictly above @a best?
 *
 * Highest pri wins; equal pri breaks to the lowest reg_index. The tie-break is
 * compared explicitly rather than left to the iteration order of
 * netmgr_select_in_t.links, even though that array is documented to be in
 * registration order - the rule is the contract, the array order is an input, and
 * a ranking that depends on both silently changes meaning if either moves.
 *
 * @a best NULL means "nothing chosen yet", so anything beats it.
 */
static BOOL_T __ranks_above(const netmgr_link_view_t *cand, const netmgr_link_view_t *best)
{
    if (NULL == best) {
        return TRUE;
    }

    if (cand->pri != best->pri) {
        return (cand->pri > best->pri) ? TRUE : FALSE;
    }

    return (cand->reg_index < best->reg_index) ? TRUE : FALSE;
}

/**
 * @brief Fold one pending deadline into netmgr_select_out_t.recheck_ms.
 *
 * netmgr.c arms ONE shared timer at the nearest deadline across every link, so
 * this module's own deadlines have to collapse the same way: the nearest
 * non-zero wins, and 0 keeps meaning "no deadline of my own" rather than
 * "immediately".
 */
static void __recheck_fold(uint32_t *recheck_ms, uint32_t deadline_ms)
{
    if (0 == deadline_ms) {
        return;
    }

    if (0 == *recheck_ms || deadline_ms < *recheck_ms) {
        *recheck_ms = deadline_ms;
    }
}

/***********************************************************
********************* the built-in rule ********************
***********************************************************/

void netmgr_policy_select_default(const netmgr_select_in_t *in, netmgr_select_out_t *out)
{
    const netmgr_link_view_t *best_clean   = NULL; /* rule 3: the NOT-SUSPECT tier */
    const netmgr_link_view_t *best_suspect = NULL; /* rule 3: the SUSPECT tier */
    const netmgr_link_view_t *pinned       = NULL; /* rule 2, only if it passed rule 1 */
    const netmgr_link_view_t *active       = NULL; /* rules 5 and 6, only if still eligible */
    const netmgr_link_view_t *winner       = NULL;
    uint32_t                  i            = 0;

    if (NULL == out) {
        return;
    }

    /* The header promises the caller pre-initialises this, but the function is
     * also the documented fallback when a hook misbehaves, so it does not rely on
     * having been called through that path. */
    out->choice     = NETCONN_AUTO;
    out->recheck_ms = 0;

    if (NULL == in || 0 == in->count || NULL == in->links) {
        return;
    }

    /* One pass covers rules 1, 3, 4 and 7: rule 1 discards, rule 7 remembers what
     * rule 1 discarded for debounce, and rules 3 and 4 rank what survives. */
    for (i = 0; i < in->count; i++) {
        const netmgr_link_view_t *view = &in->links[i];
        uint32_t                  wait = 0;

        /* Rule 1, first half: the eligibility floor. Nothing below overrides it,
         * the pin included. NETMGR_LINK_STATE_IS_UP() is the single definition of
         * the floor and DEGRADED passes it - a demotion, not an exclusion. */
        if (!NETMGR_LINK_STATE_IS_UP(view->state)) {
            continue;
        }

        /* Rule 1, second half: a link still serving out its up-debounce is not a
         * candidate yet. Rule 7: it is the reason to come back. */
        wait = __eligible_in_ms(view, in->now_ms);
        if (0 != wait) {
            __recheck_fold(&out->recheck_ms, wait);
            continue;
        }

        if (NETCONN_AUTO != in->pinned && in->pinned == view->type) {
            pinned = view;
        }

        if (NETCONN_AUTO != in->active && in->active == view->type) {
            active = view;
        }

        /* Rule 3: two tiers, collapsed to one when probe_demote is FALSE.
         * NETMGR_LINK_STATE_IS_SUSPECT() is DEGRADED and only DEGRADED, which is
         * unreachable with probe_enable FALSE - that is what makes the tiering
         * invisible on a build that does not probe. UNVERIFIED shares the clean
         * tier with ONLINE on purpose: a passive probe can only ever judge the
         * ACTIVE link, so ranking UNVERIFIED below ONLINE would leave a
         * higher-priority link waiting forever for a verdict it can only earn by
         * being selected. */
        if (in->policy.probe_demote && NETMGR_LINK_STATE_IS_SUSPECT(view->state)) {
            if (__ranks_above(view, best_suspect)) {
                best_suspect = view;
            }
        } else {
            if (__ranks_above(view, best_clean)) {
                best_clean = view;
            }
        }
    }

    /* Rule 2: an operator instruction outranks every automatic consideration
     * below, including the tiering - the operator has the right to use a link
     * netmgr believes is degraded. It does NOT outrank rule 1, which is why the
     * pin is only recorded for a view that already passed the floor: a pin on a
     * down link stays armed and does nothing, and a pin on a debouncing link
     * takes effect at the recheck rule 7 just scheduled. */
    if (NULL != pinned) {
        out->choice = pinned->type;
        return;
    }

    /* Rule 3, concluded: any not-suspect link beats every suspect one. Falling
     * through to the suspect tier is what keeps a lone DEGRADED link selected
     * instead of answering NETCONN_AUTO, pushing src_ip 0 and publishing
     * NETMGR_LINK_DOWN for a device whose LAN works and whose cloud is briefly
     * unreachable. */
    winner = (NULL != best_clean) ? best_clean : best_suspect;
    if (NULL == winner) {
        /* No link can carry traffic. A legitimate answer, and the one
         * s_netmgr.active already holds when nothing is up. */
        return;
    }

    /* Rules 5 and 6 share a precondition: the active link must still be eligible.
     * `active` is non-NULL only for a link that passed rule 1, so an active link
     * that has stopped being eligible cannot win either rule, and neither
     * stickiness nor dwell can strand the device on a dead link. */
    if (NULL != active && winner->type != active->type) {
        /* Stickiness does not survive a demotion.
         *
         * Rule 5 as written has one precondition - the active link is still
         * eligible - and DEGRADED is eligible, so a product that sets preempt
         * FALSE for route stability would also, silently, lose fail-over
         * altogether: a wifi link on a WAN-less AP goes DEGRADED, stays active
         * because it is sticky, and the cellular link it was meant to fall back to
         * is never selected. That is the headline defect this whole refactor
         * exists to fix, turned off by an unrelated-looking knob. So it is carved
         * out here rather than left to a warning in the header.
         *
         * The reading of stickiness this enforces: "do not hop between links that
         * are equally good", not "ignore that the current link is broken".
         *
         * Gated on probe_demote for a reason. With demotion off, DEGRADED is not
         * evidence for RANKING - the header says verdicts are still accumulated
         * and still visible, they just do not order the candidates - so it must
         * not be evidence for breaking stickiness either. One flag, one meaning.
         *
         * `NULL != best_clean` is the "and there is something better to move to"
         * half, and it is exactly equivalent to `winner` being non-suspect: winner
         * is best_clean whenever best_clean exists. Written as best_clean because
         * that is the question being asked. */
        BOOL_T active_demoted = FALSE;

        if (in->policy.probe_demote && NETMGR_LINK_STATE_IS_SUSPECT(active->state) && NULL != best_clean) {
            active_demoted = TRUE;
        }

        /* Rule 5: sticky. No deadline - there is nothing to wait for, the active
         * link keeps the route until it stops being eligible. */
        if (!in->policy.preempt && !active_demoted) {
            winner = active;
        } else {
            /* Rule 6: hold the switch until the dwell expires, and come back when
             * it does. Folded rather than assigned, so a nearer debounce deadline
             * from rule 7 still wins the shared timer.
             *
             * Deliberately AFTER the carve-out and not exempted by it, so the
             * order is: rule 5 may be skipped because the active link is
             * suspect, and rule 6 then still applies to the switch that skip
             * allows. Dwell guards against flapping; the tiers judge whether a
             * link works. Two independent concerns, so a demotion is not licence
             * to move the route inside a dwell window - it only means the move
             * will happen once the window closes, instead of never. */
            uint32_t dwell = __dwell_remain_ms(in);

            if (0 != dwell) {
                __recheck_fold(&out->recheck_ms, dwell);
                winner = active;
            }
        }
    }

    out->choice = winner->type;
}

/***********************************************************
************************ parameters ************************
***********************************************************/

OPERATE_RET netmgr_policy_get(netmgr_policy_t *policy)
{
    if (NULL == policy) {
        return OPRT_INVALID_PARM;
    }

    *policy = s_policy;

    return OPRT_OK;
}

OPERATE_RET netmgr_policy_set(const netmgr_policy_t *policy)
{
    /* Stored verbatim. The header's per-field tolerances - "probe_bad_threshold 0
     * is treated as 1", "verify_timeout_ms 0 disables it", "revalidate count 0
     * means never re-verify" - are rules for the CONSUMER of the value, not for
     * the store, and netmgr_policy_get() has to return what was installed or a
     * caller cannot round-trip its own settings. */
    s_policy = (NULL != policy) ? *policy : c_policy_default;

    /* And ask for the reselect the header promises: "a change takes effect at the
     * next reselect, which netmgr schedules immediately, so the caller does not
     * have to wait for a link event".
     *
     * An earlier draft of this file said the promise belonged entirely to netmgr.c
     * and left it unimplemented, on the grounds that scheduling means reaching
     * into a timer and work queue this module has no access to. The first half was
     * right and the conclusion was not: netmgr_reselect_request() is exactly that
     * access, narrowed to one call that cannot block, cannot run the state machine
     * on this thread and cannot re-enter this module. Without it a new policy took
     * effect only at the next unrelated link event, which on a stable device can be
     * hours.
     *
     * NETMGR_CHG_REASON_POLICY is passed so the pass can NAME the cause, and netmgr
     * reports it only if the new ranking actually picks differently - installing
     * the same values a board already had publishes nothing. That reason was
     * unreachable until this call existed.
     *
     * Return value ignored on purpose: the policy IS installed either way, and a
     * work queue that refuses the post only delays the reselect to the next event,
     * which is the behaviour this call improves on rather than depends on. */
    (void)netmgr_reselect_request(NETMGR_CHG_REASON_POLICY);

    return OPRT_OK;
}

/***********************************************************
********************** manual override *********************
***********************************************************/

OPERATE_RET netmgr_policy_pin(netmgr_type_e type)
{
    netmgr_link_state_e state = NETMGR_LINK_STATE_DOWN;

    /* Release. Always succeeds: there is nothing to look up and nothing that can
     * fail.
     *
     * The reselect request matters as much here as on the arm path: releasing a pin
     * hands the choice back to the automatic ranking, and until a pass runs the
     * device stays on the link the pin was holding. Requested with
     * NETMGR_CHG_REASON_NONE because the pass detects a moved pin by comparison and
     * names UNPINNED itself - it knows which link the pin left, and this function
     * has already forgotten.
     *
     * This is also the path netmgr_deinit() takes to clear the pin. It is safe:
     * netmgr_reselect_request() checks the teardown gate first and drops the
     * request, so releasing the pin during teardown queues nothing. */
    if (NETCONN_AUTO == type) {
        s_pin = NETCONN_AUTO;
        (void)netmgr_reselect_request(NETMGR_CHG_REASON_NONE);
        return OPRT_OK;
    }

    /* "not registered in this build" is a question about the registry, and
     * netconn_registry_find() is the lookup that answers it - it rejects
     * NETCONN_AUTO itself and returns NULL for a type this image has no
     * descriptor for. Asking the registry rather than netmgr's live list also
     * means a pin can be armed before netmgr_init() has registered anything. */
    if (NULL == netconn_registry_find(type)) {
        PR_ERR("netmgr pin failed, %s is not registered", NETMGR_TYPE_TO_STR(type));
        return OPRT_NOT_FOUND;
    }

    /* Arm, then ask for a pass, then report. The pin is what the caller asked for
     * and it is set either way; the return value only distinguishes "in effect
     * now" from "remembered until the link comes up".
     *
     * The order is what makes the pin take effect promptly: arming before
     * requesting means the pass cannot observe the old pin, and requesting at all
     * is what stops `netmgr switch wifi` from arming a pin that does nothing until
     * some unrelated link event happens to fire - which reads as a broken command.
     *
     * NETMGR_CHG_REASON_NONE, not _PINNED: the pass compares the live pin against
     * the one it saw last time and names PINNED with the pinned link as the
     * subject. Passing the reason from here would duplicate that and get the
     * subject wrong, because netmgr_reselect_request() reports NETCONN_AUTO.
     *
     * An earlier draft had netmgr_link_state_get() post the pass as a side effect
     * of the query below. This is the same effect, asked for explicitly, and it
     * leaves that function the pure read its own contract describes. */
    s_pin = type;
    (void)netmgr_reselect_request(NETMGR_CHG_REASON_NONE);

    /* The eligibility floor, asked of the link state machine. netmgr_link_state_get()
     * takes s_netmgr.lock, so this call is the reason netmgr_policy_pin() must not
     * itself be called with that lock held - it is a control-plane entry point
     * from the CLI or the app, never from inside a reselect. */
    if (OPRT_OK != netmgr_link_state_get(type, &state)) {
        return OPRT_RESOURCE_NOT_READY;
    }

    /* Only the state half of rule 1 is checked here. A link inside its
     * up-debounce window is reported ready, because eligible_at_ms is per-pass
     * state that netmgr.c computes while building a snapshot and there is no
     * snapshot at this point. The distinction is cosmetic - it changes what the
     * CLI prints, not what the pin does, and the pin takes effect at the recheck
     * rule 7 schedules either way. */
    return NETMGR_LINK_STATE_IS_UP(state) ? OPRT_OK : OPRT_RESOURCE_NOT_READY;
}

OPERATE_RET netmgr_policy_pin_get(netmgr_type_e *type)
{
    if (NULL == type) {
        return OPRT_INVALID_PARM;
    }

    *type = s_pin;

    return OPRT_OK;
}
