/**
 * @file local_store.c
 * @brief Local media storage: segment recorder, day index and segment reader
 * @version 2.0
 * @date 2026-09-02
 * @copyright Copyright (c) Tuya Inc.
 */
#include "local_store.h"
#include "tkl_fs.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_mutex.h"
#include "tal_semaphore.h"
#include "tal_thread.h"
#include "tal_system.h"
#include "tal_time_service.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef LOCAL_STORE_ROOT
#define LOCAL_STORE_ROOT "./media"
#endif
#ifndef LOCAL_STORE_REC_SUBDIR
#define LOCAL_STORE_REC_SUBDIR "rec"
#endif
#ifndef LOCAL_STORE_STAGE_KB
#define LOCAL_STORE_STAGE_KB 512
#endif
#ifndef LOCAL_STORE_WRITE_KB
#define LOCAL_STORE_WRITE_KB 64
#endif
#ifndef LOCAL_STORE_SYNC_MS
#define LOCAL_STORE_SYNC_MS 3000
#endif
#ifndef LOCAL_STORE_KEEP_DAYS
#define LOCAL_STORE_KEEP_DAYS 0
#endif
#ifndef LOCAL_STORE_READ_KB
#define LOCAL_STORE_READ_KB 320
#endif

#define LS_TMP_SUFFIX   ".tmp"
#define LS_MEDIA_EXT    ".media"
#define LS_IDX_EXT      ".idx"
#define LS_INDEX_FILE   "index.txt"
#define LS_STAGE_BYTES  ((uint32_t)LOCAL_STORE_STAGE_KB * 1024u)
#define LS_WRITE_BYTES  ((uint32_t)LOCAL_STORE_WRITE_KB * 1024u)
#define LS_READ_BYTES   ((uint32_t)LOCAL_STORE_READ_KB * 1024u)
#define LS_IDX_MAX      1024u
#define LS_SEG_DFLT_SEC 120u
#define LS_IDLE_FLUSH_MS 200u
#define LS_WAIT_MS      50u
#define LS_STOP_WAIT_MS 5000u
#define LS_THREAD_STACK 12288
#define LS_MAX_FRAME    (LS_READ_BYTES - sizeof(LOCAL_STORE_FRAME_HDR_T))
#define LS_START_TS_CAP 4096u

#define LS_REC_DATA 1u
#define LS_REC_CUT  2u

typedef struct {
    uint16_t type;
    uint16_t rsv;
    uint32_t len;
} LS_REC_HDR_T;

typedef struct {
    uint32_t rel_ms;
    uint32_t off;
} LS_IDX_ENT_T;

typedef BOOL_T (*LS_SEG_CB)(const LOCAL_STORE_SEG_T *seg, void *ctx);

struct LOCAL_STORE_READER {
    TUYA_FILE fp;
    uint8_t *buf;
    uint32_t valid;
    uint32_t used;
    uint64_t base; /* file offset of buf[0] */
    BOOL_T eof;
    char path[LOCAL_STORE_PATH_MAX];
};

static BOOL_T s_inited = FALSE;

static MUTEX_HANDLE  s_lock = NULL;
static SEM_HANDLE    s_wake = NULL;
static THREAD_HANDLE s_worker = NULL;
static volatile BOOL_T s_run = FALSE;
static volatile BOOL_T s_alive = FALSE;
static volatile BOOL_T s_accept = FALSE;

static uint8_t *s_ring = NULL;
static uint32_t s_ring_head = 0;
static uint32_t s_ring_tail = 0;
static volatile uint32_t s_ring_used = 0;
static uint8_t *s_stage = NULL;
static uint32_t s_stage_used = 0;
static LS_IDX_ENT_T *s_idx = NULL;
static uint32_t s_idx_cnt = 0;

static char     s_prefix[32];
static uint32_t s_seg_sec = LS_SEG_DFLT_SEC;

/* producer side, under s_lock */
static BOOL_T   s_seg_open = FALSE;
static volatile BOOL_T s_force_cut = FALSE;
static BOOL_T   s_wait_key = FALSE;
static uint32_t s_seg_start = 0;
static uint64_t s_seg_first_ms = 0;

/* writer side */
static TUYA_FILE s_fp = NULL;
static char     s_path[LOCAL_STORE_PATH_MAX];
static char     s_leaf[64];
static uint32_t s_file_start = 0;
static uint64_t s_file_bytes = 0;
static uint32_t s_last_rel_ms = 0;
static BOOL_T   s_dirty = FALSE;
static uint32_t s_last_sync_ms = 0;
static uint32_t s_last_data_ms = 0;

static LOCAL_STORE_REC_STAT_T s_stat;

/* ---------------------------------------------------------------------------
 * Paths
 * --------------------------------------------------------------------------- */
static BOOL_T __filename_is_safe(const char *filename)
{
    uint32_t i;

    if (filename == NULL || filename[0] == '\0') {
        return FALSE;
    }
    if (filename[0] == '/' || filename[0] == '\\') {
        return FALSE;
    }
    for (i = 0; filename[i] != '\0'; i++) {
        if (filename[i] == '/' || filename[i] == '\\') {
            return FALSE;
        }
        if (filename[i] == '.' && filename[i + 1] == '.') {
            return FALSE;
        }
    }
    return TRUE;
}

static OPERATE_RET __path_join(const char *parent, const char *child, char *out, uint32_t out_len)
{
    int n;
    uint32_t plen;

    if (parent == NULL || child == NULL || out == NULL || out_len == 0) {
        return OPRT_INVALID_PARM;
    }
    plen = (uint32_t)strlen(parent);
    if (plen > 0 && parent[plen - 1] == '/') {
        n = snprintf(out, out_len, "%s%s", parent, child);
    } else {
        n = snprintf(out, out_len, "%s/%s", parent, child);
    }
    if (n < 0 || (uint32_t)n >= out_len) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    return OPRT_OK;
}

/* tkl_fs_is_exist("/sdcard") fails even when FAT is mounted; never mkdir a mount root */
static BOOL_T __is_fs_mount_root(const char *path)
{
    return (path != NULL && strcmp(path, "/sdcard") == 0) ? TRUE : FALSE;
}

static OPERATE_RET __mkdir_one(const char *path)
{
    int rt;
    BOOL_T exists = FALSE;

    if (path == NULL || path[0] == '\0') {
        return OPRT_INVALID_PARM;
    }
    if (strcmp(path, ".") == 0 || __is_fs_mount_root(path)) {
        return OPRT_OK;
    }
    (void)tkl_fs_is_exist(path, &exists);
    if (exists) {
        return OPRT_OK;
    }
    rt = tkl_fs_mkdir(path);
    if (rt != 0) {
        exists = FALSE;
        (void)tkl_fs_is_exist(path, &exists);
        if (exists) {
            return OPRT_OK;
        }
        TAL_PR_ERR("tkl_fs_mkdir %s failed: %d", path, rt);
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

OPERATE_RET local_store_ensure_dir(const char *dir)
{
    char tmp[LOCAL_STORE_PATH_MAX];
    uint32_t i;
    uint32_t len;
    OPERATE_RET rt;

    if (dir == NULL || dir[0] == '\0') {
        return OPRT_INVALID_PARM;
    }
    len = (uint32_t)strlen(dir);
    if (len >= LOCAL_STORE_PATH_MAX) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    memcpy(tmp, dir, len + 1);
    i = (tmp[0] == '/') ? 1 : 0;
    for (; i < len; i++) {
        if (tmp[i] != '/') {
            continue;
        }
        tmp[i] = '\0';
        rt = __mkdir_one(tmp);
        tmp[i] = '/';
        if (rt != OPRT_OK) {
            return rt;
        }
    }
    return __mkdir_one(dir);
}

const char *local_store_root(void)
{
    return LOCAL_STORE_ROOT;
}

static OPERATE_RET __rec_base(char *out, uint32_t out_len)
{
    return __path_join(LOCAL_STORE_ROOT, LOCAL_STORE_REC_SUBDIR, out, out_len);
}

static OPERATE_RET __ts_to_ymd(uint32_t ts, uint32_t *y, uint32_t *m, uint32_t *d)
{
    POSIX_TM_S tm;
    OPERATE_RET rt;

    memset(&tm, 0, sizeof(tm));
    rt = tal_time_get_local_time_custom((TIME_T)ts, &tm);
    if (rt != OPRT_OK) {
        rt = tal_time_get(&tm);
        if (rt != OPRT_OK) {
            return rt;
        }
    }
    *y = (uint32_t)(tm.tm_year + 1900);
    *m = (uint32_t)(tm.tm_mon + 1);
    *d = (uint32_t)tm.tm_mday;
    return OPRT_OK;
}

static OPERATE_RET __day_dir(uint32_t year, uint32_t month, uint32_t day, char *out, uint32_t out_len)
{
    OPERATE_RET rt;
    char date_dir[16];
    char rec_base[LOCAL_STORE_PATH_MAX];
    int n;

    if (out == NULL || out_len == 0 || month < 1 || month > 12 || day < 1 || day > 31) {
        return OPRT_INVALID_PARM;
    }
    n = snprintf(date_dir, sizeof(date_dir), "%04u-%02u-%02u", year, month, day);
    if (n < 0 || (uint32_t)n >= sizeof(date_dir)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    rt = __rec_base(rec_base, sizeof(rec_base));
    if (rt != OPRT_OK) {
        return rt;
    }
    return __path_join(rec_base, date_dir, out, out_len);
}

static OPERATE_RET __index_path(uint32_t year, uint32_t month, uint32_t day, char *out, uint32_t out_len)
{
    OPERATE_RET rt;
    char day_path[LOCAL_STORE_PATH_MAX];

    rt = __day_dir(year, month, day, day_path, sizeof(day_path));
    if (rt != OPRT_OK) {
        return rt;
    }
    return __path_join(day_path, LS_INDEX_FILE, out, out_len);
}

/* "<name>.media" -> "<name>.idx", else append ".idx" */
static OPERATE_RET __idx_path_of(const char *media_path, char *out, uint32_t out_len)
{
    uint32_t len;
    uint32_t ext = (uint32_t)strlen(LS_MEDIA_EXT);
    int n;

    if (media_path == NULL || out == NULL) {
        return OPRT_INVALID_PARM;
    }
    len = (uint32_t)strlen(media_path);
    if (len > ext && strcmp(media_path + len - ext, LS_MEDIA_EXT) == 0) {
        len -= ext;
    }
    if (len + strlen(LS_IDX_EXT) >= out_len) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    memcpy(out, media_path, len);
    n = snprintf(out + len, out_len - len, "%s", LS_IDX_EXT);
    return (n < 0) ? OPRT_COM_ERROR : OPRT_OK;
}

static BOOL_T __leaf_is_media(const char *leaf)
{
    uint32_t len = (uint32_t)strlen(leaf);
    uint32_t ext = (uint32_t)strlen(LS_MEDIA_EXT);

    return (len > ext && strcmp(leaf + len - ext, LS_MEDIA_EXT) == 0) ? TRUE : FALSE;
}

/* "<prefix>_<start>.media" -> start */
static BOOL_T __leaf_start_ts(const char *leaf, uint32_t *start)
{
    const char *us = strrchr(leaf, '_');
    char *end = NULL;
    unsigned long v;

    if (us == NULL || !__leaf_is_media(leaf)) {
        return FALSE;
    }
    v = strtoul(us + 1, &end, 10);
    if (end == NULL || end == us + 1 || strcmp(end, LS_MEDIA_EXT) != 0 || v == 0) {
        return FALSE;
    }
    *start = (uint32_t)v;
    return TRUE;
}

OPERATE_RET local_store_make_rec_path(const char *filename, char *out, uint32_t out_len)
{
    OPERATE_RET rt;
    uint32_t y, m, d;
    char day_path[LOCAL_STORE_PATH_MAX];

    if (!__filename_is_safe(filename) || out == NULL || out_len == 0) {
        return OPRT_INVALID_PARM;
    }
    rt = local_store_init();
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __ts_to_ymd((uint32_t)tal_time_get_posix(), &y, &m, &d);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __day_dir(y, m, d, day_path, sizeof(day_path));
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = local_store_ensure_dir(day_path);
    if (rt != OPRT_OK) {
        return rt;
    }
    return __path_join(day_path, filename, out, out_len);
}

OPERATE_RET local_store_make_tmp_path(const char *final_path, char *out, uint32_t out_len)
{
    int n;

    if (final_path == NULL || final_path[0] == '\0' || out == NULL || out_len == 0) {
        return OPRT_INVALID_PARM;
    }
    n = snprintf(out, out_len, "%s%s", final_path, LS_TMP_SUFFIX);
    if (n < 0 || (uint32_t)n >= out_len) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    return OPRT_OK;
}

OPERATE_RET local_store_commit(const char *tmp_path, const char *final_path)
{
    int rt;

    if (tmp_path == NULL || final_path == NULL) {
        return OPRT_INVALID_PARM;
    }
    rt = tkl_fs_rename(tmp_path, final_path);
    if (rt != 0) {
        TAL_PR_ERR("local_store_commit rename %s -> %s failed: %d", tmp_path, final_path, rt);
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

/* ---------------------------------------------------------------------------
 * Day index
 * --------------------------------------------------------------------------- */
OPERATE_RET local_store_index_append(uint32_t start_ts, uint32_t end_ts, uint16_t type, const char *leaf)
{
    OPERATE_RET rt;
    char day_path[LOCAL_STORE_PATH_MAX];
    char idx_path[LOCAL_STORE_PATH_MAX];
    char line[192];
    TUYA_FILE fp;
    int n;
    uint32_t y, m, d;

    if (!__filename_is_safe(leaf) || end_ts < start_ts) {
        return OPRT_INVALID_PARM;
    }
    rt = __ts_to_ymd(start_ts, &y, &m, &d);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __day_dir(y, m, d, day_path, sizeof(day_path));
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = local_store_ensure_dir(day_path);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __path_join(day_path, LS_INDEX_FILE, idx_path, sizeof(idx_path));
    if (rt != OPRT_OK) {
        return rt;
    }
    n = snprintf(line, sizeof(line), "%u %u %u %s\n", start_ts, end_ts, (uint32_t)type, leaf);
    if (n < 0 || (uint32_t)n >= sizeof(line)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    fp = tkl_fopen(idx_path, "a");
    if (fp == NULL) {
        TAL_PR_ERR("open index %s failed", idx_path);
        return OPRT_FILE_OPEN_FAILED;
    }
    if (tkl_fwrite((void *)line, n, fp) != n) {
        tkl_fclose(fp);
        return OPRT_FILE_WRITE_FAILED;
    }
    tkl_fclose(fp);
    return OPRT_OK;
}

static OPERATE_RET __index_scan(uint32_t year, uint32_t month, uint32_t day, LS_SEG_CB cb, void *ctx)
{
    OPERATE_RET rt;
    char idx_path[LOCAL_STORE_PATH_MAX];
    char line[192];
    char leaf[64];
    TUYA_FILE fp;
    LOCAL_STORE_SEG_T seg;
    uint32_t start_ts, end_ts, type;
    BOOL_T exists = FALSE;

    rt = __index_path(year, month, day, idx_path, sizeof(idx_path));
    if (rt != OPRT_OK) {
        return rt;
    }
    (void)tkl_fs_is_exist(idx_path, &exists);
    if (!exists) {
        return OPRT_OK;
    }
    fp = tkl_fopen(idx_path, "r");
    if (fp == NULL) {
        return OPRT_FILE_OPEN_FAILED;
    }
    while (tkl_fgets(line, (int)sizeof(line), fp) != NULL) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') {
            continue;
        }
        leaf[0] = '\0';
        if (sscanf(line, "%u %u %u %63s", &start_ts, &end_ts, &type, leaf) < 4 || !__filename_is_safe(leaf) ||
            !__leaf_is_media(leaf)) {
            continue;
        }
        seg.start_ts = start_ts;
        seg.end_ts = end_ts;
        seg.type = (uint16_t)type;
        snprintf(seg.leaf, sizeof(seg.leaf), "%s", leaf);
        if (!cb(&seg, ctx)) {
            break;
        }
    }
    tkl_fclose(fp);
    return OPRT_OK;
}

typedef struct {
    LOCAL_STORE_SEG_T *arr;
    uint32_t cap;
    uint32_t filled;
} LS_COLLECT_CTX_T;

static BOOL_T __collect_cb(const LOCAL_STORE_SEG_T *seg, void *ctx)
{
    LS_COLLECT_CTX_T *c = (LS_COLLECT_CTX_T *)ctx;

    if (c->filled >= c->cap) {
        return FALSE;
    }
    c->arr[c->filled++] = *seg;
    return TRUE;
}

OPERATE_RET local_store_query_day(uint32_t year, uint32_t month, uint32_t day, LOCAL_STORE_SEG_T *arr,
                                  uint32_t *count)
{
    LS_COLLECT_CTX_T c;
    OPERATE_RET rt;

    if (arr == NULL || count == NULL || *count == 0) {
        return OPRT_INVALID_PARM;
    }
    c.arr = arr;
    c.cap = *count;
    c.filled = 0;
    *count = 0;
    rt = __index_scan(year, month, day, __collect_cb, &c);
    if (rt != OPRT_OK) {
        return rt;
    }
    *count = c.filled;
    return OPRT_OK;
}

OPERATE_RET local_store_query_month(uint32_t year, uint32_t month, uint32_t *day_bitmap)
{
    OPERATE_RET rt;
    uint32_t d;
    uint32_t bits = 0;
    LOCAL_STORE_SEG_T one;
    uint32_t one_count;

    if (day_bitmap == NULL || month < 1 || month > 12) {
        return OPRT_INVALID_PARM;
    }
    *day_bitmap = 0;
    rt = local_store_init();
    if (rt != OPRT_OK) {
        return rt;
    }
    for (d = 1; d <= 31; d++) {
        one_count = 1;
        if (local_store_query_day(year, month, d, &one, &one_count) == OPRT_OK && one_count > 0) {
            bits |= (1U << (d - 1));
        }
    }
    *day_bitmap = bits;
    return OPRT_OK;
}

OPERATE_RET local_store_day_file_path(uint32_t year, uint32_t month, uint32_t day, const char *leaf, char *out,
                                      uint32_t out_len)
{
    OPERATE_RET rt;
    char day_path[LOCAL_STORE_PATH_MAX];

    if (!__filename_is_safe(leaf) || out == NULL || out_len == 0) {
        return OPRT_INVALID_PARM;
    }
    rt = __day_dir(year, month, day, day_path, sizeof(day_path));
    if (rt != OPRT_OK) {
        return rt;
    }
    return __path_join(day_path, leaf, out, out_len);
}

typedef struct {
    uint32_t play_ts;
    uint32_t best_dist;
    BOOL_T found;
    LOCAL_STORE_SEG_T best;
} LS_FIND_CTX_T;

static BOOL_T __find_cb(const LOCAL_STORE_SEG_T *seg, void *ctx)
{
    LS_FIND_CTX_T *c = (LS_FIND_CTX_T *)ctx;
    uint32_t dist;

    if (c->play_ts >= seg->start_ts && c->play_ts <= seg->end_ts) {
        c->best = *seg;
        c->best_dist = 0;
        c->found = TRUE;
        return FALSE;
    }
    dist = (c->play_ts < seg->start_ts) ? seg->start_ts - c->play_ts : c->play_ts - seg->end_ts;
    if (!c->found || dist < c->best_dist) {
        c->best = *seg;
        c->best_dist = dist;
        c->found = TRUE;
    }
    return TRUE;
}

OPERATE_RET local_store_find_by_play_ts(uint32_t play_ts, LOCAL_STORE_SEG_T *out_seg, char *out_path,
                                        uint32_t path_len)
{
    OPERATE_RET rt;
    LS_FIND_CTX_T c;
    uint32_t y, m, d;

    if (out_path == NULL || path_len == 0 || play_ts == 0) {
        return OPRT_INVALID_PARM;
    }
    out_path[0] = '\0';
    rt = __ts_to_ymd(play_ts, &y, &m, &d);
    if (rt != OPRT_OK) {
        return rt;
    }
    memset(&c, 0, sizeof(c));
    c.play_ts = play_ts;
    rt = __index_scan(y, m, d, __find_cb, &c);
    if (rt != OPRT_OK) {
        return rt;
    }
    if (!c.found) {
        return OPRT_NOT_FOUND;
    }
    if (out_seg != NULL) {
        *out_seg = c.best;
    }
    return local_store_day_file_path(y, m, d, c.best.leaf, out_path, path_len);
}

typedef struct {
    uint32_t after;
    BOOL_T found;
    LOCAL_STORE_SEG_T best;
} LS_NEXT_CTX_T;

static BOOL_T __next_cb(const LOCAL_STORE_SEG_T *seg, void *ctx)
{
    LS_NEXT_CTX_T *c = (LS_NEXT_CTX_T *)ctx;

    if (seg->start_ts > c->after && (!c->found || seg->start_ts < c->best.start_ts)) {
        c->best = *seg;
        c->found = TRUE;
    }
    return TRUE;
}

OPERATE_RET local_store_find_next_seg(uint32_t start_ts, LOCAL_STORE_SEG_T *out_seg, char *out_path,
                                      uint32_t path_len)
{
    OPERATE_RET rt;
    LS_NEXT_CTX_T c;
    uint32_t y, m, d;

    if (out_path == NULL || path_len == 0 || start_ts == 0) {
        return OPRT_INVALID_PARM;
    }
    out_path[0] = '\0';
    rt = __ts_to_ymd(start_ts, &y, &m, &d);
    if (rt != OPRT_OK) {
        return rt;
    }
    memset(&c, 0, sizeof(c));
    c.after = start_ts;
    rt = __index_scan(y, m, d, __next_cb, &c);
    if (rt != OPRT_OK) {
        return rt;
    }
    if (!c.found) {
        return OPRT_NOT_FOUND;
    }
    if (out_seg != NULL) {
        *out_seg = c.best;
    }
    return local_store_day_file_path(y, m, d, c.best.leaf, out_path, path_len);
}

/* ---------------------------------------------------------------------------
 * Day directories: removal, retention
 * --------------------------------------------------------------------------- */
static OPERATE_RET __remove_dir_files(const char *dir_path)
{
    TUYA_DIR dir = NULL;
    TUYA_FILEINFO info = NULL;
    const char *name = NULL;
    char path[LOCAL_STORE_PATH_MAX];
    BOOL_T is_dir = FALSE;

    if (tkl_dir_open(dir_path, &dir) != 0) {
        return OPRT_FILE_OPEN_FAILED;
    }
    while (tkl_dir_read(dir, &info) == 0 && info != NULL) {
        if (tkl_dir_name(info, &name) != 0 || name == NULL) {
            continue;
        }
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        is_dir = FALSE;
        (void)tkl_dir_is_directory(info, &is_dir);
        if (is_dir) {
            continue;
        }
        if (__path_join(dir_path, name, path, sizeof(path)) == OPRT_OK) {
            (void)tkl_fs_remove(path);
        }
    }
    tkl_dir_close(dir);
    return OPRT_OK;
}

OPERATE_RET local_store_remove_day(uint32_t year, uint32_t month, uint32_t day)
{
    OPERATE_RET rt;
    char day_path[LOCAL_STORE_PATH_MAX];
    BOOL_T exists = FALSE;

    rt = __day_dir(year, month, day, day_path, sizeof(day_path));
    if (rt != OPRT_OK) {
        return rt;
    }
    (void)tkl_fs_is_exist(day_path, &exists);
    if (!exists) {
        return OPRT_NOT_FOUND;
    }
    rt = __remove_dir_files(day_path);
    if (rt != OPRT_OK) {
        return rt;
    }
    if (tkl_fs_remove(day_path) != 0) {
        TAL_PR_WARN("local_store rmdir %s failed", day_path);
        return OPRT_COM_ERROR;
    }
    TAL_PR_NOTICE("local_store removed day %s", day_path);
    return OPRT_OK;
}

static BOOL_T __parse_day_name(const char *name, uint32_t *y, uint32_t *m, uint32_t *d)
{
    unsigned int yy, mm, dd;

    if (strlen(name) != 10 || name[4] != '-' || name[7] != '-') {
        return FALSE;
    }
    if (sscanf(name, "%4u-%2u-%2u", &yy, &mm, &dd) != 3) {
        return FALSE;
    }
    if (mm < 1 || mm > 12 || dd < 1 || dd > 31) {
        return FALSE;
    }
    *y = yy;
    *m = mm;
    *d = dd;
    return TRUE;
}

static int32_t __days_from_civil(uint32_t y, uint32_t m, uint32_t d)
{
    int32_t yy = (int32_t)y - (m <= 2 ? 1 : 0);
    int32_t era = (yy >= 0 ? yy : yy - 399) / 400;
    uint32_t yoe = (uint32_t)(yy - era * 400);
    uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

    return era * 146097 + (int32_t)doe - 719468;
}

typedef BOOL_T (*LS_DAY_CB)(uint32_t y, uint32_t m, uint32_t d, void *ctx);

static OPERATE_RET __for_each_day(LS_DAY_CB cb, void *ctx)
{
    char rec_base[LOCAL_STORE_PATH_MAX];
    TUYA_DIR dir = NULL;
    TUYA_FILEINFO info = NULL;
    const char *name = NULL;
    uint32_t y, m, d;

    if (__rec_base(rec_base, sizeof(rec_base)) != OPRT_OK) {
        return OPRT_COM_ERROR;
    }
    if (tkl_dir_open(rec_base, &dir) != 0) {
        return OPRT_FILE_OPEN_FAILED;
    }
    while (tkl_dir_read(dir, &info) == 0 && info != NULL) {
        if (tkl_dir_name(info, &name) != 0 || name == NULL) {
            continue;
        }
        if (!__parse_day_name(name, &y, &m, &d)) {
            continue;
        }
        if (!cb(y, m, d, ctx)) {
            break;
        }
    }
    tkl_dir_close(dir);
    return OPRT_OK;
}

typedef struct {
    int32_t today;
    int32_t oldest;
    uint32_t y, m, d;
    BOOL_T found;
} LS_OLDEST_CTX_T;

static BOOL_T __oldest_cb(uint32_t y, uint32_t m, uint32_t d, void *ctx)
{
    LS_OLDEST_CTX_T *c = (LS_OLDEST_CTX_T *)ctx;
    int32_t days = __days_from_civil(y, m, d);

    if (days >= c->today) {
        return TRUE;
    }
    if (!c->found || days < c->oldest) {
        c->oldest = days;
        c->y = y;
        c->m = m;
        c->d = d;
        c->found = TRUE;
    }
    return TRUE;
}

static int32_t __today_days(void)
{
    uint32_t y, m, d;

    if (__ts_to_ymd((uint32_t)tal_time_get_posix(), &y, &m, &d) != OPRT_OK) {
        return 0;
    }
    return __days_from_civil(y, m, d);
}

/* Remove the oldest day other than today; TRUE when something was freed */
static BOOL_T __prune_oldest(void)
{
    LS_OLDEST_CTX_T c;

    memset(&c, 0, sizeof(c));
    c.today = __today_days();
    if (__for_each_day(__oldest_cb, &c) != OPRT_OK || !c.found) {
        return FALSE;
    }
    return (local_store_remove_day(c.y, c.m, c.d) == OPRT_OK) ? TRUE : FALSE;
}

static void __prune_keep_days(void)
{
    LS_OLDEST_CTX_T c;
    uint32_t guard = 0;

    if (LOCAL_STORE_KEEP_DAYS <= 0) {
        return;
    }
    for (guard = 0; guard < 400; guard++) {
        memset(&c, 0, sizeof(c));
        c.today = __today_days();
        if (c.today == 0 || __for_each_day(__oldest_cb, &c) != OPRT_OK || !c.found) {
            return;
        }
        if (c.today - c.oldest < (int32_t)LOCAL_STORE_KEEP_DAYS) {
            return;
        }
        if (local_store_remove_day(c.y, c.m, c.d) != OPRT_OK) {
            return;
        }
    }
}

/* ---------------------------------------------------------------------------
 * Key frame sidecar: array of {rel_ms, file offset}
 * --------------------------------------------------------------------------- */
static OPERATE_RET __idx_write(const char *media_path, const LS_IDX_ENT_T *ents, uint32_t cnt)
{
    char path[LOCAL_STORE_PATH_MAX];
    TUYA_FILE fp;
    int32_t bytes = (int32_t)(cnt * sizeof(LS_IDX_ENT_T));

    if (__idx_path_of(media_path, path, sizeof(path)) != OPRT_OK) {
        return OPRT_COM_ERROR;
    }
    fp = tkl_fopen(path, "wb");
    if (fp == NULL) {
        return OPRT_FILE_OPEN_FAILED;
    }
    if (bytes > 0 && tkl_fwrite((void *)ents, bytes, fp) != bytes) {
        tkl_fclose(fp);
        return OPRT_FILE_WRITE_FAILED;
    }
    tkl_fclose(fp);
    return OPRT_OK;
}

/* Returns entry count read into ents (<= cap), 0 when missing */
static uint32_t __idx_read(const char *media_path, LS_IDX_ENT_T *ents, uint32_t cap)
{
    char path[LOCAL_STORE_PATH_MAX];
    TUYA_FILE fp;
    int32_t n;

    if (__idx_path_of(media_path, path, sizeof(path)) != OPRT_OK) {
        return 0;
    }
    fp = tkl_fopen(path, "rb");
    if (fp == NULL) {
        return 0;
    }
    n = tkl_fread(ents, (int32_t)(cap * sizeof(LS_IDX_ENT_T)), fp);
    tkl_fclose(fp);
    if (n <= 0) {
        return 0;
    }
    return (uint32_t)n / (uint32_t)sizeof(LS_IDX_ENT_T);
}

static void __idx_remove(const char *media_path)
{
    char path[LOCAL_STORE_PATH_MAX];

    if (__idx_path_of(media_path, path, sizeof(path)) == OPRT_OK) {
        (void)tkl_fs_remove(path);
    }
}

/* ---------------------------------------------------------------------------
 * Reader
 * --------------------------------------------------------------------------- */
static void __reader_fill(LOCAL_STORE_READER_T *rd)
{
    uint32_t keep = rd->valid - rd->used;
    int32_t n;

    if (rd->used > 0) {
        if (keep > 0) {
            memmove(rd->buf, rd->buf + rd->used, keep);
        }
        rd->base += rd->used;
        rd->valid = keep;
        rd->used = 0;
    }
    while (!rd->eof && rd->valid < LS_READ_BYTES) {
        n = tkl_fread(rd->buf + rd->valid, (int32_t)(LS_READ_BYTES - rd->valid), rd->fp);
        if (n <= 0) {
            rd->eof = TRUE;
            break;
        }
        rd->valid += (uint32_t)n;
    }
}

static void __reader_reposition(LOCAL_STORE_READER_T *rd, uint64_t off)
{
    (void)tkl_fseek(rd->fp, (int64_t)off, SEEK_SET);
    rd->base = off;
    rd->valid = 0;
    rd->used = 0;
    rd->eof = FALSE;
}

LOCAL_STORE_READER_T *local_store_reader_open(const char *path)
{
    LOCAL_STORE_READER_T *rd;

    if (path == NULL || strlen(path) >= LOCAL_STORE_PATH_MAX) {
        return NULL;
    }
    rd = (LOCAL_STORE_READER_T *)tal_malloc(sizeof(*rd));
    if (rd == NULL) {
        return NULL;
    }
    memset(rd, 0, sizeof(*rd));
    rd->buf = (uint8_t *)Malloc(LS_READ_BYTES);
    if (rd->buf == NULL) {
        tal_free(rd);
        return NULL;
    }
    rd->fp = tkl_fopen(path, "rb");
    if (rd->fp == NULL) {
        Free(rd->buf);
        tal_free(rd);
        return NULL;
    }
    snprintf(rd->path, sizeof(rd->path), "%s", path);
    return rd;
}

void local_store_reader_close(LOCAL_STORE_READER_T *rd)
{
    if (rd == NULL) {
        return;
    }
    if (rd->fp != NULL) {
        tkl_fclose(rd->fp);
    }
    if (rd->buf != NULL) {
        Free(rd->buf);
    }
    tal_free(rd);
}

OPERATE_RET local_store_reader_next(LOCAL_STORE_READER_T *rd, LOCAL_STORE_FRAME_HDR_T *hdr, const uint8_t **payload)
{
    LOCAL_STORE_FRAME_HDR_T h;

    if (rd == NULL || hdr == NULL || payload == NULL) {
        return OPRT_INVALID_PARM;
    }
    if (rd->valid - rd->used < sizeof(h)) {
        __reader_fill(rd);
        if (rd->valid - rd->used < sizeof(h)) {
            return OPRT_NOT_FOUND;
        }
    }
    memcpy(&h, rd->buf + rd->used, sizeof(h));
    if (h.size == 0 || h.size > LS_MAX_FRAME || (h.type != LOCAL_STORE_FRAME_VIDEO_P &&
                                                 h.type != LOCAL_STORE_FRAME_VIDEO_I &&
                                                 h.type != LOCAL_STORE_FRAME_AUDIO)) {
        TAL_PR_ERR("reader: bad frame hdr type=%u size=%u at %llu", h.type, h.size,
                   (unsigned long long)(rd->base + rd->used));
        return OPRT_COM_ERROR;
    }
    if (rd->valid - rd->used < sizeof(h) + h.size) {
        __reader_fill(rd);
        if (rd->valid - rd->used < sizeof(h) + h.size) {
            return OPRT_NOT_FOUND;
        }
    }
    *hdr = h;
    *payload = rd->buf + rd->used + sizeof(h);
    rd->used += sizeof(h) + h.size;
    return OPRT_OK;
}

OPERATE_RET local_store_reader_seek(LOCAL_STORE_READER_T *rd, uint32_t rel_ms)
{
    LS_IDX_ENT_T *ents;
    uint32_t cnt, i;
    uint64_t target_off = 0;
    LOCAL_STORE_FRAME_HDR_T h;
    const uint8_t *p;
    uint64_t first_ts = 0;
    uint64_t key_off = 0;
    BOOL_T have_key = FALSE;

    if (rd == NULL) {
        return OPRT_INVALID_PARM;
    }
    ents = (LS_IDX_ENT_T *)tal_malloc(LS_IDX_MAX * sizeof(LS_IDX_ENT_T));
    if (ents != NULL) {
        cnt = __idx_read(rd->path, ents, LS_IDX_MAX);
        for (i = 0; i < cnt; i++) {
            if (ents[i].rel_ms <= rel_ms) {
                target_off = ents[i].off;
            } else {
                break;
            }
        }
        tal_free(ents);
        if (cnt > 0) {
            __reader_reposition(rd, target_off);
            return OPRT_OK;
        }
    }

    /* No sidecar: walk the headers */
    __reader_reposition(rd, 0);
    for (;;) {
        uint64_t frame_off = rd->base + rd->used;

        if (local_store_reader_next(rd, &h, &p) != OPRT_OK) {
            break;
        }
        if (first_ts == 0) {
            first_ts = h.timestamp;
        }
        if (h.timestamp - first_ts > rel_ms && have_key) {
            break;
        }
        if (h.type == LOCAL_STORE_FRAME_VIDEO_I) {
            key_off = frame_off;
            have_key = TRUE;
        }
    }
    __reader_reposition(rd, have_key ? key_off : 0);
    return OPRT_OK;
}

/* Last frame's ms offset from the first frame; FALSE when the file holds no frame */
static BOOL_T __media_last_rel_ms(const char *path, uint32_t *rel_ms)
{
    LOCAL_STORE_READER_T *rd;
    LOCAL_STORE_FRAME_HDR_T h;
    const uint8_t *p;
    uint64_t first = 0, last = 0;
    BOOL_T any = FALSE;
    LS_IDX_ENT_T ent;
    LS_IDX_ENT_T *ents;
    uint32_t cnt;

    ents = (LS_IDX_ENT_T *)tal_malloc(LS_IDX_MAX * sizeof(LS_IDX_ENT_T));
    if (ents != NULL) {
        cnt = __idx_read(path, ents, LS_IDX_MAX);
        if (cnt > 0) {
            ent = ents[cnt - 1];
            tal_free(ents);
            *rel_ms = ent.rel_ms;
            return TRUE;
        }
        tal_free(ents);
    }
    rd = local_store_reader_open(path);
    if (rd == NULL) {
        return FALSE;
    }
    while (local_store_reader_next(rd, &h, &p) == OPRT_OK) {
        if (!any) {
            first = h.timestamp;
            any = TRUE;
        }
        last = h.timestamp;
    }
    local_store_reader_close(rd);
    if (!any) {
        return FALSE;
    }
    *rel_ms = (uint32_t)(last - first);
    return TRUE;
}

/* ---------------------------------------------------------------------------
 * Boot recovery: index segments whose writer never reached rec_stop
 * --------------------------------------------------------------------------- */
typedef struct {
    uint32_t *starts;
    uint32_t cnt;
} LS_STARTS_CTX_T;

static BOOL_T __starts_cb(const LOCAL_STORE_SEG_T *seg, void *ctx)
{
    LS_STARTS_CTX_T *c = (LS_STARTS_CTX_T *)ctx;

    if (c->cnt < LS_START_TS_CAP) {
        c->starts[c->cnt++] = seg->start_ts;
    }
    return TRUE;
}

static BOOL_T __starts_has(const LS_STARTS_CTX_T *c, uint32_t start)
{
    uint32_t i;

    for (i = 0; i < c->cnt; i++) {
        if (c->starts[i] == start) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL_T __recover_day_cb(uint32_t y, uint32_t m, uint32_t d, void *ctx)
{
    LS_STARTS_CTX_T *c = (LS_STARTS_CTX_T *)ctx;
    char day_path[LOCAL_STORE_PATH_MAX];
    char path[LOCAL_STORE_PATH_MAX];
    TUYA_DIR dir = NULL;
    TUYA_FILEINFO info = NULL;
    const char *name = NULL;
    uint32_t start, rel_ms;

    c->cnt = 0;
    if (__index_scan(y, m, d, __starts_cb, c) != OPRT_OK) {
        return TRUE;
    }
    if (__day_dir(y, m, d, day_path, sizeof(day_path)) != OPRT_OK || tkl_dir_open(day_path, &dir) != 0) {
        return TRUE;
    }
    while (tkl_dir_read(dir, &info) == 0 && info != NULL) {
        if (tkl_dir_name(info, &name) != 0 || name == NULL) {
            continue;
        }
        if (!__leaf_start_ts(name, &start) || __starts_has(c, start)) {
            continue;
        }
        if (__path_join(day_path, name, path, sizeof(path)) != OPRT_OK) {
            continue;
        }
        if (!__media_last_rel_ms(path, &rel_ms)) {
            TAL_PR_WARN("local_store recover: drop empty %s", path);
            (void)tkl_fs_remove(path);
            __idx_remove(path);
            continue;
        }
        if (local_store_index_append(start, start + rel_ms / 1000u + 1u, 0, name) == OPRT_OK) {
            TAL_PR_NOTICE("local_store recover: indexed %s (%u s)", path, rel_ms / 1000u + 1u);
        }
    }
    tkl_dir_close(dir);
    return TRUE;
}

static void __recover_all(void)
{
    LS_STARTS_CTX_T c;

    c.starts = (uint32_t *)tal_malloc(LS_START_TS_CAP * sizeof(uint32_t));
    if (c.starts == NULL) {
        return;
    }
    c.cnt = 0;
    (void)__for_each_day(__recover_day_cb, &c);
    tal_free(c.starts);
}

OPERATE_RET local_store_init(void)
{
    OPERATE_RET rt;
    char rec_base[LOCAL_STORE_PATH_MAX];

    if (s_inited) {
        return OPRT_OK;
    }
    rt = local_store_ensure_dir(LOCAL_STORE_ROOT);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __rec_base(rec_base, sizeof(rec_base));
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = local_store_ensure_dir(rec_base);
    if (rt != OPRT_OK) {
        return rt;
    }
    s_inited = TRUE;
    __prune_keep_days();
    __recover_all();
    TAL_PR_NOTICE("local_store init ok, root=%s keep_days=%d", LOCAL_STORE_ROOT, (int)LOCAL_STORE_KEEP_DAYS);
    return OPRT_OK;
}

/* ---------------------------------------------------------------------------
 * Recorder ring (under s_lock)
 * --------------------------------------------------------------------------- */
static void __ring_write(const uint8_t *src, uint32_t len)
{
    uint32_t first = LS_STAGE_BYTES - s_ring_tail;

    if (first > len) {
        first = len;
    }
    memcpy(s_ring + s_ring_tail, src, first);
    if (len > first) {
        memcpy(s_ring, src + first, len - first);
    }
    s_ring_tail = (s_ring_tail + len) % LS_STAGE_BYTES;
    s_ring_used += len;
}

static void __ring_read(uint8_t *dst, uint32_t len)
{
    uint32_t first = LS_STAGE_BYTES - s_ring_head;

    if (first > len) {
        first = len;
    }
    if (dst != NULL) {
        memcpy(dst, s_ring + s_ring_head, first);
        if (len > first) {
            memcpy(dst + first, s_ring, len - first);
        }
    }
    s_ring_head = (s_ring_head + len) % LS_STAGE_BYTES;
    s_ring_used -= len;
}

static BOOL_T __ring_peek(LS_REC_HDR_T *hdr)
{
    uint32_t save_head = s_ring_head;
    uint32_t save_used = s_ring_used;

    if (s_ring_used < sizeof(*hdr)) {
        return FALSE;
    }
    __ring_read((uint8_t *)hdr, sizeof(*hdr));
    s_ring_head = save_head;
    s_ring_used = save_used;
    return TRUE;
}

static void __ring_put(uint16_t type, const void *a, uint32_t alen, const void *b, uint32_t blen)
{
    LS_REC_HDR_T hdr;

    hdr.type = type;
    hdr.rsv = 0;
    hdr.len = alen + blen;
    __ring_write((const uint8_t *)&hdr, sizeof(hdr));
    if (alen > 0) {
        __ring_write((const uint8_t *)a, alen);
    }
    if (blen > 0) {
        __ring_write((const uint8_t *)b, blen);
    }
}

/* ---------------------------------------------------------------------------
 * Recorder writer side
 * --------------------------------------------------------------------------- */
static void __seg_close(void);

static void __seg_abort_after_error(void)
{
    s_stat.write_errors++;
    s_stage_used = 0;
    __seg_close();
    s_force_cut = TRUE;
}

static OPERATE_RET __stage_flush(void)
{
    uint32_t t0, dt;
    int32_t n;

    if (s_fp == NULL || s_stage_used == 0) {
        s_stage_used = 0;
        return OPRT_OK;
    }
    t0 = tal_system_get_millisecond();
    n = tkl_fwrite(s_stage, (int32_t)s_stage_used, s_fp);
    if (n != (int32_t)s_stage_used) {
        TAL_PR_WARN("local_store write %u got %d, pruning oldest day", s_stage_used, n);
        if (__prune_oldest()) {
            n = tkl_fwrite(s_stage, (int32_t)s_stage_used, s_fp);
        }
    }
    dt = tal_system_get_millisecond() - t0;
    if (dt > s_stat.write_max_ms) {
        s_stat.write_max_ms = dt;
    }
    if (n != (int32_t)s_stage_used) {
        TAL_PR_ERR("local_store write failed want=%u got=%d, closing %s", s_stage_used, n, s_path);
        __seg_abort_after_error();
        return OPRT_FILE_WRITE_FAILED;
    }
    s_file_bytes += s_stage_used;
    s_stat.bytes_written += s_stage_used;
    s_stage_used = 0;
    s_dirty = TRUE;
    return OPRT_OK;
}

static void __file_sync(void)
{
    if (s_fp == NULL) {
        return;
    }
    (void)tkl_fflush(s_fp);
    (void)tkl_fsync(tkl_fileno(s_fp));
    (void)__idx_write(s_path, s_idx, s_idx_cnt);
    s_dirty = FALSE;
    s_last_sync_ms = tal_system_get_millisecond();
}

static void __maybe_sync(void)
{
    uint32_t now = tal_system_get_millisecond();

    if (s_fp == NULL || (s_stage_used == 0 && !s_dirty)) {
        return;
    }
    if (now - s_last_sync_ms < (uint32_t)LOCAL_STORE_SYNC_MS) {
        return;
    }
    if (__stage_flush() != OPRT_OK) {
        return;
    }
    __file_sync();
}

static void __seg_close(void)
{
    uint32_t end_ts;
    OPERATE_RET rt;

    if (s_fp == NULL) {
        return;
    }
    (void)__stage_flush();
    if (s_fp == NULL) {
        return;
    }
    __file_sync();
    tkl_fclose(s_fp);
    s_fp = NULL;

    if (s_file_bytes == 0) {
        TAL_PR_WARN("local_store rec discard empty %s", s_path);
        (void)tkl_fs_remove(s_path);
        __idx_remove(s_path);
    } else {
        end_ts = s_file_start + s_last_rel_ms / 1000u + 1u;
        rt = local_store_index_append(s_file_start, end_ts, 0, s_leaf);
        if (rt != OPRT_OK) {
            TAL_PR_ERR("local_store rec index_append failed: %d", rt);
        } else {
            TAL_PR_NOTICE("local_store rec stop %s bytes=%llu keys=%u [%u,%u]", s_path,
                          (unsigned long long)s_file_bytes, s_idx_cnt, s_file_start, end_ts);
        }
    }
    s_file_bytes = 0;
    s_idx_cnt = 0;
    s_last_rel_ms = 0;
    s_dirty = FALSE;
    s_path[0] = '\0';
    s_leaf[0] = '\0';
}

static void __seg_open(uint32_t start_sec)
{
    OPERATE_RET rt;
    uint32_t y, m, d;
    char day_path[LOCAL_STORE_PATH_MAX];
    int n;

    __prune_keep_days();

    n = snprintf(s_leaf, sizeof(s_leaf), "%s_%u%s", s_prefix, start_sec, LS_MEDIA_EXT);
    if (n < 0 || (uint32_t)n >= sizeof(s_leaf)) {
        goto fail;
    }
    if (__ts_to_ymd(start_sec, &y, &m, &d) != OPRT_OK) {
        goto fail;
    }
    rt = __day_dir(y, m, d, day_path, sizeof(day_path));
    if (rt != OPRT_OK || local_store_ensure_dir(day_path) != OPRT_OK) {
        goto fail;
    }
    if (__path_join(day_path, s_leaf, s_path, sizeof(s_path)) != OPRT_OK) {
        goto fail;
    }
    s_fp = tkl_fopen(s_path, "wb");
    if (s_fp == NULL) {
        TAL_PR_WARN("local_store rec open %s failed, pruning oldest day", s_path);
        if (__prune_oldest()) {
            s_fp = tkl_fopen(s_path, "wb");
        }
    }
    if (s_fp == NULL) {
        TAL_PR_ERR("local_store rec open failed: %s", s_path);
        goto fail;
    }
    s_file_start = start_sec;
    s_file_bytes = 0;
    s_idx_cnt = 0;
    s_last_rel_ms = 0;
    s_stage_used = 0;
    s_dirty = FALSE;
    s_last_sync_ms = tal_system_get_millisecond();
    s_stat.segments++;
    TAL_PR_NOTICE("local_store rec start %s", s_path);
    return;

fail:
    s_stat.write_errors++;
    s_path[0] = '\0';
    s_force_cut = TRUE;
}

static void __stage_put(const void *p, uint32_t n)
{
    if (s_fp == NULL) {
        return;
    }
    if (s_stage_used + n > LS_WRITE_BYTES) {
        if (__stage_flush() != OPRT_OK) {
            return;
        }
    }
    memcpy(s_stage + s_stage_used, p, n);
    s_stage_used += n;
}

static void __rec_worker(void *arg)
{
    LS_REC_HDR_T h;
    LOCAL_STORE_FRAME_HDR_T fh;
    uint32_t remaining, n, now;

    (void)arg;
    s_alive = TRUE;

    while (s_run || s_ring_used > 0) {
        tal_mutex_lock(s_lock);
        if (!__ring_peek(&h)) {
            tal_mutex_unlock(s_lock);
            now = tal_system_get_millisecond();
            if (s_stage_used > 0 && now - s_last_data_ms >= LS_IDLE_FLUSH_MS) {
                (void)__stage_flush();
            }
            __maybe_sync();
            (void)tal_semaphore_wait(s_wake, LS_WAIT_MS);
            continue;
        }
        __ring_read(NULL, sizeof(h));

        if (h.type == LS_REC_CUT) {
            uint32_t start = 0;

            __ring_read((uint8_t *)&start, sizeof(start));
            tal_mutex_unlock(s_lock);
            __seg_close();
            __seg_open(start);
            continue;
        }
        if (s_fp == NULL || h.len < sizeof(fh)) {
            __ring_read(NULL, h.len);
            tal_mutex_unlock(s_lock);
            s_stat.frames_dropped++;
            continue;
        }
        __ring_read((uint8_t *)&fh, sizeof(fh));
        tal_mutex_unlock(s_lock);

        remaining = h.len - (uint32_t)sizeof(fh);
        s_last_rel_ms = (uint32_t)(fh.timestamp - (uint64_t)s_file_start * 1000ULL);
        if (fh.type == LOCAL_STORE_FRAME_VIDEO_I && s_idx_cnt < LS_IDX_MAX) {
            s_idx[s_idx_cnt].rel_ms = s_last_rel_ms;
            s_idx[s_idx_cnt].off = (uint32_t)(s_file_bytes + s_stage_used);
            s_idx_cnt++;
        }
        __stage_put(&fh, sizeof(fh));
        while (remaining > 0) {
            if (s_fp == NULL) {
                tal_mutex_lock(s_lock);
                __ring_read(NULL, remaining);
                tal_mutex_unlock(s_lock);
                s_stat.frames_dropped++;
                break;
            }
            if (s_stage_used >= LS_WRITE_BYTES) {
                if (__stage_flush() != OPRT_OK) {
                    continue;
                }
            }
            n = LS_WRITE_BYTES - s_stage_used;
            if (n > remaining) {
                n = remaining;
            }
            tal_mutex_lock(s_lock);
            __ring_read(s_stage + s_stage_used, n);
            tal_mutex_unlock(s_lock);
            s_stage_used += n;
            remaining -= n;
        }
        s_last_data_ms = tal_system_get_millisecond();
        __maybe_sync();
    }

    __seg_close();
    s_alive = FALSE;
}

/* ---------------------------------------------------------------------------
 * Recorder producer side
 * --------------------------------------------------------------------------- */
/* Kept for the process lifetime: the capture thread reaches s_lock and s_ring without holding anything. */
static OPERATE_RET __rec_alloc_once(void)
{
    if (s_lock == NULL && tal_mutex_create_init(&s_lock) != OPRT_OK) {
        return OPRT_COM_ERROR;
    }
    if (s_wake == NULL && tal_semaphore_create_init(&s_wake, 0, 1) != OPRT_OK) {
        return OPRT_COM_ERROR;
    }
    if (s_ring == NULL) {
        s_ring = (uint8_t *)Malloc(LS_STAGE_BYTES);
    }
    if (s_stage == NULL) {
        s_stage = (uint8_t *)Malloc(LS_WRITE_BYTES);
    }
    if (s_idx == NULL) {
        s_idx = (LS_IDX_ENT_T *)Malloc(LS_IDX_MAX * sizeof(LS_IDX_ENT_T));
    }
    if (s_ring == NULL || s_stage == NULL || s_idx == NULL) {
        return OPRT_MALLOC_FAILED;
    }
    return OPRT_OK;
}

OPERATE_RET local_store_rec_start(const char *leaf_prefix, uint32_t seg_sec)
{
    OPERATE_RET rt;
    THREAD_CFG_T cfg;

    if (leaf_prefix == NULL || leaf_prefix[0] == '\0' || !__filename_is_safe(leaf_prefix) ||
        strlen(leaf_prefix) >= sizeof(s_prefix)) {
        return OPRT_INVALID_PARM;
    }
    if (s_run || s_alive) {
        (void)local_store_rec_stop();
    }
    if (s_alive) {
        TAL_PR_ERR("local_store rec start refused: previous writer still draining");
        return OPRT_RESOURCE_NOT_READY;
    }
    rt = local_store_init();
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __rec_alloc_once();
    if (rt != OPRT_OK) {
        return rt;
    }

    snprintf(s_prefix, sizeof(s_prefix), "%s", leaf_prefix);
    s_seg_sec = (seg_sec == 0) ? LS_SEG_DFLT_SEC : seg_sec;

    tal_mutex_lock(s_lock);
    s_ring_head = 0;
    s_ring_tail = 0;
    s_ring_used = 0;
    s_stage_used = 0;
    s_idx_cnt = 0;
    s_seg_open = FALSE;
    s_force_cut = FALSE;
    s_wait_key = FALSE;
    s_seg_start = 0;
    s_seg_first_ms = 0;
    s_fp = NULL;
    s_file_bytes = 0;
    s_last_rel_ms = 0;
    s_dirty = FALSE;
    s_last_data_ms = tal_system_get_millisecond();
    memset(&s_stat, 0, sizeof(s_stat));
    s_stat.ring_cap = LS_STAGE_BYTES;
    tal_mutex_unlock(s_lock);

    memset(&cfg, 0, sizeof(cfg));
    cfg.stackDepth = LS_THREAD_STACK;
    cfg.priority = THREAD_PRIO_3;
    cfg.thrdname = "local_store_wr";
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    cfg.psram_mode = 1;
#endif
    s_run = TRUE;
    s_alive = FALSE;
    if (tal_thread_create_and_start(&s_worker, NULL, NULL, __rec_worker, NULL, &cfg) != OPRT_OK) {
        s_run = FALSE;
        s_worker = NULL;
        return OPRT_COM_ERROR;
    }
    tal_mutex_lock(s_lock);
    s_accept = TRUE;
    tal_mutex_unlock(s_lock);
    TAL_PR_NOTICE("local_store rec armed prefix=%s seg=%us ring=%uKB chunk=%uKB", s_prefix, s_seg_sec,
                  LS_STAGE_BYTES / 1024u, LS_WRITE_BYTES / 1024u);
    return OPRT_OK;
}

static OPERATE_RET __rec_put_frame(uint32_t type, const uint8_t *data, uint32_t len, uint64_t ts_ms)
{
    LOCAL_STORE_FRAME_HDR_T fh;
    uint32_t now_sec;
    uint32_t need;
    BOOL_T cut = FALSE;
    BOOL_T is_video = (type != LOCAL_STORE_FRAME_AUDIO) ? TRUE : FALSE;
    BOOL_T is_key = (type == LOCAL_STORE_FRAME_VIDEO_I) ? TRUE : FALSE;

    if (data == NULL || len == 0) {
        return OPRT_INVALID_PARM;
    }
    if (s_lock == NULL || !s_accept) {
        return OPRT_RESOURCE_NOT_READY;
    }
    now_sec = (uint32_t)tal_time_get_posix();

    tal_mutex_lock(s_lock);
    if (!s_accept) {
        tal_mutex_unlock(s_lock);
        return OPRT_RESOURCE_NOT_READY;
    }
    if (is_key && (!s_seg_open || s_force_cut || now_sec >= s_seg_start + s_seg_sec)) {
        cut = TRUE;
    }
    if (!s_seg_open && !cut) {
        tal_mutex_unlock(s_lock);
        return OPRT_OK;
    }
    if (s_wait_key && !cut && !is_key) {
        s_stat.frames_dropped++;
        tal_mutex_unlock(s_lock);
        return OPRT_OK;
    }
    need = (uint32_t)sizeof(LS_REC_HDR_T) + (uint32_t)sizeof(fh) + len;
    if (cut) {
        need += (uint32_t)sizeof(LS_REC_HDR_T) + (uint32_t)sizeof(now_sec);
    }
    if (need > LS_STAGE_BYTES - s_ring_used) {
        s_stat.frames_dropped++;
        if (is_video) {
            s_wait_key = TRUE;
        }
        tal_mutex_unlock(s_lock);
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    if (cut) {
        __ring_put((uint16_t)LS_REC_CUT, &now_sec, sizeof(now_sec), NULL, 0);
        s_seg_open = TRUE;
        s_seg_start = now_sec;
        s_seg_first_ms = ts_ms;
        s_force_cut = FALSE;
    }
    fh.type = type;
    fh.size = len;
    fh.timestamp = (uint64_t)s_seg_start * 1000ULL + ((ts_ms > s_seg_first_ms) ? (ts_ms - s_seg_first_ms) : 0);
    fh.pts = ts_ms * 1000ULL;
    __ring_put((uint16_t)LS_REC_DATA, &fh, sizeof(fh), data, len);
    if (is_key) {
        s_wait_key = FALSE;
    }
    s_stat.frames_in++;
    s_stat.ring_used = s_ring_used;
    tal_mutex_unlock(s_lock);
    (void)tal_semaphore_post(s_wake);
    return OPRT_OK;
}

OPERATE_RET local_store_rec_write(const uint8_t *data, uint32_t len, uint64_t ts_ms, BOOL_T is_key)
{
    return __rec_put_frame(is_key ? LOCAL_STORE_FRAME_VIDEO_I : LOCAL_STORE_FRAME_VIDEO_P, data, len, ts_ms);
}

OPERATE_RET local_store_rec_write_audio(const uint8_t *data, uint32_t len, uint64_t ts_ms)
{
    return __rec_put_frame(LOCAL_STORE_FRAME_AUDIO, data, len, ts_ms);
}

OPERATE_RET local_store_rec_stop(void)
{
    uint32_t waited = 0;

    if (s_lock != NULL) {
        tal_mutex_lock(s_lock);
        s_accept = FALSE;
        s_seg_open = FALSE;
        tal_mutex_unlock(s_lock);
    }
    if (!s_run && s_worker == NULL) {
        return OPRT_OK;
    }
    s_run = FALSE;
    if (s_wake != NULL) {
        (void)tal_semaphore_post(s_wake);
    }
    while (s_alive && waited < LS_STOP_WAIT_MS) {
        tal_system_sleep(10);
        waited += 10;
    }
    if (s_worker != NULL) {
        tal_thread_delete(s_worker);
        s_worker = NULL;
    }
    if (s_alive) {
        TAL_PR_ERR("local_store rec worker still draining after %u ms, it closes the segment itself", waited);
    }
    TAL_PR_NOTICE("local_store rec stopped: in=%u drop=%u seg=%u err=%u wmax=%ums bytes=%llu", s_stat.frames_in,
                  s_stat.frames_dropped, s_stat.segments, s_stat.write_errors, s_stat.write_max_ms,
                  (unsigned long long)s_stat.bytes_written);
    return OPRT_OK;
}

BOOL_T local_store_rec_is_open(void)
{
    return s_accept;
}

uint32_t local_store_rec_elapsed_sec(void)
{
    TIME_T now;

    if (!s_run || !s_seg_open || s_seg_start == 0) {
        return 0;
    }
    now = tal_time_get_posix();
    if (now <= (TIME_T)s_seg_start) {
        return 0;
    }
    return (uint32_t)now - s_seg_start;
}

void local_store_rec_get_stat(LOCAL_STORE_REC_STAT_T *st)
{
    if (st == NULL) {
        return;
    }
    *st = s_stat;
    st->ring_used = s_ring_used;
}

/* ---------------------------------------------------------------------------
 * Seed: Annex-B H264 file -> .media segment
 * --------------------------------------------------------------------------- */
#define LS_SEED_BUF   (256u * 1024u)
#define LS_SEED_CHUNK (32u * 1024u)
#define LS_NEED_MORE  (-2)

static int __annexb_find_start(const uint8_t *p, uint32_t len, uint32_t from, uint32_t *sc_len)
{
    uint32_t i;

    for (i = from; i + 3 < len; i++) {
        if (p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 1) {
            *sc_len = 3;
            return (int)i;
        }
        if (i + 4 < len && p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 0 && p[i + 3] == 1) {
            *sc_len = 4;
            return (int)i;
        }
    }
    return -1;
}

/* One access unit starting at buf[offset]: SPS/PPS/SEI are glued onto the following VCL NAL */
static int __annexb_next_au(const uint8_t *buf, uint32_t offset, uint32_t size, BOOL_T at_eof, BOOL_T *is_key,
                            uint32_t *au_len, uint32_t *au_start)
{
    int sc0, sc1;
    uint32_t sc_len0 = 0, sc_len1 = 0;
    uint32_t nal_off;
    uint8_t nal_type;
    BOOL_T saw_vcl = FALSE;

    *is_key = FALSE;
    if (offset >= size) {
        return at_eof ? -1 : LS_NEED_MORE;
    }
    sc0 = __annexb_find_start(buf, size, offset, &sc_len0);
    if (sc0 < 0) {
        return at_eof ? -1 : LS_NEED_MORE;
    }
    *au_start = (uint32_t)sc0;
    nal_off = (uint32_t)sc0 + sc_len0;
    if (nal_off >= size) {
        return at_eof ? -1 : LS_NEED_MORE;
    }
    nal_type = (uint8_t)(buf[nal_off] & 0x1f);
    if (nal_type == 5 || nal_type == 7) {
        *is_key = TRUE;
    }
    if (nal_type >= 1 && nal_type <= 5) {
        saw_vcl = TRUE;
    }
    for (;;) {
        sc1 = __annexb_find_start(buf, size, nal_off + 1, &sc_len1);
        if (sc1 < 0) {
            if (!at_eof) {
                return LS_NEED_MORE;
            }
            *au_len = size - (uint32_t)sc0;
            return (*au_len > 0) ? 0 : -1;
        }
        nal_off = (uint32_t)sc1 + sc_len1;
        if (nal_off >= size) {
            if (!at_eof) {
                return LS_NEED_MORE;
            }
            *au_len = size - (uint32_t)sc0;
            return 0;
        }
        nal_type = (uint8_t)(buf[nal_off] & 0x1f);
        if (nal_type == 7 || nal_type == 8) {
            if (saw_vcl) {
                *au_len = (uint32_t)sc1 - (uint32_t)sc0;
                return 0;
            }
            if (nal_type == 7) {
                *is_key = TRUE;
            }
            continue;
        }
        if (nal_type >= 1 && nal_type <= 5) {
            if (saw_vcl) {
                *au_len = (uint32_t)sc1 - (uint32_t)sc0;
                return 0;
            }
            saw_vcl = TRUE;
            if (nal_type == 5) {
                *is_key = TRUE;
            }
            continue;
        }
        if (saw_vcl) {
            *au_len = (uint32_t)sc1 - (uint32_t)sc0;
            return 0;
        }
    }
}

typedef struct {
    TUYA_FILE fin;
    uint8_t *buf;
    uint32_t valid;
    uint32_t used;
    BOOL_T eof;
} LS_ANNEXB_T;

static void __annexb_fill(LS_ANNEXB_T *a)
{
    uint32_t keep = a->valid - a->used;
    int32_t n;

    if (keep > 0 && a->used > 0) {
        memmove(a->buf, a->buf + a->used, keep);
    }
    a->valid = keep;
    a->used = 0;
    while (!a->eof && a->valid < LS_SEED_BUF) {
        n = tkl_fread(a->buf + a->valid, (int32_t)(LS_SEED_BUF - a->valid), a->fin);
        if (n <= 0) {
            a->eof = TRUE;
            break;
        }
        a->valid += (uint32_t)n;
    }
}

/* Returns 0 with the AU described, -1 at end */
static int __annexb_read_au(LS_ANNEXB_T *a, BOOL_T *is_key, uint32_t *start, uint32_t *len)
{
    int pr;

    for (;;) {
        if (a->valid - a->used < LS_SEED_CHUNK && !a->eof) {
            __annexb_fill(a);
        }
        pr = __annexb_next_au(a->buf, a->used, a->valid, a->eof, is_key, len, start);
        if (pr == 0) {
            a->used = *start + *len;
            return 0;
        }
        if (pr == LS_NEED_MORE) {
            if (a->eof || (a->used == 0 && a->valid == LS_SEED_BUF)) {
                return -1;
            }
            __annexb_fill(a);
            continue;
        }
        return -1;
    }
}

static BOOL_T __annexb_open(LS_ANNEXB_T *a, const char *src_path)
{
    memset(a, 0, sizeof(*a));
    a->fin = tkl_fopen(src_path, "rb");
    if (a->fin == NULL) {
        return FALSE;
    }
    a->buf = (uint8_t *)Malloc(LS_SEED_BUF);
    if (a->buf == NULL) {
        tkl_fclose(a->fin);
        a->fin = NULL;
        return FALSE;
    }
    return TRUE;
}

static void __annexb_close(LS_ANNEXB_T *a)
{
    if (a->fin != NULL) {
        tkl_fclose(a->fin);
    }
    if (a->buf != NULL) {
        Free(a->buf);
    }
    memset(a, 0, sizeof(*a));
}

typedef struct {
    const char *leaf;
    BOOL_T found;
} LS_LEAF_CTX_T;

static BOOL_T __leaf_cb(const LOCAL_STORE_SEG_T *seg, void *ctx)
{
    LS_LEAF_CTX_T *c = (LS_LEAF_CTX_T *)ctx;

    if (strcmp(seg->leaf, c->leaf) == 0) {
        c->found = TRUE;
        return FALSE;
    }
    return TRUE;
}

OPERATE_RET local_store_seed_h264(const char *src_path, const char *leaf, uint32_t duration_sec)
{
    OPERATE_RET rt;
    LS_ANNEXB_T a;
    TUYA_FILE fout = NULL;
    char dst[LOCAL_STORE_PATH_MAX];
    char media_leaf[64];
    uint32_t total_au = 0, i = 0;
    uint32_t start_ts, end_ts, y, m, d;
    uint32_t au_start, au_len;
    BOOL_T is_key, src_exists = FALSE, dst_exists = FALSE;
    LS_IDX_ENT_T *idx = NULL;
    uint32_t idx_cnt = 0;
    uint64_t off = 0;
    LOCAL_STORE_FRAME_HDR_T fh;
    LS_LEAF_CTX_T lc;
    TIME_T now;
    int n;

    if (src_path == NULL || !__filename_is_safe(leaf)) {
        return OPRT_INVALID_PARM;
    }
    if (duration_sec == 0) {
        duration_sec = 60;
    }
    (void)tkl_fs_is_exist(src_path, &src_exists);
    if (!src_exists) {
        TAL_PR_ERR("seed src missing: %s", src_path);
        return OPRT_FILE_OPEN_FAILED;
    }
    rt = local_store_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    /* "<name>.h264" -> "<name>.media" so every segment shares one reader */
    {
        const char *dot = strrchr(leaf, '.');
        uint32_t stem = (dot != NULL) ? (uint32_t)(dot - leaf) : (uint32_t)strlen(leaf);

        if (stem + strlen(LS_MEDIA_EXT) >= sizeof(media_leaf)) {
            return OPRT_BUFFER_NOT_ENOUGH;
        }
        memcpy(media_leaf, leaf, stem);
        snprintf(media_leaf + stem, sizeof(media_leaf) - stem, "%s", LS_MEDIA_EXT);
    }

    now = tal_time_get_posix();
    if (now < (TIME_T)duration_sec) {
        start_ts = 1;
        end_ts = duration_sec;
    } else {
        end_ts = (uint32_t)now;
        start_ts = end_ts - duration_sec;
    }
    if (__ts_to_ymd(start_ts, &y, &m, &d) == OPRT_OK) {
        lc.leaf = media_leaf;
        lc.found = FALSE;
        (void)__index_scan(y, m, d, __leaf_cb, &lc);
        if (lc.found) {
            TAL_PR_NOTICE("local_store seed skip, leaf indexed: %s", media_leaf);
            return OPRT_OK;
        }
    }

    rt = local_store_make_rec_path(media_leaf, dst, sizeof(dst));
    if (rt != OPRT_OK) {
        return rt;
    }
    (void)tkl_fs_is_exist(dst, &dst_exists);
    if (dst_exists) {
        (void)tkl_fs_remove(dst);
    }

    if (!__annexb_open(&a, src_path)) {
        return OPRT_FILE_OPEN_FAILED;
    }
    while (__annexb_read_au(&a, &is_key, &au_start, &au_len) == 0) {
        total_au++;
    }
    __annexb_close(&a);
    if (total_au == 0) {
        TAL_PR_ERR("seed: no access unit in %s", src_path);
        return OPRT_COM_ERROR;
    }

    if (!__annexb_open(&a, src_path)) {
        return OPRT_FILE_OPEN_FAILED;
    }
    fout = tkl_fopen(dst, "wb");
    idx = (LS_IDX_ENT_T *)tal_malloc(LS_IDX_MAX * sizeof(LS_IDX_ENT_T));
    if (fout == NULL || idx == NULL) {
        __annexb_close(&a);
        if (fout != NULL) {
            tkl_fclose(fout);
        }
        if (idx != NULL) {
            tal_free(idx);
        }
        return OPRT_FILE_OPEN_FAILED;
    }
    rt = OPRT_OK;
    while (__annexb_read_au(&a, &is_key, &au_start, &au_len) == 0) {
        uint32_t rel_ms = (uint32_t)(((uint64_t)i * duration_sec * 1000ULL) / total_au);

        fh.type = is_key ? LOCAL_STORE_FRAME_VIDEO_I : LOCAL_STORE_FRAME_VIDEO_P;
        fh.size = au_len;
        fh.timestamp = (uint64_t)start_ts * 1000ULL + rel_ms;
        fh.pts = fh.timestamp * 1000ULL;
        if (is_key && idx_cnt < LS_IDX_MAX) {
            idx[idx_cnt].rel_ms = rel_ms;
            idx[idx_cnt].off = (uint32_t)off;
            idx_cnt++;
        }
        n = tkl_fwrite(&fh, (int32_t)sizeof(fh), fout);
        if (n != (int32_t)sizeof(fh) || tkl_fwrite(a.buf + au_start, (int32_t)au_len, fout) != (int32_t)au_len) {
            rt = OPRT_FILE_WRITE_FAILED;
            break;
        }
        off += sizeof(fh) + au_len;
        i++;
    }
    __annexb_close(&a);
    tkl_fclose(fout);
    if (rt != OPRT_OK) {
        tal_free(idx);
        (void)tkl_fs_remove(dst);
        return rt;
    }
    (void)__idx_write(dst, idx, idx_cnt);
    tal_free(idx);

    rt = local_store_index_append(start_ts, end_ts, 0, media_leaf);
    if (rt != OPRT_OK) {
        TAL_PR_ERR("seed index_append failed: %d", rt);
        return rt;
    }
    TAL_PR_NOTICE("local_store seed ok %s frames=%u keys=%u [%u,%u]", dst, total_au, idx_cnt, start_ts, end_ts);
    return OPRT_OK;
}
