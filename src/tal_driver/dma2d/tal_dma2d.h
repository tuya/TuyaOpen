/**
 * @file tal_dma2d.h
 * @brief tal_dma2d module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __TAL_DMA2D_H__
#define __TAL_DMA2D_H__

#include "tuya_cloud_types.h"

#include "tkl_dma2d.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
typedef void *TAL_DMA2D_HANDLE_T;

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

OPERATE_RET tal_dma2d_init(TAL_DMA2D_HANDLE_T *handle);

OPERATE_RET tal_dma2d_deinit(TAL_DMA2D_HANDLE_T handle);

OPERATE_RET tal_dma2d_convert(TAL_DMA2D_HANDLE_T handle, TKL_DMA2D_FRAME_INFO_T *src, TKL_DMA2D_FRAME_INFO_T *dst);

OPERATE_RET tal_dma2d_memcpy(TAL_DMA2D_HANDLE_T handle, TKL_DMA2D_FRAME_INFO_T *src, TKL_DMA2D_FRAME_INFO_T *dst);

OPERATE_RET tal_dma2d_wait_finish(TAL_DMA2D_HANDLE_T handle, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_DMA2D_H__ */
