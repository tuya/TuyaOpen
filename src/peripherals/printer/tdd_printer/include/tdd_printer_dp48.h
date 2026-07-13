/**
 * @file tdd_printer_dp48.h
 * @brief DP-48A ESC/POS thermal printer TDD driver
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * Drives the DP-48A (and compatible) thermal printer via UART using
 * the ESC/POS protocol. The printer accepts GBK-encoded text.
 * Text encoding conversion (UTF-8 → GBK) is handled by the TDL layer.
 */

#ifndef __TDD_PRINTER_DP48_H__
#define __TDD_PRINTER_DP48_H__

#include "tuya_cloud_types.h"
#include "tdl_printer_driver.h"
#include "tal_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/

/* DP-48A hardware constants */
#define DP48_DOTS_PER_LINE   384
#define DP48_BYTES_PER_LINE  48

typedef struct {
    TUYA_UART_NUM_E port_id;
    TAL_UART_CFG_T  uart_cfg;
} TDD_PRINTER_DP48_CFG_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Register DP-48A printer driver to TDL layer
 * @param[in] name printer name for TDL registration
 * @param[in] cfg  UART port and configuration
 * @return OPRT_OK on success, error code on failure
 * @note After registration, use tdl_printer_find() to get the handle.
 *       The driver declares protocol=ESCPOS and encoding=GBK.
 *       Use tdl_printer_send_text() for UTF-8 text (auto-converted to GBK).
 *       Use tdl_printer_send() for raw ESC/POS command bytes.
 *       Use tdl_printer_send_bitmap() for monochrome images.
 */
OPERATE_RET tdd_printer_dp48_register(char *name, TDD_PRINTER_DP48_CFG_T *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __TDD_PRINTER_DP48_H__ */
