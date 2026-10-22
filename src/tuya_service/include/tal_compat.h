/**
 * @file tal_compat.h
 * @brief Compatibility layer for legacy tal_* interfaces.
 *
 * Force-included into every component and application by the root CMakeLists.
 * Macro aliases live in tal_core_alias.h; function-level symbols are
 * implemented in compat/src/tal_compat.c.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#ifndef __TAL_COMPAT_H__
#define __TAL_COMPAT_H__

#include "tal_api.h"
#include "tkl_memory.h"

/* tal_* -> core_* aliases */
#include "tal_core_alias.h"

/* uart */
#include "tal_uart.h"

/* kv component extensions */
#include "lib_kv.h"

#define tal_system_get_free_heap_size() tkl_system_get_free_heap_size()

/* external RAM */
void *tal_psram_malloc(size_t size);
void  tal_psram_free(void *ptr);
void *tal_psram_calloc(size_t nitems, size_t size);
void *tal_psram_realloc(void *ptr, size_t size);
INT_T tal_psram_get_free_heap_size(VOID_T);

/* ai agent interfaces */
#if defined(ENABLE_AI_COMPONENTS) && (ENABLE_AI_COMPONENTS == 1)
OPERATE_RET tuya_ai_agent_set_scode(CHAR_T *scode);
VOID        tuya_ai_agent_server_vad_ctrl(BOOL_T flag);
VOID        tuya_ai_agent_set_session_param(CHAR_T *value);
#endif

#endif /* __TAL_COMPAT_H__ */
