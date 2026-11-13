/**
 * @file tdd_motion_sc7a20.c
 * @brief SC7A20 motion sensor TDD adapter.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tdd_motion_sc7a20.h"
#include "tdd_motion_i2c.h"

#include "tal_api.h"
#include "tkl_gpio.h"
#include "sc7a20.h"

#define SC7A20_GRAVITY_MPS2 9.80665f

typedef struct {
    TDD_MOTION_I2C_CFG_T bus_cfg;
    TDD_SC7A20_CFG_T chip_cfg;
    TDD_MOTION_IRQ_CFG_T irq_cfg;
    bool opened;
    bool irq_ready;
} TDD_MOTION_SC7A20_CTX_T;

static SC7A20_ODR_E __sc7a20_odr_from_hz(uint16_t sample_rate_hz)
{
    if (sample_rate_hz <= 1U) {
        return SC7A20_ODR_1HZ;
    } else if (sample_rate_hz <= 10U) {
        return SC7A20_ODR_10HZ;
    } else if (sample_rate_hz <= 25U) {
        return SC7A20_ODR_25HZ;
    } else if (sample_rate_hz <= 50U) {
        return SC7A20_ODR_50HZ;
    } else if (sample_rate_hz <= 100U) {
        return SC7A20_ODR_100HZ;
    } else if (sample_rate_hz <= 200U) {
        return SC7A20_ODR_200HZ;
    }
    return SC7A20_ODR_400HZ;
}

static void __sc7a20_fill_chip_cfg(TDD_MOTION_SC7A20_CTX_T *ctx,
                                   const TDL_MOTION_CFG_T *cfg)
{
    ctx->chip_cfg.i2c_port = ctx->bus_cfg.port;
    ctx->chip_cfg.i2c_addr = ctx->bus_cfg.addr;
    ctx->chip_cfg.i2c_speed_hz = ctx->bus_cfg.speed_hz;
    ctx->chip_cfg.range = (SC7A20_RANGE_E)cfg->accel_range;
    ctx->chip_cfg.odr = __sc7a20_odr_from_hz(cfg->sample_rate_hz);
    ctx->chip_cfg.low_power = cfg->low_power;
    ctx->chip_cfg.hpf_data = false;
}

static void __sc7a20_int_irq(void *arg)
{
    TDD_MOTION_SC7A20_CTX_T *ctx = (TDD_MOTION_SC7A20_CTX_T *)arg;

    if (ctx->irq_cfg.cb != NULL) {
        ctx->irq_cfg.cb(ctx->irq_cfg.arg, TDL_MOTION_EVENT_DATA_READY);
    }
}

static OPERATE_RET __sc7a20_int_open(TDD_MOTION_SC7A20_CTX_T *ctx)
{
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .direct = TUYA_GPIO_INPUT,
        .mode = ctx->bus_cfg.int1_active_level == TUYA_GPIO_LEVEL_HIGH ?
                TUYA_GPIO_PULLDOWN : TUYA_GPIO_PULLUP,
        .level = TUYA_GPIO_LEVEL_NONE,
    };
    TUYA_GPIO_IRQ_T irq_cfg = {
        .mode = ctx->bus_cfg.int1_active_level == TUYA_GPIO_LEVEL_HIGH ?
                TUYA_GPIO_IRQ_RISE : TUYA_GPIO_IRQ_FALL,
        .cb = __sc7a20_int_irq,
        .arg = ctx,
    };
    OPERATE_RET rt;
    TUYA_GPIO_LEVEL_E int_level = TUYA_GPIO_LEVEL_NONE;

    if (!ctx->bus_cfg.int1_enabled || ctx->bus_cfg.int1_pin >= TUYA_GPIO_NUM_MAX) {
        return OPRT_NOT_SUPPORTED;
    }
    if (ctx->bus_cfg.int1_active_level == TUYA_GPIO_LEVEL_NONE) {
        return OPRT_INVALID_PARM;
    }

    rt = tdd_sc7a20_int_polarity_config(
        &ctx->chip_cfg,
        ctx->bus_cfg.int1_active_level == TUYA_GPIO_LEVEL_LOW);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = tdd_sc7a20_data_ready_int_config(&ctx->chip_cfg, true);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = tkl_gpio_init(ctx->bus_cfg.int1_pin, &gpio_cfg);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = tkl_gpio_irq_init(ctx->bus_cfg.int1_pin, &irq_cfg);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = tkl_gpio_irq_enable(ctx->bus_cfg.int1_pin);
    if (rt != OPRT_OK) {
        (void)tkl_gpio_deinit(ctx->bus_cfg.int1_pin);
        return rt;
    }

    (void)tkl_gpio_read(ctx->bus_cfg.int1_pin, &int_level);
    PR_NOTICE("SC7A20 INT1 pin=%u idle_level=%u trigger=%u",
              (unsigned)ctx->bus_cfg.int1_pin, (unsigned)int_level,
              (unsigned)irq_cfg.mode);

    ctx->irq_ready = true;
    return OPRT_OK;
}

static void __sc7a20_int_close(TDD_MOTION_SC7A20_CTX_T *ctx)
{
    if (!ctx->irq_ready) {
        return;
    }

    (void)tkl_gpio_irq_disable(ctx->bus_cfg.int1_pin);
    (void)tkl_gpio_deinit(ctx->bus_cfg.int1_pin);
    (void)tdd_sc7a20_data_ready_int_config(&ctx->chip_cfg, false);
    ctx->irq_ready = false;
}

static OPERATE_RET __sc7a20_open(void *dev_ctx, const TDL_MOTION_OPEN_CFG_T *cfg,
                                 const TDD_MOTION_IRQ_CFG_T *irq_cfg)
{
    TDD_MOTION_SC7A20_CTX_T *ctx = (TDD_MOTION_SC7A20_CTX_T *)dev_ctx;
    OPERATE_RET rt;

    if (ctx == NULL || cfg == NULL || ctx->opened) {
        return OPRT_INVALID_PARM;
    }

    __sc7a20_fill_chip_cfg(ctx, &cfg->sample_cfg);
    rt = tdd_sc7a20_init(&ctx->chip_cfg);
    if (rt != OPRT_OK) {
        return rt;
    }

    ctx->irq_cfg.cb = NULL;
    ctx->irq_cfg.arg = NULL;
    if (cfg->events_enabled) {
        if (irq_cfg == NULL || irq_cfg->cb == NULL) {
            (void)tdd_sc7a20_deinit(&ctx->chip_cfg);
            return OPRT_INVALID_PARM;
        }
        ctx->irq_cfg = *irq_cfg;
        rt = __sc7a20_int_open(ctx);
        if (rt != OPRT_OK) {
            ctx->irq_cfg.cb = NULL;
            ctx->irq_cfg.arg = NULL;
            (void)tdd_sc7a20_deinit(&ctx->chip_cfg);
            return rt;
        }
    }

    ctx->opened = true;
    return OPRT_OK;
}

static OPERATE_RET __sc7a20_close(void *dev_ctx)
{
    TDD_MOTION_SC7A20_CTX_T *ctx = (TDD_MOTION_SC7A20_CTX_T *)dev_ctx;
    OPERATE_RET rt;

    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (!ctx->opened) {
        return OPRT_OK;
    }

    __sc7a20_int_close(ctx);
    rt = tdd_sc7a20_deinit(&ctx->chip_cfg);
    ctx->irq_cfg.cb = NULL;
    ctx->irq_cfg.arg = NULL;
    ctx->opened = false;
    return rt;
}

static OPERATE_RET __sc7a20_config(void *dev_ctx, const TDL_MOTION_CFG_T *cfg)
{
    TDD_MOTION_SC7A20_CTX_T *ctx = (TDD_MOTION_SC7A20_CTX_T *)dev_ctx;
    OPERATE_RET rt;

    if (ctx == NULL || cfg == NULL || !ctx->opened) {
        return OPRT_INVALID_PARM;
    }

    __sc7a20_fill_chip_cfg(ctx, cfg);
    rt = tdd_sc7a20_init(&ctx->chip_cfg);
    if (rt == OPRT_OK && ctx->irq_ready) {
        rt = tdd_sc7a20_data_ready_int_config(&ctx->chip_cfg, true);
    }
    return rt;
}

static OPERATE_RET __sc7a20_read(void *dev_ctx, TDL_MOTION_DATA_T *data)
{
    TDD_MOTION_SC7A20_CTX_T *ctx = (TDD_MOTION_SC7A20_CTX_T *)dev_ctx;
    SC7A20_ACCEL_DATA_T mg;
    OPERATE_RET rt;

    if (ctx == NULL || data == NULL || !ctx->opened) {
        return OPRT_INVALID_PARM;
    }

    rt = tdd_sc7a20_read_mg(&ctx->chip_cfg, &mg);
    if (rt != OPRT_OK) {
        return rt;
    }

    data->accel_mps2.x = (float)mg.x * SC7A20_GRAVITY_MPS2 / 1000.0f;
    data->accel_mps2.y = (float)mg.y * SC7A20_GRAVITY_MPS2 / 1000.0f;
    data->accel_mps2.z = (float)mg.z * SC7A20_GRAVITY_MPS2 / 1000.0f;
    data->accel_valid = true;
    return OPRT_OK;
}

OPERATE_RET tdd_motion_sc7a20_register(char *name,
                                       const TDD_MOTION_I2C_CFG_T *cfg)
{
    TDD_MOTION_SC7A20_CTX_T *ctx;
    TDD_MOTION_INTFS_T intfs = {
        .open = __sc7a20_open,
        .close = __sc7a20_close,
        .read = __sc7a20_read,
        .config = __sc7a20_config,
    };
    OPERATE_RET rt;

    if (name == NULL || cfg == NULL || cfg->port >= TUYA_I2C_NUM_MAX ||
        cfg->addr == 0U) {
        return OPRT_INVALID_PARM;
    }

    rt = tdd_motion_i2c_pinmux_config(cfg);
    if (rt != OPRT_OK) {
        return rt;
    }

    ctx = (TDD_MOTION_SC7A20_CTX_T *)tal_malloc(sizeof(TDD_MOTION_SC7A20_CTX_T));
    if (ctx == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->bus_cfg = *cfg;

    rt = tdl_motion_device_register(name, ctx, &intfs,
                                    TDL_MOTION_CAP_ACCEL | TDL_MOTION_CAP_DATA_READY);
    if (rt != OPRT_OK) {
        tal_free(ctx);
    }
    return rt;
}
