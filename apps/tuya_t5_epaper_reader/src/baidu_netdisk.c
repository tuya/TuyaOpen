#include "baidu_netdisk.h"

#include "cJSON.h"
#include "http_client_interface.h"
#include "iotdns.h"
#include "netmgr.h"
#include "qrcodegen.h"
#include "tal_api.h"
#include "tkl_fs.h"
#include "tuya_register_center.h"

#include "transport_interface.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>

#ifndef BDNDK_APP_KEY
#ifdef CONFIG_BAIDU_NETDISK_APP_KEY
#define BDNDK_APP_KEY CONFIG_BAIDU_NETDISK_APP_KEY
#else
#define BDNDK_APP_KEY "8ODhu6uzbZ9rJ7jeY5RJOR8JqM56Tjlb"
#endif
#endif

#ifndef BDNDK_APP_SECRET
#ifdef CONFIG_BAIDU_NETDISK_APP_SECRET
#define BDNDK_APP_SECRET CONFIG_BAIDU_NETDISK_APP_SECRET
#else
#define BDNDK_APP_SECRET "UXqEW3dUrYCGzxBVoV5bVJORtFQmCD1d"
#endif
#endif

#ifndef BDNDK_TARGET_DIR
#ifdef CONFIG_BAIDU_NETDISK_TARGET_DIR
#define BDNDK_TARGET_DIR CONFIG_BAIDU_NETDISK_TARGET_DIR
#else
#define BDNDK_TARGET_DIR "/TuyaT5AI"
#endif
#endif

#ifndef BDNDK_SCOPE
#ifdef CONFIG_BAIDU_NETDISK_SCOPE
#define BDNDK_SCOPE CONFIG_BAIDU_NETDISK_SCOPE
#else
#define BDNDK_SCOPE "basic,netdisk"
#endif
#endif

#ifndef BDNDK_SDCARD_MOUNT_PATH
#define BDNDK_SDCARD_MOUNT_PATH "/sdcard"
#endif

#define BDNDK_TOKEN_DIR  BDNDK_SDCARD_MOUNT_PATH "/.sd_reader"
#define BDNDK_TOKEN_FILE BDNDK_TOKEN_DIR "/baidu_token.txt"

#define BDNDK_HTTP_TIMEOUT_MS 12000

typedef struct {
    char device_code[64];
    char user_code[16];
    char qrcode_url[256];
    char verification_url[128];
    int  expires_in;
    int  interval;
} bdndk_device_auth_t;

typedef struct {
    char      access_token[512];
    char      refresh_token[512];
    int       expires_in;
    long long save_time;
} bdndk_token_t;

static THREAD_HANDLE   sg_worker_thrd;
static volatile BOOL_T sg_running      = FALSE;
static volatile BOOL_T sg_need_refresh = FALSE;

static volatile BDNDK_VIEW_E sg_view             = BDNDK_VIEW_AUTH;
static volatile BDNDK_WORK_E sg_work             = BDNDK_WORK_IDLE;
static volatile int          sg_progress_percent = -1;

static bdndk_device_auth_t sg_auth;
static bdndk_token_t       sg_token;

static BDNDK_FILE_T sg_list[120];
static int          sg_list_count = 0;

static int          sg_selected_index = -1;
static BDNDK_FILE_T sg_detail;

static volatile int        sg_pending_download_index = -1;
static volatile int        sg_pending_detail_index   = -1;
static char                sg_last_msg[128];
static volatile BOOL_T     sg_storage_ready = FALSE;
static char                sg_save_dir[160] = BDNDK_SDCARD_MOUNT_PATH "/TuyaT5AI";
static BDNDK_DETAIL_META_T sg_detail_meta;
static char                sg_last_download_url[1024];
static int                 sg_last_errno = 0;
static char                sg_last_errmsg[128];
static char                sg_last_request_id[64];

static void set_msg(const char *s)
{
    if (!s)
        s = "";
    strncpy(sg_last_msg, s, sizeof(sg_last_msg) - 1);
    sg_last_msg[sizeof(sg_last_msg) - 1] = 0;
    sg_need_refresh                      = TRUE;
}

static void        trim_url_copy(char *dst, size_t dst_len, const char *src);
static void        url_unescape_json_inplace(char *s);
static OPERATE_RET https_get_json(const char *host, const char *path, const char *ua, char **out_body);

static void bdndk_clear_last_error(void)
{
    sg_last_errno         = 0;
    sg_last_errmsg[0]     = 0;
    sg_last_request_id[0] = 0;
}

static void bdndk_set_last_error(int err_no, const char *errmsg, const char *request_id)
{
    sg_last_errno = err_no;
    if (errmsg) {
        strncpy(sg_last_errmsg, errmsg, sizeof(sg_last_errmsg) - 1);
        sg_last_errmsg[sizeof(sg_last_errmsg) - 1] = 0;
    } else {
        sg_last_errmsg[0] = 0;
    }
    if (request_id) {
        strncpy(sg_last_request_id, request_id, sizeof(sg_last_request_id) - 1);
        sg_last_request_id[sizeof(sg_last_request_id) - 1] = 0;
    } else {
        sg_last_request_id[0] = 0;
    }
}

static void bdndk_log_last_error(const char *op)
{
    if (sg_last_errno == 0 && sg_last_errmsg[0] == 0 && sg_last_request_id[0] == 0)
        return;
    PR_ERR("Baidu %s failed: errno=%d errmsg=%s request_id=%s", op ? op : "op", sg_last_errno,
           sg_last_errmsg[0] ? sg_last_errmsg : "-", sg_last_request_id[0] ? sg_last_request_id : "-");
}

static void bdndk_log_long_text(const char *tag, const char *s)
{
    if (!s || !s[0])
        return;
    size_t       len   = strlen(s);
    size_t       off   = 0;
    const size_t chunk = 200;
    while (off < len) {
        size_t n = len - off;
        if (n > chunk)
            n = chunk;
        PR_NOTICE("%s[%u]: %.*s", tag ? tag : "TEXT", (unsigned)off, (int)n, s + off);
        off += n;
    }
}

static void fix_json_escape_chars(char *json)
{
    if (!json)
        return;
    char *pos = json;
    while ((pos = strstr(pos, "\\u0026")) != NULL) {
        memmove(pos + 1, pos + 6, strlen(pos) - 5);
        *pos = '&';
        pos += 1;
    }
    pos = json;
    while ((pos = strstr(pos, "\\u003d")) != NULL) {
        memmove(pos + 1, pos + 6, strlen(pos) - 5);
        *pos = '=';
        pos += 1;
    }
    pos = json;
    while ((pos = strstr(pos, "\\/")) != NULL) {
        memmove(pos + 1, pos + 2, strlen(pos) - 1);
        *pos = '/';
        pos += 1;
    }
}

static void bdndk_format_last_error_msg(char *out, size_t out_len, const char *prefix)
{
    if (!out || out_len == 0)
        return;
    if (!prefix)
        prefix = "";
    if (sg_last_errno != 0) {
        if (sg_last_errmsg[0]) {
            snprintf(out, out_len, "%s (%d): %.64s", prefix, sg_last_errno, sg_last_errmsg);
        } else {
            snprintf(out, out_len, "%s (%d)", prefix, sg_last_errno);
        }
    } else {
        snprintf(out, out_len, "%s", prefix);
    }
}

static int bdndk_capture_errno_json(const cJSON *root, const char *op)
{
    cJSON *errno_item = cJSON_GetObjectItem((cJSON *)root, "errno");
    if (!cJSON_IsNumber(errno_item) || errno_item->valueint == 0)
        return 0;
    cJSON *errmsg_item = cJSON_GetObjectItem((cJSON *)root, "errmsg");
    cJSON *req_id_item = cJSON_GetObjectItem((cJSON *)root, "request_id");
    bdndk_set_last_error(errno_item->valueint, cJSON_IsString(errmsg_item) ? errmsg_item->valuestring : NULL,
                         cJSON_IsString(req_id_item) ? req_id_item->valuestring : NULL);
    bdndk_log_last_error(op);
    return errno_item->valueint;
}

static void bdndk_capture_error_code_json(const cJSON *root, const char *op)
{
    cJSON *code = cJSON_GetObjectItem((cJSON *)root, "error_code");
    if (!cJSON_IsNumber(code))
        return;
    cJSON *msg = cJSON_GetObjectItem((cJSON *)root, "error_msg");
    bdndk_set_last_error(code->valueint, cJSON_IsString(msg) ? msg->valuestring : NULL, NULL);
    bdndk_log_last_error(op);
}

BOOL_T bdndk_need_refresh_fetch(void)
{
    BOOL_T v        = sg_need_refresh;
    sg_need_refresh = FALSE;
    return v;
}

BDNDK_VIEW_E bdndk_view_get(void)
{
    return sg_view;
}

BDNDK_WORK_E bdndk_work_get(void)
{
    return sg_work;
}

int bdndk_work_progress_get(int *out_percent)
{
    if (out_percent) {
        *out_percent = sg_progress_percent;
    }
    return 0;
}

void bdndk_message_get(char *out, size_t out_len)
{
    if (!out || out_len == 0)
        return;
    strncpy(out, sg_last_msg, out_len - 1);
    out[out_len - 1] = 0;
}

void bdndk_last_download_url_get(char *out, size_t out_len)
{
    if (!out || out_len == 0)
        return;
    strncpy(out, sg_last_download_url, out_len - 1);
    out[out_len - 1] = 0;
}

void bdndk_last_error_get(int *out_errno, char *out_errmsg, size_t errmsg_len, char *out_request_id,
                          size_t request_id_len)
{
    if (out_errno)
        *out_errno = sg_last_errno;
    if (out_errmsg && errmsg_len > 0) {
        strncpy(out_errmsg, sg_last_errmsg, errmsg_len - 1);
        out_errmsg[errmsg_len - 1] = 0;
    }
    if (out_request_id && request_id_len > 0) {
        strncpy(out_request_id, sg_last_request_id, request_id_len - 1);
        out_request_id[request_id_len - 1] = 0;
    }
}

BOOL_T bdndk_auth_info_get(char *out_qrcode_url, size_t qrcode_len, char *out_verify_url, size_t verify_len,
                           char *out_user_code, size_t user_code_len)
{
    if (out_qrcode_url && qrcode_len) {
        strncpy(out_qrcode_url, sg_auth.qrcode_url, qrcode_len - 1);
        out_qrcode_url[qrcode_len - 1] = 0;
    }
    if (out_verify_url && verify_len) {
        strncpy(out_verify_url, sg_auth.verification_url, verify_len - 1);
        out_verify_url[verify_len - 1] = 0;
    }
    if (out_user_code && user_code_len) {
        strncpy(out_user_code, sg_auth.user_code, user_code_len - 1);
        out_user_code[user_code_len - 1] = 0;
    }
    return (sg_auth.qrcode_url[0] != 0) ? TRUE : FALSE;
}

int bdndk_list_count(void)
{
    return sg_list_count;
}

int bdndk_list_copy(BDNDK_FILE_T *out, int max)
{
    if (!out || max <= 0)
        return 0;
    int n = sg_list_count;
    if (n > max)
        n = max;
    memcpy(out, sg_list, sizeof(BDNDK_FILE_T) * (size_t)n);
    return n;
}

BOOL_T bdndk_list_get(int index, BDNDK_FILE_T *out)
{
    if (!out)
        return FALSE;
    if (index < 0 || index >= sg_list_count)
        return FALSE;
    memcpy(out, &sg_list[index], sizeof(BDNDK_FILE_T));
    return TRUE;
}

BOOL_T bdndk_detail_get(BDNDK_FILE_T *out)
{
    if (!out)
        return FALSE;
    if (sg_selected_index < 0)
        return FALSE;
    memcpy(out, &sg_detail, sizeof(BDNDK_FILE_T));
    return TRUE;
}

BOOL_T bdndk_detail_meta_get(BDNDK_DETAIL_META_T *out)
{
    if (!out)
        return FALSE;
    if (sg_selected_index < 0)
        return FALSE;
    memcpy(out, &sg_detail_meta, sizeof(sg_detail_meta));
    return TRUE;
}

OPERATE_RET bdndk_select_detail(int index)
{
    if (index < 0 || index >= sg_list_count) {
        return OPRT_INVALID_PARM;
    }
    sg_selected_index = index;
    memcpy(&sg_detail, &sg_list[index], sizeof(BDNDK_FILE_T));
    sg_view         = BDNDK_VIEW_DETAIL;
    sg_need_refresh = TRUE;
    return OPRT_OK;
}

OPERATE_RET bdndk_request_detail(int index)
{
    if (index < 0 || index >= sg_list_count) {
        return OPRT_INVALID_PARM;
    }
    sg_pending_detail_index = index;
    sg_view                 = BDNDK_VIEW_MSG;
    sg_need_refresh         = TRUE;
    set_msg("Loading detail...");
    return OPRT_OK;
}

OPERATE_RET bdndk_request_download(int index, const char *save_dir)
{
    (void)save_dir;
    if (index < 0 || index >= sg_list_count) {
        return OPRT_INVALID_PARM;
    }
    if (!sg_storage_ready) {
        set_msg("SD not mounted");
        sg_view         = BDNDK_VIEW_MSG;
        sg_need_refresh = TRUE;
        return OPRT_COM_ERROR;
    }
    if (sg_list[index].is_dir) {
        set_msg("Directory can't download");
        sg_view         = BDNDK_VIEW_MSG;
        sg_need_refresh = TRUE;
        return OPRT_NOT_SUPPORTED;
    }
    sg_pending_download_index = index;
    sg_view                   = BDNDK_VIEW_MSG;
    sg_need_refresh           = TRUE;
    return OPRT_OK;
}

static OPERATE_RET fetch_detail_by_fsid(const bdndk_token_t *t, const BDNDK_FILE_T *fi, BDNDK_DETAIL_META_T *meta)
{
    if (!t || !fi || !meta)
        return OPRT_INVALID_PARM;
    memset(meta, 0, sizeof(*meta));

    size_t cap       = 1600;
    char  *path_meta = (char *)tal_malloc(cap);
    if (!path_meta)
        return OPRT_MALLOC_FAILED;
    snprintf(path_meta, cap, "/rest/2.0/xpan/multimedia?method=filemetas&access_token=%s&fsids=[%s]&dlink=1",
             t->access_token, fi->fsid);

    char       *body = NULL;
    OPERATE_RET rt   = https_get_json("pan.baidu.com", path_meta, "pan.baidu.com", &body);
    tal_free(path_meta);
    if (rt != OPRT_OK)
        return rt;
    fix_json_escape_chars(body);
    bdndk_log_long_text("FILEMETAS_RESP", body);
    cJSON *root = cJSON_Parse(body);
    tal_free(body);
    if (!root)
        return OPRT_CJSON_PARSE_ERR;

    if (bdndk_capture_errno_json(root, "filemetas") != 0) {
        cJSON_Delete(root);
        return OPRT_COM_ERROR;
    }
    cJSON *list  = cJSON_GetObjectItem(root, "list");
    cJSON *item0 = (cJSON_IsArray(list) && cJSON_GetArraySize(list) > 0) ? cJSON_GetArrayItem(list, 0) : NULL;
    if (!item0) {
        cJSON_Delete(root);
        return OPRT_COM_ERROR;
    }

    cJSON *cat   = cJSON_GetObjectItem(item0, "category");
    cJSON *ctime = cJSON_GetObjectItem(item0, "server_ctime");
    cJSON *mtime = cJSON_GetObjectItem(item0, "server_mtime");
    cJSON *md5   = cJSON_GetObjectItem(item0, "md5");
    cJSON *dlink = cJSON_GetObjectItem(item0, "dlink");

    meta->category = cJSON_IsNumber(cat) ? cat->valueint : 0;
    if (cJSON_IsNumber(ctime))
        meta->ctime = (INT64_T)ctime->valuedouble;
    if (cJSON_IsNumber(mtime))
        meta->mtime = (INT64_T)mtime->valuedouble;
    if (cJSON_IsString(md5)) {
        strncpy(meta->md5, md5->valuestring, sizeof(meta->md5) - 1);
        meta->md5[sizeof(meta->md5) - 1] = 0;
    }
    if (cJSON_IsString(dlink)) {
        char tmp[1024];
        trim_url_copy(tmp, sizeof(tmp), dlink->valuestring);
        url_unescape_json_inplace(tmp);
        if (tmp[0]) {
            if (strchr(tmp, '?')) {
                snprintf(meta->dlink, sizeof(meta->dlink), "%s&access_token=%s", tmp, t->access_token);
            } else {
                snprintf(meta->dlink, sizeof(meta->dlink), "%s?access_token=%s", tmp, t->access_token);
            }
            strncpy(sg_last_download_url, meta->dlink, sizeof(sg_last_download_url) - 1);
            sg_last_download_url[sizeof(sg_last_download_url) - 1] = 0;
            bdndk_log_long_text("DLINK", sg_last_download_url);
        }
    }

    cJSON_Delete(root);
    return OPRT_OK;
}

static BOOL_T token_is_valid(const bdndk_token_t *t)
{
    if (!t)
        return FALSE;
    if (t->save_time == 0 || t->expires_in <= 0)
        return FALSE;
    if (t->access_token[0] == 0)
        return FALSE;
    long long now  = (long long)tal_time_get_posix();
    long long used = now - t->save_time;
    if (used < 0)
        used = 0;
    if (used < (long long)t->expires_in - 3600)
        return TRUE;
    return FALSE;
}

static OPERATE_RET ensure_token_dir(void)
{
    BOOL_T exist = FALSE;
    if (tkl_fs_is_exist(BDNDK_TOKEN_DIR, &exist) == 0 && exist) {
        return OPRT_OK;
    }
    int mk = tkl_fs_mkdir(BDNDK_TOKEN_DIR);
    if (mk == 0)
        return OPRT_OK;
    if (tkl_fs_is_exist(BDNDK_TOKEN_DIR, &exist) == 0 && exist)
        return OPRT_OK;
    return OPRT_COM_ERROR;
}

static OPERATE_RET token_save(const bdndk_token_t *t)
{
    if (!t)
        return OPRT_INVALID_PARM;
    if (ensure_token_dir() != OPRT_OK)
        return OPRT_COM_ERROR;
    TUYA_FILE f = tkl_fopen(BDNDK_TOKEN_FILE, "w");
    if (!f)
        return OPRT_COM_ERROR;
    char buf[1200];
    int  n = snprintf(buf, sizeof(buf), "access_token=%s\nrefresh_token=%s\nexpires_in=%d\nsave_time=%lld\n",
                      t->access_token, t->refresh_token, t->expires_in, t->save_time);
    if (n < 0)
        n = 0;
    tkl_fwrite(buf, n, f);
    tkl_fclose(f);
    return OPRT_OK;
}

static OPERATE_RET token_load(bdndk_token_t *t)
{
    if (!t)
        return OPRT_INVALID_PARM;
    memset(t, 0, sizeof(*t));
    TUYA_FILE f = tkl_fopen(BDNDK_TOKEN_FILE, "r");
    if (!f)
        return OPRT_NOT_FOUND;
    char line[1024];
    while (tkl_fgets(line, sizeof(line), f)) {
        if (strncmp(line, "access_token=", 13) == 0) {
            strncpy(t->access_token, line + 13, sizeof(t->access_token) - 1);
            t->access_token[strcspn(t->access_token, "\r\n")] = 0;
        } else if (strncmp(line, "refresh_token=", 14) == 0) {
            strncpy(t->refresh_token, line + 14, sizeof(t->refresh_token) - 1);
            t->refresh_token[strcspn(t->refresh_token, "\r\n")] = 0;
        } else if (strncmp(line, "expires_in=", 11) == 0) {
            t->expires_in = atoi(line + 11);
        } else if (strncmp(line, "save_time=", 10) == 0) {
            t->save_time = atoll(line + 10);
        }
    }
    tkl_fclose(f);
    return (t->access_token[0] != 0) ? OPRT_OK : OPRT_COM_ERROR;
}

static int url_encode(const char *src, char *dst, int dst_len)
{
    if (!src || !dst || dst_len <= 0)
        return -1;
    int di = 0;
    for (int i = 0; src[i] && di < dst_len - 1; i++) {
        uint8_t c          = (uint8_t)src[i];
        BOOL_T  unreserved = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                            c == '_' || c == '.' || c == '~' || c == '/' || c == ':';
        if (unreserved) {
            dst[di++] = (char)c;
        } else {
            if (di + 3 >= dst_len)
                break;
            static const char hex[] = "0123456789ABCDEF";
            dst[di++]               = '%';
            dst[di++]               = hex[(c >> 4) & 0xF];
            dst[di++]               = hex[c & 0xF];
        }
    }
    dst[di] = 0;
    return 0;
}

static void trim_url_copy(char *dst, size_t dst_len, const char *src)
{
    if (!dst || dst_len == 0)
        return;
    dst[0] = 0;
    if (!src)
        return;
    while (*src == ' ' || *src == '\t' || *src == '\r' || *src == '\n' || *src == '`')
        src++;
    size_t n = strlen(src);
    while (n > 0 &&
           (src[n - 1] == ' ' || src[n - 1] == '\t' || src[n - 1] == '\r' || src[n - 1] == '\n' || src[n - 1] == '`')) {
        n--;
    }
    if (n >= dst_len)
        n = dst_len - 1;
    memcpy(dst, src, n);
    dst[n] = 0;
}

static void url_unescape_json_inplace(char *s)
{
    if (!s)
        return;

    const char *p = s;
    char       *w = s;
    while (*p) {
        if (p[0] == '\\' && p[1] == 'u' && p[2] == '0' && p[3] == '0') {
            if ((p[4] == '2' || p[4] == '3') && (p[5] == '6' || p[5] == 'd' || p[5] == 'D')) {
                char c = 0;
                if (p[4] == '2' && p[5] == '6')
                    c = '&';
                if (p[4] == '3' && (p[5] == 'd' || p[5] == 'D'))
                    c = '=';
                if (c) {
                    *w++ = c;
                    p += 6;
                    continue;
                }
            }
        }
        if (p[0] == '\\' && p[1] == '/') {
            *w++ = '/';
            p += 2;
            continue;
        }
        *w++ = *p++;
    }
    *w = 0;
}

static OPERATE_RET https_get_json(const char *host, const char *path, const char *ua, char **out_body)
{
    if (!host || !path || !out_body)
        return OPRT_INVALID_PARM;
    *out_body           = NULL;
    uint8_t *cacert     = NULL;
    uint16_t cacert_len = 0;
    char     url_for_cert[256];
    snprintf(url_for_cert, sizeof(url_for_cert), "https://%s", host);
    if (tuya_iotdns_query_domain_certs(url_for_cert, &cacert, &cacert_len) != OPRT_OK) {
        return OPRT_COM_ERROR;
    }
    if (!cacert || cacert_len == 0) {
        if (cacert)
            tal_free(cacert);
        return OPRT_COM_ERROR;
    }

    http_client_header_t headers[] = {
        {.key = "User-Agent", .value = (char *)(ua ? ua : "pan.baidu.com")},
        {.key = "Connection", .value = "close"},
    };
    http_client_response_t resp = {0};
    http_client_status_t   st   = http_client_request(
        &(const http_client_request_t){
                .cacert        = cacert,
                .cacert_len    = cacert_len,
                .host          = host,
                .port          = 443,
                .method        = "GET",
                .path          = path,
                .headers       = headers,
                .headers_count = 2,
                .body          = NULL,
                .body_length   = 0,
                .timeout_ms    = BDNDK_HTTP_TIMEOUT_MS,
        },
        &resp);
    tal_free(cacert);
    if (st != HTTP_CLIENT_SUCCESS) {
        http_client_free(&resp);
        return OPRT_COM_ERROR;
    }
    if (resp.status_code < 200 || resp.status_code >= 300) {
        PR_ERR("HTTP %s status=%u", host, (unsigned)resp.status_code);
        if (resp.body && resp.body_length > 0) {
            size_t n = resp.body_length > 256 ? 256 : resp.body_length;
            PR_ERR("HTTP %s body: %.*s", host, (int)n, (const char *)resp.body);
        }
    }
    if (!resp.body || resp.body_length == 0) {
        http_client_free(&resp);
        return OPRT_COM_ERROR;
    }
    char *b = (char *)tal_malloc(resp.body_length + 1);
    if (!b) {
        http_client_free(&resp);
        return OPRT_MALLOC_FAILED;
    }
    memcpy(b, resp.body, resp.body_length);
    b[resp.body_length] = 0;
    http_client_free(&resp);
    *out_body = b;
    return OPRT_OK;
}

static OPERATE_RET get_device_auth(bdndk_device_auth_t *out)
{
    if (!out)
        return OPRT_INVALID_PARM;
    memset(out, 0, sizeof(*out));
    char path[768];
    snprintf(path, sizeof(path), "/oauth/2.0/device/code?response_type=device_code&client_id=%s&scope=%s",
             BDNDK_APP_KEY, BDNDK_SCOPE);

    char       *body = NULL;
    OPERATE_RET rt   = https_get_json("openapi.baidu.com", path, "pan.baidu.com", &body);
    if (rt != OPRT_OK)
        return rt;

    cJSON *root = cJSON_Parse(body);
    tal_free(body);
    if (!root)
        return OPRT_CJSON_PARSE_ERR;

    cJSON *d_code = cJSON_GetObjectItem(root, "device_code");
    cJSON *u_code = cJSON_GetObjectItem(root, "user_code");
    cJSON *qr_url = cJSON_GetObjectItem(root, "qrcode_url");
    cJSON *v_url  = cJSON_GetObjectItem(root, "verification_url");
    cJSON *exp    = cJSON_GetObjectItem(root, "expires_in");
    cJSON *inter  = cJSON_GetObjectItem(root, "interval");

    if (!cJSON_IsString(d_code) || !cJSON_IsString(u_code) || !cJSON_IsString(qr_url) || !cJSON_IsString(v_url) ||
        !cJSON_IsNumber(exp) || !cJSON_IsNumber(inter)) {
        cJSON_Delete(root);
        return OPRT_COM_ERROR;
    }

    strncpy(out->device_code, d_code->valuestring, sizeof(out->device_code) - 1);
    strncpy(out->user_code, u_code->valuestring, sizeof(out->user_code) - 1);
    strncpy(out->qrcode_url, qr_url->valuestring, sizeof(out->qrcode_url) - 1);
    strncpy(out->verification_url, v_url->valuestring, sizeof(out->verification_url) - 1);
    out->expires_in = exp->valueint;
    out->interval   = inter->valueint;
    cJSON_Delete(root);
    return OPRT_OK;
}

static OPERATE_RET poll_access_token(const bdndk_device_auth_t *auth, bdndk_token_t *out)
{
    if (!auth || !out)
        return OPRT_INVALID_PARM;
    memset(out, 0, sizeof(*out));

    int max_attempts = (auth->interval > 0) ? (auth->expires_in / auth->interval) : 0;
    if (max_attempts <= 0)
        max_attempts = 60;

    for (int i = 0; sg_running && i < max_attempts; i++) {
        char path[1024];
        snprintf(path, sizeof(path), "/oauth/2.0/token?grant_type=device_token&code=%s&client_id=%s&client_secret=%s",
                 auth->device_code, BDNDK_APP_KEY, BDNDK_APP_SECRET);

        char       *body = NULL;
        OPERATE_RET rt   = https_get_json("openapi.baidu.com", path, "pan.baidu.com", &body);
        if (rt != OPRT_OK) {
            set_msg("Auth: net error");
            tal_system_sleep(auth->interval * 1000);
            continue;
        }

        cJSON *root = cJSON_Parse(body);
        tal_free(body);
        if (!root) {
            tal_system_sleep(auth->interval * 1000);
            continue;
        }

        cJSON *err = cJSON_GetObjectItem(root, "error");
        if (cJSON_IsString(err)) {
            cJSON *desc = cJSON_GetObjectItem(root, "error_description");
            bdndk_set_last_error(0, cJSON_IsString(desc) ? desc->valuestring : err->valuestring, NULL);
            bdndk_log_last_error("auth");
            const char *ev = err->valuestring;
            if (strcmp(ev, "authorization_pending") == 0) {
                set_msg("Waiting for authorize...");
                cJSON_Delete(root);
                tal_system_sleep(auth->interval * 1000);
                continue;
            }
            cJSON_Delete(root);
            return OPRT_COM_ERROR;
        }

        cJSON *at   = cJSON_GetObjectItem(root, "access_token");
        cJSON *rtok = cJSON_GetObjectItem(root, "refresh_token");
        cJSON *exp  = cJSON_GetObjectItem(root, "expires_in");
        if (!cJSON_IsString(at) || !cJSON_IsString(rtok) || !cJSON_IsNumber(exp)) {
            cJSON_Delete(root);
            return OPRT_COM_ERROR;
        }

        strncpy(out->access_token, at->valuestring, sizeof(out->access_token) - 1);
        strncpy(out->refresh_token, rtok->valuestring, sizeof(out->refresh_token) - 1);
        out->expires_in = exp->valueint;
        out->save_time  = (long long)tal_time_get_posix();
        cJSON_Delete(root);
        return OPRT_OK;
    }
    return OPRT_TIMEOUT;
}

static OPERATE_RET refresh_access_token(const bdndk_token_t *old_t, bdndk_token_t *out)
{
    if (!old_t || !out)
        return OPRT_INVALID_PARM;
    if (old_t->refresh_token[0] == 0)
        return OPRT_NOT_FOUND;

    size_t cap  = 2048;
    char  *path = (char *)tal_malloc(cap);
    if (!path)
        return OPRT_MALLOC_FAILED;
    snprintf(path, cap, "/oauth/2.0/token?grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s",
             old_t->refresh_token, BDNDK_APP_KEY, BDNDK_APP_SECRET);
    char       *body = NULL;
    OPERATE_RET rt   = https_get_json("openapi.baidu.com", path, "pan.baidu.com", &body);
    tal_free(path);
    if (rt != OPRT_OK)
        return rt;

    cJSON *root = cJSON_Parse(body);
    tal_free(body);
    if (!root)
        return OPRT_CJSON_PARSE_ERR;
    cJSON *err = cJSON_GetObjectItem(root, "error");
    if (cJSON_IsString(err)) {
        cJSON *desc = cJSON_GetObjectItem(root, "error_description");
        bdndk_set_last_error(0, cJSON_IsString(desc) ? desc->valuestring : err->valuestring, NULL);
        bdndk_log_last_error("refresh");
        cJSON_Delete(root);
        return OPRT_COM_ERROR;
    }
    cJSON *at   = cJSON_GetObjectItem(root, "access_token");
    cJSON *rtok = cJSON_GetObjectItem(root, "refresh_token");
    cJSON *exp  = cJSON_GetObjectItem(root, "expires_in");
    if (!cJSON_IsString(at) || !cJSON_IsString(rtok) || !cJSON_IsNumber(exp)) {
        cJSON_Delete(root);
        return OPRT_COM_ERROR;
    }
    memset(out, 0, sizeof(*out));
    strncpy(out->access_token, at->valuestring, sizeof(out->access_token) - 1);
    strncpy(out->refresh_token, rtok->valuestring, sizeof(out->refresh_token) - 1);
    out->expires_in = exp->valueint;
    out->save_time  = (long long)tal_time_get_posix();
    cJSON_Delete(root);
    return OPRT_OK;
}

static OPERATE_RET get_baidu_list(const bdndk_token_t *t)
{
    if (!t)
        return OPRT_INVALID_PARM;
    bdndk_clear_last_error();

    char enc_dir[700];
    url_encode(BDNDK_TARGET_DIR, enc_dir, sizeof(enc_dir));
    size_t cap  = 2400;
    char  *path = (char *)tal_malloc(cap);
    if (!path)
        return OPRT_MALLOC_FAILED;
    snprintf(path, cap,
             "/rest/2.0/xpan/file?method=list&access_token=%s&dir=%s&order=time&desc=1&start=0&limit=%d&folder=0",
             t->access_token, enc_dir, (int)(sizeof(sg_list) / sizeof(sg_list[0])));

    char       *body = NULL;
    OPERATE_RET rt   = https_get_json("pan.baidu.com", path, "pan.baidu.com", &body);
    tal_free(path);
    if (rt != OPRT_OK)
        return rt;
    fix_json_escape_chars(body);

    cJSON *root = cJSON_Parse(body);
    tal_free(body);
    if (!root)
        return OPRT_CJSON_PARSE_ERR;

    if (bdndk_capture_errno_json(root, "list") != 0) {
        cJSON_Delete(root);
        return OPRT_COM_ERROR;
    }
    cJSON *list = cJSON_GetObjectItem(root, "list");
    if (!cJSON_IsArray(list)) {
        sg_list_count = 0;
        cJSON_Delete(root);
        return OPRT_OK;
    }

    int n = cJSON_GetArraySize(list);
    if (n < 0)
        n = 0;
    if (n > (int)(sizeof(sg_list) / sizeof(sg_list[0])))
        n = (int)(sizeof(sg_list) / sizeof(sg_list[0]));
    sg_list_count = 0;
    for (int i = 0; i < n; i++) {
        cJSON *it = cJSON_GetArrayItem(list, i);
        if (!it)
            continue;
        cJSON *fn    = cJSON_GetObjectItem(it, "server_filename");
        cJSON *p     = cJSON_GetObjectItem(it, "path");
        cJSON *isdir = cJSON_GetObjectItem(it, "isdir");
        cJSON *sz    = cJSON_GetObjectItem(it, "size");
        cJSON *fsid  = cJSON_GetObjectItem(it, "fs_id");
        if (!cJSON_IsString(fn) || !cJSON_IsString(p) || !cJSON_IsNumber(isdir))
            continue;
        BDNDK_FILE_T *dst = &sg_list[sg_list_count++];
        memset(dst, 0, sizeof(*dst));
        strncpy(dst->name, fn->valuestring, sizeof(dst->name) - 1);
        strncpy(dst->path, p->valuestring, sizeof(dst->path) - 1);
        dst->is_dir = (isdir->valueint != 0) ? TRUE : FALSE;
        if (cJSON_IsNumber(sz))
            dst->size = (INT64_T)sz->valuedouble;
        if (cJSON_IsNumber(fsid))
            snprintf(dst->fsid, sizeof(dst->fsid), "%lld", (long long)fsid->valuedouble);
        else if (cJSON_IsString(fsid))
            strncpy(dst->fsid, fsid->valuestring, sizeof(dst->fsid) - 1);
    }
    cJSON_Delete(root);
    sg_need_refresh = TRUE;
    return OPRT_OK;
}

static OPERATE_RET parse_url(const char *url, char *scheme, size_t scheme_len, char *host, size_t host_len,
                             uint16_t *port, char *path, size_t path_len)
{
    if (!url || !scheme || !host || !port || !path)
        return OPRT_INVALID_PARM;
    scheme[0] = 0;
    host[0]   = 0;
    path[0]   = 0;
    *port     = 0;

    const char *p          = strstr(url, "://");
    const char *host_begin = url;
    if (p) {
        size_t n = (size_t)(p - url);
        if (n >= scheme_len)
            n = scheme_len - 1;
        memcpy(scheme, url, n);
        scheme[n]  = 0;
        host_begin = p + 3;
    } else {
        strncpy(scheme, "https", scheme_len - 1);
        scheme[scheme_len - 1] = 0;
    }

    const char *path_begin = strchr(host_begin, '/');
    if (!path_begin)
        path_begin = host_begin + strlen(host_begin);
    size_t hostpart_len = (size_t)(path_begin - host_begin);

    char hostpart[256];
    if (hostpart_len >= sizeof(hostpart))
        hostpart_len = sizeof(hostpart) - 1;
    memcpy(hostpart, host_begin, hostpart_len);
    hostpart[hostpart_len] = 0;

    char *colon = strchr(hostpart, ':');
    if (colon) {
        *colon = 0;
        *port  = (uint16_t)atoi(colon + 1);
    } else {
        *port = (strcmp(scheme, "https") == 0) ? 443 : 80;
    }

    strncpy(host, hostpart, host_len - 1);
    host[host_len - 1] = 0;

    if (*path_begin) {
        strncpy(path, path_begin, path_len - 1);
        path[path_len - 1] = 0;
    } else {
        strncpy(path, "/", path_len - 1);
        path[path_len - 1] = 0;
    }
    return OPRT_OK;
}

static OPERATE_RET send_all(const TransportInterface_t *transport, const uint8_t *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int32_t r = transport->send(transport->pNetworkContext, data + sent, len - sent);
        if (r <= 0)
            return OPRT_COM_ERROR;
        sent += (size_t)r;
    }
    return OPRT_OK;
}

static char *find_header_end(char *buf, size_t len)
{
    if (!buf || len < 4)
        return NULL;
    for (size_t i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            return &buf[i];
        }
    }
    return NULL;
}

static int parse_status_code(const char *headers)
{
    const char *sp = headers ? strchr(headers, ' ') : NULL;
    if (!sp)
        return -1;
    return atoi(sp + 1);
}

static const char *find_header_value(const char *headers, const char *key, size_t *out_len)
{
    size_t key_len = key ? strlen(key) : 0;
    if (!headers || !key || key_len == 0)
        return NULL;
    const char *p = headers;
    while (*p) {
        const char *line_end = strstr(p, "\r\n");
        if (!line_end)
            break;
        if (line_end == p)
            break;
        if ((size_t)(line_end - p) > key_len + 1 && strncasecmp(p, key, key_len) == 0 && p[key_len] == ':') {
            const char *v = p + key_len + 1;
            while (*v == ' ' || *v == '\t')
                v++;
            const char *vend = line_end;
            while (vend > v && (vend[-1] == ' ' || vend[-1] == '\t'))
                vend--;
            if (out_len)
                *out_len = (size_t)(vend - v);
            return v;
        }
        p = line_end + 2;
    }
    return NULL;
}

static BOOL_T header_contains_token(const char *v, size_t v_len, const char *token)
{
    size_t t_len = token ? strlen(token) : 0;
    if (!v || !token || t_len == 0 || v_len < t_len)
        return FALSE;
    for (size_t i = 0; i + t_len <= v_len; i++) {
        size_t j = 0;
        for (; j < t_len; j++) {
            char a = (char)tolower((unsigned char)v[i + j]);
            char b = (char)tolower((unsigned char)token[j]);
            if (a != b)
                break;
        }
        if (j == t_len)
            return TRUE;
    }
    return FALSE;
}

typedef struct {
    const TransportInterface_t *transport;
    const uint8_t              *init;
    size_t                      init_len;
    size_t                      init_pos;
    uint8_t                     buf[512];
    size_t                      len;
    size_t                      pos;
} StreamReader_t;

static int stream_fill(StreamReader_t *sr)
{
    if (sr->init_pos < sr->init_len) {
        size_t n = sr->init_len - sr->init_pos;
        if (n > sizeof(sr->buf))
            n = sizeof(sr->buf);
        memcpy(sr->buf, sr->init + sr->init_pos, n);
        sr->init_pos += n;
        sr->len = n;
        sr->pos = 0;
        return (int)n;
    }
    int32_t r = sr->transport->recv(sr->transport->pNetworkContext, sr->buf, sizeof(sr->buf));
    if (r <= 0)
        return -1;
    sr->len = (size_t)r;
    sr->pos = 0;
    return r;
}

static int stream_read(StreamReader_t *sr, uint8_t *out, size_t want)
{
    size_t total = 0;
    while (total < want) {
        if (sr->pos >= sr->len) {
            if (stream_fill(sr) <= 0)
                return (total > 0) ? (int)total : -1;
        }
        size_t n = sr->len - sr->pos;
        if (n > want - total)
            n = want - total;
        memcpy(out + total, sr->buf + sr->pos, n);
        sr->pos += n;
        total += n;
    }
    return (int)total;
}

static int stream_read_line(StreamReader_t *sr, char *line, size_t max)
{
    size_t i = 0;
    while (i + 1 < max) {
        uint8_t c = 0;
        if (stream_read(sr, &c, 1) <= 0)
            return -1;
        line[i++] = (char)c;
        if (i >= 2 && line[i - 2] == '\r' && line[i - 1] == '\n') {
            line[i] = 0;
            return (int)i;
        }
    }
    return -1;
}

static void update_progress(INT64_T written, INT64_T file_size)
{
    if (file_size > 0) {
        int pct = (int)((written * 100) / file_size);
        if (pct < 0)
            pct = 0;
        if (pct > 100)
            pct = 100;
        if (pct != sg_progress_percent) {
            sg_progress_percent = pct;
            sg_need_refresh     = TRUE;
        }
    }
}

static OPERATE_RET recv_chunked_to_file(const TransportInterface_t *transport, const uint8_t *init, size_t init_len,
                                        TUYA_FILE f, INT64_T *written, INT64_T file_size)
{
    StreamReader_t sr = {
        .transport = transport,
        .init      = init,
        .init_len  = init_len,
        .init_pos  = 0,
        .len       = 0,
        .pos       = 0,
    };
    char    line[64];
    uint8_t buf[1024];

    for (;;) {
        if (stream_read_line(&sr, line, sizeof(line)) <= 0)
            return OPRT_COM_ERROR;
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        char         *end        = NULL;
        unsigned long chunk_size = strtoul(p, &end, 16);
        if (end == p)
            return OPRT_COM_ERROR;
        if (chunk_size == 0) {
            do {
                if (stream_read_line(&sr, line, sizeof(line)) <= 0)
                    return OPRT_COM_ERROR;
            } while (strcmp(line, "\r\n") != 0);
            return OPRT_OK;
        }
        size_t remaining = (size_t)chunk_size;
        while (remaining > 0) {
            size_t want = remaining > sizeof(buf) ? sizeof(buf) : remaining;
            int    r    = stream_read(&sr, buf, want);
            if (r <= 0)
                return OPRT_COM_ERROR;
            tkl_fwrite(buf, r, f);
            *written += r;
            update_progress(*written, file_size);
            remaining -= (size_t)r;
        }
        if (stream_read(&sr, buf, 2) != 2)
            return OPRT_COM_ERROR;
    }
}

static OPERATE_RET http_get_stream_to_file(const char *url, const char *ua, const char *save_path, INT64_T file_size)
{
    if (!url || !save_path)
        return OPRT_INVALID_PARM;

    char *cur_url  = (char *)tal_malloc(4096);
    char *path     = (char *)tal_malloc(4096);
    char *next_url = (char *)tal_malloc(4096);
    if (!cur_url || !path || !next_url) {
        if (cur_url)
            tal_free(cur_url);
        if (path)
            tal_free(path);
        if (next_url)
            tal_free(next_url);
        return OPRT_MALLOC_FAILED;
    }
    strncpy(cur_url, url, 4095);
    cur_url[4095] = 0;

    for (int redirects = 0; redirects < 5; redirects++) {
        uint8_t *cacert     = NULL;
        uint16_t cacert_len = 0;
        if (tuya_iotdns_query_domain_certs(cur_url, &cacert, &cacert_len) != OPRT_OK) {
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_COM_ERROR;
        }
        if (!cacert || cacert_len == 0) {
            if (cacert)
                tal_free(cacert);
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_COM_ERROR;
        }

        char     scheme[8], host[256];
        uint16_t port = 0;
        if (parse_url(cur_url, scheme, sizeof(scheme), host, sizeof(host), &port, path, 4096) != OPRT_OK) {
            tal_free(cacert);
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_INVALID_PARM;
        }

        TUYA_TRANSPORT_TYPE_E transport_type = (strcmp(scheme, "https") == 0) ? TRANSPORT_TYPE_TLS : TRANSPORT_TYPE_TCP;
        NetworkContext_t      network        = tuya_transporter_create(transport_type, NULL);
        if (!network) {
            tal_free(cacert);
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_MALLOC_FAILED;
        }

        if (transport_type == TRANSPORT_TYPE_TLS) {
            tuya_tls_config_t tls_config = {
                .ca_cert      = (char *)cacert,
                .ca_cert_size = cacert_len,
                .hostname     = host,
                .port         = port,
                .timeout      = BDNDK_HTTP_TIMEOUT_MS,
                .mode         = TUYA_TLS_SERVER_CERT_MODE,
                .verify       = true,
            };
            if (tuya_transporter_ctrl(network, TUYA_TRANSPORTER_SET_TLS_CONFIG, &tls_config) != OPRT_OK) {
                tal_free(cacert);
                tuya_transporter_destroy(network);
                tal_free(cur_url);
                tal_free(path);
                tal_free(next_url);
                return OPRT_COM_ERROR;
            }
        }

        if (tuya_transporter_connect(network, host, port, BDNDK_HTTP_TIMEOUT_MS) != OPRT_OK) {
            tal_free(cacert);
            tuya_transporter_close(network);
            tuya_transporter_destroy(network);
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_COM_ERROR;
        }

        TransportInterface_t transport = {
            .pNetworkContext = (NetworkContext_t *)&network,
            .send            = (TransportSend_t)NetworkTransportSend,
            .recv            = (TransportRecv_t)NetworkTransportRecv,
        };

        const char *ua_use  = (ua && ua[0]) ? ua : "pan.baidu.com";
        size_t      req_cap = strlen(path) + strlen(host) + strlen(ua_use) + 128;
        char       *req_buf = (char *)tal_malloc(req_cap);
        if (!req_buf) {
            tal_free(cacert);
            tuya_transporter_close(network);
            tuya_transporter_destroy(network);
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_MALLOC_FAILED;
        }
        int req_len = snprintf(req_buf, req_cap,
                               "GET %s HTTP/1.1\r\n"
                               "Host: %s\r\n"
                               "User-Agent: %s\r\n"
                               "Connection: close\r\n"
                               "Accept-Encoding: identity\r\n"
                               "\r\n",
                               path, host, ua_use);
        if (req_len <= 0 || (size_t)req_len >= req_cap) {
            tal_free(req_buf);
            tal_free(cacert);
            tuya_transporter_close(network);
            tuya_transporter_destroy(network);
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_COM_ERROR;
        }
        tal_free(cacert);

        PR_NOTICE("Download request:\n%s", req_buf);
        if (send_all(&transport, (const uint8_t *)req_buf, (size_t)req_len) != OPRT_OK) {
            tal_free(req_buf);
            PR_ERR("Download http send failed");
            tuya_transporter_close(network);
            tuya_transporter_destroy(network);
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_COM_ERROR;
        }
        tal_free(req_buf);

        char *hdr_buf = (char *)tal_malloc(8192);
        if (!hdr_buf) {
            tuya_transporter_close(network);
            tuya_transporter_destroy(network);
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_MALLOC_FAILED;
        }
        size_t hdr_total = 0;
        char  *hdr_end   = NULL;
        while (!hdr_end && hdr_total < 8192) {
            int32_t r = transport.recv(transport.pNetworkContext, (uint8_t *)hdr_buf + hdr_total, 8192 - hdr_total);
            if (r <= 0) {
                tal_free(hdr_buf);
                tuya_transporter_close(network);
                tuya_transporter_destroy(network);
                tal_free(cur_url);
                tal_free(path);
                tal_free(next_url);
                return OPRT_COM_ERROR;
            }
            hdr_total += (size_t)r;
            hdr_end = find_header_end(hdr_buf, hdr_total);
        }
        if (!hdr_end) {
            tal_free(hdr_buf);
            tuya_transporter_close(network);
            tuya_transporter_destroy(network);
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_INVALID_PARM;
        }
        size_t header_len          = (size_t)(hdr_end - hdr_buf) + 4;
        char   saved               = *hdr_end;
        *hdr_end                   = 0;
        int            status_code = parse_status_code(hdr_buf);
        size_t         body_len    = hdr_total > header_len ? (hdr_total - header_len) : 0;
        const uint8_t *body_start  = (const uint8_t *)(hdr_buf + header_len);

        if (status_code == 301 || status_code == 302 || status_code == 303 || status_code == 307 ||
            status_code == 308) {
            size_t      loc_len = 0;
            const char *loc     = find_header_value(hdr_buf, "Location", &loc_len);
            if (!loc || loc_len == 0) {
                *hdr_end = saved;
                tal_free(hdr_buf);
                tuya_transporter_close(network);
                tuya_transporter_destroy(network);
                tal_free(cur_url);
                tal_free(path);
                tal_free(next_url);
                return OPRT_COM_ERROR;
            }

            size_t n = loc_len;
            if (n >= 4096)
                n = 4095;
            memcpy(next_url, loc, n);
            next_url[n] = 0;

            if (strncmp(next_url, "http://", 7) != 0 && strncmp(next_url, "https://", 8) != 0) {
                if (next_url[0] == '/') {
                    snprintf(cur_url, 4096, "%s://%s:%d%s", scheme, host, (int)port, next_url);
                } else {
                    snprintf(cur_url, 4096, "%s://%s:%d/%s", scheme, host, (int)port, next_url);
                }
            } else {
                strncpy(cur_url, next_url, 4095);
                cur_url[4095] = 0;
            }

            *hdr_end = saved;
            tal_free(hdr_buf);
            tuya_transporter_close(network);
            tuya_transporter_destroy(network);
            continue;
        }

        size_t      te_len     = 0;
        const char *te         = find_header_value(hdr_buf, "Transfer-Encoding", &te_len);
        BOOL_T      is_chunked = te && header_contains_token(te, te_len, "chunked");

        INT64_T     content_len = -1;
        size_t      cl_len      = 0;
        const char *cl          = find_header_value(hdr_buf, "Content-Length", &cl_len);
        if (cl && cl_len > 0) {
            char   tmp[32];
            size_t n = cl_len;
            if (n >= sizeof(tmp))
                n = sizeof(tmp) - 1;
            memcpy(tmp, cl, n);
            tmp[n]      = 0;
            content_len = (INT64_T)strtoll(tmp, NULL, 10);
        }

        const char *ct     = NULL;
        size_t      ct_len = 0;
        ct                 = find_header_value(hdr_buf, "Content-Type", &ct_len);
        *hdr_end           = saved;

        if (ct && ct_len > 0 && header_contains_token(ct, ct_len, "application/json")) {
            uint8_t        json_buf[512];
            StreamReader_t sr = {
                .transport = &transport,
                .init      = body_start,
                .init_len  = body_len,
                .init_pos  = 0,
                .len       = 0,
                .pos       = 0,
            };
            int32_t r = stream_read(&sr, json_buf, sizeof(json_buf) - 1);
            if (r > 0) {
                json_buf[r] = 0;
                cJSON *jr   = cJSON_Parse((char *)json_buf);
                if (jr) {
                    bdndk_capture_error_code_json(jr, "download");
                    cJSON_Delete(jr);
                }
            }
            tal_free(hdr_buf);
            tuya_transporter_close(network);
            tuya_transporter_destroy(network);
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_COM_ERROR;
        }

        if (status_code != 200 && status_code != 206) {
            PR_ERR("Download http status: %d", status_code);
            tal_free(hdr_buf);
            tuya_transporter_close(network);
            tuya_transporter_destroy(network);
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_COM_ERROR;
        }

        TUYA_FILE f = tkl_fopen(save_path, "w");
        if (!f) {
            tal_free(hdr_buf);
            tuya_transporter_close(network);
            tuya_transporter_destroy(network);
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_COM_ERROR;
        }

        INT64_T written = 0;
        if (is_chunked) {
            OPERATE_RET rt = recv_chunked_to_file(&transport, body_start, body_len, f, &written, file_size);
            tkl_fclose(f);
            tal_free(hdr_buf);
            tuya_transporter_close(network);
            tuya_transporter_destroy(network);
            if (rt != OPRT_OK) {
                tal_free(cur_url);
                tal_free(path);
                tal_free(next_url);
                return rt;
            }
        } else {
            uint8_t buf[8 * 1024];
            size_t  first = body_len;
            if (content_len >= 0 && (INT64_T)first > content_len)
                first = (size_t)content_len;
            if (first > 0) {
                tkl_fwrite((void *)body_start, first, f);
                written += (INT64_T)first;
                update_progress(written, file_size);
            }

            if (content_len >= 0) {
                INT64_T remain = content_len - written;
                while (remain > 0) {
                    size_t  want = (remain > (INT64_T)sizeof(buf)) ? sizeof(buf) : (size_t)remain;
                    int32_t r    = transport.recv(transport.pNetworkContext, buf, want);
                    if (r <= 0)
                        break;
                    tkl_fwrite(buf, r, f);
                    written += r;
                    remain -= r;
                    update_progress(written, file_size);
                }
            } else {
                for (;;) {
                    int32_t r = transport.recv(transport.pNetworkContext, buf, sizeof(buf));
                    if (r <= 0)
                        break;
                    tkl_fwrite(buf, r, f);
                    written += r;
                    update_progress(written, file_size);
                }
            }

            tkl_fclose(f);
            tal_free(hdr_buf);
            tuya_transporter_close(network);
            tuya_transporter_destroy(network);
        }

        if (file_size > 0 && written >= file_size) {
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_OK;
        }
        if (file_size <= 0 && written > 0) {
            tal_free(cur_url);
            tal_free(path);
            tal_free(next_url);
            return OPRT_OK;
        }
        tal_free(cur_url);
        tal_free(path);
        tal_free(next_url);
        return OPRT_COM_ERROR;
    }
    tal_free(cur_url);
    tal_free(path);
    tal_free(next_url);
    return OPRT_COM_ERROR;
}

static OPERATE_RET download_by_fsid(const bdndk_token_t *t, const BDNDK_FILE_T *fi, const char *save_dir)
{
    if (!t || !fi || !save_dir)
        return OPRT_INVALID_PARM;
    bdndk_clear_last_error();
    sg_last_download_url[0] = 0;

    size_t meta_cap  = 1400;
    char  *path_meta = (char *)tal_malloc(meta_cap);
    if (!path_meta)
        return OPRT_MALLOC_FAILED;
    snprintf(path_meta, meta_cap, "/rest/2.0/xpan/multimedia?method=filemetas&access_token=%s&fsids=[%s]&dlink=1",
             t->access_token, fi->fsid);

    char       *body = NULL;
    OPERATE_RET rt   = https_get_json("pan.baidu.com", path_meta, "pan.baidu.com", &body);
    tal_free(path_meta);
    if (rt != OPRT_OK)
        return rt;
    fix_json_escape_chars(body);
    bdndk_log_long_text("FILEMETAS_RESP", body);
    cJSON *root = cJSON_Parse(body);
    tal_free(body);
    if (!root)
        return OPRT_CJSON_PARSE_ERR;

    if (bdndk_capture_errno_json(root, "filemetas") != 0) {
        cJSON_Delete(root);
        return OPRT_COM_ERROR;
    }
    cJSON *list  = cJSON_GetObjectItem(root, "list");
    cJSON *item0 = (cJSON_IsArray(list) && cJSON_GetArraySize(list) > 0) ? cJSON_GetArrayItem(list, 0) : NULL;
    cJSON *dlink = item0 ? cJSON_GetObjectItem(item0, "dlink") : NULL;
    if (!cJSON_IsString(dlink)) {
        cJSON_Delete(root);
        return OPRT_COM_ERROR;
    }

    char *dlink_clean  = (char *)tal_malloc(4096);
    char *download_url = (char *)tal_malloc(4096);
    if (!dlink_clean || !download_url) {
        if (dlink_clean)
            tal_free(dlink_clean);
        if (download_url)
            tal_free(download_url);
        cJSON_Delete(root);
        return OPRT_MALLOC_FAILED;
    }
    trim_url_copy(dlink_clean, 4096, dlink->valuestring);
    url_unescape_json_inplace(dlink_clean);
    if (dlink_clean[0] == 0) {
        tal_free(dlink_clean);
        tal_free(download_url);
        cJSON_Delete(root);
        return OPRT_COM_ERROR;
    }

    if (strchr(dlink_clean, '?')) {
        snprintf(download_url, 4096, "%s&access_token=%s", dlink_clean, t->access_token);
    } else {
        snprintf(download_url, 4096, "%s?access_token=%s", dlink_clean, t->access_token);
    }
    strncpy(sg_last_download_url, download_url, sizeof(sg_last_download_url) - 1);
    sg_last_download_url[sizeof(sg_last_download_url) - 1] = 0;
    bdndk_log_long_text("DLINK", sg_last_download_url);
    cJSON_Delete(root);

    char save_path[512];
    snprintf(save_path, sizeof(save_path), "%s/%s", save_dir, fi->name);
    sg_progress_percent = 0;
    rt                  = http_get_stream_to_file(download_url, "pan.baidu.com", save_path, fi->size);
    tal_free(dlink_clean);
    tal_free(download_url);
    return rt;
}

static OPERATE_RET ensure_net_up(void)
{
    netmgr_status_e st = NETMGR_LINK_DOWN;
    if (netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_STATUS, &st) == OPRT_OK && st == NETMGR_LINK_UP) {
        return OPRT_OK;
    }
    st = NETMGR_LINK_DOWN;
    if (netmgr_conn_get(NETCONN_WIRED, NETCONN_CMD_STATUS, &st) == OPRT_OK && st == NETMGR_LINK_UP) {
        return OPRT_OK;
    }
    return OPRT_COM_ERROR;
}

static void worker_main(void *arg)
{
    (void)arg;
    sg_need_refresh           = TRUE;
    sg_work                   = BDNDK_WORK_IDLE;
    sg_view                   = BDNDK_VIEW_AUTH;
    sg_progress_percent       = -1;
    sg_pending_download_index = -1;
    sg_pending_detail_index   = -1;
    sg_selected_index         = -1;
    memset(&sg_auth, 0, sizeof(sg_auth));
    memset(&sg_token, 0, sizeof(sg_token));
    sg_list_count = 0;
    memset(&sg_detail, 0, sizeof(sg_detail));
    memset(&sg_detail_meta, 0, sizeof(sg_detail_meta));
    sg_last_download_url[0] = 0;
    set_msg("");

    tuya_register_center_init();

    if (ensure_net_up() != OPRT_OK) {
        sg_work = BDNDK_WORK_ERR;
        set_msg("Network down");
        while (sg_running) {
            tal_system_sleep(300);
        }
        tal_thread_delete(sg_worker_thrd);
        return;
    }

    bdndk_token_t t;
    memset(&t, 0, sizeof(t));
    if (sg_storage_ready) {
        if (token_load(&t) == OPRT_OK) {
            if (!token_is_valid(&t)) {
                bdndk_token_t nt;
                if (refresh_access_token(&t, &nt) == OPRT_OK) {
                    memcpy(&t, &nt, sizeof(t));
                    token_save(&t);
                }
            }
        }
    }

    if (!token_is_valid(&t)) {
        sg_work         = BDNDK_WORK_AUTHING;
        sg_view         = BDNDK_VIEW_AUTH;
        sg_need_refresh = TRUE;

        if (BDNDK_APP_KEY[0] == 0 || BDNDK_APP_SECRET[0] == 0) {
            sg_work = BDNDK_WORK_ERR;
            set_msg("Set APP_KEY/SECRET");
            while (sg_running) {
                tal_system_sleep(300);
            }
            tal_thread_delete(sg_worker_thrd);
            return;
        }

        bdndk_device_auth_t auth;
        if (get_device_auth(&auth) != OPRT_OK) {
            sg_work = BDNDK_WORK_ERR;
            set_msg("Get device code fail");
            while (sg_running) {
                tal_system_sleep(300);
            }
            tal_thread_delete(sg_worker_thrd);
            return;
        }
        memcpy(&sg_auth, &auth, sizeof(auth));
        sg_need_refresh = TRUE;
        set_msg("Scan QR / open URL");

        bdndk_token_t new_t;
        OPERATE_RET   art = poll_access_token(&auth, &new_t);
        if (art != OPRT_OK) {
            sg_work = BDNDK_WORK_ERR;
            set_msg("Authorize timeout");
            while (sg_running) {
                tal_system_sleep(300);
            }
            tal_thread_delete(sg_worker_thrd);
            return;
        }
        memcpy(&t, &new_t, sizeof(t));
        if (sg_storage_ready) {
            token_save(&t);
        }
    }

    memcpy(&sg_token, &t, sizeof(t));

    sg_work         = BDNDK_WORK_LISTING;
    sg_view         = BDNDK_VIEW_LIST;
    sg_need_refresh = TRUE;
    set_msg("Loading list...");
    if (get_baidu_list(&t) != OPRT_OK) {
        sg_work = BDNDK_WORK_ERR;
        char em[128];
        if (sg_last_errno != 0) {
            bdndk_format_last_error_msg(em, sizeof(em), "List failed");
            set_msg(em);
        } else {
            set_msg("List failed");
        }
    } else {
        sg_work = BDNDK_WORK_OK;
        set_msg("");
    }

    while (sg_running) {
        int didx = sg_pending_detail_index;
        if (didx >= 0 && didx < sg_list_count) {
            sg_pending_detail_index = -1;
            sg_work                 = BDNDK_WORK_LISTING;
            sg_view                 = BDNDK_VIEW_MSG;
            sg_progress_percent     = -1;
            set_msg("Loading detail...");

            bdndk_clear_last_error();
            memset(&sg_detail_meta, 0, sizeof(sg_detail_meta));
            sg_selected_index = didx;
            memcpy(&sg_detail, &sg_list[didx], sizeof(sg_detail));

            OPERATE_RET drt = OPRT_OK;
            if (!sg_list[didx].is_dir) {
                drt = fetch_detail_by_fsid(&t, &sg_list[didx], &sg_detail_meta);
            }
            if (drt == OPRT_OK) {
                sg_work = BDNDK_WORK_OK;
                sg_view = BDNDK_VIEW_DETAIL;
                set_msg("");
            } else {
                sg_work = BDNDK_WORK_ERR;
                char em[128];
                if (sg_last_errno != 0) {
                    bdndk_format_last_error_msg(em, sizeof(em), "Detail failed");
                } else {
                    snprintf(em, sizeof(em), "Detail failed (%d)", drt);
                }
                set_msg(em);
                sg_view = BDNDK_VIEW_MSG;
            }
            sg_need_refresh = TRUE;
            tal_system_sleep(100);
            continue;
        }

        int idx = sg_pending_download_index;
        if (idx >= 0 && idx < sg_list_count) {
            sg_pending_download_index = -1;
            sg_work                   = BDNDK_WORK_DOWNLOADING;
            sg_view                   = BDNDK_VIEW_MSG;
            sg_progress_percent       = 0;
            set_msg("Downloading...");

            if (!sg_storage_ready) {
                sg_work             = BDNDK_WORK_ERR;
                sg_progress_percent = -1;
                set_msg("SD not mounted");
                continue;
            }

            BOOL_T exist = FALSE;
            if (tkl_fs_is_exist(sg_save_dir, &exist) != 0 || !exist) {
                tkl_fs_mkdir(sg_save_dir);
            }

            OPERATE_RET drt = download_by_fsid(&t, &sg_list[idx], sg_save_dir);
            if (drt == OPRT_OK) {
                sg_work             = BDNDK_WORK_OK;
                sg_progress_percent = 100;
                set_msg("Download success");
            } else {
                sg_work             = BDNDK_WORK_ERR;
                sg_progress_percent = -1;
                char em[128];
                if (sg_last_errno != 0) {
                    bdndk_format_last_error_msg(em, sizeof(em), "Download failed");
                } else {
                    snprintf(em, sizeof(em), "Download failed (%d)", drt);
                }
                set_msg(em);
            }
            sg_need_refresh = TRUE;
        }
        tal_system_sleep(200);
    }
    tal_thread_delete(sg_worker_thrd);
}

OPERATE_RET bdndk_start(void)
{
    if (sg_running)
        return OPRT_OK;
    sg_running          = TRUE;
    sg_need_refresh     = TRUE;
    sg_view             = BDNDK_VIEW_AUTH;
    sg_work             = BDNDK_WORK_IDLE;
    sg_progress_percent = -1;
    sg_last_msg[0]      = 0;
    THREAD_CFG_T cfg    = {.priority = THREAD_PRIO_3, .stackDepth = 24 * 1024, .thrdname = "bdndk"};
    if (tal_thread_create_and_start(&sg_worker_thrd, NULL, NULL, worker_main, NULL, &cfg) != OPRT_OK) {
        sg_running = FALSE;
        return OPRT_COM_ERROR;
    }
    return OPRT_OK;
}

void bdndk_stop(void)
{
    sg_running = FALSE;
}

void bdndk_storage_set(BOOL_T ready, const char *save_dir)
{
    sg_storage_ready = ready;
    if (save_dir && save_dir[0]) {
        strncpy(sg_save_dir, save_dir, sizeof(sg_save_dir) - 1);
        sg_save_dir[sizeof(sg_save_dir) - 1] = 0;
    }
    sg_need_refresh = TRUE;
}
