/**
 * @file tuya_mbuf.c
 * @brief mbuf queue aligned with TuyaOS mid_p2p kcppool semantics
 * @version 1.0
 * @date 2026-08-04
 * @copyright Copyright (c) Tuya Inc.
 *
 * OS behavior (from libtuyaos.a / tuya_p2p_lite_kcppool):
 * - Buffer memory comes from tuya_p2p_lib_malloc → tkl_system_psram_malloc
 * - Each alloc charges a fixed TUYA_MBUF_HUGE_SIZE (1600) against the queue budget
 * - Payload lives in a 1600-byte slab (mem_cache); OpenSDK has no slab source, so
 *   allocate a 1600-byte PSRAM block per mbuf instead of per-request SRAM sizes
 */
#include "tuya_mbuf.h"
#include "tal_mutex.h"
#include "tal_memory.h"
#include "tuya_cloud_types.h"
#include "tal_log.h"
#include <stdint.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#ifndef TUYA_MBUF_HUGE_SIZE
#define TUYA_MBUF_HUGE_SIZE 1600
#endif

/* Align OS tuya_p2p_lib_malloc / free (always PSRAM when EXT_RAM) */
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#define TUYA_MBUF_MALLOC(s) tal_psram_malloc(s)
#define TUYA_MBUF_CALLOC(n, s) tal_psram_calloc(n, s)
#define TUYA_MBUF_FREE(p) tal_psram_free(p)
#else
#define TUYA_MBUF_MALLOC(s) tal_malloc(s)
#define TUYA_MBUF_CALLOC(n, s) tal_calloc(n, s)
#define TUYA_MBUF_FREE(p) tal_free(p)
#endif

/* ---------------------------------------------------------------------------
 * Type definitions
 * --------------------------------------------------------------------------- */
struct tuya_mbuf_queue {
    void *head[2];
    MUTEX_HANDLE lock;
    int used_size;
    int total_size;
    tuya_mem_pool_t *pool;
    int close_flag;
    int64_t total_bytes_in;
    int64_t total_bytes_out;
};

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Create send/recv mbuf queue with byte capacity
 * @param[in] total_size capacity in bytes
 * @param[in] pool reserved (may be NULL)
 * @return queue handle or NULL
 */
tuya_mbuf_queue_t *tuya_mbuf_queue_create(int total_size, tuya_mem_pool_t *pool)
{
    tuya_mbuf_queue_t *q;

    if (total_size <= 0) {
        return NULL;
    }
    q = (tuya_mbuf_queue_t *)TUYA_MBUF_CALLOC(1, sizeof(*q));
    if (q == NULL) {
        return NULL;
    }
    q->total_size = total_size;
    q->pool = pool;
    q->head[0] = q->head;
    q->head[1] = q->head;
    if (tal_mutex_create_init(&q->lock) != OPRT_OK) {
        TUYA_MBUF_FREE(q);
        return NULL;
    }
    return q;
}

/**
 * @brief Destroy queue and free remaining mbufs
 * @param[in] q queue
 * @return none
 */
void tuya_mbuf_queue_destroy(tuya_mbuf_queue_t *q)
{
    if (q == NULL) {
        return;
    }

    if (q->used_size != 0) {
        PR_ERR("mbuf queue destroyed with %d bytes outstanding (in=%lld out=%lld)", q->used_size,
               (long long)q->total_bytes_in, (long long)q->total_bytes_out);
    }
    q->close_flag = 1;
    tal_mutex_release(q->lock);
    TUYA_MBUF_FREE(q);
}

/**
 * @brief Mark queue closed
 * @param[in] q queue
 * @return none
 */
void tuya_mbuf_queue_close(tuya_mbuf_queue_t *q)
{
    if (q == NULL) {
        return;
    }
    tal_mutex_lock(q->lock);
    q->close_flag = 1;
    tal_mutex_unlock(q->lock);
}

/**
 * @brief Get queue status (0=ok, non-zero=closed/fault)
 * @param[in] q queue
 * @return 0 if usable
 */
int tuya_mbuf_queue_get_status(tuya_mbuf_queue_t *q)
{
    int st;

    if (q == NULL) {
        return -1;
    }
    tal_mutex_lock(q->lock);
    st = q->close_flag;
    tal_mutex_unlock(q->lock);
    return st;
}

/**
 * @brief Get free byte budget
 * @param[in] q queue
 * @return free bytes
 */
int tuya_mbuf_queue_get_free_size(tuya_mbuf_queue_t *q)
{
    int free_sz;

    if (q == NULL) {
        return 0;
    }
    tal_mutex_lock(q->lock);
    free_sz = q->total_size - q->used_size;
    if (free_sz < 0) {
        free_sz = 0;
    }
    tal_mutex_unlock(q->lock);
    return free_sz;
}

/**
 * @brief Get used byte budget
 * @param[in] q queue
 * @return used bytes
 */
int tuya_mbuf_queue_get_used_size(tuya_mbuf_queue_t *q)
{
    int used;

    if (q == NULL) {
        return 0;
    }
    tal_mutex_lock(q->lock);
    used = q->used_size;
    tal_mutex_unlock(q->lock);
    return used;
}

/**
 * @brief Allocate mbuf charged against queue budget
 * @param[in] q queue
 * @param[in] size requested payload bytes (must fit in HUGE slab)
 * @return mbuf or NULL if budget insufficient / OOM / size too large
 * @note OS charges and allocates TUYA_MBUF_HUGE_SIZE (1600) per call via mem_cache
 */
tuya_mbuf_t *tuya_mbuf_alloc(tuya_mbuf_queue_t *q, int size)
{
    tuya_mbuf_t *m;
    char *buf;
    const int charge = TUYA_MBUF_HUGE_SIZE;

    if (q == NULL || size <= 0 || size > TUYA_MBUF_HUGE_SIZE) {
        return NULL;
    }
    tal_mutex_lock(q->lock);
    if (q->close_flag || (q->used_size + charge) > q->total_size) {
        tal_mutex_unlock(q->lock);
        return NULL;
    }
    q->used_size += charge;
    q->total_bytes_in += charge;
    tal_mutex_unlock(q->lock);

    m = (tuya_mbuf_t *)TUYA_MBUF_CALLOC(1, sizeof(*m));
    buf = (char *)TUYA_MBUF_MALLOC((size_t)TUYA_MBUF_HUGE_SIZE);
    if (m == NULL || buf == NULL) {
        if (m != NULL) {
            TUYA_MBUF_FREE(m);
        }
        if (buf != NULL) {
            TUYA_MBUF_FREE(buf);
        }
        tal_mutex_lock(q->lock);
        q->used_size -= charge;
        tal_mutex_unlock(q->lock);
        return NULL;
    }
    m->mbuf_queue = q;
    m->base = buf;
    m->data = buf;
    m->len = 0;
    m->total_len = TUYA_MBUF_HUGE_SIZE;
    m->ref_cnt = 1;
    return m;
}

/**
 * @brief Add reference for multi-segment KCP ownership
 * @param[in] m mbuf
 * @return none
 */
void tuya_mbuf_addref(tuya_mbuf_t *m)
{
    if (m == NULL) {
        return;
    }
    m->ref_cnt++;
}

/**
 * @brief Release mbuf (refcount); returns budget when last ref drops
 * @param[in] m mbuf
 * @return none
 */
void tuya_mbuf_free(tuya_mbuf_t *m)
{
    tuya_mbuf_queue_t *q;
    int charge;

    if (m == NULL) {
        return;
    }
    m->ref_cnt--;
    if (m->ref_cnt > 0) {
        return;
    }
    q = m->mbuf_queue;
    charge = TUYA_MBUF_HUGE_SIZE;
    TUYA_MBUF_FREE(m->base);
    TUYA_MBUF_FREE(m);
    if (q != NULL) {
        tal_mutex_lock(q->lock);
        q->used_size -= charge;
        if (q->used_size < 0) {
            q->used_size = 0;
        }
        q->total_bytes_out += charge;
        tal_mutex_unlock(q->lock);
    }
}
