/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/**
 * @addtogroup OSKernel
 * @{
 *   @defgroup OSMempool Memory Pools
 *   @{
 */

#ifndef _OS_MEMPOOL_H_
#define _OS_MEMPOOL_H_

#include <stdbool.h>
#include "tuya_ble_linkqueue.h"
#include "tuya_ble_mbuf.h"
#include "ble_endian.h"

#ifdef __cplusplus
extern "C" {
#endif

// Dyna Ram Open
#define TUYA_USE_DYNA_RAM 1
// Max Dyna Num Limit
#define TUYA_DYNA_ALLOCATION_LIMIT 0
#define TUYA_USE_DYNA_RAM_FLAG     0x02

/* OS error enumerations */
enum runtime_os_error {
    OS_OK              = 0,
    OS_ENOMEM          = 1,
    OS_EINVAL          = 2,
    OS_INVALID_PARM    = 3,
    OS_MEM_NOT_ALIGNED = 4,
    OS_BAD_MUTEX       = 5,
    OS_TIMEOUT         = 6,
    OS_ERR_IN_ISR      = 7,  /* Function cannot be called from ISR */
    OS_ERR_PRIV        = 8,  /* Privileged access error */
    OS_NOT_STARTED     = 9,  /* OS must be started to call this function, but isn't */
    OS_ENOENT          = 10, /* No such thing */
    OS_EBUSY           = 11, /* Resource busy */
    OS_ERROR           = 12, /* Generic Error */
};

typedef enum runtime_os_error stats_error_t;

/**
 * A memory block structure. This simply contains a pointer to the free list
 * chain and is only used when the block is on the free list. When the block
 * has been removed from the free list the entire memory block is usable by the
 * caller.
 */
struct os_memblock {
    SLIST_ENTRY(os_memblock) mb_next;
};

/* XXX: Change this structure so that we keep the first address in the pool? */
/* XXX: add memory debug structure and associated code */
/* XXX: Change how I coded the SLIST_HEAD here. It should be named:
   SLIST_HEAD(,os_memblock) mp_head; */

/**
 * Memory pool
 */
struct os_mempool {
    /** Size of the memory blocks, in bytes. */
    uint32_t mp_block_size;
    /** The number of memory blocks. */
    uint16_t mp_num_blocks;
    /** The number of free blocks left */
    uint16_t mp_num_free;
    /** The lowest number of free blocks seen */
    uint16_t mp_min_free;
    /** Bitmap of OS_MEMPOOL_F_[...] values. */
    uint8_t mp_flags;
    /** Address of memory buffer used by pool */
    uint32_t mp_membuf_addr;
    STAILQ_ENTRY(os_mempool) mp_list;
    SLIST_HEAD(, os_memblock);
    /** Name for memory block */
    char *name;
};

/**
 * Indicates an extended mempool.  Address can be safely cast to
 * (struct os_mempool_ext *).
 */
#define OS_MEMPOOL_F_EXT 0x01

struct os_mempool_ext;

/**
 * Block put callback function.  If configured, this callback gets executed
 * whenever a block is freed to the corresponding extended mempool.  Note: The
 * os_memblock_put() function calls this callback instead of freeing the block
 * itself.  Therefore, it is the callback's responsibility to free the block
 * via a call to os_memblock_put_from_cb().
 *
 * @param ome                   The extended mempool that a block is being
 *                                  freed back to.
 * @param data                  The block being freed.
 * @param arg                   Optional argument configured along with the
 *                                  callback.
 *
 * @return                      Indicates whether the block was successfully
 *                                  freed.  A non-zero value should only be
 *                                  returned if the block was not successfully
 *                                  released back to its pool.
 */
typedef stats_error_t os_mempool_put_fn(struct os_mempool_ext *ome, void *data, void *arg);

struct os_mempool_ext {
    struct os_mempool mpe_mp;

    /* Callback that is executed immediately when a block is freed. */
    os_mempool_put_fn *mpe_put_cb;
    void              *mpe_put_arg;
};

#define OS_MEMPOOL_INFO_NAME_LEN (32)

/**
 * Information describing a memory pool, used to return OS information
 * to the management layer.
 */
struct os_mempool_info {
    /** Size of the memory blocks in the pool */
    int omi_block_size;
    /** Number of memory blocks in the pool */
    int omi_num_blocks;
    /** Number of free memory blocks */
    int omi_num_free;
    /** Minimum number of free memory blocks ever */
    int omi_min_free;
    /** Name of the memory pool */
    char omi_name[OS_MEMPOOL_INFO_NAME_LEN];
};

/**
 * Get information about the next system memory pool.
 *
 * @param mempool The current memory pool, or NULL if starting iteration.
 * @param info    A pointer to the structure to return memory pool information
 *                into.
 *
 * @return The next memory pool in the list to get information about, or NULL
 *         when at the last memory pool.
 */
struct os_mempool *os_mempool_info_get_next(struct os_mempool *, struct os_mempool_info *);

/*
 * To calculate size of the memory buffer needed for the pool. NOTE: This size
 * is NOT in bytes! The size is the number of os_membuf_t elements required for
 * the memory pool.
 */
#if (TY_HS_OS_MEMPOOL_GUARD)
/*
 * Leave extra 4 bytes of guard area at the end.
 */
#define OS_MEMPOOL_BLOCK_SZ(sz) ((sz) + sizeof(os_membuf_t))
#else
#define OS_MEMPOOL_BLOCK_SZ(sz) (sz)
#endif

#define OS_ALIGNMENT (4)

#if (OS_ALIGNMENT == 4)
typedef uint32_t os_membuf_t;
#elif (OS_ALIGNMENT == 8)
typedef uint64_t os_membuf_t;
#elif (OS_ALIGNMENT == 16)
typedef __uint128_t os_membuf_t;
#else
#error "Unhandled `OS_ALIGNMENT` for `os_membuf_t`"
#endif /* OS_ALIGNMENT == * */

#ifndef MIN_CMP
#define MIN_CMP(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX_CMP
#define MAX_CMP(a, b) ((a) > (b) ? (a) : (b))
#endif

#define OS_ALIGN(__n, __a)          ((((__n) & ((__a)-1)) == 0) ? (__n) : ((__n) + ((__a) - ((__n) & ((__a)-1)))))
#define OS_MEMPOOL_SIZE(n, blksize) ((((blksize) + ((OS_ALIGNMENT)-1)) / (OS_ALIGNMENT)) * (n))

/** Calculates the number of bytes required to initialize a memory pool. */
#define OS_MEMPOOL_BYTES(n, blksize) (sizeof(os_membuf_t) * OS_MEMPOOL_SIZE((n), (blksize)))

int mem_init_mbuf_pool(void *mem, struct os_mempool *mempool, struct os_mbuf_pool *mbuf_pool, int num_blocks,
                       int block_size, char *name);

/**
 * Specifies a function used as a callback.  Functions of this type allocate an
 * mbuf chain meant to hold a packet fragment.  The resulting mbuf must contain
 * a pkthdr.
 *
 * @param frag_size             The number of data bytes that the mbuf will
 *                                  eventually contain.
 * @param arg                   A generic parameter.
 *
 * @return                      An allocated mbuf chain on success;
 *                              NULL on failure.
 */
typedef struct os_mbuf *mem_frag_alloc_fn(uint16_t frag_size, void *arg);

struct os_mbuf *mem_split_frag(struct os_mbuf **om, uint16_t max_frag_sz, mem_frag_alloc_fn *alloc_cb, void *cb_arg);

/**
 * Initialize a memory pool.
 *
 * @param mp            Pointer to a pointer to a mempool
 * @param blocks        The number of blocks in the pool
 * @param blocks_size   The size of the block, in bytes.
 * @param membuf        Pointer to memory to contain blocks.
 * @param name          Name of the pool.
 *
 * @return stats_error_t
 */
stats_error_t os_mempool_init(struct os_mempool *mp, uint16_t blocks, uint32_t block_size, void *membuf, char *name);

/**
 * Initializes an extended memory pool.  Extended attributes (e.g., callbacks)
 * are not specified when this function is called; they are assigned manually
 * after initialization.
 *
 * @param mpe           The extended memory pool to initialize.
 * @param blocks        The number of blocks in the pool.
 * @param block_size    The size of each block, in bytes.
 * @param membuf        Pointer to memory to contain blocks.
 * @param name          Name of the pool.
 *
 * @return stats_error_t
 */
stats_error_t os_mempool_ext_init(struct os_mempool_ext *mpe, uint16_t blocks, uint32_t block_size, void *membuf,
                                  char *name);

/**
 * Removes the specified mempool from the list of initialized mempools.
 *
 * @param mp                    The mempool to unregister.
 *
 * @return                      0 on success;
 *                              OS_INVALID_PARM if the mempool is not
 *                                  registered.
 */
stats_error_t os_mempool_unregister(struct os_mempool *mp);

/**
 * Clears a memory pool.
 *
 * @param mp            The mempool to clear.
 *
 * @return stats_error_t
 */
stats_error_t os_mempool_clear(struct os_mempool *mp);

/**
 * Performs an integrity check of the specified mempool.  This function
 * attempts to detect memory corruption in the specified memory pool.
 *
 * @param mp                    The mempool to check.
 *
 * @return                      true if the memory pool passes the integrity
 *                                  check;
 *                              false if the memory pool is corrupt.
 */
bool os_mempool_is_sane(const struct os_mempool *mp);

/**
 * Checks if a memory block was allocated from the specified mempool.
 *
 * @param mp                    The mempool to check as parent.
 * @param block_addr            The memory block to check as child.
 *
 * @return                      0 if the block does not belong to the mempool;
 *                              1 if the block does belong to the mempool.
 */
int os_memblock_from(const struct os_mempool *mp, const void *block_addr);

/**
 * Get a memory block from a memory pool
 *
 * @param mp Pointer to the memory pool
 *
 * @return void* Pointer to block if available; NULL otherwise
 */
void *os_memblock_get(struct os_mempool *mp);

/**
 * Puts the memory block back into the pool, ignoring the put callback, if any.
 * This function should only be called from a put callback to free a block
 * without causing infinite recursion.
 *
 * @param mp Pointer to memory pool
 * @param block_addr Pointer to memory block
 *
 * @return stats_error_t
 */
stats_error_t os_memblock_put_from_cb(struct os_mempool *mp, void *block_addr);

/**
 * Puts the memory block back into the pool
 *
 * @param mp Pointer to memory pool
 * @param block_addr Pointer to memory block
 *
 * @return stats_error_t
 */
stats_error_t os_memblock_put(struct os_mempool *mp, void *block_addr);

/**
 * Puts the dyna memory block back into the pool.  The buffer must have been allocated via
 * tuya_ble_hci_dyna_buf_alloc,both internal and ext mem.
 *
 * @param buf                   The buffer to free.
 */
int os_dyna_memblock_put(struct os_mempool *pool, void *buf);

/**
 * Allocates a flat buffer of the specified type.
 *
 * @param type                  The type of buffer to allocate; one of the
 *                                  BLE_HCI_TRANS_BUF_[...] constants.
 *
 * @return                      The allocated buffer on success;
 *                              NULL on buffer exhaustion.
 */
uint8_t *tuya_ble_hci_buf_alloc(int type);

/**
 * Frees the specified flat buffer.  The buffer must have been allocated via
 * tuya_ble_hci_buf_alloc().
 *
 * @param buf                   The buffer to free.
 */
#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM == 0)
void tuya_ble_hci_buf_free(uint8_t *buf);
#else
void             tuya_ble_hci_buf_free(int type, uint8_t *buf);
#endif
/**
 * Frees the specified flat buffer.  The buffer must have been allocated via
 * tuya_ble_hci_dyna_buf_alloc but not ext mem
 *
 * @param buf                   The buffer to free.
 */
int tuya_ble_hci_mp_num_buf_free(struct os_mempool *pool, uint8_t *buf);

/**
 * Before send data into controller
 *
 * @param mp Pointer to memory pool
 * @param
 *
 * @return stats_error_t
 */
int tuya_ble_hs_acl_tx(struct os_mbuf *acl_pkt);

void tuya_ble_hci_buf_init(void);

stats_error_t os_dynamempool_init(struct os_mempool *mp, uint16_t blocks, uint32_t block_size, char *name,
                                  uint8_t flags);

#ifdef __cplusplus
}
#endif

#endif /* _OS_MEMPOOL_H_ */

/**
 *   @} OSMempool
 * @} OSKernel
 */
