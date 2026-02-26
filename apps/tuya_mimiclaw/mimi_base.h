#pragma once

#include "tal_api.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIMI_LOGE(tag, fmt, ...) PR_ERR("[%s] " fmt, tag, ##__VA_ARGS__)
#define MIMI_LOGW(tag, fmt, ...) PR_WARN("[%s] " fmt, tag, ##__VA_ARGS__)
#define MIMI_LOGI(tag, fmt, ...) PR_INFO("[%s] " fmt, tag, ##__VA_ARGS__)
#define MIMI_LOGD(tag, fmt, ...) PR_DEBUG("[%s] " fmt, tag, ##__VA_ARGS__)

#define MIMI_WAIT_FOREVER 0xFFFFFFFFu

static inline void mimi_build_kv_key(const char *ns, const char *key, char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }
    snprintf(out, out_size, "mimi.%s.%s", ns ? ns : "", key ? key : "");
}

static inline OPERATE_RET mimi_kv_set_string(const char *ns, const char *key, const char *value)
{
    if (!key || !value) {
        return OPRT_INVALID_PARM;
    }

    char full_key[64] = {0};
    mimi_build_kv_key(ns, key, full_key, sizeof(full_key));

    size_t len = strlen(value) + 1;
    return tal_kv_set(full_key, (const uint8_t *)value, len);
}

static inline OPERATE_RET mimi_kv_get_string(const char *ns, const char *key, char *out, size_t out_size)
{
    if (!key || !out || out_size == 0) {
        return OPRT_INVALID_PARM;
    }

    char full_key[64] = {0};
    mimi_build_kv_key(ns, key, full_key, sizeof(full_key));

    uint8_t    *buf = NULL;
    size_t      len = 0;
    OPERATE_RET rt  = tal_kv_get(full_key, &buf, &len);
    if (rt != OPRT_OK || !buf || len == 0) {
        out[0] = '\0';
        if (buf) {
            tal_kv_free(buf);
        }
        return (rt == OPRT_OK) ? OPRT_NOT_FOUND : rt;
    }

    size_t copy_len = (len < out_size - 1) ? len : (out_size - 1);
    memcpy(out, buf, copy_len);
    out[copy_len] = '\0';
    tal_kv_free(buf);
    return OPRT_OK;
}

static inline OPERATE_RET mimi_kv_del(const char *ns, const char *key)
{
    if (!key) {
        return OPRT_INVALID_PARM;
    }

    char full_key[64] = {0};
    mimi_build_kv_key(ns, key, full_key, sizeof(full_key));
    return tal_kv_del(full_key);
}
