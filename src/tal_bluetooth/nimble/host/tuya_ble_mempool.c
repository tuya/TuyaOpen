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

#include "tuya_ble_mbuf.h"
#include "tuya_ble_mempool.h"
#include "tuya_hs_port.h"
#include "ble_trace_api.h"
#include "tuya_ble_hci.h"
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "tuya_ble_cfg.h"

#if !(TY_HS_OS_SYSVIEW_TRACE_MEMPOOL)
#define OS_TRACE_DISABLE_FILE_API
#endif

#define os_mempool_poison(mp, start)
#define os_mempool_poison_check(mp, start)
#define os_mempool_guard(mp, start)
#define os_mempool_guard_check(mp, start)

#define OS_MEM_TRUE_BLOCK_SIZE(bsize)       OS_ALIGN(bsize, OS_ALIGNMENT)
#define OS_MEMPOOL_TRUE_BLOCK_SIZE(mp)      OS_MEM_TRUE_BLOCK_SIZE(mp->mp_block_size)

#define SYSINIT_MSYS_1_MEMBLOCK_SIZE        OS_ALIGN((TY_HS_MSYS_1_BLOCK_SIZE), 4)
#define SYSINIT_MSYS_1_MEMPOOL_SIZE         OS_MEMPOOL_SIZE((TY_HS_MSYS_1_BLOCK_COUNT), SYSINIT_MSYS_1_MEMBLOCK_SIZE)

STAILQ_HEAD(, os_mempool)                   g_os_mempool_list;
static STAILQ_HEAD(, os_mbuf_pool)          g_msys_pool_list = STAILQ_HEAD_INITIALIZER(g_msys_pool_list);

#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM==0)
static os_membuf_t                          os_msys_1_data[SYSINIT_MSYS_1_MEMPOOL_SIZE];
#endif

static struct os_mbuf_pool                  os_msys_1_mbuf_pool;
static struct os_mempool                    os_msys_1_mempool;

int mem_init_mbuf_pool(void *mem, struct os_mempool *mempool, struct os_mbuf_pool *mbuf_pool, int num_blocks,
                    int block_size, char *name)
{
    int rc;

    rc = os_mempool_init(mempool, num_blocks, block_size, mem, name);
    if (rc != 0) {
        return rc;
    }

    rc = os_mbuf_pool_init(mbuf_pool, mempool, block_size, num_blocks);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int mem_dyna_init_mbuf_pool(struct os_mempool *mempool, struct os_mbuf_pool *mbuf_pool, int num_blocks,
                    int block_size, char *name)
{
    int rc;

    rc = os_dynamempool_init(mempool, num_blocks, block_size, name, TUYA_USE_DYNA_RAM_FLAG);
    if (rc != 0) {
        return rc;
    }

    rc = os_dyna_mbuf_pool_init(mbuf_pool, mempool, block_size, 
                            num_blocks);
    if (rc != 0) {
        return rc;
    }

    return 0;
}


/*
 * Splits an appropriately-sized fragment from the front of an mbuf chain, as
 * neeeded.  If the length of the mbuf chain greater than specified maximum
 * fragment size, a new mbuf is allocated, and data is moved from the source
 * mbuf to the new mbuf.  If the mbuf chain is small enough to fit in a single
 * fragment, the source mbuf itself is returned unmodified, and the suplied
 * pointer is set to NULL.
 *
 * This function is expected to be called in a loop until the entire mbuf chain
 * has been consumed.  For example:
 *
 *     struct os_mbuf *frag;
 *     struct os_mbuf *rsp;
 *     // [...]
 *     while (rsp != NULL) {
 *         frag = mem_split_frag(&rsp, get_mtu(), frag_alloc, NULL);
 *         if (frag == NULL) {
 *             os_mbuf_free_chain(rsp);
 *             return OS_ENOMEM;
 *         }
 *         send_packet(frag)
 *     }
 *
 * @param om                    The packet to fragment.  Upon fragmentation,
 *                                  this mbuf is adjusted such that the
 *                                  fragment data is removed.  If the packet
 *                                  constitutes a single fragment, this gets
 *                                  set to NULL on success.
 * @param max_frag_sz           The maximum payload size of a fragment.
 *                                  Typically this is the MTU of the
 *                                  connection.
 * @param alloc_cb              Points to a function that allocates an mbuf to
 *                                  hold a fragment.  This function gets called
 *                                  before the source mbuf chain is modified,
 *                                  so it can safely inspect it.
 * @param cb_arg                Generic parameter that gets passed to the
 *                                  callback function.
 *
 * @return                      The next fragment to send on success;
 *                              NULL on failure.
 */
struct os_mbuf *mem_split_frag(struct os_mbuf **om, uint16_t max_frag_sz, mem_frag_alloc_fn *alloc_cb, void *cb_arg)
{
    struct os_mbuf *frag;
    int rc;

    if (OS_MBUF_PKTLEN(*om) <= max_frag_sz) {
        /* Final fragment. */
        frag = *om;
        *om = NULL;
        return frag;
    }

    /* Packet needs to be split.  Allocate a new buffer for the fragment. */
    frag = alloc_cb(max_frag_sz, cb_arg);
    if (frag == NULL) {
        goto err;
    }

    /* Move data from the front of the packet into the fragment mbuf. */
    rc = os_mbuf_appendfrom(frag, *om, 0, max_frag_sz);
    if (rc != 0) {
        goto err;
    }
    os_mbuf_adj(*om, max_frag_sz);
    /* Free unused portion of of source mbuf chain, if possible. */
    *om = os_mbuf_trim_front(*om);
    return frag;

err:
    os_mbuf_free_chain(frag);
    return NULL;
}

#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM==0)
static void os_msys_init_once(void *data, struct os_mempool *mempool, struct os_mbuf_pool *mbuf_pool, int block_count, int block_size, char *name)
{
    int rc;

    rc = mem_init_mbuf_pool(data, mempool, mbuf_pool, block_count, block_size, name);
    BLE_PANIC_ASSERT(rc == 0);

    rc = os_msys_register(mbuf_pool);
    BLE_PANIC_ASSERT(rc == 0);
}
#endif

static void os_dyna_msys_init_once(struct os_mempool *mempool, struct os_mbuf_pool *mbuf_pool, int block_count, int block_size, char *name)
{
    int rc;

    rc = mem_dyna_init_mbuf_pool(mempool, mbuf_pool, block_count, block_size, name);
    BLE_PANIC_ASSERT(rc == 0);

    rc = os_msys_register(mbuf_pool);
    BLE_PANIC_ASSERT(rc == 0);
}

void ty_os_msys_init(void)
{
    os_msys_reset();
    
#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM==0)
    os_msys_init_once(os_msys_1_data,
                      &os_msys_1_mempool,
                      &os_msys_1_mbuf_pool,
                      (TY_HS_MSYS_1_BLOCK_COUNT),
                      SYSINIT_MSYS_1_MEMBLOCK_SIZE,
                      "msys_1");
#else
    os_dyna_msys_init_once(&os_msys_1_mempool,
                      &os_msys_1_mbuf_pool,
                      (TY_HS_MSYS_1_BLOCK_COUNT),
                      SYSINIT_MSYS_1_MEMBLOCK_SIZE,
                      "msys_1");
#endif
}

static stats_error_t os_mempool_init_internal(struct os_mempool *mp, uint16_t blocks,
                         uint32_t block_size, void *membuf, char *name,
                         uint8_t flags)
{
    int true_block_size;
    int i;
    uint8_t *block_addr;
    struct os_memblock *block_ptr;

    /* Check for valid parameters */
    if (!mp || (block_size == 0)) {
        return OS_INVALID_PARM;
    }

    if ((!membuf) && (blocks != 0)) {
        return OS_INVALID_PARM;
    }

    if (membuf != NULL) {
        /* Blocks need to be sized properly and memory buffer should be
         * aligned
         */
        if (((uint32_t)(uintptr_t)membuf & (OS_ALIGNMENT - 1)) != 0) {
            return OS_MEM_NOT_ALIGNED;
        }
    }

    /* Initialize the memory pool structure */
    mp->mp_block_size = block_size;
    mp->mp_num_free = blocks;
    mp->mp_min_free = blocks;
    mp->mp_flags = flags;
    mp->mp_num_blocks = blocks;
    mp->mp_membuf_addr = (uint32_t)(uintptr_t)membuf;
    mp->name = name;
    SLIST_FIRST(mp) = membuf;

    if (blocks > 0) {
        os_mempool_poison(mp, membuf);
        os_mempool_guard(mp, membuf);
        true_block_size = OS_MEMPOOL_TRUE_BLOCK_SIZE(mp);
        // PR_DEBUG("INIT MEM BLOCK:%s,block_size %d,num block %d,start addr %x\r\n",mp->name, block_size, blocks, membuf);
        /* Chain the memory blocks to the free list */
        block_addr = (uint8_t *)membuf;
        block_ptr = (struct os_memblock *)block_addr;
        for (i = 1; i < blocks; i++) {
            block_addr += true_block_size;
            os_mempool_poison(mp, block_addr);
            os_mempool_guard(mp, block_addr);
            SLIST_NEXT(block_ptr, mb_next) = (struct os_memblock *)block_addr;
            block_ptr = (struct os_memblock *)block_addr;
            // PR_DEBUG("HCI EVENT INIT:%x,%d,%d \r\n", block_ptr, mp->mp_num_free, i);
        }
        /* Last one in the list should be NULL */
        SLIST_NEXT(block_ptr, mb_next) = NULL;
    }

    STAILQ_INSERT_TAIL(&g_os_mempool_list, mp, mp_list);

    return OS_OK;
}

stats_error_t os_mempool_init(struct os_mempool *mp, uint16_t blocks, uint32_t block_size, void *membuf, char *name)
{
    return os_mempool_init_internal(mp, blocks, block_size, membuf, name, 0);
}

stats_error_t os_dynamempool_init(struct os_mempool *mp, uint16_t blocks, uint32_t block_size, char *name, uint8_t flags)
{
    // int true_block_size;
    // int i;
    // uint8_t *block_addr;

    /* Check for valid parameters */
    if (!mp || (block_size == 0)) {
        return OS_INVALID_PARM;
    }

    /* Initialize the memory pool structure */
    mp->mp_block_size = block_size;
    mp->mp_num_free = blocks;
    mp->mp_min_free = blocks;
    mp->mp_flags = flags;
    mp->mp_num_blocks = blocks;
    mp->name = name;
    mp->mp_membuf_addr = 0;

    return OS_OK;
}

stats_error_t os_mempool_ext_init(struct os_mempool_ext *mpe, uint16_t blocks,
                    uint32_t block_size, void *membuf, char *name)
{
    stats_error_t rc;
#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM==0)
    rc = os_mempool_init_internal(&mpe->mpe_mp, blocks, block_size, membuf,
                                  name, OS_MEMPOOL_F_EXT);
    if (rc != 0) {
        return rc;
    }
#else
    rc = os_dynamempool_init(&mpe->mpe_mp, blocks, block_size, 
                    name, OS_MEMPOOL_F_EXT);
    if (rc != 0) {
        return rc;
    }
#endif
    mpe->mpe_put_cb = NULL;
    mpe->mpe_put_arg = NULL;

    return OS_OK;
}

stats_error_t os_mempool_unregister(struct os_mempool *mp)
{
    struct os_mempool *prev;
    struct os_mempool *next;
    struct os_mempool *cur;

    /* Remove the mempool from the global stailq.  This is done manually rather
     * than with `STAILQ_REMOVE` to allow for a graceful failure if the mempool
     * isn't found.
     */

    prev = NULL;
    STAILQ_FOREACH(cur, &g_os_mempool_list, mp_list) {
        if (cur == mp) {
            break;
        }
        prev = cur;
    }

    if (cur == NULL) {
        return OS_INVALID_PARM;
    }

    if (prev == NULL) {
        STAILQ_REMOVE_HEAD(&g_os_mempool_list, mp_list);
    } else {
        next = STAILQ_NEXT(cur, mp_list);
        if (next == NULL) {
            g_os_mempool_list.stqh_last = &STAILQ_NEXT(prev, mp_list);
        }

        STAILQ_NEXT(prev, mp_list) = next;
    }

    return OS_OK;
}

stats_error_t os_mempool_clear(struct os_mempool *mp)
{
    struct os_memblock *block_ptr;
    int true_block_size;
    uint8_t *block_addr;
    uint16_t blocks;

    if (!mp) {
        return OS_INVALID_PARM;
    }

    true_block_size = OS_MEMPOOL_TRUE_BLOCK_SIZE(mp);

    /* cleanup the memory pool structure */
    mp->mp_num_free = mp->mp_num_blocks;
    mp->mp_min_free = mp->mp_num_blocks;
    os_mempool_poison(mp, (void *)mp->mp_membuf_addr);
    os_mempool_guard(mp, (void *)mp->mp_membuf_addr);
    SLIST_FIRST(mp) = (void *)(uintptr_t)mp->mp_membuf_addr;

    /* Chain the memory blocks to the free list */
    block_addr = (uint8_t *)(uintptr_t)mp->mp_membuf_addr;
    block_ptr = (struct os_memblock *)block_addr;
    blocks = mp->mp_num_blocks;

    while (blocks > 1) {
        block_addr += true_block_size;
        os_mempool_poison(mp, block_addr);
        os_mempool_guard(mp, block_addr);
        SLIST_NEXT(block_ptr, mb_next) = (struct os_memblock *)block_addr;
        block_ptr = (struct os_memblock *)block_addr;
        --blocks;
    }

    /* Last one in the list should be NULL */
    SLIST_NEXT(block_ptr, mb_next) = NULL;

    return OS_OK;
}

bool os_mempool_is_sane(const struct os_mempool *mp)
{
    struct os_memblock *block;

    /* Verify that each block in the free list belongs to the mempool. */
    SLIST_FOREACH(block, mp, mb_next) {
        if (!os_memblock_from(mp, block)) {
            return false;
        }
        os_mempool_poison_check(mp, block);
        os_mempool_guard_check(mp, block);
    }

    return true;
}

int os_memblock_from(const struct os_mempool *mp, const void *block_addr)
{
    uint32_t true_block_size;
    uintptr_t baddr32;
    uint32_t end;

    ble_static_assert(sizeof block_addr == sizeof baddr32,
                  "Pointer to void must be 32-bits.");

    baddr32 = (uint32_t)(uintptr_t)block_addr;
    true_block_size = OS_MEMPOOL_TRUE_BLOCK_SIZE(mp);
    end = mp->mp_membuf_addr + (mp->mp_num_blocks * true_block_size);

    /* Check that the block is in the memory buffer range. */
    if ((baddr32 < mp->mp_membuf_addr) || (baddr32 >= end)) {
        return 0;
    }

    /* All freed blocks should be on true block size boundaries! */
    if (((baddr32 - mp->mp_membuf_addr) % true_block_size) != 0) {
        return 0;
    }

    return 1;
}

void *os_memblock_get(struct os_mempool *mp)
{
    struct os_memblock *block;

    os_trace_api_u32(OS_TRACE_ID_MEMBLOCK_GET, (uint32_t)(uintptr_t)mp);

#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM==1)
    if(mp->mp_flags > 0) {
        PR_ERR("ERR IN MEM GET :%s", mp->name);
    }
#endif
    /* Check to make sure they passed in a memory pool (or something) */
    block = NULL;
    if (mp) {
        tuya_ble_hs_enter_critical();
        /* Check for any free */
        if (mp->mp_num_free) {
            /* Get a free block */
            block = SLIST_FIRST(mp);

            /* Set new free list head */
            SLIST_FIRST(mp) = SLIST_NEXT(block, mb_next);

            /* Decrement number free by 1 */
            mp->mp_num_free--;
            if (mp->mp_min_free > mp->mp_num_free) {
                mp->mp_min_free = mp->mp_num_free;
            }
        }
        tuya_ble_hs_exit_critical();

        if (block) {
            os_mempool_poison_check(mp, block);
            os_mempool_guard_check(mp, block);
        }
    }

    os_trace_api_ret_u32(OS_TRACE_ID_MEMBLOCK_GET, (uint32_t)(uintptr_t)block);
    // PR_DEBUG("M BLOCK GET:%x , %x, %x, %s ", block, mp->mp_block_size, mp->mp_num_free, mp->name);
    return (void *)block;
}

stats_error_t os_memblock_put_from_cb(struct os_mempool *mp, void *block_addr)
{
    struct os_memblock *block;

    os_trace_api_u32x2(OS_TRACE_ID_MEMBLOCK_PUT_FROM_CB, (uint32_t)(uintptr_t)mp,
                       (uint32_t)(uintptr_t)block_addr);

    os_mempool_guard_check(mp, block_addr);
    os_mempool_poison(mp, block_addr);

    block = (struct os_memblock *)block_addr;
    tuya_ble_hs_enter_critical();

    /* Chain current free list pointer to this block; make this block head */
    SLIST_NEXT(block, mb_next) = SLIST_FIRST(mp);
    SLIST_FIRST(mp) = block;

    /* XXX: Should we check that the number free <= number blocks? */
    /* Increment number free */
    mp->mp_num_free++;

    tuya_ble_hs_exit_critical();

    os_trace_api_ret_u32(OS_TRACE_ID_MEMBLOCK_PUT_FROM_CB, (uint32_t)OS_OK);

    return OS_OK;
}

stats_error_t os_memblock_put(struct os_mempool *mp, void *block_addr)
{
    struct os_mempool_ext *mpe;
    stats_error_t ret;
#if (TY_HS_OS_MEMPOOL_CHECK)
    struct os_memblock *block;
#endif
    // PR_DEBUG("M BLOCK PUT:%x , %x, %x, %s ", block_addr, mp->mp_block_size, mp->mp_num_free, mp->name);
    os_trace_api_u32x2(OS_TRACE_ID_MEMBLOCK_PUT, (uint32_t)(uintptr_t)mp,
                       (uint32_t)(uintptr_t)block_addr);

    /* Make sure parameters are valid */
    if ((mp == NULL) || (block_addr == NULL)) {
        ret = OS_INVALID_PARM;
        goto done;
    }

#if (TY_HS_OS_MEMPOOL_CHECK)
    /* Check that the block we are freeing is a valid block! */
    TUYA_HS_ASSERT(os_memblock_from(mp, block_addr));

    /*
     * Check for duplicate free.
     */
    SLIST_FOREACH(block, mp, mb_next) {
        TUYA_HS_ASSERT(block != (struct os_memblock *)block_addr);
    }
#endif
    /* If this is an extended mempool with a put callback, call the callback
     * instead of freeing the block directly.
     */
#if defined(TUYA_USE_DYNA_RAM) && (TUYA_USE_DYNA_RAM==1)
    if(mp->mp_flags > 0) {
        PR_ERR("ERR IN MEM PUT :%s", mp->name);
    }
#endif
    if (mp->mp_flags & OS_MEMPOOL_F_EXT) {
        mpe = (struct os_mempool_ext *)mp;
        if (mpe->mpe_put_cb != NULL) {
            ret = mpe->mpe_put_cb(mpe, block_addr, mpe->mpe_put_arg);
            goto done;
        }
    }

    /* No callback; free the block. */
    ret = os_memblock_put_from_cb(mp, block_addr);

done:
    os_trace_api_ret_u32(OS_TRACE_ID_MEMBLOCK_PUT, (uint32_t)ret);
    return ret;
}

struct os_mempool *os_mempool_info_get_next(struct os_mempool *mp, struct os_mempool_info *omi)
{
    struct os_mempool *cur;

    if (mp == NULL) {
        cur = STAILQ_FIRST(&g_os_mempool_list);
    } else {
        cur = STAILQ_NEXT(mp, mp_list);
    }

    if (cur == NULL) {
        return (NULL);
    }

    omi->omi_block_size = cur->mp_block_size;
    omi->omi_num_blocks = cur->mp_num_blocks;
    omi->omi_num_free = cur->mp_num_free;
    omi->omi_min_free = cur->mp_min_free;
    omi->omi_name[0] = '\0';
    strncat(omi->omi_name, cur->name, sizeof(omi->omi_name) - 1);

    return (cur);
}

void os_mempool_module_init(void)
{
    STAILQ_INIT(&g_os_mempool_list);
}
