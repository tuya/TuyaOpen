/**
 * @file tdd_transport_uart.h
 * @brief tdd_transport_uart module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDD_TRANSPORT_UART_H__
#define __TDD_TRANSPORT_UART_H__

#include "tuya_cloud_types.h"
#include "tdl_transport_driver.h"

#include "tal_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TUYA_UART_NUM_E port_id;
    TAL_UART_CFG_T cfg;
} TDD_TRANSPORT_UART_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tdd_transport_uart_register(char *name, TDD_TRANSPORT_UART_CFG_T cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_TRANSPORT_UART_H__ */
