/**
 * @file netmgr_retry.c
 * @brief Back-off arithmetic, extracted from netconn_wifi.c. See netmgr_retry.h
 *        for why it exists and who the two callers are.
 *
 * This translation unit has no dependency beyond tuya_cloud_types.h: no timer,
 * no mutex, no allocation, no logging, no clock. Every function is a total
 * function of its arguments. That is what lets it be unit-tested on the host
 * with nothing linked in, and it is why NULL is answered with a defined value
 * everywhere instead of with OPRT_INVALID_PARM.
 *
 * Timing equivalence with today's netconn_wifi.c
 * ----------------------------------------------
 * The table below reproduces the CURRENT observable back-off exactly. That claim
 * needs proof, because today's saturation does not come from the table at all.
 *
 * Today, s_netmgr_wifi initialises
 *
 *     .table_size = NETCONN_WIFI_CONN_TABLE,        // 16
 *     .table      = {1, 3, 5, 10, 15, 20},          // 6 entries, [6..15] zero
 *
 * and both back-off sites (netconn_wifi.c:159 in __netconn_wifi_event(), :195 in
 * __netconn_wifi_conn_timer()) run
 *
 *     tal_sw_timer_start(wifi->conn.timer, wifi->conn.table[wifi->conn.count] * 1000, TAL_TIMER_ONCE);
 *     if (wifi->conn.count < wifi->conn.table_size - 1) { wifi->conn.count++; }
 *
 * so the saturation point is attempt 15, not attempt 5, and attempts 6..15 arm
 * the timer with a literal 0. That is not a no-op and it is not a zero-length
 * timer, because tal_sw_timer_start() (src/tal_system/src/tal_sw_timer.c:411)
 * reads
 *
 *     if (time_ms) {
 *         timer->interval = time_ms;
 *     }
 *     timer->expire_time = (uint64_t)secTime * 1000 + (uint64_t)msTime + timer->interval;
 *
 * A zero argument silently REUSES the interval from the previous arm of that
 * timer. The retained value is 20 000 ms, so the observed back-off saturates at
 * 20 s and the sequence really is 1, 3, 5, 10, 15, 20, 20, 20, ...
 *
 * Where the retained 20 000 comes from is the part worth writing down, because
 * netmgr_retry.h's own note gets it slightly wrong. It is NOT the leftover from
 * arming with table[5] * 1000. conn.timer is shared with the connect-attempt
 * timeout, and in the steady failure loop the most recent NON-ZERO arm is always
 * netconn_wifi.c:86 in __netconn_wifi_connect_process():
 *
 *     tal_sw_timer_start(wifi->conn.timer, WIFI_CONN_TIMEOUT_MAX * 1000, TAL_TIMER_ONCE);
 *
 * Trace one cycle at count >= 6: stat WAIT, timer fires -> __netconn_wifi_connect()
 * -> :86 arms 20 000 and stat becomes CHECK -> the attempt resolves either as a
 * failure event (:159) or as a connect timeout (:195), and both arm 0, which
 * retains that 20 000. So the saturated back-off is governed by
 * WIFI_CONN_TIMEOUT_MAX (netconn_wifi.h:73, value 20), and it matches
 * table[5] (value 20) only because two unrelated constants happen to be equal.
 * Change WIFI_CONN_TIMEOUT_MAX to 15 today and the saturated back-off silently
 * becomes 15 s with no edit to the back-off table.
 *
 * netmgr_retry_table_assoc therefore declares count 6, which moves the
 * saturation point to attempt 5 and makes the interval explicit on every arm:
 *
 *     attempt   0   1   2   3   4   5   6   7  ...
 *     today     1   3   5  10  15  20  20* 20*     (* = arm 0, interval retained)
 *     here      1   3   5  10  15  20  20  20      (* clamped to entry[5])
 *
 * Identical timing, no dependency on `time_ms == 0`, and no coupling between the
 * back-off table and the connect timeout. Record this in the M3 changelog as a
 * hardening, not as a behaviour change.
 *
 * NETCONN_CMD_RECONN_TABLE was never affected either way: netconn_wifi.c:574
 * sets table_size to the caller's own entry count, so a product-supplied table
 * always saturated on its real last entry. The long ULP back-off path keeps its
 * behaviour byte for byte.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "netmgr_retry.h"

/***********************************************************
*************************** macro **************************
***********************************************************/

/**
 * @brief Largest interval, in seconds, that still fits a uint32_t once
 *        multiplied by 1000.
 *
 * NETCONN_CMD_RECONN_TABLE clamps the NUMBER of product-supplied entries but not
 * their VALUES (netconn_wifi.c:572-574 only bounds rc->size), so a nonsense
 * entry can reach a table. Clamping here means such an entry produces the
 * longest representable wait instead of wrapping round to a very short one,
 * which is the failure direction that matters: a wrapped interval turns a
 * back-off into a busy retry loop.
 */
#define NETMGR_RETRY_INTERVAL_MAX_S (0xFFFFFFFFU / 1000U)

/***********************************************************
*********************** back-off tables ********************
***********************************************************/

/**
 * @brief The wifi association intervals, in seconds.
 *
 * The literal initialiser from s_netmgr_wifi, unchanged. Six entries, so the
 * declared count and the real length agree - which is exactly the mismatch the
 * file comment above describes removing.
 */
static const uint32_t s_retry_assoc_entry[] = {1, 3, 5, 10, 15, 20};

/**
 * @brief The revalidation intervals, in seconds.
 *
 * New in M3; nothing in the tree had revalidation back-off before, so there is
 * no legacy timing to preserve here.
 */
static const uint32_t s_retry_revalidate_entry[] = {30, 60, 120, 300, 600};

const netmgr_retry_table_t netmgr_retry_table_assoc = {
    .entry = s_retry_assoc_entry,
    .count = (uint32_t)(sizeof(s_retry_assoc_entry) / sizeof(s_retry_assoc_entry[0])),
};

const netmgr_retry_table_t netmgr_retry_table_revalidate = {
    .entry = s_retry_revalidate_entry,
    .count = (uint32_t)(sizeof(s_retry_revalidate_entry) / sizeof(s_retry_revalidate_entry[0])),
};

/***********************************************************
********************* internal helper **********************
***********************************************************/

/**
 * @brief How many entries of a table may actually be read.
 *
 * Folds the three ways a table can be unusable into one number, so every public
 * function can start from "count == 0 means no back-off":
 *
 *   - table itself NULL             -> 0
 *   - table->entry NULL             -> 0, whatever count says. A caller that
 *                                      zeroed a netmgr_retry_t and never called
 *                                      netmgr_retry_bind() lands here;
 *   - count > NETMGR_RETRY_TABLE_MAX -> clamped to NETMGR_RETRY_TABLE_MAX.
 *
 * The last clamp deserves a word. netmgr_retry.h fixes NETMGR_RETRY_TABLE_MAX as
 * the upper bound on entries, so an over-long count is already a contract
 * violation; clamping only ever makes the module read FEWER entries than it was
 * told about, so it can never read past a table that respects the bound, and it
 * turns a garbage count (an uninitialised field, a wild 0xFFFFFFFF) into a
 * bounded read rather than an out-of-bounds one. The cost is that a caller who
 * deliberately passes 20 entries sees entries 16..19 ignored, which is the
 * documented bound being enforced rather than a surprise.
 *
 * Note what CANNOT be checked: whether count agrees with the real length of
 * @a entry. C offers no way to ask, so count is trusted up to the bound. A
 * caller that declares more entries than it allocated gets an out-of-bounds read
 * and that is the caller's bug; sizeof-derived counts, as used for both tables
 * above, make it unrepresentable.
 *
 * @param[in] table the table to measure; NULL is treated as empty
 *
 * @return the number of entries that may be indexed, 0 for "no back-off"
 */
static uint32_t __netmgr_retry_count(const netmgr_retry_table_t *table)
{
    if (NULL == table || NULL == table->entry) {
        return 0;
    }

    return (table->count > NETMGR_RETRY_TABLE_MAX) ? NETMGR_RETRY_TABLE_MAX : table->count;
}

/**
 * @brief Has an armed deadline been reached, in a way that survives wrap-around?
 *
 * The millisecond base netmgr.c uses is a uint32_t and wraps every 49.7 days, so
 * a plain `now_ms >= deadline_ms` would report "not due" for the whole 49.7 days
 * after a deadline armed just before the wrap. The signed-difference idiom
 * answers correctly across the wrap as long as the interval is far below 2^31 ms
 * (24.8 days); the longest interval this module can produce is 600 s from
 * netmgr_retry_table_revalidate, and even a clamped
 * NETMGR_RETRY_INTERVAL_MAX_S entry stays under 2^32 ms, so the only way to
 * break the assumption is a deadline armed more than 24.8 days in the future,
 * which no table here can express.
 *
 * @param[in] deadline_ms the armed deadline; must be non-zero (armed)
 * @param[in] now_ms      current time in the same base
 *
 * @return TRUE when @a now_ms is at or past @a deadline_ms
 */
static BOOL_T __netmgr_retry_reached(uint32_t deadline_ms, uint32_t now_ms)
{
    return ((int32_t)(now_ms - deadline_ms) >= 0) ? TRUE : FALSE;
}

/***********************************************************
************************ pure layer ************************
***********************************************************/

uint32_t netmgr_retry_interval_ms(const netmgr_retry_table_t *table, uint32_t attempt)
{
    uint32_t count   = __netmgr_retry_count(table);
    uint32_t index   = 0;
    uint32_t seconds = 0;

    if (0 == count) {
        return 0;
    }

    /* Clamp at the last entry: "grows to the last entry and then repeats it". */
    index   = (attempt < count) ? attempt : (count - 1);
    seconds = table->entry[index];
    if (seconds > NETMGR_RETRY_INTERVAL_MAX_S) {
        seconds = NETMGR_RETRY_INTERVAL_MAX_S;
    }

    return seconds * 1000U;
}

uint32_t netmgr_retry_advance(const netmgr_retry_table_t *table, uint32_t *attempt)
{
    uint32_t count       = 0;
    uint32_t interval_ms = 0;

    /* Nothing to bump. Still answer the first interval rather than 0, so a
     * caller that only wants the arithmetic is not forced to invent a counter. */
    if (NULL == attempt) {
        return netmgr_retry_interval_ms(table, 0);
    }

    count = __netmgr_retry_count(table);

    /* Index BEFORE bumping, which is the order the wifi driver uses today. */
    interval_ms = netmgr_retry_interval_ms(table, *attempt);

    /* Saturate at count - 1, never wrap: the caller indexes with this counter,
     * so a wrapping counter would restart the back-off from the first entry.
     * `count > 1` also keeps `count - 1` from underflowing on an empty table,
     * where the only sensible saturation point is 0. */
    if (count > 1 && *attempt < (count - 1)) {
        *attempt = *attempt + 1;
    }

    return interval_ms;
}

/***********************************************************
*********************** bundled layer **********************
***********************************************************/

void netmgr_retry_bind(netmgr_retry_t *retry, const netmgr_retry_table_t *table)
{
    if (NULL == retry) {
        return;
    }

    if (NULL == table) {
        retry->table.entry = NULL;
        retry->table.count = 0;
    } else {
        /* Shallow copy on purpose: netmgr_retry_table_t owns no storage and
         * netmgr_retry.h makes the caller responsible for outliving us. */
        retry->table = *table;
    }

    retry->attempt     = 0;
    retry->deadline_ms = 0;
}

void netmgr_retry_reset(netmgr_retry_t *retry)
{
    if (NULL == retry) {
        return;
    }

    retry->attempt     = 0;
    retry->deadline_ms = 0;
}

uint32_t netmgr_retry_fail(netmgr_retry_t *retry, uint32_t now_ms)
{
    uint32_t interval_ms = 0;
    uint32_t deadline_ms = 0;

    if (NULL == retry) {
        return 0;
    }

    interval_ms = netmgr_retry_advance(&retry->table, &retry->attempt);
    deadline_ms = now_ms + interval_ms;

    /* 0 is the not-armed sentinel, so a deadline that lands exactly on 0 - which
     * needs now_ms + interval_ms to wrap onto it, or now_ms == 0 with an empty
     * table - is nudged by 1 ms rather than read back as "never armed". */
    if (0 == deadline_ms) {
        deadline_ms = 1;
    }

    retry->deadline_ms = deadline_ms;

    /* An empty table is armed at `now`, so netmgr_retry_due() answers TRUE on
     * the very next poll. netmgr_retry.h specifies only the RETURN value for
     * this case ("0 ... the caller should treat the retry as due immediately"),
     * and leaving the context unarmed instead would make due() answer FALSE
     * forever - turning "no table" into "never retry", the opposite of the
     * degenerate-build intent that a count-0 table switches back-off OFF. Both
     * the return value and every later poll now say "due now". */
    return (0 == interval_ms) ? 0 : deadline_ms;
}

BOOL_T netmgr_retry_due(const netmgr_retry_t *retry, uint32_t now_ms)
{
    if (NULL == retry || 0 == retry->deadline_ms) {
        return FALSE;
    }

    return __netmgr_retry_reached(retry->deadline_ms, now_ms);
}

uint32_t netmgr_retry_remain_ms(const netmgr_retry_t *retry, uint32_t now_ms)
{
    if (NULL == retry || 0 == retry->deadline_ms) {
        return 0;
    }

    /* Already due contributes no deadline either: the caller acts on it in this
     * same pass, so folding it into the shared minimum would arm a 0 ms timer. */
    if (__netmgr_retry_reached(retry->deadline_ms, now_ms)) {
        return 0;
    }

    /* Not reached, so the unsigned difference is the true positive remainder and
     * cannot be 0 - which is what lets the caller fold with "ignore zero". */
    return retry->deadline_ms - now_ms;
}
