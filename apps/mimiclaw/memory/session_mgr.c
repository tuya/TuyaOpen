#include "session_mgr.h"
#include "mimi_config.h"

#include "cJSON.h"
// #include "tal_fs.h"

static const char *TAG = "session";

static void session_path(const char *chat_id, char *buf, size_t size)
{
    snprintf(buf, size, "%s/tg_%s.jsonl", MIMI_SPIFFS_SESSION_DIR, chat_id);
}

static void session_normalize_chat_id(const char *chat_id, char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (!chat_id || chat_id[0] == '\0') {
        return;
    }

    const char *start = chat_id;
    if (strncmp(chat_id, "tg_", 3) == 0) {
        start = chat_id + 3;
    }

    const char *suffix = strstr(start, ".jsonl");
    size_t      n      = suffix ? (size_t)(suffix - start) : strlen(start);
    if (n >= out_size) {
        n = out_size - 1;
    }
    if (n > 0) {
        memcpy(out, start, n);
    }
    out[n] = '\0';
}

OPERATE_RET session_mgr_init(void)
{
    MIMI_LOGI(TAG, "session manager init: %s", MIMI_SPIFFS_SESSION_DIR);
    return OPRT_OK;
}

OPERATE_RET session_append(const char *chat_id, const char *role, const char *content)
{
    if (!chat_id || !role || !content) {
        return OPRT_INVALID_PARM;
    }

    char path[128] = {0};
    session_path(chat_id, path, sizeof(path));

    TUYA_FILE f = mimi_fopen(path, "a");
    if (!f) {
        return OPRT_FILE_OPEN_FAILED;
    }

    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        mimi_fclose(f);
        return OPRT_CR_CJSON_ERR;
    }

    cJSON_AddStringToObject(obj, "role", role);
    cJSON_AddStringToObject(obj, "content", content);
    cJSON_AddNumberToObject(obj, "ts", (double)tal_time_get_posix());

    char *line = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);

    if (!line) {
        mimi_fclose(f);
        return OPRT_CR_CJSON_ERR;
    }

    (void)mimi_fwrite(line, (int)strlen(line), f);
    (void)mimi_fwrite("\n", 1, f);
    cJSON_free(line);
    mimi_fclose(f);
    return OPRT_OK;
}

OPERATE_RET session_get_history_json(const char *chat_id, char *buf, size_t size, int max_msgs)
{
    if (!chat_id || !buf || size == 0 || max_msgs <= 0) {
        return OPRT_INVALID_PARM;
    }
    if (max_msgs > MIMI_SESSION_MAX_MSGS) {
        max_msgs = MIMI_SESSION_MAX_MSGS;
    }

    char path[128] = {0};
    session_path(chat_id, path, sizeof(path));

    TUYA_FILE f = mimi_fopen(path, "r");
    if (!f) {
        snprintf(buf, size, "[]");
        return OPRT_OK;
    }

    cJSON *ring[MIMI_SESSION_MAX_MSGS] = {0};
    int    count                       = 0;
    int    write_idx                   = 0;

    char line[2048] = {0};
    while (mimi_fgets(line, (int)sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        if (line[0] == '\0') {
            continue;
        }

        cJSON *obj = cJSON_Parse(line);
        if (!obj) {
            continue;
        }

        if (count >= max_msgs && ring[write_idx]) {
            cJSON_Delete(ring[write_idx]);
        }
        ring[write_idx] = obj;
        write_idx       = (write_idx + 1) % max_msgs;
        if (count < max_msgs) {
            count++;
        }
    }
    mimi_fclose(f);

    cJSON *arr   = cJSON_CreateArray();
    int    start = (count < max_msgs) ? 0 : write_idx;
    for (int i = 0; i < count; i++) {
        int    idx = (start + i) % max_msgs;
        cJSON *src = ring[idx];

        cJSON *entry   = cJSON_CreateObject();
        cJSON *role    = cJSON_GetObjectItem(src, "role");
        cJSON *content = cJSON_GetObjectItem(src, "content");

        cJSON_AddStringToObject(entry, "role", cJSON_IsString(role) ? role->valuestring : "user");
        cJSON_AddStringToObject(entry, "content", cJSON_IsString(content) ? content->valuestring : "");
        cJSON_AddItemToArray(arr, entry);
    }

    for (int i = 0; i < count; i++) {
        int idx = (start + i) % max_msgs;
        cJSON_Delete(ring[idx]);
    }

    char *json_str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    if (!json_str) {
        snprintf(buf, size, "[]");
        return OPRT_CR_CJSON_ERR;
    }

    strncpy(buf, json_str, size - 1);
    buf[size - 1] = '\0';
    cJSON_free(json_str);
    return OPRT_OK;
}

OPERATE_RET session_clear(const char *chat_id)
{
    if (!chat_id || chat_id[0] == '\0') {
        return OPRT_INVALID_PARM;
    }
    if (strchr(chat_id, '/') || strchr(chat_id, '\\')) {
        return OPRT_INVALID_PARM;
    }

    char normalized[96] = {0};
    session_normalize_chat_id(chat_id, normalized, sizeof(normalized));
    if (normalized[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    char path[128] = {0};
    session_path(normalized, path, sizeof(path));

    return (mimi_fs_remove(path) == OPRT_OK) ? OPRT_OK : OPRT_NOT_FOUND;
}

OPERATE_RET session_clear_all(uint32_t *out_removed)
{
    uint32_t removed = 0;
    if (out_removed) {
        *out_removed = 0;
    }

    TUYA_DIR dir = NULL;
    if (mimi_dir_open(MIMI_SPIFFS_SESSION_DIR, &dir) != OPRT_OK || !dir) {
        return OPRT_FILE_OPEN_FAILED;
    }

    while (1) {
        TUYA_FILEINFO info = NULL;
        if (mimi_dir_read(dir, &info) != OPRT_OK || !info) {
            break;
        }

        const char *name = NULL;
        if (mimi_dir_name(info, &name) != OPRT_OK || !name || name[0] == '\0') {
            continue;
        }
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        BOOL_T is_regular = FALSE;
        if (mimi_dir_is_regular(info, &is_regular) != OPRT_OK || !is_regular) {
            continue;
        }

        char path[160] = {0};
        snprintf(path, sizeof(path), "%s/%s", MIMI_SPIFFS_SESSION_DIR, name);
        if (mimi_fs_remove(path) == OPRT_OK) {
            removed++;
        }
    }

    mimi_dir_close(dir);
    if (out_removed) {
        *out_removed = removed;
    }
    return (removed > 0) ? OPRT_OK : OPRT_NOT_FOUND;
}

OPERATE_RET session_list(session_list_cb_t cb, void *user_data, uint32_t *out_count)
{
    uint32_t count = 0;
    if (out_count) {
        *out_count = 0;
    }

    TUYA_DIR dir = NULL;
    if (mimi_dir_open(MIMI_SPIFFS_SESSION_DIR, &dir) != OPRT_OK || !dir) {
        MIMI_LOGW(TAG, "open session dir failed");
        return OPRT_FILE_OPEN_FAILED;
    }

    while (1) {
        TUYA_FILEINFO info = NULL;
        if (mimi_dir_read(dir, &info) != OPRT_OK || !info) {
            break;
        }

        const char *name = NULL;
        if (mimi_dir_name(info, &name) == OPRT_OK && name && name[0] != '\0') {
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
                continue;
            }

            count++;
            if (cb) {
                cb(name, user_data);
            }
        }
    }

    mimi_dir_close(dir);
    if (out_count) {
        *out_count = count;
    }
    return (count > 0) ? OPRT_OK : OPRT_NOT_FOUND;
}
