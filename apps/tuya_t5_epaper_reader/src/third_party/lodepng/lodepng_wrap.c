#include <string.h>
#include "tal_api.h"
#include "tal_memory.h"
#include "tkl_memory.h"

#define LV_USE_PNG       1
#define LV_PNG_USE_PSRAM 0
#define LV_UNUSED(x)     ((void)(x))
#define lv_memcpy        memcpy
#define lv_memset        memset

#define LODEPNG_NO_COMPILE_DISK
#define LODEPNG_NO_COMPILE_ENCODER
#define LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS
#define LODEPNG_NO_COMPILE_CPP
#define LODEPNG_NO_COMPILE_ALLOCATORS

#include "lodepng.h"

void *lodepng_malloc(size_t size)
{
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    PR_DEBUG("png_alloc: malloc %u heap_free=0x%x psram_free=0x%x", (unsigned)size, tal_system_get_free_heap_size(),
             tkl_system_psram_get_free_heap_size());
    return tal_psram_malloc(size);
#else
    PR_DEBUG("png_alloc: malloc %u heap_free=0x%x", (unsigned)size, tal_system_get_free_heap_size());
    return tal_malloc(size);
#endif
}

void *lodepng_realloc(void *ptr, size_t new_size)
{
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    PR_DEBUG("png_alloc: realloc %u heap_free=0x%x psram_free=0x%x", (unsigned)new_size,
             tal_system_get_free_heap_size(), tkl_system_psram_get_free_heap_size());
    return tal_psram_realloc(ptr, new_size);
#else
    PR_DEBUG("png_alloc: realloc %u heap_free=0x%x", (unsigned)new_size, tal_system_get_free_heap_size());
    return tal_realloc(ptr, new_size);
#endif
}

void lodepng_free(void *ptr)
{
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    PR_DEBUG("png_alloc: free heap_free=0x%x psram_free=0x%x", tal_system_get_free_heap_size(),
             tkl_system_psram_get_free_heap_size());
    tal_psram_free(ptr);
#else
    PR_DEBUG("png_alloc: free heap_free=0x%x", tal_system_get_free_heap_size());
    tal_free(ptr);
#endif
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../../../../../../platform/T5AI/t5_os/ap/components/lvgl/src/extra/libs/png/lodepng.c"
#pragma GCC diagnostic pop
