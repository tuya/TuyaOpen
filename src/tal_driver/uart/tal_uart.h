/**
 * @file tal_uart.h
 * @brief Common process - adapter the uart api provide by OS
 *
 * @copyright Copyright 2023 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_UART_H__
#define __TAL_UART_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define O_BLOCK       1
#define O_ASYNC_WRITE (1 << 1)
#define O_FLOW_CTRL   (1 << 2)
#define O_TX_DMA      (1 << 3)
#define O_RX_DMA      (1 << 4)

typedef struct {
    uint32_t rx_buffer_size;
#ifdef CONFIG_TX_ASYNC
    uint32_t tx_buffer_size;
#endif
    uint8_t open_mode;
    TUYA_UART_BASE_CFG_T base_cfg;
} TAL_UART_CFG_T;

/**
 * @brief init uart
 *
 * @param[in] port_id: uart port id, id index starts from 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the
 * TUYA_UART_TYPE_E type the low 16bit - means uart port id you can input like
 * this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @param[in] cfg: uart configure
 *
 * @note This API is used to init uart.
 *
 * @return the uart init result, 0, init success, other init error
 */
OPERATE_RET tal_uart_init(TUYA_UART_NUM_E port_id, TAL_UART_CFG_T *cfg);

/**
 * @brief read data from uart
 *
 * @param[in] port_id: uart port id, id index starts from 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the
 * TUYA_UART_TYPE_E type the low 16bit - means uart port id you can input like
 * this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @param[in] data: read data buffer
 * @param[in] len: the read size
 *
 * @note This API is used to read data from uart.
 *
 * @return >=0, the read size; < 0, read error
 */
int tal_uart_read(TUYA_UART_NUM_E port_id, uint8_t *data, uint32_t len);

/**
 * @brief send data by uart
 *
 * @param[in] port_id: uart port id, id index starts from 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the
 * TUYA_UART_TYPE_E type the low 16bit - means uart port id you can input like
 * this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @param[in] data: send data buffer
 * @param[in] len: the send size
 *
 * @note This API is used to send data by uart.
 *
 * @return >=0, the write size; < 0, write error
 */
int tal_uart_write(TUYA_UART_NUM_E port_id, const uint8_t *data, uint32_t len);

/**
 * @brief deinit uart
 *
 * @param[in] port_id: uart port id, id index starts from 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the
 * TUYA_UART_TYPE_E type the low 16bit - means uart port id you can input like
 * this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 *
 * @note This API is used to deinit uart.
 *
 * @return the uart deinit result, 0, deinit success, other deinit error
 */
OPERATE_RET tal_uart_deinit(TUYA_UART_NUM_E port_id);

/**
 * @brief uart irq callback
 *
 * @param[in] port_id: uart port id, id index starts from 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the
 * TUYA_UART_TYPE_E type the low 16bit - means uart port id you can input like
 * this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @param[out] buff: data receive buff
 * @param[in] len: receive length
 *
 * @return none
 */
typedef void (*TAL_UART_IRQ_CB)(TUYA_UART_NUM_E port_id, void *buff, uint16_t len);

/**
 * @brief enable uart rx interrupt and register interrupt callback func
 *
 * @param[in] port_id: uart port id, id index starts from 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the
 * TUYA_UART_TYPE_E type the low 16bit - means uart port id you can input like
 * this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 * @param[in] rx_cb: receive interrupt callback
 *
 * @return none
 */
void tal_uart_rx_reg_irq_cb(TUYA_UART_NUM_E port_id, TAL_UART_IRQ_CB rx_cb);

/**
 * @brief get rx data size
 *
 * @param[in] port_num: uart port num, id index starts from 0
 *                     in linux platform,
 *                         high 16 bits aslo means uart type,
 *                                   it's value must be one of the
 * TUYA_UART_TYPE_E type the low 16bit - means uart port id you can input like
 * this TUYA_UART_PORT_ID(TUYA_UART_SYS, 2)
 *
 * @return none
 */
int tal_uart_get_rx_data_size(TUYA_UART_NUM_E port_num);

#ifdef __cplusplus
}
#endif

#endif
