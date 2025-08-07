/**
 * @file at_client.h
 * @brief at_client module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __AT_CLIENT_H__
#define __AT_CLIENT_H__

#include "tuya_cloud_types.h"
#include "at_line.h"

#include "tdl_transport_manage.h"

#include "tuya_slist.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define LINE_END_SYMBOL_MAX_LEN 8
#define LINE_END_SYMBOL_CRLF    "\r\n"
#define LINE_END_SYMBOL_LF      "\n"
#define LINE_END_SYMBOL_CR      "\r"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    SLIST_HEAD node;                               // Node for single linked list
    char *prefix;                                  // Prefix of the response
    char *suffix;                                  // Suffix of the response
    void (*urc_handler)(char *data, uint32_t len); // URC handler function
} AT_URC_T;

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET at_client_init(TDL_TRANSPORT_HANDLE transport_hdl);

OPERATE_RET at_client_deinit(void);

OPERATE_RET at_client_send(const char *cmd, uint32_t len);

AT_LINE_T *at_client_get_response(uint32_t timeout_ms);

OPERATE_RET at_client_send_with_rsp(const char *cmd, uint32_t len, AT_LINE_T **rsp_line, uint32_t timeout_ms);

void at_client_response_free(AT_LINE_T *line);

OPERATE_RET at_client_urc_handler_register(AT_URC_T *urc_handler);

#ifdef __cplusplus
}
#endif

#endif /* __AT_CLIENT_H__ */
