#include "im_utils.h"

void im_safe_copy(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_size, "%s", src);
}

uint16_t im_parse_http_status(const char *raw_resp)
{
    if (!raw_resp || strncmp(raw_resp, "HTTP/", 5) != 0) {
        return 0;
    }
    const char *sp = strchr(raw_resp, ' ');
    return sp ? (uint16_t)atoi(sp + 1) : 0;
}

int im_find_header_end(const char *buf, int len)
{
    if (!buf || len < 4) {
        return -1;
    }
    for (int i = 0; i <= len - 4; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            return i + 4;
        }
    }
    return -1;
}

uint64_t im_fnv1a64(const char *s)
{
    uint64_t h = 14695981039346656037ULL;
    if (!s) {
        return h;
    }
    while (*s) {
        h ^= (unsigned char)(*s++);
        h *= 1099511628211ULL;
    }
    return h;
}

const char *im_json_str(cJSON *obj, const char *key, const char *fallback)
{
    cJSON *item = obj ? cJSON_GetObjectItem(obj, key) : NULL;
    return (cJSON_IsString(item) && item->valuestring) ? item->valuestring : fallback;
}

int im_json_int(cJSON *obj, const char *key, int fallback)
{
    cJSON *item = obj ? cJSON_GetObjectItem(obj, key) : NULL;
    return cJSON_IsNumber(item) ? (int)item->valuedouble : fallback;
}

uint32_t im_json_uint(cJSON *obj, const char *key, uint32_t fallback)
{
    cJSON *item = obj ? cJSON_GetObjectItem(obj, key) : NULL;
    if (!cJSON_IsNumber(item) || item->valuedouble < 0) {
        return fallback;
    }
    return (uint32_t)item->valuedouble;
}
