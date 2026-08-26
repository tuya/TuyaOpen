/**
 * @file test_netmgr_retry.c
 * @brief Host unit test for netmgr_retry.c, transcribing the contract that
 *        src/tuya_cloud_service/netmgr/include/netmgr_retry.h documents into
 *        assertions.
 *
 * Every CHECK_* below cites the piece of the header (or, where the header is
 * silent, the .c comment) it is transcribing, so a failing assertion points
 * back at the sentence it disagrees with rather than requiring a re-read of
 * the source to know what was meant.
 *
 * A second section, clearly marked PROBE, exercises behaviour the header does
 * NOT document (netmgr_retry_advance() with a NULL attempt pointer, and the
 * uint32_t millisecond-base arithmetic near its wrap point / near 2^31 ms).
 * Those probes print what the code actually does; they never fail the run,
 * because asserting undocumented behaviour would turn "this is what happens
 * to happen today" into a contract nobody wrote. See the run_all.sh output
 * and tests/netmgr/README.md for how to read the results.
 *
 * Compiled against tests/netmgr/shim/tuya_cloud_types.h, not the real
 * tuya_cloud_types.h -- see that file and the README for why and what was
 * checked to justify it.
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "netmgr_retry.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK_U32(desc, expected, actual)                                                        \
    do {                                                                                          \
        uint32_t _e = (uint32_t)(expected);                                                       \
        uint32_t _a = (uint32_t)(actual);                                                          \
        if (_e == _a) {                                                                            \
            g_pass++;                                                                              \
        } else {                                                                                    \
            g_fail++;                                                                               \
            fprintf(stderr, "FAIL: %s\n    expected: %" PRIu32 "\n    actual:   %" PRIu32 "\n",     \
                    (desc), _e, _a);                                                                \
        }                                                                                            \
    } while (0)

#define CHECK_BOOL(desc, expected, actual)                                                        \
    do {                                                                                            \
        BOOL_T _e = (expected);                                                                     \
        BOOL_T _a = (actual);                                                                       \
        if ((!!_e) == (!!_a)) {                                                                      \
            g_pass++;                                                                               \
        } else {                                                                                     \
            g_fail++;                                                                                \
            fprintf(stderr, "FAIL: %s\n    expected: %s\n    actual:   %s\n", (desc),                \
                    _e ? "TRUE" : "FALSE", _a ? "TRUE" : "FALSE");                                    \
        }                                                                                             \
    } while (0)

#define CHECK_PTR_EQ(desc, expected, actual)                                                       \
    do {                                                                                             \
        const void *_e = (const void *)(expected);                                                  \
        const void *_a = (const void *)(actual);                                                    \
        if (_e == _a) {                                                                              \
            g_pass++;                                                                                \
        } else {                                                                                      \
            g_fail++;                                                                                 \
            fprintf(stderr, "FAIL: %s\n    expected ptr: %p\n    actual ptr:   %p\n", (desc), _e, _a); \
        }                                                                                              \
    } while (0)

/***********************************************************
 * 1. The two tables themselves (netmgr_retry.h: netmgr_retry_table_assoc,
 *    netmgr_retry_table_revalidate)
 ***********************************************************/
static void test_tables(void)
{
    printf("-- tables --\n");

    CHECK_U32("netmgr_retry_table_assoc.count == 6", 6, netmgr_retry_table_assoc.count);
    {
        const uint32_t expected[] = {1, 3, 5, 10, 15, 20};
        size_t i;
        for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
            char desc[96];
            snprintf(desc, sizeof(desc), "netmgr_retry_table_assoc.entry[%zu] (seconds)", i);
            CHECK_U32(desc, expected[i], netmgr_retry_table_assoc.entry[i]);
        }
    }

    CHECK_U32("netmgr_retry_table_revalidate.count == 5", 5, netmgr_retry_table_revalidate.count);
    {
        const uint32_t expected[] = {30, 60, 120, 300, 600};
        size_t i;
        for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
            char desc[96];
            snprintf(desc, sizeof(desc), "netmgr_retry_table_revalidate.entry[%zu] (seconds)", i);
            CHECK_U32(desc, expected[i], netmgr_retry_table_revalidate.entry[i]);
        }
    }
}

/***********************************************************
 * 2. netmgr_retry_interval_ms()
 ***********************************************************/
static void test_interval_ms(void)
{
    printf("-- netmgr_retry_interval_ms() --\n");

    /* The sequence 2eae2654 named: 1/3/5/10/15/20 ms*1000, then 20 forever.
     * "attempt 99 gets entry[count - 1]" (netmgr_retry.h). */
    {
        const uint32_t expected_ms[] = {1000, 3000, 5000, 10000, 15000, 20000};
        uint32_t attempt;
        for (attempt = 0; attempt < 6; attempt++) {
            char desc[80];
            snprintf(desc, sizeof(desc), "assoc interval_ms(attempt=%" PRIu32 ")", attempt);
            CHECK_U32(desc, expected_ms[attempt], netmgr_retry_interval_ms(&netmgr_retry_table_assoc, attempt));
        }
        CHECK_U32("assoc interval_ms(attempt=6) clamps to entry[5]", 20000,
                  netmgr_retry_interval_ms(&netmgr_retry_table_assoc, 6));
        CHECK_U32("assoc interval_ms(attempt=7) clamps to entry[5]", 20000,
                  netmgr_retry_interval_ms(&netmgr_retry_table_assoc, 7));
        CHECK_U32("assoc interval_ms(attempt=99) clamps to entry[5]", 20000,
                  netmgr_retry_interval_ms(&netmgr_retry_table_assoc, 99));
    }

    /* Revalidate table: same clamping rule, order of magnitude slower. */
    {
        const uint32_t expected_ms[] = {30000, 60000, 120000, 300000, 600000};
        uint32_t attempt;
        for (attempt = 0; attempt < 5; attempt++) {
            char desc[80];
            snprintf(desc, sizeof(desc), "revalidate interval_ms(attempt=%" PRIu32 ")", attempt);
            CHECK_U32(desc, expected_ms[attempt],
                      netmgr_retry_interval_ms(&netmgr_retry_table_revalidate, attempt));
        }
        CHECK_U32("revalidate interval_ms(attempt=5) clamps to entry[4]", 600000,
                  netmgr_retry_interval_ms(&netmgr_retry_table_revalidate, 5));
        CHECK_U32("revalidate interval_ms(attempt=6) clamps to entry[4]", 600000,
                  netmgr_retry_interval_ms(&netmgr_retry_table_revalidate, 6));
        CHECK_U32("revalidate interval_ms(attempt=100) clamps to entry[4]", 600000,
                  netmgr_retry_interval_ms(&netmgr_retry_table_revalidate, 100));
    }

    /* "Deliberately total: an empty or NULL table answers 0 rather than
     * failing" (netmgr_retry.h). */
    CHECK_U32("interval_ms(NULL table, attempt=0) == 0", 0, netmgr_retry_interval_ms(NULL, 0));
    CHECK_U32("interval_ms(NULL table, attempt=42) == 0", 0, netmgr_retry_interval_ms(NULL, 42));

    {
        const uint32_t some_entries[] = {1, 3, 5};
        netmgr_retry_table_t zero_count = {some_entries, 0};
        CHECK_U32("interval_ms(count=0 table, attempt=0) == 0", 0,
                  netmgr_retry_interval_ms(&zero_count, 0));
        CHECK_U32("interval_ms(count=0 table, attempt=99) == 0", 0,
                  netmgr_retry_interval_ms(&zero_count, 99));
    }

    /* __netmgr_retry_count() in netmgr_retry.c: "table->entry NULL -> 0,
     * whatever count says." A struct with a non-zero count but a NULL entry
     * pointer (e.g. a zeroed netmgr_retry_t that was never bound) must still
     * answer 0, not read through the NULL pointer. */
    {
        netmgr_retry_table_t null_entry = {NULL, 5};
        CHECK_U32("interval_ms(NULL entry, count=5, attempt=0) == 0", 0,
                  netmgr_retry_interval_ms(&null_entry, 0));
    }
}

/***********************************************************
 * 3. netmgr_retry_advance()
 ***********************************************************/
static void test_advance(void)
{
    printf("-- netmgr_retry_advance() --\n");

    /* "the interval in milliseconds for the attempt number BEFORE the bump"
     * and saturation at count-1, not wraparound (netmgr_retry.h). Run the
     * assoc table 30 times: it must land on and stay at attempt 5 / 20000ms,
     * never restart at attempt 0 / 1000ms. */
    {
        netmgr_retry_table_t table = netmgr_retry_table_assoc;
        uint32_t attempt = 0;
        const uint32_t expected_ms[] = {1000, 3000, 5000, 10000, 15000, 20000};
        int i;

        for (i = 0; i < 6; i++) {
            char desc[80];
            snprintf(desc, sizeof(desc), "advance() call #%d returns pre-bump interval", i);
            CHECK_U32(desc, expected_ms[i], netmgr_retry_advance(&table, &attempt));
        }
        CHECK_U32("attempt saturates at count-1 (5) after 6 advances", 5, attempt);

        for (i = 0; i < 24; i++) { /* 24 more calls: 30 total */
            CHECK_U32("advance() stays at 20000ms past saturation", 20000,
                      netmgr_retry_advance(&table, &attempt));
        }
        CHECK_U32("attempt still saturated at 5 after 30 advances (no wraparound)", 5, attempt);
    }

    /* Same saturation shape for the revalidate table, count 5 -> index 4. */
    {
        netmgr_retry_table_t table = netmgr_retry_table_revalidate;
        uint32_t attempt = 0;
        int i;
        for (i = 0; i < 5; i++) {
            netmgr_retry_advance(&table, &attempt);
        }
        CHECK_U32("revalidate attempt saturates at count-1 (4)", 4, attempt);
        for (i = 0; i < 10; i++) {
            CHECK_U32("revalidate advance() stays at 600000ms past saturation", 600000,
                      netmgr_retry_advance(&table, &attempt));
        }
        CHECK_U32("revalidate attempt still saturated at 4 after 15 advances", 4, attempt);
    }

    /* NULL table behaves like the "deliberately total" empty-table case:
     * interval 0, and `count > 1` guards the counter so it is never bumped
     * (netmgr_retry.c: "count > 1 also keeps count - 1 from underflowing on
     * an empty table, where the only sensible saturation point is 0"). */
    {
        uint32_t attempt = 0;
        int i;
        for (i = 0; i < 3; i++) {
            CHECK_U32("advance(NULL table, ...) returns 0", 0, netmgr_retry_advance(NULL, &attempt));
        }
        CHECK_U32("advance(NULL table, ...) never bumps attempt off 0", 0, attempt);
    }

    /* Same for an explicit count=0 table (entry non-NULL). */
    {
        const uint32_t some_entries[] = {1, 3, 5};
        netmgr_retry_table_t zero_count = {some_entries, 0};
        uint32_t attempt = 0;
        CHECK_U32("advance(count=0 table, ...) returns 0", 0, netmgr_retry_advance(&zero_count, &attempt));
        CHECK_U32("advance(count=0 table, ...) never bumps attempt off 0", 0, attempt);
    }
}

/***********************************************************
 * 4. netmgr_retry_bind() / netmgr_retry_reset()
 ***********************************************************/
static void test_bind_reset(void)
{
    printf("-- netmgr_retry_bind() / netmgr_retry_reset() --\n");

    /* "@param[out] retry the context to initialise; NULL is a no-op" and the
     * analogous line for reset(). Reaching the next line is the assertion. */
    netmgr_retry_bind(NULL, &netmgr_retry_table_assoc);
    g_pass++;
    printf("PASS: netmgr_retry_bind(NULL, table) does not crash\n");

    netmgr_retry_reset(NULL);
    g_pass++;
    printf("PASS: netmgr_retry_reset(NULL) does not crash\n");

    {
        netmgr_retry_t ctx;
        memset(&ctx, 0xAA, sizeof(ctx)); /* poison, so bind() must clear it */
        netmgr_retry_bind(&ctx, &netmgr_retry_table_assoc);
        CHECK_PTR_EQ("bind() copies table.entry", netmgr_retry_table_assoc.entry, ctx.table.entry);
        CHECK_U32("bind() copies table.count", netmgr_retry_table_assoc.count, ctx.table.count);
        CHECK_U32("bind() clears attempt", 0, ctx.attempt);
        CHECK_U32("bind() clears deadline_ms", 0, ctx.deadline_ms);
    }

    {
        /* "@param[in] table the table to use; NULL means 'no back-off'" */
        netmgr_retry_t ctx;
        memset(&ctx, 0xAA, sizeof(ctx));
        netmgr_retry_bind(&ctx, NULL);
        CHECK_PTR_EQ("bind(ctx, NULL) sets table.entry NULL", (void *)NULL, ctx.table.entry);
        CHECK_U32("bind(ctx, NULL) sets table.count 0", 0, ctx.table.count);
        CHECK_U32("bind(ctx, NULL) clears attempt", 0, ctx.attempt);
        CHECK_U32("bind(ctx, NULL) clears deadline_ms", 0, ctx.deadline_ms);
    }

    {
        /* "Forget every failure: attempt 0, not armed." after a real fail(). */
        netmgr_retry_t ctx;
        netmgr_retry_bind(&ctx, &netmgr_retry_table_assoc);
        netmgr_retry_fail(&ctx, 1000);
        netmgr_retry_reset(&ctx);
        CHECK_U32("reset() zeroes attempt", 0, ctx.attempt);
        CHECK_U32("reset() zeroes deadline_ms (unarmed)", 0, ctx.deadline_ms);
        CHECK_BOOL("due() is FALSE right after reset()", FALSE, netmgr_retry_due(&ctx, 999999));
    }
}

/***********************************************************
 * 5. netmgr_retry_fail()
 ***********************************************************/
static void test_fail(void)
{
    printf("-- netmgr_retry_fail() --\n");

    /* "@return ... 0 when @a retry is NULL or its table is empty" */
    CHECK_U32("fail(NULL, now) returns 0", 0, netmgr_retry_fail(NULL, 12345));

    /* "An empty table is also ARMED at @a now_ms, so every later
     * netmgr_retry_due() poll keeps answering 'due now' rather than 'never'."
     * This is the contract that is easiest to get backwards: the RETURN
     * value is 0 (matching interval_ms()'s total contract), but the context
     * itself is armed, not left unarmed. */
    {
        netmgr_retry_t ctx;
        netmgr_retry_bind(&ctx, NULL); /* NULL table -> empty */
        uint32_t ret = netmgr_retry_fail(&ctx, 1000);
        CHECK_U32("fail() with empty table returns 0", 0, ret);
        CHECK_U32("fail() with empty table still arms deadline_ms at now_ms", 1000, ctx.deadline_ms);
        CHECK_BOOL("due() right after arming an empty table at now is TRUE", TRUE,
                   netmgr_retry_due(&ctx, 1000));
        CHECK_BOOL("due() stays TRUE on every later poll (never re-fails)", TRUE,
                   netmgr_retry_due(&ctx, 999999999));
    }

    /* Continuous association failures from now=1000: deadlines advance by
     * the assoc table's intervals (1,3,5,10,15,20,20 seconds), matching
     * 2eae2654's "byte for byte" sequence claim end to end through fail(). */
    {
        netmgr_retry_t ctx;
        netmgr_retry_bind(&ctx, &netmgr_retry_table_assoc);
        uint32_t now = 1000;
        const uint32_t expected_deadline[] = {2000, 5000, 10000, 20000, 35000, 55000, 75000};
        size_t i;
        for (i = 0; i < sizeof(expected_deadline) / sizeof(expected_deadline[0]); i++) {
            char desc[96];
            uint32_t ret = netmgr_retry_fail(&ctx, now);
            snprintf(desc, sizeof(desc), "fail() #%zu return value == armed deadline", i);
            CHECK_U32(desc, expected_deadline[i], ret);
            snprintf(desc, sizeof(desc), "fail() #%zu ctx.deadline_ms", i);
            CHECK_U32(desc, expected_deadline[i], ctx.deadline_ms);
            now = expected_deadline[i]; /* simulate polling exactly at the deadline */
        }
    }
}

/***********************************************************
 * 6. netmgr_retry_due()
 ***********************************************************/
static void test_due(void)
{
    printf("-- netmgr_retry_due() --\n");

    CHECK_BOOL("due(NULL, now) is FALSE", FALSE, netmgr_retry_due(NULL, 500));

    {
        netmgr_retry_t ctx;
        netmgr_retry_bind(&ctx, &netmgr_retry_table_assoc);
        CHECK_BOOL("due() before any fail() (unarmed) is FALSE", FALSE, netmgr_retry_due(&ctx, 999999));

        netmgr_retry_fail(&ctx, 1000); /* deadline = 2000 */
        CHECK_BOOL("due() exactly at the deadline is TRUE", TRUE, netmgr_retry_due(&ctx, 2000));
        CHECK_BOOL("due() past the deadline is TRUE", TRUE, netmgr_retry_due(&ctx, 2001));
        CHECK_BOOL("due() one ms before the deadline is FALSE", FALSE, netmgr_retry_due(&ctx, 1999));
    }
}

/***********************************************************
 * 7. netmgr_retry_remain_ms() -- the three-way contract
 ***********************************************************/
static void test_remain_ms(void)
{
    printf("-- netmgr_retry_remain_ms() --\n");

    CHECK_U32("remain_ms(NULL, now) is 0", 0, netmgr_retry_remain_ms(NULL, 500));

    {
        netmgr_retry_t ctx;
        netmgr_retry_bind(&ctx, &netmgr_retry_table_assoc);
        CHECK_U32("remain_ms() not armed -> 0", 0, netmgr_retry_remain_ms(&ctx, 999999));

        netmgr_retry_fail(&ctx, 1000); /* deadline = 2000 */
        CHECK_U32("remain_ms() armed and exactly due -> 0", 0, netmgr_retry_remain_ms(&ctx, 2000));
        CHECK_U32("remain_ms() armed and past due -> 0", 0, netmgr_retry_remain_ms(&ctx, 5000));

        /* "armed and future -> the remaining milliseconds, never 0" */
        CHECK_U32("remain_ms() far before the deadline", 1000, netmgr_retry_remain_ms(&ctx, 1000));
        CHECK_U32("remain_ms() one ms before the deadline is 1, NEVER 0", 1,
                  netmgr_retry_remain_ms(&ctx, 1999));
    }
}

/***********************************************************
 * PROBE -- undocumented behaviour. Printed, never asserted.
 ***********************************************************/
static void probe_undocumented(void)
{
    printf("\n=== PROBE (informational only -- NOT asserted) ===\n");

    /* Probe A: netmgr_retry_advance() with a NULL attempt pointer. The
     * header's @param block for advance() says nothing about attempt being
     * NULL-able; only the .c comment ("Nothing to bump. Still answer the
     * first interval rather than 0...") documents it, and that comment is
     * implementation prose, not the header contract this test transcribes
     * everywhere else. */
    {
        uint32_t iv = netmgr_retry_advance(&netmgr_retry_table_assoc, NULL);
        printf("A) advance(&netmgr_retry_table_assoc, NULL) = %" PRIu32 " ms (no crash)\n", iv);
        iv = netmgr_retry_advance(NULL, NULL);
        printf("   advance(NULL, NULL)                      = %" PRIu32 " ms (no crash)\n", iv);
    }

    /* Probe B: the classic uint32_t millisecond wrap (crossing 0xFFFFFFFF)
     * with an interval the shipped tables can actually produce. This is the
     * case __netmgr_retry_reached()'s comment claims the signed-diff idiom
     * handles correctly. */
    {
        netmgr_retry_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.table = netmgr_retry_table_assoc;
        ctx.deadline_ms = 0xFFFFFFF0u; /* armed 16ms before the counter wraps */
        printf("B) classic wrap: deadline_ms=0xFFFFFFF0, now_ms=5 (21ms after deadline, post-wrap)\n");
        printf("   due()       = %s (real elapsed since deadline: 21ms)\n",
               netmgr_retry_due(&ctx, 5) ? "TRUE" : "FALSE");
        printf("   remain_ms() = %" PRIu32 "\n", netmgr_retry_remain_ms(&ctx, 5));
    }

    /* Probe C: an oversized caller-supplied table entry. netmgr_retry.h says
     * NETCONN_CMD_RECONN_TABLE "clamps user input" only for NETMGR_RETRY_TABLE_MAX
     * (the entry COUNT); netmgr_retry.c's own comment on
     * NETMGR_RETRY_INTERVAL_MAX_S says entry VALUES are not clamped by the
     * caller, "so a nonsense entry can reach a table," and this module then
     * clamps that value to NETMGR_RETRY_INTERVAL_MAX_S seconds (0xFFFFFFFF/1000)
     * so the *1000 multiplication does not overflow. That clamped result is
     * ~4294967000ms (~49.7 days) -- comfortably under 2^32ms, but far above
     * the 2^31ms (~24.8 day) bound that same file's comment on
     * __netmgr_retry_reached() says the signed-diff wrap-safe compare needs.
     */
    {
        static const uint32_t huge_entry[] = {4294967u}; /* NETMGR_RETRY_INTERVAL_MAX_S */
        netmgr_retry_table_t huge_table = {huge_entry, 1};
        uint32_t iv = netmgr_retry_interval_ms(&huge_table, 0);
        printf("C) oversized entry: interval_ms(huge_table, attempt=0) = %" PRIu32
               " ms (~%.1f days)\n",
               iv, iv / 86400000.0);

        netmgr_retry_t ctx;
        netmgr_retry_bind(&ctx, &huge_table);
        uint32_t deadline = netmgr_retry_fail(&ctx, 0);
        printf("   fail(now_ms=0) armed deadline_ms = %" PRIu32 "\n", deadline);

        uint32_t probe_now = 2000000000u; /* ~23.1 days after arming; real remainder ~26.5 days */
        double real_remaining_days = (deadline - (double)probe_now) / 86400000.0;
        printf("   due(now_ms=%" PRIu32 ")       = %s (real remaining ~%.1f days -- 'due' would be wrong)\n",
               probe_now, netmgr_retry_due(&ctx, probe_now) ? "TRUE" : "FALSE", real_remaining_days);
        printf("   remain_ms(now_ms=%" PRIu32 ") = %" PRIu32
               " (a correct answer would be close to %" PRIu32 ")\n",
               probe_now, netmgr_retry_remain_ms(&ctx, probe_now), deadline - probe_now);
    }
}

int main(void)
{
    test_tables();
    test_interval_ms();
    test_advance();
    test_bind_reset();
    test_fail();
    test_due();
    test_remain_ms();

    probe_undocumented();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
