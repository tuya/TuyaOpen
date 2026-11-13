/**
 * @file tdd_encoder_gpio.c
 * @brief GPIO-based rotary encoder hardware driver.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tdd_encoder_gpio.h"
#include "tdl_encoder_driver.h"

#include "tal_api.h"
#include "tkl_gpio.h"

#define TDD_ENCODER_GPIO_A      0x01U
#define TDD_ENCODER_GPIO_B      0x02U
#define TDD_ENCODER_GPIO_BUTTON 0x04U

typedef struct {
    TDD_ENCODER_GPIO_CFG_T cfg;
    TDD_ENCODER_IRQ_CFG_T irq_cfg;
    volatile int32_t pending_delta;
    volatile uint8_t state;
    uint8_t gpio_ready;
    uint8_t irq_ready;
    bool opened;
} TDD_ENCODER_GPIO_CTX_T;

static const int8_t s_encoder_transition[16] = {
     0,  1, -1,  0,
    -1,  0,  0,  1,
     1,  0,  0, -1,
     0, -1,  1,  0,
};

static uint8_t __tdd_encoder_read_state(TDD_ENCODER_GPIO_CTX_T *ctx)
{
    TUYA_GPIO_LEVEL_E a_level;
    TUYA_GPIO_LEVEL_E b_level;

    if (tkl_gpio_read(ctx->cfg.pin_a, &a_level) != OPRT_OK ||
        tkl_gpio_read(ctx->cfg.pin_b, &b_level) != OPRT_OK) {
        return ctx->state;
    }

    return (uint8_t)((a_level == TUYA_GPIO_LEVEL_HIGH ? 0x02U : 0U) |
                     (b_level == TUYA_GPIO_LEVEL_HIGH ? 0x01U : 0U));
}

static uint8_t __tdd_encoder_read_button(TDD_ENCODER_GPIO_CTX_T *ctx)
{
    TUYA_GPIO_LEVEL_E level;

    if (!ctx->cfg.button_enabled ||
        tkl_gpio_read(ctx->cfg.pin_button, &level) != OPRT_OK) {
        return 0U;
    }

    return level == ctx->cfg.button_active_level ? 1U : 0U;
}

static void __tdd_encoder_process_state(TDD_ENCODER_GPIO_CTX_T *ctx,
                                        uint8_t current, bool notify)
{
    uint8_t previous = ctx->state;
    int8_t direction;

    if (previous == current) {
        return;
    }

    ctx->state = current;
    direction = s_encoder_transition[((uint32_t)previous << 2U) | current];
    if (direction > 0 && ctx->pending_delta < INT32_MAX) {
        ctx->pending_delta++;
    } else if (direction < 0 && ctx->pending_delta > INT32_MIN) {
        ctx->pending_delta--;
    } else {
        return;
    }

    if (notify && ctx->irq_cfg.cb != NULL) {
        ctx->irq_cfg.cb(ctx->irq_cfg.arg, TDD_ENCODER_IRQ_SOURCE_ROTATE);
    }
}

static void __tdd_encoder_process_phase_irq(TDD_ENCODER_GPIO_CTX_T *ctx,
                                            TUYA_GPIO_NUM_E pin,
                                            bool pin_a)
{
    TUYA_GPIO_LEVEL_E level;
    uint8_t previous = ctx->state;
    uint8_t current;

    if (tkl_gpio_read(pin, &level) != OPRT_OK) {
        return;
    }

    if (pin_a) {
        current = (uint8_t)((level == TUYA_GPIO_LEVEL_HIGH ? 0x02U : 0U) |
                            (previous & 0x01U));
    } else {
        current = (uint8_t)((previous & 0x02U) |
                            (level == TUYA_GPIO_LEVEL_HIGH ? 0x01U : 0U));
    }
    __tdd_encoder_process_state(ctx, current, true);
}

static void __tdd_encoder_a_irq(void *arg)
{
    TDD_ENCODER_GPIO_CTX_T *ctx = (TDD_ENCODER_GPIO_CTX_T *)arg;

    __tdd_encoder_process_phase_irq(ctx, ctx->cfg.pin_a, true);
}

static void __tdd_encoder_b_irq(void *arg)
{
    TDD_ENCODER_GPIO_CTX_T *ctx = (TDD_ENCODER_GPIO_CTX_T *)arg;

    __tdd_encoder_process_phase_irq(ctx, ctx->cfg.pin_b, false);
}

static void __tdd_encoder_button_irq(void *arg)
{
    TDD_ENCODER_GPIO_CTX_T *ctx = (TDD_ENCODER_GPIO_CTX_T *)arg;

    if (ctx->irq_cfg.cb != NULL) {
        ctx->irq_cfg.cb(ctx->irq_cfg.arg, TDD_ENCODER_IRQ_SOURCE_BUTTON);
    }
}

static OPERATE_RET __tdd_encoder_pin_init(TDD_ENCODER_GPIO_CTX_T *ctx,
                                           TUYA_GPIO_NUM_E pin,
                                           TUYA_GPIO_MODE_E mode,
                                           uint8_t flag)
{
    TUYA_GPIO_BASE_CFG_T gpio_cfg = {
        .mode = mode,
        .direct = TUYA_GPIO_INPUT,
        .level = TUYA_GPIO_LEVEL_HIGH,
    };
    OPERATE_RET rt;

    rt = tkl_gpio_init(pin, &gpio_cfg);
    if (rt == OPRT_OK) {
        ctx->gpio_ready |= flag;
    }
    return rt;
}

static OPERATE_RET __tdd_encoder_pin_irq_init(TDD_ENCODER_GPIO_CTX_T *ctx,
                                               TUYA_GPIO_NUM_E pin,
                                               TUYA_GPIO_IRQ_CB cb,
                                               uint8_t flag)
{
    TUYA_GPIO_IRQ_T irq_cfg = {
        .mode = TUYA_GPIO_IRQ_RISE_FALL,
        .cb = cb,
        .arg = ctx,
    };
    OPERATE_RET rt;

    rt = tkl_gpio_irq_init(pin, &irq_cfg);
    if (rt == OPRT_OK) {
        ctx->irq_ready |= flag;
    }
    return rt;
}

static void __tdd_encoder_gpio_close(TDD_ENCODER_GPIO_CTX_T *ctx)
{
    if (ctx->irq_ready & TDD_ENCODER_GPIO_A) {
        (void)tkl_gpio_irq_disable(ctx->cfg.pin_a);
    }
    if (ctx->irq_ready & TDD_ENCODER_GPIO_B) {
        (void)tkl_gpio_irq_disable(ctx->cfg.pin_b);
    }
    if (ctx->irq_ready & TDD_ENCODER_GPIO_BUTTON) {
        (void)tkl_gpio_irq_disable(ctx->cfg.pin_button);
    }

    if (ctx->gpio_ready & TDD_ENCODER_GPIO_A) {
        (void)tkl_gpio_deinit(ctx->cfg.pin_a);
    }
    if (ctx->gpio_ready & TDD_ENCODER_GPIO_B) {
        (void)tkl_gpio_deinit(ctx->cfg.pin_b);
    }
    if (ctx->gpio_ready & TDD_ENCODER_GPIO_BUTTON) {
        (void)tkl_gpio_deinit(ctx->cfg.pin_button);
    }

    ctx->gpio_ready = 0U;
    ctx->irq_ready = 0U;
    ctx->pending_delta = 0;
    ctx->opened = false;
}

static OPERATE_RET __tdd_encoder_open(void *dev_ctx,
                                      const TDD_ENCODER_IRQ_CFG_T *irq_cfg)
{
    TDD_ENCODER_GPIO_CTX_T *ctx = (TDD_ENCODER_GPIO_CTX_T *)dev_ctx;
    OPERATE_RET rt;

    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (ctx->opened) {
        return OPRT_COM_ERROR;
    }

    if (ctx->cfg.irq_enabled && irq_cfg != NULL) {
        ctx->irq_cfg = *irq_cfg;
    } else {
        memset(&ctx->irq_cfg, 0, sizeof(ctx->irq_cfg));
    }
    ctx->pending_delta = 0;

    rt = __tdd_encoder_pin_init(ctx, ctx->cfg.pin_a, ctx->cfg.pin_mode,
                                TDD_ENCODER_GPIO_A);
    if (rt != OPRT_OK) {
        goto __ERROR;
    }
    rt = __tdd_encoder_pin_init(ctx, ctx->cfg.pin_b, ctx->cfg.pin_mode,
                                TDD_ENCODER_GPIO_B);
    if (rt != OPRT_OK) {
        goto __ERROR;
    }
    if (ctx->cfg.button_enabled) {
        rt = __tdd_encoder_pin_init(ctx, ctx->cfg.pin_button,
                                    ctx->cfg.button_pin_mode,
                                    TDD_ENCODER_GPIO_BUTTON);
        if (rt != OPRT_OK) {
            goto __ERROR;
        }
    }

    ctx->state = __tdd_encoder_read_state(ctx);

    if (ctx->cfg.irq_enabled) {
        rt = __tdd_encoder_pin_irq_init(ctx, ctx->cfg.pin_a,
                                        __tdd_encoder_a_irq, TDD_ENCODER_GPIO_A);
        if (rt != OPRT_OK) {
            goto __ERROR;
        }
        rt = __tdd_encoder_pin_irq_init(ctx, ctx->cfg.pin_b,
                                        __tdd_encoder_b_irq, TDD_ENCODER_GPIO_B);
        if (rt != OPRT_OK) {
            goto __ERROR;
        }
        if (ctx->cfg.button_enabled) {
            rt = __tdd_encoder_pin_irq_init(ctx, ctx->cfg.pin_button,
                                            __tdd_encoder_button_irq,
                                            TDD_ENCODER_GPIO_BUTTON);
            if (rt != OPRT_OK) {
                goto __ERROR;
            }
        }

        rt = tkl_gpio_irq_enable(ctx->cfg.pin_a);
        if (rt != OPRT_OK) {
            goto __ERROR;
        }
        rt = tkl_gpio_irq_enable(ctx->cfg.pin_b);
        if (rt != OPRT_OK) {
            goto __ERROR;
        }
        if (ctx->cfg.button_enabled) {
            rt = tkl_gpio_irq_enable(ctx->cfg.pin_button);
            if (rt != OPRT_OK) {
                goto __ERROR;
            }
        }
    }

    ctx->opened = true;
    return OPRT_OK;

__ERROR:
    __tdd_encoder_gpio_close(ctx);
    return rt;
}

static OPERATE_RET __tdd_encoder_close(void *dev_ctx)
{
    TDD_ENCODER_GPIO_CTX_T *ctx = (TDD_ENCODER_GPIO_CTX_T *)dev_ctx;

    if (ctx == NULL) {
        return OPRT_INVALID_PARM;
    }

    __tdd_encoder_gpio_close(ctx);
    return OPRT_OK;
}

static OPERATE_RET __tdd_encoder_read(void *dev_ctx, TDD_ENCODER_DATA_T *data)
{
    TDD_ENCODER_GPIO_CTX_T *ctx = (TDD_ENCODER_GPIO_CTX_T *)dev_ctx;
    uint32_t irq_mask;

    if (ctx == NULL || data == NULL || !ctx->opened) {
        return OPRT_INVALID_PARM;
    }

    if (!ctx->cfg.irq_enabled) {
        __tdd_encoder_process_state(ctx, __tdd_encoder_read_state(ctx), false);
    }

    irq_mask = tal_system_enter_critical();
    data->delta = ctx->pending_delta;
    ctx->pending_delta = 0;
    tal_system_exit_critical(irq_mask);
    data->button_pressed = __tdd_encoder_read_button(ctx);
    return OPRT_OK;
}

OPERATE_RET tdd_encoder_gpio_register(char *name, TDD_ENCODER_GPIO_CFG_T *cfg)
{
    TDD_ENCODER_GPIO_CTX_T *ctx;
    TDL_ENCODER_HW_CFG_T hw_cfg;
    TDD_ENCODER_INTFS_T intfs = {
        .open = __tdd_encoder_open,
        .close = __tdd_encoder_close,
        .read = __tdd_encoder_read,
    };
    OPERATE_RET rt;

    if (name == NULL || cfg == NULL || cfg->pin_a >= TUYA_GPIO_NUM_MAX ||
        cfg->pin_b >= TUYA_GPIO_NUM_MAX || cfg->pin_a == cfg->pin_b ||
        cfg->button_active_level == TUYA_GPIO_LEVEL_NONE) {
        return OPRT_INVALID_PARM;
    }
    if (cfg->steps_per_detent == 0U || cfg->steps_per_detent > 64U) {
        return OPRT_INVALID_PARM;
    }
    if (cfg->button_enabled &&
        (cfg->pin_button >= TUYA_GPIO_NUM_MAX ||
         cfg->pin_button == cfg->pin_a || cfg->pin_button == cfg->pin_b)) {
        return OPRT_INVALID_PARM;
    }
    if (cfg->pin_mode == TUYA_GPIO_PUSH_PULL ||
        cfg->pin_mode == TUYA_GPIO_OPENDRAIN ||
        cfg->pin_mode == TUYA_GPIO_OPENDRAIN_PULLUP ||
        cfg->button_pin_mode == TUYA_GPIO_PUSH_PULL ||
        cfg->button_pin_mode == TUYA_GPIO_OPENDRAIN ||
        cfg->button_pin_mode == TUYA_GPIO_OPENDRAIN_PULLUP) {
        return OPRT_INVALID_PARM;
    }

    ctx = (TDD_ENCODER_GPIO_CTX_T *)tal_malloc(sizeof(TDD_ENCODER_GPIO_CTX_T));
    if (ctx == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(ctx, 0, sizeof(TDD_ENCODER_GPIO_CTX_T));
    ctx->cfg = *cfg;
    hw_cfg.debounce_ms = cfg->debounce_ms;
    hw_cfg.steps_per_detent = cfg->steps_per_detent;
    hw_cfg.reverse = cfg->reverse;

    rt = tdl_encoder_device_register(name, ctx, &hw_cfg, &intfs);
    if (rt != OPRT_OK) {
        tal_free(ctx);
    }
    return rt;
}
