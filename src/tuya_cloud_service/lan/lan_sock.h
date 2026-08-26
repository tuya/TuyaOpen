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
 * Despite the name, this module knows nothing about LAN specifically: it is a
 * generic select()-based socket reader loop. Each owner (the LAN service, the
 * AI monitor service, ...) creates and holds its own independent loop
 * instance via tuya_sock_loop_create() and is responsible for destroying it.
 * Instances never share state, so there is no reference counting and no lock
 * around instance lifetime.
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
 * @brief opaque handle to one independent socket loop instance.
 *
 * Each owner holds its own handle. The struct definition is private to
 * lan_sock.c; callers only ever see a pointer.
 */
typedef struct lan_sloop_s *lan_sloop_t;

/**
 * @brief create and start an independent socket loop instance.
 *
 * @param[in] reader_num max number of sockets this instance can hold
 * registered at once. Sized by the caller for its own needs (e.g. LAN sizes
 * it off its own client limit; the AI monitor sizes it off its own
 * max_clients).
 * @param[out] out receives the new instance handle on success. Left
 * pointing at NULL on failure. Also remembered internally so that when this
 * instance's thread later terminates (tuya_sock_loop_disable() followed by
 * the thread noticing it), it can null the very same slot on its own -- see
 * tuya_sock_loop_is_inited().
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_sock_loop_create(uint32_t reader_num, lan_sloop_t *out);

/**
 * @brief register sock
 *
 * @param[in] loop the owning loop instance
 * @param[in] sock_info reg sock info
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_reg_lan_sock(lan_sloop_t loop, sloop_sock_t sock_info);

/**
 * @brief unregister sock
 *
 * @param[in] loop the owning loop instance
 * @param[in] sock fd
 *
 * @note The sock will closed internal, user no need closed manually
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
OPERATE_RET tuya_unreg_lan_sock(lan_sloop_t loop, int sock);

/**
 * @brief ask this loop instance's thread to terminate.
 *
 * Termination happens asynchronously: the thread notices on its next pass
 * (bounded by its select()/sleep timeouts), runs each remaining reader's
 * quit callback, frees the instance, and nulls the owner slot that was
 * passed to tuya_sock_loop_create() -- poll tuya_sock_loop_is_inited() on
 * that same slot to find out when it is safe to free whatever the readers'
 * callbacks were touching.
 *
 * @param[in] loop the owning loop instance
 */
void tuya_sock_loop_disable(lan_sloop_t loop);

/**
 * @brief get sock loop terminate vaule
 *
 * @param[in] loop the owning loop instance
 *
 * @return terminate value
 *
 */
BOOL_T tuya_get_sock_loop_terminate(lan_sloop_t loop);

/**
 * @brief dump lan sock info
 *
 * @param[in] loop the owning loop instance
 *
 */
void tuya_dump_lan_sock_reader(lan_sloop_t loop);

/**
 * @brief check whether a loop instance is still alive.
 *
 * @param[in] loop the owning loop instance (safe to pass NULL)
 *
 * @return TRUE if loop is a live instance, otherwise FALSE
 */
BOOL_T tuya_sock_loop_is_inited(lan_sloop_t loop);

#ifdef __cplusplus
}
#endif
#endif //__TUYA_LAN_SOCK_H__
