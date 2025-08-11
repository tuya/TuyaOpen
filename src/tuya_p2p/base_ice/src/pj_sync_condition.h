#ifndef __SYNC_CONDITION_H__
#define __SYNC_CONDITION_H__

#include <stdio.h>
#include <pthread.h>

typedef struct tagSyncCondition {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int condition_met; // Condition flag
} sync_cond_t;

int sync_cond_init(sync_cond_t *pSyncCond);
void sync_cond_notify(sync_cond_t *pSyncCond);
void sync_cond_wait(sync_cond_t *pSyncCond);
void sync_cond_clean(sync_cond_t *pSyncCond);

#endif /* __SYNC_CONDITION_H__ */