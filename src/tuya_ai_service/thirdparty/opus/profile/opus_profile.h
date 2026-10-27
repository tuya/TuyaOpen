/**
 * @file opus_profile.h
 * @brief Optional benchmark callbacks for fixed-point SILK profiling.
 * @copyright Copyright (c) 2026 Tuya Inc. All Rights Reserved.
 */
#ifndef TUYAOPEN_OPUS_PROFILE_H
#define TUYAOPEN_OPUS_PROFILE_H

#include <stdint.h>

typedef enum {
    OPUS_PROFILE_PITCH,
    OPUS_PROFILE_SHAPE,
    OPUS_PROFILE_PREDICT,
    OPUS_PROFILE_GAINS,
    OPUS_PROFILE_LBRR,
    OPUS_PROFILE_NSQ,
    OPUS_PROFILE_ENTROPY,
    OPUS_PROFILE_NSQ_REWHITE,
    OPUS_PROFILE_NSQ_SCALE,
    OPUS_PROFILE_NSQ_NORMAL,
    OPUS_PROFILE_NSQ_DELAYED,
    OPUS_PROFILE_PRED_LTP_ANALYSIS,
    OPUS_PROFILE_PRED_LTP_QUANT,
    OPUS_PROFILE_PRED_LTP_FILTER,
    OPUS_PROFILE_PRED_UNVOICED_SCALE,
    OPUS_PROFILE_PRED_LPC,
    OPUS_PROFILE_PRED_NLSF,
    OPUS_PROFILE_PRED_RESIDUAL,
    OPUS_PROFILE_STAGE_COUNT
} OPUS_PROFILE_STAGE_T;

/* Implemented by the benchmark; no logging occurs inside these callbacks. */
uint64_t tuyaopen_opus_profile_now(void);
void     tuyaopen_opus_profile_record(OPUS_PROFILE_STAGE_T stage, uint64_t elapsed_us);
void     tuyaopen_opus_profile_iteration(unsigned int iteration);

#endif /* TUYAOPEN_OPUS_PROFILE_H */
