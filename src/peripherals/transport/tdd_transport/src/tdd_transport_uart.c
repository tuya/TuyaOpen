/**
 * @file tdd_transport_uart.c
 * @brief tdd_transport_uart module is used to implement UART transport layer
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_transport_uart.h"

#include "tal_log.h"
#include "tal_memory.h"
#include "tal_uart.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDD_TRANSPORT_UART_CFG_T cfg;
} TDD_TRANSPORT_UART_HANDLE_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

static OPERATE_RET __tdd_transport_uart_open(TDD_TRANSPORT_HANDLE_T handle)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_TRANSPORT_UART_HANDLE_T *hdl = (TDD_TRANSPORT_UART_HANDLE_T *)handle;

    TUYA_CALL_ERR_RETURN(tal_uart_init(hdl->cfg.port_id, &hdl->cfg.cfg));

    return rt;
}

static OPERATE_RET __tdd_transport_uart_send(TDD_TRANSPORT_HANDLE_T handle, const uint8_t *data, uint32_t len)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_TRANSPORT_UART_HANDLE_T *hdl = (TDD_TRANSPORT_UART_HANDLE_T *)handle;

    return tal_uart_write(hdl->cfg.port_id, data, len);
}

static uint32_t __tdd_transport_uart_read(TDD_TRANSPORT_HANDLE_T handle, uint8_t *data, uint32_t len)
{
    uint32_t read_len = 0;

    TUYA_CHECK_NULL_RETURN(handle, 0);
    TDD_TRANSPORT_UART_HANDLE_T *hdl = (TDD_TRANSPORT_UART_HANDLE_T *)handle;

    int ret = tal_uart_read(hdl->cfg.port_id, data, len);
    if (ret < 0) {
        PR_ERR("UART read error: %d", ret);
        read_len = 0;
    } else {
        read_len = ret;
    }

    return read_len;
}

static uint32_t __tdd_transport_uart_available(TDD_TRANSPORT_HANDLE_T handle)
{
    TUYA_CHECK_NULL_RETURN(handle, 0);
    TDD_TRANSPORT_UART_HANDLE_T *hdl = (TDD_TRANSPORT_UART_HANDLE_T *)handle;

    int available_len = tal_uart_get_rx_data_size(hdl->cfg.port_id);
    if (available_len < 0) {
        PR_ERR("UART available error: %d", available_len);
        return 0;
    }

    return (uint32_t)available_len;
}

static OPERATE_RET __tdd_transport_uart_config(TDD_TRANSPORT_HANDLE_T handle, TDL_TRANSPORT_CMD_T cmd, void *param)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_TRANSPORT_UART_HANDLE_T *hdl = (TDD_TRANSPORT_UART_HANDLE_T *)handle;

    (void)hdl;

    switch (cmd) {
    case TDL_TRANSPORT_CMD_RX_BUFFER_RESET: {
        // Reset the RX buffer
        // TODO:
        rt = OPRT_NOT_SUPPORTED;
        PR_ERR("RX buffer reset not supported");
    } break;
    default:
        break;
    }

    return rt;
}

static OPERATE_RET __tdd_transport_uart_close(TDD_TRANSPORT_HANDLE_T handle)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    TDD_TRANSPORT_UART_HANDLE_T *hdl = (TDD_TRANSPORT_UART_HANDLE_T *)handle;

    TUYA_CALL_ERR_RETURN(tal_uart_deinit(hdl->cfg.port_id));

    return rt;
}

OPERATE_RET tdd_transport_uart_register(char *name, TDD_TRANSPORT_UART_CFG_T cfg)
{
    OPERATE_RET rt = OPRT_OK;

    TDD_TRANSPORT_UART_HANDLE_T *hdl = (TDD_TRANSPORT_UART_HANDLE_T *)tal_malloc(sizeof(TDD_TRANSPORT_UART_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(hdl, OPRT_MALLOC_FAILED);
    memset(hdl, 0, sizeof(TDD_TRANSPORT_UART_HANDLE_T));

    memcpy(&hdl->cfg, &cfg, sizeof(TDD_TRANSPORT_UART_CFG_T));

    TDD_TRANSPORT_INTFS_T uart_intfs = {
        .open = __tdd_transport_uart_open,
        .send = __tdd_transport_uart_send,
        .read = __tdd_transport_uart_read,
        .available = __tdd_transport_uart_available,
        .config = __tdd_transport_uart_config,
        .close = __tdd_transport_uart_close,
    };

    rt = tdl_transport_driver_register(name, &uart_intfs, (TDD_TRANSPORT_HANDLE_T)hdl);
    if (rt != OPRT_OK) {
        tal_free(hdl);
        hdl = NULL;
        PR_ERR("Failed to register UART transport: %d", rt);
    }

    return rt;
}
