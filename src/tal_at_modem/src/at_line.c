/**
 * @file at_line.c
 * @brief at_line module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "at_line.h"

#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AT_LINE_MAGIC 0x12345678

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint32_t magic;
    SLIST_HEAD line_head;
    uint32_t line_count;
} AT_LINE_HANDLE_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
OPERATE_RET at_line_init(AT_LINE_HANDLE *line_hdl)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(line_hdl, OPRT_INVALID_PARM);

    AT_LINE_HANDLE_T *line = (AT_LINE_HANDLE_T *)tal_malloc(sizeof(AT_LINE_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(line, OPRT_MALLOC_FAILED);
    memset(line, 0, sizeof(AT_LINE_HANDLE_T));

    line->magic = AT_LINE_MAGIC;
    tuya_init_slist_node(&line->line_head);
    line->line_count = 0;

    *line_hdl = (AT_LINE_HANDLE)line;

    return rt;
}

OPERATE_RET at_line_add(AT_LINE_HANDLE line_hdl, AT_LINE_T *new_line)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(line_hdl, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(new_line, OPRT_INVALID_PARM);

    AT_LINE_HANDLE_T *at_line = (AT_LINE_HANDLE_T *)line_hdl;
    if (at_line->magic != AT_LINE_MAGIC) {
        PR_ERR("Invalid AT line handle");
        return OPRT_INVALID_PARM;
    }

    tuya_init_slist_node(&new_line->node);
    tuya_slist_add_tail(&at_line->line_head, &new_line->node);
    at_line->line_count++;

    return rt;
}

AT_LINE_T *at_line_get(AT_LINE_HANDLE line_hdl)
{
    TUYA_CHECK_NULL_RETURN(line_hdl, NULL);
    AT_LINE_HANDLE_T *at_line = (AT_LINE_HANDLE_T *)line_hdl;
    if (at_line->magic != AT_LINE_MAGIC) {
        PR_ERR("Invalid AT line handle");
        return NULL;
    }

    if (0 == at_line->line_count) {
        PR_DEBUG("No lines available");
        return NULL;
    }

    AT_LINE_T *line = NULL;
    line = (AT_LINE_T *)at_line->line_head.next;

    // Remove the line from the list
    tuya_slist_del(&at_line->line_head, &line->node);
    at_line->line_count--;

    return line;
}

AT_LINE_T *at_line_get_by_key(AT_LINE_HANDLE line_hdl, const char *key)
{
    TUYA_CHECK_NULL_RETURN(line_hdl, NULL);
    TUYA_CHECK_NULL_RETURN(key, NULL);
    AT_LINE_HANDLE_T *at_line = (AT_LINE_HANDLE_T *)line_hdl;
    if (at_line->magic != AT_LINE_MAGIC) {
        PR_ERR("Invalid AT line handle");
        return NULL;
    }

    AT_LINE_T *line = NULL;
    SLIST_HEAD *pos = NULL;

    SLIST_FOR_EACH(pos, &at_line->line_head)
    {
        line = (AT_LINE_T *)pos;
        if (strncmp(line->line, key, strlen(key)) == 0) {
            // Found the line with the key
            tuya_slist_del(&at_line->line_head, &line->node);
            at_line->line_count--;
            return line;
        }
    }

    return NULL; // Not found
}

uint32_t at_line_get_count(AT_LINE_HANDLE line_hdl)
{
    if (line_hdl == NULL) {
        return 0;
    }

    AT_LINE_HANDLE_T *at_line = (AT_LINE_HANDLE_T *)line_hdl;
    if (at_line->magic != AT_LINE_MAGIC) {
        PR_ERR("Invalid AT line handle");
        return 0;
    }

    return at_line->line_count;
}

AT_LINE_T *at_line_create(const char *line, uint32_t len)
{
    TUYA_CHECK_NULL_RETURN(line, NULL);

    AT_LINE_T *new_line = (AT_LINE_T *)tal_malloc(sizeof(AT_LINE_T) + len + 1);
    TUYA_CHECK_NULL_RETURN(new_line, NULL);
    memset(new_line, 0, sizeof(AT_LINE_T));

    new_line->len = len;
    new_line->line = (char *)(new_line + 1); // Point to the memory after the struct

    memcpy(new_line->line, line, len);
    new_line->line[len] = '\0'; // Null-terminate the string

    return new_line;
}

OPERATE_RET at_line_free(AT_LINE_T *line)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(line, OPRT_INVALID_PARM);

    tal_free(line);

    return rt;
}

OPERATE_RET at_line_deinit(AT_LINE_HANDLE line_hdl)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(line_hdl, OPRT_INVALID_PARM);
    AT_LINE_HANDLE_T *at_line = (AT_LINE_HANDLE_T *)line_hdl;
    if (at_line->magic != AT_LINE_MAGIC) {
        PR_ERR("Invalid AT line handle");
        return OPRT_INVALID_PARM;
    }

    // Free all lines in the list
    AT_LINE_T *line = NULL;
    while ((line = at_line_get(line_hdl)) != NULL) {
        at_line_free(line);
    }

    // Free the handle itself
    tal_free(at_line);

    return rt;
}