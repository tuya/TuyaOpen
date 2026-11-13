/**
 * @file tdl_motion_driver.h
 * @brief Tuya Device Driver motion sensor contract.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef _TDL_MOTION_DRIVER_H_
#define _TDL_MOTION_DRIVER_H_

#include "tuya_cloud_types.h"
#include "tdl_motion_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*TDD_MOTION_IRQ_CB)(void *arg, TDL_MOTION_EVENT_E event);

typedef struct {
    TDD_MOTION_IRQ_CB cb;
    void *arg;
} TDD_MOTION_IRQ_CFG_T;

typedef struct {
    TUYA_I2C_NUM_E port;
    TUYA_GPIO_NUM_E scl_pin;
    TUYA_GPIO_NUM_E sda_pin;
    uint8_t addr;
    uint32_t speed_hz;

    TUYA_GPIO_NUM_E int1_pin;
    TUYA_GPIO_LEVEL_E int1_active_level;
    bool int1_enabled;
} TDD_MOTION_I2C_CFG_T;

typedef struct {
    OPERATE_RET (*open)(void *dev_ctx, const TDL_MOTION_OPEN_CFG_T *cfg,
                        const TDD_MOTION_IRQ_CFG_T *irq_cfg);
    OPERATE_RET (*close)(void *dev_ctx);
    OPERATE_RET (*read)(void *dev_ctx, TDL_MOTION_DATA_T *data);
    OPERATE_RET (*config)(void *dev_ctx, const TDL_MOTION_CFG_T *cfg);
} TDD_MOTION_INTFS_T;

/**
 * @brief Register a motion hardware driver.
 */
OPERATE_RET tdl_motion_device_register(char *name, void *dev_ctx,
                                       const TDD_MOTION_INTFS_T *intfs,
                                       uint32_t capabilities);

#ifdef __cplusplus
}
#endif

#endif /* _TDL_MOTION_DRIVER_H_ */
