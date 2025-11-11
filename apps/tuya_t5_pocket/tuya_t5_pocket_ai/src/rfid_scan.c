/**
 * @file rfid_scan.c
 * @brief Implements RFID scanning functionality for IoT device
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "rfid_scan.h"
#include "tkl_pinmux.h"
#include "tal_uart.h"
#include "tal_system.h"
#include "tal_api.h"

#include "ai_audio.h"

#include "app_display.h"
#include "rfid_scan_screen.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define USR_UART_NUM      TUYA_UART_NUM_2
#define READ_BUFFER_SIZE  2048
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    RFID_DATA_TYPE_E data_type;
    RFID_TAG_TYPE_E tag_type;
    uint16_t block_addr;    // only data_type is RFID_DATA_TYPE_BLOCK_DATA used
    RFID_SCAN_LENGTH_E data_len;
    uint8_t data[16];       // For UIDs less than 16 bytes, all the remaining bytes are 00h.
} RFID_SCAN_DATA;

typedef struct {
    uint8_t dev_id;
    RFID_DATA_CMD_E cmd;
    uint8_t length;
    RFID_SCAN_DATA data;
    uint16_t crc;
} RFID_SCAN_FRAME;

/***********************************************************
***********************variable define**********************
***********************************************************/

static THREAD_HANDLE rfid_scan_thread = NULL;
static THREAD_HANDLE log_scan_thread = NULL;
static uint8_t sg_read_buffer[READ_BUFFER_SIZE];
static RFID_SCAN_FRAME rfid_dev;

// Log scan thread control
static BOOL_T log_scan_running = FALSE;

static uint16_t crc16_mbrtu(uint8_t *buf, uint32_t len)
{
    uint32_t i;
    uint16_t crc_value = 0xFFFF;
    if ((buf == NULL) || (len == 0))
    {
        return 0;
    }
    while (len--) {
        crc_value ^= *buf++;
        for (i = 0; i < 8; i++)
        {
            if (crc_value & 0x0001)
                crc_value = (crc_value >> 1) ^ 0xA001;
            else
                crc_value = crc_value >> 1;
        }
    }
    return (crc_value);
 }

 static void compute_next(const char *p, int *next)
{
    int m = (int)strlen(p);
    next[0] = 0;
    for (int i = 1, len = 0; i < m; ) {
        if (p[i] == p[len]) {
            ++len;
            next[i++] = len;
        } else if (len > 0) {
            len = next[len - 1];
        } else {
            next[i++] = 0;
        }
    }
}

int kmp_search(const char *s, const char *p)
{
    int n = (int)strlen(s);
    int m = (int)strlen(p);
    if (m == 0) return -1;
    if (n < m) return -1;

    int *next = tal_psram_malloc(m * sizeof(int));
    if (!next) return -1;
    compute_next(p, next);

    int i = 0, j = 0;
    while (i < n) {
        if (s[i] == p[j]) {
            ++i; ++j;
            if (j == m) {
                free(next);
                return i - j;
            }
        } else if (j > 0) {
            j = next[j - 1];
        } else {
            ++i;
        }
    }
    tal_psram_free(next);
    return -1;
}

void __log_scan_thread(void *param)
{
    OPERATE_RET rt = OPRT_OK;

    tkl_io_pinmux_config(TUYA_IO_PIN_40, TUYA_UART2_RX);
    tkl_io_pinmux_config(TUYA_IO_PIN_41, TUYA_UART2_TX);

    /* UART 2 init */
    TAL_UART_CFG_T cfg = {0};
    cfg.base_cfg.baudrate = 460800;
    cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    cfg.rx_buffer_size = 2048;
    cfg.open_mode = O_BLOCK;
    rt = tal_uart_init(USR_UART_NUM, &cfg);

    if (rt != OPRT_OK) {
        PR_ERR("UART init failed");
        return;
    }

    while (log_scan_running) {
        // RFID scanning logic goes here
        int read_len = tal_uart_read(USR_UART_NUM, (uint8_t *)sg_read_buffer, READ_BUFFER_SIZE);
        if (read_len > 0) {
            sg_read_buffer[read_len] = '\0';
            int16_t pos = kmp_search((const char *)sg_read_buffer, "ty E");
            if (pos >= 0) {
                PR_DEBUG("KMP search result: %d, data len: %d", pos, read_len);
                app_display_send_msg(POCKET_DISP_TP_AI_LOG, sg_read_buffer, read_len);
                ai_text_agent_upload((uint8_t *)sg_read_buffer, read_len);
            }
        }
        // Log RFID scan data
        tal_system_sleep(50);
    }

    PR_NOTICE("Log scan thread stopped");
}

void __rfid_scan_thread(void *param)
{
    while (!log_scan_running) {
        // RFID scanning logic goes here
        int read_len = tal_uart_read(USR_UART_NUM, (uint8_t *)sg_read_buffer, READ_BUFFER_SIZE);
        if(read_len <= 0) {
            tal_system_sleep(100);
            continue;
        }

        rfid_dev.dev_id = sg_read_buffer[0];
        rfid_dev.cmd = sg_read_buffer[1];
        rfid_dev.length = sg_read_buffer[2];
        rfid_dev.data.data_type = (RFID_DATA_TYPE_E)((sg_read_buffer[3] << 8) | sg_read_buffer[4]);
        rfid_dev.data.tag_type = (RFID_TAG_TYPE_E)((sg_read_buffer[5] << 8) | sg_read_buffer[6]);
        rfid_dev.data.block_addr = (sg_read_buffer[7] << 8) | sg_read_buffer[8];
        rfid_dev.data.data_len = (RFID_SCAN_LENGTH_E)((sg_read_buffer[9] << 8) | sg_read_buffer[10]);
        memcpy(rfid_dev.data.data, &sg_read_buffer[11], 16);
        rfid_dev.crc = (sg_read_buffer[27] << 8) | sg_read_buffer[28];

        uint16_t calculated_crc = crc16_mbrtu((uint8_t *)&sg_read_buffer[0], read_len - 2);
        calculated_crc = calculated_crc << 8 | (calculated_crc >> 8);
        if (calculated_crc != rfid_dev.crc) {
            // PR_ERR("CRC mismatch: received 0x%04X, calculated 0x%04X", rfid_dev.crc, calculated_crc);
            tal_system_sleep(100);
            continue;
        }

        // Call callback function to update UI with RFID data
        rfid_scan_screen_data_update(rfid_dev.dev_id,
                                       rfid_dev.data.tag_type, 
                                       rfid_dev.data.data, 
                                       rfid_dev.data.data_len);
        app_display_send_msg(POCKET_DISP_TP_RFID_SCAN_SUCCESS, NULL, 0);

        // PR_NOTICE("dev_id: %02x, cmd: %02x, length: %d, data type: %d, tag type: %d, block addr: %d, data len: %d",
        //           rfid_dev.dev_id, rfid_dev.cmd, rfid_dev.length, rfid_dev.data.data_type,
        //           rfid_dev.data.tag_type, rfid_dev.data.block_addr, rfid_dev.data.data_len);
        // PR_NOTICE("UID : %02x %02x %02x %02x %02x %02x %02x %02x",
        //           rfid_dev.data.data[0], rfid_dev.data.data[1], rfid_dev.data.data[2], rfid_dev.data.data[3],
        //           rfid_dev.data.data[4], rfid_dev.data.data[5], rfid_dev.data.data[6], rfid_dev.data.data[7]);
        tal_system_sleep(50);
    }
}

OPERATE_RET rfid_scan_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    tkl_io_pinmux_config(TUYA_IO_PIN_40, TUYA_UART2_RX);
    tkl_io_pinmux_config(TUYA_IO_PIN_41, TUYA_UART2_TX);

    /* UART 2 init */
    TAL_UART_CFG_T cfg = {0};
    cfg.base_cfg.baudrate = 115200;
    cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    cfg.rx_buffer_size = 2048;
    cfg.open_mode = O_BLOCK;
    rt = tal_uart_init(USR_UART_NUM, &cfg);

    // Start RFID scan thread
    THREAD_CFG_T thrd_param = {2048, 4, "rfid_scan_thread"};
    tal_thread_create_and_start(&rfid_scan_thread, NULL, NULL, __rfid_scan_thread, NULL, &thrd_param);

    return rt;
}

/**
 * @brief Start log scanning thread
 * This function starts the UART log scanning thread for AI log screen.
 */
OPERATE_RET rfid_log_scan_start(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (log_scan_running) {
        PR_WARN("Log scan thread already running");
        return OPRT_OK;
    }

    log_scan_running = TRUE;

    if (rfid_scan_thread) {
        tal_thread_delete(rfid_scan_thread);
        tal_system_sleep(50);
        tal_uart_deinit(USR_UART_NUM);
        rfid_scan_thread = NULL;
    }

    THREAD_CFG_T thrd_param = {4096, 4, "log_scan_thread"};
    rt = tal_thread_create_and_start(&log_scan_thread, NULL, NULL, __log_scan_thread, NULL, &thrd_param);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to create log scan thread: %d", rt);
        log_scan_running = FALSE;
        return rt;
    }

    PR_NOTICE("Log scan thread creation requested");
    return OPRT_OK;
}

/**
 * @brief Stop log scanning thread
 * This function stops the UART log scanning thread.
 */
OPERATE_RET rfid_log_scan_stop(void)
{
    if (!log_scan_running) {
        PR_WARN("Log scan thread not running");
        return OPRT_OK;
    }

    PR_NOTICE("Stopping log scan thread...");
    log_scan_running = FALSE;

    // Wait for thread to finish
    if (log_scan_thread) {
        tal_thread_delete(log_scan_thread);
        tal_system_sleep(50);
        tal_uart_deinit(USR_UART_NUM);
        log_scan_thread = NULL;
    }

    rfid_scan_init(); // Re-initialize RFID scan after stopping log scan

    PR_NOTICE("Log scan thread stopped");
    return OPRT_OK;
}

