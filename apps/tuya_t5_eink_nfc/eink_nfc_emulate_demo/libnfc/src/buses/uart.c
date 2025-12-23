/*-
 * Free/Libre Near Field Communication (NFC) library
 * UART driver - Tuya Platform Adaptation
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
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

serial_port uart_open(const char *pcPortName)
{
    (void)pcPortName;

    PR_NOTICE("====================================");
    PR_NOTICE("[uart_open] CALLED");
    PR_NOTICE("[uart_open] Port: %s", pcPortName ? pcPortName : "NULL");
    PR_NOTICE("====================================");

    if (uart_initialized) {
        PR_WARN("[uart_open] UART already initialized!");
        return (serial_port)1;
    }

    PR_NOTICE("[uart_open] Step 1: Configuring pinmux");
    // Configure UART2 pinmux (PIN40=RX, PIN41=TX)
    tkl_io_pinmux_config(PN532_UART_RX_PIN, TUYA_UART2_RX);
    tkl_io_pinmux_config(PN532_UART_TX_PIN, TUYA_UART2_TX);
    PR_NOTICE("[uart_open] Pinmux OK: PIN40=RX, PIN41=TX");

    PR_NOTICE("[uart_open] Step 2: Init UART2 @ %d baud", PN532_UART_BAUDRATE);
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
        PR_ERR("[uart_open] Returning INVALID_SERIAL_PORT");
        return INVALID_SERIAL_PORT;
    }
    PR_NOTICE("[uart_open] UART2 init SUCCESS!");

    uart_initialized = true;

    PR_NOTICE("[uart_open] Step 3: Sending PN532 wakeup");
    // PN532 wakeup sequence - just preamble, no command!
    tal_system_sleep(200);
    uint8_t wakeup[] = {0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    PR_NOTICE("[uart_open] TX wakeup (16 bytes): 55 55 00 00 00 00 00 00 00 00 00 00 00 00 00 00");
    int wakeup_sent = tal_uart_write(PN532_UART_PORT, (uint8_t *)wakeup, sizeof(wakeup));
    PR_NOTICE("[uart_open] Wakeup sent: %d/%d bytes", wakeup_sent, sizeof(wakeup));

    // Wait for PN532 to process wakeup - check multiple times
    uint32_t rx_available = 0;
    for (int retry = 0; retry < 10; retry++) {
        tal_system_sleep(50); // Check every 50ms
        rx_available = tal_uart_get_rx_data_size(PN532_UART_PORT);
        if (rx_available > 0) {
            PR_NOTICE("[uart_open] PN532 responded after %dms", (retry + 1) * 50);
            break;
        }
    }
    PR_NOTICE("[uart_open] After wakeup delay, RX bytes available: %d", rx_available);

    if (rx_available > 0) {
        uint8_t response[64];
        int     actual = tal_uart_read(PN532_UART_PORT, response, (rx_available > 64) ? 64 : rx_available);
        if (actual > 0) {
            PR_NOTICE("[uart_open] Wakeup response (%d bytes):", actual);
            // Print in groups of 16 bytes
            for (int i = 0; i < actual; i += 16) {
                int remaining = actual - i;
                if (remaining >= 16) {
                    PR_NOTICE("    %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                              response[i + 0], response[i + 1], response[i + 2], response[i + 3], response[i + 4],
                              response[i + 5], response[i + 6], response[i + 7], response[i + 8], response[i + 9],
                              response[i + 10], response[i + 11], response[i + 12], response[i + 13], response[i + 14],
                              response[i + 15]);
                } else {
                    // Print remaining bytes
                    char  buf[128] = {0};
                    char *p        = buf;
                    for (int j = 0; j < remaining; j++) {
                        p += sprintf(p, "%02x ", response[i + j]);
                    }
                    PR_NOTICE("    %s", buf);
                }
            }
        }
    }

    PR_NOTICE("[uart_open] Step 5: Clearing RX buffer");
    // Clear RX buffer - use non-blocking approach
    uint8_t dummy[128];
    int     cleared = 0;
    for (int i = 0; i < 10; i++) { // Max 10 iterations
        uint32_t avail = tal_uart_get_rx_data_size(PN532_UART_PORT);
        if (avail == 0)
            break;

        uint32_t to_clear   = avail > sizeof(dummy) ? sizeof(dummy) : avail;
        int      read_bytes = tal_uart_read(PN532_UART_PORT, dummy, to_clear);
        if (read_bytes > 0) {
            cleared += read_bytes;
            // Print as hex line (like libnfc debug log)
            PR_NOTICE("[uart_open] RX (clearing): %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x "
                      "%02x %02x %02x",
                      dummy[0], dummy[1], dummy[2], dummy[3], dummy[4], dummy[5], dummy[6], dummy[7], dummy[8],
                      dummy[9], dummy[10], dummy[11], dummy[12], dummy[13], dummy[14], dummy[15]);
        }
        tal_system_sleep(5);
    }
    PR_NOTICE("[uart_open] RX total cleared: %d bytes", cleared);

    PR_NOTICE("[uart_open] === SUCCESS! Returning handle ====");
    return (serial_port)1;
}

void uart_close(const serial_port sp)
{
    (void)sp;
    if (!uart_initialized)
        return;
    tal_uart_deinit(PN532_UART_PORT);
    uart_initialized = false;
    PR_INFO("PN532 UART closed");
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

    // Print TX data in hex (libnfc style: lowercase, compact)
    PR_NOTICE("TX: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", pbtTx[0], pbtTx[1],
              pbtTx[2], pbtTx[3], pbtTx[4], pbtTx[5], pbtTx[6], pbtTx[7], pbtTx[8], pbtTx[9], pbtTx[10], pbtTx[11],
              pbtTx[12], pbtTx[13], pbtTx[14], pbtTx[15]);
    if (szTx > 16) {
        PR_NOTICE("    %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", pbtTx[16],
                  pbtTx[17], pbtTx[18], pbtTx[19], pbtTx[20], pbtTx[21], pbtTx[22], pbtTx[23], pbtTx[24], pbtTx[25],
                  pbtTx[26], pbtTx[27], pbtTx[28], pbtTx[29], pbtTx[30], pbtTx[31]);
    }
    if (szTx > 32) {
        PR_NOTICE("    ... (%d more bytes)", szTx - 32);
    }

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

    // timeout=0 means infinite wait (blocking mode)
    // if (timeout == 0) {
    //     PR_DEBUG("[uart_receive] Expecting %d bytes, BLOCKING MODE (infinite wait)", szRx);
    // } else {
    //     PR_DEBUG("[uart_receive] Expecting %d bytes, timeout=%dms", szRx, timeout);
    // }

    size_t   received    = 0;
    uint32_t start       = tkl_system_get_millisecond();
    int      check_count = 0;

    // Loop until we receive all bytes or timeout
    // timeout=0 means infinite wait (no timeout check)
    while (received < szRx) {
        uint32_t avail = tal_uart_get_rx_data_size(PN532_UART_PORT);
        check_count++;

        if (avail > 0) {
            // PR_NOTICE("[uart_receive] Check #%d: %d bytes available", check_count, avail);
            uint32_t to_read    = (szRx - received) < avail ? (szRx - received) : avail;
            int      read_bytes = tal_uart_read(PN532_UART_PORT, pbtRx + received, to_read);
            if (read_bytes > 0) {
                // PR_NOTICE("[uart_receive] Read %d bytes (total: %d/%d)", read_bytes, received + read_bytes, szRx);
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

    // Print RX data in hex (libnfc style) - FIXED: only print received bytes
    if (received > 0) {
        char  hex_buf[256];
        char *p = hex_buf;
        for (size_t i = 0; i < received && i < 64; i++) {
            p += sprintf(p, "%02x ", pbtRx[i]);
            if ((i + 1) % 16 == 0 && (i + 1) < received) {
                PR_NOTICE("RX: %s", hex_buf);
                p = hex_buf;
            }
        }
        if (p != hex_buf) {
            PR_NOTICE("RX: %s", hex_buf);
        }
        if (received > 64) {
            PR_NOTICE("    ... (%d more bytes)", received - 64);
        }
    }

    if (received < szRx) {
        PR_ERR("[RX INCOMPLETE] Expected %d bytes, got %d in %dms", szRx, received,
               tal_system_get_millisecond() - start);
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

    if (flushed > 0) {
        PR_NOTICE("[uart_flush_input] Flushed %d bytes", flushed);
    }
}

char **uart_list_ports(void)
{
    // Not implemented for embedded system
    return NULL;
}
