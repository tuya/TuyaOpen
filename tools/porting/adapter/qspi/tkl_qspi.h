/**
* @file tkl_qspi.h
* @brief Common process - adapter the qspi api
* @version 0.1
* @date 2021-08-06
*
* @copyright Copyright 2021-2030 Tuya Inc. All Rights Reserved.
*
*/
#ifndef __TKL_SPI_H__
#define __TKL_SPI_H__

#include "tuya_cloud_types.h"
#include "tkl_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_QSPI_FIFO_SIZE (256)
/**
 * @brief qspi init
 * 
 * @param[in] port: qspi port
 * @param[in] cfg:  QSPI parameter settings
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_qspi_init(TUYA_QSPI_NUM_E port, const TUYA_QSPI_BASE_CFG_T *cfg);

/**
 * @brief qspi deinit
 * 
 * @param[in] port: qspi port
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_qspi_deinit(TUYA_QSPI_NUM_E port);

/**
 * qspi send
 *
 * @param[in]  port      the qspi device
 * @param[in]  data     qspi send data
 * @param[in]  size     qspi send data size
 *
 * @return  OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_qspi_send(TUYA_QSPI_NUM_E port, void *data, uint16_t size);

OPERATE_RET tkl_qspi_send_cmd(TUYA_QSPI_NUM_E port, uint8_t cmd);

OPERATE_RET tkl_qspi_send_data_indirect_mode(TUYA_QSPI_NUM_E port, uint8_t *data, uint32_t data_len);

/**
 * @brief qspi read from addr by mapping mode
 * NOTE: 
 *
 * @param[in] port: qspi port, id index starts at 0
 * @param[out] data:  address of buffer
 * @param[in] size:  size of read
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_qspi_recv(TUYA_QSPI_NUM_E port, void *data, uint16_t size);

/**
 * @brief qspi command send
 * NOTE: 
 *
 * @param[in] port: qspi port, id index starts at 0
 * @param[in] command:  qspi command configure
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_qspi_comand(TUYA_QSPI_NUM_E port, TUYA_QSPI_CMD_T *command);

/**
 * @brief adort qspi transfer,or qspi send, or qspi recv
 * 
 * @param[in] port: qspi port
 * 
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */

OPERATE_RET tkl_qspi_abort_transfer(TUYA_QSPI_NUM_E port);

/**
 * @brief qspi irq init
 * NOTE: call this API will not enable interrupt
 *
 * @param[in] port: qspi port, id index starts at 0
 * @param[in] cb:  qspi irq cb
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_qspi_irq_init(TUYA_QSPI_NUM_E port, TUYA_QSPI_IRQ_CB cb);

/**
 * @brief qspi irq enable
 *
 * @param[in] port: qspi port id, id index starts at 0
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_qspi_irq_enable(TUYA_QSPI_NUM_E port);

/**
 * @brief qspi irq disable
 *
 * @param[in] port: qspi port id, id index starts at 0
 *k
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_qspi_irq_disable(TUYA_QSPI_NUM_E port);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif


