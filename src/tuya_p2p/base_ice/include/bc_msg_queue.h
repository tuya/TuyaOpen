/**
 * @file bc_msg_queue.h
 * @brief Byte-oriented signaling message queue (align TuyaOS mid_p2p bc_msg_queue)
 * @version 1.0
 * @date 2026-08-04
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef __BC_MSG_QUEUE_H__
#define __BC_MSG_QUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#ifndef BC_MSG_SIZE_MAX
#define BC_MSG_SIZE_MAX (200 * 1024)
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
typedef struct bc_msg_queue bc_msg_queue_t;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Create an empty message queue
 * @return queue handle or NULL
 */
bc_msg_queue_t *bc_msg_queue_create(void);

/**
 * @brief Mark queue closed and wake blocked pop
 * @param[in] q queue
 * @return none
 */
void bc_msg_queue_close(bc_msg_queue_t *q);

/**
 * @brief Destroy queue and free remaining nodes
 * @param[in] q queue
 * @return none
 */
void bc_msg_queue_destroy(bc_msg_queue_t *q);

/**
 * @brief Get queued payload bytes
 * @param[in] q queue
 * @return used bytes, or 0 if q is NULL
 */
int bc_msg_queue_get_length(bc_msg_queue_t *q);

/**
 * @brief Push a message copy to queue tail
 * @param[in] q queue
 * @param[in] type message type (0=signaling incoming in OS)
 * @param[in] data payload
 * @param[in] len payload length in bytes
 * @return 0 on success, -1 on failure
 */
int bc_msg_queue_push_back(bc_msg_queue_t *q, int type, const void *data, int len);

/**
 * @brief Pop front message (blocking until data or closed)
 * @param[in] q queue
 * @param[out] type optional message type
 * @param[out] buf output buffer
 * @param[in,out] len in: buf capacity; out: copied length
 * @return copied length on success, -1 if closed/error
 */
int bc_msg_queue_pop_front(bc_msg_queue_t *q, int *type, void *buf, int *len);

#ifdef __cplusplus
}
#endif

#endif /* __BC_MSG_QUEUE_H__ */
