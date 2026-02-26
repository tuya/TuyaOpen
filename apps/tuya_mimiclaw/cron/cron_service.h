#pragma once

#include "mimi_base.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CRON_KIND_EVERY = 0,
    CRON_KIND_AT    = 1,
} cron_kind_t;

typedef struct {
    char        id[9];
    char        name[32];
    bool        enabled;
    cron_kind_t kind;
    uint32_t    interval_s;
    int64_t     at_epoch;
    char        message[256];
    char        channel[16];
    char        chat_id[96];
    int64_t     last_run;
    int64_t     next_run;
    bool        delete_after_run;
} cron_job_t;

OPERATE_RET cron_service_init(void);
OPERATE_RET cron_service_start(void);
void        cron_service_stop(void);

OPERATE_RET cron_add_job(cron_job_t *job);
OPERATE_RET cron_remove_job(const char *job_id);
void        cron_list_jobs(const cron_job_t **jobs, int *count);
