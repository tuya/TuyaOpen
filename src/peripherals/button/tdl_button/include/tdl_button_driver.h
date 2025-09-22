/**
 * @file tdl_button_driver.h
 * @brief Tuya Driver Layer button driver interface definitions.
 *
 * This file defines the driver interface for the Tuya Driver Layer (TDL) button
 * subsystem. It provides the core structures, types, and function prototypes
 * that button device drivers must implement to integrate with the TDL button
 * management system. The interface supports both timer-based scanning and
 * interrupt-driven button detection modes.
 *
 * Key features:
 * - Support for multiple button operation modes (timer scan and interrupt)
 * - Device handle abstraction for hardware independence
 * - Callback mechanism for interrupt-driven button events
 * - Driver registration interface for different button implementations
 * - Unified control interface for button operations
 *
 * This layer acts as an abstraction between hardware-specific button implementations
 * (TDD layer) and higher-level button event processing in applications.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef _TDL_BUTTON_DRIVER_H_
#define _TDL_BUTTON_DRIVER_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *DEVICE_BUTTON_HANDLE;
typedef void (*TDL_BUTTON_CB)(void *arg);

typedef enum {
    BUTTON_TIMER_SCAN_MODE = 0,
    BUTTON_IRQ_MODE,
} TDL_BUTTON_MODE_E;

typedef struct {
    DEVICE_BUTTON_HANDLE dev_handle; // tdd handle
    TDL_BUTTON_CB irq_cb;            // irq cb
} TDL_BUTTON_OPRT_INFO;

typedef struct {
    OPERATE_RET (*button_create)(TDL_BUTTON_OPRT_INFO *dev);
    OPERATE_RET (*button_delete)(TDL_BUTTON_OPRT_INFO *dev);
    OPERATE_RET (*read_value)(TDL_BUTTON_OPRT_INFO *dev, uint8_t *value);
} TDL_BUTTON_CTRL_INFO;

typedef struct {
    void *dev_handle;
    TDL_BUTTON_MODE_E mode;
} TDL_BUTTON_DEVICE_INFO_T;

// Button software configuration
OPERATE_RET tdl_button_register(char *name, TDL_BUTTON_CTRL_INFO *button_ctrl_info,
                                TDL_BUTTON_DEVICE_INFO_T *button_cfg_info);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*_TDL_BUTTON_H_*/