/**
 * @file at_line.h
 * @brief at_line module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __AT_LINE_H__
#define __AT_LINE_H__

#include "tuya_cloud_types.h"

#include "tuya_slist.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef void *AT_LINE_HANDLE;

#pragma pack(1)
typedef struct {
    SLIST_HEAD node;
    uint32_t len; // Length of the line
    char *line;   // Line of AT command or response
} AT_LINE_T;
#pragma pack()

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET at_line_init(AT_LINE_HANDLE *line_hdl);

OPERATE_RET at_line_add(AT_LINE_HANDLE line_hdl, AT_LINE_T *new_line);

AT_LINE_T *at_line_get(AT_LINE_HANDLE line_hdl);

AT_LINE_T *at_line_get_by_key(AT_LINE_HANDLE line_hdl, const char *key);

uint32_t at_line_get_count(AT_LINE_HANDLE line_hdl);

AT_LINE_T *at_line_create(const char *line, uint32_t len);

OPERATE_RET at_line_free(AT_LINE_T *line);

OPERATE_RET at_line_deinit(AT_LINE_HANDLE line_hdl);

#ifdef __cplusplus
}
#endif

#endif /* __AT_LINE_H__ */
