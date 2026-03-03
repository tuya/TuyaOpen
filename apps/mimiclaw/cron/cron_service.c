#include "cron/cron_service.h"

#include "bus/message_bus.h"
#include "mimi_config.h"

#include "cJSON.h"
// #include "tal_fs.h"

#include <string.h>

static const char *TAG = "cron";

#define MAX_CRON_JOBS       MIMI_CRON_MAX_JOBS
#define CRON_FILE_MAX_BYTES 8192
#define CRON_THREAD_STACK   (6 * 1024)

static cron_job_t    s_jobs[MAX_CRON_JOBS];
static int           s_job_count   = 0;
static THREAD_HANDLE s_cron_thread = NULL;

static OPERATE_RET cron_save_jobs(void);

static int64_t cron_now_epoch(void)
{
    return (int64_t)tal_time_get_posix();
}

static bool cron_sanitize_destination(cron_job_t *job)
{
    bool changed = false;

    if (!job) {
        return false;
    }

    if (job->channel[0] == '\0') {
        strncpy(job->channel, MIMI_CHAN_SYSTEM, sizeof(job->channel) - 1);
        changed = true;
    }

    if (strcmp(job->channel, MIMI_CHAN_TELEGRAM) == 0) {
        if (job->chat_id[0] == '\0' || strcmp(job->chat_id, "cron") == 0) {
            MIMI_LOGW(TAG, "cron job %s has invalid telegram chat_id, fallback to system:cron",
                      job->id[0] ? job->id : "<new>");
            strncpy(job->channel, MIMI_CHAN_SYSTEM, sizeof(job->channel) - 1);
            strncpy(job->chat_id, "cron", sizeof(job->chat_id) - 1);
            changed = true;
        }
    } else if (job->chat_id[0] == '\0') {
        strncpy(job->chat_id, "cron", sizeof(job->chat_id) - 1);
        changed = true;
    }

    return changed;
}

static void cron_generate_id(char *id_buf, size_t id_buf_size)
{
    uint32_t r = ((uint32_t)tal_system_get_random(0x7fffffffu)) ^ (uint32_t)tal_system_get_millisecond();
    snprintf(id_buf, id_buf_size, "%08x", (unsigned int)r);
}

static OPERATE_RET cron_load_jobs(void)
{
    TUYA_FILE f = mimi_fopen(MIMI_CRON_FILE, "r");
    if (!f) {
        MIMI_LOGI(TAG, "no cron file found, starting fresh");
        s_job_count = 0;
        return OPRT_OK;
    }

    int fsize = mimi_fgetsize(MIMI_CRON_FILE);
    if (fsize <= 0 || fsize > CRON_FILE_MAX_BYTES) {
        MIMI_LOGW(TAG, "cron file invalid size: %d", fsize);
        mimi_fclose(f);
        s_job_count = 0;
        return OPRT_OK;
    }

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) {
        mimi_fclose(f);
        return OPRT_MALLOC_FAILED;
    }

    int n = mimi_fread(buf, fsize, f);
    mimi_fclose(f);
    if (n < 0) {
        free(buf);
        return OPRT_COM_ERROR;
    }
    buf[n] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        MIMI_LOGW(TAG, "failed to parse cron json");
        s_job_count = 0;
        return OPRT_OK;
    }

    cJSON *jobs_arr = cJSON_GetObjectItem(root, "jobs");
    if (!jobs_arr || !cJSON_IsArray(jobs_arr)) {
        cJSON_Delete(root);
        s_job_count = 0;
        return OPRT_OK;
    }

    s_job_count   = 0;
    bool repaired = false;

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, jobs_arr)
    {
        if (s_job_count >= MAX_CRON_JOBS) {
            break;
        }

        cron_job_t *job = &s_jobs[s_job_count];
        memset(job, 0, sizeof(*job));

        const char *id       = cJSON_GetStringValue(cJSON_GetObjectItem(item, "id"));
        const char *name     = cJSON_GetStringValue(cJSON_GetObjectItem(item, "name"));
        const char *kind_str = cJSON_GetStringValue(cJSON_GetObjectItem(item, "kind"));
        const char *message  = cJSON_GetStringValue(cJSON_GetObjectItem(item, "message"));
        const char *channel  = cJSON_GetStringValue(cJSON_GetObjectItem(item, "channel"));
        const char *chat_id  = cJSON_GetStringValue(cJSON_GetObjectItem(item, "chat_id"));

        if (!id || !name || !kind_str || !message) {
            continue;
        }

        strncpy(job->id, id, sizeof(job->id) - 1);
        strncpy(job->name, name, sizeof(job->name) - 1);
        strncpy(job->message, message, sizeof(job->message) - 1);
        strncpy(job->channel, channel ? channel : MIMI_CHAN_SYSTEM, sizeof(job->channel) - 1);
        strncpy(job->chat_id, chat_id ? chat_id : "cron", sizeof(job->chat_id) - 1);
        if (cron_sanitize_destination(job)) {
            repaired = true;
        }

        cJSON *enabled_j = cJSON_GetObjectItem(item, "enabled");
        job->enabled     = enabled_j ? cJSON_IsTrue(enabled_j) : true;

        cJSON *delete_j       = cJSON_GetObjectItem(item, "delete_after_run");
        job->delete_after_run = delete_j ? cJSON_IsTrue(delete_j) : false;

        if (strcmp(kind_str, "every") == 0) {
            job->kind       = CRON_KIND_EVERY;
            cJSON *interval = cJSON_GetObjectItem(item, "interval_s");
            job->interval_s = (interval && cJSON_IsNumber(interval)) ? (uint32_t)interval->valuedouble : 0;
        } else if (strcmp(kind_str, "at") == 0) {
            job->kind       = CRON_KIND_AT;
            cJSON *at_epoch = cJSON_GetObjectItem(item, "at_epoch");
            job->at_epoch   = (at_epoch && cJSON_IsNumber(at_epoch)) ? (int64_t)at_epoch->valuedouble : 0;
        } else {
            continue;
        }

        cJSON *last_run = cJSON_GetObjectItem(item, "last_run");
        job->last_run   = (last_run && cJSON_IsNumber(last_run)) ? (int64_t)last_run->valuedouble : 0;

        cJSON *next_run = cJSON_GetObjectItem(item, "next_run");
        job->next_run   = (next_run && cJSON_IsNumber(next_run)) ? (int64_t)next_run->valuedouble : 0;

        s_job_count++;
    }

    cJSON_Delete(root);

    if (repaired) {
        (void)cron_save_jobs();
    }

    MIMI_LOGI(TAG, "loaded %d cron jobs", s_job_count);
    return OPRT_OK;
}

static OPERATE_RET cron_save_jobs(void)
{
    cJSON *root     = cJSON_CreateObject();
    cJSON *jobs_arr = cJSON_CreateArray();
    if (!root || !jobs_arr) {
        cJSON_Delete(root);
        cJSON_Delete(jobs_arr);
        return OPRT_MALLOC_FAILED;
    }

    for (int i = 0; i < s_job_count; i++) {
        cron_job_t *job  = &s_jobs[i];
        cJSON      *item = cJSON_CreateObject();
        if (!item) {
            continue;
        }

        cJSON_AddStringToObject(item, "id", job->id);
        cJSON_AddStringToObject(item, "name", job->name);
        cJSON_AddBoolToObject(item, "enabled", job->enabled);
        cJSON_AddStringToObject(item, "kind", job->kind == CRON_KIND_EVERY ? "every" : "at");
        if (job->kind == CRON_KIND_EVERY) {
            cJSON_AddNumberToObject(item, "interval_s", job->interval_s);
        } else {
            cJSON_AddNumberToObject(item, "at_epoch", (double)job->at_epoch);
        }

        cJSON_AddStringToObject(item, "message", job->message);
        cJSON_AddStringToObject(item, "channel", job->channel);
        cJSON_AddStringToObject(item, "chat_id", job->chat_id);
        cJSON_AddNumberToObject(item, "last_run", (double)job->last_run);
        cJSON_AddNumberToObject(item, "next_run", (double)job->next_run);
        cJSON_AddBoolToObject(item, "delete_after_run", job->delete_after_run);

        cJSON_AddItemToArray(jobs_arr, item);
    }

    cJSON_AddItemToObject(root, "jobs", jobs_arr);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str) {
        return OPRT_MALLOC_FAILED;
    }

    TUYA_FILE f = mimi_fopen(MIMI_CRON_FILE, "w");
    if (!f) {
        cJSON_free(json_str);
        MIMI_LOGE(TAG, "failed to open %s for writing", MIMI_CRON_FILE);
        return OPRT_FILE_OPEN_FAILED;
    }

    size_t len = strlen(json_str);
    int    wn  = mimi_fwrite(json_str, (int)len, f);
    mimi_fclose(f);
    cJSON_free(json_str);

    if (wn < 0 || (size_t)wn != len) {
        MIMI_LOGE(TAG, "cron save incomplete: %d/%u", wn, (unsigned)len);
        return OPRT_COM_ERROR;
    }

    MIMI_LOGI(TAG, "saved %d cron jobs", s_job_count);
    return OPRT_OK;
}

static void cron_process_due_jobs(void)
{
    int64_t now = cron_now_epoch();
    MIMI_LOGI(TAG, "checking cron jobs at epoch %lld, job count=%d", (long long)now, s_job_count);
    bool changed = false;

    for (int i = 0; i < s_job_count; i++) {
        cron_job_t *job = &s_jobs[i];
        if (!job->enabled || job->next_run <= 0 || job->next_run > now) {
            continue;
        }

        MIMI_LOGI(TAG, "cron firing: %s (%s)", job->name, job->id);

        mimi_msg_t msg = {0};
        strncpy(msg.channel, job->channel, sizeof(msg.channel) - 1);
        strncpy(msg.chat_id, job->chat_id, sizeof(msg.chat_id) - 1);
        msg.content = strdup(job->message);

        if (msg.content) {
            OPERATE_RET rt = message_bus_push_inbound(&msg);
            if (rt != OPRT_OK) {
                MIMI_LOGW(TAG, "failed to push cron message rt=%d", rt);
                free(msg.content);
            }
        }

        job->last_run = now;

        if (job->kind == CRON_KIND_AT) {
            if (job->delete_after_run) {
                MIMI_LOGI(TAG, "deleting one-shot cron: %s", job->id);
                for (int j = i; j < s_job_count - 1; j++) {
                    s_jobs[j] = s_jobs[j + 1];
                }
                s_job_count--;
                i--;
            } else {
                job->enabled  = false;
                job->next_run = 0;
            }
        } else {
            job->next_run = now + job->interval_s;
        }

        changed = true;
    }

    if (changed) {
        (void)cron_save_jobs();
    }
}

static void cron_task_main(void *arg)
{
    (void)arg;

    while (1) {
        tal_system_sleep(MIMI_CRON_CHECK_INTERVAL_MS);
        cron_process_due_jobs();
    }
}

static void compute_initial_next_run(cron_job_t *job)
{
    int64_t now = cron_now_epoch();

    if (job->kind == CRON_KIND_EVERY) {
        job->next_run = now + job->interval_s;
    } else if (job->kind == CRON_KIND_AT) {
        if (job->at_epoch > now) {
            job->next_run = job->at_epoch;
        } else {
            job->next_run = 0;
            job->enabled  = false;
        }
    }
}

OPERATE_RET cron_service_init(void)
{
    return cron_load_jobs();
}

OPERATE_RET cron_service_start(void)
{
    if (s_cron_thread) {
        MIMI_LOGW(TAG, "cron thread already running");
        return OPRT_OK;
    }

    int64_t now = cron_now_epoch();
    for (int i = 0; i < s_job_count; i++) {
        cron_job_t *job = &s_jobs[i];
        if (job->enabled && job->next_run <= 0) {
            if (job->kind == CRON_KIND_EVERY) {
                job->next_run = now + job->interval_s;
            } else if (job->kind == CRON_KIND_AT && job->at_epoch > now) {
                job->next_run = job->at_epoch;
            }
        }
    }

    THREAD_CFG_T cfg = {0};
    cfg.stackDepth   = CRON_THREAD_STACK;
    cfg.priority     = THREAD_PRIO_1;
    cfg.thrdname     = "mimi_cron";

    OPERATE_RET rt = tal_thread_create_and_start(&s_cron_thread, NULL, NULL, cron_task_main, NULL, &cfg);
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "create cron thread failed: %d", rt);
        s_cron_thread = NULL;
        return rt;
    }

    MIMI_LOGI(TAG, "cron started (%d jobs, check every %u sec)", s_job_count,
              (unsigned)(MIMI_CRON_CHECK_INTERVAL_MS / 1000));
    return OPRT_OK;
}

void cron_service_stop(void)
{
    if (s_cron_thread) {
        (void)tal_thread_delete(s_cron_thread);
        s_cron_thread = NULL;
        MIMI_LOGI(TAG, "cron stopped");
    }
}

OPERATE_RET cron_add_job(cron_job_t *job)
{
    if (!job) {
        return OPRT_INVALID_PARM;
    }

    if (s_job_count >= MAX_CRON_JOBS) {
        MIMI_LOGW(TAG, "max cron jobs reached (%d)", MAX_CRON_JOBS);
        return OPRT_COM_ERROR;
    }

    cron_generate_id(job->id, sizeof(job->id));
    (void)cron_sanitize_destination(job);

    job->enabled  = true;
    job->last_run = 0;
    compute_initial_next_run(job);

    s_jobs[s_job_count] = *job;
    s_job_count++;

    (void)cron_save_jobs();

    MIMI_LOGI(TAG, "added cron job: %s (%s) kind=%s next=%lld", job->name, job->id,
              job->kind == CRON_KIND_EVERY ? "every" : "at", (long long)job->next_run);
    return OPRT_OK;
}

OPERATE_RET cron_remove_job(const char *job_id)
{
    if (!job_id || job_id[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    for (int i = 0; i < s_job_count; i++) {
        if (strcmp(s_jobs[i].id, job_id) == 0) {
            for (int j = i; j < s_job_count - 1; j++) {
                s_jobs[j] = s_jobs[j + 1];
            }
            s_job_count--;
            (void)cron_save_jobs();
            return OPRT_OK;
        }
    }

    return OPRT_NOT_FOUND;
}

void cron_list_jobs(const cron_job_t **jobs, int *count)
{
    if (jobs) {
        *jobs = s_jobs;
    }
    if (count) {
        *count = s_job_count;
    }
}
