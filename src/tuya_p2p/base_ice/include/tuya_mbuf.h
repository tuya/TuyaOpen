/**
 * @file tuya_mbuf.h
 * @brief Byte-budget mbuf queue aligned with TuyaOS mid_p2p kcppool
 * @version 1.0
 * @date 2026-08-04
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef __TUYA_MBUF_H__
#define __TUYA_MBUF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Align TuyaOS mid_p2p kcppool slab / budget unit */
#ifndef TUYA_MBUF_HUGE_SIZE
#define TUYA_MBUF_HUGE_SIZE 1600
#endif

typedef struct tuya_mbuf_queue tuya_mbuf_queue_t;
typedef struct tuya_mbuf tuya_mbuf_t;
typedef struct tuya_mem_pool tuya_mem_pool_t;

struct tuya_mbuf {
    void *node[2];
    tuya_mbuf_queue_t *mbuf_queue;
    char *base;
    char *data;
    int len;
    int total_len;
    int flag;
    int ref_cnt;
};

/**
 * @brief Create send/recv mbuf queue with byte capacity
 * @param[in] total_size capacity in bytes
 * @param[in] pool reserved (may be NULL; OS passes mem pool)
 * @return queue handle or NULL
 */
tuya_mbuf_queue_t *tuya_mbuf_queue_create(int total_size, tuya_mem_pool_t *pool);

/**
 * @brief Destroy queue and free remaining mbufs
 * @param[in] q queue
 * @return none
 */
void tuya_mbuf_queue_destroy(tuya_mbuf_queue_t *q);

/**
 * @brief Mark queue closed
 * @param[in] q queue
 * @return none
 */
void tuya_mbuf_queue_close(tuya_mbuf_queue_t *q);

/**
 * @brief Get queue status (0=ok, non-zero=closed/fault)
 * @param[in] q queue
 * @return 0 if usable
 */
int tuya_mbuf_queue_get_status(tuya_mbuf_queue_t *q);

/**
 * @brief Get free byte budget
 * @param[in] q queue
 * @return free bytes
 */
int tuya_mbuf_queue_get_free_size(tuya_mbuf_queue_t *q);

/**
 * @brief Get used byte budget
 * @param[in] q queue
 * @return used bytes
 */
int tuya_mbuf_queue_get_used_size(tuya_mbuf_queue_t *q);

/**
 * @brief Allocate mbuf charged against queue budget
 * @param[in] q queue
 * @param[in] size buffer bytes to reserve
 * @return mbuf or NULL if budget insufficient / OOM
 */
tuya_mbuf_t *tuya_mbuf_alloc(tuya_mbuf_queue_t *q, int size);

/**
 * @brief Release mbuf (refcount); returns budget when last ref drops
 * @param[in] m mbuf
 * @return none
 */
void tuya_mbuf_free(tuya_mbuf_t *m);

/**
 * @brief Add reference for multi-segment KCP ownership
 * @param[in] m mbuf
 * @return none
 */
void tuya_mbuf_addref(tuya_mbuf_t *m);

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_MBUF_H__ */
