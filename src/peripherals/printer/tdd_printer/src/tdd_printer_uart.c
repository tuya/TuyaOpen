/**
 * @file tdd_printer_uart.c
 * @brief UART based printer TDD implementation
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_printer_uart.h"

#include "tal_log.h"
#include "tal_memory.h"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDD_PRINTER_UART_CFG_T cfg;
} TDD_PRINTER_UART_HANDLE_T;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __tdd_printer_uart_open(TDD_PRINTER_HANDLE_T handle)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_PRINTER_UART_HANDLE_T *hdl = (TDD_PRINTER_UART_HANDLE_T *)handle;

    return tal_uart_init(hdl->cfg.port_id, &hdl->cfg.cfg);
}

static OPERATE_RET __tdd_printer_uart_write(TDD_PRINTER_HANDLE_T handle, const uint8_t *data, uint32_t len)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(data, OPRT_INVALID_PARM);

    TDD_PRINTER_UART_HANDLE_T *hdl = (TDD_PRINTER_UART_HANDLE_T *)handle;

    int ret = tal_uart_write(hdl->cfg.port_id, data, len);
    if (ret < 0) {
        PR_ERR("UART write error: %d", ret);
        return ret;
    }

    if ((uint32_t)ret != len) {
        PR_WARN("UART partial write, expect:%u real:%d", len, ret);
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_printer_uart_close(TDD_PRINTER_HANDLE_T handle)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_PRINTER_UART_HANDLE_T *hdl = (TDD_PRINTER_UART_HANDLE_T *)handle;

    return tal_uart_deinit(hdl->cfg.port_id);
}

OPERATE_RET tdd_printer_uart_register(char *name, TDD_PRINTER_UART_CFG_T cfg)
{
    OPERATE_RET rt = OPRT_OK;

    TDD_PRINTER_UART_HANDLE_T *hdl = (TDD_PRINTER_UART_HANDLE_T *)tal_malloc(sizeof(TDD_PRINTER_UART_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(hdl, OPRT_MALLOC_FAILED);
    memset(hdl, 0, sizeof(TDD_PRINTER_UART_HANDLE_T));

    memcpy(&hdl->cfg, &cfg, sizeof(TDD_PRINTER_UART_CFG_T));

    TDD_PRINTER_INTFS_T uart_intfs = {
        .open = __tdd_printer_uart_open,
        .write = __tdd_printer_uart_write,
        .close = __tdd_printer_uart_close,
    };

    TDD_PRINTER_DEV_INFO_T dev_info = {0};

    rt = tdl_printer_driver_register(name, &uart_intfs, &dev_info, (TDD_PRINTER_HANDLE_T)hdl);
    if (rt != OPRT_OK) {
        tal_free(hdl);
        hdl = NULL;
        PR_ERR("Failed to register UART printer: %d", rt);
    }

    return rt;
}
