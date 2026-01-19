/**
 * @file tal_memory.h
 * @brief Provides memory management functions for Tuya IoT applications.
 *
 * This header file defines a set of memory management functions that abstract
 * the underlying memory allocation mechanisms. It provides a simplified
 * interface for dynamic memory allocation, deallocation, and reallocation,
 * including functions for allocating zero-initialized memory. These functions
 * are designed to be used across the Tuya IoT SDK to ensure consistent and
 * efficient memory management practices. Additionally, it includes a function
 * to retrieve the system's current free heap size, aiding in memory usage
 * diagnostics and optimization.
 *
 * The memory management functions defined in this file are crucial for
 * developing robust and scalable IoT applications on the Tuya platform,
 * providing developers with tools to manage memory usage effectively.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __TAL_MEMORY_H__
#define __TAL_MEMORY_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************
 ********************* constant ( macro and enum ) *********************
 **********************************************************************/
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
#define Malloc(req_size)             tal_psram_malloc(req_size)
#define Calloc(req_count, req_size)  tal_psram_calloc(req_count, req_size)
#define Free(ptr)                    tal_psram_free(ptr)
#else
#define Malloc(req_size)             tal_malloc(req_size)
#define Calloc(req_count, req_size)  tal_calloc(req_count, req_size)
#define Free(ptr)                    tal_free(ptr)
#endif

/***********************************************************************
 ********************* struct ******************************************
 **********************************************************************/

/***********************************************************************
 ********************* variable ****************************************
 **********************************************************************/

/***********************************************************************
 ********************* function ****************************************
 **********************************************************************/

/**
 * @brief This API is used to alloc memory of system.
 *
 * @param[in] size: memory size
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
void *tal_malloc(size_t size);

/**
 * @brief This API is used to free memory of system.
 *
 * @param[in] ptr: memory point
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
void tal_free(void *ptr);

/**
 * @brief Allocate and clear the memory
 *
 * @param[in]       nitems      the numbers of memory block
 * @param[in]       size        the size of the memory block
 *
 * @return the memory address calloced
 */
void *tal_calloc(size_t nitems, size_t size);

/**
 * @brief Re-allocate the memory
 *
 * @param[in]       nitems      source memory address
 * @param[in]       size        the size after re-allocate
 *
 * @return void
 */
void *tal_realloc(void *ptr, size_t size);

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
void *tal_psram_malloc(size_t size);

void tal_psram_free(void *ptr);

void *tal_psram_calloc(size_t nitems, size_t size);

void *tal_psram_realloc(void *ptr, size_t size);
#endif

/**
 * @brief Get system free heap size
 *
 * @param[in] param: none
 *
 * @return OPRT_OK on success. Others on error, please refer to
 * tuya_error_code.h
 */
int tal_system_get_free_heap_size(void);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_MEMORY_H__ */
