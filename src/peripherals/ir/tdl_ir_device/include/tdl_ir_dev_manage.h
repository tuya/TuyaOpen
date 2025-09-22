/**
 * @file tdl_ir_dev_manage.h
 * @brief Infrared device management interface for Tuya IoT devices.
 *
 * This file provides comprehensive device management functionality for infrared
 * communication within the Tuya IoT ecosystem. It implements a complete infrared
 * device management system that supports multiple IR protocols (NEC, timecode),
 * device registration, and both synchronous and asynchronous communication modes.
 *
 * Key functionalities provided:
 * - IR device discovery, registration, and lifecycle management
 * - Support for multiple IR protocols (NEC protocol, raw timecode)
 * - Bidirectional IR communication (transmit and receive)
 * - Protocol-specific configuration and error handling
 * - Queue-based data management for IR receive operations
 * - Callback mechanisms for asynchronous IR data processing
 * - Comprehensive device status monitoring and control
 *
 * The module supports various IR operation modes including receive-only,
 * transmit-only, and full-duplex communication, making it suitable for
 * applications such as remote controls, IR sensors, and IoT device communication.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TDL_IR_DEV_MANAGE_H__
#define __TDL_IR_DEV_MANAGE_H__

#include "tuya_cloud_types.h"
#include "tdl_ir_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

#define IR_DEV_NAME_MAX_LEN         (16u)

/* ir protocol */
typedef uint8_t IR_PROT_E;
#define IR_PROT_TIMECODE            0
#define IR_PROT_NEC                 1
#define IR_PROT_MAX                 2

typedef unsigned char IR_SEND_STATUS;
#define IR_STA_SEND_IDLE            0
#define IR_STA_SEND_BUILD           1
#define IR_STA_SENDING              2
#define IR_STA_SEND_FINISH          3
#define IR_STA_SEND_MAX             4

typedef unsigned char IR_RECV_STATUS;
#define IR_STA_RECV_IDLE            0
#define IR_STA_RECVING              1
#define IR_STA_RECV_FINISH          2
#define IR_STA_RECV_PARSER          3
#define IR_STA_RECV_OVERFLOW        4
#define IR_STA_RECV_MAX             5

/* tdl ir config command */
typedef unsigned char IR_CMD_E;
#define IR_CMD_GET_SEND_STATUS      1
#define IR_CMD_GET_RECV_STATUS      2
#define IR_CMD_SEND_HW_RESET        3
#define IR_CMD_RECV_HW_INIT         4
#define IR_CMD_RECV_HW_DEINIT       5
#define IR_CMD_RECV_TASK_START      6
#define IR_CMD_RECV_TASK_STOP       7
#define IR_CMD_RECV_QUEUE_CLEAN     8
#define IR_CMD_RECV_CB_REGISTER     9
#define IR_CMD_CODE_INTER_DELAY_SET 10 // Set the delay between continuous transmission of infrared code, unit: (uint32_t) us
#define IR_CMD_RECV_TASK_STACK_SET  11 // Set the stack size of the infrared receive task

/***********************************************************
***********************typedef define***********************
***********************************************************/

typedef void *IR_HANDLE_T;

/* nec protocol config struct */
typedef struct {
    uint8_t is_nec_msb; // 1: msb, 0: lsb

    // percent value range: 0-100
    uint8_t lead_err;
    uint8_t logics_err; // logic code high level error percent
    uint8_t logic0_err;
    uint8_t logic1_err;
    uint8_t repeat_err;
} IR_NEC_CFG_T;

/* ir protocol config union */
typedef union {
    IR_NEC_CFG_T nec_cfg;
} IR_PROT_CFG_U;

/* ir nec protocol data struct */
#pragma pack(1)
typedef struct {
    uint16_t addr;
    uint16_t cmd;
    uint16_t repeat_cnt;
} IR_DATA_NEC_T;
#pragma pack()

/* ir timecode data struct */
typedef struct {
    uint16_t len;
    uint32_t *data;
} IR_DATA_TIMECODE_T;

/* ir data union */
typedef union {
    IR_DATA_NEC_T nec_data;
    IR_DATA_TIMECODE_T timecode;
} IR_DATA_U;

/* ir device config struct */
typedef struct {
    IR_MODE_E ir_mode;

    uint8_t recv_queue_num;
    uint16_t recv_buf_size; // recv data size
    uint16_t recv_timeout; //unit :ms

    IR_PROT_E prot_opt;
    IR_PROT_CFG_U prot_cfg;
} IR_DEV_CFG_T;

typedef void (*IR_APP_RECV_CB)(uint8_t is_frame_finish, IR_DATA_U *recv_data);

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief find ir device
 *
 * @param[in] dev_name: device name
 * @param[out] handle: ir device handle
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdl_ir_dev_find(char *dev_name, IR_HANDLE_T *handle);

/**
 * @brief open ir device
 *
 * @param[in] handle: ir device handle
 * @param[in] config: ir device config
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdl_ir_dev_open(IR_HANDLE_T handle, IR_DEV_CFG_T *config);

/**
 * @brief close ir device
 *
 * @param[in] handle: ir device handle
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdl_ir_dev_close(IR_HANDLE_T handle);

/**
 * @brief ir data send
 *
 * @param[in] handle: ir device handle
 * @param[in] ir_data: send data
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdl_ir_dev_send(IR_HANDLE_T handle, uint32_t freq, IR_DATA_U ir_data, uint8_t send_cnt);

/**
 * @brief ir data recv
 *
 * @param[in] handle: ir device handle
 * @param[in] ir_data: recv data
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdl_ir_dev_recv(IR_HANDLE_T handle, IR_DATA_U **ir_data, uint32_t timeout_ms);

/**
 * @brief release recv data
 *
 * @param[in] handle: ir device handle
 * @param[in] ir_data: recv data
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdl_ir_dev_recv_release(IR_HANDLE_T handle, IR_DATA_U *ir_data);

/**
 * @brief config ir device
 *
 * @param[in] handle: ir device handle
 * @param[in] cmd: ir device command
 * @param[inout] params: config params or output data
 *
 * @return OPRT_OK on success. Others on error, please refer to "tuya_error_code.h"
 */
OPERATE_RET tdl_ir_config(IR_HANDLE_T handle, IR_CMD_E cmd, void *params);


#ifdef __cplusplus
}
#endif

#endif /* __TDL_IR_DEV_MANAGE_H__ */
