/**
 * @file local_store.h
 * @brief Local media storage: segment recorder, day index and segment reader (tkl_fs)
 * @version 2.0
 * @date 2026-09-02
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef __LOCAL_STORE_H__
#define __LOCAL_STORE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

#ifndef LOCAL_STORE_PATH_MAX
#define LOCAL_STORE_PATH_MAX 256
#endif

#define LOCAL_STORE_FRAME_VIDEO_P 0u
#define LOCAL_STORE_FRAME_VIDEO_I 1u
#define LOCAL_STORE_FRAME_AUDIO   3u

/* Same 24-byte record as the cloud .media slice, so one segment can feed both. */
#pragma pack(push, 1)
typedef struct {
    uint32_t type;
    uint32_t size;
    uint64_t timestamp; /* wall clock ms */
    uint64_t pts;       /* capture clock us */
} LOCAL_STORE_FRAME_HDR_T;
#pragma pack(pop)

typedef struct {
    uint32_t start_ts;
    uint32_t end_ts;
    uint16_t type;
    char leaf[64];
} LOCAL_STORE_SEG_T;

typedef struct {
    uint32_t frames_in;
    uint32_t frames_dropped;
    uint32_t segments;
    uint32_t write_errors;
    uint32_t ring_used;
    uint32_t ring_cap;
    uint32_t write_max_ms;
    uint64_t bytes_written;
} LOCAL_STORE_REC_STAT_T;

typedef struct LOCAL_STORE_READER LOCAL_STORE_READER_T;

OPERATE_RET local_store_init(void);
const char *local_store_root(void);
OPERATE_RET local_store_ensure_dir(const char *dir);
OPERATE_RET local_store_make_rec_path(const char *filename, char *out, uint32_t out_len);
OPERATE_RET local_store_make_tmp_path(const char *final_path, char *out, uint32_t out_len);
OPERATE_RET local_store_commit(const char *tmp_path, const char *final_path);

/* Day index: ROOT/rec/YYYY-MM-DD/index.txt, one "start end type leaf" line per segment */
OPERATE_RET local_store_index_append(uint32_t start_ts, uint32_t end_ts, uint16_t type, const char *leaf);
OPERATE_RET local_store_query_month(uint32_t year, uint32_t month, uint32_t *day_bitmap);
OPERATE_RET local_store_query_day(uint32_t year, uint32_t month, uint32_t day, LOCAL_STORE_SEG_T *arr,
                                  uint32_t *count);
OPERATE_RET local_store_day_file_path(uint32_t year, uint32_t month, uint32_t day, const char *leaf, char *out,
                                      uint32_t out_len);
OPERATE_RET local_store_find_by_play_ts(uint32_t play_ts, LOCAL_STORE_SEG_T *out_seg, char *out_path,
                                        uint32_t path_len);
/**
 * @brief Next segment of the same day after start_ts
 * @return OPRT_OK, OPRT_NOT_FOUND when start_ts was the last one
 */
OPERATE_RET local_store_find_next_seg(uint32_t start_ts, LOCAL_STORE_SEG_T *out_seg, char *out_path,
                                      uint32_t path_len);
OPERATE_RET local_store_remove_day(uint32_t year, uint32_t month, uint32_t day);

/**
 * @brief Convert an Annex-B H264 file into a .media segment in today's index
 * @param[in] duration_sec seconds the file is indexed as; frames get evenly spaced timestamps
 */
OPERATE_RET local_store_seed_h264(const char *src_path, const char *leaf, uint32_t duration_sec);

/* ---------------------------------------------------------------------------
 * Recorder. rec_write copies into a ring and returns; a worker thread batches
 * the bytes to the filesystem and rolls segments on key frames.
 * --------------------------------------------------------------------------- */
OPERATE_RET local_store_rec_start(const char *leaf_prefix, uint32_t seg_sec);
OPERATE_RET local_store_rec_write(const uint8_t *data, uint32_t len, uint64_t ts_ms, BOOL_T is_key);
OPERATE_RET local_store_rec_write_audio(const uint8_t *data, uint32_t len, uint64_t ts_ms);
OPERATE_RET local_store_rec_stop(void);
BOOL_T local_store_rec_is_open(void);
uint32_t local_store_rec_elapsed_sec(void);
void local_store_rec_get_stat(LOCAL_STORE_REC_STAT_T *st);

/* ---------------------------------------------------------------------------
 * Reader. Payload pointers stay valid until the next reader call.
 * --------------------------------------------------------------------------- */
LOCAL_STORE_READER_T *local_store_reader_open(const char *path);
/**
 * @brief Position on the key frame at or before rel_ms from segment start
 */
OPERATE_RET local_store_reader_seek(LOCAL_STORE_READER_T *rd, uint32_t rel_ms);
/**
 * @return OPRT_OK, OPRT_NOT_FOUND at end of file, other on corrupt data
 */
OPERATE_RET local_store_reader_next(LOCAL_STORE_READER_T *rd, LOCAL_STORE_FRAME_HDR_T *hdr, const uint8_t **payload);
void local_store_reader_close(LOCAL_STORE_READER_T *rd);

#ifdef __cplusplus
}
#endif
#endif /* __LOCAL_STORE_H__ */
