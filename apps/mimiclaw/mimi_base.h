#pragma once

#include "tal_api.h"
#include "mimi_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if MIMI_USE_SDCARD
#include "tkl_fs.h"
#define mimi_fopen            tkl_fopen
#define mimi_fclose           tkl_fclose
#define mimi_fread            tkl_fread
#define mimi_fwrite           tkl_fwrite
#define mimi_fseek            tkl_fseek
#define mimi_ftell            tkl_ftell
#define mimi_fgets            tkl_fgets
#define mimi_feof             tkl_feof
#define mimi_fs_is_exist      tkl_fs_is_exist
#define mimi_fs_remove        tkl_fs_remove
#define mimi_fs_mkdir         tkl_fs_mkdir
#define mimi_fs_rename        tkl_fs_rename
#define mimi_dir_open         tkl_dir_open
#define mimi_dir_close        tkl_dir_close
#define mimi_dir_read         tkl_dir_read
#define mimi_dir_name         tkl_dir_name
#define mimi_dir_is_directory tkl_dir_is_directory
#define mimi_dir_is_regular   tkl_dir_is_regular
#define mimi_fgetsize         tkl_fgetsize
#else
#include "tal_fs.h"
#define mimi_fopen            tal_fopen
#define mimi_fclose           tal_fclose
#define mimi_fread            tal_fread
#define mimi_fwrite           tal_fwrite
#define mimi_fseek            tal_fseek
#define mimi_ftell            tal_ftell
#define mimi_fgets            tal_fgets
#define mimi_feof             tal_feof
#define mimi_fs_is_exist      tal_fs_is_exist
#define mimi_fs_remove        tal_fs_remove
#define mimi_fs_mkdir         tal_fs_mkdir
#define mimi_fs_rename        tal_fs_rename
#define mimi_dir_open         tal_dir_open
#define mimi_dir_close        tal_dir_close
#define mimi_dir_read         tal_dir_read
#define mimi_dir_name         tal_dir_name
#define mimi_dir_is_directory tal_dir_is_directory
#define mimi_dir_is_regular   tal_dir_is_regular
#define mimi_fgetsize         tal_fgetsize
#endif

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
