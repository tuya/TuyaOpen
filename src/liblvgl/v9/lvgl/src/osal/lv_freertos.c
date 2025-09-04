/**
 * @file lv_freertos.c
 *
 */

/**
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: MIT
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_os.h"

#if LV_USE_OS == LV_OS_FREERTOS

#ifdef TUYA_TKL_THREAD
#include "tkl_atomic.h"
#include "tkl_system.h"
#include "tkl_task_notify.h"
#else
#if (ESP_PLATFORM)
    #include "freertos/atomic.h"
#else
    #include "atomic.h"
#endif
#endif

#include "../misc/lv_log.h"
/*********************
 *      DEFINES
 *********************/

#define ulMAX_COUNT 10U
#ifndef pcTASK_NAME
    #define pcTASK_NAME "lvglDraw"
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void prvRunThread(void * pxArg);

static void prvMutexInit(lv_mutex_t * pxMutex);

static void prvCheckMutexInit(lv_mutex_t * pxMutex);

#ifdef TUYA_TKL_THREAD

static void prvCondInit(lv_thread_sync_t * pxCond);

static void prvCheckCondInit(lv_thread_sync_t * pxCond);

#if !USE_FREERTOS_TASK_NOTIFY
static void prvTestAndDecrement(lv_thread_sync_t * pxCond,
                                uint32_t ulLocalWaitingThreads);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_result_t lv_thread_init(lv_thread_t * pxThread, lv_thread_prio_t xSchedPriority,
                           void (*pvStartRoutine)(void *), size_t usStackSize,
                           void * xAttr)
{
    pxThread->xTaskArg = xAttr;
    pxThread->pvStartRoutine = pvStartRoutine;

    OPERATE_RET ret = OPRT_OK;

#if defined(ENABLE_SMP) && (ENABLE_SMP == 1)
        static uint32_t coreID = 0;
        ret = tkl_thread_smp_create(&pxThread->xTaskHandle,
                                coreID,
                                pcTASK_NAME,
                                usStackSize,
                                xSchedPriority,
                                prvRunThread,
                                (void *)pxThread);
        if (coreID < (LV_DRAW_SW_DRAW_UNIT_CNT - 1))
            coreID++;
#else 
        ret = tkl_thread_create(&pxThread->xTaskHandle,
                                pcTASK_NAME,
                                usStackSize,
                                xSchedPriority,
                                prvRunThread,
                                (void *)pxThread);

#endif

    /* Ensure that the FreeRTOS task was successfully created. */
    if(ret != OPRT_OK) {
        LV_LOG_ERROR("xTaskCreate failed!");
        return LV_RESULT_INVALID;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_thread_delete(lv_thread_t * pxThread)
{
    tkl_thread_release(pxThread->xTaskHandle);
    return LV_RESULT_OK;
}

lv_result_t lv_mutex_init(lv_mutex_t * pxMutex)
{
    /* If mutex in uninitialized, perform initialization. */
    prvCheckMutexInit(pxMutex);

    return LV_RESULT_OK;
}

lv_result_t lv_mutex_lock(lv_mutex_t * pxMutex)
{
    /* If mutex in uninitialized, perform initialization. */
    prvCheckMutexInit(pxMutex);
    tkl_mutex_lock(pxMutex->xMutex);

    return LV_RESULT_OK;
}

lv_result_t lv_mutex_lock_isr(lv_mutex_t * pxMutex)
{
    /* If mutex in uninitialized, perform initialization. */
    prvCheckMutexInit(pxMutex);
    tkl_mutex_lock(pxMutex->xMutex);

    return LV_RESULT_OK;
}

lv_result_t lv_mutex_unlock(lv_mutex_t * pxMutex)
{
    /* If mutex in uninitialized, perform initialization. */
    prvCheckMutexInit(pxMutex);
    tkl_mutex_unlock(pxMutex->xMutex);
    return LV_RESULT_OK;
}

lv_result_t lv_mutex_delete(lv_mutex_t * pxMutex)
{
    tkl_mutex_release(pxMutex->xMutex);
    pxMutex->xIsInitialized = FALSE;

    return LV_RESULT_OK;
}

lv_result_t lv_thread_sync_init(lv_thread_sync_t * pxCond)
{
    /* If the cond is uninitialized, perform initialization. */
    prvCheckCondInit(pxCond);

    return LV_RESULT_OK;
}

lv_result_t lv_thread_sync_wait(lv_thread_sync_t * pxCond)
{
    lv_result_t lvRes = LV_RESULT_OK;

    /* If the cond is uninitialized, perform initialization. */
    prvCheckCondInit(pxCond);

#if USE_FREERTOS_TASK_NOTIFY
    TKL_THREAD_HANDLE xCurrentTaskHandle = NULL;
    tkl_thread_get_id(&xCurrentTaskHandle);

    UINT_T level = tkl_system_enter_critical();
    BOOL_T xSyncSygnal = pxCond->xSyncSignal;
    pxCond->xSyncSignal = FALSE;
    if(xSyncSygnal == FALSE) {
        /* The signal hasn't been sent yet. Tell the sender to notify this task */
        pxCond->xTaskToNotify = xCurrentTaskHandle;
    }
    /* If we have a signal from the other task, we should not ask to be notified */
    tkl_system_exit_critical(level);

    if(xSyncSygnal == FALSE) {
        /* Wait for other task to notify this task. */
        tkl_task_notify_take(TRUE, TKL_NOTIFY_WAIT_FROEVER);
    }
    /* If the signal was received, no wait needs to be done */
#else
    uint32_t ulLocalWaitingThreads;

    /* Acquire the mutex. */
    tkl_mutex_lock(pxCond->xSyncMutex);

    while(!pxCond->xSyncSignal) {
        /* Increase the counter of threads blocking on condition variable, then
         * release the mutex. */

        /* Atomically increments thread waiting by 1, and
         * stores number of threads waiting before increment. */
        ulLocalWaitingThreads = tkl_system_atomic_inc(&pxCond->ulWaitingThreads);

        OPERATE_RET xMutexStatus = tkl_mutex_unlock(pxCond->xSyncMutex);

        /* Wait on the condition variable. */
        if(xMutexStatus == OPRT_OK) {
            OPERATE_RET xCondWaitStatus = tkl_semaphore_wait(
                                             pxCond->xCondWaitSemaphore,
                                             TKL_SEM_WAIT_FOREVER);

            /* Relock the mutex. */
            tkl_mutex_lock(pxCond->xSyncMutex);

            if(xCondWaitStatus != OPRT_OK) {
                LV_LOG_ERROR("xSemaphoreTake(xCondWaitSemaphore) failed!");
                lvRes = LV_RESULT_INVALID;

                /* Atomically decrements thread waiting by 1.
                 * If iLocalWaitingThreads is updated by other thread(s) in between,
                 * this implementation guarantees to decrement by 1 based on the
                 * value currently in pxCond->ulWaitingThreads. */
                prvTestAndDecrement(pxCond, ulLocalWaitingThreads + 1);
            }
        }
        else {
            LV_LOG_ERROR("xSemaphoreGive(xSyncMutex) failed!");
            lvRes = LV_RESULT_INVALID;

            /* Atomically decrements thread waiting by 1.
             * If iLocalWaitingThreads is updated by other thread(s) in between,
             * this implementation guarantees to decrement by 1 based on the
             * value currently in pxCond->ulWaitingThreads. */
            prvTestAndDecrement(pxCond, ulLocalWaitingThreads + 1);
        }
    }

    pxCond->xSyncSignal = FALSE;

    /* Release the mutex. */
    tkl_mutex_unlock(pxCond->xSyncMutex);
#endif

    return lvRes;
}

lv_result_t lv_thread_sync_signal(lv_thread_sync_t * pxCond)
{
    /* If the cond is uninitialized, perform initialization. */
    prvCheckCondInit(pxCond);

#if USE_FREERTOS_TASK_NOTIFY
    UINT_T level = tkl_system_enter_critical();
    TKL_THREAD_HANDLE xTaskToNotify = pxCond->xTaskToNotify;
    pxCond->xTaskToNotify = NULL;
    if(xTaskToNotify == NULL) {
        /* No task waiting to be notified. Send this signal for later */
        pxCond->xSyncSignal = TRUE;
    }
    /* If a task is already waiting, there is no need to set the sync signal */
    tkl_system_exit_critical(level);

    if(xTaskToNotify != NULL) {
        /* There is a task waiting. Send a notification to it */
        tkl_task_notify_give(xTaskToNotify);
    }
    /* If there was no task waiting to be notified, we sent a signal for it to see later. */
#else
    /* Acquire the mutex. */
    tkl_mutex_lock(pxCond->xSyncMutex);

    pxCond->xSyncSignal = TRUE;

    /* Local copy of number of threads waiting. */
    uint32_t ulLocalWaitingThreads = pxCond->ulWaitingThreads;

    /* Test local copy of threads waiting is larger than zero. */
    while(ulLocalWaitingThreads > 0) {
        /* Atomically check whether the copy in memory has changed.
         * If not, set the copy of threads waiting in memory to zero. */
        if(TRUE == tkl_system_atomic_cmp_and_set(
               &pxCond->ulWaitingThreads,
               0,
               ulLocalWaitingThreads)) {
            /* Unblock all. */
            for(uint32_t i = 0; i < ulLocalWaitingThreads; i++) {
                tkl_semaphore_post(pxCond->xCondWaitSemaphore);
            }

            break;
        }

        /* Local copy is out dated. Reload from memory and retry. */
        ulLocalWaitingThreads = pxCond->ulWaitingThreads;
    }

    /* Release the mutex. */
    tkl_mutex_unlock(pxCond->xSyncMutex);
#endif

    return LV_RESULT_OK;
}

lv_result_t lv_thread_sync_delete(lv_thread_sync_t * pxCond)
{
#if !USE_FREERTOS_TASK_NOTIFY
    /* Cleanup all resources used by the cond. */
    tkl_semaphore_release(pxCond->xCondWaitSemaphore);
    tkl_mutex_release(pxCond->xSyncMutex);
    pxCond->ulWaitingThreads = 0;
#endif
    pxCond->xSyncSignal = FALSE;
    pxCond->xIsInitialized = FALSE;

    return LV_RESULT_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void prvRunThread(void * pxArg)
{
    lv_thread_t * pxThread = (lv_thread_t *)pxArg;

    /* Run the thread routine. */
    pxThread->pvStartRoutine((void *)pxThread->xTaskArg);

    tkl_thread_release(pxThread->xTaskHandle);
}

static void prvMutexInit(lv_mutex_t * pxMutex)
{
    if (tkl_mutex_create_init(&pxMutex->xMutex) != OPRT_OK) {
        LV_LOG_ERROR("xSemaphoreCreateMutex failed!");
        return;
    }
    pxMutex->xIsInitialized = TRUE;
}

static void prvCheckMutexInit(lv_mutex_t * pxMutex)
{
    /* Check if the mutex needs to be initialized. */
    if(pxMutex->xIsInitialized == FALSE) {
        /* Mutex initialization must be in a critical section to prevent two threads
         * from initializing it at the same time. */
        UINT_T level = tkl_system_enter_critical();

        /* Check again that the mutex is still uninitialized, i.e. it wasn't
         * initialized while this function was waiting to enter the critical
         * section. */
        if(pxMutex->xIsInitialized == FALSE) {
            prvMutexInit(pxMutex);
        }

        /* Exit the critical section. */
        tkl_system_exit_critical(level);
    }
}

static void prvCondInit(lv_thread_sync_t * pxCond)
{
#if USE_FREERTOS_TASK_NOTIFY
    pxCond->xTaskToNotify = NULL;
#else
    tkl_semaphore_create_init(&pxCond->xCondWaitSemaphore, 0U, ulMAX_COUNT);

    /* Ensure that the FreeRTOS semaphore was successfully created. */
    if(pxCond->xCondWaitSemaphore == NULL) {
        LV_LOG_ERROR("xSemaphoreCreateCounting failed!");
        return;
    }

    tkl_mutex_create_init(&pxCond->xSyncMutex);

    /* Ensure that the FreeRTOS mutex was successfully created. */
    if(pxCond->xSyncMutex == NULL) {
        LV_LOG_ERROR("xSemaphoreCreateMutex failed!");
        /* Cleanup. */
        tkl_semaphore_release(pxCond->xCondWaitSemaphore);
        return;
    }

    /* Condition variable successfully created. */
    pxCond->ulWaitingThreads = 0;
#endif

    pxCond->xSyncSignal = FALSE;
    pxCond->xIsInitialized = TRUE;
}

static void prvCheckCondInit(lv_thread_sync_t * pxCond)
{
    /* Check if the condition variable needs to be initialized. */
    if(pxCond->xIsInitialized == FALSE) {
        /* Cond initialization must be in a critical section to prevent two
         * threads from initializing it at the same time. */
        UINT_T level = tkl_system_enter_critical();

        /* Check again that the condition is still uninitialized, i.e. it wasn't
         * initialized while this function was waiting to enter the critical
         * section. */
        if(pxCond->xIsInitialized == FALSE) {
            prvCondInit(pxCond);
        }

        /* Exit the critical section. */
        tkl_system_exit_critical(level);
    }
}

#if !USE_FREERTOS_TASK_NOTIFY
static void prvTestAndDecrement(lv_thread_sync_t * pxCond,
                                uint32_t ulLocalWaitingThreads)
{
    /* Test local copy of threads waiting is larger than zero. */
    while(ulLocalWaitingThreads > 0) {
        /* Atomically check whether the copy in memory has changed.
         * If not, decrease the copy of threads waiting in memory. */
        if(TRUE == tkl_system_atomic_cmp_and_set(
               &pxCond->ulWaitingThreads,
               ulLocalWaitingThreads - 1,
               ulLocalWaitingThreads)) {
            /* Signal one succeeded. Break. */
            break;
        }

        /* Local copy may be out dated. Reload from memory and retry. */
        ulLocalWaitingThreads = pxCond->ulWaitingThreads;
    }
}
#endif

#else       //TUYA_TKL_THREAD
#if !USE_FREERTOS_TASK_NOTIFY
static void prvCondInit(lv_thread_sync_t * pxCond);

static void prvCheckCondInit(lv_thread_sync_t * pxCond);

static void prvTestAndDecrement(lv_thread_sync_t * pxCond,
                                uint32_t ulLocalWaitingThreads);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/

#if (ESP_PLATFORM)
    static portMUX_TYPE critSectionMux = portMUX_INITIALIZER_UNLOCKED;
#endif

/**********************
 *      MACROS
 **********************/

#if (ESP_PLATFORM)
    #define _enter_critical()   taskENTER_CRITICAL(&critSectionMux);
    #define _exit_critical()    taskEXIT_CRITICAL(&critSectionMux);
#else
    #define _enter_critical()   taskENTER_CRITICAL();
    #define _exit_critical()    taskEXIT_CRITICAL();
#endif

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_result_t lv_thread_init(lv_thread_t * pxThread, lv_thread_prio_t xSchedPriority,
                           void (*pvStartRoutine)(void *), size_t usStackSize,
                           void * xAttr)
{
    pxThread->xTaskArg = xAttr;
    pxThread->pvStartRoutine = pvStartRoutine;

    BaseType_t xTaskCreateStatus = xTaskCreate(
                                       prvRunThread,
                                       pcTASK_NAME,
                                       (configSTACK_DEPTH_TYPE)(usStackSize / sizeof(StackType_t)),
                                       (void *)pxThread,
                                       tskIDLE_PRIORITY + xSchedPriority,
                                       &pxThread->xTaskHandle);

    /* Ensure that the FreeRTOS task was successfully created. */
    if(xTaskCreateStatus != pdPASS) {
        LV_LOG_ERROR("xTaskCreate failed!");
        return LV_RESULT_INVALID;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_thread_delete(lv_thread_t * pxThread)
{
    vTaskDelete(pxThread->xTaskHandle);

    return LV_RESULT_OK;
}

lv_result_t lv_mutex_init(lv_mutex_t * pxMutex)
{
    /* If mutex in uninitialized, perform initialization. */
    prvCheckMutexInit(pxMutex);

    return LV_RESULT_OK;
}

lv_result_t lv_mutex_lock(lv_mutex_t * pxMutex)
{
    /* If mutex in uninitialized, perform initialization. */
    prvCheckMutexInit(pxMutex);

    BaseType_t xMutexTakeStatus = xSemaphoreTake(pxMutex->xMutex, portMAX_DELAY);
    if(xMutexTakeStatus != pdTRUE) {
        LV_LOG_ERROR("xSemaphoreTake failed!");
        return LV_RESULT_INVALID;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_mutex_lock_isr(lv_mutex_t * pxMutex)
{
    /* If mutex in uninitialized, perform initialization. */
    prvCheckMutexInit(pxMutex);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    BaseType_t xMutexTakeStatus = xSemaphoreTakeFromISR(pxMutex->xMutex, &xHigherPriorityTaskWoken);
    if(xMutexTakeStatus != pdTRUE) {
        LV_LOG_ERROR("xSemaphoreTake failed!");
        return LV_RESULT_INVALID;
    }

    /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context switch
    should be performed to ensure the interrupt returns directly to the highest
    priority task.  The macro used for this purpose is dependent on the port in
    use and may be called portEND_SWITCHING_ISR(). */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

    return LV_RESULT_OK;
}

lv_result_t lv_mutex_unlock(lv_mutex_t * pxMutex)
{
    /* If mutex in uninitialized, perform initialization. */
    prvCheckMutexInit(pxMutex);

    BaseType_t xMutexGiveStatus = xSemaphoreGive(pxMutex->xMutex);
    if(xMutexGiveStatus != pdTRUE) {
        LV_LOG_ERROR("xSemaphoreGive failed!");
        return LV_RESULT_INVALID;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_mutex_delete(lv_mutex_t * pxMutex)
{
    vSemaphoreDelete(pxMutex->xMutex);
    pxMutex->xIsInitialized = pdFALSE;

    return LV_RESULT_OK;
}

lv_result_t lv_thread_sync_init(lv_thread_sync_t * pxCond)
{
#if USE_FREERTOS_TASK_NOTIFY
    /* Store the handle of the calling task. */
    pxCond->xTaskToNotify = xTaskGetCurrentTaskHandle();
#else
    /* If the cond is uninitialized, perform initialization. */
    prvCheckCondInit(pxCond);
#endif

    return LV_RESULT_OK;
}

lv_result_t lv_thread_sync_wait(lv_thread_sync_t * pxCond)
{
    lv_result_t lvRes = LV_RESULT_OK;

#if USE_FREERTOS_TASK_NOTIFY
    LV_UNUSED(pxCond);

    /* Wait for other task to notify this task. */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#else
    uint32_t ulLocalWaitingThreads;

    /* If the cond is uninitialized, perform initialization. */
    prvCheckCondInit(pxCond);

    /* Acquire the mutex. */
    xSemaphoreTake(pxCond->xSyncMutex, portMAX_DELAY);

    while(!pxCond->xSyncSignal) {
        /* Increase the counter of threads blocking on condition variable, then
         * release the mutex. */

        /* Atomically increments thread waiting by 1, and
         * stores number of threads waiting before increment. */
        ulLocalWaitingThreads = Atomic_Increment_u32(&pxCond->ulWaitingThreads);

        BaseType_t xMutexStatus = xSemaphoreGive(pxCond->xSyncMutex);

        /* Wait on the condition variable. */
        if(xMutexStatus == pdTRUE) {
            BaseType_t xCondWaitStatus = xSemaphoreTake(
                                             pxCond->xCondWaitSemaphore,
                                             portMAX_DELAY);

            /* Relock the mutex. */
            xSemaphoreTake(pxCond->xSyncMutex, portMAX_DELAY);

            if(xCondWaitStatus != pdTRUE) {
                LV_LOG_ERROR("xSemaphoreTake(xCondWaitSemaphore) failed!");
                lvRes = LV_RESULT_INVALID;

                /* Atomically decrements thread waiting by 1.
                 * If iLocalWaitingThreads is updated by other thread(s) in between,
                 * this implementation guarantees to decrement by 1 based on the
                 * value currently in pxCond->ulWaitingThreads. */
                prvTestAndDecrement(pxCond, ulLocalWaitingThreads + 1);
            }
        }
        else {
            LV_LOG_ERROR("xSemaphoreGive(xSyncMutex) failed!");
            lvRes = LV_RESULT_INVALID;

            /* Atomically decrements thread waiting by 1.
             * If iLocalWaitingThreads is updated by other thread(s) in between,
             * this implementation guarantees to decrement by 1 based on the
             * value currently in pxCond->ulWaitingThreads. */
            prvTestAndDecrement(pxCond, ulLocalWaitingThreads + 1);
        }
    }

    pxCond->xSyncSignal = pdFALSE;

    /* Release the mutex. */
    xSemaphoreGive(pxCond->xSyncMutex);
#endif

    return lvRes;
}

lv_result_t lv_thread_sync_signal(lv_thread_sync_t * pxCond)
{
#if USE_FREERTOS_TASK_NOTIFY
    /* Send a notification to the task waiting. */
    xTaskNotifyGive(pxCond->xTaskToNotify);
#else
    /* If the cond is uninitialized, perform initialization. */
    prvCheckCondInit(pxCond);

    /* Acquire the mutex. */
    xSemaphoreTake(pxCond->xSyncMutex, portMAX_DELAY);

    pxCond->xSyncSignal = pdTRUE;

    /* Local copy of number of threads waiting. */
    uint32_t ulLocalWaitingThreads = pxCond->ulWaitingThreads;

    /* Test local copy of threads waiting is larger than zero. */
    while(ulLocalWaitingThreads > 0) {
        /* Atomically check whether the copy in memory has changed.
         * If not, set the copy of threads waiting in memory to zero. */
        if(ATOMIC_COMPARE_AND_SWAP_SUCCESS == Atomic_CompareAndSwap_u32(
               &pxCond->ulWaitingThreads,
               0,
               ulLocalWaitingThreads)) {
            /* Unblock all. */
            for(uint32_t i = 0; i < ulLocalWaitingThreads; i++) {
                xSemaphoreGive(pxCond->xCondWaitSemaphore);
            }

            break;
        }

        /* Local copy is out dated. Reload from memory and retry. */
        ulLocalWaitingThreads = pxCond->ulWaitingThreads;
    }

    /* Release the mutex. */
    xSemaphoreGive(pxCond->xSyncMutex);
#endif

    return LV_RESULT_OK;
}

lv_result_t lv_thread_sync_delete(lv_thread_sync_t * pxCond)
{
#if USE_FREERTOS_TASK_NOTIFY
    LV_UNUSED(pxCond);
#else
    /* Cleanup all resources used by the cond. */
    vSemaphoreDelete(pxCond->xCondWaitSemaphore);
    vSemaphoreDelete(pxCond->xSyncMutex);
    pxCond->ulWaitingThreads = 0;
    pxCond->xSyncSignal = pdFALSE;
    pxCond->xIsInitialized = pdFALSE;
#endif

    return LV_RESULT_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void prvRunThread(void * pxArg)
{
    lv_thread_t * pxThread = (lv_thread_t *)pxArg;

    /* Run the thread routine. */
    pxThread->pvStartRoutine((void *)pxThread->xTaskArg);

    vTaskDelete(NULL);
}

static void prvMutexInit(lv_mutex_t * pxMutex)
{
    pxMutex->xMutex = xSemaphoreCreateMutex();

    /* Ensure that the FreeRTOS mutex was successfully created. */
    if(pxMutex->xMutex == NULL) {
        LV_LOG_ERROR("xSemaphoreCreateMutex failed!");
        return;
    }

    /* Mutex successfully created. */
    pxMutex->xIsInitialized = pdTRUE;
}

static void prvCheckMutexInit(lv_mutex_t * pxMutex)
{
    /* Check if the mutex needs to be initialized. */
    if(pxMutex->xIsInitialized == pdFALSE) {
        /* Mutex initialization must be in a critical section to prevent two threads
         * from initializing it at the same time. */
        _enter_critical();

        /* Check again that the mutex is still uninitialized, i.e. it wasn't
         * initialized while this function was waiting to enter the critical
         * section. */
        if(pxMutex->xIsInitialized == pdFALSE) {
            prvMutexInit(pxMutex);
        }

        /* Exit the critical section. */
        _exit_critical();
    }
}

#if !USE_FREERTOS_TASK_NOTIFY
static void prvCondInit(lv_thread_sync_t * pxCond)
{
    pxCond->xCondWaitSemaphore = xSemaphoreCreateCounting(ulMAX_COUNT, 0U);

    /* Ensure that the FreeRTOS semaphore was successfully created. */
    if(pxCond->xCondWaitSemaphore == NULL) {
        LV_LOG_ERROR("xSemaphoreCreateCounting failed!");
        return;
    }

    pxCond->xSyncMutex = xSemaphoreCreateMutex();

    /* Ensure that the FreeRTOS mutex was successfully created. */
    if(pxCond->xSyncMutex == NULL) {
        LV_LOG_ERROR("xSemaphoreCreateMutex failed!");
        /* Cleanup. */
        vSemaphoreDelete(pxCond->xCondWaitSemaphore);
        return;
    }

    /* Condition variable successfully created. */
    pxCond->ulWaitingThreads = 0;
    pxCond->xSyncSignal = pdFALSE;
    pxCond->xIsInitialized = pdTRUE;
}

static void prvCheckCondInit(lv_thread_sync_t * pxCond)
{
    BaseType_t xSemCreateStatus = pdTRUE;

    /* Check if the condition variable needs to be initialized. */
    if(pxCond->xIsInitialized == pdFALSE) {
        /* Cond initialization must be in a critical section to prevent two
         * threads from initializing it at the same time. */
        _enter_critical();

        /* Check again that the condition is still uninitialized, i.e. it wasn't
         * initialized while this function was waiting to enter the critical
         * section. */
        if(pxCond->xIsInitialized == pdFALSE) {
            prvCondInit(pxCond);
        }

        /* Exit the critical section. */
        _exit_critical();
    }
}

static void prvTestAndDecrement(lv_thread_sync_t * pxCond,
                                uint32_t ulLocalWaitingThreads)
{
    /* Test local copy of threads waiting is larger than zero. */
    while(ulLocalWaitingThreads > 0) {
        /* Atomically check whether the copy in memory has changed.
         * If not, decrease the copy of threads waiting in memory. */
        if(ATOMIC_COMPARE_AND_SWAP_SUCCESS == Atomic_CompareAndSwap_u32(
               &pxCond->ulWaitingThreads,
               ulLocalWaitingThreads - 1,
               ulLocalWaitingThreads)) {
            /* Signal one succeeded. Break. */
            break;
        }

        /* Local copy may be out dated. Reload from memory and retry. */
        ulLocalWaitingThreads = pxCond->ulWaitingThreads;
    }
}
#endif

#endif      //TUYA_TKL_THREAD

#endif /*LV_USE_OS == LV_OS_FREERTOS*/
