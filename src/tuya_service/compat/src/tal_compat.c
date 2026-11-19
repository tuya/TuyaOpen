/**
 * @file tal_compat.c
 * @brief Function-level compatibility symbols for legacy tal_* interfaces.
 *
 * Platform prebuilt libraries reference tal_psram_* and tal_log_print_* by
 * their legacy names; the platform tkl API and core_log provide the
 * implementations.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_compat.h"

#include <stdarg.h>
#include <stdio.h>

#include "core_log.h"

#if defined(TUYAOPEN_PLATFORM_ESP32)
#include "esp_heap_caps.h"
#else
#include "tkl_memory.h"
#endif

#if defined(TUYAOPEN_PLATFORM_ESP32)
void *tal_psram_malloc(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

void tal_psram_free(void *ptr)
{
    if (NULL == ptr) {
        return;
    }
    heap_caps_free(ptr);
}

void *tal_psram_calloc(size_t nitems, size_t size)
{
    return heap_caps_calloc(nitems, size, MALLOC_CAP_SPIRAM);
}

void *tal_psram_realloc(void *ptr, size_t size)
{
    return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM);
}

INT_T tal_psram_get_free_heap_size(VOID_T)
{
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}
#else
void *tal_psram_malloc(size_t size)
{
    return tkl_system_psram_malloc(size);
}

void tal_psram_free(void *ptr)
{
    if (NULL == ptr) {
        return;
    }
    tkl_system_psram_free(ptr);
}

void *tal_psram_calloc(size_t nitems, size_t size)
{
    return tkl_system_psram_calloc(nitems, size);
}

void *tal_psram_realloc(void *ptr, size_t size)
{
    return tkl_system_psram_realloc(ptr, size);
}

INT_T tal_psram_get_free_heap_size(VOID_T)
{
    return tkl_system_psram_get_free_heap_size();
}
#endif

/* legacy names referenced by platform prebuilt libraries */
#undef tal_log_print_raw
#undef tal_log_print_secure

OPERATE_RET tal_log_print_raw(const char *pFmt, ...)
{
    va_list     ap;
    OPERATE_RET ret;

    va_start(ap, pFmt);
    ret = core_log_vprint_raw(pFmt, ap);
    va_end(ap);

    return ret;
}

OPERATE_RET tal_log_print_secure(BOOL_T is_const_fmt, const CORE_LOG_LEVEL_E level, const char *file, const int line,
                                 const char *fmt, ...)
{
    char    buf[512];
    va_list ap;

    (void)is_const_fmt;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    return core_log_print_secure(TRUE, level, file, line, "%s", buf);
}
