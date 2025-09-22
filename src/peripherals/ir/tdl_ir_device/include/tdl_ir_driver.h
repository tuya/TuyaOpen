/**
 * @file tdl_ir_driver.h
 * @brief Tuya Device Layer (TDL) infrared driver interface definitions.
 *
 * This header file defines the interface for the TDL infrared driver subsystem,
 * providing abstractions for IR transmission and reception operations. It 
 * establishes the foundation for infrared communication functionality within
 * the Tuya IoT platform, supporting various IR modes including receive-only,
 * send-only, and bidirectional communication.
 *
 * The driver interface includes:
 * - IR driver operation modes (receive, transmit, bidirectional)
 * - Callback mechanisms for transmission completion and data reception
 * - Hardware abstraction through the TDD (Tuya Device Driver) interface
 * - State management for IR driver operations
 *
 * This module serves as the bridge between application-level IR functionality
 * and low-level hardware drivers, providing a standardized interface for
 * infrared communication across different Tuya device platforms.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_IR_DRIVER_H__
#define __TDL_IR_DRIVER_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define IR_INPUT_INVALID        (-1)

/* ir mode */
typedef unsigned char           IR_MODE_E;
#define IR_MODE_RECV_ONLY       0
#define IR_MODE_SEND_ONLY       1
#define IR_MODE_SEND_RECV       2
#define IR_MODE_MAX             3

typedef unsigned char           IR_DRIVER_STATE_E;
#define IR_DRV_PRE_SEND_STATE       0
#define IR_DRV_SEND_FINISH_STATE    1
#define IR_DRV_PRE_RECV_STATE       2
#define IR_DRV_RECV_FINISH_STATE    3
#define IR_DRV_SEND_HW_RESET        4
#define IR_DRV_RECV_HW_INIT         5
#define IR_DRV_RECV_HW_DEINIT       6
#define IR_DRV_IRQ_ENABLE_TIME_SET  7 // for/bk7231n
// #define IR_DRV_CODE_INTER_DELAY_SET 8 // Set the delay between continuous transmission of infrared code

/***********************************************************
***********************typedef define***********************
***********************************************************/

typedef void *IR_DRV_HANDLE_T;

typedef int (*IR_DRV_OUTPUT_FINISH_CB)(void *args);
typedef int (*IR_DRV_RECV_CB)(IR_DRV_HANDLE_T drv_hdl, unsigned int raw_data, void *args);

/* tdl processing callbacks */
typedef struct {
    IR_DRV_OUTPUT_FINISH_CB output_finish_cb;
    IR_DRV_RECV_CB          recv_cb;
}IR_TDL_TRANS_CB;

typedef struct {
    int (*open)(IR_DRV_HANDLE_T drv_hdl, unsigned char mode, IR_TDL_TRANS_CB ir_tdl_cb, void *args);
    int (*close)(IR_DRV_HANDLE_T drv_hdl, unsigned char mode);
    int (*output)(IR_DRV_HANDLE_T drv_hdl, unsigned int freq, unsigned char is_active, unsigned int time_us);

    int (*status_notif)(IR_DRV_HANDLE_T drv_hdl, IR_DRIVER_STATE_E state, void *args);
} TDD_IR_INTFS_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief register ir device
 *
 * @param[in] dev_name: device name
 * @param[in] cfg: device config params
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdl_ir_dev_register(char *dev_name, IR_DRV_HANDLE_T drv_hdl, TDD_IR_INTFS_T *ir_intfs);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_IR_DRIVER_H__ */
