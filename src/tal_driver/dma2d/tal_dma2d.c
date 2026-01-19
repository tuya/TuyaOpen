/**
 * @file tal_dma2d.c
 * @brief DMA2D (Direct Memory Access 2D) hardware abstraction layer
 *
 * This module provides a hardware abstraction layer for DMA2D operations,
 * including initialization, format conversion, memory copy, and completion
 * waiting. It manages DMA2D hardware resources with thread-safe operations
 * and supports multiple concurrent DMA2D contexts.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include <stdint.h>
#include <string.h>

#include "tal_dma2d.h"
#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#define TAL_DMA2D_MALLOC tal_psram_malloc
#define TAL_DMA2D_FREE   tal_psram_free
#else
#define TAL_DMA2D_MALLOC tal_malloc
#define TAL_DMA2D_FREE   tal_free
#endif

#define POLLING_INTERVAL_MS               300
#define HARDWARE_DMA2D_WAIT_TIMEOUT_MS    5000
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t    is_busy;
    uint8_t    is_wait_sem;
    SEM_HANDLE finish_sem;
} TAL_DMA2D_CTRL_T;

typedef struct {
    uint8_t              init_cnt;
    TAL_DMA2D_CTRL_T    *curr_sub_obj;
    MUTEX_HANDLE         mutex;
    TAL_DMA2D_CTRL_T     hardware_dma2d;
} TAL_DMA2D_T;

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static TAL_DMA2D_T sg_dma2d = {0};

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Initialize DMA2D control object
 * @param dma2d Pointer to DMA2D control structure
 * @return OPRT_OK on success, error code otherwise
 */
static OPERATE_RET __dma2d_obj_source_init(TAL_DMA2D_CTRL_T *dma2d)
{
    OPERATE_RET rt = OPRT_OK;

    if(NULL == dma2d) {
        return OPRT_INVALID_PARM;
    }

    TUYA_CALL_ERR_RETURN(tal_semaphore_create_init(&dma2d->finish_sem, 0, 1));
    dma2d->is_busy = false;
    dma2d->is_wait_sem = false;
    
    return OPRT_OK;
}

/**
 * @brief Deinitialize DMA2D control object
 * @param dma2d Pointer to DMA2D control structure
 * @return OPRT_OK on success, error code otherwise
 */
static OPERATE_RET __dma2d_obj_source_deinit(TAL_DMA2D_CTRL_T *dma2d)
{
    if(NULL == dma2d) {
        return OPRT_INVALID_PARM;
    }

    if(dma2d->finish_sem) {
        tal_semaphore_release(dma2d->finish_sem);
        dma2d->finish_sem = NULL;
    }
    dma2d->is_busy = false;
    dma2d->is_wait_sem = false;

    return OPRT_OK;
}

/**
 * @brief Release DMA2D source and notify waiting threads
 * @param dma2d Pointer to DMA2D control structure
 */
static void __dma2d_release_source(TAL_DMA2D_CTRL_T *dma2d)
{
    if(NULL == dma2d) {
        return;
    }

    if(dma2d->is_wait_sem && dma2d->finish_sem) {
        tal_semaphore_post(dma2d->finish_sem);
    }

    dma2d->is_busy = false;
}

/**
 * @brief Wait for DMA2D source to be released
 * @param dma2d Pointer to DMA2D control structure
 * @param timeout_ms Timeout in milliseconds, or SEM_WAIT_FOREVER for infinite wait
 * @return true if source was released, false on timeout
 */
static bool __dma2d_wait_source_release(TAL_DMA2D_CTRL_T *dma2d, uint32_t timeout_ms)
{
    uint32_t elapsed_time = 0;

    if(NULL == dma2d) {
        return true;
    }

    if(false == dma2d->is_busy) {
        return true;
    }

    dma2d->is_wait_sem = true;

    while(dma2d->is_busy) {
        if(timeout_ms != SEM_WAIT_FOREVER && elapsed_time >= timeout_ms) {
            break;
        }

        tal_semaphore_wait(dma2d->finish_sem, POLLING_INTERVAL_MS);

        elapsed_time += POLLING_INTERVAL_MS;
    }

    dma2d->is_wait_sem = false;

    return !dma2d->is_busy;
}

/**
 * @brief DMA2D interrupt callback function
 * @brief Called when DMA2D operation completes, releases hardware and context resources
 * @param type Interrupt type enumeration
 * @param args Callback arguments (unused)
 */
static void __dma2d_irq_callback(TUYA_DMA2D_IRQ_E type, void *args)
{
    TAL_DMA2D_CTRL_T *sub_obj = sg_dma2d.curr_sub_obj;

    sg_dma2d.curr_sub_obj = NULL;
    __dma2d_release_source(&sg_dma2d.hardware_dma2d);

    __dma2d_release_source(sub_obj);

}

/**
 * @brief Initialize DMA2D handle
 * @brief Allocates and initializes a new DMA2D context with semaphore for completion signaling
 * @param handle Pointer to store the DMA2D handle
 * @return OPRT_OK on success, OPRT_MALLOC_FAILED on memory allocation failure,
 *         or other error codes on initialization failure
 */
OPERATE_RET tal_dma2d_init(TAL_DMA2D_HANDLE_T *handle)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_DMA2D_BASE_CFG_T cfg = {0};
    TAL_DMA2D_CTRL_T *context = NULL;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    context = TAL_DMA2D_MALLOC(sizeof(TAL_DMA2D_CTRL_T));
    if (context == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(context, 0, sizeof(TAL_DMA2D_CTRL_T));

    TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&context->finish_sem, 0, 1), __ERR);

    if (sg_dma2d.init_cnt == 0) {
        TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_dma2d.mutex), __ERR);
        TUYA_CALL_ERR_GOTO(__dma2d_obj_source_init(&sg_dma2d.hardware_dma2d), __ERR);

        cfg.cb = __dma2d_irq_callback;
        cfg.arg = NULL;
        TUYA_CALL_ERR_GOTO(tkl_dma2d_init(&cfg), __ERR);
    }
    if (sg_dma2d.init_cnt < UINT8_MAX) {
        sg_dma2d.init_cnt++;
    }

    *handle = context;

    return rt;
__ERR:
    tal_dma2d_deinit(handle);

    return rt;
}

/**
 * @brief Deinitialize DMA2D handle
 * @brief Releases DMA2D context resources and decrements initialization counter.
 *        If this is the last handle, deinitializes the hardware DMA2D instance.
 * @param handle DMA2D handle to deinitialize
 * @return OPRT_OK on success, OPRT_INVALID_PARM if handle is invalid
 */
OPERATE_RET tal_dma2d_deinit(TAL_DMA2D_HANDLE_T handle)
{
    OPERATE_RET rt = OPRT_OK;
    TAL_DMA2D_CTRL_T *dam2d_obj = NULL;

    // Free handle context
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    dam2d_obj = (TAL_DMA2D_CTRL_T *)handle;

    __dma2d_obj_source_deinit(dam2d_obj);

    TAL_DMA2D_FREE(dam2d_obj);

    // Free dma2d instance
    if (NULL == sg_dma2d.mutex) {
        return rt;
    }

    tal_mutex_lock(sg_dma2d.mutex);

    if (sg_dma2d.init_cnt > 0) {
        sg_dma2d.init_cnt--;
        if (sg_dma2d.init_cnt == 0) {
            sg_dma2d.curr_sub_obj = NULL;
            __dma2d_obj_source_deinit(&sg_dma2d.hardware_dma2d);
        }
    }

    tal_mutex_unlock(sg_dma2d.mutex);

    tal_mutex_release(sg_dma2d.mutex);
    sg_dma2d.mutex = NULL;

    return rt;
}

/**
 * @brief Convert frame format using DMA2D hardware
 * @brief Waits for hardware to be available, then starts format conversion operation
 * @param handle DMA2D handle
 * @param src Pointer to source frame information structure
 * @param dst Pointer to destination frame information structure
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid,
 *         OPRT_COM_ERROR if hardware is busy or conversion fails
 */
OPERATE_RET tal_dma2d_convert(TAL_DMA2D_HANDLE_T handle, TKL_DMA2D_FRAME_INFO_T *src, TKL_DMA2D_FRAME_INFO_T *dst)
{
    OPERATE_RET rt = OPRT_OK;
    TAL_DMA2D_CTRL_T *dam2d_obj = NULL;
    bool is_source_release = false;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    dam2d_obj = (TAL_DMA2D_CTRL_T *)handle;

    tal_mutex_lock(sg_dma2d.mutex);

    is_source_release = __dma2d_wait_source_release(&sg_dma2d.hardware_dma2d, HARDWARE_DMA2D_WAIT_TIMEOUT_MS);
    if(is_source_release == false) {
        PR_ERR("dma2d hardware busy timeout");
        rt = OPRT_COM_ERROR;
        goto __ERR;
    }

    sg_dma2d.curr_sub_obj = dam2d_obj;

    sg_dma2d.hardware_dma2d.is_busy = true;
    
    dam2d_obj->is_busy = true;

    rt = tkl_dma2d_convert(src, dst);
    if (OPRT_OK != rt) {
        PR_ERR("tkl_dma2d_convert failed, rt: %d", rt);
        sg_dma2d.hardware_dma2d.is_busy = false;
        dam2d_obj->is_busy = false;
        goto __ERR;
    }

    tal_mutex_unlock(sg_dma2d.mutex);

    return rt;

__ERR:
    sg_dma2d.curr_sub_obj = NULL;

    tal_mutex_unlock(sg_dma2d.mutex);

    return rt;
}

/**
 * @brief Copy memory using DMA2D hardware
 * @brief Waits for hardware to be available, then starts memory copy operation
 * @param handle DMA2D handle
 * @param src Pointer to source frame information structure
 * @param dst Pointer to destination frame information structure
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid,
 *         OPRT_COM_ERROR if hardware is busy or copy operation fails
 */
OPERATE_RET tal_dma2d_memcpy(TAL_DMA2D_HANDLE_T handle, TKL_DMA2D_FRAME_INFO_T *src, TKL_DMA2D_FRAME_INFO_T *dst)
{
    OPERATE_RET rt = OPRT_OK;
    TAL_DMA2D_CTRL_T *dam2d_obj = NULL;
    bool is_source_release = false;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    dam2d_obj = (TAL_DMA2D_CTRL_T *)handle;

    tal_mutex_lock(sg_dma2d.mutex);

    is_source_release = __dma2d_wait_source_release(&sg_dma2d.hardware_dma2d, HARDWARE_DMA2D_WAIT_TIMEOUT_MS);
    if(is_source_release == false) {
        PR_ERR("dma2d hardware busy timeout");
        rt = OPRT_COM_ERROR;
        goto __ERR;
    }

    sg_dma2d.curr_sub_obj = dam2d_obj;

    sg_dma2d.hardware_dma2d.is_busy = true;

    dam2d_obj->is_busy = true;

    rt = tkl_dma2d_memcpy(src, dst);
    if (OPRT_OK != rt) {
        PR_ERR("tkl_dma2d_memcpy failed, rt: %d", rt);
        sg_dma2d.hardware_dma2d.is_busy = false;   
        dam2d_obj->is_busy = false;
        goto __ERR;
    }

    tal_mutex_unlock(sg_dma2d.mutex);

    return rt;

__ERR:
    sg_dma2d.curr_sub_obj = NULL;

    tal_mutex_unlock(sg_dma2d.mutex);

    return rt;
}

/**
 * @brief Wait for DMA2D operation to finish
 * @brief Waits for the DMA2D operation associated with the handle to complete
 *        using polling mechanism with configurable interval
 * @param handle DMA2D handle
 * @param timeout_ms Timeout in milliseconds
 * @return OPRT_OK on success, OPRT_INVALID_PARM if handle is invalid,
 *         OPRT_COM_ERROR if timeout occurs
 */
OPERATE_RET tal_dma2d_wait_finish(TAL_DMA2D_HANDLE_T handle, uint32_t timeout_ms)
{
    OPERATE_RET rt = OPRT_OK;
    TAL_DMA2D_CTRL_T *dam2d_obj = NULL;
    bool is_source_release = false;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    dam2d_obj = (TAL_DMA2D_CTRL_T *)handle;

    is_source_release = __dma2d_wait_source_release(dam2d_obj, timeout_ms);
    if(is_source_release == false) {
        PR_ERR("wait dma2d finish timeout");
        rt = OPRT_COM_ERROR;
    }

    return rt;
}
