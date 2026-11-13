/**
 * @file tdl_encoder_driver.h
 * @brief Tuya Device Driver encoder driver contract.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef _TDL_ENCODER_DRIVER_H_
#define _TDL_ENCODER_DRIVER_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TDD_ENCODER_IRQ_SOURCE_ROTATE = 0,
    TDD_ENCODER_IRQ_SOURCE_BUTTON,
} TDD_ENCODER_IRQ_SOURCE_E;

typedef void (*TDD_ENCODER_IRQ_CB)(void *arg, TDD_ENCODER_IRQ_SOURCE_E source);

typedef struct {
    TDD_ENCODER_IRQ_CB cb;
    void *arg;
} TDD_ENCODER_IRQ_CFG_T;

typedef struct {
    /* Hardware and mechanical traits supplied by the registered TDD device. */
    uint16_t debounce_ms;
    uint8_t steps_per_detent;
    bool reverse;
} TDL_ENCODER_HW_CFG_T;

typedef struct {
    /* Raw quadrature transitions accumulated since the previous read. */
    int32_t delta;
    /* Raw active state of the encoder button, before TDL debouncing. */
    uint8_t button_pressed;
} TDD_ENCODER_DATA_T;

typedef struct {
    /* irq_cfg is optional and may be NULL for polled hardware drivers. */
    OPERATE_RET (*open)(void *dev_ctx, const TDD_ENCODER_IRQ_CFG_T *irq_cfg);
    OPERATE_RET (*close)(void *dev_ctx);
    OPERATE_RET (*read)(void *dev_ctx, TDD_ENCODER_DATA_T *data);
} TDD_ENCODER_INTFS_T;

/**
 * @brief Register an encoder hardware driver.
 *
 * @param[in] name Encoder device name.
 * @param[in] dev_ctx Hardware driver context.
 * @param[in] hw_cfg Hardware and mechanical traits used by TDL.
 * @param[in] intfs Driver interface table.
 *
 * @return OPRT_OK on success, otherwise an error code.
 */
OPERATE_RET tdl_encoder_device_register(char *name, void *dev_ctx,
                                        const TDL_ENCODER_HW_CFG_T *hw_cfg,
                                        const TDD_ENCODER_INTFS_T *intfs);

#ifdef __cplusplus
}
#endif

#endif /* _TDL_ENCODER_DRIVER_H_ */
