/**
 * @file sys_arch.h
 * @brief LWIP-ÏµÍ³¶Ô½Ó½Ó¿Ú·â×°
 *
 * @copyright Copyright(C),2018-2020, Í¿Ñ»¿Æ¼¼ www.tuya.com
 *
 */

#ifndef __SYS_RTXC_H__
#define __SYS_RTXC_H__

#include "tal_mutex.h"
#include "tal_queue.h"
#include "tal_semaphore.h"
#include "tal_thread.h"
#include "tal_system.h"

#define SYS_MBOX_NULL (QUEUE_HANDLE)0
#define SYS_SEM_NULL  (SEM_HANDLE)0

/* ------------------------ Type definitions ------------------------------ */
typedef SEM_HANDLE    sys_sem_t;
typedef MUTEX_HANDLE  sys_mutex_t;
typedef THREAD_HANDLE sys_thread_t;
typedef int           sys_prot_t;
typedef QUEUE_HANDLE  sys_mbox_t;

#endif /* __SYS_RTXC_H__ */
