/**
 * @file ikcp_minmax.c
 * @brief Windowed min/max tracker aligned with TuyaOS tuya_p2p_lite_win_minmax
 * @version 1.0
 * @date 2026-08-04
 * @copyright Copyright (c) Tuya Inc.
 *
 * @note Algorithm matches Linux kernel lib/minmax.c (used by BBR / OS mid_p2p).
 */
#include "ikcp_minmax.h"

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Reset minmax window with one sample
 * @param[in,out] m tracker
 * @param[in] t timestamp
 * @param[in] meas measurement
 * @return measurement value
 */
uint32_t minmax_reset(struct minmax *m, uint32_t t, uint32_t meas)
{
    struct minmax_sample val;

    val.t = t;
    val.v = meas;
    m->s[0] = val;
    m->s[1] = val;
    m->s[2] = val;
    return meas;
}

/**
 * @brief Get current best sample value
 * @param[in] m tracker
 * @return sample value at s[0]
 */
uint32_t minmax_get(const struct minmax *m)
{
    return m->s[0].v;
}

/**
 * @brief Choose how to place a new sample into the 3-slot window
 * @param[in,out] m tracker
 * @param[in] win window length
 * @param[in] val new sample
 * @return none
 */
static void minmax_subwin_update(struct minmax *m, uint32_t win, const struct minmax_sample *val)
{
    uint32_t dt = val->t - m->s[0].t;

    if (dt > win) {
        m->s[0] = m->s[1];
        m->s[1] = m->s[2];
        m->s[2] = *val;
        if (val->t - m->s[0].t > win) {
            m->s[0] = m->s[1];
            m->s[1] = m->s[2];
            m->s[2] = *val;
        }
    } else if (m->s[1].t == m->s[0].t && dt > win / 4) {
        m->s[2] = m->s[1] = *val;
    } else if (m->s[2].t == m->s[1].t && dt > win / 2) {
        m->s[2] = *val;
    }
}

/**
 * @brief Update running maximum over a time window
 * @param[in,out] m tracker
 * @param[in] win window length
 * @param[in] t timestamp
 * @param[in] meas measurement
 * @return current max
 */
uint32_t minmax_running_max(struct minmax *m, uint32_t win, uint32_t t, uint32_t meas)
{
    struct minmax_sample val;

    val.t = t;
    val.v = meas;
    if (meas >= m->s[0].v || t - m->s[2].t > win) {
        return minmax_reset(m, t, meas);
    }
    if (meas >= m->s[1].v) {
        m->s[2] = m->s[1] = val;
    } else if (meas >= m->s[2].v) {
        m->s[2] = val;
    }
    minmax_subwin_update(m, win, &val);
    return m->s[0].v;
}

/**
 * @brief Update running minimum over a time window
 * @param[in,out] m tracker
 * @param[in] win window length
 * @param[in] t timestamp
 * @param[in] meas measurement
 * @return current min
 */
uint32_t minmax_running_min(struct minmax *m, uint32_t win, uint32_t t, uint32_t meas)
{
    struct minmax_sample val;

    val.t = t;
    val.v = meas;
    if (meas <= m->s[0].v || t - m->s[2].t > win) {
        return minmax_reset(m, t, meas);
    }
    if (meas <= m->s[1].v) {
        m->s[2] = m->s[1] = val;
    } else if (meas <= m->s[2].v) {
        m->s[2] = val;
    }
    minmax_subwin_update(m, win, &val);
    return m->s[0].v;
}
