#include "pj_sync_condition.h"

int sync_cond_init(sync_cond_t *pSyncCond)
{
    if (pthread_mutex_init(&pSyncCond->mutex, NULL) != 0) {
        perror("mutex init failed");
        return -1;
    }

    if (pthread_cond_init(&pSyncCond->cond, NULL) != 0) {
        perror("cond init failed");
        pthread_mutex_destroy(&pSyncCond->mutex);
        return -1;
    }

    pSyncCond->condition_met = 0;
    return 0;
}

void sync_cond_notify(sync_cond_t *pSyncCond)
{
    pthread_mutex_lock(&pSyncCond->mutex);

    // Set condition to true
    pSyncCond->condition_met = 1;

    // Notify waiting threads (you can choose one of the following methods)
    pthread_cond_signal(&pSyncCond->cond); // Wake up at least one waiting thread
    // pthread_cond_broadcast(&pSyncCond->cond); // Wake up all waiting threads

    pthread_mutex_unlock(&pSyncCond->mutex);
}

// Wait condition function
void sync_cond_wait(sync_cond_t *pSyncCond)
{
    pthread_mutex_lock(&pSyncCond->mutex);

    while (pSyncCond->condition_met == 0) {
        // Wait for condition variable, will automatically release mutex and reacquire on return
        pthread_cond_wait(&pSyncCond->cond, &pSyncCond->mutex);
    }

    // Reset condition flag (if needed)
    pSyncCond->condition_met = 0;

    pthread_mutex_unlock(&pSyncCond->mutex);
}

// Cleanup function
void sync_cond_clean(sync_cond_t *pSyncCond)
{
    pthread_mutex_destroy(&pSyncCond->mutex);
    pthread_cond_destroy(&pSyncCond->cond);
    return;
}