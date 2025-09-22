/**
 * @file lan_sock.c
 * @brief This file contains the implementation of a LAN socket loop mechanism.
 * It includes functions for initializing the socket loop, adding and removing
 * socket readers, handling socket events, and deinitializing the socket loop.
 * The mechanism is designed to manage multiple socket readers, handle socket
 * events efficiently, and provide a clean shutdown process.
 *
 * The implementation utilizes a select-based approach to monitor and react to
 * socket events across multiple sockets. It supports operations such as adding
 * a new socket reader, updating existing readers, and removing readers. Error
 * handling and socket event detection are integral parts of the loop to ensure
 * robust operation.
 *
 * Additionally, the file includes utility functions for setting up the
 * environment for socket event handling, including initializing and
 * deinitializing resources, managing the socket readers list, and processing
 * socket events through a loop mechanism.
 *
 * This implementation is part of the Tuya IoT SDK and aims to provide a
 * reliable and efficient way to handle LAN socket communication for IoT
 * devices.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "lan_sock.h"
#include "tal_api.h"
#include "tal_network.h"
#include "tuya_lan.h"

#pragma pack(1)

#define LAN_UDP_READER_CNT 5
typedef struct LAN_SLOOP_S {
    int max_sock;
    THREAD_HANDLE thread;
    int cnt;
    sloop_sock_t *readers;
    BOOL_T terminate;
    QUEUE_HANDLE queue;
} LAN_SLOOP_S, *P_LAN_SLOOP_S;
#pragma pack()

static P_LAN_SLOOP_S g_sloop = NULL;
#define LAN_QUEUE_NUM 6

#ifndef STACK_SIZE_LAN
#define STACK_SIZE_LAN (4 * 1024)
#endif

static uint32_t __ty_sock_get_reader_num(void)
{
    return (LAN_UDP_READER_CNT + tuya_lan_get_client_num());
}

static void __sock_table_set_fds(TUYA_FD_SET_T *rfds, TUYA_FD_SET_T *efds)
{
    int idx;
    for (idx = 0; idx < __ty_sock_get_reader_num(); idx++) {
        if (g_sloop->readers[idx].sock >= 0) {
            tal_net_fd_set(g_sloop->readers[idx].sock, rfds);
            tal_net_fd_set(g_sloop->readers[idx].sock, efds);
        }
    }
}

static void __sock_select_err_handle()
{
    int idx;
    for (idx = 0; idx < __ty_sock_get_reader_num(); idx++) {
        if (g_sloop->readers[idx].sock >= 0) {
            if (g_sloop->readers[idx].err) {
                g_sloop->readers[idx].err(g_sloop->readers[idx].sock);
            }
        }
    }
    return;
}

void __ty_sock_loop_deinit(void)
{
    if (NULL == g_sloop) {
        return;
    }

    uint8_t idx = 0;
    if (g_sloop->readers) {
        for (idx = 0; idx < __ty_sock_get_reader_num(); idx++) {
            if (g_sloop->readers[idx].sock != -1) {
                PR_DEBUG("deinit lan sock %d and close it", g_sloop->readers[idx].sock);
                tal_net_close(g_sloop->readers[idx].sock);
                g_sloop->readers[idx].sock = -1;
                g_sloop->readers[idx].pre_select = NULL;
                g_sloop->readers[idx].read = NULL;
                g_sloop->readers[idx].err = NULL;
                g_sloop->readers[idx].quit = NULL;
                g_sloop->cnt--;
            }
        }
        tal_free(g_sloop->readers);
        g_sloop->readers = NULL;
    }
    if (g_sloop->queue) {
        tal_queue_free(g_sloop->queue);
    }
    if (g_sloop->thread) {
        tal_thread_delete(g_sloop->thread);
    }
    tal_free(g_sloop);
    g_sloop = NULL;
    PR_DEBUG("deinit sock loop success");
    return;
}

void __ty_add_sock_reader(sloop_sock_t sock_info)
{
    if (sock_info.sock > g_sloop->max_sock) {
        g_sloop->max_sock = sock_info.sock;
    }

    uint8_t idx = 0;
    for (idx = 0; idx < __ty_sock_get_reader_num(); idx++) {
        if ((sock_info.sock == g_sloop->readers[idx].sock) && (g_sloop->readers[idx].read == sock_info.read)) {
            PR_DEBUG("update lan sock %d,read:%p", sock_info.sock, sock_info.read);
            memset(&g_sloop->readers[idx], 0, sizeof(sloop_sock_t));
            memcpy(&g_sloop->readers[idx], &sock_info, sizeof(sloop_sock_t));
            break;
        }
    }

    if (idx == __ty_sock_get_reader_num()) {
        for (idx = 0; idx < __ty_sock_get_reader_num(); idx++) {
            if (-1 == g_sloop->readers[idx].sock) {
                PR_DEBUG("reg lan sock %d,read:%p", sock_info.sock, sock_info.read);
                memset(&g_sloop->readers[idx], 0, sizeof(sloop_sock_t));
                memcpy(&g_sloop->readers[idx], &sock_info, sizeof(sloop_sock_t));
                g_sloop->cnt++;
                break;
            }
        }
    }

    if (idx == __ty_sock_get_reader_num()) {
        PR_ERR("out of range");
        return;
    }

    return;
}

void __ty_del_sock_reader(int sock)
{
    uint8_t idx = 0;
    for (idx = 0; idx < __ty_sock_get_reader_num(); idx++) {
        if (g_sloop->readers[idx].sock == sock) {
            PR_DEBUG("unreg lan sock %d and close it", sock);
            tal_net_close(g_sloop->readers[idx].sock);
            g_sloop->readers[idx].sock = -1;
            // g_sloop->readers[idx].pre_select = NULL;
            g_sloop->readers[idx].read = NULL;
            g_sloop->readers[idx].err = NULL;
            g_sloop->readers[idx].quit = NULL;
            g_sloop->cnt--;
            break;
        }
    }

    if (idx == __ty_sock_get_reader_num()) {
        PR_ERR("unreg not found");
        return;
    }

    return;
}

void tuya_sock_loop_run(void *data)
{
    int actv_cnt = 0;
    int idx = 0;
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

    // while (tuya_get_sock_loop_terminate() &&
    // tal_thread_get_state(g_sloop->thread) == THREAD_STATE_RUNNING) {
    while (tuya_get_sock_loop_terminate()) {
        memset(&queue_data, 0, sizeof(sloop_sock_t));
        if (tal_queue_fetch(g_sloop->queue, &queue_data, 0) == 0) {
            if (queue_data.read) {
                __ty_add_sock_reader(queue_data);
            } else {
                __ty_del_sock_reader(queue_data.sock);
            }
        }
        for (idx = 0; idx < __ty_sock_get_reader_num(); idx++) {
            if (g_sloop->readers[idx].pre_select) {
                g_sloop->readers[idx].pre_select();
            }
        }
        if (g_sloop->cnt == 0) {
            tal_system_sleep(2000);
            continue;
        }

        tal_net_fd_zero(rfds);
        tal_net_fd_zero(efds);
        __sock_table_set_fds(rfds, efds);
        actv_cnt = tal_net_select(g_sloop->max_sock + 1, rfds, NULL, efds, 1 * 1000);
        if (actv_cnt < 0) {
            PR_ERR("errno:%d", tal_net_get_errno());
            __sock_select_err_handle();
            tal_system_sleep(1000);
            continue;
        } else if (actv_cnt == 0) {
            continue;
        } else {
            for (idx = 0; idx < __ty_sock_get_reader_num(); idx++) {
                if (g_sloop->readers[idx].sock >= 0) {
                    if (0 == tal_net_fd_isset(g_sloop->readers[idx].sock, efds)) {
                        continue;
                    }
                    if (g_sloop->readers[idx].err) {
                        PR_ERR("socket err:%d, sock:%d, idx:%d", tal_net_get_errno(), g_sloop->readers[idx].sock, idx);
                        g_sloop->readers[idx].err(g_sloop->readers[idx].sock);
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

        for (idx = 0; idx < __ty_sock_get_reader_num(); idx++) {
            if (g_sloop->readers[idx].sock >= 0) {
                if (tal_net_fd_isset(g_sloop->readers[idx].sock, rfds)) {
                    if (g_sloop->readers[idx].read) {
                        g_sloop->readers[idx].read(g_sloop->readers[idx].sock);
                        actv_cnt--;
                        if (0 == actv_cnt) {
                            break;
                        }
                    }
                }
            }
        }
    }

    for (idx = 0; idx < __ty_sock_get_reader_num(); idx++) {
        if (g_sloop->readers[idx].quit) {
            g_sloop->readers[idx].quit();
        }
    }

Err:
    if (rfds) {
        tal_free(rfds);
    }
    if (efds) {
        tal_free(efds);
    }

    tuya_lan_exit();
    __ty_sock_loop_deinit();

    return;
}

/**
 * @brief Initializes the socket loop for LAN communication.
 *
 * This function initializes the socket loop for LAN communication in the Tuya
 * Open SDK for devices. It sets up the necessary resources and configurations
 * for the socket loop to handle LAN communication.
 *
 * @return The result of the operation.
 *         - OPRT_OK: The socket loop initialization was successful.
 *         - Other values: An error occurred during the socket loop
 * initialization.
 */
OPERATE_RET tuya_sock_loop_init(void)
{
    OPERATE_RET op_ret = OPRT_OK;
    uint8_t idx = 0;
    if (g_sloop) {
        return OPRT_OK;
    }

    g_sloop = tal_malloc(sizeof(LAN_SLOOP_S));
    if (NULL == g_sloop) {
        return OPRT_MALLOC_FAILED;
    }
    memset(g_sloop, 0, sizeof(LAN_SLOOP_S));
    g_sloop->terminate = TRUE;

    op_ret = tal_queue_create_init(&g_sloop->queue, sizeof(sloop_sock_t), LAN_QUEUE_NUM);
    if (OPRT_OK != op_ret) {
        PR_ERR("init queue err");
        goto Err;
    }

    uint32_t readers_len = __ty_sock_get_reader_num() * sizeof(sloop_sock_t);
    g_sloop->readers = tal_malloc(readers_len);
    if (NULL == g_sloop->readers) {
        PR_ERR("tal_malloc err");
        goto Err;
    }
    memset(g_sloop->readers, 0, readers_len);
    for (idx = 0; idx < __ty_sock_get_reader_num(); idx++) {
        g_sloop->readers[idx].sock = -1;
    }
    THREAD_CFG_T thread_cfg = {.priority = THREAD_PRIO_2, .stackDepth = STACK_SIZE_LAN, .thrdname = "lan_sock_loop"};

    op_ret = tal_thread_create_and_start(&g_sloop->thread, NULL, NULL, tuya_sock_loop_run, NULL, &thread_cfg);
    if (OPRT_OK != op_ret) {
        goto Err;
    }

    PR_DEBUG("init sock loop success");
    return OPRT_OK;

Err:
    PR_DEBUG("init error");

    __ty_sock_loop_deinit();

    return op_ret;
}

/**
 * @brief Registers a LAN socket.
 *
 * This function is used to register a LAN socket for communication.
 *
 * @param sock_info The information of the socket to be registered.
 * @return The result of the operation.
 *         Possible return values:
 *         - OPRT_OK: The socket was successfully registered.
 *         - Other error codes: An error occurred while registering the socket.
 */
OPERATE_RET tuya_reg_lan_sock(sloop_sock_t sock_info)
{
    OPERATE_RET op_ret = OPRT_OK;
    op_ret = tal_queue_post(g_sloop->queue, &sock_info, 0);
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
 * @param sock The socket descriptor of the LAN socket to unregister.
 * @return The result of the operation. Possible values are:
 *         - OPRT_OK: The LAN socket was successfully unregistered.
 *         - Other error codes indicating the failure reason.
 */
OPERATE_RET tuya_unreg_lan_sock(int sock)
{
    OPERATE_RET op_ret = OPRT_OK;
    sloop_sock_t sock_info = {0};
    sock_info.sock = sock;
    op_ret = tal_queue_post(g_sloop->queue, &sock_info, 0);
    if (OPRT_OK != op_ret) {
        PR_ERR("queue post err");
        return op_ret;
    }
    PR_DEBUG("unreg post queue %d", sock);
    return OPRT_OK;
}

/**
 * @brief Disables the socket loop for Tuya Cloud service.
 *
 * This function disables the socket loop used for Tuya Cloud service. Once
 * disabled, the socket loop will no longer process incoming socket events.
 */
void tuya_sock_loop_disable(void)
{
    if (NULL == g_sloop) {
        return;
    }

    g_sloop->terminate = FALSE;
}

/**
 * @brief Retrieves the termination status of the socket loop.
 *
 * This function returns the termination status of the socket loop.
 *
 * @return The termination status of the socket loop.
 *         - TRUE: The socket loop has terminated.
 *         - FALSE: The socket loop is still running.
 */
BOOL_T tuya_get_sock_loop_terminate(void)
{
    if (NULL == g_sloop) {
        return FALSE;
    }

    return g_sloop->terminate;
}

/**
 * @brief Function to dump the LAN socket reader.
 *
 * This function is responsible for dumping the LAN socket reader.
 * It does not take any parameters and does not return a value.
 */
void tuya_dump_lan_sock_reader(void)
{
    uint8_t idx = 0;
    if (NULL == g_sloop) {
        return;
    }
    PR_DEBUG("**************lan sock reader info dump begin**************");
    PR_DEBUG("support readers:%d", __ty_sock_get_reader_num());
    PR_DEBUG("sock cnt:%d", g_sloop->cnt);
    PR_DEBUG("terminate:%d", g_sloop->terminate);
    PR_DEBUG("max_sock:%d", g_sloop->max_sock);
    for (idx = 0; idx < __ty_sock_get_reader_num(); idx++) {
        if (g_sloop->readers[idx].read) {
            PR_DEBUG("***** sock:%d *****", g_sloop->readers[idx].sock);
            PR_DEBUG("read:%p", g_sloop->readers[idx].read);
            if (g_sloop->readers[idx].err) {
                PR_DEBUG("err:%p", g_sloop->readers[idx].err);
            }
            if (g_sloop->readers[idx].pre_select) {
                PR_DEBUG("pre_select:%p", g_sloop->readers[idx].pre_select);
            }
            if (g_sloop->readers[idx].quit) {
                PR_DEBUG("quit:%p", g_sloop->readers[idx].quit);
            }
        }
    }
    PR_DEBUG("**************lan sock reader info dump end**************");

    return;
}
