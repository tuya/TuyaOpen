/**
 * @file cortex_m3_math_selftest.c
 * @brief Target-side arithmetic checks outside the timed Opus benchmark.
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#include "../optimization/opus_cortex_m3_macros.h"

static int check_products(opus_int32 a, opus_int32 b, opus_int32 accumulator)
{
    opus_int32 wb = (opus_int32)(((opus_int64)a * (opus_int16)b) >> 16);
    opus_int32 wt = (opus_int32)(((opus_int64)a * (b >> 16)) >> 16);
    /* A half-range accumulator keeps these additions within int32 limits. */
    accumulator >>= 1;
    return silk_SMULWB(a, b) == wb && silk_SMULWT(a, b) == wt && silk_SMLAWB(accumulator, a, b) == accumulator + wb &&
           silk_SMLAWT(accumulator, a, b) == accumulator + wt;
}

/* Returns the number of checked input triples, or -1 on the first mismatch. */
int tuyaopen_opus_m3_math_selftest(void)
{
    static const opus_int32 edges[] = {
        (-2147483647 - 1), -2147483647, -65537, -65536, -32769, -32768, -1, 0, 1, 32767, 32768, 65535, 65536,
        2147483647};
    int checked = 0;
    for (unsigned int i = 0; i < sizeof(edges) / sizeof(edges[0]); i++) {
        for (unsigned int j = 0; j < sizeof(edges) / sizeof(edges[0]); j++) {
            if (!check_products(edges[i], edges[j], edges[(i + j) % (sizeof(edges) / sizeof(edges[0]))])) {
                return -1;
            }
            checked++;
        }
    }
    opus_uint32 state = 20261012u;
    for (int i = 0; i < 4096; i++) {
        state            = state * 1664525u + 1013904223u;
        opus_int32 a     = (opus_int32)state;
        state            = state * 1664525u + 1013904223u;
        opus_int32 b     = (opus_int32)state;
        state            = state * 1664525u + 1013904223u;
        opus_int32 accum = (opus_int32)state;
        if (!check_products(a, b, accum)) {
            return -1;
        }
        checked++;
    }
    return checked;
}
