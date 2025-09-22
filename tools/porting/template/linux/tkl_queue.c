/**
 * @file tkl_queue.c
 * @brief the default weak implements of tuya os queue, this implement only used when OS=linux
 * @version 0.1
 * @date 2019-08-15
 *
 * @copyright Copyright 2020-2021 Tuya Inc. All Rights Reserved.
 *
 */

// --- BEGIN: user defines and implements ---
#include "tkl_queue.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct rpa_queue_t {
    void **data;
    volatile uint32_t nelts; /**< # elements */
    uint32_t in;             /**< next empty location */
    uint32_t out;            /**< next filled location */
    uint32_t bounds;         /**< max size of queue */
    uint32_t full_waiters;
    uint32_t empty_waiters;
    pthread_mutex_t *one_big_mutex;
    pthread_cond_t *not_empty;
    pthread_cond_t *not_full;
    int terminated;
} rpa_queue_t;

#define RPA_WAIT_NONE    0
#define RPA_WAIT_FOREVER -1

#define rpa_queue_full(queue)  ((queue)->nelts == (queue)->bounds)
#define rpa_queue_empty(queue) ((queue)->nelts == 0)

static void set_timeout(struct timespec *abstime, int wait_ms)
{
    clock_gettime(CLOCK_REALTIME, abstime);
    /* add seconds */
    abstime->tv_sec += (wait_ms / 1000);
    /* add and carry microseconds */
    long ms = abstime->tv_nsec / 1000000L;
    ms += wait_ms % 1000;
    while (ms > 1000) {
        ms -= 1000;
        abstime->tv_sec += 1;
    }
    abstime->tv_nsec = ms * 1000000L;
}

static void rpa_queue_destroy(rpa_queue_t *queue)
{
    /* Ignore errors here, we can't do anything about them anyway. */
    pthread_cond_destroy(queue->not_empty);
    pthread_cond_destroy(queue->not_full);
    pthread_mutex_destroy(queue->one_big_mutex);
}

static BOOL_T rpa_queue_create(rpa_queue_t **q, uint32_t queue_capacity)
{
    rpa_queue_t *queue;
    queue = malloc(sizeof(rpa_queue_t));
    if (!queue) {
        return false;
    }
    *q = queue;
    memset(queue, 0, sizeof(rpa_queue_t));

    if (!(queue->one_big_mutex = malloc(sizeof(pthread_mutex_t))))
        return false;
    if (!(queue->not_empty = malloc(sizeof(pthread_cond_t))))
        return false;
    if (!(queue->not_full = malloc(sizeof(pthread_cond_t))))
        return false;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    int rv = pthread_mutex_init(queue->one_big_mutex, &attr);
    if (rv != 0) {
        goto error;
    }

    rv = pthread_cond_init(queue->not_empty, NULL);
    if (rv != 0) {
        goto error;
    }

    rv = pthread_cond_init(queue->not_full, NULL);
    if (rv != 0) {
        goto error;
    }

    /* Set all the data in the queue to NULL */
    queue->data = malloc(queue_capacity * sizeof(void *));
    queue->bounds = queue_capacity;
    queue->nelts = 0;
    queue->in = 0;
    queue->out = 0;
    queue->terminated = 0;
    queue->full_waiters = 0;
    queue->empty_waiters = 0;

    return true;

error:
    free(queue);
    return false;
}

static BOOL_T rpa_queue_trypush(rpa_queue_t *queue, void *data)
{
    BOOL_T rv;

    if (queue->terminated) {
        return false; /* no more elements ever again */
    }

    rv = pthread_mutex_lock(queue->one_big_mutex);
    if (rv != 0) {
        return false;
    }

    if (rpa_queue_full(queue)) {
        rv = pthread_mutex_unlock(queue->one_big_mutex);
        return false; // EAGAIN;
    }

    queue->data[queue->in] = data;
    queue->in++;
    if (queue->in >= queue->bounds) {
        queue->in -= queue->bounds;
    }
    queue->nelts++;

    if (queue->empty_waiters) {
        rv = pthread_cond_signal(queue->not_empty);
        if (rv != 0) {
            pthread_mutex_unlock(queue->one_big_mutex);
            return false;
        }
    }

    pthread_mutex_unlock(queue->one_big_mutex);
    return true;
}

static BOOL_T rpa_queue_timedpush(rpa_queue_t *queue, void *data, int wait_ms)
{
    BOOL_T rv;

    if (wait_ms == RPA_WAIT_NONE)
        return rpa_queue_trypush(queue, data);

    if (queue->terminated) {
        return false; /* no more elements ever again */
    }

    rv = pthread_mutex_lock(queue->one_big_mutex);
    if (rv != 0) {
        return false;
    }

    if (rpa_queue_full(queue)) {
        if (!queue->terminated) {
            queue->full_waiters++;
            if (wait_ms == RPA_WAIT_FOREVER) {
                rv = pthread_cond_wait(queue->not_full, queue->one_big_mutex);
            } else {
                struct timespec abstime;
                set_timeout(&abstime, wait_ms);
                rv = pthread_cond_timedwait(queue->not_full, queue->one_big_mutex, &abstime);
            }
            queue->full_waiters--;
            if (rv != 0) {
                pthread_mutex_unlock(queue->one_big_mutex);
                return false;
            }
        }
        /* If we wake up and it's still empty, then we were interrupted */
        if (rpa_queue_full(queue)) {
            rv = pthread_mutex_unlock(queue->one_big_mutex);
            if (rv != 0) {
                return false;
            }
            if (queue->terminated) {
                return false; /* no more elements ever again */
            } else {
                return false; // EINTR;
            }
        }
    }

    queue->data[queue->in] = data;
    queue->in++;
    if (queue->in >= queue->bounds) {
        queue->in -= queue->bounds;
    }
    queue->nelts++;

    if (queue->empty_waiters) {
        rv = pthread_cond_signal(queue->not_empty);
        if (rv != 0) {
            pthread_mutex_unlock(queue->one_big_mutex);
            return false;
        }
    }

    pthread_mutex_unlock(queue->one_big_mutex);
    return true;
}

static BOOL_T rpa_queue_trypop(rpa_queue_t *queue, void **data)
{
    BOOL_T rv;

    if (queue->terminated) {
        return false; /* no more elements ever again */
    }

    rv = pthread_mutex_lock(queue->one_big_mutex);
    if (rv != 0) {
        return false;
    }

    if (rpa_queue_empty(queue)) {
        rv = pthread_mutex_unlock(queue->one_big_mutex);
        return false; // EAGAIN;
    }

    *data = queue->data[queue->out];
    queue->nelts--;

    queue->out++;
    if (queue->out >= queue->bounds) {
        queue->out -= queue->bounds;
    }
    if (queue->full_waiters) {
        rv = pthread_cond_signal(queue->not_full);
        if (rv != 0) {
            pthread_mutex_unlock(queue->one_big_mutex);
            return false;
        }
    }

    pthread_mutex_unlock(queue->one_big_mutex);
    return true;
}

static BOOL_T rpa_queue_timedpop(rpa_queue_t *queue, void **data, int wait_ms)
{
    BOOL_T rv;

    if (wait_ms == RPA_WAIT_NONE)
        return rpa_queue_trypop(queue, data);

    if (queue->terminated) {
        return false; /* no more elements ever again */
    }

    rv = pthread_mutex_lock(queue->one_big_mutex);
    if (rv != 0) {
        return false;
    }

    /* Keep waiting until we wake up and find that the queue is not empty. */
    if (rpa_queue_empty(queue)) {
        if (!queue->terminated) {
            queue->empty_waiters++;
            if (wait_ms == RPA_WAIT_FOREVER) {
                rv = pthread_cond_wait(queue->not_empty, queue->one_big_mutex);
            } else {
                struct timespec abstime;
                set_timeout(&abstime, wait_ms);
                rv = pthread_cond_timedwait(queue->not_empty, queue->one_big_mutex, &abstime);
            }
            queue->empty_waiters--;
            if (rv != 0) {
                pthread_mutex_unlock(queue->one_big_mutex);
                return false;
            }
        }
        /* If we wake up and it's still empty, then we were interrupted */
        if (rpa_queue_empty(queue)) {
            rv = pthread_mutex_unlock(queue->one_big_mutex);
            if (rv != 0) {
                return false;
            }
            if (queue->terminated) {
                return false; /* no more elements ever again */
            } else {
                return false; // EINTR;
            }
        }
    }

    *data = queue->data[queue->out];
    queue->nelts--;

    queue->out++;
    if (queue->out >= queue->bounds) {
        queue->out -= queue->bounds;
    }
    if (queue->full_waiters) {
        rv = pthread_cond_signal(queue->not_full);
        if (rv != 0) {
            pthread_mutex_unlock(queue->one_big_mutex);
            return false;
        }
    }

    pthread_mutex_unlock(queue->one_big_mutex);
    return true;
}

typedef struct {
    rpa_queue_t *queue;
    int msgsize;
} TKL_QUEUE_T;
// --- END: user defines and implements ---

/**
 * @brief Create message queue
 *
 * @param[in] msgsize message size
 * @param[in] msgcount message number
 * @param[out] queue the queue handle created
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_queue_create_init(TKL_QUEUE_HANDLE *queue, int msgsize, int msgcount)
{
    // --- BEGIN: user implements ---
    TKL_QUEUE_T *tkl_queue = NULL;

    if ((NULL == queue) || (0 == msgsize) || (0 == msgcount)) {
        return OPRT_INVALID_PARM;
    }

    tkl_queue = (TKL_QUEUE_T *)malloc(sizeof(TKL_QUEUE_T));
    if (!tkl_queue) {
        return OPRT_MALLOC_FAILED;
    }

    if (!rpa_queue_create(&tkl_queue->queue, msgcount)) {
        return OPRT_OS_ADAPTER_QUEUE_CREAT_FAILED;
    }
    tkl_queue->msgsize = msgsize;

    *queue = (TKL_QUEUE_HANDLE)tkl_queue;

    return OPRT_OK;
    // --- END: user implements ---
}

/**
 * @brief post a message to the message queue
 *
 * @param[in] queue the handle of the queue
 * @param[in] data the data of the message
 * @param[in] timeout timeout time
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_queue_post(const TKL_QUEUE_HANDLE queue, void *data, uint32_t timeout)
{
    // --- BEGIN: user implements ---
    if (NULL == queue || NULL == data) {
        return OPRT_INVALID_PARM;
    }

    TKL_QUEUE_T *tkl_queue = (TKL_QUEUE_T *)queue;
    int wait_ms = 0;

    void *buf = (void *)malloc(tkl_queue->msgsize);
    if (NULL == buf) {
        return OPRT_MALLOC_FAILED;
    }

    memcpy(buf, (void *)data, tkl_queue->msgsize);

    if (timeout == TKL_QUEUE_WAIT_FROEVER) {
        wait_ms = RPA_WAIT_FOREVER;
    } else {
        wait_ms = timeout;
    }

    if (!rpa_queue_timedpush(tkl_queue->queue, buf, wait_ms)) {
        return OPRT_OS_ADAPTER_QUEUE_SEND_FAIL;
    }

    return OPRT_OK;
    // --- END: user implements ---
}

/**
 * @brief fetch message from the message queue
 *
 * @param[in] queue the message queue handle
 * @param[inout] msg the message fetch form the message queue
 * @param[in] timeout timeout time
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tkl_queue_fetch(const TKL_QUEUE_HANDLE queue, void *msg, uint32_t timeout)
{
    // --- BEGIN: user implements ---
    if (NULL == queue) {
        return OPRT_INVALID_PARM;
    }

    TKL_QUEUE_T *tkl_queue = (TKL_QUEUE_T *)queue;
    void *buf = NULL;
    int wait_ms;

    if (timeout == TKL_QUEUE_WAIT_FROEVER) {
        wait_ms = RPA_WAIT_FOREVER;
    } else {
        wait_ms = timeout;
    }

    if (!rpa_queue_timedpop(tkl_queue->queue, (void **)&buf, wait_ms)) {
        return OPRT_OS_ADAPTER_QUEUE_RECV_FAIL;
    }

    if (buf) {
        memcpy((void *)msg, buf, tkl_queue->msgsize);
        free(buf);
    }

    return OPRT_OK;
    // --- END: user implements ---
}

/**
 * @brief free the message queue
 *
 * @param[in] queue the message queue handle
 *
 * @return void
 */
void tkl_queue_free(const TKL_QUEUE_HANDLE queue)
{
    // --- BEGIN: user implements ---
    if (NULL == queue) {
        return;
    }

    TKL_QUEUE_T *tkl_queue = (TKL_QUEUE_T *)queue;
    rpa_queue_destroy(tkl_queue->queue);
    free(tkl_queue);

    return;
    // --- END: user implements ---
}
