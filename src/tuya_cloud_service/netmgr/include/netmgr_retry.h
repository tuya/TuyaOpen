/**
 * @file netmgr_retry.h
 * @brief Back-off arithmetic, extracted from netconn_wifi.c so more than one
 *        caller can have it.
 *
 * Computes intervals and deadlines; never arms anything. The pure layer
 * (netmgr_retry_interval_ms(), netmgr_retry_advance()) touches no state a
 * caller does not already own, so netconn_wifi.c can adopt it WITHOUT any
 * change to netconn_wifi.h, which is on the public include path. The bundled
 * layer (netmgr_retry_t and its helpers) is for netmgr.c, which has no legacy
 * fields to preserve.
 *
 * Association back-off applies only to NETCONN_CTRL_MANAGED links - see
 * netconn_registry.h for why wired and cellular can't have it. Revalidation
 * back-off applies to all three.
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
 * @brief Upper bound on entries in a back-off table. Matches
 *        NETCONN_WIFI_CONN_TABLE (netconn_wifi.h, 16), so netconn_wifi.c can
 *        pass its existing `conn.table[]` here with no reallocation.
 */
#define NETMGR_RETRY_TABLE_MAX 16

/**
 * @brief A sequence of back-off intervals, in SECONDS - the unit
 *        netmgr_reconn_table_t (netmgr.h) and the wifi driver already use.
 *        The interval accessors below return milliseconds, what
 *        tal_sw_timer_start() takes.
 *
 * @a entry is NOT copied. The caller owns the storage and it must outlive
 * every netmgr_retry_* call made against it - a `static` array or struct
 * field for both in-tree callers.
 */
typedef struct {
    /** Intervals in seconds, ascending by convention but not enforced. */
    const uint32_t *entry;
    /**
     * Number of valid entries in @ref entry. Zero means "no back-off" - every
     * interval computes to 0 - but the two consumers read that differently:
     *
     *   - association (netconn_wifi.c): retry IMMEDIATELY. netmgr_retry_fail()
     *     arms an empty table at `now`, so netmgr_retry_due() answers TRUE on
     *     the next poll;
     *   - revalidation (netmgr.c, netmgr_policy_t.revalidate): NEVER re-verify.
     *     netmgr.c leaves the context unarmed instead of calling
     *     netmgr_retry_fail(), so netmgr_retry_due() answers FALSE forever.
     */
    uint32_t count;
} netmgr_retry_table_t;

/**
 * @brief The wifi association table that netconn_wifi.c ships today.
 *
 * {1, 3, 5, 10, 15, 20}, the literal initialiser in s_netmgr_wifi - declared
 * here so any future MANAGED driver can share it instead of carrying its own
 * magic numbers.
 */
extern const netmgr_retry_table_t netmgr_retry_table_assoc;

/**
 * @brief The revalidation table, used when a link has been marked unreachable.
 *
 * {30, 60, 120, 300, 600}, an order of magnitude slower than the association
 * table: promoting a link back to reachable moves the route, which makes
 * tuya_iot tear down and re-establish its MQTT session, so retrying too fast
 * would put the device in a permanent reconnect loop. The last entry repeats
 * forever.
 */
extern const netmgr_retry_table_t netmgr_retry_table_revalidate;

/***********************************************************
************************ pure layer ************************
***********************************************************/

/**
 * @brief Interval for a given attempt number, in milliseconds.
 *
 * Indexed by @a attempt and CLAMPED at the last entry - "grows to the last
 * entry and then repeats it" in netmgr.h. Deliberately total: an empty or
 * NULL table answers 0 rather than failing, so an unconfigured caller
 * degrades to "retry immediately" instead of having to branch - which is
 * also what turns every back-off in the module off for a count-0 table.
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
 * The counter saturates at count - 1 rather than wrapping: the caller INDEXES
 * with the counter before bumping it, so a wrapping counter would restart the
 * back-off from the first entry every time it fills the table.
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
 * For netmgr.c, which keeps one of these per registered link and evaluates
 * all of them from the single shared deadline. A zeroed instance is valid
 * ("no table, no attempts, not armed"), so it needs no constructor beyond
 * netmgr_retry_bind(). Not thread-safe, and deliberately not made so - the
 * caller is responsible for serializing access.
 */
typedef struct {
    /** Which intervals to use. Bound once by netmgr_retry_bind(). */
    netmgr_retry_table_t table;
    /** Failures so far, saturating at table.count - 1. */
    uint32_t attempt;
    /**
     * Absolute deadline in the same millisecond base netmgr.c passes to
     * netmgr_retry_due(), or 0 for "not armed". Absolute rather than
     * remaining, because the shared scheduler re-evaluates every link on
     * every wake-up and a remaining-time field would have to be decremented
     * by each of them.
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
 *         is empty. An empty table is still ARMED at @a now_ms, so every later
 *         netmgr_retry_due() poll answers "due now" rather than "never" - the
 *         revalidation consumer, for which count 0 means NEVER (see
 *         netmgr_retry_table_t.count), must not call this at all and leave the
 *         context unarmed instead.
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
 * netmgr.c folds this into the minimum it arms its one tal_sw_timer with:
 *
 *   - not armed, or armed and already due -> 0 either way, because "no
 *     deadline to contribute" and "the caller acts on it this very pass"
 *     both fold the same way;
 *   - armed and future -> the remaining milliseconds, never 0.
 *
 * So a caller folds with "ignore zero, take the minimum of the rest" and
 * cannot accidentally arm a zero-length timer.
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
