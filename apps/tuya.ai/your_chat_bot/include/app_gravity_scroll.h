/**
 * @file app_gravity_scroll.h
 * @brief Reusable gravity helpers: pitch scroll + gyro flick L/R (IMU-agnostic)
 * @version 1.2
 * @date 2026-07-30
 * @copyright Copyright (c) Tuya Inc.
 *
 * Vertical: continuous scroll from accelerometer tilt (pitch).
 * Lateral: edge-triggered LEFT/RIGHT from gyroscope rate (angular velocity),
 *          not static lean angle — a quick flick fires confirm / back.
 */
#ifndef __APP_GRAVITY_SCROLL_H__
#define __APP_GRAVITY_SCROLL_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
/** Which accel pair defines a tilt angle (scroll only) */
typedef enum {
    APP_GRAVITY_TILT_AY_AZ = 0, /**< pitch ≈ atan2(ay, az) */
    APP_GRAVITY_TILT_AX_AZ = 1, /**< roll  ≈ atan2(ax, az) */
    APP_GRAVITY_TILT_AX_AY = 2, /**< roll  ≈ atan2(ax, ay) */
} APP_GRAVITY_TILT_AXIS_E;

/** Which gyro axis maps to left/right flick */
typedef enum {
    APP_GRAVITY_GYRO_X = 0,
    APP_GRAVITY_GYRO_Y = 1,
    APP_GRAVITY_GYRO_Z = 2,
} APP_GRAVITY_GYRO_AXIS_E;

/** Discrete lateral gesture (one-shot after rate threshold) */
typedef enum {
    APP_GRAVITY_GESTURE_NONE  = 0,
    APP_GRAVITY_GESTURE_LEFT  = 1, /**< left flick → typically back */
    APP_GRAVITY_GESTURE_RIGHT = 2, /**< right flick → typically confirm */
} APP_GRAVITY_GESTURE_E;

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct {
    float deadzone_deg;
    float release_deg;
    float max_tilt_deg;
    float lpf_alpha;
    float max_speed_px_s;
    APP_GRAVITY_TILT_AXIS_E tilt_axis;
    int8_t invert;
    uint16_t calibrate_samples;
} APP_GRAVITY_SCROLL_CFG_T;

typedef struct {
    APP_GRAVITY_SCROLL_CFG_T cfg;
    float                    baseline_deg;
    float                    tilt_lpf_deg;
    float                    pixel_accum;
    uint16_t                 calib_count;
    float                    calib_sum;
    uint8_t                  calibrated;
    uint8_t                  scrolling;
} APP_GRAVITY_SCROLL_T;

/**
 * Gyro flick config: fire when |ω| exceeds thresh (deg/s), not lean angle.
 * Angular rate is proportional to angular momentum for roughly fixed inertia.
 */
typedef struct {
    /** Fire when |primary gyro| >= this (deg/s). Typical: 100~220 */
    float thresh_dps;
    /** Re-arm when |primary| falls below this (deg/s) */
    float release_dps;
    /** Low-pass alpha on gyro, 0..1 */
    float lpf_alpha;
    /** Primary axis for left/right flick */
    APP_GRAVITY_GYRO_AXIS_E axis;
    /** Require |primary| >= dominance_ratio * max(|other axes|) */
    float dominance_ratio;
    /** +1 / -1 — flip so +rate maps to RIGHT */
    int8_t invert;
    /** Ignore further gestures for this many ms after a fire */
    uint32_t cooldown_ms;
} APP_GRAVITY_FLICK_CFG_T;

typedef struct {
    APP_GRAVITY_FLICK_CFG_T cfg;
    float                   rate_lpf_dps[3];
    uint8_t                 armed;
    uint8_t                 primed; /**< saw rising edge past thresh */
    uint32_t                cooldown_until_ms;
} APP_GRAVITY_FLICK_T;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Fill cfg with recommended defaults for handheld list scrolling
 * @param[out] cfg config to fill
 * @return none
 */
void app_gravity_scroll_cfg_default(APP_GRAVITY_SCROLL_CFG_T *cfg);

/**
 * @brief Initialize gravity-scroll context
 * @param[in,out] ctx context
 * @param[in] cfg config (NULL → defaults)
 * @return OPRT_OK on success
 */
OPERATE_RET app_gravity_scroll_init(APP_GRAVITY_SCROLL_T *ctx, const APP_GRAVITY_SCROLL_CFG_T *cfg);

/**
 * @brief Restart baseline calibration
 * @param[in,out] ctx context
 * @return none
 */
void app_gravity_scroll_recalibrate(APP_GRAVITY_SCROLL_T *ctx);

/**
 * @brief Feed accelerometer sample; get scroll delta in pixels
 * @param[in,out] ctx context
 * @param[in] ax accel X
 * @param[in] ay accel Y
 * @param[in] az accel Z
 * @param[in] dt_ms elapsed ms
 * @return signed scroll delta in pixels
 */
float app_gravity_scroll_update(APP_GRAVITY_SCROLL_T *ctx, float ax, float ay, float az, uint32_t dt_ms);

/**
 * @brief Current filtered tilt relative to baseline (degrees)
 * @param[in] ctx context
 * @return tilt degrees
 */
float app_gravity_scroll_get_tilt_deg(const APP_GRAVITY_SCROLL_T *ctx);

/**
 * @brief Whether baseline calibration finished
 * @param[in] ctx context
 * @return 1 if calibrated
 */
uint8_t app_gravity_scroll_is_calibrated(const APP_GRAVITY_SCROLL_T *ctx);

/**
 * @brief Whether vertical scroll is currently active
 * @param[in] ctx context
 * @return 1 if scrolling
 */
uint8_t app_gravity_scroll_is_scrolling(const APP_GRAVITY_SCROLL_T *ctx);

/**
 * @brief Fill cfg with defaults for gyro-rate flick gestures
 * @param[out] cfg config to fill
 * @return none
 */
void app_gravity_flick_cfg_default(APP_GRAVITY_FLICK_CFG_T *cfg);

/**
 * @brief Initialize flick-gesture context
 * @param[in,out] ctx context
 * @param[in] cfg config (NULL → defaults)
 * @return OPRT_OK on success
 */
OPERATE_RET app_gravity_flick_init(APP_GRAVITY_FLICK_T *ctx, const APP_GRAVITY_FLICK_CFG_T *cfg);

/**
 * @brief Reset flick detector state
 * @param[in,out] ctx context
 * @return none
 */
void app_gravity_flick_reset(APP_GRAVITY_FLICK_T *ctx);

/**
 * @brief Feed gyro sample (deg/s); return edge-triggered LEFT/RIGHT or NONE
 * @param[in,out] ctx context
 * @param[in] gx gyro X (deg/s)
 * @param[in] gy gyro Y (deg/s)
 * @param[in] gz gyro Z (deg/s)
 * @param[in] now_ms monotonic ms
 * @return APP_GRAVITY_GESTURE_* when |ω| crosses thresh with dominance
 * @note After invert: +rate → RIGHT (confirm), -rate → LEFT (back).
 */
APP_GRAVITY_GESTURE_E app_gravity_flick_update(APP_GRAVITY_FLICK_T *ctx, float gx, float gy, float gz,
                                               uint32_t now_ms);

/**
 * @brief Filtered primary-axis rate after invert (deg/s)
 * @param[in] ctx context
 * @return rate dps
 */
float app_gravity_flick_get_rate_dps(const APP_GRAVITY_FLICK_T *ctx);

/**
 * Discrete shake / jerk (NOT tilt angle): fire once when dynamic accel
 * or gyro rate on a chosen axis crosses a threshold, then re-arm on release.
 */
typedef enum {
    APP_GRAVITY_SHAKE_NONE = 0,
    APP_GRAVITY_SHAKE_POS  = 1, /**< +axis impulse */
    APP_GRAVITY_SHAKE_NEG  = 2, /**< -axis impulse */
} APP_GRAVITY_SHAKE_E;

typedef enum {
    APP_GRAVITY_AXIS_X = 0,
    APP_GRAVITY_AXIS_Y = 1,
    APP_GRAVITY_AXIS_Z = 2,
} APP_GRAVITY_AXIS_E;

typedef struct {
    float accel_thresh_g;   /**< |dyn accel| fire threshold (g), e.g. 0.45 */
    float accel_release_g;  /**< re-arm when |dyn| below this */
    float gyro_thresh_dps;  /**< optional gyro fire (0 = accel only) */
    float gyro_release_dps;
    float gravity_lpf;      /**< slow LPF for gravity baseline 0..1 */
    float dyn_lpf;          /**< LPF on dynamic accel 0..1 */
    /** Require |primary| >= dominance_ratio * max(|other axes|); 0 disables */
    float dominance_ratio;
    APP_GRAVITY_AXIS_E accel_axis;
    APP_GRAVITY_AXIS_E gyro_axis;
    int8_t invert;
    uint32_t cooldown_ms;
} APP_GRAVITY_SHAKE_CFG_T;

typedef struct {
    APP_GRAVITY_SHAKE_CFG_T cfg;
    float                   accel_base[3];
    float                   dyn_lpf[3];
    float                   gyro_lpf[3];
    uint8_t                 base_ready;
    uint16_t                base_count;
    uint8_t                 armed;
    uint32_t                cooldown_until_ms;
} APP_GRAVITY_SHAKE_T;

/**
 * @brief Fill shake cfg defaults (impulse / jerk, not tilt)
 * @param[out] cfg config
 * @return none
 */
void app_gravity_shake_cfg_default(APP_GRAVITY_SHAKE_CFG_T *cfg);

/**
 * @brief Initialize shake detector
 * @param[in,out] ctx context
 * @param[in] cfg config (NULL → defaults)
 * @return OPRT_OK on success
 */
OPERATE_RET app_gravity_shake_init(APP_GRAVITY_SHAKE_T *ctx, const APP_GRAVITY_SHAKE_CFG_T *cfg);

/**
 * @brief Reset arming / cooldown (keep baseline)
 * @param[in,out] ctx context
 * @return none
 */
void app_gravity_shake_reset(APP_GRAVITY_SHAKE_T *ctx);

/**
 * @brief Feed IMU sample; return edge-triggered POS/NEG or NONE
 * @param[in,out] ctx context
 * @param[in] ax ay az accel (g)
 * @param[in] gx gy gz gyro (dps)
 * @param[in] now_ms monotonic ms
 * @return APP_GRAVITY_SHAKE_*
 * @note Ignores static tilt: uses accel minus slow gravity baseline, and/or gyro rate.
 */
APP_GRAVITY_SHAKE_E app_gravity_shake_update(APP_GRAVITY_SHAKE_T *ctx, float ax, float ay, float az,
                                             float gx, float gy, float gz, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* __APP_GRAVITY_SCROLL_H__ */
