/**
 * @file lan_sock.h
 * @brief Header file for LAN socket operations in Tuya IoT SDK.
 *
 * This header file defines the interfaces for LAN socket operations, including
 * socket read, pre-select, and error handling callbacks. It provides a
 * mechanism for integrating socket operations with the Tuya IoT SDK,
 * facilitating network communication for IoT devices within a local area
 * network (LAN). The defined callbacks allow for efficient handling of socket
 * events, error management, and user data association, ensuring robust and
 * responsive network communication.
 *
 * The file is part of the Tuya IoT SDK and is essential for developers
 * implementing custom LAN communication protocols or integrating existing
 * LAN-based services with Tuya's IoT platform.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_LAN_SOCK_H__
#define __TUYA_LAN_SOCK_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief sock read handler
 *
 * @param[in] sock fd
 * @param[in] sock_ctx user data
 *
 */
typedef void (*sloop_sock_read)(int32_t sock);

/**
 * @brief pre select handler
 *
 */
typedef void (*sloop_sock_pre_select)();

/**
 * @brief sock err handler
 *
 * @param[in] sock fd
 * @param[in] sock_ctx user data
 *
 */
typedef void (*sloop_sock_err)(int sock);

/**
 * @brief sock loop thread quit handler
 *
 */
typedef void (*sloop_sock_quit)();

/**
 * @brief reg sock info
 *
 */
typedef struct sloop_sock_t {
    int sock;
    sloop_sock_pre_select pre_select;
    sloop_sock_read read;
    sloop_sock_err err;
    sloop_sock_quit quit;
} sloop_sock_t;

/**
 * @brief register sock
 *
 * @param[in] sock_info reg sock info
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_reg_lan_sock(sloop_sock_t sock_info);

/**
 * @brief unregister sock
 *
 * @param[in] sock fd
 *
 * @note The sock will closed internal, user no need closed manually
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_unreg_lan_sock(int sock);

/**
 * @brief set sock loop disable
 *
 */
void tuya_sock_loop_disable();

/**
 * @brief get sock loop terminate vaule
 *
 * @return terminate value
 *
 */
int tuya_get_sock_loop_terminate();

/**
 * @brief dump lan sock info
 *
 */
void tuya_dump_lan_sock_reader();

/**
 * @brief sock loop init
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_sock_loop_init(void);

#ifdef __cplusplus
}
#endif
#endif //__TUYA_LAN_SOCK_H__
