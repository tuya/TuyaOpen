/**
 * @file tkl_uart.h
 * @brief Common process - adapter the uart api
 * @version 0.1
 * @date 2021-08-06
 *
 * @copyright Copyright 2021-2022 Tuya Inc. All Rights Reserved.
 *
 */
#ifndef __TKL_UART_H__
#define __TKL_UART_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief uart init
 *
 * @param[in] port_id: uart port id, id index starts at 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the TUYA_UART_TYPE_E type
 *                         the low 16bit - means uart port id
 *                         you can input like this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @param[in] cfg: uart config
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_uart_init(TUYA_UART_NUM_E port_id, TUYA_UART_BASE_CFG_T *cfg);

/**
 * @brief uart deinit
 *
 * @param[in] port_id: uart port id, id index starts at 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the TUYA_UART_TYPE_E type
 *                         the low 16bit - means uart port id
 *                         you can input like this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_uart_deinit(TUYA_UART_NUM_E port_id);

/**
 * @brief uart write data
 *
 * @param[in] port_id: uart port id, id index starts at 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the TUYA_UART_TYPE_E type
 *                         the low 16bit - means uart port id
 *                         you can input like this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @param[in] data: write buff
 * @param[in] len:  buff len
 *
 * @return return > 0: number of data written; return <= 0: write errror
 */
int tkl_uart_write(TUYA_UART_NUM_E port_id, void *buff, uint16_t len);

/**
 * @brief enable uart rx interrupt and regist interrupt callback
 *
 * @param[in] port_id: uart port id, id index starts at 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the TUYA_UART_TYPE_E type
 *                         the low 16bit - means uart port id
 *                         you can input like this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @param[in] rx_cb: receive callback
 *
 * @return none
 */
void tkl_uart_rx_irq_cb_reg(TUYA_UART_NUM_E port_id, TUYA_UART_IRQ_CB rx_cb);

/**
 * @brief regist uart tx interrupt callback
 * If this function is called, it indicates that the data is sent asynchronously through interrupt,
 * and then write is invoked to initiate asynchronous transmission.
 *
 * @param[in] port_id: uart port id, id index starts at 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the TUYA_UART_TYPE_E type
 *                         the low 16bit - means uart port id
 *                         you can input like this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @param[in] rx_cb: receive callback
 *
 * @return none
 */
void tkl_uart_tx_irq_cb_reg(TUYA_UART_NUM_E port_id, TUYA_UART_IRQ_CB tx_cb);

/**
 * @brief uart read data
 *
 * @param[in] port_id: uart port id, id index starts at 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the TUYA_UART_TYPE_E type
 *                         the low 16bit - means uart port id
 *                         you can input like this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @param[out] data: read data
 * @param[in] len:  buff len
 *
 * @return return >= 0: number of data read; return < 0: read errror
 */
int tkl_uart_read(TUYA_UART_NUM_E port_id, void *buff, uint16_t len);

/**
 * @brief set uart transmit interrupt status
 *
 * @param[in] port_id: uart port id, id index starts at 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the TUYA_UART_TYPE_E type
 *                         the low 16bit - means uart port id
 *                         you can input like this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @param[in] enable: TRUE-enalbe tx int, FALSE-disable tx int
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_uart_set_tx_int(TUYA_UART_NUM_E port_id, BOOL_T enable);

/**
 * @brief set uart receive flowcontrol
 *
 * @param[in] port_id: uart port id, id index starts at 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the TUYA_UART_TYPE_E type
 *                         the low 16bit - means uart port id
 *                         you can input like this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @param[in] enable: TRUE-enalbe rx flowcontrol, FALSE-disable rx flowcontrol
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_uart_set_rx_flowctrl(TUYA_UART_NUM_E port_id, BOOL_T enable);

/**
 * @brief wait for uart data
 *
 * @param[in] port_id: uart port id, id index starts at 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the TUYA_UART_TYPE_E type
 *                         the low 16bit - means uart port id
 *                         you can input like this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @param[in] timeout_ms: the max wait time, unit is millisecond
 *                        -1 : block indefinitely
 *                        0  : non-block
 *                        >0 : timeout in milliseconds
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_uart_wait_for_data(TUYA_UART_NUM_E port_id, int timeout_ms);

/**
 * @brief uart control
 *
 * @param[in] uart refer to tuya_uart_t
 * @param[in] cmd control command
 * @param[in] arg command argument
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_uart_ioctl(TUYA_UART_NUM_E port_id, uint32_t cmd, void *arg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
