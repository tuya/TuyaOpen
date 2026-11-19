/**
 * @file tal_dma2d.h
 * @brief DMA2D (Direct Memory Access 2D) hardware abstraction layer header
 *
 * This header file defines the interface for DMA2D operations, including
 * initialization, format conversion, memory copy, and completion waiting.
 * It provides a hardware abstraction layer that manages DMA2D resources
 * with thread-safe operations and supports multiple concurrent contexts.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
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

/**
 * @brief Initialize DMA2D handle
 * @param handle Pointer to store the DMA2D handle
 * @return OPRT_OK on success, OPRT_MALLOC_FAILED on memory allocation failure,
 *         or other error codes on initialization failure
 */
OPERATE_RET tal_dma2d_init(TAL_DMA2D_HANDLE_T *handle);

/**
 * @brief Deinitialize DMA2D handle
 * @param handle DMA2D handle to deinitialize
 * @return OPRT_OK on success, OPRT_INVALID_PARM if handle is invalid
 */
OPERATE_RET tal_dma2d_deinit(TAL_DMA2D_HANDLE_T handle);

/**
 * @brief Convert frame format using DMA2D hardware
 * @param handle DMA2D handle
 * @param src Pointer to source frame information structure
 * @param dst Pointer to destination frame information structure
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid,
 *         OPRT_COM_ERROR if hardware is busy or conversion fails
 */
OPERATE_RET tal_dma2d_convert(TAL_DMA2D_HANDLE_T handle, TKL_DMA2D_FRAME_INFO_T *src, TKL_DMA2D_FRAME_INFO_T *dst);

/**
 * @brief Copy memory using DMA2D hardware
 * @param handle DMA2D handle
 * @param src Pointer to source frame information structure
 * @param dst Pointer to destination frame information structure
 * @return OPRT_OK on success, OPRT_INVALID_PARM if parameters are invalid,
 *         OPRT_COM_ERROR if hardware is busy or copy operation fails
 */
OPERATE_RET tal_dma2d_memcpy(TAL_DMA2D_HANDLE_T handle, TKL_DMA2D_FRAME_INFO_T *src, TKL_DMA2D_FRAME_INFO_T *dst);

/**
 * @brief Wait for DMA2D operation to finish
 * @param handle DMA2D handle
 * @param timeout_ms Timeout in milliseconds
 * @return OPRT_OK on success, OPRT_INVALID_PARM if handle is invalid,
 *         OPRT_COM_ERROR if timeout occurs
 */
OPERATE_RET tal_dma2d_wait_finish(TAL_DMA2D_HANDLE_T handle, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_DMA2D_H__ */
