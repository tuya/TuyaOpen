/**
 * @file uart_expand.h
 * @brief Implements UART expansion functionality for IoT devices
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __UART_EXPAND_H__
#define __UART_EXPAND_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include "tuya_cloud_types.h"

/**
 * @brief UART mode enumeration
 * 
 * NOTE: UART_MODE_PRINTER is NOT a switchable UART mode.
 * Printer uses the same UART port but in a special way:
 * - Data is written to ring buffer via uart_print_write()
 * - Printer thread batches data and temporarily switches to 9600 baud
 * - After printing, UART is restored to the current mode (RFID/AI_LOG)
 */
typedef enum {
    UART_MODE_RFID_SCAN = 0,    // RFID scanning mode (115200 baud)
    UART_MODE_AI_LOG,            // AI log mode (460800 baud)
    UART_MODE_MAX
} UART_MODE_E;

/**
 * @brief UART data callback function type
 * @param mode Current UART mode
 * @param data Received data buffer
 * @param len Length of received data
 */
typedef void (*uart_data_callback_t)(UART_MODE_E mode, const uint8_t *data, size_t len);

/**
 * @brief Initialize UART expansion functionality
 */
OPERATE_RET uart_expand_init(void);

/**
 * @brief Switch UART mode
 * @param mode Target UART mode
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET uart_expand_switch_mode(UART_MODE_E mode);

/**
 * @brief Get current UART mode
 * @return Current UART mode
 */
UART_MODE_E uart_expand_get_mode(void);

/**
 * @brief Register data callback for specific mode
 * @param mode UART mode
 * @param callback Callback function
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET uart_expand_register_callback(UART_MODE_E mode, uart_data_callback_t callback);

/**
 * @brief Write data to UART print ring buffer
 * @param data Data buffer to write
 * @param len Length of data
 * @return Number of bytes written
 */
uint32_t uart_print_write(const uint8_t *data, size_t len);

#if defined(__cplusplus)
}
#endif

#endif // __UART_EXPAND_H__