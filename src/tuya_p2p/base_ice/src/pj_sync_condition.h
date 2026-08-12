/**
 * @file pj_sync_condition.h
 * @brief Condition-style sync using TAL mutex + semaphore
 */
#ifndef __SYNC_CONDITION_H__
#define __SYNC_CONDITION_H__

#include "tal_mutex.h"
#include "tal_semaphore.h"

typedef struct tagSyncCondition {
    MUTEX_HANDLE mutex;
    SEM_HANDLE sem;
    int waiters;
    int condition_met;
} sync_cond_t;

int sync_cond_init(sync_cond_t *pSyncCond);
void sync_cond_notify(sync_cond_t *pSyncCond);
void sync_cond_wait(sync_cond_t *pSyncCond);
void sync_cond_clean(sync_cond_t *pSyncCond);

#endif /* __SYNC_CONDITION_H__ */
