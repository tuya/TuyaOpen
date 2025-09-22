/**
 * @file tuya_mem_heap.h
 * @brief TUYA memory heap management
 * @version 0.1
 * @date 2021-05-05
 *
 * @copyright Copyright 2021 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TUYA_MEMORY_HEAP_H__
#define __TUYA_MEMORY_HEAP_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEM_HEAP_LIST_NUM (4)

typedef struct {
    void (*enter_critical)(void);
    void (*exit_critical)(void);
    void (*dbg_output)(char *format, ...);
} heap_context_t;

typedef struct {
    unsigned long total_size;          // total heap size
    unsigned long free_size;           // current free heap size
    unsigned long free_watermark;      // minimum ever free heap size
    unsigned long max_free_block_size; // size of the largest free block
} heap_state_t;

typedef void *HEAP_HANDLE;

int tuya_mem_heap_init(heap_context_t *ctx);
int tuya_mem_heap_create(void *start_addr, unsigned int size, HEAP_HANDLE *handle);
int tuya_mem_heap_delete(HEAP_HANDLE handle);
void *tuya_mem_heap_malloc(HEAP_HANDLE handle, unsigned int size);
void *tuya_mem_heap_calloc(HEAP_HANDLE handle, unsigned int size);
void *tuya_mem_heap_realloc(HEAP_HANDLE handle, void *ptr, unsigned int size);
void tuya_mem_heap_free(HEAP_HANDLE handle, void *ptr);
void tuya_mem_heap_state(HEAP_HANDLE handle, heap_state_t *state);
int tuya_mem_heap_available(HEAP_HANDLE handle);

void *tuya_mem_heap_debug_malloc(HEAP_HANDLE handle, unsigned int size, char *filename, int line);
void *tuya_mem_heap_debug_calloc(HEAP_HANDLE handle, unsigned int size, char *filename, int line);
void *tuya_mem_heap_debug_realloc(HEAP_HANDLE handle, void *ptr, unsigned int size, char *filename, int line);
int tuya_mem_heap_diagnose(HEAP_HANDLE handle);

#ifdef __cplusplus
}
#endif

#endif //__TUYA_MEMORY_HEAP_H__
