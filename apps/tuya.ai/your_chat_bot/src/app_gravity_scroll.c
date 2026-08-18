/**
 * @file app_gravity_scroll.c
 * @brief Reusable gravity helpers (pitch scroll + gyro flick L/R)
 * @version 1.2
 * @date 2026-07-30
 * @copyright Copyright (c) Tuya Inc.
 */
#include "app_gravity_scroll.h"

#include <math.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG_FROM_RAD(r) ((float)((r) * (180.0 / M_PI)))
#define DT_MS_MIN       1u
#define DT_MS_MAX       100u

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Fill cfg with recommended defaults for handheld list scrolling
 * @param[out] cfg config to fill
 * @return none
 */
void app_gravity_scroll_cfg_default(APP_GRAVITY_SCROLL_CFG_T *cfg)
{
    if (cfg == NULL) {
        return;
    }
    cfg->deadzone_deg      = 8.0f;
    cfg->release_deg       = 5.0f;
    cfg->max_tilt_deg      = 40.0f;
    cfg->lpf_alpha         = 0.25f;
    cfg->max_speed_px_s    = 420.0f;
    cfg->tilt_axis         = APP_GRAVITY_TILT_AY_AZ;
    cfg->invert            = 1;
    cfg->calibrate_samples = 25; /* ~0.5s at 50Hz */
}

/**
 * @brief Compute raw tilt angle (degrees) from accel and axis mapping
 * @param[in] axis which accel pair
 * @param[in] ax accel X
 * @param[in] ay accel Y
 * @param[in] az accel Z
 * @return tilt degrees
 */
static float __tilt_from_accel_axis(APP_GRAVITY_TILT_AXIS_E axis, float ax, float ay, float az)
{
    float a = 0.0f;
    float b = 1.0f;

    switch (axis) {
    case APP_GRAVITY_TILT_AX_AZ:
        a = ax;
        b = az;
        break;
    case APP_GRAVITY_TILT_AX_AY:
        a = ax;
        b = ay;
        break;
    case APP_GRAVITY_TILT_AY_AZ:
    default:
        a = ay;
        b = az;
        break;
    }
    return DEG_FROM_RAD(atan2f(a, b));
}

/**
 * @brief Clamp value into [lo, hi]
 * @param[in] v value
 * @param[in] lo lower bound
 * @param[in] hi upper bound
 * @return clamped value
 */
static float __clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

/**
 * @brief Initialize gravity-scroll context
 * @param[in,out] ctx context
 * @param[in] cfg config (NULL → defaults)
 * @return OPRT_OK on success
 */
OPERATE_RET app_gravity_scroll_init(APP_GRAVITY_SCROLL_T *ctx, const APP_GRAVITY_SCROLL_CFG_T *cfg)
{
    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }
    memset(ctx, 0, sizeof(*ctx));
    if (cfg != NULL) {
        ctx->cfg = *cfg;
    } else {
        app_gravity_scroll_cfg_default(&ctx->cfg);
    }
    if (ctx->cfg.deadzone_deg < 1.0f) {
        ctx->cfg.deadzone_deg = 1.0f;
    }
    if (ctx->cfg.release_deg <= 0.0f || ctx->cfg.release_deg > ctx->cfg.deadzone_deg) {
        ctx->cfg.release_deg = ctx->cfg.deadzone_deg * 0.6f;
    }
    if (ctx->cfg.max_tilt_deg <= ctx->cfg.deadzone_deg) {
        ctx->cfg.max_tilt_deg = ctx->cfg.deadzone_deg + 20.0f;
    }
    if (ctx->cfg.lpf_alpha <= 0.0f || ctx->cfg.lpf_alpha > 1.0f) {
        ctx->cfg.lpf_alpha = 0.25f;
    }
    if (ctx->cfg.max_speed_px_s < 1.0f) {
        ctx->cfg.max_speed_px_s = 200.0f;
    }
    if (ctx->cfg.invert == 0) {
        ctx->cfg.invert = 1;
    }
    if (ctx->cfg.calibrate_samples == 0) {
        ctx->cfg.calibrate_samples = 20;
    }
    app_gravity_scroll_recalibrate(ctx);
    return OPRT_OK;
}

/**
 * @brief Restart baseline calibration (e.g. when user holds device still)
 * @param[in,out] ctx context
 * @return none
 */
void app_gravity_scroll_recalibrate(APP_GRAVITY_SCROLL_T *ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->calib_count  = 0;
    ctx->calib_sum    = 0.0f;
    ctx->calibrated   = 0;
    ctx->scrolling    = 0;
    ctx->pixel_accum  = 0.0f;
    ctx->tilt_lpf_deg = 0.0f;
    ctx->baseline_deg = 0.0f;
}

/**
 * @brief Feed one accelerometer sample and get scroll delta for this frame
 * @param[in,out] ctx context
 * @param[in] ax accel X (any unit; only ratios matter)
 * @param[in] ay accel Y
 * @param[in] az accel Z
 * @param[in] dt_ms elapsed milliseconds since last call (clamped internally)
 * @return signed scroll delta in pixels (fractional remainder kept in ctx)
 * @note Positive return → scroll content downward (show lower items), unless invert.
 */
float app_gravity_scroll_update(APP_GRAVITY_SCROLL_T *ctx, float ax, float ay, float az, uint32_t dt_ms)
{
    float tilt_raw;
    float tilt_rel;
    float abs_rel;
    float enter_dz;
    float exit_dz;
    float usable;
    float span;
    float speed;
    float dy;

    if (ctx == NULL) {
        return 0.0f;
    }
    if (dt_ms < DT_MS_MIN) {
        dt_ms = DT_MS_MIN;
    } else if (dt_ms > DT_MS_MAX) {
        dt_ms = DT_MS_MAX;
    }

    tilt_raw = __tilt_from_accel_axis(ctx->cfg.tilt_axis, ax, ay, az);

    if (!ctx->calibrated) {
        ctx->calib_sum += tilt_raw;
        ctx->calib_count++;
        if (ctx->calib_count >= ctx->cfg.calibrate_samples) {
            ctx->baseline_deg = ctx->calib_sum / (float)ctx->calib_count;
            ctx->tilt_lpf_deg = ctx->baseline_deg;
            ctx->calibrated   = 1;
        }
        return 0.0f;
    }

    ctx->tilt_lpf_deg =
        ctx->cfg.lpf_alpha * tilt_raw + (1.0f - ctx->cfg.lpf_alpha) * ctx->tilt_lpf_deg;

    tilt_rel = ctx->tilt_lpf_deg - ctx->baseline_deg;
    abs_rel  = fabsf(tilt_rel);
    enter_dz = ctx->cfg.deadzone_deg;
    exit_dz  = ctx->cfg.release_deg;

    if (!ctx->scrolling) {
        if (abs_rel < enter_dz) {
            ctx->pixel_accum = 0.0f;
            return 0.0f;
        }
        ctx->scrolling = 1;
    } else if (abs_rel < exit_dz) {
        ctx->scrolling   = 0;
        ctx->pixel_accum = 0.0f;
        return 0.0f;
    }

    usable = abs_rel - enter_dz;
    span   = ctx->cfg.max_tilt_deg - enter_dz;
    if (span < 1.0f) {
        span = 1.0f;
    }
    usable = __clampf(usable, 0.0f, span);
    speed  = (usable / span) * ctx->cfg.max_speed_px_s;
    if (tilt_rel < 0.0f) {
        speed = -speed;
    }
    speed *= (float)ctx->cfg.invert;

    dy = speed * ((float)dt_ms / 1000.0f);
    ctx->pixel_accum += dy;
    {
        int32_t step = (int32_t)ctx->pixel_accum;
        ctx->pixel_accum -= (float)step;
        return (float)step;
    }
}

/**
 * @brief Current filtered tilt relative to baseline (degrees)
 * @param[in] ctx context
 * @return tilt degrees (0 if not calibrated)
 */
float app_gravity_scroll_get_tilt_deg(const APP_GRAVITY_SCROLL_T *ctx)
{
    if (ctx == NULL || !ctx->calibrated) {
        return 0.0f;
    }
    return ctx->tilt_lpf_deg - ctx->baseline_deg;
}

/**
 * @brief Whether baseline calibration finished
 * @param[in] ctx context
 * @return 1 if calibrated, else 0
 */
uint8_t app_gravity_scroll_is_calibrated(const APP_GRAVITY_SCROLL_T *ctx)
{
    if (ctx == NULL) {
        return 0;
    }
    return ctx->calibrated;
}

/**
 * @brief Whether vertical scroll is currently active (past deadzone)
 * @param[in] ctx context
 * @return 1 if scrolling, else 0
 */
uint8_t app_gravity_scroll_is_scrolling(const APP_GRAVITY_SCROLL_T *ctx)
{
    if (ctx == NULL) {
        return 0;
    }
    return ctx->scrolling;
}

/**
 * @brief Fill cfg with defaults for gyro-rate flick gestures
 * @param[out] cfg config to fill
 * @return none
 */
void app_gravity_flick_cfg_default(APP_GRAVITY_FLICK_CFG_T *cfg)
{
    if (cfg == NULL) {
        return;
    }
    cfg->thresh_dps      = 140.0f;
    cfg->release_dps     = 40.0f;
    cfg->lpf_alpha       = 0.45f;
    cfg->axis            = APP_GRAVITY_GYRO_Y;
    cfg->dominance_ratio = 1.25f;
    cfg->invert          = 1;
    cfg->cooldown_ms     = 450;
}

/**
 * @brief Reset flick detector state
 * @param[in,out] ctx context
 * @return none
 */
void app_gravity_flick_reset(APP_GRAVITY_FLICK_T *ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->rate_lpf_dps[0]   = 0.0f;
    ctx->rate_lpf_dps[1]   = 0.0f;
    ctx->rate_lpf_dps[2]   = 0.0f;
    ctx->armed             = 1;
    ctx->primed            = 0;
    ctx->cooldown_until_ms = 0;
}

/**
 * @brief Initialize flick-gesture context
 * @param[in,out] ctx context
 * @param[in] cfg config (NULL → defaults)
 * @return OPRT_OK on success
 */
OPERATE_RET app_gravity_flick_init(APP_GRAVITY_FLICK_T *ctx, const APP_GRAVITY_FLICK_CFG_T *cfg)
{
    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }
    memset(ctx, 0, sizeof(*ctx));
    if (cfg != NULL) {
        ctx->cfg = *cfg;
    } else {
        app_gravity_flick_cfg_default(&ctx->cfg);
    }
    if (ctx->cfg.thresh_dps < 20.0f) {
        ctx->cfg.thresh_dps = 20.0f;
    }
    if (ctx->cfg.release_dps <= 0.0f || ctx->cfg.release_dps >= ctx->cfg.thresh_dps) {
        ctx->cfg.release_dps = ctx->cfg.thresh_dps * 0.3f;
    }
    if (ctx->cfg.lpf_alpha <= 0.0f || ctx->cfg.lpf_alpha > 1.0f) {
        ctx->cfg.lpf_alpha = 0.45f;
    }
    /* dominance_ratio <= 0 disables the cross-axis dominance gate */
    if (ctx->cfg.dominance_ratio > 0.0f && ctx->cfg.dominance_ratio < 0.5f) {
        ctx->cfg.dominance_ratio = 0.5f;
    }
    if (ctx->cfg.invert == 0) {
        ctx->cfg.invert = 1;
    }
    if (ctx->cfg.cooldown_ms == 0) {
        ctx->cfg.cooldown_ms = 300;
    }
    if ((unsigned)ctx->cfg.axis > (unsigned)APP_GRAVITY_GYRO_Z) {
        ctx->cfg.axis = APP_GRAVITY_GYRO_Y;
    }
    app_gravity_flick_reset(ctx);
    return OPRT_OK;
}

/**
 * @brief Pick primary / other gyro components after LPF
 * @param[in] ctx context
 * @param[out] primary signed primary rate (before invert)
 * @param[out] other_max max abs of the other two axes
 * @return none
 */
static void __flick_rates(const APP_GRAVITY_FLICK_T *ctx, float *primary, float *other_max)
{
    float a = fabsf(ctx->rate_lpf_dps[0]);
    float b = fabsf(ctx->rate_lpf_dps[1]);
    float c = fabsf(ctx->rate_lpf_dps[2]);

    switch (ctx->cfg.axis) {
    case APP_GRAVITY_GYRO_X:
        *primary   = ctx->rate_lpf_dps[0];
        *other_max = (b > c) ? b : c;
        break;
    case APP_GRAVITY_GYRO_Z:
        *primary   = ctx->rate_lpf_dps[2];
        *other_max = (a > b) ? a : b;
        break;
    case APP_GRAVITY_GYRO_Y:
    default:
        *primary   = ctx->rate_lpf_dps[1];
        *other_max = (a > c) ? a : c;
        break;
    }
}

/**
 * @brief Feed gyro sample (deg/s); return edge-triggered LEFT/RIGHT or NONE
 * @param[in,out] ctx context
 * @param[in] gx gyro X (deg/s)
 * @param[in] gy gyro Y (deg/s)
 * @param[in] gz gyro Z (deg/s)
 * @param[in] now_ms monotonic ms
 * @return APP_GRAVITY_GESTURE_* when |ω| crosses thresh with dominance
 */
APP_GRAVITY_GESTURE_E app_gravity_flick_update(APP_GRAVITY_FLICK_T *ctx, float gx, float gy, float gz,
                                               uint32_t now_ms)
{
    float a;
    float primary;
    float other_max;
    float signed_rate;
    float abs_rate;

    if (ctx == NULL) {
        return APP_GRAVITY_GESTURE_NONE;
    }

    a = ctx->cfg.lpf_alpha;
    ctx->rate_lpf_dps[0] = a * gx + (1.0f - a) * ctx->rate_lpf_dps[0];
    ctx->rate_lpf_dps[1] = a * gy + (1.0f - a) * ctx->rate_lpf_dps[1];
    ctx->rate_lpf_dps[2] = a * gz + (1.0f - a) * ctx->rate_lpf_dps[2];

    __flick_rates(ctx, &primary, &other_max);
    signed_rate = primary * (float)ctx->cfg.invert;
    abs_rate    = fabsf(signed_rate);

    if (abs_rate < ctx->cfg.release_dps) {
        ctx->armed  = 1;
        ctx->primed = 0;
        return APP_GRAVITY_GESTURE_NONE;
    }

    if (!ctx->armed) {
        return APP_GRAVITY_GESTURE_NONE;
    }
    if (now_ms < ctx->cooldown_until_ms) {
        return APP_GRAVITY_GESTURE_NONE;
    }
    if (abs_rate < ctx->cfg.thresh_dps) {
        return APP_GRAVITY_GESTURE_NONE;
    }
    /* Optional: reject if another axis is much stronger (0 = disabled) */
    if (ctx->cfg.dominance_ratio > 0.0f && abs_rate < ctx->cfg.dominance_ratio * other_max) {
        return APP_GRAVITY_GESTURE_NONE;
    }

    ctx->armed             = 0;
    ctx->primed            = 1;
    ctx->cooldown_until_ms = now_ms + ctx->cfg.cooldown_ms;
    if (signed_rate > 0.0f) {
        return APP_GRAVITY_GESTURE_RIGHT;
    }
    return APP_GRAVITY_GESTURE_LEFT;
}

/**
 * @brief Filtered primary-axis rate after invert (deg/s)
 * @param[in] ctx context
 * @return rate dps
 */
float app_gravity_flick_get_rate_dps(const APP_GRAVITY_FLICK_T *ctx)
{
    float primary;
    float other_max;

    if (ctx == NULL) {
        return 0.0f;
    }
    __flick_rates(ctx, &primary, &other_max);
    (void)other_max;
    return primary * (float)ctx->cfg.invert;
}

/**
 * @brief Fill shake cfg defaults (impulse / jerk, not tilt)
 * @param[out] cfg config
 * @return none
 */
void app_gravity_shake_cfg_default(APP_GRAVITY_SHAKE_CFG_T *cfg)
{
    if (cfg == NULL) {
        return;
    }
    cfg->accel_thresh_g   = 0.50f;
    cfg->accel_release_g  = 0.18f;
    cfg->gyro_thresh_dps  = 180.0f;
    cfg->gyro_release_dps = 45.0f;
    cfg->gravity_lpf      = 0.05f;
    cfg->dyn_lpf          = 0.40f;
    cfg->dominance_ratio  = 1.35f;
    cfg->accel_axis       = APP_GRAVITY_AXIS_X;
    cfg->gyro_axis        = APP_GRAVITY_AXIS_X;
    cfg->invert           = 1;
    cfg->cooldown_ms      = 450;
}

/**
 * @brief Reset arming / cooldown
 * @param[in,out] ctx context
 * @return none
 */
void app_gravity_shake_reset(APP_GRAVITY_SHAKE_T *ctx)
{
    if (ctx == NULL) {
        return;
    }
    memset(ctx->dyn_lpf, 0, sizeof(ctx->dyn_lpf));
    memset(ctx->gyro_lpf, 0, sizeof(ctx->gyro_lpf));
    ctx->armed             = 1;
    ctx->cooldown_until_ms = 0;
}

/**
 * @brief Initialize shake detector
 * @param[in,out] ctx context
 * @param[in] cfg config (NULL → defaults)
 * @return OPRT_OK on success
 */
OPERATE_RET app_gravity_shake_init(APP_GRAVITY_SHAKE_T *ctx, const APP_GRAVITY_SHAKE_CFG_T *cfg)
{
    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }
    memset(ctx, 0, sizeof(*ctx));
    if (cfg != NULL) {
        ctx->cfg = *cfg;
    } else {
        app_gravity_shake_cfg_default(&ctx->cfg);
    }
    if (ctx->cfg.accel_thresh_g < 0.15f) {
        ctx->cfg.accel_thresh_g = 0.15f;
    }
    if (ctx->cfg.accel_release_g <= 0.0f || ctx->cfg.accel_release_g >= ctx->cfg.accel_thresh_g) {
        ctx->cfg.accel_release_g = ctx->cfg.accel_thresh_g * 0.35f;
    }
    if (ctx->cfg.gyro_thresh_dps < 0.0f) {
        ctx->cfg.gyro_thresh_dps = 0.0f;
    }
    if (ctx->cfg.gyro_release_dps < 0.0f ||
        (ctx->cfg.gyro_thresh_dps > 0.0f && ctx->cfg.gyro_release_dps >= ctx->cfg.gyro_thresh_dps)) {
        ctx->cfg.gyro_release_dps = ctx->cfg.gyro_thresh_dps * 0.25f;
    }
    if (ctx->cfg.gravity_lpf <= 0.0f || ctx->cfg.gravity_lpf > 1.0f) {
        ctx->cfg.gravity_lpf = 0.05f;
    }
    if (ctx->cfg.dyn_lpf <= 0.0f || ctx->cfg.dyn_lpf > 1.0f) {
        ctx->cfg.dyn_lpf = 0.40f;
    }
    if (ctx->cfg.dominance_ratio > 0.0f && ctx->cfg.dominance_ratio < 1.0f) {
        ctx->cfg.dominance_ratio = 1.0f;
    }
    if (ctx->cfg.invert == 0) {
        ctx->cfg.invert = 1;
    }
    if (ctx->cfg.cooldown_ms == 0) {
        ctx->cfg.cooldown_ms = 350;
    }
    if ((unsigned)ctx->cfg.accel_axis > (unsigned)APP_GRAVITY_AXIS_Z) {
        ctx->cfg.accel_axis = APP_GRAVITY_AXIS_X;
    }
    if ((unsigned)ctx->cfg.gyro_axis > (unsigned)APP_GRAVITY_AXIS_Z) {
        ctx->cfg.gyro_axis = APP_GRAVITY_AXIS_X;
    }
    app_gravity_shake_reset(ctx);
    return OPRT_OK;
}

/**
 * @brief Pick component by axis
 * @param[in] axis axis
 * @param[in] x y z components
 * @return selected component
 */
static float __axis_pick(APP_GRAVITY_AXIS_E axis, float x, float y, float z)
{
    switch (axis) {
    case APP_GRAVITY_AXIS_Y:
        return y;
    case APP_GRAVITY_AXIS_Z:
        return z;
    case APP_GRAVITY_AXIS_X:
    default:
        return x;
    }
}

/**
 * @brief Max abs of the two axes that are not `axis`
 * @param[in] axis primary
 * @param[in] v0 v1 v2 components
 * @return other-axis max abs
 */
static float __axis_other_max(APP_GRAVITY_AXIS_E axis, float v0, float v1, float v2)
{
    float a = fabsf(v0);
    float b = fabsf(v1);
    float c = fabsf(v2);

    switch (axis) {
    case APP_GRAVITY_AXIS_Y:
        return (a > c) ? a : c;
    case APP_GRAVITY_AXIS_Z:
        return (a > b) ? a : b;
    case APP_GRAVITY_AXIS_X:
    default:
        return (b > c) ? b : c;
    }
}

/**
 * @brief Feed IMU sample; return edge-triggered POS/NEG or NONE
 * @param[in,out] ctx context
 * @param[in] ax ay az accel (g)
 * @param[in] gx gy gz gyro (dps)
 * @param[in] now_ms monotonic ms
 * @return APP_GRAVITY_SHAKE_*
 */
APP_GRAVITY_SHAKE_E app_gravity_shake_update(APP_GRAVITY_SHAKE_T *ctx, float ax, float ay, float az,
                                             float gx, float gy, float gz, uint32_t now_ms)
{
    float a;
    float dyn[3];
    float gyr[3];
    float primary = 0.0f;
    float other = 0.0f;
    float abs_primary = 0.0f;
    uint8_t use_gyro = 0;
    uint8_t accel_hit = 0;
    uint8_t gyro_hit = 0;

    if (ctx == NULL) {
        return APP_GRAVITY_SHAKE_NONE;
    }

    if (!ctx->base_ready) {
        ctx->accel_base[0] += ax;
        ctx->accel_base[1] += ay;
        ctx->accel_base[2] += az;
        ctx->base_count++;
        if (ctx->base_count >= 20) {
            ctx->accel_base[0] /= (float)ctx->base_count;
            ctx->accel_base[1] /= (float)ctx->base_count;
            ctx->accel_base[2] /= (float)ctx->base_count;
            ctx->base_ready = 1;
        }
        return APP_GRAVITY_SHAKE_NONE;
    }

    a = ctx->cfg.gravity_lpf;
    ctx->accel_base[0] = a * ax + (1.0f - a) * ctx->accel_base[0];
    ctx->accel_base[1] = a * ay + (1.0f - a) * ctx->accel_base[1];
    ctx->accel_base[2] = a * az + (1.0f - a) * ctx->accel_base[2];

    a = ctx->cfg.dyn_lpf;
    dyn[0] = ax - ctx->accel_base[0];
    dyn[1] = ay - ctx->accel_base[1];
    dyn[2] = az - ctx->accel_base[2];
    ctx->dyn_lpf[0] = a * dyn[0] + (1.0f - a) * ctx->dyn_lpf[0];
    ctx->dyn_lpf[1] = a * dyn[1] + (1.0f - a) * ctx->dyn_lpf[1];
    ctx->dyn_lpf[2] = a * dyn[2] + (1.0f - a) * ctx->dyn_lpf[2];

    ctx->gyro_lpf[0] = a * gx + (1.0f - a) * ctx->gyro_lpf[0];
    ctx->gyro_lpf[1] = a * gy + (1.0f - a) * ctx->gyro_lpf[1];
    ctx->gyro_lpf[2] = a * gz + (1.0f - a) * ctx->gyro_lpf[2];

    dyn[0] = ctx->dyn_lpf[0];
    dyn[1] = ctx->dyn_lpf[1];
    dyn[2] = ctx->dyn_lpf[2];
    gyr[0] = ctx->gyro_lpf[0];
    gyr[1] = ctx->gyro_lpf[1];
    gyr[2] = ctx->gyro_lpf[2];

    /* Re-arm when both channels quiet on primary axis */
    {
        float p_dyn = fabsf(__axis_pick(ctx->cfg.accel_axis, dyn[0], dyn[1], dyn[2]));
        float p_gyr = fabsf(__axis_pick(ctx->cfg.gyro_axis, gyr[0], gyr[1], gyr[2]));
        if (p_dyn < ctx->cfg.accel_release_g &&
            (ctx->cfg.gyro_thresh_dps <= 0.0f || p_gyr < ctx->cfg.gyro_release_dps)) {
            ctx->armed = 1;
            return APP_GRAVITY_SHAKE_NONE;
        }
    }

    if (!ctx->armed || now_ms < ctx->cooldown_until_ms) {
        return APP_GRAVITY_SHAKE_NONE;
    }

    /* Accel path with axis dominance */
    primary = __axis_pick(ctx->cfg.accel_axis, dyn[0], dyn[1], dyn[2]);
    abs_primary = fabsf(primary);
    other = __axis_other_max(ctx->cfg.accel_axis, dyn[0], dyn[1], dyn[2]);
    if (abs_primary >= ctx->cfg.accel_thresh_g) {
        if (ctx->cfg.dominance_ratio <= 0.0f || abs_primary >= ctx->cfg.dominance_ratio * other) {
            accel_hit = 1;
        }
    }

    /* Gyro path with axis dominance */
    if (ctx->cfg.gyro_thresh_dps > 0.0f) {
        float gp = __axis_pick(ctx->cfg.gyro_axis, gyr[0], gyr[1], gyr[2]);
        float ga = fabsf(gp);
        float go = __axis_other_max(ctx->cfg.gyro_axis, gyr[0], gyr[1], gyr[2]);
        if (ga >= ctx->cfg.gyro_thresh_dps) {
            if (ctx->cfg.dominance_ratio <= 0.0f || ga >= ctx->cfg.dominance_ratio * go) {
                gyro_hit = 1;
                if (!accel_hit || (ga / ctx->cfg.gyro_thresh_dps) > (abs_primary / ctx->cfg.accel_thresh_g)) {
                    use_gyro = 1;
                    primary = gp;
                }
            }
        }
    }

    if (!accel_hit && !gyro_hit) {
        return APP_GRAVITY_SHAKE_NONE;
    }

    if (!use_gyro) {
        primary = __axis_pick(ctx->cfg.accel_axis, dyn[0], dyn[1], dyn[2]);
    }
    primary *= (float)ctx->cfg.invert;

    ctx->armed = 0;
    ctx->cooldown_until_ms = now_ms + ctx->cfg.cooldown_ms;
    if (primary > 0.0f) {
        return APP_GRAVITY_SHAKE_POS;
    }
    return APP_GRAVITY_SHAKE_NEG;
}
