/**
 * @file tal_system.c
 * @brief Implements system-level functionalities for Tuya IoT applications.
 *
 * This source file provides the implementation of system-level functionalities
 * for Tuya IoT applications, including memory allocation and deallocation. It
 * serves as a wrapper around the Tuya Kernel Layer (TKL) memory management
 * functions, offering a simplified interface for dynamic memory management
 * within the Tuya Abstract Layer (TAL). Additionally, it integrates logging
 * functionalities to report memory allocation failures, aiding in the debugging
 * and monitoring of memory usage.
 *
 * Key functionalities include:
 * - Dynamic memory allocation and deallocation.
 * - Logging of memory allocation failures with current free heap size.
 * - Integration with Tuya's IoT SDK for system-level operations.
 *
 * The implementation aims to provide robust and efficient memory management
 * practices, essential for the reliable operation of IoT devices in
 * resource-constrained environments.
 *
 * @note This file is part of the Tuya IoT Development Platform and is intended
 * for use in Tuya-based applications. It is subject to the platform's license
 * and copyright terms.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tkl_system.h"
#include "tkl_memory.h"
#include "tal_system.h"
#include "tal_sleep.h"
#include "tal_log.h"
#include "tal_memory.h"

/**
 * @brief Enter a critical section by disabling interrupts.
 *
 * This function enters a critical section by disabling interrupts and returns
 * the current interrupt mask. Critical sections are used to protect shared
 * resources from being accessed concurrently by interrupt handlers and the main
 * execution context. The returned interrupt mask should be saved and passed
 * to tal_system_exit_critical() when exiting the critical section.
 *
 * This function is typically used in conjunction with tal_system_exit_critical()
 * to protect code sections that must execute atomically. It can also be used
 * with the TAL_ENTER_CRITICAL() and TAL_EXIT_CRITICAL() macros for convenience.
 *
 * @return The current interrupt mask value that should be passed to
 *         tal_system_exit_critical() when exiting the critical section.
 */
uint32_t tal_system_enter_critical(void)
{
    return tkl_system_enter_critical();
}

/**
 * @brief Exit a critical section by restoring interrupts.
 *
 * This function exits a critical section by restoring the interrupt state
 * using the interrupt mask that was returned by tal_system_enter_critical().
 * This restores the system to its previous interrupt state, allowing interrupts
 * to be processed again.
 *
 * The interrupt mask parameter must be the value returned by the corresponding
 * call to tal_system_enter_critical(). Mismatched calls or incorrect mask
 * values may result in incorrect interrupt state restoration.
 *
 * @param irq_mask The interrupt mask value returned by tal_system_enter_critical()
 *                 when entering the critical section.
 */
void tal_system_exit_critical(uint32_t irq_mask)
{
    tkl_system_exit_critical(irq_mask);
}

/**
 * @brief Allocates a block of memory of the specified size.
 *
 * This function is used to dynamically allocate memory of the specified size.
 *
 * @param size The size of the memory block to allocate.
 * @return A pointer to the allocated memory block, or NULL if the allocation
 * fails.
 */
void *tal_malloc(size_t size)
{
    if (0 == size) {
        return NULL;
    }

    void *ptr = NULL;
    ptr = tkl_system_malloc(size);
    if (NULL == ptr) {
        PR_ERR("0x%x malloc failed:0x%x free:0x%x", __builtin_return_address(0), size, tal_system_get_free_heap_size());
    }

    return ptr;
}

/**
 * @brief Frees the memory pointed to by the given pointer.
 *
 * This function is used to deallocate memory that was previously allocated
 * using the `malloc` or `calloc` functions. It takes a pointer to the memory
 * block that needs to be freed and releases the memory back to the system.
 *
 * @param ptr Pointer to the memory block to be freed.
 */
void tal_free(void *ptr)
{
    if (NULL == ptr) {
        return;
    }

    tkl_system_free(ptr);
}

/**
 * Allocates memory for an array of elements, initialized to zero.
 *
 * This function allocates memory for an array of elements, where each element
 * is of size 'size'. The memory is initialized to zero.
 *
 * @param nitems The number of elements to allocate memory for.
 * @param size The size of each element in bytes.
 * @return A pointer to the allocated memory, or NULL if the allocation fails.
 */
void *tal_calloc(size_t nitems, size_t size)
{
    return tkl_system_calloc(nitems, size);
}

/**
 * @brief Reallocates a block of memory.
 *
 *
 * @param ptr   Pointer to the memory block to be reallocated.
 * @param size  New size for the memory block, in bytes.
 * @return      Pointer to the reallocated memory block, or `NULL` if the
 * operation fails.
 */
void *tal_realloc(void *ptr, size_t size)
{
    return tkl_system_realloc(ptr, size);
}

#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
/**
 * @brief Allocate memory from PSRAM (Pseudo Static RAM)
 *
 * This function allocates a block of memory of the specified size from the
 * external PSRAM heap. PSRAM is typically used for large memory allocations
 * that don't fit in the internal RAM, such as image buffers, audio buffers,
 * or other large data structures.
 *
 * @param[in] size: The size of memory to allocate in bytes
 *
 * @return Pointer to the allocated memory block on success, NULL on failure
 */
void *tal_psram_malloc(size_t size)
{
    if (0 == size) {
        return NULL;
    }

    void *ptr = NULL;
    ptr = tkl_system_psram_malloc(size);

    if (NULL == ptr) {
        PR_ERR("0x%x psram malloc failed:0x%x free:0x%x", __builtin_return_address(0), size, tal_system_get_free_heap_size());
    }

    return ptr;
}

/**
 * @brief Free memory allocated from PSRAM
 *
 * This function deallocates a memory block that was previously allocated
 * using tal_psram_malloc(), tal_psram_calloc(), or tal_psram_realloc().
 * The pointer must have been returned by one of these functions.
 *
 * @param[in] ptr: Pointer to the memory block to be freed. If NULL, the
 *                 function does nothing.
 *
 * @return none
 */
void tal_psram_free(void *ptr)
{
    if (NULL == ptr) {
        return;
    }

    tkl_system_psram_free(ptr);
}

/**
 * @brief Allocate and zero-initialize memory from PSRAM
 *
 * This function allocates memory for an array of elements from PSRAM, where
 * each element is of the specified size. The allocated memory is initialized
 * to zero. This is equivalent to calling tal_psram_malloc() followed by
 * memset() to zero.
 *
 * @param[in] nitems: The number of elements to allocate
 * @param[in] size: The size of each element in bytes
 *
 * @return Pointer to the allocated and zero-initialized memory block on
 *         success, NULL on failure
 */
void *tal_psram_calloc(size_t nitems, size_t size)
{
    return tkl_system_psram_calloc(nitems, size);
}

/**
 * @brief Reallocate memory block in PSRAM
 *
 * This function changes the size of a memory block previously allocated in
 * PSRAM. The contents of the memory block are preserved up to the minimum
 * of the old and new sizes. If the new size is larger, the additional memory
 * is not initialized.
 *
 * @param[in] ptr: Pointer to the memory block to be reallocated. If NULL,
 *                 the function behaves like tal_psram_malloc(size)
 * @param[in] size: The new size for the memory block in bytes
 *
 * @return Pointer to the reallocated memory block on success, NULL on
 *         failure. The original pointer may become invalid after this call.
 */
void *tal_psram_realloc(void *ptr, size_t size)
{
    return tkl_system_psram_realloc(ptr, size);
}

/**
 * @brief Get the amount of free heap memory in PSRAM
 *
 * This function returns the total amount of free memory available in the
 * PSRAM heap. This can be useful for monitoring memory usage and debugging
 * memory-related issues.
 *
 * @param[in] param: none
 *
 * @return The amount of free heap memory in PSRAM in bytes
 */
int tal_psram_get_free_heap_size(void)
{
    return tkl_system_psram_get_free_heap_size();
}
#endif

/**
 * @brief Sleeps for the specified amount of time in milliseconds.
 *
 * This function is used to put the system to sleep for the specified amount of
 * time.
 *
 * @param time_ms The time to sleep in milliseconds.
 */
void tal_system_sleep(uint32_t time_ms)
{
    tkl_system_sleep(time_ms);
}

/**
 * @brief Resets the TAL system.
 *
 * This function calls the `tkl_system_reset()` function to reset the TAL
 * system.
 *
 * @note This function should be called when a system reset is required.
 */
void tal_system_reset(void)
{
    tkl_system_reset();
}

/**
 * @brief Get the free heap size.
 *
 * This function returns the amount of free heap memory available in the system.
 *
 * @return The free heap size in bytes.
 */
int tal_system_get_free_heap_size(void)
{
    return tkl_system_get_free_heap_size();
}

/**
 * @brief Retrieves the system tick count.
 *
 * This function returns the current system tick count.
 *
 * @return The system tick count.
 */
SYS_TICK_T tal_system_get_tick_count(void)
{
    return tkl_system_get_tick_count();
}

SYS_TIME_T g_sys_time_offset = 0; // used for debug
/**
 * @brief Get the current system time in milliseconds.
 *
 * This function returns the current system time in milliseconds by calling the
 * `tkl_system_get_millisecond()` function and adding the system time offset.
 *
 * @return The current system time in milliseconds.
 */
SYS_TIME_T tal_system_get_millisecond(void)
{
    return tkl_system_get_millisecond() + g_sys_time_offset;
}

/**
 * @brief Get a random number within the specified range.
 *
 * This function returns a random number within the specified range.
 *
 * @param range The range within which the random number should be generated.
 * @return The generated random number.
 */
int tal_system_get_random(uint32_t range)
{
    return tkl_system_get_random(range);
}

/**
 * @brief Retrieves the reset reason of the system.
 *
 * This function calls the tkl_system_get_reset_reason() function to get the
 * reset reason of the system.
 *
 * @param[out] describe A pointer to a character pointer that will be updated
 * with the reset reason description.
 *
 * @return The reset reason of the system.
 */
TUYA_RESET_REASON_E tal_system_get_reset_reason(char **describe)
{
    return tkl_system_get_reset_reason(describe);
}

/**
 * @brief Get CPU information.
 *
 * This function retrieves the CPU information and returns it in the `cpu_ary`
 * array. The number of CPUs found is stored in the `cpu_cnt` variable.
 *
 * @param[out] cpu_ary Pointer to the array that will store the CPU information.
 * @param[out] cpu_cnt Pointer to the variable that will store the number of
 * CPUs found.
 * @return The result of the operation. Possible values are defined by the
 * `OPERATE_RET` enum.
 */
OPERATE_RET tal_system_get_cpu_info(TUYA_CPU_INFO_T **cpu_ary, int32_t *cpu_cnt)
{
    return tkl_system_get_cpu_info(cpu_ary, (int *)cpu_cnt);
}
