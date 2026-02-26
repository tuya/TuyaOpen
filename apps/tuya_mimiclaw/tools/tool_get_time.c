#include "tool_get_time.h"

#include "http_client_interface.h"
#include "mimi_config.h"
#include "proxy/http_proxy.h"
#include "tls_cert_bundle.h"

#include <ctype.h>
#include <stdio.h>

static const char *TAG = "tool_time";

#define TIME_SYNC_HOST       MIMI_TG_API_HOST
#define TIME_SYNC_PORT       443
#define TIME_SYNC_PATH       "/"
#define TIME_SYNC_TIMEOUT_MS (10 * 1000)

static uint8_t *s_time_cacert        = NULL;
static size_t   s_time_cacert_len    = 0;
static bool     s_time_tls_no_verify = false;
static bool     s_tz_inited          = false;

static const char *k_months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static const char *k_weekday[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
};

static char ascii_tolower(char c)
{
    return (char)tolower((unsigned char)c);
}

static bool parse_posix_tz_base_offset(const char *tz, int *utc_offset_sec)
{
    if (!tz || !tz[0] || !utc_offset_sec) {
        return false;
    }

    const char *p = tz;
    while (*p && ((ascii_tolower(*p) >= 'a') && (ascii_tolower(*p) <= 'z'))) {
        p++;
    }
    if (!*p) {
        return false;
    }

    int sign = 1;
    if (*p == '+' || *p == '-') {
        sign = (*p == '-') ? -1 : 1;
        p++;
    }
    if (!isdigit((unsigned char)*p)) {
        return false;
    }

    int hours = 0;
    while (isdigit((unsigned char)*p)) {
        hours = hours * 10 + (*p - '0');
        p++;
    }

    int mins = 0;
    if (*p == ':') {
        p++;
        if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) {
            return false;
        }
        mins = (p[0] - '0') * 10 + (p[1] - '0');
    }

    int posix_offset_sec = sign * (hours * 3600 + mins * 60);
    *utc_offset_sec      = -posix_offset_sec;
    return true;
}

static void ensure_timezone_from_config(void)
{
    if (s_tz_inited) {
        return;
    }

    int tz_sec = 0;
    if (parse_posix_tz_base_offset(MIMI_TIMEZONE, &tz_sec)) {
        OPERATE_RET rt = tal_time_set_time_zone_seconds(tz_sec);
        if (rt != OPRT_OK) {
            MIMI_LOGW(TAG, "set timezone from config failed rt=%d", rt);
        }
    }
    s_tz_inited = true;
}

static bool header_key_match(const char *line, size_t line_len, const char *key)
{
    size_t key_len = strlen(key);
    if (line_len < key_len + 1 || line[key_len] != ':') {
        return false;
    }

    for (size_t i = 0; i < key_len; i++) {
        if (ascii_tolower(line[i]) != ascii_tolower(key[i])) {
            return false;
        }
    }
    return true;
}

static bool find_header_value(const char *headers, size_t headers_len, const char *key, char *out, size_t out_size)
{
    if (!headers || !key || !out || out_size == 0) {
        return false;
    }

    out[0]     = '\0';
    size_t pos = 0;
    while (pos < headers_len) {
        size_t line_start = pos;
        while (pos < headers_len && headers[pos] != '\n') {
            pos++;
        }
        size_t line_end = pos;
        if (pos < headers_len && headers[pos] == '\n') {
            pos++;
        }

        while (line_start < line_end && (headers[line_start] == '\r' || headers[line_start] == '\n')) {
            line_start++;
        }
        while (line_end > line_start && (headers[line_end - 1] == '\r' || headers[line_end - 1] == '\n')) {
            line_end--;
        }
        if (line_end <= line_start) {
            continue;
        }

        const char *line     = headers + line_start;
        size_t      line_len = line_end - line_start;
        if (!header_key_match(line, line_len, key)) {
            continue;
        }

        size_t val_start = strlen(key) + 1;
        while (val_start < line_len && (line[val_start] == ' ' || line[val_start] == '\t')) {
            val_start++;
        }
        size_t val_end = line_len;
        while (val_end > val_start && (line[val_end - 1] == ' ' || line[val_end - 1] == '\t')) {
            val_end--;
        }
        if (val_end <= val_start) {
            return false;
        }

        size_t copy_len = val_end - val_start;
        if (copy_len >= out_size) {
            copy_len = out_size - 1;
        }
        memcpy(out, line + val_start, copy_len);
        out[copy_len] = '\0';
        return true;
    }

    return false;
}

static bool parse_http_date_to_epoch(const char *date_str, TIME_T *epoch_out)
{
    if (!date_str || !epoch_out) {
        return false;
    }

    int  day = 0, year = 0, hour = 0, min = 0, sec = 0;
    char mon[4] = {0};
    if (sscanf(date_str, "%*[^,], %d %3s %d %d:%d:%d", &day, mon, &year, &hour, &min, &sec) != 6) {
        return false;
    }

    int mon_idx = -1;
    for (int i = 0; i < 12; i++) {
        if (strcmp(mon, k_months[i]) == 0) {
            mon_idx = i;
            break;
        }
    }
    if (mon_idx < 0) {
        return false;
    }

    POSIX_TM_S utc_tm = {
        .tm_sec  = sec,
        .tm_min  = min,
        .tm_hour = hour,
        .tm_mday = day,
        .tm_mon  = mon_idx,
        .tm_year = year - 1900,
    };

    TIME_T epoch = tal_time_mktime(&utc_tm);
    if (epoch <= 0) {
        return false;
    }

    *epoch_out = epoch;
    return true;
}

static void format_utc_offset(int tz_sec, char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return;
    }

    int abs_sec = (tz_sec < 0) ? -tz_sec : tz_sec;
    int hh      = abs_sec / 3600;
    int mm      = (abs_sec % 3600) / 60;
    snprintf(buf, buf_size, "UTC%c%02d:%02d", (tz_sec >= 0) ? '+' : '-', hh, mm);
}

static void format_local_time(TIME_T epoch, char *out, size_t out_size)
{
    POSIX_TM_S tm_local = {0};
    if (tal_time_get_local_time_custom(epoch, &tm_local) != OPRT_OK) {
        if (!tal_time_gmtime_r(&epoch, &tm_local)) {
            snprintf(out, out_size, "time synced (epoch=%lld)", (long long)epoch);
            return;
        }
    }

    int  tz_sec       = 0;
    char tz_label[16] = "UTC";
    if (tal_time_get_time_zone_seconds(&tz_sec) == OPRT_OK) {
        format_utc_offset(tz_sec, tz_label, sizeof(tz_label));
    }

    const char *wday = "Unknown";
    if (tm_local.tm_wday >= 0 && tm_local.tm_wday <= 6) {
        wday = k_weekday[tm_local.tm_wday];
    }

    snprintf(out, out_size, "%04d-%02d-%02d %02d:%02d:%02d %s (%s)", tm_local.tm_year + 1900, tm_local.tm_mon + 1,
             tm_local.tm_mday, tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec, tz_label, wday);
}

static OPERATE_RET ensure_time_cert(void)
{
    if (s_time_cacert && s_time_cacert_len > 0) {
        s_time_tls_no_verify = false;
        return OPRT_OK;
    }

    OPERATE_RET rt = mimi_tls_query_domain_certs(TIME_SYNC_HOST, &s_time_cacert, &s_time_cacert_len);
    if (rt != OPRT_OK || !s_time_cacert || s_time_cacert_len == 0) {
        if (s_time_cacert) {
            tal_free(s_time_cacert);
        }
        s_time_cacert        = NULL;
        s_time_cacert_len    = 0;
        s_time_tls_no_verify = true;
        MIMI_LOGD(TAG, "cert unavailable for %s, fallback to TLS no-verify mode rt=%d", TIME_SYNC_HOST, rt);
        return OPRT_OK;
    }

    s_time_tls_no_verify = false;
    return OPRT_OK;
}

static OPERATE_RET fetch_date_direct(char *date_buf, size_t date_buf_size)
{
    if (!date_buf || date_buf_size == 0) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = ensure_time_cert();
    if (rt != OPRT_OK) {
        return rt;
    }

    http_client_header_t headers[] = {
        {.key = "Connection", .value = "close"},
    };

    OPERATE_RET last_rt   = OPRT_NOT_FOUND;
    const char *methods[] = {"GET", "HEAD"};
    for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]); i++) {
        const char *method = methods[i];

        http_client_response_t response = {0};
        http_client_status_t   http_rt  = http_client_request(
            &(const http_client_request_t){
                   .cacert        = s_time_cacert,
                   .cacert_len    = s_time_cacert_len,
                   .tls_no_verify = s_time_tls_no_verify,
                   .host          = TIME_SYNC_HOST,
                   .port          = TIME_SYNC_PORT,
                   .method        = method,
                   .path          = TIME_SYNC_PATH,
                   .headers       = headers,
                   .headers_count = (uint8_t)(sizeof(headers) / sizeof(headers[0])),
                   .body          = (const uint8_t *)"",
                   .body_length   = 0,
                   .timeout_ms    = TIME_SYNC_TIMEOUT_MS,
            },
            &response);

        if (http_rt != HTTP_CLIENT_SUCCESS) {
            MIMI_LOGW(TAG, "direct %s request failed: %d", method, http_rt);
            last_rt = OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
            continue;
        }

        bool ok = false;
        if (response.headers && response.headers_length > 0) {
            ok = find_header_value((const char *)response.headers, response.headers_length, "Date", date_buf,
                                   date_buf_size);
        }

        uint16_t status = response.status_code;
        http_client_free(&response);
        if (status < 200 || status >= 400) {
            MIMI_LOGW(TAG, "direct %s time sync http status=%u", method, status);
        }

        if (ok) {
            return OPRT_OK;
        }

        MIMI_LOGW(TAG, "direct %s response missing Date header", method);
        last_rt = OPRT_NOT_FOUND;
    }

    return last_rt;
}

static OPERATE_RET fetch_date_via_proxy(char *date_buf, size_t date_buf_size)
{
    if (!date_buf || date_buf_size == 0) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET last_rt   = OPRT_NOT_FOUND;
    const char *methods[] = {"GET", "HEAD"};
    for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]); i++) {
        const char   *method = methods[i];
        proxy_conn_t *conn   = proxy_conn_open(TIME_SYNC_HOST, TIME_SYNC_PORT, TIME_SYNC_TIMEOUT_MS);
        if (!conn) {
            last_rt = OPRT_LINK_CORE_NET_CONNECT_ERROR;
            continue;
        }

        char req[256] = {0};
        int  req_len  = snprintf(req, sizeof(req),
                                 "%s %s HTTP/1.1\r\n"
                                   "Host: " TIME_SYNC_HOST "\r\n"
                                   "Connection: close\r\n\r\n",
                                 method, TIME_SYNC_PATH);
        if (req_len <= 0 || req_len >= (int)sizeof(req)) {
            proxy_conn_close(conn);
            return OPRT_COM_ERROR;
        }

        if (proxy_conn_write(conn, req, req_len) < 0) {
            proxy_conn_close(conn);
            MIMI_LOGW(TAG, "proxy %s request write failed", method);
            last_rt = OPRT_LINK_CORE_NET_SOCKET_ERROR;
            continue;
        }

        char raw[1024] = {0};
        int  total     = 0;
        while (total < (int)sizeof(raw) - 1) {
            int n = proxy_conn_read(conn, raw + total, (int)sizeof(raw) - 1 - total, TIME_SYNC_TIMEOUT_MS);
            if (n == OPRT_RESOURCE_NOT_READY) {
                continue;
            }
            if (n <= 0) {
                break;
            }
            total += n;
            raw[total] = '\0';
            if (strstr(raw, "\r\n\r\n")) {
                break;
            }
        }
        proxy_conn_close(conn);

        if (total <= 0) {
            MIMI_LOGW(TAG, "proxy %s request got empty response", method);
            last_rt = OPRT_COM_ERROR;
            continue;
        }

        bool ok = find_header_value(raw, (size_t)total, "Date", date_buf, date_buf_size);
        if (ok) {
            return OPRT_OK;
        }

        MIMI_LOGW(TAG, "proxy %s response missing Date header", method);
        last_rt = OPRT_NOT_FOUND;
    }

    return last_rt;
}

OPERATE_RET tool_get_time_execute(const char *input_json, char *output, size_t output_size)
{
    (void)input_json;

    if (!output || output_size == 0) {
        return OPRT_INVALID_PARM;
    }

    output[0] = '\0';
    (void)tal_time_service_init();
    ensure_timezone_from_config();

    char        date_val[64] = {0};
    OPERATE_RET rt           = http_proxy_is_enabled() ? fetch_date_via_proxy(date_val, sizeof(date_val))
                                                       : fetch_date_direct(date_val, sizeof(date_val));
    if (rt != OPRT_OK) {
        snprintf(output, output_size, "Error: failed to fetch time (rt=%d)", rt);
        MIMI_LOGE(TAG, "%s", output);
        return rt;
    }

    TIME_T epoch = 0;
    if (!parse_http_date_to_epoch(date_val, &epoch)) {
        snprintf(output, output_size, "Error: failed to parse Date header: %s", date_val);
        MIMI_LOGE(TAG, "%s", output);
        return OPRT_INVALID_PARM;
    }

    rt = tal_time_set_posix(epoch, 2);
    if (rt != OPRT_OK) {
        snprintf(output, output_size, "Error: failed to set system time (rt=%d)", rt);
        MIMI_LOGE(TAG, "%s", output);
        return rt;
    }

    format_local_time(epoch, output, output_size);
    MIMI_LOGI(TAG, "time synced via %s, date=%s, local=%s", http_proxy_is_enabled() ? "proxy" : "direct", date_val,
              output);
    return OPRT_OK;
}
