/**
 * @file opus_cortex_m3_macros.h
 * @brief Bit-exact SILK halfword multiplication for Cortex-M3.
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef TUYAOPEN_OPUS_CORTEX_M3_MACROS_H
#define TUYAOPEN_OPUS_CORTEX_M3_MACROS_H

/* Forced into the Opus target before other headers. The upstream include
 * guard keeps later includes from replacing these four overrides. */
#include "macros.h"

#if !defined(__GNUC__) || !defined(__arm__) || !defined(__ARM_ARCH_7M__)
#error "TuyaOpen Opus Cortex-M3 multiplication requires an ARMv7-M GNU-compatible compiler"
#endif

#if defined(OPUS_ARM_INLINE_ASM) || defined(OPUS_ARM_INLINE_EDSP)
#error "Cortex-M3 multiplication must not be combined with the upstream ARM assembly paths"
#endif

static OPUS_INLINE opus_int32 tuyaopen_opus_m3_mul_high(opus_int32 a, opus_int32 b)
{
    opus_uint32 low;
    opus_int32  high;
    /* Early-clobber outputs cannot overlap either input. SMULL does not
     * change condition flags; no memory or condition-code clobber is needed. */
    __asm__("smull %0, %1, %2, %3" : "=&r"(low), "=&r"(high) : "r"(a), "r"(b));
    return high;
}

static OPUS_INLINE opus_int32 tuyaopen_opus_m3_smulwb(opus_int32 a, opus_int32 b)
{
    /* Shift unsigned to avoid left-shifting a negative signed value.
     * high32(a * (int16(b) << 16)) equals floor(a * int16(b) / 65536). */
    opus_int32 shifted_b = (opus_int32)((opus_uint32)(opus_int16)b << 16);
    return tuyaopen_opus_m3_mul_high(a, shifted_b);
}

static OPUS_INLINE opus_int32 tuyaopen_opus_m3_smulwt(opus_int32 a, opus_int32 b)
{
    opus_int32 shifted_b = (opus_int32)((opus_uint32)b & 0xffff0000u);
    return tuyaopen_opus_m3_mul_high(a, shifted_b);
}

#undef silk_SMULWB
#define silk_SMULWB(a, b) tuyaopen_opus_m3_smulwb((a), (b))
#undef silk_SMLAWB
#define silk_SMLAWB(a, b, c) ((a) + silk_SMULWB((b), (c)))
#undef silk_SMULWT
#define silk_SMULWT(a, b) tuyaopen_opus_m3_smulwt((a), (b))
#undef silk_SMLAWT
#define silk_SMLAWT(a, b, c) ((a) + silk_SMULWT((b), (c)))

#endif /* TUYAOPEN_OPUS_CORTEX_M3_MACROS_H */
