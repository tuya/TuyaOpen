/**
 * @file tdd_printer_dp48.c
 * @brief DP-48A ESC/POS thermal printer TDD driver implementation
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_printer_dp48.h"

#include "tal_log.h"
#include "tal_memory.h"
#include "tal_system.h"
#include "tal_uart.h"

/***********************************************************
************************macro define************************
***********************************************************/
/* TX-drain guard before tal_uart_deinit().
 *
 * tal_uart_write() returns once bytes are queued in the TX ring / hardware
 * FIFO, not once they have actually been clocked out on the line, and
 * tal_uart has no public flush/drain API. If close() calls
 * tal_uart_deinit() while the FIFO still holds bytes, those bytes are
 * lost: the printer receives a truncated stream and the next print job
 * starts mid-command from the printer's point of view, producing garbled
 * output.
 *
 * Sleep before deinit to give the FIFO time to fully drain. The size of a
 * typical UART hardware FIFO + small ring buffer at 9600 baud is bounded
 * by a few hundred bytes, so 200 ms is comfortably above the worst case. */
#define DP48_UART_DRAIN_GUARD_MS 200

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDD_PRINTER_DP48_CFG_T cfg;
} TDD_PRINTER_DP48_HANDLE_T;

/***********************************************************
***********************function define**********************
***********************************************************/
/* Initialize the UART. If init fails (e.g. because a previous open did
 * not deinit cleanly, or another module left the port in a stale state),
 * deinit once and try again so the next print job has a clean port. */
static OPERATE_RET __tdd_printer_dp48_open(TDD_PRINTER_HANDLE_T handle)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_PRINTER_DP48_HANDLE_T *hdl = (TDD_PRINTER_DP48_HANDLE_T *)handle;

    OPERATE_RET rt = tal_uart_init(hdl->cfg.port_id, &hdl->cfg.uart_cfg);
    if (rt == OPRT_OK) {
        return OPRT_OK;
    }

    PR_WARN("DP48 UART init failed (rt=%d), retrying after deinit", rt);
    (void)tal_uart_deinit(hdl->cfg.port_id);
    rt = tal_uart_init(hdl->cfg.port_id, &hdl->cfg.uart_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("DP48 UART init retry failed: %d", rt);
    }
    return rt;
}

static OPERATE_RET __tdd_printer_dp48_write(TDD_PRINTER_HANDLE_T handle,
                                             const uint8_t *data, uint32_t len)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(data, OPRT_INVALID_PARM);

    TDD_PRINTER_DP48_HANDLE_T *hdl = (TDD_PRINTER_DP48_HANDLE_T *)handle;

    int ret = tal_uart_write(hdl->cfg.port_id, data, len);
    if (ret < 0) {
        PR_ERR("DP48 UART write error: %d", ret);
        return ret;
    }

    if ((uint32_t)ret != len) {
        PR_WARN("DP48 UART partial write, expect:%u real:%d", len, ret);
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_printer_dp48_close(TDD_PRINTER_HANDLE_T handle)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_PRINTER_DP48_HANDLE_T *hdl = (TDD_PRINTER_DP48_HANDLE_T *)handle;

    /* Wait for any bytes still queued in the UART TX FIFO / ring buffer
     * to be clocked out before tearing the peripheral down — see the
     * comment on DP48_UART_DRAIN_GUARD_MS. */
    tal_system_sleep(DP48_UART_DRAIN_GUARD_MS);

    return tal_uart_deinit(hdl->cfg.port_id);
}

static OPERATE_RET __tdd_printer_dp48_paper_feed(TDD_PRINTER_HANDLE_T handle, uint32_t lines)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    if (lines == 0) {
        return OPRT_OK;
    }

    TDD_PRINTER_DP48_HANDLE_T *hdl = (TDD_PRINTER_DP48_HANDLE_T *)handle;

    /* ESC d n — feed n lines */
    uint8_t n = (lines > 255u) ? 255u : (uint8_t)lines;
    uint8_t cmd[] = {0x1B, 0x64, n};

    int ret = tal_uart_write(hdl->cfg.port_id, cmd, sizeof(cmd));
    return (ret == (int)sizeof(cmd)) ? OPRT_OK : OPRT_COM_ERROR;
}

OPERATE_RET tdd_printer_dp48_register(char *name, TDD_PRINTER_DP48_CFG_T *cfg)
{
    TUYA_CHECK_NULL_RETURN(name, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(cfg, OPRT_INVALID_PARM);

    TDD_PRINTER_DP48_HANDLE_T *hdl =
        (TDD_PRINTER_DP48_HANDLE_T *)tal_malloc(sizeof(TDD_PRINTER_DP48_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(hdl, OPRT_MALLOC_FAILED);
    memset(hdl, 0, sizeof(TDD_PRINTER_DP48_HANDLE_T));
    memcpy(&hdl->cfg, cfg, sizeof(TDD_PRINTER_DP48_CFG_T));

    /* TAL requires a non-zero rx ring buffer even for TX-only devices */
    if (hdl->cfg.uart_cfg.rx_buffer_size == 0) {
        hdl->cfg.uart_cfg.rx_buffer_size = 64;
    }

    TDD_PRINTER_INTFS_T dp48_intfs = {
        .open        = __tdd_printer_dp48_open,
        .write       = __tdd_printer_dp48_write,
        .close       = __tdd_printer_dp48_close,
        .paper_feed  = __tdd_printer_dp48_paper_feed,
    };

    TDD_PRINTER_DEV_INFO_T dev_info = {
        .dots_per_line  = DP48_DOTS_PER_LINE,
        .bytes_per_line = DP48_BYTES_PER_LINE,
        .encoding       = TDD_PRINTER_ENCODING_GBK,
        .protocol       = TDD_PRINTER_PROTOCOL_ESCPOS,
    };

    OPERATE_RET rt = tdl_printer_driver_register(name, &dp48_intfs, &dev_info,
                                                  (TDD_PRINTER_HANDLE_T)hdl);
    if (rt != OPRT_OK) {
        tal_free(hdl);
        hdl = NULL;
        PR_ERR("Failed to register DP48 printer: %d", rt);
    }

    return rt;
}
