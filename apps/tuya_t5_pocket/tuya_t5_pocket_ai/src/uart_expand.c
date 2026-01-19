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

#include "ai_agent.h"
#include "app_display.h"
#include "rfid_scan.h"
#include "ai_log.h"
#include "utf8_to_gbk.h"
#include "dp48a_printer.h"
#include "ai_log_screen.h"
#include "rfid_scan_screen.h"
#include "camera_screen.h"
#include "yuv422_to_binary.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define USR_UART_NUM      TUYA_UART_NUM_2
#define READ_BUFFER_SIZE  1024
#define UTF8_RINGBUF_SIZE 1024 /* Ring buffer size */

#define RFID_SCAN_BAUDRATE 115200
#define RFID_SCAN_FREQ     100 /* in ms */
#define AI_LOG_BAUDRATE    460800
#define AI_LOG_FREQ        50 /* in ms */
#define PRINTER_BAUDRATE   9600
#define PRINTER_FREQ       50 /* in ms */

#define MODE_SWITCH_TIMEOUT 200 /* Mode switch timeout in ms */
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
static void __camera_screen_lifecycle_handler(BOOL_T is_init);
static void __camera_photo_print_handler(const YUV422_TO_BINARY_PARAMS_T *params);

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
    ai_ui_disp_msg(POCKET_DISP_TP_AI_LOG, (uint8_t *)data, len);

    // Upload to AI text agent if not streaming
    if (app_get_text_stream_status() == FALSE) {
        ai_agent_send_text((char *)data);
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
        PR_DEBUG("[UART] AI log screen initialized, switching to AI log mode");

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
        PR_DEBUG("[UART] AI log screen deinitialized, switching back to RFID mode");

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
 * @brief Camera screen lifecycle handler
 * @param is_init TRUE for init, FALSE for deinit
 */
static void __camera_screen_lifecycle_handler(BOOL_T is_init)
{
    if (is_init) {
        PR_DEBUG("[UART] Camera screen initialized, registering print callback");
        camera_screen_register_print_cb(__camera_photo_print_handler);
    } else {
        PR_DEBUG("[UART] Camera screen deinitialized, unregistering print callback");
        camera_screen_register_print_cb(NULL);
    }
}

/**
 * @brief Camera photo print handler
 * Called when ENTER key is pressed with raw YUV422 data
 * @param params Conversion parameters with YUV422 data and pre-allocated buffer
 * @note Buffer is allocated before callback and freed after callback returns
 */
static void __camera_photo_print_handler(const YUV422_TO_BINARY_PARAMS_T *params)
{
    if (!params || !params->yuv422_data || !params->binary_data || !params->config || params->src_width <= 0 ||
        params->src_height <= 0 || params->dst_width <= 0 || params->dst_height <= 0) {
        PR_ERR("Invalid parameters or missing pre-allocated buffer");
        return;
    }

    PR_NOTICE("Starting camera photo print from YUV422: %dx%d -> %dx%d, method=%d", params->src_width,
              params->src_height, params->dst_width, params->dst_height, params->config->method);

    int convert_result = yuv422_to_printer_binary(params);
    if (convert_result != 0) {
        PR_ERR("Failed to convert YUV422 to binary: %d", convert_result);
        return;
    }

    // Save current UART mode
    UART_MODE_E saved_mode;
    tal_mutex_lock(sg_mode_mutex);
    saved_mode = sg_current_mode;
    PR_DEBUG("Current UART mode: %d", saved_mode);

    // Switch to printer baudrate
    if (__uart_reinit_with_baudrate(PRINTER_BAUDRATE) != OPRT_OK) {
        PR_ERR("Failed to switch to printer baudrate");
        tal_mutex_unlock(sg_mode_mutex);
        return;
    }
    tal_mutex_unlock(sg_mode_mutex);

    // Small delay for baudrate stabilization
    tal_system_sleep(50);

    // Print photo
    dp48a_set_align(DP48A_ALIGN_CENTER);
    dp48a_print_line("--- Camera Photo ---");
    dp48a_feed_lines(1);
    dp48a_print_bitmap(params->dst_width, params->dst_height, params->binary_data);

    // Wait for print to complete
    PR_DEBUG("Waiting for print to complete...");
    tal_system_sleep(200);
    dp48a_feed_lines(3);
    dp48a_print_enter();
    tal_system_sleep(100);

    // Restore UART baudrate
    tal_mutex_lock(sg_mode_mutex);
    __uart_reinit_with_baudrate(sg_mode_configs[saved_mode].baudrate);
    tal_mutex_unlock(sg_mode_mutex);
}

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
    for (uint32_t timeout = 0; sg_mode_switch_request && timeout < MODE_SWITCH_TIMEOUT; timeout += 10) {
        tal_system_sleep(10);
    }

    if (sg_mode_switch_request) {
        PR_ERR("Mode switch timeout");
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

UART_MODE_E uart_expand_get_mode(void)
{
    tal_mutex_lock(sg_mode_mutex);
    UART_MODE_E mode = sg_current_mode;
    tal_mutex_unlock(sg_mode_mutex);
    return mode;
}

OPERATE_RET uart_expand_register_callback(UART_MODE_E mode, uart_data_callback_t callback)
{
    if (mode >= UART_MODE_MAX) {
        PR_ERR("Invalid UART mode: %d", mode);
        return OPRT_INVALID_PARM;
    }

    tal_mutex_lock(sg_mode_mutex);
    sg_mode_configs[mode].callback = callback;
    tal_mutex_unlock(sg_mode_mutex);

    return OPRT_OK;
}

static OPERATE_RET __uart_reinit_with_baudrate(uint32_t baudrate)
{
    tal_uart_deinit(USR_UART_NUM);
    tal_system_sleep(5);

    TAL_UART_CFG_T cfg = {
        .base_cfg = {.baudrate = baudrate,
                     .databits = TUYA_UART_DATA_LEN_8BIT,
                     .stopbits = TUYA_UART_STOP_LEN_1BIT,
                     .parity = TUYA_UART_PARITY_TYPE_NONE},
        .rx_buffer_size = 2048,
        .open_mode = 0 // Non-blocking mode
    };

    OPERATE_RET rt = tal_uart_init(USR_UART_NUM, &cfg);
    if (rt != OPRT_OK) {
        PR_ERR("UART reinit failed with baudrate %d, error: %d", baudrate, rt);
        return rt;
    }

    tal_system_sleep(5);
    sg_current_baudrate = baudrate;
    return OPRT_OK;
}

static void __process_rfid_scan_data(const uint8_t *data, int len)
{
    if (len > 28) {
        rfid_scan_process(data, len, __rfid_scan_data_callback);
    }
}

static void __process_ai_log_data(const uint8_t *data, int len)
{
    if (kmp_search((const char *)data, "ty E") >= 0) {
        uart_data_callback_t callback = sg_mode_configs[UART_MODE_AI_LOG].callback;
        if (callback) {
            callback(UART_MODE_AI_LOG, data, len);
        }
    }
}

static void __uart_worker_thread(void *param)
{
    while (sg_worker_running) {
        // Check for mode switch request
        tal_mutex_lock(sg_mode_mutex);
        BOOL_T need_switch = sg_mode_switch_request;
        UART_MODE_E new_mode = sg_target_mode;
        tal_mutex_unlock(sg_mode_mutex);

        if (need_switch) {
            uint32_t new_baudrate = sg_mode_configs[new_mode].baudrate;

            if (__uart_reinit_with_baudrate(new_baudrate) != OPRT_OK) {
                PR_ERR("Worker UART reinit failed!");
            }

            tal_mutex_lock(sg_mode_mutex);
            sg_current_mode = new_mode;
            sg_mode_switch_request = FALSE;
            tal_mutex_unlock(sg_mode_mutex);
        }

        // Read UART data
        int read_len = tal_uart_read(USR_UART_NUM, sg_read_buffer, READ_BUFFER_SIZE);

        if (read_len > 0) {
            sg_read_buffer[read_len] = '\0';

            tal_mutex_lock(sg_mode_mutex);
            UART_MODE_E current_mode = sg_current_mode;
            tal_mutex_unlock(sg_mode_mutex);

            uint32_t sleep_time = RFID_SCAN_FREQ;
            switch (current_mode) {
            case UART_MODE_RFID_SCAN:
                __process_rfid_scan_data(sg_read_buffer, read_len);
                break;
            case UART_MODE_AI_LOG:
                __process_ai_log_data(sg_read_buffer, read_len);
                sleep_time = AI_LOG_FREQ;
                break;
            default:
                PR_WARN("Unknown mode %d", current_mode);
                break;
            }
            tal_system_sleep(sleep_time);
        } else {
            if (read_len < 0) {
                PR_ERR("UART read error: %d", read_len);
            }
            tal_system_sleep(RFID_SCAN_FREQ);
        }
    }
    PR_NOTICE("UART worker thread stopped");
}

uint32_t uart_print_write(const uint8_t *data, size_t len)
{
    return tuya_ring_buff_write(sg_print_ringbuf, data, len);
}

static void __rfid_scan_data_callback(uint8_t dev_id, RFID_TAG_TYPE_E tag_type, const uint8_t *uid, uint8_t uid_len)
{
    rfid_scan_screen_data_update(dev_id, tag_type, uid, uid_len);
    ai_ui_disp_msg(POCKET_DISP_TP_RFID_SCAN_SUCCESS, NULL, 0);
}

void __printer_scan_thread(void *param)
{
    uint8_t utf8_buf[5];
    uint8_t gbk_buf[4];
    BOOL_T is_printing = FALSE;
    UART_MODE_E saved_mode = UART_MODE_RFID_SCAN;

    dp48a_init();
    PR_NOTICE("Printer scan thread started");

    while (printer_scan_running) {
        if (!sg_print_ringbuf) {
            PR_ERR("Printer ringbuf is NULL");
            tal_system_sleep(100);
            continue;
        }

        uint32_t available = tuya_ring_buff_used_size_get(sg_print_ringbuf);

        if (available == 0) {
            if (is_printing) {
                // Stream ended, restore baudrate
                if (!app_get_text_stream_status()) {
                    dp48a_print_enter();
                    dp48a_feed_lines(2);
                    tal_system_sleep(RFID_SCAN_FREQ);
                }

                tal_mutex_lock(sg_mode_mutex);
                __uart_reinit_with_baudrate(sg_mode_configs[saved_mode].baudrate);
                sg_target_mode = saved_mode;
                sg_mode_switch_request = TRUE;
                tal_mutex_unlock(sg_mode_mutex);

                is_printing = FALSE;
            }
            tal_system_sleep(100);
            continue;
        }

        // Data available, switch to printer baudrate if needed
        if (!is_printing) {
            tal_mutex_lock(sg_mode_mutex);
            saved_mode = sg_current_mode;
            __uart_reinit_with_baudrate(PRINTER_BAUDRATE);
            tal_mutex_unlock(sg_mode_mutex);

            dp48a_set_align(DP48A_ALIGN_LEFT);
            is_printing = TRUE;
        }

        // Read and process UTF8 character
        uint8_t first_byte;
        if (tuya_ring_buff_read(sg_print_ringbuf, &first_byte, 1) != 1) {
            tal_system_sleep(10);
            continue;
        }

        uint8_t char_len = utf8_full_char_len(first_byte);
        if (char_len == 0) {
            PR_WARN("Invalid UTF8 first byte: 0x%02X", first_byte);
            continue;
        }

        utf8_buf[0] = first_byte;

        // Wait for remaining bytes if multi-byte character
        if (char_len > 1) {
            uint32_t retry = 0;
            while (tuya_ring_buff_used_size_get(sg_print_ringbuf) < (char_len - 1) && retry < 200) {
                tal_system_sleep(10);
                retry++;
            }

            if (tuya_ring_buff_used_size_get(sg_print_ringbuf) < (char_len - 1) ||
                tuya_ring_buff_read(sg_print_ringbuf, &utf8_buf[1], char_len - 1) != (char_len - 1)) {
                // Incomplete data, print placeholder
                uint8_t placeholder = 0x3F; // '?'
                dp48a_print_text_raw(&placeholder, 1);
                tal_system_sleep(5);
                continue;
            }
        }

        // Convert UTF8 to GBK and print
        int gbk_len = utf8_to_gbk_buf(utf8_buf, char_len, gbk_buf, sizeof(gbk_buf));
        uint8_t *print_data = (gbk_len > 0) ? gbk_buf : (uint8_t[]){0x3F};
        int print_len = (gbk_len > 0) ? gbk_len : 1;

        dp48a_print_text_raw(print_data, print_len);
        tal_system_sleep(5);
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
    THREAD_CFG_T thrd_param_worker = {0};
    thrd_param_worker.stackDepth = 1024 * 2;
    thrd_param_worker.priority = THREAD_PRIO_1;
    thrd_param_worker.thrdname = "uart_worker_thread";
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
    THREAD_CFG_T thrd_param_printer = {0};
    thrd_param_printer.stackDepth = 1024 * 4;
    thrd_param_printer.priority = THREAD_PRIO_1;
    thrd_param_printer.thrdname = "printer_scan_thread";
    rt =
        tal_thread_create_and_start(&printer_scan_thread, NULL, NULL, __printer_scan_thread, NULL, &thrd_param_printer);
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

    // Register camera screen lifecycle callback
    camera_screen_register_lifecycle_cb(__camera_screen_lifecycle_handler);

    PR_NOTICE("UART expansion initialized with unified worker thread");
    return OPRT_OK;
}