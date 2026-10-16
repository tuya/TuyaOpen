/**
 * @file tal_compat.c
 * @brief 交付仓兼容层的**函数级实现**（tal_compat.h 负责宏重定向/声明，这里提供真实符号）。
 *
 * 为什么需要真实符号（而不是纯宏）：平台自带的**预编译静态库**是按上游
 * `tal_psram_*` 名字编好的（例：platform/LINUX/.../tkl_audio/libs/<board>/audio_subsys/
 * libaudio_subsys.a 里的 speexdsp/rnn_vad/mfcc 引用 tal_psram_malloc/free/calloc）。
 * 宏只能改源码，改不了 .a 里的重定位符号，因此必须在链接期提供同名函数。
 *
 * 实现口径与上游 `src/tal_system/src/tal_system.c` 一致：直绑平台 tkl 的 psram 接口
 * （上游 tal_psram_malloc → tkl_system_psram_malloc 等）。**刻意不走 core 的
 * tal_ex_ram_***：core 的 ex_ram 接口在 `ENABLE_EXT_RAM` 关闭时整体编掉
 * （iot-core/tal_system/src/tal_api.c:182-193），而本兼容层要无条件可链接。
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_compat.h"
#include "tkl_memory.h"

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
