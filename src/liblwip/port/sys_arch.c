/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * Modifications Copyright (c) 2006 Christian Walter <wolti@sil.at>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 * Modifcations: Christian Walter <wolti@sil.at>
 *
 * $Id: sys_arch.c,v 1.6 2006/09/24 22:04:53 wolti Exp $
 */
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "lwip/arch/sys_arch.h"
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/sio.h"
#include "lwip/stats.h"
#include "lwip/timeouts.h"

#include "tkl_output.h"

/* ------------------------ Defines --------------------------------------- */
#define MAX_FREE_POLL_CNT     50
#define RETRY_FREE_POLL_DELAY 10
#define TY_LWIP_WAIT_FOREVER  0xFFFFFFFF /* For 32bit OS */
#define TY_SYS_ARCH_DBG_EN    0
/* --------------------------- Variables ---------------------------------- */
static sys_mutex_t g_lwip_mutex = NULL;
// static pthread_key_t sys_thread_sem_key;

/* ------------------------ External functions ------------------------------ */
// static void sys_thread_sem_free(void *data);

/* ------------------------ Start implementation -------------------------- */
#if TY_SYS_ARCH_DBG_EN
static void SYS_ARCH_DBG(const char *fmt, ...)
{
    va_list ap;
    char *buf;
    int buflen;
    int len;

    va_start(ap, fmt);
    buflen = vsnprintf(NULL, 0, fmt, ap) + 1;
    va_end(ap);

    buf = tal_malloc(buflen);
    if (buf == NULL) {
        return;
    }

    va_start(ap, fmt);
    len = vsnprintf(buf, buflen, fmt, ap);
    va_end(ap);

    tkl_log_output(buf);
    tal_free(buf);
}
#else
#define SYS_ARCH_DBG(fmt, args...)
#endif /* TY_SYS_ARCH_DBG_EN */

void sys_init(void)
{
    if (tal_mutex_create_init(&g_lwip_mutex) != ERR_OK) {
        SYS_ARCH_DBG("%s: call tal_mutex_create_init failed\n", __func__);
    }

#if LWIP_NETCONN_SEM_PER_THREAD
    // Create the pthreads key for the per-thread semaphore storage
    // pthread_key_create(&sys_thread_sem_key, sys_thread_sem_free);
#endif
}

/*
 * This optional function does a "fast" critical region protection and returns
 * the previous protection level. This function is only called during very short
 * critical regions. An embedded system which supports ISR-based drivers might
 * want to implement this function by disabling interrupts. Task-based systems
 * might want to implement this by using a mutex or disabling tasking. This
 * function should support recursive calls from the same task or interrupt. In
 * other words, sys_arch_protect() could be called while already protected. In
 * that case the return value indicates that it is already protected.
 *
 * sys_arch_protect() is only required if your port is supporting an operating
 * system.
 */
sys_prot_t sys_arch_protect(void)
{
    if (tal_mutex_lock(g_lwip_mutex) != ERR_OK) {
        SYS_ARCH_DBG("%s: call tal_mutex_lock failed\n", __func__);
        return ERR_MEM;
    }

    return ERR_OK;
}

/*
 * This optional function does a "fast" set of critical region protection to the
 * value specified by pval. See the documentation for sys_arch_protect() for
 * more information. This function is only required if your port is supporting
 * an operating system.
 */
void sys_arch_unprotect(sys_prot_t pval)
{
    if (tal_mutex_unlock(g_lwip_mutex) != ERR_OK) {
        SYS_ARCH_DBG("%s: call tal_mutex_unlock failed\n", __func__);
    }
}

/* ------------------------ Start implementation ( Threads ) -------------- */

/*
 * Starts a new thread named "name" with priority "prio" that will begin its
 * execution in the function "thread()". The "arg" argument will be passed as an
 * argument to the thread() function. The stack size to used for this thread is
 * the "stacksize" parameter. The id of the new thread is returned. Both the id
 * and the priority are system dependent.
 */
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
    THREAD_HANDLE CreatedTask = NULL;
    long result;
    THREAD_CFG_T thread_cfg;

    thread_cfg.priority = prio;
    thread_cfg.stackDepth = stacksize;
    thread_cfg.thrdname = (char *)name;

    result = tal_thread_create_and_start(&CreatedTask, NULL, NULL, thread, arg, &thread_cfg);
    if (ERR_OK != result) {
        SYS_ARCH_DBG("%s: create thread %s failed(result=%d)\n", __func__, name, result);
    }

    return CreatedTask;
}

/* ------------------------ Start implementation ( Semaphores ) ----------- */

/* Creates and returns a new semaphore. The "count" argument specifies
 * the initial state of the semaphore.
 */
err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    if (tal_semaphore_create_init(sem, count, 1) != ERR_OK) {
        SYS_ARCH_DBG("%s: call tal_semaphore_create_init failed\n", __func__);
        return ERR_MEM;
    }

    return ERR_OK;
}

/* Deallocates a semaphore */
void sys_sem_free(sys_sem_t *sem)
{
    if (tal_semaphore_release(*sem) != ERR_OK) {
        SYS_ARCH_DBG("%s: call tal_semaphore_release failed\n", __func__);
    }
}

/* Signals a semaphore */
void sys_sem_signal(sys_sem_t *sem)
{
    if (tal_semaphore_post(*sem) != ERR_OK) {
        SYS_ARCH_DBG("%s: call tal_semaphore_post failed\n", __func__);
    }
}

/*
 * Blocks the thread while waiting for the semaphore to be
 * signaled. If the "timeout" argument is non-zero, the thread should
 * only be blocked for the specified time (measured in
 * milliseconds).
 *
 * If the timeout argument is non-zero, the return value is the number of
 * milliseconds spent waiting for the semaphore to be signaled. If the
 * semaphore wasn't signaled within the specified time, the return value is
 * SYS_ARCH_TIMEOUT. If the thread didn't have to wait for the semaphore
 * (i.e., it was already signaled), the function may return zero.
 *
 * Notice that lwIP implements a function with a similar name,
 * sys_sem_wait(), that uses the sys_arch_sem_wait() function.
 */
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    unsigned int start, end, elapsed;

    if (0 == timeout) {
        timeout = TY_LWIP_WAIT_FOREVER;
    }

    start = tal_system_get_millisecond();

    if (tal_semaphore_wait(*sem, timeout) != ERR_OK) {
        SYS_ARCH_DBG("%s: call tal_semaphore_wait failed\n", __func__);
        elapsed = SYS_ARCH_TIMEOUT;
    } else {
        end = tal_system_get_millisecond();
        elapsed = end - start;
    }

    return elapsed;
}

err_t sys_mutex_trylock(sys_mutex_t *pxMutex)
{
    if (tal_semaphore_wait(*pxMutex, 0) != ERR_OK) {
        SYS_ARCH_DBG("%s: call tal_mutex_lock timeout\n", __func__);
        return ERR_MEM;
    }
    return ERR_OK;
}
/* ------------------------ Start implementation ( Mailboxes ) ------------ */

/*
Creates an empty mailbox.
*/
err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    if (tal_queue_create_init(mbox, sizeof(void *), size) != ERR_OK) {
        SYS_ARCH_DBG("%s: call tal_queue_create_init failed\n", __func__);
        return ERR_MEM;
    }

    if (*mbox == NULL) {
        SYS_ARCH_DBG("%s: null mbox\n", __func__);
        return ERR_MEM;
    }

    return ERR_OK;
}

void sys_delay_ms(uint32_t ms)
{
    tal_system_sleep(ms);
}

/*
Deallocates a mailbox. If there are messages still present in the
mailbox when the mailbox is deallocated, it is an indication of a
programming error in lwIP and the developer should be notified.
*/
void sys_mbox_free(sys_mbox_t *mbox)
{
    tal_queue_free(*mbox);
}

/*
 * This function sends a message to a mailbox. It is unusual in that no error
 * return is made. This is because the caller is responsible for ensuring that
 * the mailbox queue will not fail. The caller does this by limiting the number
 * of msg structures which exist for a given mailbox.
 */
void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    if (tal_queue_post(*mbox, &msg, TY_LWIP_WAIT_FOREVER) != ERR_OK) {
        SYS_ARCH_DBG("%s: call tal_queue_post failed\n", __func__);
    }
}

/*
 * Try to post the "msg" to the mailbox. Returns ERR_MEM if this one is full,
 * else, ERR_OK if the "msg" is posted.
 */
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    if (tal_queue_post(*mbox, &msg, 0) != ERR_OK) {
        SYS_ARCH_DBG("%s: call tal_queue_post failed\n", __func__);
        return ERR_MEM;
    }

    return ERR_OK;
}

/*
 * Blocks the thread until a message arrives in the mailbox, but does
 * not block the thread longer than "timeout" milliseconds (similar to
 * the sys_arch_sem_wait() function). The "msg" argument is a result
 * parameter that is set by the function (i.e., by doing "*msg =
 * ptr"). The "msg" parameter maybe NULL to indicate that the message
 * should be dropped.
 *
 * Note that a function with a similar name, sys_mbox_fetch(), is
 * implemented by lwIP.
 */
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    void *dummyptr;
    unsigned int StartTime, EndTime, Elapsed;
    unsigned long result;

    StartTime = tal_system_get_millisecond();
    if (msg == NULL) {
        msg = &dummyptr;
    }

    if (*mbox == NULL) {
        *msg = NULL;
        SYS_ARCH_DBG("%s: input invalid params\n", __func__);
        return ERR_MEM;
    }

    if (timeout) {
        if (tal_queue_fetch(*mbox, &(*msg), timeout) == ERR_OK) {
            EndTime = tal_system_get_millisecond();
            Elapsed = EndTime - StartTime;

            if (Elapsed == 0) {
                Elapsed = 1;
            }
            result = Elapsed;
        } else {
            *msg = NULL;
            result = SYS_ARCH_TIMEOUT;
            SYS_ARCH_DBG("%s: mbox fetch wait timeout %d\n", __func__, timeout);
        }
    } else {
        while(tal_queue_fetch(*mbox, &(*msg), TY_LWIP_WAIT_FOREVER) != ERR_OK);
		EndTime = tal_system_get_millisecond();
        Elapsed = EndTime - StartTime;

        if (Elapsed == 0) {
            Elapsed = 1;
        }
        result = Elapsed;
    }

    // SYS_ARCH_DBG("%s: mobx fetch timeout %d StartTime %d EndTime %d Elapsed
    // %d\n",
    //     __func__, timeout, StartTime, EndTime, Elapsed);
    return result;
}

/*
 * This is similar to sys_arch_mbox_fetch, however if a message is not present
 * in the mailbox, it immediately returns with the code SYS_MBOX_EMPTY
 * On success 0 is returned.
 */
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    void *pvDummy;

    if (msg == NULL) {
        msg = &pvDummy;
    }

    if (tal_queue_fetch(*mbox, &(*msg), 0) != ERR_OK) {
        SYS_ARCH_DBG("%s: call tal_queue_fetch failed\n", __func__);
        return SYS_MBOX_EMPTY;
    }

    return ERR_OK;
}

/** Returns the current time in milliseconds. */
u32_t sys_now(void)
{
    return tal_system_get_millisecond();
}

#if !LWIP_COMPAT_MUTEX
/** Create a new mutex
 * @param mutex pointer to the mutex to create
 * @return a new mutex */
err_t sys_mutex_new(sys_mutex_t *mutex)
{
    if (tal_mutex_create_init(mutex) != ERR_OK) {
        SYS_ARCH_DBG("%s: call tal_mutex_create_init failed\n", __func__);
        return ERR_MEM;
    }

    return ERR_OK;
}

/** Lock a mutex
 * @param mutex the mutex to lock */
void sys_mutex_lock(sys_mutex_t *mutex)
{
    tal_mutex_lock(*mutex);
}

/** Unlock a mutex
 * @param mutex the mutex to unlock */
void sys_mutex_unlock(sys_mutex_t *mutex)
{
    tal_mutex_unlock(*mutex);
}

/** Delete a semaphore
 * @param mutex the mutex to delete */
void sys_mutex_free(sys_mutex_t *mutex)
{
    tal_mutex_release(*mutex);
}

/** Check if a mutex is valid/allocated: return 1 for valid, 0 for invalid */
int sys_mutex_valid(sys_mutex_t *mutex)
{
    return (int)(*mutex);
}

/** Set a mutex invalid so that sys_mutex_valid returns 0 */
void sys_mutex_set_invalid(sys_mutex_t *mutex)
{
    *mutex = NULL;
}
#endif /* LWIP_COMPAT_MUTEX */

/** Set a dealy */
void sys_arch_msleep(int ms)
{
    tal_system_sleep(ms);
}

#if LWIP_NETCONN_SEM_PER_THREAD
/**
 * @brief sys_get_task_handle
 *
 * @param   void
 * @return  void*
 *
 */
#include "tkl_thread.h"

void *sys_get_task_handle(void)
{
    TKL_THREAD_HANDLE thread;

    tkl_thread_get_id(&thread);

    return (void *)thread;
}

#endif /* LWIP_NETCONN_SEM_PER_THREAD */

uint32_t sys_random(void)
{
    return rand();
}

#if LWIP_COMPAT_MUTEX
int sys_mbox_valid(sys_mbox_t *x)
{
    return (((*x) == NULL) ? 0 : 1);
}

void sys_mbox_set_invalid(sys_mbox_t *x)
{
    ((*x) = NULL);
}

int sys_sem_valid(sys_sem_t *x)
{
    return (((*x) == NULL) ? 0 : 1);
}

void sys_sem_set_invalid(sys_sem_t *x)
{
    ((*x) = NULL);
}
#endif /* LWIP_COMPAT_MUTEX */
