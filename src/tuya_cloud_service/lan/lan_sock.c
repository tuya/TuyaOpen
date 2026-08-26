/**
 * @file lan_sock.c
 * @brief This file contains the implementation of a generic select()-based
 * socket loop mechanism. It includes functions for creating an independent
 * loop instance, adding and removing socket readers, handling socket
 * events, and destroying the instance.
 * The mechanism is designed to manage multiple socket readers, handle socket
 * events efficiently, and provide a clean shutdown process.
 *
 * The implementation utilizes a select-based approach to monitor and react to
 * socket events across multiple sockets. It supports operations such as adding
 * a new socket reader, updating existing readers, and removing readers. Error
 * handling and socket event detection are integral parts of the loop to ensure
 * robust operation.
 *
 * This module deliberately knows nothing about LAN, or about the AI monitor,
 * or about any other owner. Each owner calls tuya_sock_loop_create() to get
 * its own private instance, sized for its own needs, and is responsible for
 * disabling and waiting it out before freeing whatever its reader callbacks
 * touch. There is no shared state between instances, so there is no
 * reference counting and no lock protecting instance lifetime.
 *
 * Additionally, the file includes utility functions for setting up the
 * environment for socket event handling, including initializing and
 * deinitializing resources, managing the socket readers list, and processing
 * socket events through a loop mechanism.
 *
 * This implementation is part of the Tuya IoT SDK and aims to provide a
 * reliable and efficient way to handle socket-loop communication for IoT
 * devices.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "lan_sock.h"
#include "tal_api.h"
#include "tal_network.h"

#pragma pack(1)

typedef struct lan_sloop_s {
    int max_sock;
    THREAD_HANDLE thread;
    int cnt;
    sloop_sock_t *readers;
    uint32_t reader_num;
    BOOL_T terminate;
    QUEUE_HANDLE queue;
    /*
     * Address of the owner's variable that holds this instance's handle
     * (e.g. &lan->sock_loop). Set once at create() time. When this
     * instance's own thread tears itself down (see tuya_sock_loop_run()),
     * it nulls *self_slot as the very last thing it does, so that a caller
     * polling tuya_sock_loop_is_inited() on that same variable can tell
     * when the thread is truly gone and it is safe to free whatever the
     * reader callbacks were touching.
     */
    lan_sloop_t *self_slot;
} LAN_SLOOP_S;
#pragma pack()

#define LAN_QUEUE_NUM 6

#ifndef STACK_SIZE_LAN
#define STACK_SIZE_LAN (4 * 1024)
#endif

static void __sock_table_set_fds(lan_sloop_t self, TUYA_FD_SET_T *rfds, TUYA_FD_SET_T *efds)
{
    uint32_t idx;
    for (idx = 0; idx < self->reader_num; idx++) {
        if (self->readers[idx].sock >= 0) {
            tal_net_fd_set(self->readers[idx].sock, rfds);
            tal_net_fd_set(self->readers[idx].sock, efds);
        }
    }
}

static void __sock_select_err_handle(lan_sloop_t self)
{
    uint32_t idx;
    for (idx = 0; idx < self->reader_num; idx++) {
        if (self->readers[idx].sock >= 0) {
            if (self->readers[idx].err) {
                self->readers[idx].err(self->readers[idx].sock);
            }
        }
    }
    return;
}

static void __ty_sock_loop_deinit(lan_sloop_t self)
{
    if (NULL == self) {
        return;
    }

    uint32_t idx = 0;
    if (self->readers) {
        for (idx = 0; idx < self->reader_num; idx++) {
            if (self->readers[idx].sock != -1) {
                PR_DEBUG("deinit lan sock %d and close it", self->readers[idx].sock);
                tal_net_close(self->readers[idx].sock);
                self->readers[idx].sock = -1;
                self->readers[idx].pre_select = NULL;
                self->readers[idx].read = NULL;
                self->readers[idx].err = NULL;
                self->readers[idx].quit = NULL;
                self->cnt--;
            }
        }
        tal_free(self->readers);
        self->readers = NULL;
    }
    if (self->queue) {
        tal_queue_free(self->queue);
    }
    if (self->thread) {
        tal_thread_delete(self->thread);
    }

    lan_sloop_t *self_slot = self->self_slot;
    tal_free(self);
    if (self_slot) {
        *self_slot = NULL;
    }
    PR_DEBUG("deinit sock loop success");
    return;
}

static void __ty_add_sock_reader(lan_sloop_t self, sloop_sock_t sock_info)
{
    if (sock_info.sock > self->max_sock) {
        self->max_sock = sock_info.sock;
    }

    uint32_t idx = 0;
    for (idx = 0; idx < self->reader_num; idx++) {
        if ((sock_info.sock == self->readers[idx].sock) && (self->readers[idx].read == sock_info.read)) {
            PR_DEBUG("update lan sock %d,read:%p", sock_info.sock, sock_info.read);
            memset(&self->readers[idx], 0, sizeof(sloop_sock_t));
            memcpy(&self->readers[idx], &sock_info, sizeof(sloop_sock_t));
            break;
        }
    }

    if (idx == self->reader_num) {
        for (idx = 0; idx < self->reader_num; idx++) {
            if (-1 == self->readers[idx].sock) {
                PR_DEBUG("reg lan sock %d,read:%p", sock_info.sock, sock_info.read);
                memset(&self->readers[idx], 0, sizeof(sloop_sock_t));
                memcpy(&self->readers[idx], &sock_info, sizeof(sloop_sock_t));
                self->cnt++;
                break;
            }
        }
    }

    if (idx == self->reader_num) {
        PR_ERR("out of range");
        return;
    }

    return;
}

static void __ty_del_sock_reader(lan_sloop_t self, int sock)
{
    uint32_t idx = 0;
    for (idx = 0; idx < self->reader_num; idx++) {
        if (self->readers[idx].sock == sock) {
            PR_DEBUG("unreg lan sock %d and close it", sock);
            tal_net_close(self->readers[idx].sock);
            self->readers[idx].sock = -1;
            // self->readers[idx].pre_select = NULL;
            self->readers[idx].read = NULL;
            self->readers[idx].err = NULL;
            self->readers[idx].quit = NULL;
            self->cnt--;
            break;
        }
    }

    if (idx == self->reader_num) {
        PR_ERR("unreg not found");
        return;
    }

    return;
}

void tuya_sock_loop_run(void *data)
{
    lan_sloop_t self = (lan_sloop_t)data;
    int actv_cnt = 0;
    uint32_t idx = 0;
    TUYA_FD_SET_T *rfds, *efds;
    sloop_sock_t queue_data = {0};

    rfds = tal_malloc(sizeof(TUYA_FD_SET_T));
    efds = tal_malloc(sizeof(TUYA_FD_SET_T));
    if (rfds == NULL || efds == NULL) {
        PR_ERR("malloc err");
        goto Err;
    }
    memset(rfds, 0, sizeof(TUYA_FD_SET_T));
    memset(efds, 0, sizeof(TUYA_FD_SET_T));

    // while (tuya_get_sock_loop_terminate(self) &&
    // tal_thread_get_state(self->thread) == THREAD_STATE_RUNNING) {
    while (tuya_get_sock_loop_terminate(self)) {
        memset(&queue_data, 0, sizeof(sloop_sock_t));
        if (tal_queue_fetch(self->queue, &queue_data, 0) == 0) {
            if (queue_data.read) {
                __ty_add_sock_reader(self, queue_data);
            } else {
                __ty_del_sock_reader(self, queue_data.sock);
            }
        }
        for (idx = 0; idx < self->reader_num; idx++) {
            if (self->readers[idx].pre_select) {
                self->readers[idx].pre_select();
            }
        }
        if (self->cnt == 0) {
            tal_system_sleep(2000);
            continue;
        }

        tal_net_fd_zero(rfds);
        tal_net_fd_zero(efds);
        __sock_table_set_fds(self, rfds, efds);
        actv_cnt = tal_net_select(self->max_sock + 1, rfds, NULL, efds, 1 * 1000);
        if (actv_cnt < 0) {
            PR_ERR("errno:%d", tal_net_get_errno());
            __sock_select_err_handle(self);
            tal_system_sleep(1000);
            continue;
        } else if (actv_cnt == 0) {
            continue;
        } else {
            for (idx = 0; idx < self->reader_num; idx++) {
                if (self->readers[idx].sock >= 0) {
                    if (0 == tal_net_fd_isset(self->readers[idx].sock, efds)) {
                        continue;
                    }
                    if (self->readers[idx].err) {
                        PR_ERR("socket err:%d, sock:%d, idx:%d", tal_net_get_errno(), self->readers[idx].sock, idx);
                        self->readers[idx].err(self->readers[idx].sock);
                    }
                    actv_cnt--;
                    if (0 == actv_cnt) {
                        break;
                    }
                }
            }
        }

        if (0 == actv_cnt) {
            continue;
        }

        for (idx = 0; idx < self->reader_num; idx++) {
            if (self->readers[idx].sock >= 0) {
                if (tal_net_fd_isset(self->readers[idx].sock, rfds)) {
                    if (self->readers[idx].read) {
                        self->readers[idx].read(self->readers[idx].sock);
                        actv_cnt--;
                        if (0 == actv_cnt) {
                            break;
                        }
                    }
                }
            }
        }
    }

    for (idx = 0; idx < self->reader_num; idx++) {
        if (self->readers[idx].quit) {
            self->readers[idx].quit();
        }
    }

Err:
    if (rfds) {
        tal_free(rfds);
    }
    if (efds) {
        tal_free(efds);
    }

    __ty_sock_loop_deinit(self);

    return;
}

/**
 * @brief Creates and starts an independent socket loop instance.
 *
 * @return The result of the operation.
 *         - OPRT_OK: The socket loop was created successfully.
 *         - Other values: An error occurred during creation.
 */
OPERATE_RET tuya_sock_loop_create(uint32_t reader_num, lan_sloop_t *out)
{
    OPERATE_RET op_ret = OPRT_OK;
    uint32_t idx = 0;

    if (NULL == out || 0 == reader_num) {
        return OPRT_INVALID_PARM;
    }

    if (*out) {
        return OPRT_OK;
    }

    lan_sloop_t self = tal_malloc(sizeof(LAN_SLOOP_S));
    if (NULL == self) {
        return OPRT_MALLOC_FAILED;
    }
    memset(self, 0, sizeof(LAN_SLOOP_S));
    self->terminate = TRUE;
    self->reader_num = reader_num;
    self->self_slot = out;
    *out = self;

    op_ret = tal_queue_create_init(&self->queue, sizeof(sloop_sock_t), LAN_QUEUE_NUM);
    if (OPRT_OK != op_ret) {
        PR_ERR("init queue err");
        goto Err;
    }

    uint32_t readers_len = reader_num * sizeof(sloop_sock_t);
    self->readers = tal_malloc(readers_len);
    if (NULL == self->readers) {
        PR_ERR("tal_malloc err");
        goto Err;
    }
    memset(self->readers, 0, readers_len);
    for (idx = 0; idx < reader_num; idx++) {
        self->readers[idx].sock = -1;
    }
    THREAD_CFG_T thread_cfg = {.priority = THREAD_PRIO_2, .stackDepth = STACK_SIZE_LAN, .thrdname = "sock_loop"};

    op_ret = tal_thread_create_and_start(&self->thread, NULL, NULL, tuya_sock_loop_run, self, &thread_cfg);
    if (OPRT_OK != op_ret) {
        goto Err;
    }

    PR_DEBUG("init sock loop success");
    return OPRT_OK;

Err:
    PR_DEBUG("init error");

    __ty_sock_loop_deinit(self);

    return op_ret;
}

/**
 * @brief Registers a LAN socket.
 *
 * This function is used to register a LAN socket for communication.
 *
 * @param loop the owning loop instance
 * @param sock_info The information of the socket to be registered.
 * @return The result of the operation.
 *         Possible return values:
 *         - OPRT_OK: The socket was successfully registered.
 *         - Other error codes: An error occurred while registering the socket.
 */
OPERATE_RET tuya_reg_lan_sock(lan_sloop_t loop, sloop_sock_t sock_info)
{
    OPERATE_RET op_ret = OPRT_OK;
    if (NULL == loop) {
        PR_ERR("sock loop not ready");
        return OPRT_RESOURCE_NOT_READY;
    }
    op_ret = tal_queue_post(loop->queue, &sock_info, 0);
    if (OPRT_OK != op_ret) {
        PR_ERR("queue post err");
        return op_ret;
    }
    PR_DEBUG("reg post queue %d", sock_info.sock);
    return OPRT_OK;
}

/**
 * @brief Unregisters a LAN socket.
 *
 * This function unregisters a LAN socket identified by the given socket
 * descriptor.
 *
 * @param loop the owning loop instance
 * @param sock The socket descriptor of the LAN socket to unregister.
 * @return The result of the operation. Possible values are:
 *         - OPRT_OK: The LAN socket was successfully unregistered.
 *         - Other error codes indicating the failure reason.
 */
OPERATE_RET tuya_unreg_lan_sock(lan_sloop_t loop, int sock)
{
    OPERATE_RET op_ret = OPRT_OK;
    sloop_sock_t sock_info = {0};
    if (NULL == loop) {
        PR_ERR("sock loop not ready");
        return OPRT_RESOURCE_NOT_READY;
    }
    sock_info.sock = sock;
    op_ret = tal_queue_post(loop->queue, &sock_info, 0);
    if (OPRT_OK != op_ret) {
        PR_ERR("queue post err");
        return op_ret;
    }
    PR_DEBUG("unreg post queue %d", sock);
    return OPRT_OK;
}

/**
 * @brief Disables the given socket loop instance.
 *
 * This function disables the socket loop used for Tuya Cloud service. Once
 * disabled, the socket loop will no longer process incoming socket events.
 */
void tuya_sock_loop_disable(lan_sloop_t loop)
{
    if (NULL == loop) {
        return;
    }

    loop->terminate = FALSE;
}

/**
 * @brief Retrieves the termination status of the given socket loop instance.
 *
 * This function returns the termination status of the socket loop.
 *
 * @return The termination status of the socket loop.
 *         - TRUE: The socket loop has terminated.
 *         - FALSE: The socket loop is still running.
 */
BOOL_T tuya_get_sock_loop_terminate(lan_sloop_t loop)
{
    if (NULL == loop) {
        return FALSE;
    }

    return loop->terminate;
}

BOOL_T tuya_sock_loop_is_inited(lan_sloop_t loop)
{
    return (loop != NULL);
}

/**
 * @brief Function to dump a socket loop instance's reader table.
 *
 * This function is responsible for dumping the LAN socket reader.
 * It does not take any parameters and does not return a value.
 */
void tuya_dump_lan_sock_reader(lan_sloop_t loop)
{
    uint32_t idx = 0;
    if (NULL == loop) {
        return;
    }
    PR_DEBUG("**************lan sock reader info dump begin**************");
    PR_DEBUG("support readers:%d", loop->reader_num);
    PR_DEBUG("sock cnt:%d", loop->cnt);
    PR_DEBUG("terminate:%d", loop->terminate);
    PR_DEBUG("max_sock:%d", loop->max_sock);
    for (idx = 0; idx < loop->reader_num; idx++) {
        if (loop->readers[idx].read) {
            PR_DEBUG("***** sock:%d *****", loop->readers[idx].sock);
            PR_DEBUG("read:%p", loop->readers[idx].read);
            if (loop->readers[idx].err) {
                PR_DEBUG("err:%p", loop->readers[idx].err);
            }
            if (loop->readers[idx].pre_select) {
                PR_DEBUG("pre_select:%p", loop->readers[idx].pre_select);
            }
            if (loop->readers[idx].quit) {
                PR_DEBUG("quit:%p", loop->readers[idx].quit);
            }
        }
    }
    PR_DEBUG("**************lan sock reader info dump end**************");

    return;
}
