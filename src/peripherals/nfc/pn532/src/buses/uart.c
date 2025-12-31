/*-
 * Free/Libre Near Field Communication (NFC) library
 * UART driver - Tuya Platform Adaptation
 */

#ifdef HAVE_CONFIG_H
#include "nfc_config.h"
#endif

#include "buses/uart.h"
#include "platform_compat.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Tuya platform includes
#include "tal_uart.h"
#include "tkl_pinmux.h"
#include "tal_system.h"
#include "tal_log.h"

// UART2 configuration for PN532
#define PN532_UART_PORT     TUYA_UART_NUM_2
#define PN532_UART_BAUDRATE 115200
#define PN532_UART_RX_PIN   TUYA_IO_PIN_40
#define PN532_UART_TX_PIN   TUYA_IO_PIN_41

static bool uart_initialized = false;

/**
 * @brief Print hex data with prefix (embedded version of print_hex)
 * @param prefix Prefix string (e.g., "TX:" or "RX:")
 * @param pbtData Data buffer
 * @param szLen Data length
 */
#ifdef LOG
static void uart_print_hex(const char *prefix, const uint8_t *pbtData, const size_t szLen)
{
    if (!pbtData || szLen == 0)
        return;

    // Print in lines of 16 bytes
    for (size_t offset = 0; offset < szLen; offset += 16) {
        char   hex_buf[64];
        char  *p        = hex_buf;
        size_t line_len = (szLen - offset) > 16 ? 16 : (szLen - offset);

        for (size_t i = 0; i < line_len; i++) {
            p += snprintf(p, sizeof(hex_buf) - (p - hex_buf), "%02x ", pbtData[offset + i]);
        }

        if (offset == 0) {
            PR_NOTICE("%s %s", prefix, hex_buf);
        } else {
            PR_NOTICE("    %s", hex_buf);
        }
    }
}
#define UART_PRINT_HEX(prefix, data, len) uart_print_hex(prefix, data, len)
#else
#define UART_PRINT_HEX(prefix, data, len) ((void)0)
#endif

serial_port uart_open(const char *pcPortName)
{
    (void)pcPortName;

    if (uart_initialized) {
        PR_DEBUG("[uart_open] UART already initialized!");
        return (serial_port)1;
    }

    // Configure UART2 pinmux (PIN40=RX, PIN41=TX)
    tkl_io_pinmux_config(PN532_UART_RX_PIN, TUYA_UART2_RX);
    tkl_io_pinmux_config(PN532_UART_TX_PIN, TUYA_UART2_TX);

#ifdef LOG
    PR_DEBUG("[uart_open] Pinmux OK: PIN40=RX, PIN41=TX");
    PR_DEBUG("[uart_open] Step 2: Init UART2 @ %d baud", PN532_UART_BAUDRATE);
#endif

    // Configure UART using tal_uart_init
    TAL_UART_CFG_T cfg    = {0};
    cfg.base_cfg.baudrate = PN532_UART_BAUDRATE;
    cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.base_cfg.parity   = TUYA_UART_PARITY_TYPE_NONE;
    cfg.rx_buffer_size    = 512;
    cfg.open_mode         = O_BLOCK;

    OPERATE_RET rt = tal_uart_init(PN532_UART_PORT, &cfg);
    if (rt != OPRT_OK) {
        PR_ERR("[uart_open] tal_uart_init FAILED! Error: %d", rt);
        return INVALID_SERIAL_PORT;
    }

    uart_initialized = true;

    return (serial_port)1;
}

void uart_close(const serial_port sp)
{
    (void)sp;
    if (!uart_initialized)
        return;
    tal_uart_deinit(PN532_UART_PORT);
    uart_initialized = false;
    PR_DEBUG("PN532 UART closed");
}

void uart_set_speed(serial_port sp, const uint32_t uiPortSpeed)
{
    (void)sp;
    (void)uiPortSpeed;
    // Baudrate is fixed at init time
}

uint32_t uart_get_speed(const serial_port sp)
{
    (void)sp;
    return PN532_UART_BAUDRATE;
}

int uart_send(serial_port sp, const uint8_t *pbtTx, const size_t szTx, int timeout)
{
    (void)sp;
    (void)timeout;

    if (!uart_initialized || !pbtTx || szTx == 0)
        return -1;

    // Print TX data in hex (only in NFC_DEBUG mode)
    UART_PRINT_HEX("TX:", pbtTx, szTx);

    int sent = tal_uart_write(PN532_UART_PORT, (uint8_t *)pbtTx, szTx);
    if (sent < 0 || (size_t)sent != szTx) {
        PR_ERR("[uart_send] FAILED! Expected %d bytes, sent %d", szTx, sent);
        return -1;
    }
    // Success: return 0 (POSIX style)
    return 0;
}

int uart_receive(serial_port sp, uint8_t *pbtRx, const size_t szRx, void *abort_p, int timeout)
{
    (void)sp;
    (void)abort_p;

    if (!uart_initialized || !pbtRx || szRx == 0)
        return -1;

    size_t   received = 0;
    uint32_t start    = tal_system_get_millisecond();

    // Loop until we receive all bytes or timeout
    // timeout=0 means infinite wait (no timeout check)
    while (received < szRx) {
        uint32_t avail = tal_uart_get_rx_data_size(PN532_UART_PORT);

        if (avail > 0) {
            start               = tal_system_get_millisecond(); // Reset timeout timer on data received
            uint32_t to_read    = (szRx - received) < avail ? (szRx - received) : avail;
            int      read_bytes = tal_uart_read(PN532_UART_PORT, pbtRx + received, to_read);
            if (read_bytes > 0) {
                received += read_bytes;

                // If we got all bytes, break immediately
                if (received >= szRx) {
                    break;
                }
            } else if (read_bytes < 0) {
                PR_ERR("[uart_receive] Read error!");
                return -1;
            }
        }

        if (received < szRx) {
            tal_system_sleep(20);
        }

        // Check timeout only if timeout > 0
        if (timeout > 0 && ((int)(tal_system_get_millisecond() - start)) >= timeout) {
            break; // Timeout reached
        }
    }

    // Print RX data in hex (only in NFC_DEBUG mode)
    if (received > 0) {
        UART_PRINT_HEX("RX:", pbtRx, received);
    }

    if (received < szRx) {
        // PR_ERR("[RX INCOMPLETE] Expected %d bytes, got %d in %dms", szRx, received,
        //    tal_system_get_millisecond() - start);
        // Return error if we didn't receive all requested bytes
        return -1;
    }

    // Success: received all requested bytes, return 0 (POSIX style)
    return 0;
}

void uart_flush_input(const serial_port sp, bool wait)
{
    (void)sp;

    if (!uart_initialized)
        return;

    // Add delay if requested
    if (wait) {
        tal_system_sleep(50);
    }

    // Non-blocking flush - check data availability first
    uint8_t dummy[128];
    int     flushed = 0;
    for (int i = 0; i < 10; i++) { // Max 10 iterations
        uint32_t avail = tal_uart_get_rx_data_size(PN532_UART_PORT);
        if (avail == 0)
            break;

        uint32_t to_flush   = avail > sizeof(dummy) ? sizeof(dummy) : avail;
        int      read_bytes = tal_uart_read(PN532_UART_PORT, dummy, to_flush);
        if (read_bytes > 0) {
            flushed += read_bytes;
        }
        tal_system_sleep(5);
    }

#ifdef LOG
    if (flushed > 0) {
        PR_DEBUG("[uart_flush_input] Flushed %d bytes", flushed);
    }
#endif
}

char **uart_list_ports(void)
{
    // Not implemented for embedded system
    return NULL;
}
