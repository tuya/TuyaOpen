/* host stub: does NOT actually start a thread (tests run synchronously) */
#ifndef __STUB_TAL_THREAD_H__
#define __STUB_TAL_THREAD_H__
#include "tuya_cloud_types.h"
typedef void *THREAD_HANDLE;
typedef enum { THREAD_PRIO_0 = 5, THREAD_PRIO_1 = 4, THREAD_PRIO_2 = 3 } THREAD_PRIO_E;
typedef struct {
    uint32_t stackDepth;
    uint8_t  priority;
    char    *thrdname;
} THREAD_CFG_T;
typedef void (*THREAD_FUNC_CB)(void *arg);
static inline OPERATE_RET tal_thread_create_and_start(THREAD_HANDLE *h, void *enter, void *exit, THREAD_FUNC_CB func,
                                                      const void *arg, const THREAD_CFG_T *cfg)
{
    *h = (void *)1; /* do not run the worker in unit tests */
    return OPRT_OK;
}
static inline OPERATE_RET tal_thread_delete(THREAD_HANDLE h) { return OPRT_OK; }
#endif
