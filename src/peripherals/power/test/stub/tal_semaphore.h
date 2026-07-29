/* host stub */
#ifndef __STUB_TAL_SEMAPHORE_H__
#define __STUB_TAL_SEMAPHORE_H__
#include "tuya_cloud_types.h"
typedef void *SEM_HANDLE;
#define SEM_WAIT_FOREVER 0xFFFFFFFFu
static inline OPERATE_RET tal_semaphore_create_init(SEM_HANDLE *h, uint32_t c, uint32_t m) { *h = (void *)1; return OPRT_OK; }
static inline OPERATE_RET tal_semaphore_wait(SEM_HANDLE h, uint32_t t) { return OPRT_OK; }
static inline OPERATE_RET tal_semaphore_post(SEM_HANDLE h) { return OPRT_OK; }
#endif
