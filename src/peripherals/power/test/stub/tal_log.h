/* host stub */
#ifndef __STUB_TAL_LOG_H__
#define __STUB_TAL_LOG_H__
#include <stdio.h>
#include "tuya_cloud_types.h"
#define PR_ERR(...)    do { printf("[ERR] "); printf(__VA_ARGS__); printf("\n"); } while (0)
#define PR_WARN(...)   do { } while (0)
#define PR_NOTICE(...) do { } while (0)
#define PR_DEBUG(...)  do { } while (0)
/* mirror the repo convention: the macro uses the caller's own `rt` variable */
#define TUYA_CALL_ERR_RETURN(x)                                                                                        \
    do {                                                                                                               \
        rt = (x);                                                                                                      \
        if (OPRT_OK != rt) {                                                                                           \
            return rt;                                                                                                 \
        }                                                                                                              \
    } while (0)
#define TUYA_CALL_ERR_LOG(x) (void)(x)
#endif
