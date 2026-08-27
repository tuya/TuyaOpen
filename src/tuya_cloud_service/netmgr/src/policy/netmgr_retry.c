/**
 * @file netmgr_retry.c
 * @brief Back-off arithmetic, extracted from netconn_wifi.c. See netmgr_retry.h
 *        for why it exists and who the two callers are.
 *
 * No dependency beyond tuya_cloud_types.h: no timer, no mutex, no allocation,
 * no logging, no clock. Every function is total, which is what lets it be
 * unit-tested on the host with nothing linked in, and why NULL is answered
 * with a defined value everywhere instead of OPRT_INVALID_PARM.
 *
 * netmgr_retry_table_assoc reproduces netconn_wifi.c's pre-extraction
 * sequence (1, 3, 5, 10, 15, 20, 20, ...) exactly; see 2eae2654 for the
 * analysis this replaced.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "netmgr_retry.h"

/***********************************************************
*************************** macro **************************
***********************************************************/

/**
 * @brief Largest interval, in seconds, that keeps __netmgr_retry_reached()'s
 *        signed-difference idiom correct (see its [-2^31, 2^31) bound below).
 *        A product-supplied table's entry count is bounded but its values are
 *        never validated, so a nonsense entry is clamped here.
 */
#define NETMGR_RETRY_INTERVAL_MAX_S (0x7FFFFFFFU / 1000U)

/***********************************************************
*********************** back-off tables ********************
***********************************************************/

/**
 * @brief The wifi association intervals, in seconds. The literal initialiser
 *        from s_netmgr_wifi, unchanged - six entries, so the declared count
 *        and the real length agree.
 */
static const uint32_t s_retry_assoc_entry[] = {1, 3, 5, 10, 15, 20};

/** @brief The revalidation intervals, in seconds. No legacy timing to preserve. */
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
 * Folds three ways a table can be unusable into one number: table NULL,
 * table->entry NULL (an unbound netmgr_retry_t lands here), or count over
 * NETMGR_RETRY_TABLE_MAX - clamped down rather than trusted, so a garbage
 * count reads as a bounded, if truncated, table instead of an out-of-bounds
 * one.
 *
 * What can't be checked: whether count agrees with the real length of
 * @a entry. A caller that declares more entries than it allocated gets an
 * out-of-bounds read - that is the caller's bug; sizeof-derived counts, as
 * used for both tables above, make it unrepresentable.
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
 * The millisecond base wraps every 49.7 days, so a plain `now_ms >=
 * deadline_ms` misreads for the whole 49.7 days after a deadline armed just
 * before the wrap. Reading `now_ms - deadline_ms` as a two's-complement
 * int32_t instead is correct only while the true difference stays inside
 * [-2^31, 2^31) ms - past that bound before the deadline an over-long
 * interval reads as already due, and past it after the deadline one left
 * unpolled for 2^31 ms (24.8 days) reads as not yet due again.
 * NETMGR_RETRY_INTERVAL_MAX_S keeps every interval this module arms inside
 * the near-side bound.
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

    /* Arming at `now` (rather than leaving the context unarmed) is what makes
     * every later netmgr_retry_due() poll answer TRUE too, not just this call's
     * return value - "no table" means "retry immediately", not "never". */
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
