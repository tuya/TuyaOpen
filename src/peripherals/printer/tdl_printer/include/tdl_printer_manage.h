/**
 * @file tdl_printer_manage.h
 * @brief Printer manager API for TDL layer
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TDL_PRINTER_MANAGE_H__
#define __TDL_PRINTER_MANAGE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define TDL_PRINTER_NAME_MAX_LEN 32

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void *TDL_PRINTER_HANDLE;

typedef enum {
    TDL_PRINTER_EVENT_NONE = 0,
    TDL_PRINTER_EVENT_PAPER_OUT,
    TDL_PRINTER_EVENT_PAPER_IN,
    TDL_PRINTER_EVENT_OVERHEATED,
    TDL_PRINTER_EVENT_TEMP_NORMAL,
    TDL_PRINTER_EVENT_ERROR,
} TDL_PRINTER_EVENT_E;

typedef void (*TDL_PRINTER_EVENT_CB)(TDL_PRINTER_HANDLE handle, TDL_PRINTER_EVENT_E event, void *data, void *arg);

typedef struct {
    TDL_PRINTER_EVENT_CB event_cb;
    void                *event_cb_arg;
    uint32_t             poll_interval_ms;
} TDL_PRINTER_OPEN_PARAM_T;

typedef struct {
    uint32_t dots_per_line;
    uint32_t bytes_per_line;
} TDL_PRINTER_DEV_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Find a registered printer by name
 * @param[in] name printer name
 * @param[out] handle output printer handle
 * @return OPRT_OK on success, OPRT_NOT_FOUND if not registered
 */
OPERATE_RET tdl_printer_find(const char *name, TDL_PRINTER_HANDLE *handle);

/**
 * @brief Open the printer and optionally start status monitoring
 * @param[in] handle printer handle
 * @param[in] param open parameters (callback, poll interval). NULL if
 *            no event monitoring is needed.
 * @return OPRT_OK on success
 * @note When param is provided with a non-NULL event_cb, a background
 *       thread is created to poll the TDD driver's get_status at the
 *       specified poll_interval_ms. If poll_interval_ms is 0, the
 *       default interval from Kconfig is used. The callback is invoked
 *       from the monitoring thread context on status changes.
 */
OPERATE_RET tdl_printer_open(TDL_PRINTER_HANDLE handle, TDL_PRINTER_OPEN_PARAM_T *param);

/**
 * @brief Start a print job
 * @param[in] handle printer handle
 * @return OPRT_OK on success, OPRT_NOT_SUPPORTED if the TDD driver does
 *         not implement start
 * @note Must be called before tdl_printer_send. The TDD driver may
 *       perform pre-checks (paper, temperature) and hardware preparation.
 */
OPERATE_RET tdl_printer_start(TDL_PRINTER_HANDLE handle);

/**
 * @brief Send data to the printer
 * @param[in] handle printer handle
 * @param[in] data data buffer
 * @param[in] len data length
 * @return OPRT_OK on success
 */
OPERATE_RET tdl_printer_send(TDL_PRINTER_HANDLE handle, const uint8_t *data, uint32_t len);

/**
 * @brief End a print job
 * @param[in] handle printer handle
 * @return OPRT_OK on success, OPRT_NOT_SUPPORTED if the TDD driver does
 *         not implement end
 * @note Must be called after the last tdl_printer_send of a job. The TDD
 *       driver may perform post-processing such as paper feed to tear
 *       position, motor stop, etc.
 */
OPERATE_RET tdl_printer_end(TDL_PRINTER_HANDLE handle);

/**
 * @brief Get printer device information
 * @param[in] handle printer handle
 * @param[out] info device information output
 * @return OPRT_OK on success
 */
OPERATE_RET tdl_printer_get_dev_info(TDL_PRINTER_HANDLE handle, TDL_PRINTER_DEV_INFO_T *info);

/**
 * @brief Feed paper without printing
 * @param[in] handle printer handle
 * @param[in] lines number of dot lines to feed
 * @return OPRT_OK on success
 */
OPERATE_RET tdl_printer_paper_feed(TDL_PRINTER_HANDLE handle, uint32_t lines);

/**
 * @brief Send UTF-8 text to the printer with automatic encoding conversion
 * @param[in] handle printer handle
 * @param[in] text UTF-8 encoded text string
 * @return OPRT_OK on success, OPRT_NOT_SUPPORTED if driver protocol is RAW
 * @note For GBK drivers (e.g. DP48A), text is converted from UTF-8 to GBK
 *       internally. For UTF-8 or NONE drivers, text is sent as-is.
 */
OPERATE_RET tdl_printer_send_text(TDL_PRINTER_HANDLE handle, const char *text);

/**
 * @brief Send a monochrome bitmap to the printer
 * @param[in] handle printer handle
 * @param[in] x      horizontal start position in dots (0 = left edge)
 * @param[in] width  image width in pixels
 * @param[in] height image height in pixels
 * @param[in] data   1-bit monochrome bitmap, row-major, MSB first,
 *                   (width+7)/8 bytes per row
 * @return OPRT_OK on success
 * @note Images extending beyond the right edge are silently clipped.
 *       If x >= printer_width, the call succeeds without printing anything.
 *       For RAW protocol, dots_per_line must be non-zero.
 */
OPERATE_RET tdl_printer_send_bitmap(TDL_PRINTER_HANDLE handle,
                                    uint16_t x, uint16_t width,
                                    uint16_t height, const uint8_t *data);

/**
 * @brief Close the printer and release resources
 * @param[in] handle printer handle
 * @return OPRT_OK on success
 * @note Stops the status monitoring thread if running.
 */
OPERATE_RET tdl_printer_close(TDL_PRINTER_HANDLE handle);

#ifdef __cplusplus
}
#endif

#endif /* __TDL_PRINTER_MANAGE_H__ */
