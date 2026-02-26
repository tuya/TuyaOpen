#include "tool_files.h"

#include "cJSON.h"
#include "tal_fs.h"

#include <sys/stat.h>

static const char *TAG = "tool_files";

#define MAX_FILE_SIZE (32 * 1024)

static bool validate_path(const char *path)
{
    if (!path) {
        return false;
    }
    if (strncmp(path, "/spiffs/", 8) != 0) {
        return false;
    }
    if (strstr(path, "..") != NULL) {
        return false;
    }
    return true;
}

OPERATE_RET tool_read_file_execute(const char *input_json, char *output, size_t output_size)
{
    if (!input_json || !output || output_size == 0) {
        return OPRT_INVALID_PARM;
    }

    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return OPRT_CJSON_PARSE_ERR;
    }

    const char *path = cJSON_GetStringValue(cJSON_GetObjectItem(root, "path"));
    if (!validate_path(path)) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: invalid path");
        return OPRT_INVALID_PARM;
    }

    TUYA_FILE f = tal_fopen(path, "r");
    if (!f) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: file not found");
        return OPRT_NOT_FOUND;
    }

    size_t max_read = output_size - 1;
    if (max_read > MAX_FILE_SIZE) {
        max_read = MAX_FILE_SIZE;
    }

    int n = tal_fread(output, (int)max_read, f);
    if (n < 0) {
        tal_fclose(f);
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: read failed");
        return OPRT_COM_ERROR;
    }
    output[n] = '\0';

    tal_fclose(f);
    cJSON_Delete(root);
    MIMI_LOGI(TAG, "read_file path=%s bytes=%u", path, (unsigned)n);
    return OPRT_OK;
}

OPERATE_RET tool_write_file_execute(const char *input_json, char *output, size_t output_size)
{
    if (!input_json || !output || output_size == 0) {
        return OPRT_INVALID_PARM;
    }

    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return OPRT_CJSON_PARSE_ERR;
    }

    const char *path    = cJSON_GetStringValue(cJSON_GetObjectItem(root, "path"));
    const char *content = cJSON_GetStringValue(cJSON_GetObjectItem(root, "content"));

    if (!validate_path(path) || !content) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: invalid path/content");
        return OPRT_INVALID_PARM;
    }

    TUYA_FILE f = tal_fopen(path, "w");
    if (!f) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: open file failed");
        return OPRT_FILE_OPEN_FAILED;
    }

    int wn = tal_fwrite((void *)content, (int)strlen(content), f);
    tal_fclose(f);
    if (wn < 0) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: write failed");
        return OPRT_COM_ERROR;
    }

    snprintf(output, output_size, "OK: wrote %u bytes", (unsigned)strlen(content));
    cJSON_Delete(root);
    return OPRT_OK;
}

OPERATE_RET tool_edit_file_execute(const char *input_json, char *output, size_t output_size)
{
    if (!input_json || !output || output_size == 0) {
        return OPRT_INVALID_PARM;
    }

    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return OPRT_CJSON_PARSE_ERR;
    }

    const char *path  = cJSON_GetStringValue(cJSON_GetObjectItem(root, "path"));
    const char *old_s = cJSON_GetStringValue(cJSON_GetObjectItem(root, "old_string"));
    const char *new_s = cJSON_GetStringValue(cJSON_GetObjectItem(root, "new_string"));

    if (!validate_path(path) || !old_s || !new_s) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: invalid parameters");
        return OPRT_INVALID_PARM;
    }

    TUYA_FILE f = tal_fopen(path, "r");
    if (!f) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: file not found");
        return OPRT_NOT_FOUND;
    }

    char *buf = (char *)malloc(MAX_FILE_SIZE + 1);
    if (!buf) {
        tal_fclose(f);
        cJSON_Delete(root);
        return OPRT_MALLOC_FAILED;
    }

    int n = tal_fread(buf, MAX_FILE_SIZE, f);
    tal_fclose(f);
    if (n < 0) {
        free(buf);
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: read failed");
        return OPRT_COM_ERROR;
    }
    buf[n] = '\0';

    char *pos = strstr(buf, old_s);
    if (!pos) {
        free(buf);
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: old_string not found");
        return OPRT_NOT_FOUND;
    }

    size_t prefix_len = (size_t)(pos - buf);
    size_t suffix_off = prefix_len + strlen(old_s);
    size_t need       = prefix_len + strlen(new_s) + strlen(buf + suffix_off) + 1;

    char *new_buf = (char *)malloc(need);
    if (!new_buf) {
        free(buf);
        cJSON_Delete(root);
        return OPRT_MALLOC_FAILED;
    }

    memcpy(new_buf, buf, prefix_len);
    memcpy(new_buf + prefix_len, new_s, strlen(new_s));
    strcpy(new_buf + prefix_len + strlen(new_s), buf + suffix_off);

    f = tal_fopen(path, "w");
    if (!f) {
        free(new_buf);
        free(buf);
        cJSON_Delete(root);
        return OPRT_FILE_OPEN_FAILED;
    }

    (void)tal_fwrite(new_buf, (int)strlen(new_buf), f);
    tal_fclose(f);

    snprintf(output, output_size, "OK: edit done");
    free(new_buf);
    free(buf);
    cJSON_Delete(root);
    return OPRT_OK;
}

OPERATE_RET tool_list_dir_execute(const char *input_json, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return OPRT_INVALID_PARM;
    }

    const char *prefix = "/spiffs/";

    cJSON *root = NULL;
    if (input_json && input_json[0]) {
        root = cJSON_Parse(input_json);
        if (root) {
            const char *prefix_in = cJSON_GetStringValue(cJSON_GetObjectItem(root, "prefix"));
            if (prefix_in && validate_path(prefix_in)) {
                prefix = prefix_in;
            }
        }
    }

    TUYA_DIR dir = NULL;
    if (tal_dir_open(prefix, &dir) != OPRT_OK || !dir) {
        if (root) {
            cJSON_Delete(root);
        }
        snprintf(output, output_size, "Error: open dir failed");
        return OPRT_DIR_OPEN_FAILED;
    }

    size_t off = 0;
    off += snprintf(output + off, output_size - off, "Dir: %s\n", prefix);

    while (off < output_size - 2) {
        TUYA_FILEINFO info = NULL;
        if (tal_dir_read(dir, &info) != OPRT_OK || !info) {
            break;
        }

        const char *name = NULL;
        if (tal_dir_name(info, &name) != OPRT_OK || !name) {
            continue;
        }

        off += snprintf(output + off, output_size - off, "- %s\n", name);
    }

    tal_dir_close(dir);
    if (root) {
        cJSON_Delete(root);
    }
    return OPRT_OK;
}
