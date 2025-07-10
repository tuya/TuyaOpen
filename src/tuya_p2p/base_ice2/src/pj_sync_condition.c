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

    // 设置条件为真
    pSyncCond->condition_met = 1;

    // 通知等待的线程（可以选择以下其中一种方式）
    pthread_cond_signal(&pSyncCond->cond); // 唤醒至少一个等待线程
    // pthread_cond_broadcast(&pSyncCond->cond); // 唤醒所有等待线程

    pthread_mutex_unlock(&pSyncCond->mutex);
}

// 等待条件函数
void sync_cond_wait(sync_cond_t *pSyncCond)
{
    pthread_mutex_lock(&pSyncCond->mutex);

    while (pSyncCond->condition_met == 0) {
        // 等待条件变量，会自动释放互斥锁并在返回时重新获取
        pthread_cond_wait(&pSyncCond->cond, &pSyncCond->mutex);
    }

    // 重置条件标志（如果需要）
    pSyncCond->condition_met = 0;

    pthread_mutex_unlock(&pSyncCond->mutex);
}

// 清理函数
void sync_cond_clean(sync_cond_t *pSyncCond)
{
    pthread_mutex_destroy(&pSyncCond->mutex);
    pthread_cond_destroy(&pSyncCond->cond);
    return;
}