/**
 * @file local_store.h
 * @brief Local media storage path helpers (tal_fs)
 * @version 1.0
 * @date 2026-07-30
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef __LOCAL_STORE_H__
#define __LOCAL_STORE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#ifndef LOCAL_STORE_PATH_MAX
#define LOCAL_STORE_PATH_MAX 256
#endif

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Initialize local store and ensure root / recording base dirs exist
 * @return OPRT_OK on success, error code on failure
 * @note Safe to call more than once; subsequent calls are no-ops after success
 */
OPERATE_RET local_store_init(VOID_T);

/**
 * @brief Get configured storage root path
 * @return Pointer to root string (compile-time config), never NULL when enabled
 */
CONST CHAR_T *local_store_root(VOID_T);

/**
 * @brief Ensure directory exists (mkdir -p style via tal_fs)
 * @param[in] dir absolute or relative directory path
 * @return OPRT_OK on success, error code on failure
 */
OPERATE_RET local_store_ensure_dir(CONST CHAR_T *dir);

/**
 * @brief Build dated recording path: ROOT/REC/YYYY-MM-DD/filename and create dirs
 * @param[in] filename leaf name only (no '/' or ".." allowed), e.g. "cam0.mp4"
 * @param[out] out output path buffer
 * @param[in] out_len size of out in bytes
 * @return OPRT_OK on success, OPRT_INVALID_PARM / OPRT_BUFFER_NOT_ENOUGH on failure
 */
OPERATE_RET local_store_make_rec_path(CONST CHAR_T *filename, CHAR_T *out, UINT32_T out_len);

/**
 * @brief Build temporary sibling path by appending ".tmp" to final_path
 * @param[in] final_path target final path
 * @param[out] out output tmp path buffer
 * @param[in] out_len size of out in bytes
 * @return OPRT_OK on success
 */
OPERATE_RET local_store_make_tmp_path(CONST CHAR_T *final_path, CHAR_T *out, UINT32_T out_len);

/**
 * @brief Atomically promote tmp file to final path (tal_fs_rename)
 * @param[in] tmp_path temporary file path
 * @param[in] final_path destination path
 * @return OPRT_OK on success
 */
OPERATE_RET local_store_commit(CONST CHAR_T *tmp_path, CONST CHAR_T *final_path);

/* ---------------------------------------------------------------------------
 * Day index (align OS playback month/day query)
 * Index file: ROOT/rec/YYYY-MM-DD/index.txt
 * Line format: start_ts end_ts type leafname
 * --------------------------------------------------------------------------- */

/**
 * @brief One recording segment in the day index
 */
typedef struct {
    UINT32_T start_ts; /**< UTC/local epoch seconds */
    UINT32_T end_ts;   /**< Epoch seconds */
    UINT16_T type;     /**< 0=normal, reserved for event types */
    CHAR_T leaf[64];   /**< Filename only under the day directory */
} LOCAL_STORE_SEG_T;

/**
 * @brief Append one segment to the day index (creates day dir + index.txt)
 * @param[in] start_ts segment start epoch seconds
 * @param[in] end_ts segment end epoch seconds
 * @param[in] type segment type
 * @param[in] leaf filename only (must already exist or be about to commit)
 * @return OPRT_OK on success
 */
OPERATE_RET local_store_index_append(UINT32_T start_ts, UINT32_T end_ts, UINT16_T type, CONST CHAR_T *leaf);

/**
 * @brief Query which days of a month have recordings (bit0=day1 ... bit30=day31)
 * @param[in] year full year e.g. 2026
 * @param[in] month 1..12
 * @param[out] day_bitmap bit mask of days that have an index or media files
 * @return OPRT_OK on success
 */
OPERATE_RET local_store_query_month(UINT32_T year, UINT32_T month, UINT32_T *day_bitmap);

/**
 * @brief Query recording segments for one day
 * @param[in] year full year
 * @param[in] month 1..12
 * @param[in] day 1..31
 * @param[out] arr output segment array (caller-owned)
 * @param[in,out] count in: capacity, out: filled count
 * @return OPRT_OK on success (count may be 0)
 */
OPERATE_RET local_store_query_day(UINT32_T year, UINT32_T month, UINT32_T day, LOCAL_STORE_SEG_T *arr,
                                  UINT32_T *count);

/**
 * @brief Build full path for a day leaf: ROOT/rec/YYYY-MM-DD/leaf
 * @param[in] year full year
 * @param[in] month 1..12
 * @param[in] day 1..31
 * @param[in] leaf filename
 * @param[out] out path buffer
 * @param[in] out_len buffer size
 * @return OPRT_OK on success
 */
OPERATE_RET local_store_day_file_path(UINT32_T year, UINT32_T month, UINT32_T day, CONST CHAR_T *leaf, CHAR_T *out,
                                      UINT32_T out_len);

/**
 * @brief Find a recording segment covering play_ts and build its full path
 * @param[in] play_ts playback time (epoch seconds), from App PB start
 * @param[out] out_seg optional segment meta (may be NULL)
 * @param[out] out_path full file path buffer
 * @param[in] path_len out_path size
 * @return OPRT_OK on success, OPRT_NOT_FOUND if no segment
 * @note Picks first segment with start_ts <= play_ts <= end_ts; if none, nearest by start_ts
 */
OPERATE_RET local_store_find_by_play_ts(UINT32_T play_ts, LOCAL_STORE_SEG_T *out_seg, CHAR_T *out_path,
                                        UINT32_T path_len);

/**
 * @brief Seed one Annex-B H264 file into today's index for PB bring-up
 * @param[in] src_path existing H264 file (e.g. ./demo_video.264)
 * @param[in] leaf destination leaf name under day dir (e.g. "pb_demo.h264")
 * @param[in] duration_sec indexed duration in seconds (>=1)
 * @return OPRT_OK on success
 * @note Copies bytes via tal_fs; safe to call once at boot for demo
 */
OPERATE_RET local_store_seed_h264(CONST CHAR_T *src_path, CONST CHAR_T *leaf, UINT32_T duration_sec);

/**
 * @brief Start a live Annex-B H264 recording segment under today's day dir
 * @param[in] leaf_prefix filename prefix without path (e.g. "live"), leaf becomes prefix_ts.h264
 * @return OPRT_OK on success
 * @note Closes any previous open segment first. Aligns OS: encode path appends while live.
 */
OPERATE_RET local_store_rec_start(CONST CHAR_T *leaf_prefix);

/**
 * @brief Append one encoded frame to the current recording segment
 * @param[in] data Annex-B AU bytes
 * @param[in] len byte length
 * @return OPRT_OK on success, OPRT_RESOURCE_NOT_READY if no segment open
 */
OPERATE_RET local_store_rec_write(CONST UINT8_T *data, UINT32_T len);

/**
 * @brief Close current segment and append it to the day index
 * @return OPRT_OK on success (no-op if nothing open)
 * @note end_ts = now; empty segments (0 bytes) are discarded without index
 */
OPERATE_RET local_store_rec_stop(VOID_T);

/**
 * @brief Whether a recording segment is currently open
 * @return TRUE if open
 */
BOOL_T local_store_rec_is_open(VOID_T);

/**
 * @brief Elapsed seconds of the current open segment (0 if none)
 * @return seconds since segment start_ts
 */
UINT32_T local_store_rec_elapsed_sec(VOID_T);

#ifdef __cplusplus
}
#endif
#endif /* __LOCAL_STORE_H__ */
