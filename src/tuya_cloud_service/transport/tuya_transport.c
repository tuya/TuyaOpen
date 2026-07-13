/**
 * @file tuya_transport.c
 * @brief Implementation of Tuya transport array for managing multiple
 * transporters.
 *
 * This file contains the implementation of the Tuya transport array, a
 * mechanism for managing multiple transporters (TCP, TLS, WebSocket) within the
 * Tuya IoT SDK. It provides functionalities to create a transport array, add
 * transporters to the array, and manage the lifecycle of these transporters.
 * The transport array allows for efficient switching and management of
 * different transport protocols, facilitating flexible communication strategies
 * for Tuya IoT devices.
 *
 * The implementation includes the creation of a transport array handle, dynamic
 * memory allocation for the transport array, and debugging support for tracking
 * the transport array's state. This approach enhances the modularity and
 * scalability of Tuya IoT device communication.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tuya_transporter.h"
#include "tls_transporter.h"
#include "tcp_transporter.h"
#include "websocket_transporter.h"
#include "mix_method.h"

#define MAX_TRANSPORTER_NUM (2)

struct tuya_transport_array_handle {
    tuya_transporter_t array[MAX_TRANSPORTER_NUM];
    uint8_t index;
};

tuya_transport_array_handle_t tuya_transport_array_create()
{
    tuya_transport_array_handle_t p_transport_array =
        (tuya_transport_array_handle_t)Malloc(sizeof(struct tuya_transport_array_handle));
    PR_DEBUG("p_trans array:0x%x", p_transport_array);
    if (p_transport_array) {
        memset(p_transport_array, 0, sizeof(struct tuya_transport_array_handle));
    }

    return p_transport_array;
}

/**
 * @brief Adds a transporter to the transport array.
 *
 * This function adds a transporter to the specified transport array handle.
 *
 * @param transport_handle The handle of the transport array.
 * @param transporter The transporter to be added.
 * @param scheme The scheme associated with the transporter.
 *
 * @return The result of the operation. Possible values are:
 *         - OPRT_OK: Operation successful.
 *         - OPRT_INVALID_PARM: Invalid parameter.
 *         - OPRT_MALLOC_FAILED: Memory allocation failed.
 *         - OPRT_ERR: Other error occurred.
 */
OPERATE_RET tuya_transport_array_add_transporter(tuya_transport_array_handle_t transport_handle,
                                                 tuya_transporter_t transporter, char *scheme)
{
    if (transport_handle->index >= MAX_TRANSPORTER_NUM) {
        return OPRT_INDEX_OUT_OF_BOUND;
    }

    transporter->scheme = mm_strdup(scheme);
    transport_handle->array[transport_handle->index++] = transporter;
    return OPRT_OK;
}

/**
 * @brief Retrieves a transporter from the transport array based on the
 * specified scheme.
 *
 * This function searches for a transporter in the transport array associated
 * with the given transport handle that matches the specified scheme. If a
 * matching transporter is found, it is returned.
 *
 * @param transport_handle The handle to the transport array.
 * @param scheme The scheme to match against when searching for a transporter.
 * @return The matching transporter if found, or NULL if no matching transporter
 * is found.
 */
tuya_transporter_t tuya_transport_array_get_transporter(tuya_transport_array_handle_t transport_handle, char *scheme)
{
    int i = 0;
    for (; i < MAX_TRANSPORTER_NUM; i++) {
        if (strcmp(transport_handle->array[i]->scheme, scheme) == 0) {
            return transport_handle->array[i];
        }
    }
    return NULL;
}

/**
 * @brief Destroys a transport array handle.
 *
 * This function is used to destroy a transport array handle, freeing up any
 * resources associated with it.
 *
 * @param transport_handle The transport array handle to destroy.
 * @return OPERATE_RET Returns OPRT_OK if the transport handle is successfully
 * destroyed, or an error code if an error occurs.
 */
OPERATE_RET
tuya_transport_array_destroy(tuya_transport_array_handle_t transport_handle)
{
    int i = 0;

    for (; i < MAX_TRANSPORTER_NUM; i++) {
        if (transport_handle->array[i]) {
            tuya_transporter_destroy(transport_handle->array[i]);
        }
    }
    Free(transport_handle);
    return OPRT_OK;
}

/**
 * @brief Creates a Tuya transporter.
 *
 * This function creates a Tuya transporter based on the specified transport
 * type and dependency.
 *
 * @param transport_type The transport type of the Tuya transporter.
 * @param dependency The dependency of the Tuya transporter.
 *
 * @return The created Tuya transporter.
 */
tuya_transporter_t tuya_transporter_create(TUYA_TRANSPORT_TYPE_E transport_type, tuya_transporter_t dependency)
{
    if (transport_type == TRANSPORT_TYPE_TCP) {
        return tuya_tcp_transporter_create();
    } else if (transport_type == TRANSPORT_TYPE_TLS) {
        return tuya_tls_transporter_create();
    }
#if defined(ENABLE_WEBSOCKET) && (ENABLE_WEBSOCKET == 1)
    else if (transport_type == TRANSPORT_TYPE_WEBSOCKET) {
        return tuya_websocket_transporter_create();
    }
#endif
    return NULL;
}

/**
 * @brief Destroys a tuya_transporter_t object.
 *
 * This function is used to destroy a tuya_transporter_t object, freeing up any
 * resources associated with it.
 *
 * @param t The tuya_transporter_t object to destroy.
 * @return An OPERATE_RET value indicating the result of the operation.
 *         - OPRT_OK: The tuya_transporter_t object was successfully destroyed.
 *         - Other error codes may be returned to indicate specific failures.
 */
OPERATE_RET tuya_transporter_destroy(tuya_transporter_t t)
{
    if (t && t->f_destroy) {
        t->f_destroy(t);
    }
    return OPRT_OK;
}

/**
 * @brief Reads data from the specified transporter.
 *
 * This function reads data from the specified transporter and stores it in the
 * provided buffer.
 *
 * @param transporter The transporter to read data from.
 * @param buf The buffer to store the read data.
 * @param len The maximum number of bytes to read.
 * @param timeout_ms The timeout value in milliseconds for the read operation.
 *
 * @return The number of bytes read on success, or a negative error code on
 * failure.
 */
OPERATE_RET tuya_transporter_read(tuya_transporter_t transporter, uint8_t *buf, int len, int timeout_ms)
{

    if (transporter && transporter->f_read) {
        return transporter->f_read(transporter, buf, len, timeout_ms);
    }

    return OPRT_INVALID_PARM;
}

/**
 * @brief Writes data to the Tuya transporter.
 *
 * This function writes the specified data to the Tuya transporter.
 *
 * @param t The Tuya transporter to write data to.
 * @param buf Pointer to the buffer containing the data to be written.
 * @param len The length of the data to be written.
 * @param timeout_ms The timeout value in milliseconds for the write operation.
 *
 * @return The result of the write operation. Possible return values are:
 *         - OPRT_OK: Write operation successful.
 *         - OPRT_INVALID_PARAMS: Invalid parameters passed to the function.
 *         - OPRT_TIMEOUT: Write operation timed out.
 *         - OPRT_FAILED: Write operation failed.
 */
OPERATE_RET tuya_transporter_write(tuya_transporter_t t, uint8_t *buf, int len, int timeout_ms)
{
    if (t && t->f_write) {
        return t->f_write(t, buf, len, timeout_ms);
    }
    return OPRT_INVALID_PARM;
}

/**
 * @brief Reads data from the transport layer using polling.
 *
 * This function reads data from the specified transport layer using polling.
 *
 * @param t The transport layer to read data from.
 * @param timeout_ms The timeout value in milliseconds for the read operation.
 *
 * @return The result of the read operation.
 *         Possible return values:
 *         - SUCCESS: The read operation was successful.
 *         - FAILURE: The read operation failed.
 */
OPERATE_RET tuya_transporter_poll_read(tuya_transporter_t t, int timeout_ms)
{
    if (t && t->f_poll_read) {
        return t->f_poll_read(t, timeout_ms);
    }
    return OPRT_INVALID_PARM;
}

/**
 * @brief Writes data to the transport layer in a polling manner.
 *
 * This function is used to write data to the transport layer in a polling
 * manner.
 *
 * @param t The transport object.
 * @param timeout_ms The timeout value in milliseconds.
 * @return The result of the operation.
 */
OPERATE_RET tuya_transporter_poll_write(tuya_transporter_t t, int timeout_ms)
{
    if (t && t->f_poll_write) {
        return t->f_poll_write(t, timeout_ms);
    }
    return OPRT_INVALID_PARM;
}

/**
 * @brief Connects the Tuya transporter to the specified host and port.
 *
 * This function establishes a connection between the Tuya transporter and the
 * specified host and port.
 *
 * @param t The Tuya transporter instance.
 * @param host The host address to connect to.
 * @param port The port number to connect to.
 * @param timeout_ms The connection timeout in milliseconds.
 * @return The result of the operation. Possible values are:
 *         - OPRT_OK: The connection was successfully established.
 *         - OPRT_INVALID_PARM: Invalid parameters were provided.
 *         - OPRT_TIMEOUT: The connection attempt timed out.
 *         - OPRT_SOCKET_ERROR: An error occurred while creating or connecting
 * the socket.
 *         - OPRT_NETWORK_ERROR: A network error occurred.
 *         - OPRT_NO_MEMORY: Insufficient memory to perform the operation.
 *         - OPRT_UNKNOWN_ERROR: An unknown error occurred.
 */
OPERATE_RET tuya_transporter_connect(tuya_transporter_t t, const char *host, int port, int timeout_ms)
{
    if (t && t->f_connect) {
        return t->f_connect(t, host, port, timeout_ms);
    }
    return OPRT_INVALID_PARM;
}

/**
 * @brief Closes the specified Tuya transporter.
 *
 * This function is used to close the Tuya transporter identified by the given
 * `t` parameter.
 *
 * @param t The Tuya transporter to be closed.
 * @return The result of the operation. Possible values are:
 *         - `OPRT_OK` if the transporter was closed successfully.
 *         - An error code if an error occurred during the operation.
 */
OPERATE_RET tuya_transporter_close(tuya_transporter_t t)
{
    if (t && t->f_close) {
        return t->f_close(t);
    }
    return OPRT_INVALID_PARM;
}

/**
 * @brief Controls the Tuya transporter.
 *
 * This function is used to control the Tuya transporter by sending a command
 * and arguments.
 *
 * @param t The Tuya transporter to control.
 * @param cmd The command to send.
 * @param args The arguments for the command.
 *
 * @return The result of the operation.
 */
OPERATE_RET tuya_transporter_ctrl(tuya_transporter_t t, uint32_t cmd, void *args)
{
    if (t && t->f_ctrl) {
        return t->f_ctrl(t, cmd, args);
    }
    return OPRT_INVALID_PARM;
}

/**
 * @brief Sets the function pointers for the transport layer of the Tuya Cloud
 * service.
 *
 * This function sets the function pointers for various transport layer
 * operations such as connecting, closing, reading, writing, polling for read,
 * polling for write, destroying, and controlling the transport layer.
 *
 * @param t The Tuya transporter object.
 * @param connect The function pointer to the connect operation.
 * @param close The function pointer to the close operation.
 * @param read The function pointer to the read operation.
 * @param write The function pointer to the write operation.
 * @param poll_read The function pointer to the poll read operation.
 * @param poll_write The function pointer to the poll write operation.
 * @param destroy The function pointer to the destroy operation.
 * @param ctrl The function pointer to the control operation.
 * @return The operation result status.
 */
OPERATE_RET
tuya_transporter_set_func(tuya_transporter_t t, transporter_connect_fn connect, transporter_close_fn close,
                          transporter_read_fn read, transporter_write_fn write, transporter_poll_read_fn poll_read,
                          transporter_poll_read_fn poll_write, transporter_destroy_fn destroy, transporter_ctrl ctrl)
{

    t->f_connect = connect;
    t->f_close = close;
    t->f_read = read;
    t->f_write = write;
    t->f_poll_read = poll_read;
    t->f_poll_write = poll_write;
    t->f_destroy = destroy;
    t->f_ctrl = ctrl;

    return OPRT_OK;
}
