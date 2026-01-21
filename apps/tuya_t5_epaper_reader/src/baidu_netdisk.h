#pragma once

#include "tuya_cloud_types.h"

typedef enum {
    BDNDK_VIEW_AUTH = 0,
    BDNDK_VIEW_LIST,
    BDNDK_VIEW_DETAIL,
    BDNDK_VIEW_MSG,
} BDNDK_VIEW_E;

typedef enum {
    BDNDK_WORK_IDLE = 0,
    BDNDK_WORK_AUTHING,
    BDNDK_WORK_LISTING,
    BDNDK_WORK_DOWNLOADING,
    BDNDK_WORK_OK,
    BDNDK_WORK_ERR,
} BDNDK_WORK_E;

typedef struct {
    char    name[128];
    char    path[256];
    char    fsid[64];
    INT64_T size;
    BOOL_T  is_dir;
} BDNDK_FILE_T;

typedef struct {
    INT64_T ctime;
    INT64_T mtime;
    int     category;
    char    md5[64];
    char    dlink[1024];
} BDNDK_DETAIL_META_T;

OPERATE_RET bdndk_start(void);
void        bdndk_stop(void);

void bdndk_storage_set(BOOL_T ready, const char *save_dir);

BDNDK_VIEW_E bdndk_view_get(void);
BDNDK_WORK_E bdndk_work_get(void);
int          bdndk_work_progress_get(int *out_percent);
BOOL_T       bdndk_need_refresh_fetch(void);

void bdndk_message_get(char *out, size_t out_len);
void bdndk_last_error_get(int *out_errno, char *out_errmsg, size_t errmsg_len, char *out_request_id,
                          size_t request_id_len);
void bdndk_last_download_url_get(char *out, size_t out_len);

BOOL_T bdndk_auth_info_get(char *out_qrcode_url, size_t qrcode_len, char *out_verify_url, size_t verify_len,
                           char *out_user_code, size_t user_code_len);

int    bdndk_list_count(void);
int    bdndk_list_copy(BDNDK_FILE_T *out, int max);
BOOL_T bdndk_list_get(int index, BDNDK_FILE_T *out);
BOOL_T bdndk_detail_get(BDNDK_FILE_T *out);
BOOL_T bdndk_detail_meta_get(BDNDK_DETAIL_META_T *out);

OPERATE_RET bdndk_select_detail(int index);
OPERATE_RET bdndk_request_detail(int index);
OPERATE_RET bdndk_request_download(int index, const char *save_dir);
