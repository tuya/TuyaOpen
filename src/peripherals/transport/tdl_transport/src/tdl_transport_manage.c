/**
 * @file tdl_transport_manage.c
 * @brief tdl_transport_manage module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdl_transport_manage.h"

#include "tal_api.h"

#include "tuya_list.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TDL_TRANSPORT_MAGIC 0x12345678 // Magic number to identify the transport structure

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef uint8_t TDL_TRANSPORT_STATUS_T;
#define TDL_TRANSPORT_STATUS_UNINIT  0x00
#define TDL_TRANSPORT_STATUS_INITED  0x01
#define TDL_TRANSPORT_STATUS_READY   0x02
#define TDL_TRANSPORT_STATUS_SEND    0x03
#define TDL_TRANSPORT_STATUS_RECVING 0x04

typedef struct {
    LIST_HEAD node;

    int magic;                             // Magic number to identify the structure
    char name[TDL_TRANSPORT_NAME_MAX_LEN]; // Transport name

    TDL_TRANSPORT_STATUS_T status;     // Transport status
    TDD_TRANSPORT_HANDLE_T tdd_handle; // Transport driver handle

    TDD_TRANSPORT_INTFS_T intfs; // Transport driver interfaces
} TDL_TRANSPORT_T, TDL_TRANSPORT_NODE_T;

typedef struct {
    LIST_HEAD head; // List head for managing transport nodes

    MUTEX_HANDLE mutex; // Mutex for thread safety
} TDL_TRANSPORT_LIST_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_TRANSPORT_LIST_T g_transport_list = {.head = LIST_HEAD_INIT(g_transport_list.head), .mutex = NULL};

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET tdl_transport_find(const char *name, TDL_TRANSPORT_HANDLE *handle)
{
    // OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(name, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    if (NULL == g_transport_list.mutex) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(g_transport_list.mutex);

    struct tuya_list_head *p = NULL;
    TDL_TRANSPORT_NODE_T *transport = NULL;

    tuya_list_for_each(p, &g_transport_list.head)
    {
        transport = tuya_list_entry(p, TDL_TRANSPORT_NODE_T, node);
        if (transport->magic != TDL_TRANSPORT_MAGIC) {
            tal_mutex_unlock(g_transport_list.mutex);
            return OPRT_COM_ERROR; // Invalid magic number, maybe a corrupted list
        }

        if (strcmp(name, transport->name) == 0) {
            *handle = (TDL_TRANSPORT_HANDLE)transport;
            tal_mutex_unlock(g_transport_list.mutex);
            return OPRT_OK; // Found the transport
        }
    }

    tal_mutex_unlock(g_transport_list.mutex);

    return OPRT_NOT_FOUND;
}

OPERATE_RET tdl_transport_open(TDL_TRANSPORT_HANDLE handle)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDL_TRANSPORT_NODE_T *node = (TDL_TRANSPORT_NODE_T *)handle;

    if (node->magic != TDL_TRANSPORT_MAGIC) {
        PR_ERR("Invalid transport handle magic: %d", node->magic);
        return OPRT_COM_ERROR; // Invalid magic number
    }

    if (node->status != TDL_TRANSPORT_STATUS_UNINIT) {
        PR_ERR("Transport handle is not uninitialized: %d", node->status);
        return OPRT_COM_ERROR; // Already initialized or in use
    }

    TUYA_CHECK_NULL_RETURN(node->intfs.open, OPRT_INVALID_PARM);
    TUYA_CALL_ERR_RETURN(node->intfs.open(node->tdd_handle));

    node->status = TDL_TRANSPORT_STATUS_INITED;

    PR_DEBUG("Transport opened successfully: %s", node->name);

    return OPRT_OK;
}

OPERATE_RET tdl_transport_send(TDL_TRANSPORT_HANDLE handle, const uint8_t *data, uint32_t len)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(data, OPRT_INVALID_PARM);
    if (len == 0) {
        PR_ERR("Data length must be greater than 0");
        return OPRT_INVALID_PARM; // Length must be greater than 0
    }

    TDL_TRANSPORT_NODE_T *node = (TDL_TRANSPORT_NODE_T *)handle;
    if (node->magic != TDL_TRANSPORT_MAGIC) {
        PR_ERR("Invalid transport handle magic: %d", node->magic);
        return OPRT_COM_ERROR; // Invalid magic number
    }
    if (node->status != TDL_TRANSPORT_STATUS_INITED) {
        PR_ERR("Transport handle is not init, current status: %d, transport name: %s", node->status, node->name);
        return OPRT_COM_ERROR;
    }

    TUYA_CHECK_NULL_RETURN(node->intfs.send, OPRT_INVALID_PARM);

    // PR_DEBUG("Sending %s to transport: %s", (char *)data, node->name);
    rt = node->intfs.send(node->tdd_handle, data, len);
    if (rt < 0) {
        PR_ERR("Transport send error: %d", rt);
        return rt; // Return the error code from the send operation
    }

    return OPRT_OK;
}

uint32_t tdl_transport_read(TDL_TRANSPORT_HANDLE handle, uint8_t *data, uint32_t len)
{
    // OPERATE_RET rt = OPRT_OK;
    uint32_t recv_len = 0;

    TUYA_CHECK_NULL_RETURN(handle, 0);
    TUYA_CHECK_NULL_RETURN(data, 0);
    if (len == 0) {
        PR_ERR("Data length must be greater than 0");
        return 0; // Length must be greater than 0
    }

    TDL_TRANSPORT_NODE_T *node = (TDL_TRANSPORT_NODE_T *)handle;
    if (node->magic != TDL_TRANSPORT_MAGIC) {
        PR_ERR("Invalid transport handle magic: %d", node->magic);
        return 0; // Invalid magic number
    }
    if (node->status != TDL_TRANSPORT_STATUS_INITED) {
        PR_ERR("Transport handle is not init, current status: %d, transport name: %s", node->status, node->name);
        return 0;
    }

    TUYA_CHECK_NULL_RETURN(node->intfs.read, 0);
    recv_len = node->intfs.read(node->tdd_handle, data, len);

    return recv_len;
}

uint32_t tdl_transport_available(TDL_TRANSPORT_HANDLE handle)
{
    TUYA_CHECK_NULL_RETURN(handle, 0);

    TDL_TRANSPORT_NODE_T *node = (TDL_TRANSPORT_NODE_T *)handle;
    if (node->magic != TDL_TRANSPORT_MAGIC) {
        PR_ERR("Invalid transport handle magic: %d", node->magic);
        return 0; // Invalid magic number
    }
    if (node->status != TDL_TRANSPORT_STATUS_INITED) {
        PR_ERR("Transport handle is not init, current status: %d, transport name: %s", node->status, node->name);
        return 0;
    }

    TUYA_CHECK_NULL_RETURN(node->intfs.available, 0);
    return node->intfs.available(node->tdd_handle);
}

OPERATE_RET tdl_transport_config(TDL_TRANSPORT_HANDLE handle, TDL_TRANSPORT_CMD_T cmd, void *param)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDL_TRANSPORT_NODE_T *node = (TDL_TRANSPORT_NODE_T *)handle;
    if (node->magic != TDL_TRANSPORT_MAGIC) {
        PR_ERR("Invalid transport handle magic: %d", node->magic);
        return OPRT_COM_ERROR; // Invalid magic number
    }

    switch (cmd) {
    case TDL_TRANSPORT_CMD_RX_BUFFER_RESET: {
        TUYA_CHECK_NULL_RETURN(node->intfs.config, 0);
        rt = node->intfs.config(node->tdd_handle, cmd, param);
    } break;
    default:
        break;
    }

    return rt;
}

OPERATE_RET tdl_transport_close(TDL_TRANSPORT_HANDLE handle)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    rt = OPRT_NOT_SUPPORTED;
    return rt;
}

OPERATE_RET
tdl_transport_driver_register(char *name, TDD_TRANSPORT_INTFS_T *intfs, TDD_TRANSPORT_HANDLE_T tdd_hdl)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(name, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(intfs, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(tdd_hdl, OPRT_INVALID_PARM);

    if (NULL == g_transport_list.mutex) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&g_transport_list.mutex));
    }

    TDL_TRANSPORT_NODE_T *node = (TDL_TRANSPORT_NODE_T *)tal_malloc(sizeof(TDL_TRANSPORT_NODE_T));
    TUYA_CHECK_NULL_RETURN(node, OPRT_MALLOC_FAILED);
    memset(node, 0, sizeof(TDL_TRANSPORT_NODE_T));

    INIT_LIST_HEAD(&node->node);

    node->magic = TDL_TRANSPORT_MAGIC;
    node->status = TDL_TRANSPORT_STATUS_UNINIT;
    node->tdd_handle = tdd_hdl;
    strncpy(node->name, name, sizeof(node->name) - 1);
    memcpy(&node->intfs, intfs, sizeof(TDD_TRANSPORT_INTFS_T));

    // Add the new transport node to the list
    tal_mutex_lock(g_transport_list.mutex);
    tuya_list_add_tail(&node->node, &g_transport_list.head);
    tal_mutex_unlock(g_transport_list.mutex);

    return rt;
}