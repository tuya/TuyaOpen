#include "channels/telegram_bot.h"

#include "bus/message_bus.h"
#include "cJSON.h"
#include "http_client_interface.h"
#include "im_config.h"
#include "proxy/http_proxy.h"
#include "certs/tls_cert_bundle.h"
#include "im_utils.h"

#include <inttypes.h>

static const char   *TAG                   = "telegram";
static char          s_bot_token[128]      = {0};
static int64_t       s_update_offset       = 0;
static int64_t       s_last_saved_offset   = -1;
static uint32_t      s_last_offset_save_ms = 0;
static THREAD_HANDLE s_poll_thread         = NULL;
static uint8_t      *s_tg_cacert           = NULL;
static size_t        s_tg_cacert_len       = 0;
static bool          s_tg_tls_no_verify    = false;

#define TG_HOST                     IM_TG_API_HOST
#define TG_HTTP_TIMEOUT_MS          ((IM_TG_POLL_TIMEOUT_S + 5) * 1000)
#define TG_HTTP_RESP_BUF_SIZE       (16 * 1024)
#define TG_PROXY_READ_SLICE_MS      1000
#define TG_PROXY_READ_TOTAL_MS      ((IM_TG_POLL_TIMEOUT_S + 20) * 1000)
#define TG_PROXY_LONGPOLL_TIMEOUT_S 20
#define TG_OFFSET_NVS_KEY           "update_offset"
#define TG_DEDUP_CACHE_SIZE         64
#define TG_OFFSET_SAVE_INTERVAL_MS  (5 * 1000)
#define TG_OFFSET_SAVE_STEP         10

static uint64_t s_seen_msg_keys[TG_DEDUP_CACHE_SIZE] = {0};
static size_t   s_seen_msg_idx                       = 0;

static uint64_t make_msg_key(const char *chat_id, int msg_id)
{
    uint64_t h = im_fnv1a64(chat_id);
    return (h << 16) ^ (uint64_t)(msg_id & 0xFFFF) ^ ((uint64_t)msg_id << 32);
}

static bool seen_msg_contains(uint64_t key)
{
    for (size_t i = 0; i < TG_DEDUP_CACHE_SIZE; i++) {
        if (s_seen_msg_keys[i] == key) {
            return true;
        }
    }
    return false;
}

static void seen_msg_insert(uint64_t key)
{
    s_seen_msg_keys[s_seen_msg_idx] = key;
    s_seen_msg_idx                  = (s_seen_msg_idx + 1) % TG_DEDUP_CACHE_SIZE;
}

static void save_update_offset_if_needed(bool force)
{
    if (s_update_offset <= 0) {
        return;
    }

    uint32_t now_ms      = tal_system_get_millisecond();
    bool     should_save = force;
    if (!should_save && s_last_saved_offset >= 0) {
        if ((s_update_offset - s_last_saved_offset) >= TG_OFFSET_SAVE_STEP) {
            should_save = true;
        } else if ((int)(now_ms - s_last_offset_save_ms) >= TG_OFFSET_SAVE_INTERVAL_MS) {
            should_save = true;
        }
    } else if (!should_save) {
        should_save = true;
    }

    if (!should_save) {
        return;
    }

    char offset_buf[24] = {0};
    snprintf(offset_buf, sizeof(offset_buf), "%lld", (long long)s_update_offset);
    if (im_kv_set_string(IM_NVS_TG, TG_OFFSET_NVS_KEY, offset_buf) == OPRT_OK) {
        s_last_saved_offset   = s_update_offset;
        s_last_offset_save_ms = now_ms;
    }
}

static OPERATE_RET ensure_tg_cert(void)
{
    if (s_tg_cacert && s_tg_cacert_len > 0) {
        s_tg_tls_no_verify = false;
        return OPRT_OK;
    }

    OPERATE_RET rt = im_tls_query_domain_certs(TG_HOST, &s_tg_cacert, &s_tg_cacert_len);
    if (rt != OPRT_OK || !s_tg_cacert || s_tg_cacert_len == 0) {
        if (s_tg_cacert) {
            im_free(s_tg_cacert);
        }
        s_tg_cacert        = NULL;
        s_tg_cacert_len    = 0;
        s_tg_tls_no_verify = true;
        IM_LOGD(TAG, "cert unavailable for %s, fallback to TLS no-verify mode", TG_HOST);
        return OPRT_OK;
    }

    s_tg_tls_no_verify = false;
    return OPRT_OK;
}

static OPERATE_RET tg_http_call_via_proxy(const char *path, const char *post_data, char *resp_buf, size_t resp_buf_size,
                                          uint16_t *status_code)
{
    proxy_conn_t *conn = proxy_conn_open(TG_HOST, 443, TG_HTTP_TIMEOUT_MS);
    if (!conn) {
        IM_LOGE(TAG, "proxy open failed host=%s", TG_HOST);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    int  body_len        = post_data ? (int)strlen(post_data) : 0;
    char req_header[768] = {0};
    int  req_len         = 0;
    if (post_data) {
        req_len = snprintf(req_header, sizeof(req_header),
                           "POST %s HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n\r\n",
                           path, TG_HOST, body_len);
    } else {
        req_len = snprintf(req_header, sizeof(req_header),
                           "GET %s HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Connection: close\r\n\r\n",
                           path, TG_HOST);
    }
    if (req_len <= 0 || req_len >= (int)sizeof(req_header)) {
        proxy_conn_close(conn);
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    if (proxy_conn_write(conn, req_header, req_len) != req_len) {
        proxy_conn_close(conn);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }
    if (body_len > 0 && proxy_conn_write(conn, post_data, body_len) != body_len) {
        proxy_conn_close(conn);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    size_t raw_cap = 4096;
    size_t raw_len = 0;
    char  *raw     = im_calloc(1, raw_cap);
    if (!raw) {
        proxy_conn_close(conn);
        return OPRT_MALLOC_FAILED;
    }

    uint32_t wait_begin_ms = tal_system_get_millisecond();
    while (1) {
        if (raw_len + 1024 >= raw_cap) {
            size_t new_cap = raw_cap * 2;
            char  *tmp     = im_realloc(raw, new_cap);
            if (!tmp) {
                im_free(raw);
                proxy_conn_close(conn);
                return OPRT_MALLOC_FAILED;
            }
            raw     = tmp;
            raw_cap = new_cap;
        }

        int n = proxy_conn_read(conn, raw + raw_len, (int)(raw_cap - raw_len - 1), TG_PROXY_READ_SLICE_MS);
        if (n == OPRT_RESOURCE_NOT_READY) {
            if ((int)(tal_system_get_millisecond() - wait_begin_ms) >= TG_PROXY_READ_TOTAL_MS) {
                im_free(raw);
                proxy_conn_close(conn);
                return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
            }
            continue;
        }
        if (n < 0) {
            if (raw_len > 0) {
                IM_LOGW(TAG, "proxy read closed with rt=%d, parse partial response len=%u", n, (unsigned)raw_len);
                break;
            }
            im_free(raw);
            proxy_conn_close(conn);
            return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
        }
        if (n == 0) {
            break;
        }

        raw_len += (size_t)n;
        raw[raw_len]  = '\0';
        wait_begin_ms = tal_system_get_millisecond();
    }
    proxy_conn_close(conn);

    if (raw_len == 0) {
        im_free(raw);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    if (status_code) {
        *status_code = im_parse_http_status(raw);
    }

    resp_buf[0] = '\0';
    char *body  = strstr(raw, "\r\n\r\n");
    if (body) {
        body += 4;
        size_t body_len_sz = strlen(body);
        size_t copy        = (body_len_sz < resp_buf_size - 1) ? body_len_sz : (resp_buf_size - 1);
        memcpy(resp_buf, body, copy);
        resp_buf[copy] = '\0';
    }
    im_free(raw);

    return OPRT_OK;
}

static OPERATE_RET tg_http_call_direct(const char *path, const char *post_data, char *resp_buf, size_t resp_buf_size,
                                       uint16_t *status_code)
{
    OPERATE_RET rt = ensure_tg_cert();
    if (rt != OPRT_OK) {
        return rt;
    }

    http_client_header_t headers[1]   = {0};
    uint8_t              header_count = 0;
    if (post_data) {
        headers[header_count++] = (http_client_header_t){
            .key   = "Content-Type",
            .value = "application/json",
        };
    }

    http_client_response_t response = {0};
    http_client_status_t   http_rt  = http_client_request(
        &(const http_client_request_t){
               .cacert        = s_tg_cacert,
               .cacert_len    = s_tg_cacert_len,
               .tls_no_verify = s_tg_tls_no_verify,
               .host          = TG_HOST,
               .port          = 443,
               .method        = post_data ? "POST" : "GET",
               .path          = path,
               .headers       = headers,
               .headers_count = header_count,
               .body          = (const uint8_t *)(post_data ? post_data : ""),
               .body_length   = post_data ? strlen(post_data) : 0,
               .timeout_ms    = TG_HTTP_TIMEOUT_MS,
        },
        &response);
    if (http_rt != HTTP_CLIENT_SUCCESS) {
        IM_LOGE(TAG, "http request failed: %d", http_rt);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    if (status_code) {
        *status_code = response.status_code;
    }

    resp_buf[0] = '\0';
    if (response.body && response.body_length > 0) {
        size_t copy = (response.body_length < resp_buf_size - 1) ? response.body_length : (resp_buf_size - 1);
        memcpy(resp_buf, response.body, copy);
        resp_buf[copy] = '\0';
    }

    http_client_free(&response);
    return OPRT_OK;
}

static OPERATE_RET tg_http_call(const char *path, const char *post_data, char *resp_buf, size_t resp_buf_size,
                                uint16_t *status_code)
{
    if (!path || !resp_buf || resp_buf_size == 0) {
        return OPRT_INVALID_PARM;
    }

    if (http_proxy_is_enabled()) {
        return tg_http_call_via_proxy(path, post_data, resp_buf, resp_buf_size, status_code);
    }

    return tg_http_call_direct(path, post_data, resp_buf, resp_buf_size, status_code);
}

static bool tg_response_is_ok(const char *json_str, const char **out_desc)
{
    if (out_desc) {
        *out_desc = NULL;
    }
    if (!json_str || json_str[0] == '\0') {
        return false;
    }

    bool   ok   = false;
    cJSON *root = cJSON_Parse(json_str);
    if (root) {
        cJSON *ok_field = cJSON_GetObjectItem(root, "ok");
        ok              = cJSON_IsTrue(ok_field);
        if (!ok && out_desc) {
            cJSON *desc = cJSON_GetObjectItem(root, "description");
            if (cJSON_IsString(desc) && desc->valuestring) {
                *out_desc = desc->valuestring;
            }
        }
        cJSON_Delete(root);
        return ok;
    }

    if (strstr(json_str, "\"ok\":true") != NULL) {
        return true;
    }

    return false;
}

static void process_updates(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return;
    }

    cJSON *ok     = cJSON_GetObjectItem(root, "ok");
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!cJSON_IsTrue(ok) || !cJSON_IsArray(result)) {
        cJSON_Delete(root);
        return;
    }

    cJSON *update = NULL;
    cJSON_ArrayForEach(update, result)
    {
        int64_t uid       = -1;
        cJSON  *update_id = cJSON_GetObjectItem(update, "update_id");
        if (cJSON_IsNumber(update_id)) {
            uid = (int64_t)update_id->valuedouble;
        }
        if (uid >= 0) {
            if (uid < s_update_offset) {
                continue;
            }
            s_update_offset = uid + 1;
            save_update_offset_if_needed(false);
        }

        cJSON *message = cJSON_GetObjectItem(update, "message");
        cJSON *chat    = message ? cJSON_GetObjectItem(message, "chat") : NULL;
        cJSON *chat_id = chat ? cJSON_GetObjectItem(chat, "id") : NULL;
        if (!message || !chat_id) {
            continue;
        }

        char chat_id_str[32] = {0};
        if (cJSON_IsString(chat_id) && chat_id->valuestring) {
            im_safe_copy(chat_id_str, sizeof(chat_id_str), chat_id->valuestring);
        } else if (cJSON_IsNumber(chat_id)) {
            snprintf(chat_id_str, sizeof(chat_id_str), "%.0f", chat_id->valuedouble);
        } else {
            continue;
        }

        int    msg_id_val = -1;
        cJSON *message_id = cJSON_GetObjectItem(message, "message_id");
        if (cJSON_IsNumber(message_id)) {
            msg_id_val = (int)message_id->valuedouble;
        }
        if (msg_id_val >= 0) {
            uint64_t msg_key = make_msg_key(chat_id_str, msg_id_val);
            if (seen_msg_contains(msg_key)) {
                IM_LOGW(TAG, "drop duplicate update_id=%" PRId64 " chat=%s message_id=%d", uid, chat_id_str,
                          msg_id_val);
                continue;
            }
            seen_msg_insert(msg_key);
        }

        cJSON *text = cJSON_GetObjectItem(message, "text");
        if (cJSON_IsString(text) && text->valuestring) {
            IM_LOGI(TAG, "rx inbound_text channel=%s chat=%s len=%u", IM_CHAN_TELEGRAM, chat_id_str,
                      (unsigned)strlen(text->valuestring));
        }

        cJSON *document = cJSON_GetObjectItem(message, "document");
        if (cJSON_IsObject(document)) {
            const char *file_name = im_json_str(document, "file_name", "<empty>");
            const char *mime_type = im_json_str(document, "mime_type", "<empty>");
            uint32_t    file_size = im_json_uint(document, "file_size", 0);
            IM_LOGI(TAG, "rx document chat=%s name=%s mime=%s size=%u", chat_id_str, file_name, mime_type,
                      (unsigned)file_size);
        }

        if (!cJSON_IsString(text) || !text->valuestring) {
            continue;
        }

        im_msg_t msg = {0};
        strncpy(msg.channel, IM_CHAN_TELEGRAM, sizeof(msg.channel) - 1);
        strncpy(msg.chat_id, chat_id_str, sizeof(msg.chat_id) - 1);
        msg.content = im_strdup(text->valuestring);
        if (!msg.content) {
            continue;
        }

        OPERATE_RET rt = message_bus_push_inbound(&msg);
        if (rt != OPRT_OK) {
            IM_LOGW(TAG, "push inbound failed rt=%d", rt);
            im_free(msg.content);
        }
    }

    save_update_offset_if_needed(false);
    cJSON_Delete(root);
}

static void telegram_poll_task(void *arg)
{
    (void)arg;
    uint32_t fail_delay_ms = IM_TG_FAIL_BASE_MS;
    char    *resp          = im_malloc(TG_HTTP_RESP_BUF_SIZE);
    if (!resp) {
        IM_LOGE(TAG, "alloc telegram poll resp buffer failed");
        return;
    }

    IM_LOGI(TAG, "telegram poll task started host=%s", TG_HOST);

    while (1) {
        if (s_bot_token[0] == '\0') {
            fail_delay_ms = IM_TG_FAIL_BASE_MS;
            tal_system_sleep(3000);
            continue;
        }

        int poll_timeout_s = IM_TG_POLL_TIMEOUT_S;
        if (http_proxy_is_enabled() && poll_timeout_s > TG_PROXY_LONGPOLL_TIMEOUT_S) {
            poll_timeout_s = TG_PROXY_LONGPOLL_TIMEOUT_S;
        }

        char path[320] = {0};
        int  n         = snprintf(path, sizeof(path), "/bot%s/getUpdates?offset=%lld&timeout=%d", s_bot_token,
                                  (long long)s_update_offset, poll_timeout_s);
        if (n <= 0 || (size_t)n >= sizeof(path)) {
            IM_LOGE(TAG, "getUpdates path too long");
            fail_delay_ms = IM_TG_FAIL_BASE_MS;
            tal_system_sleep(3000);
            continue;
        }

        memset(resp, 0, TG_HTTP_RESP_BUF_SIZE);
        uint16_t    status = 0;
        OPERATE_RET rt     = tg_http_call(path, NULL, resp, TG_HTTP_RESP_BUF_SIZE, &status);
        if (rt != OPRT_OK || status != 200) {
            IM_LOGD(TAG, "getUpdates failed rt=%d http=%u retry_in_ms=%u", rt, status, fail_delay_ms);
            tal_system_sleep(fail_delay_ms);
            if (fail_delay_ms < IM_TG_FAIL_MAX_MS) {
                uint32_t next_delay = fail_delay_ms << 1;
                fail_delay_ms       = (next_delay > IM_TG_FAIL_MAX_MS) ? IM_TG_FAIL_MAX_MS : next_delay;
            }
            continue;
        }

        fail_delay_ms = IM_TG_FAIL_BASE_MS;
        process_updates(resp);
    }

    im_free(resp);
}

OPERATE_RET telegram_bot_init(void)
{
    if (IM_SECRET_TG_TOKEN[0] != '\0') {
        im_safe_copy(s_bot_token, sizeof(s_bot_token), IM_SECRET_TG_TOKEN);
    }

    char tmp[128] = {0};
    if (im_kv_get_string(IM_NVS_TG, IM_NVS_KEY_TG_TOKEN, tmp, sizeof(tmp)) == OPRT_OK) {
        im_safe_copy(s_bot_token, sizeof(s_bot_token), tmp);
    }

    memset(tmp, 0, sizeof(tmp));
    if (im_kv_get_string(IM_NVS_TG, TG_OFFSET_NVS_KEY, tmp, sizeof(tmp)) == OPRT_OK && tmp[0] != '\0') {
        long long offset = strtoll(tmp, NULL, 10);
        if (offset > 0) {
            s_update_offset     = offset;
            s_last_saved_offset = offset;
            IM_LOGI(TAG, "loaded telegram update offset: %lld", offset);
        }
    }

    IM_LOGI(TAG, "telegram init credential=%s", s_bot_token[0] ? "configured" : "empty");
    return OPRT_OK;
}

OPERATE_RET telegram_bot_start(void)
{
    if (s_bot_token[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    if (s_poll_thread) {
        return OPRT_OK;
    }

    THREAD_CFG_T cfg = {0};
    cfg.stackDepth   = IM_TG_POLL_STACK;
    cfg.priority     = THREAD_PRIO_1;
    cfg.thrdname     = "im_tg_poll";

    OPERATE_RET rt = tal_thread_create_and_start(&s_poll_thread, NULL, NULL, telegram_poll_task, NULL, &cfg);
    if (rt != OPRT_OK) {
        IM_LOGE(TAG, "create poll thread failed: %d", rt);
        return rt;
    }

    return OPRT_OK;
}

OPERATE_RET telegram_send_message(const char *chat_id, const char *text)
{
    if (!chat_id || !text) {
        return OPRT_INVALID_PARM;
    }

    if (s_bot_token[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    size_t text_len = strlen(text);
    size_t offset   = 0;
    bool   all_ok   = true;

    while (offset < text_len || (text_len == 0 && offset == 0)) {
        size_t chunk = text_len - offset;
        if (chunk > IM_TG_MAX_MSG_LEN) {
            chunk = IM_TG_MAX_MSG_LEN;
        }
        if (text_len == 0) {
            chunk = 0;
        }

        char *segment = im_calloc(1, chunk + 1);
        if (!segment) {
            return OPRT_MALLOC_FAILED;
        }
        if (chunk > 0) {
            memcpy(segment, text + offset, chunk);
        }
        segment[chunk] = '\0';

        cJSON *body = cJSON_CreateObject();
        if (!body) {
            im_free(segment);
            return OPRT_MALLOC_FAILED;
        }
        cJSON_AddStringToObject(body, "chat_id", chat_id);
        cJSON_AddStringToObject(body, "text", segment);
        cJSON_AddStringToObject(body, "parse_mode", "Markdown");
        char *json = cJSON_PrintUnformatted(body);
        cJSON_Delete(body);

        char path[256] = {0};
        int  n         = snprintf(path, sizeof(path), "/bot%s/sendMessage", s_bot_token);
        if (n <= 0 || (size_t)n >= sizeof(path)) {
            im_free(segment);
            cJSON_free(json);
            return OPRT_BUFFER_NOT_ENOUGH;
        }

        char *resp = im_malloc(TG_HTTP_RESP_BUF_SIZE);
        if (!resp) {
            im_free(segment);
            cJSON_free(json);
            return OPRT_MALLOC_FAILED;
        }
        memset(resp, 0, TG_HTTP_RESP_BUF_SIZE);

        uint16_t    status          = 0;
        OPERATE_RET rt              = OPRT_MALLOC_FAILED;
        bool        sent_ok         = false;
        bool        markdown_failed = false;
        const char *desc            = NULL;

        if (json) {
            IM_LOGD(TAG, "send telegram chunk bytes=%u", (unsigned)chunk);
            rt = tg_http_call(path, json, resp, TG_HTTP_RESP_BUF_SIZE, &status);
            if (rt == OPRT_OK && status == 200) {
                sent_ok = tg_response_is_ok(resp, &desc);
                if (!sent_ok) {
                    markdown_failed = true;
                    IM_LOGI(TAG, "markdown rejected rt=%d status=%u", rt, status);
                }
            }
        }
        cJSON_free(json);

        if (!sent_ok) {
            cJSON *body2 = cJSON_CreateObject();
            if (!body2) {
                im_free(resp);
                im_free(segment);
                return OPRT_MALLOC_FAILED;
            }
            cJSON_AddStringToObject(body2, "chat_id", chat_id);
            cJSON_AddStringToObject(body2, "text", segment);
            char *json2 = cJSON_PrintUnformatted(body2);
            cJSON_Delete(body2);

            if (!json2) {
                im_free(resp);
                im_free(segment);
                return OPRT_MALLOC_FAILED;
            }

            memset(resp, 0, TG_HTTP_RESP_BUF_SIZE);
            status = 0;
            desc   = NULL;
            rt     = tg_http_call(path, json2, resp, TG_HTTP_RESP_BUF_SIZE, &status);
            cJSON_free(json2);
            if (rt == OPRT_OK && status == 200) {
                sent_ok = tg_response_is_ok(resp, &desc);
            }
            if (!sent_ok) {
                IM_LOGE(TAG, "plain send failed rt=%d status=%u", rt, status);
                all_ok = false;
            } else if (markdown_failed) {
                IM_LOGI(TAG, "plain-text fallback succeeded");
            }
        }

        if (sent_ok) {
            IM_LOGD(TAG, "telegram send success bytes=%u", (unsigned)chunk);
        } else {
            all_ok = false;
        }

        im_free(resp);
        im_free(segment);
        if (text_len == 0) {
            break;
        }
        offset += chunk;
    }

    return all_ok ? OPRT_OK : OPRT_COM_ERROR;
}

OPERATE_RET telegram_set_token(const char *token)
{
    if (!token) {
        return OPRT_INVALID_PARM;
    }

    im_safe_copy(s_bot_token, sizeof(s_bot_token), token);
    s_update_offset       = 0;
    s_last_saved_offset   = -1;
    s_last_offset_save_ms = 0;
    (void)im_kv_del(IM_NVS_TG, TG_OFFSET_NVS_KEY);
    return im_kv_set_string(IM_NVS_TG, IM_NVS_KEY_TG_TOKEN, token);
}
