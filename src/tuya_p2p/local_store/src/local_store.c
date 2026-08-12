/**
 * @file local_store.c
 * @brief Local media storage path helpers implementation
 * @version 1.0
 * @date 2026-07-30
 * @copyright Copyright (c) Tuya Inc.
 */
#include "local_store.h"
#include "tkl_fs.h"
#include "tal_log.h"
#include "tal_time_service.h"
#include <string.h>
#include <stdio.h>
/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#ifndef LOCAL_STORE_ROOT
#define LOCAL_STORE_ROOT "./media"
#endif

#ifndef LOCAL_STORE_REC_SUBDIR
#define LOCAL_STORE_REC_SUBDIR "rec"
#endif

#define LOCAL_STORE_TMP_SUFFIX ".tmp"

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static BOOL_T s_inited = FALSE;
static TUYA_FILE s_rec_fp = NULL;
static char s_rec_leaf[64];
static char s_rec_path[LOCAL_STORE_PATH_MAX];
static uint32_t s_rec_start_ts = 0;
static uint32_t s_rec_bytes = 0;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Reject empty, absolute-looking leaf, or path traversal in filename
 * @param[in] filename leaf name only
 * @return TRUE if safe, FALSE otherwise
 */
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

/**
 * @brief Join parent and child with a single '/'
 * @param[in] parent parent directory
 * @param[in] child child name or relative segment
 * @param[out] out output buffer
 * @param[in] out_len size of out
 * @return OPRT_OK on success
 */
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

/**
 * @brief Whether path is a VFS mount root that must not be mkdir'd
 * @param[in] path directory path
 * @return TRUE if mount root
 * @note Align examples/peripherals/sd: mount /sdcard then open files under it.
 *       tkl_fs_is_exist("/sdcard") often fails even when FAT is mounted.
 */
static BOOL_T __is_fs_mount_root(const char *path)
{
    if (path == NULL) {
        return FALSE;
    }
    if (strcmp(path, "/sdcard") == 0) {
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Create one directory if missing
 * @param[in] path directory path
 * @return OPRT_OK on success
 */
static OPERATE_RET __mkdir_one(const char *path)
{
    int rt;
    BOOL_T exists = FALSE;

    if (path == NULL || path[0] == '\0') {
        return OPRT_INVALID_PARM;
    }
    /* Skip relative current-dir token */
    if (strcmp(path, ".") == 0) {
        return OPRT_OK;
    }
    if (__is_fs_mount_root(path)) {
        return OPRT_OK;
    }

    /* Like examples/camera/output_sdcard: ignore is_exist return, use flag */
    exists = FALSE;
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

/**
 * @brief Ensure directory exists (mkdir -p style via tkl_fs, like peripherals/sd)
 * @param[in] dir absolute or relative directory path
 * @return OPRT_OK on success, error code on failure
 */
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

    /* Walk segments; keep leading '/' for absolute paths */
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

/**
 * @brief Get configured storage root path
 * @return Pointer to root string (compile-time config), never NULL when enabled
 */
const char *local_store_root(void)
{
    return LOCAL_STORE_ROOT;
}

/**
 * @brief Initialize local store and ensure root / recording base dirs exist
 * @return OPRT_OK on success, error code on failure
 * @note Safe to call more than once; subsequent calls are no-ops after success
 */
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

    rt = __path_join(LOCAL_STORE_ROOT, LOCAL_STORE_REC_SUBDIR, rec_base, sizeof(rec_base));
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = local_store_ensure_dir(rec_base);
    if (rt != OPRT_OK) {
        return rt;
    }

    s_inited = TRUE;
    TAL_PR_DEBUG("local_store init ok, root=%s", LOCAL_STORE_ROOT);
    return OPRT_OK;
}

/**
 * @brief Build dated recording path: ROOT/REC/YYYY-MM-DD/filename and create dirs
 * @param[in] filename leaf name only (no '/' or ".." allowed), e.g. "cam0.mp4"
 * @param[out] out output path buffer
 * @param[in] out_len size of out in bytes
 * @return OPRT_OK on success, OPRT_INVALID_PARM / OPRT_BUFFER_NOT_ENOUGH on failure
 */
OPERATE_RET local_store_make_rec_path(const char *filename, char *out, uint32_t out_len)
{
    OPERATE_RET rt;
    POSIX_TM_S tm;
    char date_dir[16];
    char rec_base[LOCAL_STORE_PATH_MAX];
    char day_path[LOCAL_STORE_PATH_MAX];
    int n;

    if (!__filename_is_safe(filename) || out == NULL || out_len == 0) {
        return OPRT_INVALID_PARM;
    }

    rt = local_store_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = tal_time_get(&tm);
    if (rt != OPRT_OK) {
        /* Fallback when time not synced: still produce a stable folder name */
        n = snprintf(date_dir, sizeof(date_dir), "unknown");
    } else {
        n = snprintf(date_dir, sizeof(date_dir), "%04d-%02d-%02d",
                     tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    }
    if (n < 0 || (uint32_t)n >= sizeof(date_dir)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    rt = __path_join(LOCAL_STORE_ROOT, LOCAL_STORE_REC_SUBDIR, rec_base, sizeof(rec_base));
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __path_join(rec_base, date_dir, day_path, sizeof(day_path));
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = local_store_ensure_dir(day_path);
    if (rt != OPRT_OK) {
        return rt;
    }

    return __path_join(day_path, filename, out, out_len);
}

/**
 * @brief Build temporary sibling path by appending ".tmp" to final_path
 * @param[in] final_path target final path
 * @param[out] out output tmp path buffer
 * @param[in] out_len size of out in bytes
 * @return OPRT_OK on success
 */
OPERATE_RET local_store_make_tmp_path(const char *final_path, char *out, uint32_t out_len)
{
    int n;

    if (final_path == NULL || final_path[0] == '\0' || out == NULL || out_len == 0) {
        return OPRT_INVALID_PARM;
    }
    n = snprintf(out, out_len, "%s%s", final_path, LOCAL_STORE_TMP_SUFFIX);
    if (n < 0 || (uint32_t)n >= out_len) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    return OPRT_OK;
}

/**
 * @brief Atomically promote tmp file to final path (tkl_fs_rename)
 * @param[in] tmp_path temporary file path
 * @param[in] final_path destination path
 * @return OPRT_OK on success
 */
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

/**
 * @brief Build ROOT/rec/YYYY-MM-DD path and ensure dirs
 */
static OPERATE_RET __day_dir(uint32_t year, uint32_t month, uint32_t day, char *out, uint32_t out_len)
{
    OPERATE_RET rt;
    char date_dir[16];
    char rec_base[LOCAL_STORE_PATH_MAX];
    int n;

    if (out == NULL || out_len == 0 || month < 1 || month > 12 || day < 1 || day > 31) {
        return OPRT_INVALID_PARM;
    }
    rt = local_store_init();
    if (rt != OPRT_OK) {
        return rt;
    }
    n = snprintf(date_dir, sizeof(date_dir), "%04u-%02u-%02u", (uint32_t)year, (uint32_t)month, (uint32_t)day);
    if (n < 0 || (uint32_t)n >= sizeof(date_dir)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    rt = __path_join(LOCAL_STORE_ROOT, LOCAL_STORE_REC_SUBDIR, rec_base, sizeof(rec_base));
    if (rt != OPRT_OK) {
        return rt;
    }
    return __path_join(rec_base, date_dir, out, out_len);
}

/**
 * @brief Index file path under a day directory
 */
static OPERATE_RET __index_path(uint32_t year, uint32_t month, uint32_t day, char *out, uint32_t out_len)
{
    OPERATE_RET rt;
    char day_path[LOCAL_STORE_PATH_MAX];

    rt = __day_dir(year, month, day, day_path, sizeof(day_path));
    if (rt != OPRT_OK) {
        return rt;
    }
    return __path_join(day_path, "index.txt", out, out_len);
}

/**
 * @brief Append one segment to the day index
 */
OPERATE_RET local_store_index_append(uint32_t start_ts, uint32_t end_ts, uint16_t type, const char *leaf)
{
    OPERATE_RET rt;
    POSIX_TM_S tm;
    char day_path[LOCAL_STORE_PATH_MAX];
    char idx_path[LOCAL_STORE_PATH_MAX];
    char line[192];
    TUYA_FILE fp;
    int n;
    uint32_t year, month, day;

    if (!__filename_is_safe(leaf) || end_ts < start_ts) {
        return OPRT_INVALID_PARM;
    }

    memset(&tm, 0, sizeof(tm));
    rt = tal_time_get_local_time_custom((TIME_T)start_ts, &tm);
    if (rt != OPRT_OK) {
        rt = tal_time_get(&tm);
        if (rt != OPRT_OK) {
            return rt;
        }
    }
    year = (uint32_t)(tm.tm_year + 1900);
    month = (uint32_t)(tm.tm_mon + 1);
    day = (uint32_t)tm.tm_mday;

    rt = __day_dir(year, month, day, day_path, sizeof(day_path));
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = local_store_ensure_dir(day_path);
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = __path_join(day_path, "index.txt", idx_path, sizeof(idx_path));
    if (rt != OPRT_OK) {
        return rt;
    }

    n = snprintf(line, sizeof(line), "%u %u %u %s\n", (uint32_t)start_ts, (uint32_t)end_ts, (uint32_t)type, leaf);
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

/**
 * @brief Query month day bitmap from existing day directories
 */
OPERATE_RET local_store_query_month(uint32_t year, uint32_t month, uint32_t *day_bitmap)
{
    OPERATE_RET rt;
    char rec_base[LOCAL_STORE_PATH_MAX];
    char day_path[LOCAL_STORE_PATH_MAX];
    char idx_path[LOCAL_STORE_PATH_MAX];
    char date_dir[16];
    BOOL_T exists = FALSE;
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
    rt = __path_join(LOCAL_STORE_ROOT, LOCAL_STORE_REC_SUBDIR, rec_base, sizeof(rec_base));
    if (rt != OPRT_OK) {
        return rt;
    }
    for (d = 1; d <= 31; d++) {
        snprintf(date_dir, sizeof(date_dir), "%04u-%02u-%02u", (uint32_t)year, (uint32_t)month, (uint32_t)d);
        rt = __path_join(rec_base, date_dir, day_path, sizeof(day_path));
        if (rt != OPRT_OK) {
            continue;
        }
        exists = FALSE;
        (void)tkl_fs_is_exist(day_path, &exists);
        if (!exists) {
            continue;
        }
        /* Only mark days that have a readable index with >=1 segment */
        rt = __path_join(day_path, "index.txt", idx_path, sizeof(idx_path));
        if (rt != OPRT_OK) {
            continue;
        }
        exists = FALSE;
        (void)tkl_fs_is_exist(idx_path, &exists);
        if (!exists) {
            continue;
        }
        one_count = 1;
        if (local_store_query_day(year, month, d, &one, &one_count) == OPRT_OK && one_count > 0) {
            bits |= (1U << (d - 1));
        }
    }
    *day_bitmap = bits;
    return OPRT_OK;
}

/**
 * @brief Query day segments from index.txt
 */
OPERATE_RET local_store_query_day(uint32_t year, uint32_t month, uint32_t day, LOCAL_STORE_SEG_T *arr, uint32_t *count)
{
    OPERATE_RET rt;
    char idx_path[LOCAL_STORE_PATH_MAX];
    char line[192];
    TUYA_FILE fp;
    uint32_t cap;
    uint32_t filled = 0;
    uint32_t start_ts, end_ts, type;
    char leaf[64];

    if (arr == NULL || count == NULL || *count == 0) {
        return OPRT_INVALID_PARM;
    }
    cap = *count;
    *count = 0;

    rt = __index_path(year, month, day, idx_path, sizeof(idx_path));
    if (rt != OPRT_OK) {
        return rt;
    }
    {
        BOOL_T exists = FALSE;
        (void)tkl_fs_is_exist(idx_path, &exists);
        if (!exists) {
            return OPRT_OK;
        }
    }

    fp = tkl_fopen(idx_path, "r");
    if (fp == NULL) {
        return OPRT_FILE_OPEN_FAILED;
    }
    while (filled < cap && tkl_fgets(line, (int)sizeof(line), fp) != NULL) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') {
            continue;
        }
        leaf[0] = '\0';
        if (sscanf(line, "%u %u %u %63s", &start_ts, &end_ts, &type, leaf) < 4) {
            continue;
        }
        if (!__filename_is_safe(leaf)) {
            continue;
        }
        arr[filled].start_ts = start_ts;
        arr[filled].end_ts = end_ts;
        arr[filled].type = (uint16_t)type;
        snprintf(arr[filled].leaf, sizeof(arr[filled].leaf), "%s", leaf);
        filled++;
    }
    tkl_fclose(fp);
    *count = filled;
    return OPRT_OK;
}

/**
 * @brief Full path for a day leaf file
 */
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

/**
 * @brief Find a recording segment covering play_ts and build its full path
 * @param[in] play_ts playback time (epoch seconds)
 * @param[out] out_seg optional segment meta
 * @param[out] out_path full file path buffer
 * @param[in] path_len out_path size
 * @return OPRT_OK on success, OPRT_NOT_FOUND if no segment
 */
OPERATE_RET local_store_find_by_play_ts(uint32_t play_ts, LOCAL_STORE_SEG_T *out_seg, char *out_path,
                                        uint32_t path_len)
{
    OPERATE_RET rt;
    POSIX_TM_S tm;
    LOCAL_STORE_SEG_T segs[64];
    uint32_t count = 64;
    uint32_t i;
    int best = -1;
    uint32_t best_dist = 0xFFFFFFFFu;
    uint32_t year, month, day;

    if (out_path == NULL || path_len == 0 || play_ts == 0) {
        return OPRT_INVALID_PARM;
    }
    out_path[0] = '\0';
    memset(&tm, 0, sizeof(tm));
    rt = tal_time_get_local_time_custom((TIME_T)play_ts, &tm);
    if (rt != OPRT_OK) {
        return rt;
    }
    year = (uint32_t)(tm.tm_year + 1900);
    month = (uint32_t)(tm.tm_mon + 1);
    day = (uint32_t)tm.tm_mday;

    rt = local_store_query_day(year, month, day, segs, &count);
    if (rt != OPRT_OK) {
        return rt;
    }
    if (count == 0) {
        return OPRT_NOT_FOUND;
    }
    for (i = 0; i < count; i++) {
        if (play_ts >= segs[i].start_ts && play_ts <= segs[i].end_ts) {
            best = (int)i;
            break;
        }
        {
            uint32_t dist;
            if (play_ts < segs[i].start_ts) {
                dist = segs[i].start_ts - play_ts;
            } else {
                dist = play_ts - segs[i].end_ts;
            }
            if (dist < best_dist) {
                best_dist = dist;
                best = (int)i;
            }
        }
    }
    if (best < 0) {
        return OPRT_NOT_FOUND;
    }
    if (out_seg != NULL) {
        *out_seg = segs[best];
    }
    return local_store_day_file_path(year, month, day, segs[best].leaf, out_path, path_len);
}

/**
 * @brief Seed one Annex-B H264 file into today's index for PB bring-up
 * @param[in] src_path existing H264 file
 * @param[in] leaf destination leaf name
 * @param[in] duration_sec indexed duration in seconds
 * @return OPRT_OK on success
 */
OPERATE_RET local_store_seed_h264(const char *src_path, const char *leaf, uint32_t duration_sec)
{
    OPERATE_RET rt;
    char dst[LOCAL_STORE_PATH_MAX];
    char buf[4096];
    TUYA_FILE fin = NULL;
    TUYA_FILE fout = NULL;
    int nread;
    TIME_T now;
    uint32_t start_ts, end_ts;
    BOOL_T exists = FALSE;
    BOOL_T src_exists = FALSE;

    if (src_path == NULL || !__filename_is_safe(leaf)) {
        return OPRT_INVALID_PARM;
    }
    if (duration_sec == 0) {
        duration_sec = 60;
    }
    /* Require seed source before creating day dirs — avoids empty calendar dots */
    src_exists = FALSE;
    (void)tkl_fs_is_exist(src_path, &src_exists);
    if (!src_exists) {
        TAL_PR_ERR("seed src missing: %s", src_path);
        return OPRT_FILE_OPEN_FAILED;
    }
    rt = local_store_init();
    if (rt != OPRT_OK) {
        return rt;
    }
    rt = local_store_make_rec_path(leaf, dst, sizeof(dst));
    if (rt != OPRT_OK) {
        return rt;
    }
    /* Skip re-copy if already present */
    exists = FALSE;
    (void)tkl_fs_is_exist(dst, &exists);
    if (exists) {
        TAL_PR_NOTICE("local_store seed skip copy, exists: %s", dst);
    } else {
        fin = tkl_fopen(src_path, "rb");
        if (fin == NULL) {
            TAL_PR_ERR("seed open src %s failed", src_path);
            return OPRT_FILE_OPEN_FAILED;
        }
        fout = tkl_fopen(dst, "wb");
        if (fout == NULL) {
            tkl_fclose(fin);
            TAL_PR_ERR("seed open dst %s failed", dst);
            return OPRT_FILE_OPEN_FAILED;
        }
        for (;;) {
            nread = tkl_fread(buf, (int)sizeof(buf), fin);
            if (nread <= 0) {
                break;
            }
            if (tkl_fwrite(buf, nread, fout) != nread) {
                tkl_fclose(fin);
                tkl_fclose(fout);
                return OPRT_FILE_WRITE_FAILED;
            }
        }
        tkl_fclose(fin);
        tkl_fclose(fout);
    }

    now = tal_time_get_posix();
    if (now < (TIME_T)duration_sec) {
        start_ts = 1;
        end_ts = duration_sec;
    } else {
        end_ts = (uint32_t)now;
        start_ts = end_ts - duration_sec;
    }
    /* Skip duplicate index lines for the same leaf on reboot */
    {
        POSIX_TM_S tm;
        LOCAL_STORE_SEG_T segs[64];
        uint32_t count = 64;
        uint32_t i;
        uint32_t year, month, day;

        memset(&tm, 0, sizeof(tm));
        if (tal_time_get_local_time_custom((TIME_T)start_ts, &tm) == OPRT_OK) {
            year = (uint32_t)(tm.tm_year + 1900);
            month = (uint32_t)(tm.tm_mon + 1);
            day = (uint32_t)tm.tm_mday;
            if (local_store_query_day(year, month, day, segs, &count) == OPRT_OK) {
                for (i = 0; i < count; i++) {
                    if (strcmp(segs[i].leaf, leaf) == 0) {
                        TAL_PR_NOTICE("local_store seed skip index, leaf exists: %s", leaf);
                        return OPRT_OK;
                    }
                }
            }
        }
    }
    rt = local_store_index_append(start_ts, end_ts, 0, leaf);
    if (rt != OPRT_OK) {
        TAL_PR_ERR("seed index_append failed: %d", rt);
        return rt;
    }
    TAL_PR_NOTICE("local_store seed ok path=%s [%u,%u]", dst, start_ts, end_ts);
    return OPRT_OK;
}

/**
 * @brief Whether a recording segment is currently open
 * @return TRUE if open
 */
BOOL_T local_store_rec_is_open(void)
{
    return (s_rec_fp != NULL) ? TRUE : FALSE;
}

/**
 * @brief Elapsed seconds of the current open segment
 * @return seconds since start, or 0
 */
uint32_t local_store_rec_elapsed_sec(void)
{
    TIME_T now;

    if (s_rec_fp == NULL || s_rec_start_ts == 0) {
        return 0;
    }
    now = tal_time_get_posix();
    if (now <= (TIME_T)s_rec_start_ts) {
        return 0;
    }
    return (uint32_t)now - s_rec_start_ts;
}

/**
 * @brief Close current segment and append day index if non-empty
 * @return OPRT_OK on success
 */
OPERATE_RET local_store_rec_stop(void)
{
    OPERATE_RET rt = OPRT_OK;
    uint32_t end_ts;

    if (s_rec_fp == NULL) {
        return OPRT_OK;
    }
    tkl_fclose(s_rec_fp);
    s_rec_fp = NULL;

    end_ts = (uint32_t)tal_time_get_posix();
    if (end_ts < s_rec_start_ts) {
        end_ts = s_rec_start_ts;
    }
    if (s_rec_bytes == 0) {
        TAL_PR_WARN("local_store rec discard empty %s", s_rec_path);
        (void)tkl_fs_remove(s_rec_path);
    } else {
        if (end_ts == s_rec_start_ts) {
            end_ts = s_rec_start_ts + 1;
        }
        rt = local_store_index_append(s_rec_start_ts, end_ts, 0, s_rec_leaf);
        if (rt != OPRT_OK) {
            TAL_PR_ERR("local_store rec index_append failed: %d", rt);
        } else {
            TAL_PR_NOTICE("local_store rec stop %s bytes=%u [%u,%u]", s_rec_path, s_rec_bytes, s_rec_start_ts,
                          end_ts);
        }
    }
    s_rec_bytes = 0;
    s_rec_start_ts = 0;
    s_rec_leaf[0] = '\0';
    s_rec_path[0] = '\0';
    return rt;
}

/**
 * @brief Start a live Annex-B H264 recording segment
 * @param[in] leaf_prefix prefix for leaf name
 * @return OPRT_OK on success
 */
OPERATE_RET local_store_rec_start(const char *leaf_prefix)
{
    OPERATE_RET rt;
    TIME_T now;
    char leaf[64];
    int n;

    if (leaf_prefix == NULL || leaf_prefix[0] == '\0' || !__filename_is_safe(leaf_prefix)) {
        return OPRT_INVALID_PARM;
    }
    if (s_rec_fp != NULL) {
        (void)local_store_rec_stop();
    }
    rt = local_store_init();
    if (rt != OPRT_OK) {
        return rt;
    }
    now = tal_time_get_posix();
    if (now <= 0) {
        now = 1;
    }
    n = snprintf(leaf, sizeof(leaf), "%s_%u.h264", leaf_prefix, (uint32_t)now);
    if (n < 0 || (uint32_t)n >= sizeof(leaf) || !__filename_is_safe(leaf)) {
        return OPRT_BUFFER_NOT_ENOUGH;
    }
    rt = local_store_make_rec_path(leaf, s_rec_path, sizeof(s_rec_path));
    if (rt != OPRT_OK) {
        return rt;
    }
    s_rec_fp = tkl_fopen(s_rec_path, "wb");
    if (s_rec_fp == NULL) {
        TAL_PR_ERR("local_store rec open failed: %s", s_rec_path);
        s_rec_path[0] = '\0';
        return OPRT_FILE_OPEN_FAILED;
    }
    snprintf(s_rec_leaf, sizeof(s_rec_leaf), "%s", leaf);
    s_rec_start_ts = (uint32_t)now;
    s_rec_bytes = 0;
    TAL_PR_NOTICE("local_store rec start %s", s_rec_path);
    return OPRT_OK;
}

/**
 * @brief Append one encoded frame to the current recording segment
 * @param[in] data frame bytes
 * @param[in] len length
 * @return OPRT_OK on success
 */
OPERATE_RET local_store_rec_write(const uint8_t *data, uint32_t len)
{
    int n;

    if (data == NULL || len == 0) {
        return OPRT_INVALID_PARM;
    }
    if (s_rec_fp == NULL) {
        return OPRT_RESOURCE_NOT_READY;
    }
    n = tkl_fwrite((void *)data, (int)len, s_rec_fp);
    if (n != (int)len) {
        TAL_PR_ERR("local_store rec write fail want=%u got=%d", len, n);
        return OPRT_FILE_WRITE_FAILED;
    }
    s_rec_bytes += len;
    return OPRT_OK;
}
