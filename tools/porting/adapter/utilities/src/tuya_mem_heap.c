/**
 * @file tuya_memory_heap.c
 * @brief TUYA memory heap management
 * @version 0.1
 * @date 2021-05-05
 *
 * @copyright Copyright 2021 Tuya Inc. All Rights Reserved.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tuya_iot_config.h"
#include "tuya_mem_heap.h"

#define MEM_DEBUG_ASSERT_ON (0)
#define MEM_BLOCK_STATIC    (0)
#define MEM_ANTI_FRAGMENT   (1)
#define MEM_DEBUG_FREE_FILL (0)

#define MEM_DEBUG_FILL_VAL (0xF7)
#define MEM_BLOCK_MIN_SIZE (24)
#if defined(OPERATING_SYSTEM) && (SYSTEM_LINUX == OPERATING_SYSTEM)
#define MEM_ALIGN_NUM (8)
#else
#define MEM_ALIGN_NUM (4)
#endif
#define FIT_FIND_DEPTH (3)

#if MEM_BLOCK_MIN_SIZE < MEM_ALIGN_NUM
#error "MEM_BLOCK_MIN_SIZE < MEM_ALIGN_NUM"
#endif

typedef struct MEM_HeapBlock_s {
    unsigned long size;
    struct MEM_HeapBlock_s *next;
} MEM_HeapBlock_t;

typedef struct {
    MEM_HeapBlock_t *free_list;
    unsigned char *base;
    unsigned long size;
    unsigned long free;
    unsigned long free_watermark;
} MEM_Heap_t;

typedef struct {
    unsigned long size;
    unsigned long free;
    unsigned long free_largest;
    unsigned long valid;
    unsigned long used_block;
    unsigned long free_block;
} MEM_HeapStatus_t;

#define MEM_DBG_LEAK_MAGIC 0x13572468
typedef struct {
    char *filename;
    long line;
    unsigned long size;
    unsigned long magic;
} MEM_DbgLeak_t;

#define ALIGN_UP(x)   (((unsigned long)(x) + (MEM_ALIGN_NUM - 1)) & (~(MEM_ALIGN_NUM - 1)))
#define ALIGN_DOWN(x) ((unsigned long)(x) & (~(MEM_ALIGN_NUM - 1)))

#if defined(MEM_DEBUG_ASSERT_ON) && (MEM_DEBUG_ASSERT_ON == 1)
#define MEM_ASSERT(x)                                                                                                  \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            s_heap_ctx.dbg_output("[MEM DBG] :mem assert at line %d\r\n", __LINE__);                                   \
            while (1)                                                                                                  \
                ;                                                                                                      \
        }                                                                                                              \
    } while (0)
#else
#define MEM_ASSERT(x)
#endif

#define MEM_BLOCK_HEAD_SIZE (sizeof(MEM_HeapBlock_t) - sizeof(unsigned long))
#define MEM_HEAP_MIN_SIZE   (MEM_BLOCK_MIN_SIZE + MEM_BLOCK_HEAD_SIZE)

#define MEM_BLOCK_STAT_USE  0x55
#define MEM_BLOCK_STAT_FREE 0xaa

#define MEM_DOG_ADDR(block) ((unsigned char *)block + block->size - 1)
#define MEM_LEAK_DBG_ADDR(block)                                                                                       \
    (MEM_DbgLeak_t *)((unsigned long)(intptr_t)block + block->size - sizeof(MEM_DbgLeak_t) - MEM_ALIGN_NUM)

static MEM_Heap_t mem_heap_list[MEM_HEAP_LIST_NUM] = {0};
static unsigned long s_heap_free_size = 0;
static unsigned long s_heap_free_size_watermark = 0; // minimum free size ever
static heap_context_t s_heap_ctx;

static int mem_heap_init(MEM_Heap_t *heap, void *ptr, unsigned long size)
{
#if defined(MEM_DEBUG_FREE_FILL) && (MEM_DEBUG_FREE_FILL == 1)
    memset(ptr, MEM_DEBUG_FILL_VAL, size);
#endif

    heap->base = ptr;
    heap->size = size;

    ptr = (void *)(intptr_t)ALIGN_UP((intptr_t)ptr);
    size -= (unsigned long)(intptr_t)ptr - (unsigned long)(intptr_t)heap->base;
    size = ALIGN_DOWN(size);

    if (size < MEM_HEAP_MIN_SIZE) {
        return -1;
    }

    heap->free_list = (MEM_HeapBlock_t *)ptr;
    heap->free_list->next = NULL;
    heap->free_list->size = size;

    heap->free = size;
    heap->free_watermark = size;
    s_heap_free_size += size;
    s_heap_free_size_watermark = s_heap_free_size;

    *MEM_DOG_ADDR(heap->free_list) = MEM_BLOCK_STAT_FREE;

    MEM_ASSERT((unsigned long)heap->free_list >= (unsigned long)heap->base);
    MEM_ASSERT((unsigned long)heap->free_list + heap->free_list->size <= (unsigned long)heap->base + heap->size);

    return 0;
}

static MEM_HeapBlock_t *mem_chunk_get(MEM_Heap_t *heap, unsigned long size)
{
    MEM_HeapBlock_t *pre_block;
    MEM_HeapBlock_t *this_block;
    MEM_HeapBlock_t *new_block;

#if defined(MEM_ANTI_FRAGMENT) && (MEM_ANTI_FRAGMENT == 1)
    MEM_HeapBlock_t *pre_block_bak;
    MEM_HeapBlock_t *this_block_bak;
    long find_num = 0;
#endif

    pre_block = NULL;
    this_block = heap->free_list;

    while (this_block) {
        MEM_ASSERT((unsigned long)this_block >= ALIGN_UP(heap->base));
        MEM_ASSERT((unsigned long)this_block + this_block->size <= ALIGN_DOWN(heap->base + heap->size));

        if (this_block->size >= size) {
#if defined(MEM_ANTI_FRAGMENT) && (MEM_ANTI_FRAGMENT == 1)
            pre_block_bak = this_block;
            this_block_bak = this_block->next;
            while (this_block_bak) {
                if ((this_block_bak->size >= size) && (this_block_bak->size <= this_block->size)) {
                    this_block = this_block_bak;
                    pre_block = pre_block_bak;
                    find_num++;
                }

                if (find_num >= FIT_FIND_DEPTH) {
                    break;
                }

                pre_block_bak = this_block_bak;
                this_block_bak = this_block_bak->next;
            }
#endif

            if ((this_block->size - size) >= MEM_HEAP_MIN_SIZE) {
                this_block->size -= size;

                *MEM_DOG_ADDR(this_block) = MEM_BLOCK_STAT_FREE;
                new_block = (MEM_HeapBlock_t *)(intptr_t)((unsigned long)(intptr_t)this_block + this_block->size);
                new_block->size = size;

                *MEM_DOG_ADDR(new_block) = MEM_BLOCK_STAT_USE;
                return new_block;
            } else {
                if (pre_block) {
                    pre_block->next = this_block->next;
                } else {
                    heap->free_list = this_block->next;
                }

                *MEM_DOG_ADDR(this_block) = MEM_BLOCK_STAT_USE;
                return this_block;
            }
        }

        pre_block = this_block;
        this_block = this_block->next;

        MEM_ASSERT((!this_block) || (this_block > pre_block));
    }

    return (NULL);
}

static MEM_Heap_t *MEM_HeapCreate(void *ptr, unsigned long size)
{
    MEM_Heap_t *heap = NULL;
    long i;

    if (ptr == NULL || size == 0) {
        s_heap_ctx.dbg_output("[MEM DBG] MEM_HeapCreate params err\r\n");
        return NULL;
    }

    s_heap_ctx.enter_critical();
    for (i = 0; i < MEM_HEAP_LIST_NUM; i++) {
        if (mem_heap_list[i].size == 0) {
            break;
        }
    }

    if (i < MEM_HEAP_LIST_NUM) {
        heap = &mem_heap_list[i];
        if (mem_heap_init(heap, ptr, size) != 0) {
            heap->size = 0;
            heap = NULL;
        }
    }
    s_heap_ctx.exit_critical();

    return heap;
}

static void MEM_HeapDelete(MEM_Heap_t *heap)
{
    long i = 0;

    if (heap == NULL) {
        s_heap_ctx.dbg_output("[MEM DBG] MEM_HeapDelete params err\r\n");
        return;
    }

    s_heap_ctx.enter_critical();
    for (i = 0; i < MEM_HEAP_LIST_NUM; i++) {
        if (heap == &mem_heap_list[i]) {
            break;
        }
    }

    if (i < MEM_HEAP_LIST_NUM) {
        memset(heap, 0, sizeof(MEM_Heap_t));
    }
    s_heap_ctx.exit_critical();
}

static void *MEM_Allocate(MEM_Heap_t *heap, unsigned long size)
{
    unsigned long new_size;
    MEM_HeapBlock_t *block;

    if (heap == NULL || size == 0) {
        return (NULL);
    }

    size = size < 4 ? 4 : size;

    new_size = ALIGN_UP(size + 1) + MEM_BLOCK_HEAD_SIZE;
    if (new_size < size) {
        return (NULL);
    }

    s_heap_ctx.enter_critical();
    block = mem_chunk_get(heap, new_size);
    if (block) {
        heap->free -= block->size;
        if (heap->free_watermark > heap->free) {
            heap->free_watermark = heap->free;
        }

        s_heap_free_size -= block->size;
        if (s_heap_free_size_watermark > s_heap_free_size) {
            s_heap_free_size_watermark = s_heap_free_size;
        }
    }
    s_heap_ctx.exit_critical();

    if (block) {
        return (void *)((unsigned long)(intptr_t)block + MEM_BLOCK_HEAD_SIZE);
    }

    return NULL;
}

static void *MEM_AllocateDebug(MEM_Heap_t *heap, unsigned long size, char *filename, long line)
{
    void *p;
    MEM_HeapBlock_t *block;

    MEM_DbgLeak_t *leak;
    unsigned long new_size = ALIGN_UP(size);
    p = MEM_Allocate(heap, new_size + sizeof(MEM_DbgLeak_t));
    if (p) {
        block = (MEM_HeapBlock_t *)((unsigned long)(intptr_t)p - MEM_BLOCK_HEAD_SIZE);

        /*得到保存调试信息的地址*/
        leak = MEM_LEAK_DBG_ADDR(block);
        leak->filename = filename;
        leak->line = line;
        leak->size = size;
        leak->magic = MEM_DBG_LEAK_MAGIC;
    }

    return p;
}

static void MEM_Deallocate(MEM_Heap_t *heap, void *ptr)
{
    MEM_HeapBlock_t *free_block;
    MEM_HeapBlock_t *next_block;
    MEM_HeapBlock_t *pre_block;
    unsigned char *pdog;

    if (heap == NULL || ptr == NULL) {
        return;
    }

    free_block = (MEM_HeapBlock_t *)((unsigned long)(intptr_t)ptr - MEM_BLOCK_HEAD_SIZE);

    pdog = MEM_DOG_ADDR(free_block);

    if (*pdog != MEM_BLOCK_STAT_USE) {
        s_heap_ctx.dbg_output("[MEM DBG] MEM_Deallocate MEM_DEBUG_DOG_TAG err %p,size=%d\r\n", ptr, free_block->size);

        if (*pdog == MEM_BLOCK_STAT_FREE) {
            s_heap_ctx.dbg_output("[MEM DBG] mem %p might be freed yet\r\n", ptr);
        }

        return;
    }

#if defined(MEM_DEBUG_FREE_FILL) && (MEM_DEBUG_FREE_FILL == 1)
    memset(ptr, MEM_DEBUG_FILL_VAL, free_block->size - MEM_BLOCK_HEAD_SIZE);
#endif

    s_heap_ctx.enter_critical();

    *pdog = MEM_BLOCK_STAT_FREE;

    MEM_ASSERT((unsigned long)free_block >= (unsigned long)heap->base);
    MEM_ASSERT((unsigned long)free_block + free_block->size <= (unsigned long)heap->base + heap->size);

    heap->free += free_block->size;
    s_heap_free_size += free_block->size;

    next_block = heap->free_list;
    pre_block = NULL;
    while (next_block && (next_block < free_block)) {
        MEM_ASSERT((unsigned long)next_block >= ALIGN_UP(heap->base));
        MEM_ASSERT((unsigned long)next_block + next_block->size <= ALIGN_DOWN(heap->base + heap->size));

        pre_block = next_block;
        next_block = next_block->next;

        MEM_ASSERT((!next_block) || (next_block > pre_block));
    }

    MEM_ASSERT((!next_block) || (next_block > free_block));
    MEM_ASSERT((!pre_block) || (pre_block < free_block));

    free_block->next = next_block;
    if (!pre_block) {
        heap->free_list = free_block;
        pre_block = free_block;
    } else {
        if (((char *)pre_block + pre_block->size) == (char *)free_block) {
#if defined(MEM_DEBUG_FREE_FILL) && (MEM_DEBUG_FREE_FILL == 1)
            *MEM_DOG_ADDR(pre_block) = MEM_DEBUG_FILL_VAL;
#endif

            pre_block->size += free_block->size;

#if defined(MEM_DEBUG_FREE_FILL) && (MEM_DEBUG_FREE_FILL == 1)
            memset(free_block, MEM_DEBUG_FILL_VAL, MEM_BLOCK_HEAD_SIZE);
#endif
        } else {
            pre_block->next = free_block;
            pre_block = free_block;
        }
    }

    if (next_block) {
        if (((char *)pre_block + pre_block->size) == (char *)next_block) {
#if defined(MEM_DEBUG_FREE_FILL) && (MEM_DEBUG_FREE_FILL == 1)
            *MEM_DOG_ADDR(pre_block) = MEM_DEBUG_FILL_VAL;
#endif
            pre_block->size += next_block->size;
            pre_block->next = next_block->next;

#if defined(MEM_DEBUG_FREE_FILL) && (MEM_DEBUG_FREE_FILL == 1)
            memset(next_block, MEM_DEBUG_FILL_VAL, sizeof(MEM_HeapBlock_t));
#endif
        }
    }
    s_heap_ctx.exit_critical();
}

static void MEM_HeapStatus(MEM_Heap_t *heap, MEM_HeapStatus_t *status)
{
    MEM_HeapBlock_t *freeBlockp = NULL;
    MEM_HeapBlock_t *thisBlockp = NULL;
    MEM_DbgLeak_t *leak = NULL;
    unsigned long result = 0;
    unsigned long addr = 0;
    unsigned long top_addr = 0;
    unsigned long thisSize = 0;

    if (heap == NULL || status == NULL) {
        return;
    }

    memset(status, 0, sizeof(MEM_HeapStatus_t));
    status->size = heap->size;

    addr = ALIGN_UP((intptr_t)heap->base);
    top_addr = ALIGN_DOWN((intptr_t)heap->base + heap->size);

    s_heap_ctx.enter_critical();

    freeBlockp = heap->free_list;
    while (addr < top_addr) {
        thisBlockp = (MEM_HeapBlock_t *)(intptr_t)addr;

        MEM_ASSERT(thisBlockp);
        MEM_ASSERT((unsigned long)thisBlockp >= (unsigned long)heap->base);
        MEM_ASSERT((unsigned long)thisBlockp + thisBlockp->size <= (unsigned long)heap->base + heap->size);

        if (*MEM_DOG_ADDR(thisBlockp) == MEM_BLOCK_STAT_USE) {
            if (thisBlockp == freeBlockp) {
                result = 1;
                goto EXIT;
            }

            leak = MEM_LEAK_DBG_ADDR(thisBlockp);
            if (leak->magic == MEM_DBG_LEAK_MAGIC) {
                s_heap_ctx.exit_critical();
                s_heap_ctx.dbg_output("[MEM DBG] [mem use] %s:%d, addr=%p, size=%d\r\n", leak->filename, leak->line,
                                      thisBlockp, leak->size);
                s_heap_ctx.enter_critical();
            }

            status->used_block++;
        } else if (*MEM_DOG_ADDR(thisBlockp) == MEM_BLOCK_STAT_FREE) {
            MEM_ASSERT(freeBlockp);
            MEM_ASSERT((unsigned long)freeBlockp >= ALIGN_UP(heap->base));
            MEM_ASSERT((unsigned long)freeBlockp + freeBlockp->size <= ALIGN_DOWN(heap->base + heap->size));

            if (thisBlockp != freeBlockp) {
                result = 2;
                goto EXIT;
            }

            thisSize = thisBlockp->size - MEM_BLOCK_HEAD_SIZE - 1;

            status->free += thisSize;

            if (thisSize > status->free_largest) {
                status->free_largest = thisSize;
            }

            freeBlockp = freeBlockp->next;
            status->free_block++;
        } else {
            result = 3;
            goto EXIT;
        }

        addr += thisBlockp->size;
    }

    MEM_ASSERT(addr == top_addr);
    MEM_ASSERT(!freeBlockp);

    if ((addr == top_addr) && (!freeBlockp)) {
        status->valid = 1;
    }

EXIT:
    s_heap_ctx.exit_critical();

    if (0 != result) {
        if (1 == result) {
            s_heap_ctx.dbg_output("[MEM DBG] [ERROR]thisBlockp == freeBlockp,addr=%p,size=%d\r\n", thisBlockp,
                                  thisBlockp->size);
        } else if (2 == result) {
            s_heap_ctx.dbg_output("[MEM DBG] [ERROR]thisBlockp != freeBlockp,addr=%p,size=%d\r\n", thisBlockp,
                                  thisBlockp->size);
        } else if (3 == result) {
            s_heap_ctx.dbg_output("[MEM DBG] DOG TAG ERR:addr=%p,size=%d\r\n", thisBlockp, thisBlockp->size);
        }
    }
}

int tuya_mem_heap_init(heap_context_t *ctx)
{
    if ((NULL == ctx) || (NULL == ctx->enter_critical) || (NULL == ctx->exit_critical) || (NULL == ctx->dbg_output)) {
        return -1;
    }

    s_heap_ctx.enter_critical = ctx->enter_critical;
    s_heap_ctx.exit_critical = ctx->exit_critical;
    s_heap_ctx.dbg_output = ctx->dbg_output;

    return 0;
}

int tuya_mem_heap_create(void *start_addr, unsigned int size, HEAP_HANDLE *handle)
{
    MEM_Heap_t *pMemHeap = NULL;

    s_heap_ctx.dbg_output("[MEM DBG] heap init-------size:%d addr:%p---------\r\n", size, start_addr);

    pMemHeap = MEM_HeapCreate(start_addr, size);
    if (NULL == pMemHeap) {
        return -1;
    }

    if (handle) {
        *handle = (HEAP_HANDLE)pMemHeap;
    }

    return 0;
}

int tuya_mem_heap_delete(HEAP_HANDLE handle)
{
    MEM_HeapDelete((MEM_Heap_t *)handle);
    return 0;
}

void *tuya_mem_heap_malloc(HEAP_HANDLE handle, unsigned int size)
{
    if (0 != handle) {
        return MEM_Allocate((MEM_Heap_t *)handle, size);
    } else {
        long idx = 0;
        void *ptr = NULL;
        MEM_Heap_t *pHeap = NULL;

        for (idx = 0; idx < MEM_HEAP_LIST_NUM; idx++) {
            pHeap = &mem_heap_list[idx];
            if (pHeap->size > 0) {
                if (pHeap->free > (size + MEM_BLOCK_MIN_SIZE)) {
                    ptr = MEM_Allocate(pHeap, size);
                    if (NULL != ptr) {
                        return ptr;
                    }
                }
            } else {
                break;
            }
        }

        return NULL;
    }
}

void *tuya_mem_heap_calloc(HEAP_HANDLE handle, unsigned int size)
{
    void *ptr = tuya_mem_heap_malloc(handle, size);
    if (ptr) {
        memset(ptr, 0, size);
    }

    return ptr;
}

void *tuya_mem_heap_realloc(HEAP_HANDLE handle, void *ptr, unsigned int size)
{
    if (NULL == ptr) {
        return tuya_mem_heap_malloc(handle, size);
    }

    MEM_HeapBlock_t *old_block = (MEM_HeapBlock_t *)((unsigned long)(intptr_t)ptr - MEM_BLOCK_HEAD_SIZE);
    unsigned char *pdog = MEM_DOG_ADDR(old_block);

    if (*pdog != MEM_BLOCK_STAT_USE) {
        s_heap_ctx.dbg_output("[MEM DBG] realloc MEM_DEBUG_DOG_TAG err %p,size=%d\r\n", ptr, old_block->size);
        return NULL;
    }

    unsigned long new_size;
    size = size < 4 ? 4 : size;

    new_size = ALIGN_UP(size + 1) + MEM_BLOCK_HEAD_SIZE;
    if (new_size <= old_block->size) { // old buffer is big enough
        return ptr;
    }

    // alloc new buffer
    void *tmp = tuya_mem_heap_malloc(handle, size);
    if (NULL == tmp) {
        return NULL;
    }

    memcpy(tmp, ptr, old_block->size - MEM_BLOCK_HEAD_SIZE);
    tuya_mem_heap_free(handle, ptr);
    return tmp;
}

void tuya_mem_heap_free(HEAP_HANDLE handle, void *ptr)
{
    if (0 != handle) {
        MEM_Deallocate((MEM_Heap_t *)handle, ptr);
    } else {
        long idx = 0;
        MEM_Heap_t *pHeap = NULL;

        for (idx = 0; idx < MEM_HEAP_LIST_NUM; idx++) {
            pHeap = &mem_heap_list[idx];
            if (pHeap->size > 0) {
                if (((unsigned char *)ptr > pHeap->base) && ((unsigned char *)ptr < (pHeap->base + pHeap->size))) {
                    MEM_Deallocate(pHeap, ptr);
                    break;
                }
            } else {
                break;
            }
        }
    }
}

int tuya_mem_heap_available(HEAP_HANDLE handle)
{
    if (0 == handle) {
        return s_heap_free_size;
    } else {
        return ((MEM_Heap_t *)handle)->free;
    }
}

void tuya_mem_heap_state(HEAP_HANDLE handle, heap_state_t *state)
{
    if (NULL == state) {
        return;
    }

    MEM_Heap_t *pHeap = (MEM_Heap_t *)handle;

    if (0 == handle) {
        long idx = 0;

        state->free_size = s_heap_free_size;
        state->free_watermark = s_heap_free_size_watermark;

        for (idx = 0; idx < MEM_HEAP_LIST_NUM; idx++) {
            pHeap = &mem_heap_list[idx];
            if (pHeap->size > 0) {
                state->total_size += pHeap->size;
            } else {
                break;
            }
        }
    } else {
        state->total_size = pHeap->size;
        state->free_size = pHeap->free;
        state->free_watermark = pHeap->free_watermark;
    }
}

void *tuya_mem_heap_debug_malloc(HEAP_HANDLE handle, unsigned int size, char *filename, int line)
{
    if (0 != handle) {
        return MEM_AllocateDebug((MEM_Heap_t *)handle, size, filename, line);
    } else {
        long idx = 0;
        void *ptr = NULL;
        MEM_Heap_t *pHeap = NULL;

        for (idx = 0; idx < MEM_HEAP_LIST_NUM; idx++) {
            pHeap = &mem_heap_list[idx];
            if (pHeap->size > 0) {
                if (pHeap->free > (size + MEM_BLOCK_MIN_SIZE)) {
                    ptr = MEM_AllocateDebug((MEM_Heap_t *)pHeap, size, filename, line);
                    if (NULL != ptr) {
                        return ptr;
                    }
                }
            } else {
                break;
            }
        }

        return NULL;
    }
}

void *tuya_mem_heap_debug_calloc(HEAP_HANDLE handle, unsigned int size, char *filename, int line)
{
    void *ptr = tuya_mem_heap_debug_malloc(handle, size, filename, line);
    if (ptr) {
        memset(ptr, 0, size);
    }

    return ptr;
}

void *tuya_mem_heap_debug_realloc(HEAP_HANDLE handle, void *ptr, unsigned int size, char *filename, int line)
{
    void *tmp = tuya_mem_heap_debug_malloc(handle, size, filename, line);
    if (NULL == tmp) {
        return NULL;
    }

    memcpy(tmp, ptr, size);
    tuya_mem_heap_free(handle, ptr);
    return tmp;
}

int tuya_mem_heap_diagnose(HEAP_HANDLE handle)
{
    MEM_Heap_t *pMemHeap = (MEM_Heap_t *)handle;
    MEM_HeapStatus_t memst;

    if (0 != handle) {
        MEM_HeapStatus(pMemHeap, &memst);

        if (!memst.valid) {
            s_heap_ctx.dbg_output("[MEM DBG] SYS_MemStat !!!!! MEM MNG DAMAGED!!!!! \r\n");
        }

        s_heap_ctx.dbg_output("[MEM DBG] Heap size=%d, free=%d, free_largest=%d, malloc_block=%d, free_block=%d\r\n",
                              memst.size, memst.free, memst.free_largest, memst.used_block, memst.free_block);
    } else {
        long idx = 0;

        for (idx = 0; idx < MEM_HEAP_LIST_NUM; idx++) {
            pMemHeap = &mem_heap_list[idx];
            if (pMemHeap->size > 0) {
                MEM_HeapStatus(pMemHeap, &memst);

                if (!memst.valid) {
                    s_heap_ctx.dbg_output("[MEM DBG] SYS_MemStat !!!!! MEM MNG DAMAGED!!!!! \r\n");
                }

                s_heap_ctx.dbg_output(
                    "[MEM DBG] Heap size=%d, free=%d, free_largest=%d, malloc_block=%d, free_block=%d\r\n", memst.size,
                    memst.free, memst.free_largest, memst.used_block, memst.free_block);
            } else {
                break;
            }
        }
    }

    return 0;
}
