/**
 * @file tdd_ir_driver.h
 * @brief Tuya Device Driver (TDD) infrared hardware driver interface.
 *
 * This header file defines the hardware abstraction layer for infrared
 * communication drivers within the Tuya IoT platform. It provides a
 * standardized interface for registering and configuring IR hardware
 * drivers that support different timer configurations and GPIO pin
 * assignments for IR transmission and reception.
 *
 * The driver interface supports multiple hardware configurations:
 * - Single timer mode for basic IR operations
 * - Dual timer mode for advanced timing precision
 * - Capture mode for high-precision IR signal analysis
 * - Configurable GPIO pins for IR transmit and receive
 * - PWM duty cycle control for IR carrier frequency generation
 *
 * This module serves as the hardware abstraction layer between the
 * TDL (Tuya Device Layer) IR management system and platform-specific
 * hardware implementations, enabling consistent IR functionality
 * across different Tuya-supported microcontroller platforms.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDD_IR_DRIVER_H__
#define __TDD_IR_DRIVER_H__

#include "tdl_ir_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/

typedef unsigned char IR_DRIVER_TYPE_E;
#define IR_DRV_SINGLE_TIMER     0
#define IR_DRV_DUAL_TIMER       1
#define IR_DRV_CAPTURE          2
#define IR_DRV_TYPE_MAX         3

typedef struct {
    int send_pin; /* This pin should support 38kHz pwm output */
    int recv_pin;
    int send_timer;
    int recv_timer; /* Use only in IR_DRV_DUAL_TIMER */
    int send_duty;
} IR_DRV_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief register ir driver, maximum 16 bytes
 *
 * @param[in] dev_name: device name, 
 * @param[in] drv_cfg: driver config params
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
int tdd_ir_driver_register(char *dev_name, IR_DRIVER_TYPE_E driver_type, IR_DRV_CFG_T drv_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_IR_DRIVER_H__ */
