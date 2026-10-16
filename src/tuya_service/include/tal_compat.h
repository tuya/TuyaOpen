/**
 * @file tal_compat.h
 * @brief 交付仓兼容层：把上游 TuyaOpen 里应用/组件常用、而本仓 core 不再提供
 *        （或已改由平台 tkl 层/契约提供）的 tal_* 接口，就地重定向到等价实现，
 *        让 apps/ 下的应用尽量少改动（目标：0 改动，逐步推进）。
 *
 * 用法：本头随 src/tuya_service 的统一 include 路径（COMPONENT_PUBINC）全局可见，
 *       并由根 CMakeLists 对 **应用与全部组件** 统一预包含（-include tal_compat.h），
 *       因此任何 TU 无需显式 include 即生效（部分服务组件头也主动带入，双保险）。
 *       新加兼容项时：只在这里加宏/声明，不要改回调用点。
 *
 * 组成：① 宏重定向（tal_system_get_free_heap_size 等）；② 真实函数补齐
 *       （tal_psram_*，实现见 src/tuya_service/compat/src/tal_compat.c）。
 *       能用函数就别用宏——宏在"当语句用"时会触发 -Wunused-value（ESP32 -Werror 下致命），
 *       且宏改不了预编译库里的重定位符号。
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __TAL_COMPAT_H__
#define __TAL_COMPAT_H__

#include "tal_api.h"    /* tal_free */
#include "tkl_memory.h" /* tkl_system_get_free_heap_size */

/* 上游 tal_api.h 会把 tal_uart.h 一并带出（上游 tal_api.h:57），core 的 tal_api.h
 * 无硬件接口面；本仓 tal_uart 由 src/tuya_service/tal_driver 提供，这里补上声明，
 * 使 `tal_uart_write(...)`/`TUYA_UART_NUM_0` 等上游写法原样可用 */
#include "tal_uart.h"

/* 上游 tal_api.h → tal_kv.h 链会带出 tal_kv 组件超集（tal_lfs_get/tal_kv_free/
 * tal_kv_serialize_* + lfs.h）；core 的 tal_api.h 带出的只有 core 版 tal_kv.h
 * （4 个 API）⇒ 组件超集被遮蔽（audio_player/datasink_file.c 的 tal_lfs_get、
 * app 的 tal_kv_free 就是这样丢声明的）。这里显式包含组件超集
 * （组件头已改名 lib_kv.h，与 core 版文件名/保护宏都不同，可共存） */
#include "lib_kv.h"

/* 堆查询：上游 tal_system_get_free_heap_size() → 平台 tkl_system_get_free_heap_size()
 * （本仓 core 的 tal_system 无此接口） */
#ifndef tal_system_get_free_heap_size
#define tal_system_get_free_heap_size() tkl_system_get_free_heap_size()
#endif

/* KV 值释放（tal_kv_free）不在本层：它是 tal_kv 组件的原有导出，声明与实现都在
 * src/tuya_service/lib_kv（上游原位），本头上面已把组件版 tal_kv.h 带入 */

/* PSRAM（外部 RAM）：上游 tal_psram_* 在 core 里改名为 tal_ex_ram_*（且 core 的
 * ex_ram 接口在 ENABLE_EXT_RAM 关闭时整体编掉）。这里不做宏重定向，而是提供
 * **同名真实函数**（实现在 src/tuya_service/compat/src/tal_compat.c，直绑平台
 * tkl_system_psram_*）——因为平台预编译库（如 LINUX 的 libaudio_subsys.a）
 * 按旧名引用这些符号，宏改不了 .a 里的重定位项。 */
void *tal_psram_malloc(size_t size);
void  tal_psram_free(void *ptr);
void *tal_psram_calloc(size_t nitems, size_t size);
void *tal_psram_realloc(void *ptr, size_t size);
INT_T tal_psram_get_free_heap_size(VOID_T);

/* ---------------------------------------------------------------------------
 * AI agent 接口补齐：core 的 svc_ai_agent 相对上游裁剪掉的三个导出，
 * 实现在 src/ai_components/utility/src/ai_agent_api_compat.c（含语义差异说明），
 * 这里只补声明——上游 ai_mode / ai_picture 的调用点原样保留。
 * --------------------------------------------------------------------------- */
#if defined(ENABLE_AI_COMPONENTS) && (ENABLE_AI_COMPONENTS == 1)
OPERATE_RET tuya_ai_agent_set_scode(CHAR_T *scode);
VOID        tuya_ai_agent_server_vad_ctrl(BOOL_T flag);
VOID        tuya_ai_agent_set_session_param(CHAR_T *value);
#endif

#endif /* __TAL_COMPAT_H__ */
