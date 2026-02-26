#include "tool_web_search.h"

#include "cJSON.h"
#include "http_client_interface.h"
#include "mimi_config.h"
#include "tls_cert_bundle.h"

static char     s_search_key[128]      = {0};
static uint8_t *s_search_cacert        = NULL;
static size_t   s_search_cacert_len    = 0;
static bool     s_search_tls_no_verify = false;

static const char *TAG = "web_search";

#define SEARCH_HOST          "api.search.brave.com"
#define SEARCH_PATH          "/res/v1/web/search"
#define SEARCH_RESULT_COUNT  5
#define SEARCH_TIMEOUT_MS    (15 * 1000)
#define SEARCH_RESP_BUF_SIZE (24 * 1024)

static void safe_copy(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_size, "%s", src);
}

static OPERATE_RET ensure_search_cert(void)
{
    if (s_search_cacert && s_search_cacert_len > 0) {
        s_search_tls_no_verify = false;
        return OPRT_OK;
    }

    OPERATE_RET rt = mimi_tls_query_domain_certs(SEARCH_HOST, &s_search_cacert, &s_search_cacert_len);
    if (rt != OPRT_OK || !s_search_cacert || s_search_cacert_len == 0) {
        if (s_search_cacert) {
            tal_free(s_search_cacert);
        }
        s_search_cacert        = NULL;
        s_search_cacert_len    = 0;
        s_search_tls_no_verify = true;
        MIMI_LOGW(TAG, "cert unavailable for %s, fallback to TLS no-verify mode rt=%d", SEARCH_HOST, rt);
        return OPRT_OK;
    }

    s_search_tls_no_verify = false;
    return OPRT_OK;
}

static size_t url_encode(const char *src, char *dst, size_t dst_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t            pos   = 0;

    if (!src || !dst || dst_size == 0) {
        return 0;
    }

    for (; *src && pos + 1 < dst_size; src++) {
        unsigned char c = (unsigned char)*src;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            dst[pos++] = (char)c;
        } else if (c == ' ') {
            dst[pos++] = '+';
        } else {
            if (pos + 3 >= dst_size) {
                break;
            }
            dst[pos++] = '%';
            dst[pos++] = hex[c >> 4];
            dst[pos++] = hex[c & 0x0F];
        }
    }

    dst[pos] = '\0';
    return pos;
}

static void format_results(cJSON *root, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return;
    }
    output[0] = '\0';

    cJSON *web     = cJSON_GetObjectItem(root, "web");
    cJSON *results = web ? cJSON_GetObjectItem(web, "results") : NULL;
    if (!cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0) {
        snprintf(output, output_size, "No web results found.");
        return;
    }

    size_t off  = 0;
    int    idx  = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, results)
    {
        if (idx >= SEARCH_RESULT_COUNT || off >= output_size - 1) {
            break;
        }

        cJSON *title = cJSON_GetObjectItem(item, "title");
        cJSON *url   = cJSON_GetObjectItem(item, "url");
        cJSON *desc  = cJSON_GetObjectItem(item, "description");

        const char *title_s = cJSON_IsString(title) ? title->valuestring : "(no title)";
        const char *url_s   = cJSON_IsString(url) ? url->valuestring : "";
        const char *desc_s  = cJSON_IsString(desc) ? desc->valuestring : "";

        int n = snprintf(output + off, output_size - off, "%d. %s\n   %s\n   %s\n\n", idx + 1, title_s, url_s, desc_s);
        if (n <= 0) {
            break;
        }
        if ((size_t)n >= output_size - off) {
            off         = output_size - 1;
            output[off] = '\0';
            break;
        }

        off += (size_t)n;
        idx++;
    }

    if (idx == 0) {
        snprintf(output, output_size, "No web results found.");
    }
}

static OPERATE_RET search_http_call(const char *path, char *resp_buf, size_t resp_size, uint16_t *status_code)
{
    if (!path || !resp_buf || resp_size == 0) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = ensure_search_cert();
    if (rt != OPRT_OK) {
        return rt;
    }

    http_client_header_t headers[2]   = {0};
    uint8_t              header_count = 0;
    headers[header_count++]           = (http_client_header_t){
                  .key   = "Accept",
                  .value = "application/json",
    };
    headers[header_count++] = (http_client_header_t){
        .key   = "X-Subscription-Token",
        .value = s_search_key,
    };

    http_client_response_t response = {0};
    http_client_status_t   http_rt  = http_client_request(
        &(const http_client_request_t){
               .cacert        = s_search_cacert,
               .cacert_len    = s_search_cacert_len,
               .tls_no_verify = s_search_tls_no_verify,
               .host          = SEARCH_HOST,
               .port          = 443,
               .method        = "GET",
               .path          = path,
               .headers       = headers,
               .headers_count = header_count,
               .body          = (const uint8_t *)"",
               .body_length   = 0,
               .timeout_ms    = SEARCH_TIMEOUT_MS,
        },
        &response);

    if (http_rt != HTTP_CLIENT_SUCCESS) {
        MIMI_LOGE(TAG, "http request failed: %d", http_rt);
        return OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR;
    }

    if (status_code) {
        *status_code = response.status_code;
    }

    resp_buf[0] = '\0';
    if (response.body && response.body_length > 0) {
        size_t copy = (response.body_length < resp_size - 1) ? response.body_length : (resp_size - 1);
        memcpy(resp_buf, response.body, copy);
        resp_buf[copy] = '\0';
    }

    http_client_free(&response);
    return OPRT_OK;
}

OPERATE_RET tool_web_search_init(void)
{
    if (MIMI_SECRET_SEARCH_KEY[0] != '\0') {
        safe_copy(s_search_key, sizeof(s_search_key), MIMI_SECRET_SEARCH_KEY);
    }

    char tmp[128] = {0};
    if (mimi_kv_get_string(MIMI_NVS_SEARCH, MIMI_NVS_KEY_API_KEY, tmp, sizeof(tmp)) == OPRT_OK) {
        safe_copy(s_search_key, sizeof(s_search_key), tmp);
    }

    MIMI_LOGI(TAG, "web search init credential=%s", s_search_key[0] ? "configured" : "empty");
    return OPRT_OK;
}

OPERATE_RET tool_web_search_execute(const char *input_json, char *output, size_t output_size)
{
    if (!input_json || !output || output_size == 0) {
        return OPRT_INVALID_PARM;
    }

    output[0] = '\0';

    if (s_search_key[0] == '\0') {
        snprintf(output, output_size, "Error: No search API key configured. Use CLI: set_search_key <api_key>");
        return OPRT_NOT_FOUND;
    }

    cJSON *input = cJSON_Parse(input_json);
    if (!input) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return OPRT_CJSON_PARSE_ERR;
    }

    cJSON *query = cJSON_GetObjectItem(input, "query");
    if (!cJSON_IsString(query) || !query->valuestring || query->valuestring[0] == '\0') {
        cJSON_Delete(input);
        snprintf(output, output_size, "Error: missing 'query'");
        return OPRT_INVALID_PARM;
    }

    char encoded_query[384] = {0};
    url_encode(query->valuestring, encoded_query, sizeof(encoded_query));
    cJSON_Delete(input);

    if (encoded_query[0] == '\0') {
        snprintf(output, output_size, "Error: empty query");
        return OPRT_INVALID_PARM;
    }

    char path[512] = {0};
    int  n         = snprintf(path, sizeof(path), "%s?q=%s&count=%d", SEARCH_PATH, encoded_query, SEARCH_RESULT_COUNT);
    if (n <= 0 || (size_t)n >= sizeof(path)) {
        snprintf(output, output_size, "Error: query too long");
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    char *resp = tal_malloc(SEARCH_RESP_BUF_SIZE);
    if (!resp) {
        snprintf(output, output_size, "Error: alloc search response buffer failed");
        return OPRT_MALLOC_FAILED;
    }
    memset(resp, 0, SEARCH_RESP_BUF_SIZE);
    uint16_t    status = 0;
    OPERATE_RET rt     = search_http_call(path, resp, SEARCH_RESP_BUF_SIZE, &status);
    if (rt != OPRT_OK) {
        snprintf(output, output_size, "Error: search request failed (rt=%d)", rt);
        tal_free(resp);
        return rt;
    }

    if (status != 200) {
        snprintf(output, output_size, "Error: search API http=%u", status);
        tal_free(resp);
        return OPRT_COM_ERROR;
    }

    cJSON *root = cJSON_Parse(resp);
    tal_free(resp);
    if (!root) {
        snprintf(output, output_size, "Error: parse search response failed");
        return OPRT_CJSON_PARSE_ERR;
    }

    format_results(root, output, output_size);
    cJSON_Delete(root);
    return OPRT_OK;
}

OPERATE_RET tool_web_search_set_key(const char *api_key)
{
    if (!api_key || api_key[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    safe_copy(s_search_key, sizeof(s_search_key), api_key);
    return mimi_kv_set_string(MIMI_NVS_SEARCH, MIMI_NVS_KEY_API_KEY, api_key);
}
