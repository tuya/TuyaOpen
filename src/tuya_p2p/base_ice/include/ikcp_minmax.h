/**
 * @file ikcp_minmax.h
 * @brief Windowed min/max tracker (TuyaOS mid_p2p win_minmax)
 * @version 1.0
 * @date 2026-08-04
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef __IKCP_MINMAX_H__
#define __IKCP_MINMAX_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
struct minmax_sample {
    uint32_t t;
    uint32_t v;
};

struct minmax {
    struct minmax_sample s[3];
};

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Reset minmax window with one sample
 * @param[in,out] m tracker
 * @param[in] t timestamp
 * @param[in] meas measurement
 * @return measurement value
 */
uint32_t minmax_reset(struct minmax *m, uint32_t t, uint32_t meas);

/**
 * @brief Get current best (max or min) sample value
 * @param[in] m tracker
 * @return sample value at s[0]
 */
uint32_t minmax_get(const struct minmax *m);

/**
 * @brief Update running maximum over a time window
 * @param[in,out] m tracker
 * @param[in] win window length
 * @param[in] t timestamp
 * @param[in] meas measurement
 * @return current max
 */
uint32_t minmax_running_max(struct minmax *m, uint32_t win, uint32_t t, uint32_t meas);

/**
 * @brief Update running minimum over a time window
 * @param[in,out] m tracker
 * @param[in] win window length
 * @param[in] t timestamp
 * @param[in] meas measurement
 * @return current min
 */
uint32_t minmax_running_min(struct minmax *m, uint32_t win, uint32_t t, uint32_t meas);

#ifdef __cplusplus
}
#endif

#endif /* __IKCP_MINMAX_H__ */
