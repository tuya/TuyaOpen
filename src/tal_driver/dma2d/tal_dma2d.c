/**
 * @file tal_dma2d.c
 * @brief tal_dma2d module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */
#include <stdint.h>
#include <string.h>

#include "tal_dma2d.h"
#include "tal_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TAL_DMA2D_MALLOC tal_malloc
#define TAL_DMA2D_FREE   tal_free

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t init_cnt;
    SEM_HANDLE finish_sem;
    MUTEX_HANDLE mutex;
} TAL_DMA2D_T;

typedef struct {
    uint8_t need_wait_finish;
    SEM_HANDLE finish_sem;
} TAL_DMA2D_HANDLE_CONTEXT_T;

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
static void __dma2d_irq_callback(TUYA_DMA2D_IRQ_E type, void *args)
{
    TAL_DMA2D_T *dma2d = (TAL_DMA2D_T *)args;

    if (dma2d->finish_sem) {
        tal_semaphore_post(dma2d->finish_sem);
    }

    return;
}

OPERATE_RET tal_dma2d_init(TAL_DMA2D_HANDLE_T *handle)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_DMA2D_BASE_CFG_T cfg = {0};
    TAL_DMA2D_HANDLE_CONTEXT_T *context = NULL;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    context = TAL_DMA2D_MALLOC(sizeof(TAL_DMA2D_HANDLE_CONTEXT_T));
    if (context == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    memset(context, 0, sizeof(TAL_DMA2D_HANDLE_CONTEXT_T));

    TUYA_CALL_ERR_GOTO(tal_semaphore_create_init(&context->finish_sem, 0, 1), __ERR);

    if (sg_dma2d.init_cnt == 0) {
        TUYA_CALL_ERR_GOTO(tal_mutex_create_init(&sg_dma2d.mutex), __ERR);

        cfg.cb = __dma2d_irq_callback;
        cfg.arg = &sg_dma2d;
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

OPERATE_RET tal_dma2d_deinit(TAL_DMA2D_HANDLE_T handle)
{
    OPERATE_RET rt = OPRT_OK;
    TAL_DMA2D_HANDLE_CONTEXT_T *context = NULL;

    // Free handle context
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    context = (TAL_DMA2D_HANDLE_CONTEXT_T *)handle;

    if (context->finish_sem) {
        tal_semaphore_release(context->finish_sem);
        context->finish_sem = NULL;
    }
    TAL_DMA2D_FREE(context);

    // Free dma2d instance
    if (NULL == sg_dma2d.mutex) {
        return rt;
    }

    tal_mutex_lock(sg_dma2d.mutex);

    if (sg_dma2d.init_cnt > 0) {
        sg_dma2d.init_cnt--;
        if (sg_dma2d.init_cnt == 0) {
            sg_dma2d.finish_sem = NULL;
            tkl_dma2d_deinit();
        }
    }

    tal_mutex_unlock(sg_dma2d.mutex);

    tal_mutex_release(sg_dma2d.mutex);
    sg_dma2d.mutex = NULL;

    return rt;
}

OPERATE_RET tal_dma2d_convert(TAL_DMA2D_HANDLE_T handle, TKL_DMA2D_FRAME_INFO_T *src, TKL_DMA2D_FRAME_INFO_T *dst)
{
    OPERATE_RET rt = OPRT_OK;
    TAL_DMA2D_HANDLE_CONTEXT_T *context = NULL;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    context = (TAL_DMA2D_HANDLE_CONTEXT_T *)handle;

    tal_mutex_lock(sg_dma2d.mutex);

    sg_dma2d.finish_sem = context->finish_sem;
    TUYA_CALL_ERR_GOTO(tkl_dma2d_convert(src, dst), __ERR);
    context->need_wait_finish = true;

    return rt;

__ERR:
    sg_dma2d.finish_sem = NULL;

    tal_mutex_unlock(sg_dma2d.mutex);

    return rt;
}

OPERATE_RET tal_dma2d_memcpy(TAL_DMA2D_HANDLE_T handle, TKL_DMA2D_FRAME_INFO_T *src, TKL_DMA2D_FRAME_INFO_T *dst)
{
    OPERATE_RET rt = OPRT_OK;
    TAL_DMA2D_HANDLE_CONTEXT_T *context = NULL;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    context = (TAL_DMA2D_HANDLE_CONTEXT_T *)handle;

    tal_mutex_lock(sg_dma2d.mutex);

    sg_dma2d.finish_sem = context->finish_sem;
    TUYA_CALL_ERR_GOTO(tkl_dma2d_memcpy(src, dst), __ERR);
    context->need_wait_finish = true;

    return rt;

__ERR:
    sg_dma2d.finish_sem = NULL;

    tal_mutex_unlock(sg_dma2d.mutex);

    return rt;
}

OPERATE_RET tal_dma2d_wait_finish(TAL_DMA2D_HANDLE_T handle, uint32_t timeout_ms)
{
    OPERATE_RET rt = OPRT_OK;
    TAL_DMA2D_HANDLE_CONTEXT_T *context = NULL;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    context = (TAL_DMA2D_HANDLE_CONTEXT_T *)handle;
    if (context->need_wait_finish == false) {
        return OPRT_OK;
    }

    if (context->finish_sem) {
        rt = tal_semaphore_wait(context->finish_sem, timeout_ms);
    }

    context->need_wait_finish = false;
    sg_dma2d.finish_sem = NULL;
    tal_mutex_unlock(sg_dma2d.mutex);

    return rt;
}
