/**
 * @file uart_expand.c
 * @brief Implements UART expansion functionality for IoT device
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "uart_expand.h"
#include "tkl_pinmux.h"
#include "tal_uart.h"
#include "tal_api.h"
#include "tuya_ringbuf.h"

#include "ai_audio.h"

#include "app_display.h"
#include "rfid_scan.h"
#include "ai_log.h"
#include "utf8_to_gbk.h"
#include "dp48a_printer.h"
#include "ai_log_screen.h"
#include "rfid_scan_screen.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define USR_UART_NUM      TUYA_UART_NUM_2
#define READ_BUFFER_SIZE  1024
#define UTF8_RINGBUF_SIZE 1024  /* Ring buffer size */

#define RFID_SCAN_BAUDRATE 115200
#define RFID_SCAN_FREQ     100      /* in ms */
#define AI_LOG_BAUDRATE    460800
#define AI_LOG_FREQ        50       /* in ms */
#define PRINTER_BAUDRATE   9600
#define PRINTER_FREQ       50       /* in ms */

#define MODE_SWITCH_TIMEOUT 200     /* Mode switch timeout in ms */
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    UART_MODE_E mode;
    uint32_t baudrate;
    uart_data_callback_t callback;
} uart_mode_config_t;

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE uart_worker_thread = NULL;
static THREAD_HANDLE printer_scan_thread = NULL;

static uint8_t sg_read_buffer[READ_BUFFER_SIZE];
static TUYA_RINGBUFF_T sg_print_ringbuf = NULL;

// Current UART mode
static UART_MODE_E sg_current_mode = UART_MODE_RFID_SCAN;
static MUTEX_HANDLE sg_mode_mutex = NULL;
static BOOL_T sg_mode_switch_request = FALSE;
static UART_MODE_E sg_target_mode = UART_MODE_RFID_SCAN;

// Worker thread's cached baudrate (shared between worker and printer threads)
static uint32_t sg_current_baudrate = 0;

// Mode configurations (NOTE: UART_MODE_PRINTER removed as printer uses separate mechanism)
static uart_mode_config_t sg_mode_configs[UART_MODE_MAX] = {
    {UART_MODE_RFID_SCAN, RFID_SCAN_BAUDRATE, NULL},
    {UART_MODE_AI_LOG, AI_LOG_BAUDRATE, NULL},
};

// Worker thread control
static BOOL_T sg_worker_running = FALSE;
// Printer scan thread control
static BOOL_T printer_scan_running = FALSE;

extern BOOL_T app_get_text_stream_status(void);

/***********************************************************
********************function declaration********************
***********************************************************/
static void __uart_worker_thread(void *param);
static void __rfid_scan_data_callback(uint8_t dev_id, RFID_TAG_TYPE_E tag_type, const uint8_t *uid, uint8_t uid_len);
static OPERATE_RET __uart_reinit_with_baudrate(uint32_t baudrate);
static void __ai_log_screen_lifecycle_handler(BOOL_T is_init);
static void __ai_log_uart_data_callback(UART_MODE_E mode, const uint8_t *data, size_t len);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief AI log UART data callback
 * Called when AI log data is received from UART
 */
static void __ai_log_uart_data_callback(UART_MODE_E mode, const uint8_t *data, size_t len)
{
    if (mode != UART_MODE_AI_LOG || !data || len == 0) {
        return;
    }
    
    // Send data to display manager
    app_display_send_msg(POCKET_DISP_TP_AI_LOG, (uint8_t *)data, len);
    
    // Upload to AI text agent if not streaming
    if (app_get_text_stream_status() == FALSE) {
        ai_text_agent_upload((uint8_t *)data, len);
    }
}

/**
 * @brief AI log screen lifecycle handler
 * Called when AI log screen is initialized or deinitialized
 * 
 * @param is_init TRUE for init, FALSE for deinit
 */
static void __ai_log_screen_lifecycle_handler(BOOL_T is_init)
{
    OPERATE_RET rt;
    
    if (is_init) {
        PR_NOTICE("[UART] AI log screen initialized, switching to AI log mode");
        
        // Register AI log data callback
        rt = uart_expand_register_callback(UART_MODE_AI_LOG, __ai_log_uart_data_callback);
        if (rt != OPRT_OK) {
            PR_ERR("Failed to register AI log callback: %d", rt);
            return;
        }
        
        // Switch to AI log mode
        rt = uart_expand_switch_mode(UART_MODE_AI_LOG);
        if (rt != OPRT_OK) {
            PR_ERR("Failed to switch to AI log mode: %d", rt);
        }
    } else {
        PR_NOTICE("[UART] AI log screen deinitialized, switching back to RFID mode");
        
        // Unregister callback
        uart_expand_register_callback(UART_MODE_AI_LOG, NULL);
        
        // Switch back to RFID scan mode
        rt = uart_expand_switch_mode(UART_MODE_RFID_SCAN);
        if (rt != OPRT_OK) {
            PR_ERR("Failed to switch back to RFID scan mode: %d", rt);
        }
    }
}

/**
 * @brief Switch UART mode
 * @param mode Target UART mode
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET uart_expand_switch_mode(UART_MODE_E mode)
{
    if (mode >= UART_MODE_MAX) {
        PR_ERR("Invalid UART mode: %d", mode);
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(sg_mode_mutex);
    
    if (sg_current_mode == mode) {
        tal_mutex_unlock(sg_mode_mutex);
        return OPRT_OK;
    }

    PR_DEBUG("Switching UART mode: %d -> %d", sg_current_mode, mode);
    sg_target_mode = mode;
    sg_mode_switch_request = TRUE;
    
    tal_mutex_unlock(sg_mode_mutex);
    
    // Wait for mode switch to complete
    uint32_t timeout = 0;
    while (sg_mode_switch_request && timeout < MODE_SWITCH_TIMEOUT) {
        tal_system_sleep(10);
        timeout += 10;
    }
    
    if (sg_mode_switch_request) {
        PR_ERR("Mode switch timeout after %dms", timeout);
        return OPRT_COM_ERROR;
    }
    
    return OPRT_OK;
}

/**
 * @brief Get current UART mode
 * @return Current UART mode
 */
UART_MODE_E uart_expand_get_mode(void)
{
    UART_MODE_E mode;
    tal_mutex_lock(sg_mode_mutex);
    mode = sg_current_mode;
    tal_mutex_unlock(sg_mode_mutex);
    return mode;
}

/**
 * @brief Register data callback for specific mode
 * @param mode UART mode
 * @param callback Callback function
 * @return OPRT_OK on success, error code otherwise
 */
OPERATE_RET uart_expand_register_callback(UART_MODE_E mode, uart_data_callback_t callback)
{
    if (mode >= UART_MODE_MAX) {
        PR_ERR("Invalid UART mode: %d", mode);
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(sg_mode_mutex);
    sg_mode_configs[mode].callback = callback;
    tal_mutex_unlock(sg_mode_mutex);
    
    PR_DEBUG("Registered callback for mode %d", mode);
    return OPRT_OK;
}

/**
 * @brief Reinitialize UART with specified baudrate
 */
static OPERATE_RET __uart_reinit_with_baudrate(uint32_t baudrate)
{
    OPERATE_RET rt;
    
    // Deinitialize UART
    tal_uart_deinit(USR_UART_NUM);
    tal_system_sleep(5);
    
    // Reinitialize with new baudrate
    TAL_UART_CFG_T cfg = {0};
    cfg.base_cfg.baudrate = baudrate;
    cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    cfg.rx_buffer_size = 2048;
    cfg.open_mode = 0;  // Non-blocking mode
    
    rt = tal_uart_init(USR_UART_NUM, &cfg);
    if (rt != OPRT_OK) {
        PR_ERR("UART reinit failed with baudrate %d, error: %d", baudrate, rt);
        return rt;
    }
    
    tal_system_sleep(5);
    sg_current_baudrate = baudrate;
    
    return OPRT_OK;
}

/**
 * @brief Process RFID scan mode data
 */
static void __process_rfid_scan_data(const uint8_t *data, int len)
{
    if (len <= 28) {
        return;
    }

    // Process RFID scan data using abstracted function
    OPERATE_RET rt = rfid_scan_process(data, len, __rfid_scan_data_callback);
    if (rt != OPRT_OK) {
        PR_ERR("RFID process failed: %d", rt);
    }
}

/**
 * @brief Process AI log mode data
 */
static void __process_ai_log_data(const uint8_t *data, int len)
{
    int16_t pos = kmp_search((const char *)data, "ty E");
    if (pos >= 0) {
        // Call registered callback
        uart_data_callback_t callback = sg_mode_configs[UART_MODE_AI_LOG].callback;
        if (callback) {
            callback(UART_MODE_AI_LOG, data, len);
        }
    }
}

/**
 * @brief Unified UART worker thread
 * Handles all UART modes with dynamic switching
 */
static void __uart_worker_thread(void *param)
{
    OPERATE_RET rt = OPRT_OK;

    while (sg_worker_running) {
        // Check for mode switch request
        tal_mutex_lock(sg_mode_mutex);
        if (sg_mode_switch_request) {
            UART_MODE_E new_mode = sg_target_mode;
            uint32_t new_baudrate = sg_mode_configs[new_mode].baudrate;
            tal_mutex_unlock(sg_mode_mutex);
            
            // Reinitialize UART on mode switch
            rt = __uart_reinit_with_baudrate(new_baudrate);
            if (rt != OPRT_OK) {
                PR_ERR("Worker UART reinit failed!");
            }
            
            // Update current mode
            tal_mutex_lock(sg_mode_mutex);
            sg_current_mode = new_mode;
            sg_mode_switch_request = FALSE;
            tal_mutex_unlock(sg_mode_mutex);
            
            PR_DEBUG("Mode switched to %d (baudrate: %d)", new_mode, new_baudrate);
        } else {
            tal_mutex_unlock(sg_mode_mutex);
        }

        // Read UART data
        int read_len = tal_uart_read(USR_UART_NUM, (uint8_t *)sg_read_buffer, READ_BUFFER_SIZE);
        
        if (read_len > 0) {
            sg_read_buffer[read_len] = '\0';
            
            // Process data based on current mode
            tal_mutex_lock(sg_mode_mutex);
            UART_MODE_E current_mode = sg_current_mode;
            tal_mutex_unlock(sg_mode_mutex);
            
            switch (current_mode) {
                case UART_MODE_RFID_SCAN:
                    __process_rfid_scan_data(sg_read_buffer, read_len);
                    tal_system_sleep(RFID_SCAN_FREQ);
                    break;
                    
                case UART_MODE_AI_LOG:
                    __process_ai_log_data(sg_read_buffer, read_len);
                    tal_system_sleep(AI_LOG_FREQ);
                    break;
                    
                default:
                    PR_WARN("Unknown mode %d", current_mode);
                    tal_system_sleep(RFID_SCAN_FREQ);
                    break;
            }
        } else if (read_len < 0) {
            PR_ERR("UART read error: %d", read_len);
            tal_system_sleep(RFID_SCAN_FREQ);
        } else {
            tal_system_sleep(RFID_SCAN_FREQ);
        }
    }
    PR_NOTICE("UART worker thread stopped");
}

uint32_t uart_print_write(const uint8_t *data, size_t len)
{
    return tuya_ring_buff_write(sg_print_ringbuf, data, len);
}

/**
 * @brief Callback function for RFID scan data update
 * @param dev_id Device ID
 * @param tag_type Tag type
 * @param uid UID data buffer
 * @param uid_len UID length
 */
static void __rfid_scan_data_callback(uint8_t dev_id, RFID_TAG_TYPE_E tag_type, const uint8_t *uid, uint8_t uid_len)
{
    // Update UI with RFID data
    rfid_scan_screen_data_update(dev_id, tag_type, uid, uid_len);
    app_display_send_msg(POCKET_DISP_TP_RFID_SCAN_SUCCESS, NULL, 0);
}

/**
 * @brief Printer scan thread, reads UTF8 data from ring buffer and converts to GBK for printing
 * 
 * DESIGN NOTE: Printer requires 9600 baud, but shares UART with RFID(115200) and AI Log(460800).
 * Solution: Switch to 9600 baud when data available, print immediately, switch back when stream ends.
 */
void __printer_scan_thread(void *param)
{
    uint8_t utf8_buf[5];  // Max 4 bytes for a single UTF8 character
    uint8_t gbk_buf[4];   // GBK conversion buffer
    BOOL_T is_printing = FALSE;  // Track if we're in printing mode
    UART_MODE_E saved_mode = UART_MODE_RFID_SCAN;
    dp48a_init();

    PR_NOTICE("Printer scan thread started");
    
    while (printer_scan_running) {        
        if (NULL == sg_print_ringbuf) {
            PR_ERR("Printer ringbuf is NULL");
            tal_system_sleep(100);
            continue;
        }
        
        // Check available bytes in ring buffer
        uint32_t available = tuya_ring_buff_used_size_get(sg_print_ringbuf);
        
        if (available == 0) {
            // Ring buffer empty
            if (is_printing) {
                // Stream ended, restore baudrate
                if (app_get_text_stream_status() == FALSE) {
                    dp48a_print_enter();
                    dp48a_feed_lines(2);
                    tal_system_sleep(RFID_SCAN_FREQ);
                }
                
                // Restore baudrate
                tal_mutex_lock(sg_mode_mutex);
                __uart_reinit_with_baudrate(sg_mode_configs[saved_mode].baudrate);
                
                // Trigger mode switch to wake up worker thread
                sg_target_mode = saved_mode;
                sg_mode_switch_request = TRUE;
                tal_mutex_unlock(sg_mode_mutex);
                
                is_printing = FALSE;
                PR_DEBUG("Print session completed");
            }
            
            // Wait for new data
            tal_system_sleep(100);
            continue;
        }
        
        // Data available, check if need to switch to printer baudrate
        if (!is_printing) {
            tal_mutex_lock(sg_mode_mutex);
            saved_mode = sg_current_mode;
            __uart_reinit_with_baudrate(PRINTER_BAUDRATE);
            tal_mutex_unlock(sg_mode_mutex);
            
            dp48a_set_align(DP48A_ALIGN_LEFT);
            is_printing = TRUE;
            PR_DEBUG("Print session started");
        }
        
        // Read first byte to determine character length
        uint8_t first_byte;
        if (tuya_ring_buff_read(sg_print_ringbuf, &first_byte, 1) != 1) {
            tal_system_sleep(10);
            continue;
        }
        
        // Determine the full character length needed
        uint8_t char_len = utf8_full_char_len(first_byte);
        if (char_len == 0) {
            PR_WARN("Invalid UTF8 first byte: 0x%02X", first_byte);
            continue;
        }
        
        // Put first byte into buffer
        utf8_buf[0] = first_byte;
        
        // If more bytes needed, wait for them to arrive
        if (char_len > 1) {
            // Wait for remaining bytes, max 2 seconds
            uint32_t retry = 0;
            while (tuya_ring_buff_used_size_get(sg_print_ringbuf) < (char_len - 1) && retry < 200) {
                tal_system_sleep(10);
                retry++;
            }
        
            // Check if data arrived successfully
            uint32_t available_now = tuya_ring_buff_used_size_get(sg_print_ringbuf);
            if (available_now < (char_len - 1)) {
                // Incomplete data, print placeholder
                uint8_t placeholder = 0x3F;  // '?'
                dp48a_print_text_raw(&placeholder, 1);
                tal_system_sleep(5);
                continue;
            }
            
            // Read remaining bytes
            if (tuya_ring_buff_read(sg_print_ringbuf, &utf8_buf[1], char_len - 1) != (char_len - 1)) {
                uint8_t placeholder = 0x3F;  // '?'
                dp48a_print_text_raw(&placeholder, 1);
                tal_system_sleep(5);
                continue;
            }
        }
        
        // Convert UTF8 to GBK
        int gbk_len = utf8_to_gbk_buf(utf8_buf, char_len, gbk_buf, sizeof(gbk_buf));
        if (gbk_len > 0) {
            // Print immediately
            dp48a_print_text_raw(gbk_buf, gbk_len);
            tal_system_sleep(gbk_len + 5);
        } else {
            // Conversion failed, print placeholder
            uint8_t placeholder = 0x3F;  // '?'
            dp48a_print_text_raw(&placeholder, 1);
            tal_system_sleep(5);
        }
    }
    
    PR_NOTICE("Printer scan thread stopped");
}

OPERATE_RET uart_expand_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Create mutex for mode switching
    rt = tal_mutex_create_init(&sg_mode_mutex);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to create mode mutex: %d", rt);
        return rt;
    }

    // Create printer ring buffer
    rt = tuya_ring_buff_create(UTF8_RINGBUF_SIZE, OVERFLOW_STOP_TYPE, &sg_print_ringbuf);
    if (rt != OPRT_OK || sg_print_ringbuf == NULL) {
        PR_ERR("Failed to create print ringbuf, rt=%d", rt);
        tal_mutex_release(sg_mode_mutex);
        return OPRT_MALLOC_FAILED;
    }

    // Configure UART pins
    tkl_io_pinmux_config(TUYA_IO_PIN_40, TUYA_UART2_RX);
    tkl_io_pinmux_config(TUYA_IO_PIN_41, TUYA_UART2_TX);

    // Initialize UART with default RFID scan baudrate
    rt = __uart_reinit_with_baudrate(RFID_SCAN_BAUDRATE);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to initialize UART: %d", rt);
        tuya_ring_buff_free(sg_print_ringbuf);
        tal_mutex_release(sg_mode_mutex);
        return rt;
    }

    // Start unified UART worker thread
    sg_worker_running = TRUE;
    sg_current_mode = UART_MODE_RFID_SCAN;
    THREAD_CFG_T thrd_param_worker = {2048, 4, "uart_worker_thread"};
    rt = tal_thread_create_and_start(&uart_worker_thread, NULL, NULL, __uart_worker_thread, NULL, &thrd_param_worker);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to create UART worker thread: %d", rt);
        sg_worker_running = FALSE;
        tal_uart_deinit(USR_UART_NUM);
        tuya_ring_buff_free(sg_print_ringbuf);
        tal_mutex_release(sg_mode_mutex);
        return rt;
    }

    // Start printer scan thread (always running in background)
    printer_scan_running = TRUE;
    THREAD_CFG_T thrd_param_printer = {4096, 4, "printer_scan_thread"};
    rt = tal_thread_create_and_start(&printer_scan_thread, NULL, NULL, __printer_scan_thread, NULL, &thrd_param_printer);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to create printer scan thread: %d", rt);
        printer_scan_running = FALSE;
        sg_worker_running = FALSE;
        tal_thread_delete(uart_worker_thread);
        tal_uart_deinit(USR_UART_NUM);
        tuya_ring_buff_free(sg_print_ringbuf);
        tal_mutex_release(sg_mode_mutex);
        return rt;
    }

    // Register AI log screen lifecycle callback
    ai_log_screen_register_lifecycle_cb(__ai_log_screen_lifecycle_handler);

    PR_NOTICE("UART expansion initialized with unified worker thread");
    return OPRT_OK;
}
