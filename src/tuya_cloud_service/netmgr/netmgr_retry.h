/**
 * @file netmgr_retry.h
 * @brief Back-off arithmetic, extracted from netconn_wifi.c so more than one
 *        caller can have it.
 *
 * What is being extracted
 * -----------------------
 * netconn_wifi.c carries the only back-off state machine in the module, spread
 * over four fields of netconn_wifi_conn_t (`stat`, `count`, `table`,
 * `table_size`) and three functions that all mutate them: __netconn_wifi_event(),
 * __netconn_wifi_conn_timer() and __netconn_wifi_connect_process(). The same
 * eight lines appear twice, once in the event handler and once in the timer
 * handler:
 *
 *     tal_sw_timer_start(wifi->conn.timer, wifi->conn.table[wifi->conn.count] * 1000, TAL_TIMER_ONCE);
 *     if (wifi->conn.count < wifi->conn.table_size - 1) {
 *         wifi->conn.count++;
 *     }
 *     wifi->conn.stat = NETCONN_WIFI_CONN_WAIT;
 *
 * Nothing else in netmgr has any back-off at all. netconn_wired.c and
 * netconn_cellular.c have none, and cannot be given association back-off (see
 * "What this does NOT solve" below), but they do need REVALIDATION back-off once
 * netmgr_probe.h can mark a link as unreachable - a link that failed its
 * reachability check must not be retried immediately or forever.
 *
 * Shape: no timer, no thread, no global state
 * ------------------------------------------
 * This module computes intervals and deadlines. It never arms anything. That is
 * deliberate and it is what lets the same code serve two callers whose timing
 * mechanisms are unrelated:
 *
 *   - netconn_wifi.c keeps its own tal_sw_timer (conn.timer). Its back-off is
 *     interleaved with a connect-attempt timeout on the same timer, which is
 *     driver business and stays there;
 *   - netmgr.c drives revalidation off the single shared deadline the whole
 *     module now uses (see the deadline scheduler note in netmgr_policy.h), so
 *     it needs the NEXT DEADLINE as a number, not a timer of its own.
 *
 * Two layers are offered for exactly that reason. The pure layer
 * (netmgr_retry_interval_ms(), netmgr_retry_advance()) touches no state a caller
 * does not already own, so netconn_wifi.c can adopt it WITHOUT any change to
 * netconn_wifi.h - which matters, because that header is on the global public
 * include path and app code uses its types. The bundled layer
 * (netmgr_retry_t and its helpers) is for netmgr.c, which has no legacy fields
 * to preserve.
 *
 * What this does NOT solve
 * ------------------------
 * "Shared by all three links" is only true for revalidation, not for
 * association. netconn_registry.h's control levels say why, and they are not
 * negotiable from here:
 *
 *   - NETCONN_CTRL_OBSERVE (wired): tal_wired.h has no connect and no
 *     disconnect. There is no attempt to retry. A back-off module cannot invent
 *     one;
 *   - NETCONN_CTRL_SUSTAINED (cellular): tal_cellular.h has tal_cellular_init()
 *     and no deinit, no connect/disconnect pair. Calling tal_cellular_init() a
 *     second time is not a documented re-dial and must not be used as one;
 *   - NETCONN_CTRL_MANAGED (wifi): the only link with an attempt to retry.
 *
 * So association back-off has exactly one user today and will have more only
 * when a TAL grows the verbs. Revalidation back-off has three users immediately.
 * State that plainly rather than shipping two empty adapters.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __NETMGR_RETRY_H__
#define __NETMGR_RETRY_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
*********************** back-off table *********************
***********************************************************/

/**
 * @brief Upper bound on entries in a back-off table.
 *
 * Matches NETCONN_WIFI_CONN_TABLE, which netconn_wifi.h already fixes at 16 and
 * which NETCONN_CMD_RECONN_TABLE already clamps user input to. Keeping the two
 * equal is what lets netconn_wifi.c pass a pointer to its existing
 * `conn.table[]` here with no reallocation and no header change.
 */
#define NETMGR_RETRY_TABLE_MAX 16

/**
 * @brief A sequence of back-off intervals, in SECONDS.
 *
 * Seconds, not milliseconds, because that is the unit netmgr_reconn_table_t
 * already uses in netmgr.h and the unit the wifi driver already stores. A
 * conversion here would put two units in one module for no gain; the interval
 * accessors below return milliseconds because that is what tal_sw_timer_start()
 * takes.
 *
 * @a entry is NOT copied. The caller owns the storage and it must outlive every
 * netmgr_retry_* call made against it - which for both in-tree callers is a
 * `static` array or a field of a static struct.
 */
typedef struct {
    /** Intervals in seconds, ascending by convention but not enforced. */
    const uint32_t *entry;
    /** Number of valid entries in @ref entry. Zero means "no back-off". */
    uint32_t count;
} netmgr_retry_table_t;

/**
 * @brief The wifi association table that netconn_wifi.c ships today.
 *
 * {1, 3, 5, 10, 15, 20} - the literal initialiser in s_netmgr_wifi. Declared
 * here so the driver and any future MANAGED driver share one definition instead
 * of each carrying its own magic numbers, and so a reviewer can see that
 * adopting this module changes no timing.
 */
extern const netmgr_retry_table_t netmgr_retry_table_assoc;

/**
 * @brief The revalidation table, used when a link has been marked unreachable.
 *
 * {30, 60, 120, 300, 600}. Deliberately an order of magnitude slower than the
 * association table, because a revalidation attempt is expensive in a way an
 * association attempt is not: promoting a link back to
 * NETMGR_LINK_STATE_UNVERIFIED makes it win selection again, which moves the
 * route, which makes tuya_iot tear down and re-establish its MQTT session (see
 * __tuya_iot_link_type_change_cb() calling tuya_iot_reconnect()). A one-second
 * revalidation would put the device in a permanent reconnect loop.
 *
 * The last entry repeats forever, so a link whose network never comes back is
 * retried every ten minutes and no more often.
 */
extern const netmgr_retry_table_t netmgr_retry_table_revalidate;

/***********************************************************
************************ pure layer ************************
***********************************************************/

/**
 * @brief Interval for a given attempt number, in milliseconds.
 *
 * The table is indexed by @a attempt and CLAMPED at the last entry, which is
 * what "grows to the last entry and then repeats it" in netmgr.h means. So
 * attempt 0 gets entry[0], attempt 99 gets entry[count - 1].
 *
 * Deliberately total: an empty or NULL table answers 0 rather than failing, so a
 * caller that has not been configured degrades to "retry immediately" instead of
 * having to branch. That is also what makes the M4 degenerate build trivial - a
 * table of count 0 turns every back-off in the module off.
 *
 * @param[in] table   the table to read; NULL is treated as empty
 * @param[in] attempt zero-based attempt number
 *
 * @return the interval in milliseconds, or 0 when there is no table
 */
uint32_t netmgr_retry_interval_ms(const netmgr_retry_table_t *table, uint32_t attempt);

/**
 * @brief Bump an attempt counter the way the wifi driver bumps it today.
 *
 * The counter saturates at count - 1 rather than wrapping, which is the exact
 * behaviour of the `if (count < table_size - 1) count++;` idiom this replaces.
 * Preserving the saturation matters: the wifi driver INDEXES with the counter
 * before bumping it, so a wrapping counter would restart the back-off from one
 * second every sixteen failures.
 *
 * One latent hazard in the original is worth recording, because adopting this
 * module removes it and a reviewer should know that is not an accident.
 * s_netmgr_wifi initialises `table_size = NETCONN_WIFI_CONN_TABLE` (16) while
 * giving only six entries, so table[6..15] are zero and the driver's saturation
 * point is attempt 15, not attempt 5. Attempts 6 and up therefore call
 *
 *     tal_sw_timer_start(wifi->conn.timer, 0, TAL_TIMER_ONCE);
 *
 * and that only behaves because tal_sw_timer_start() reads
 * `if (time_ms) { timer->interval = time_ms; }` - a zero argument silently keeps
 * the interval from the previous arm, which is 20 s. So the observable back-off
 * is correct by accident, and it is load-bearing on a TAL quirk that reads like
 * an oversight. netmgr_retry_table_assoc has count 6, so the saturation point
 * becomes attempt 5 and the interval is passed explicitly every time. Same
 * timing, no dependency on the quirk. Note this in the M3 changelog as a
 * hardening, not as a behaviour change.
 *
 * NETCONN_CMD_RECONN_TABLE is unaffected either way: it already sets table_size
 * to the caller's count, so a product-supplied table never had the mismatch.
 *
 * @param[in]     table   the table whose length bounds the counter
 * @param[in,out] attempt bumped in place, saturating at count - 1
 *
 * @return the interval in milliseconds for the attempt number BEFORE the bump,
 *         so a caller can arm and advance in one call
 */
uint32_t netmgr_retry_advance(const netmgr_retry_table_t *table, uint32_t *attempt);

/***********************************************************
*********************** bundled layer **********************
***********************************************************/

/**
 * @brief Table, attempt counter and absolute deadline in one place.
 *
 * For netmgr.c, which keeps one of these per registered link and evaluates all
 * of them from the single shared deadline. A zeroed instance is valid and means
 * "no table, no attempts, not armed", so it needs no constructor beyond
 * netmgr_retry_bind().
 *
 * Not thread-safe and deliberately not made so. Every instance netmgr.c owns
 * lives inside s_netmgr and is therefore covered by s_netmgr.lock; the arithmetic
 * here is short and non-blocking, so it is one of the few things that IS allowed
 * to run under that lock (contrast conn->get(), which is not).
 */
typedef struct {
    /** Which intervals to use. Bound once by netmgr_retry_bind(). */
    netmgr_retry_table_t table;
    /** Failures so far, saturating at table.count - 1. */
    uint32_t attempt;
    /**
     * Absolute deadline in the same millisecond base netmgr.c passes to
     * netmgr_retry_due(), or 0 for "not armed".
     *
     * Absolute rather than remaining, because the shared deadline scheduler
     * re-evaluates every link on every wake-up and a remaining-time field would
     * have to be decremented by each of them.
     */
    uint32_t deadline_ms;
} netmgr_retry_t;

/**
 * @brief Point a context at a table and clear its counters.
 *
 * @param[out] retry the context to initialise; NULL is a no-op
 * @param[in]  table the table to use; NULL means "no back-off"
 */
void netmgr_retry_bind(netmgr_retry_t *retry, const netmgr_retry_table_t *table);

/**
 * @brief Forget every failure: attempt 0, not armed.
 *
 * Called when a link becomes reachable again, so the next failure starts from
 * the top of the table rather than from wherever the last streak ended.
 *
 * @param[in,out] retry the context to reset; NULL is a no-op
 */
void netmgr_retry_reset(netmgr_retry_t *retry);

/**
 * @brief Record a failure and arm the next deadline.
 *
 * @param[in,out] retry  the context; NULL is a no-op
 * @param[in]     now_ms current time in the caller's millisecond base
 *
 * @return the deadline that was armed, or 0 when @a retry is NULL or its table
 *         is empty - in which case the caller should treat the retry as due
 *         immediately, matching netmgr_retry_interval_ms()'s total contract
 */
uint32_t netmgr_retry_fail(netmgr_retry_t *retry, uint32_t now_ms);

/**
 * @brief Has the armed deadline passed?
 *
 * @param[in] retry  the context; NULL answers FALSE
 * @param[in] now_ms current time in the caller's millisecond base
 *
 * @return TRUE when armed and due, FALSE when not armed or not yet due
 */
BOOL_T netmgr_retry_due(const netmgr_retry_t *retry, uint32_t now_ms);

/**
 * @brief Milliseconds until the armed deadline, for the shared scheduler.
 *
 * This is the value netmgr.c folds into the minimum it arms its one
 * tal_sw_timer with, so the contract at the boundary matters:
 *
 *   - not armed        -> 0, meaning "contributes no deadline"
 *   - armed and due    -> 0, meaning the same thing, because the caller is about
 *                         to act on it in this very pass
 *   - armed and future -> the remaining milliseconds, never 0
 *
 * A caller therefore folds with "ignore zero, take the minimum of the rest",
 * and cannot accidentally arm a zero-length timer.
 *
 * @param[in] retry  the context; NULL answers 0
 * @param[in] now_ms current time in the caller's millisecond base
 *
 * @return remaining milliseconds, or 0 per the rules above
 */
uint32_t netmgr_retry_remain_ms(const netmgr_retry_t *retry, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* __NETMGR_RETRY_H__ */
