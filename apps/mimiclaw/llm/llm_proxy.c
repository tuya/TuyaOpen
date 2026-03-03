#include "llm_proxy.h"

#include "http_client_interface.h"
#include "tls_cert_bundle.h"

static const char *TAG = "llm";

#define LLM_API_KEY_MAX_LEN 320
#define LLM_MODEL_MAX_LEN   64
#define LLM_API_URL_MAX_LEN 192

static char s_api_key[LLM_API_KEY_MAX_LEN] = {0};
static char s_model[LLM_MODEL_MAX_LEN]     = MIMI_LLM_DEFAULT_MODEL;
static char s_provider[32]                 = MIMI_LLM_PROVIDER_DEFAULT;
static char s_api_url[LLM_API_URL_MAX_LEN] = {0};

static uint8_t *s_openai_cacert        = NULL;
static size_t   s_openai_cacert_len    = 0;
static uint8_t *s_anthropic_cacert     = NULL;
static size_t   s_anthropic_cacert_len = 0;

typedef struct {
    char host[96];
    char path[160];
} llm_endpoint_t;

static llm_endpoint_t s_openai_endpoint    = {0};
static llm_endpoint_t s_anthropic_endpoint = {0};

#define LLM_LOG_PREVIEW_LEN 1024

static void llm_log_payload(const char *label, const char *payload)
{
    size_t total = payload ? strlen(payload) : 0;
    MIMI_LOGI(TAG, "========== %s (%u bytes) ==========", label ? label : "llm payload", (unsigned)total);
    if (payload && total > 0) {
        if (total <= LLM_LOG_PREVIEW_LEN) {
            MIMI_LOGI(TAG, "%s", payload);
        } else {
            char preview[LLM_LOG_PREVIEW_LEN + 1];
            memcpy(preview, payload, LLM_LOG_PREVIEW_LEN);
            preview[LLM_LOG_PREVIEW_LEN] = '\0';
            MIMI_LOGI(TAG, "%s ...[truncated, %u more bytes]", preview, (unsigned)(total - LLM_LOG_PREVIEW_LEN));
        }
    }
    MIMI_LOGI(TAG, "========== end %s ==========", label ? label : "llm payload");
}

static void safe_copy(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t n = strnlen(src, dst_size - 1);
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static bool provider_is_openai(void)
{
    return strcmp(s_provider, "openai") == 0;
}

static void free_cached_cert(uint8_t **cert, size_t *cert_len)
{
    if (cert && *cert) {
        tal_free(*cert);
        *cert = NULL;
    }
    if (cert_len) {
        *cert_len = 0;
    }
}

static void clear_endpoint_cache(void)
{
    memset(&s_openai_endpoint, 0, sizeof(s_openai_endpoint));
    memset(&s_anthropic_endpoint, 0, sizeof(s_anthropic_endpoint));
    free_cached_cert(&s_openai_cacert, &s_openai_cacert_len);
    free_cached_cert(&s_anthropic_cacert, &s_anthropic_cacert_len);
}

static const char *llm_default_endpoint_url(void)
{
    return provider_is_openai() ? MIMI_OPENAI_API_URL : MIMI_LLM_API_URL;
}

static const char *llm_active_endpoint_url(void)
{
    return (s_api_url[0] != '\0') ? s_api_url : llm_default_endpoint_url();
}

static void parse_endpoint_url(const char *url, llm_endpoint_t *endpoint)
{
    if (!endpoint) {
        return;
    }

    endpoint->host[0] = '\0';
    snprintf(endpoint->path, sizeof(endpoint->path), "/");

    if (!url || url[0] == '\0') {
        return;
    }

    const char *begin  = url;
    const char *scheme = strstr(url, "://");
    if (scheme) {
        begin = scheme + 3;
    }

    size_t host_len = strcspn(begin, "/:");
    if (host_len == 0) {
        return;
    }
    if (host_len >= sizeof(endpoint->host)) {
        host_len = sizeof(endpoint->host) - 1;
    }
    memcpy(endpoint->host, begin, host_len);
    endpoint->host[host_len] = '\0';

    const char *path = strchr(begin, '/');
    if (path && path[0] == '/') {
        snprintf(endpoint->path, sizeof(endpoint->path), "%s", path);
    }
}

static llm_endpoint_t *llm_current_endpoint(void)
{
    llm_endpoint_t *endpoint = provider_is_openai() ? &s_openai_endpoint : &s_anthropic_endpoint;
    parse_endpoint_url(llm_active_endpoint_url(), endpoint);
    return endpoint;
}

static const char *llm_api_host(void)
{
    llm_endpoint_t *endpoint = llm_current_endpoint();
    if (!endpoint || endpoint->host[0] == '\0') {
        return NULL;
    }
    return endpoint->host;
}

static const char *llm_api_path(void)
{
    llm_endpoint_t *endpoint = llm_current_endpoint();
    if (!endpoint || endpoint->path[0] == '\0') {
        return "/";
    }
    return endpoint->path;
}

static void get_provider_cert(uint8_t **cert, size_t **cert_len)
{
    if (provider_is_openai()) {
        *cert     = s_openai_cacert;
        *cert_len = &s_openai_cacert_len;
    } else {
        *cert     = s_anthropic_cacert;
        *cert_len = &s_anthropic_cacert_len;
    }
}

static OPERATE_RET ensure_provider_cert(void)
{
    uint8_t *cert     = NULL;
    size_t  *cert_len = NULL;
    get_provider_cert(&cert, &cert_len);

    if (cert && cert_len && *cert_len > 0) {
        return OPRT_OK;
    }

    if (!cert_len) {
        return OPRT_COM_ERROR;
    }

    const char *host = llm_api_host();
    if (!host || host[0] == '\0') {
        MIMI_LOGE(TAG, "invalid llm endpoint host for provider=%s", s_provider);
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = mimi_tls_query_domain_certs(host, &cert, cert_len);
    if (rt != OPRT_OK || !cert || *cert_len == 0) {
        if (cert) {
            tal_free(cert);
        }
        cert      = NULL;
        *cert_len = 0;
        MIMI_LOGW(TAG, "cert unavailable for %s, fallback to TLS no-verify mode rt=%d", host, rt);
    }

    if (provider_is_openai()) {
        s_openai_cacert = cert;
    } else {
        s_anthropic_cacert = cert;
    }

    return OPRT_OK;
}

static char *join_text_blocks(cJSON *content)
{
    if (!content) {
        return strdup("");
    }
    if (cJSON_IsString(content)) {
        return strdup(content->valuestring ? content->valuestring : "");
    }
    if (!cJSON_IsArray(content)) {
        return strdup("");
    }

    size_t total = 0;
    cJSON *block = NULL;
    cJSON_ArrayForEach(block, content)
    {
        cJSON *btype = cJSON_GetObjectItem(block, "type");
        if (!cJSON_IsString(btype) || strcmp(btype->valuestring, "text") != 0) {
            continue;
        }
        cJSON *text = cJSON_GetObjectItem(block, "text");
        if (cJSON_IsString(text) && text->valuestring) {
            total += strlen(text->valuestring);
        }
    }

    char *out = calloc(1, total + 1);
    if (!out) {
        return NULL;
    }

    size_t off = 0;
    cJSON_ArrayForEach(block, content)
    {
        cJSON *btype = cJSON_GetObjectItem(block, "type");
        if (!cJSON_IsString(btype) || strcmp(btype->valuestring, "text") != 0) {
            continue;
        }
        cJSON *text = cJSON_GetObjectItem(block, "text");
        if (!cJSON_IsString(text) || !text->valuestring) {
            continue;
        }
        size_t len = strlen(text->valuestring);
        memcpy(out + off, text->valuestring, len);
        off += len;
    }

    out[off] = '\0';
    return out;
}

static cJSON *convert_tools_openai(const char *tools_json)
{
    if (!tools_json) {
        return NULL;
    }

    cJSON *arr = cJSON_Parse(tools_json);
    if (!arr || !cJSON_IsArray(arr)) {
        cJSON_Delete(arr);
        return NULL;
    }

    cJSON *out  = cJSON_CreateArray();
    cJSON *tool = NULL;
    cJSON_ArrayForEach(tool, arr)
    {
        cJSON *name   = cJSON_GetObjectItem(tool, "name");
        cJSON *desc   = cJSON_GetObjectItem(tool, "description");
        cJSON *schema = cJSON_GetObjectItem(tool, "input_schema");
        if (!cJSON_IsString(name)) {
            continue;
        }

        cJSON *func = cJSON_CreateObject();
        cJSON_AddStringToObject(func, "name", name->valuestring);
        if (cJSON_IsString(desc)) {
            cJSON_AddStringToObject(func, "description", desc->valuestring);
        }
        if (schema) {
            cJSON_AddItemToObject(func, "parameters", cJSON_Duplicate(schema, 1));
        }

        cJSON *wrap = cJSON_CreateObject();
        cJSON_AddStringToObject(wrap, "type", "function");
        cJSON_AddItemToObject(wrap, "function", func);
        cJSON_AddItemToArray(out, wrap);
    }

    cJSON_Delete(arr);
    return out;
}

static cJSON *convert_messages_openai(const char *system_prompt, cJSON *messages)
{
    cJSON *out = cJSON_CreateArray();
    if (!out) {
        return NULL;
    }

    if (system_prompt && system_prompt[0]) {
        cJSON *sys = cJSON_CreateObject();
        cJSON_AddStringToObject(sys, "role", "system");
        cJSON_AddStringToObject(sys, "content", system_prompt);
        cJSON_AddItemToArray(out, sys);
    }

    if (!messages || !cJSON_IsArray(messages)) {
        return out;
    }

    cJSON *msg = NULL;
    cJSON_ArrayForEach(msg, messages)
    {
        cJSON *role    = cJSON_GetObjectItem(msg, "role");
        cJSON *content = cJSON_GetObjectItem(msg, "content");
        if (!cJSON_IsString(role)) {
            continue;
        }

        if (strcmp(role->valuestring, "assistant") == 0 && cJSON_IsArray(content)) {
            cJSON *m = cJSON_CreateObject();
            cJSON_AddStringToObject(m, "role", "assistant");

            char *text_buf = join_text_blocks(content);
            if (text_buf) {
                cJSON_AddStringToObject(m, "content", text_buf);
                free(text_buf);
            } else {
                cJSON_AddStringToObject(m, "content", "");
            }

            cJSON *tool_calls = NULL;
            cJSON *block      = NULL;
            cJSON_ArrayForEach(block, content)
            {
                cJSON *btype = cJSON_GetObjectItem(block, "type");
                if (!cJSON_IsString(btype) || strcmp(btype->valuestring, "tool_use") != 0) {
                    continue;
                }
                cJSON *id    = cJSON_GetObjectItem(block, "id");
                cJSON *name  = cJSON_GetObjectItem(block, "name");
                cJSON *input = cJSON_GetObjectItem(block, "input");
                if (!cJSON_IsString(name)) {
                    continue;
                }

                if (!tool_calls) {
                    tool_calls = cJSON_CreateArray();
                }

                cJSON *tc = cJSON_CreateObject();
                if (cJSON_IsString(id)) {
                    cJSON_AddStringToObject(tc, "id", id->valuestring);
                }
                cJSON_AddStringToObject(tc, "type", "function");

                cJSON *func = cJSON_CreateObject();
                cJSON_AddStringToObject(func, "name", name->valuestring);
                if (input) {
                    char *args = cJSON_PrintUnformatted(input);
                    if (args) {
                        cJSON_AddStringToObject(func, "arguments", args);
                        cJSON_free(args);
                    }
                }
                cJSON_AddItemToObject(tc, "function", func);
                cJSON_AddItemToArray(tool_calls, tc);
            }
            if (tool_calls) {
                cJSON_AddItemToObject(m, "tool_calls", tool_calls);
            }
            cJSON_AddItemToArray(out, m);
            continue;
        }

        if (strcmp(role->valuestring, "user") == 0 && cJSON_IsArray(content)) {
            cJSON *block = NULL;
            cJSON_ArrayForEach(block, content)
            {
                cJSON *btype = cJSON_GetObjectItem(block, "type");
                if (!cJSON_IsString(btype)) {
                    continue;
                }
                if (strcmp(btype->valuestring, "tool_result") == 0) {
                    cJSON *tool_id  = cJSON_GetObjectItem(block, "tool_use_id");
                    cJSON *tcontent = cJSON_GetObjectItem(block, "content");
                    if (!cJSON_IsString(tool_id)) {
                        continue;
                    }
                    cJSON *tm = cJSON_CreateObject();
                    cJSON_AddStringToObject(tm, "role", "tool");
                    cJSON_AddStringToObject(tm, "tool_call_id", tool_id->valuestring);
                    cJSON_AddStringToObject(tm, "content", cJSON_IsString(tcontent) ? tcontent->valuestring : "");
                    cJSON_AddItemToArray(out, tm);
                }
            }

            char *user_text = join_text_blocks(content);
            if (user_text) {
                cJSON *um = cJSON_CreateObject();
                cJSON_AddStringToObject(um, "role", "user");
                cJSON_AddStringToObject(um, "content", user_text);
                cJSON_AddItemToArray(out, um);
                free(user_text);
            }
            continue;
        }

        char *text = join_text_blocks(content);
        if (!text) {
            continue;
        }
        cJSON *m = cJSON_CreateObject();
        cJSON_AddStringToObject(m, "role", role->valuestring);
        cJSON_AddStringToObject(m, "content", text);
        cJSON_AddItemToArray(out, m);
        free(text);
    }

    return out;
}

static void extract_text_anthropic(cJSON *root, char *buf, size_t size)
{
    if (!buf || size == 0) {
        return;
    }
    buf[0] = '\0';

    cJSON *content = cJSON_GetObjectItem(root, "content");
    if (!cJSON_IsArray(content)) {
        return;
    }

    size_t off   = 0;
    cJSON *block = NULL;
    cJSON_ArrayForEach(block, content)
    {
        cJSON *btype = cJSON_GetObjectItem(block, "type");
        cJSON *text  = cJSON_GetObjectItem(block, "text");
        if (!cJSON_IsString(btype) || strcmp(btype->valuestring, "text") != 0) {
            continue;
        }
        if (!cJSON_IsString(text) || !text->valuestring || off >= size - 1) {
            continue;
        }
        size_t len  = strlen(text->valuestring);
        size_t copy = (len < size - off - 1) ? len : (size - off - 1);
        memcpy(buf + off, text->valuestring, copy);
        off += copy;
    }
    buf[off] = '\0';
}

static void extract_text_openai(cJSON *root, char *buf, size_t size)
{
    if (!buf || size == 0) {
        return;
    }
    buf[0] = '\0';

    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    cJSON *choice0 = cJSON_IsArray(choices) ? cJSON_GetArrayItem(choices, 0) : NULL;
    cJSON *message = choice0 ? cJSON_GetObjectItem(choice0, "message") : NULL;
    cJSON *content = message ? cJSON_GetObjectItem(message, "content") : NULL;
    if (!cJSON_IsString(content) || !content->valuestring) {
        return;
    }

    strncpy(buf, content->valuestring, size - 1);
    buf[size - 1] = '\0';
}

static OPERATE_RET llm_http_call(const char *post_data, char *resp_buf, size_t resp_buf_size, uint16_t *status_code)
{
    if (!post_data || !resp_buf || resp_buf_size == 0) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = ensure_provider_cert();
    if (rt != OPRT_OK) {
        return rt;
    }

    uint8_t *cacert     = NULL;
    size_t  *cacert_len = NULL;
    get_provider_cert(&cacert, &cacert_len);
    if (!cacert_len) {
        return OPRT_COM_ERROR;
    }
    bool tls_no_verify = (!cacert || *cacert_len == 0);
    if (tls_no_verify) {
        MIMI_LOGW(TAG, "llm host=%s use TLS no-verify mode", llm_api_host() ? llm_api_host() : "");
    }

    const char *host = llm_api_host();
    const char *path = llm_api_path();
    if (!host || host[0] == '\0' || !path || path[0] != '/') {
        return OPRT_INVALID_PARM;
    }

    char                 auth[LLM_API_KEY_MAX_LEN + 16] = {0};
    http_client_header_t headers[4]                     = {0};
    uint8_t              header_count                   = 0;

    headers[header_count++] = (http_client_header_t){
        .key   = "Content-Type",
        .value = "application/json",
    };

    if (provider_is_openai()) {
        snprintf(auth, sizeof(auth), "Bearer %s", s_api_key);
        headers[header_count++] = (http_client_header_t){
            .key   = "Authorization",
            .value = auth,
        };
    } else {
        headers[header_count++] = (http_client_header_t){
            .key   = "x-api-key",
            .value = s_api_key,
        };
        headers[header_count++] = (http_client_header_t){
            .key   = "anthropic-version",
            .value = MIMI_LLM_API_VERSION,
        };
    }

    http_client_response_t response = {0};
    http_client_status_t   http_rt  = http_client_request(
        &(const http_client_request_t){
               .cacert        = cacert,
               .cacert_len    = *cacert_len,
               .tls_no_verify = tls_no_verify,
               .host          = host,
               .port          = 443,
               .method        = "POST",
               .path          = path,
               .headers       = headers,
               .headers_count = header_count,
               .body          = (const uint8_t *)post_data,
               .body_length   = strlen(post_data),
               .timeout_ms    = 120 * 1000,
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
        size_t copy = (response.body_length < resp_buf_size - 1) ? response.body_length : (resp_buf_size - 1);
        memcpy(resp_buf, response.body, copy);
        resp_buf[copy] = '\0';
    }

    http_client_free(&response);
    return OPRT_OK;
}

OPERATE_RET llm_proxy_init(void)
{
    s_api_url[0] = '\0';

    if (MIMI_SECRET_API_KEY[0] != '\0') {
        safe_copy(s_api_key, sizeof(s_api_key), MIMI_SECRET_API_KEY);
    }

    if (MIMI_SECRET_MODEL[0] != '\0') {
        safe_copy(s_model, sizeof(s_model), MIMI_SECRET_MODEL);
    }

    if (MIMI_SECRET_MODEL_PROVIDER[0] != '\0') {
        safe_copy(s_provider, sizeof(s_provider), MIMI_SECRET_MODEL_PROVIDER);
    }

    char api_key_tmp[LLM_API_KEY_MAX_LEN] = {0};
    if (mimi_kv_get_string(MIMI_NVS_LLM, MIMI_NVS_KEY_API_KEY, api_key_tmp, sizeof(api_key_tmp)) == OPRT_OK) {
        safe_copy(s_api_key, sizeof(s_api_key), api_key_tmp);
    }

    char model_tmp[LLM_MODEL_MAX_LEN] = {0};
    if (mimi_kv_get_string(MIMI_NVS_LLM, MIMI_NVS_KEY_MODEL, model_tmp, sizeof(model_tmp)) == OPRT_OK) {
        safe_copy(s_model, sizeof(s_model), model_tmp);
    }

    char provider_tmp[32] = {0};
    if (mimi_kv_get_string(MIMI_NVS_LLM, MIMI_NVS_KEY_PROVIDER, provider_tmp, sizeof(provider_tmp)) == OPRT_OK) {
        safe_copy(s_provider, sizeof(s_provider), provider_tmp);
    }

    char api_url_tmp[LLM_API_URL_MAX_LEN] = {0};
    if (mimi_kv_get_string(MIMI_NVS_LLM, MIMI_NVS_KEY_API_URL, api_url_tmp, sizeof(api_url_tmp)) == OPRT_OK &&
        api_url_tmp[0] != '\0') {
        safe_copy(s_api_url, sizeof(s_api_url), api_url_tmp);
    }

    clear_endpoint_cache();

    MIMI_LOGI(TAG, "llm init provider=%s model=%s endpoint=%s url_src=%s credential=%s", s_provider, s_model,
              llm_api_host() ? "valid" : "invalid", s_api_url[0] ? "nvs" : "config.h",
              s_api_key[0] ? "configured" : "empty");
    return OPRT_OK;
}

OPERATE_RET llm_set_api_key(const char *api_key)
{
    if (!api_key) {
        return OPRT_INVALID_PARM;
    }
    safe_copy(s_api_key, sizeof(s_api_key), api_key);
    return mimi_kv_set_string(MIMI_NVS_LLM, MIMI_NVS_KEY_API_KEY, api_key);
}

OPERATE_RET llm_set_api_url(const char *api_url)
{
    if (!api_url) {
        return OPRT_INVALID_PARM;
    }

    if (api_url[0] == '\0') {
        s_api_url[0] = '\0';
        clear_endpoint_cache();
        return mimi_kv_del(MIMI_NVS_LLM, MIMI_NVS_KEY_API_URL);
    }

    safe_copy(s_api_url, sizeof(s_api_url), api_url);
    clear_endpoint_cache();
    return mimi_kv_set_string(MIMI_NVS_LLM, MIMI_NVS_KEY_API_URL, api_url);
}

OPERATE_RET llm_set_provider(const char *provider)
{
    if (!provider) {
        return OPRT_INVALID_PARM;
    }
    safe_copy(s_provider, sizeof(s_provider), provider);
    clear_endpoint_cache();
    return mimi_kv_set_string(MIMI_NVS_LLM, MIMI_NVS_KEY_PROVIDER, provider);
}

OPERATE_RET llm_set_model(const char *model)
{
    if (!model) {
        return OPRT_INVALID_PARM;
    }
    safe_copy(s_model, sizeof(s_model), model);
    return mimi_kv_set_string(MIMI_NVS_LLM, MIMI_NVS_KEY_MODEL, model);
}

OPERATE_RET llm_chat(const char *system_prompt, const char *messages_json, char *response_buf, size_t buf_size)
{
    if (!response_buf || buf_size == 0) {
        return OPRT_INVALID_PARM;
    }

    if (s_api_key[0] == '\0') {
        snprintf(response_buf, buf_size, "LLM API key not configured. Use CLI set_api_key.");
        return OPRT_NOT_FOUND;
    }

    cJSON *body = cJSON_CreateObject();
    if (!body) {
        return OPRT_MALLOC_FAILED;
    }
    cJSON_AddStringToObject(body, "model", s_model);
    if (provider_is_openai()) {
        cJSON_AddNumberToObject(body, "max_completion_tokens", MIMI_LLM_MAX_TOKENS);
    } else {
        cJSON_AddNumberToObject(body, "max_tokens", MIMI_LLM_MAX_TOKENS);
    }

    cJSON *parsed_messages = NULL;
    if (messages_json && messages_json[0]) {
        parsed_messages = cJSON_Parse(messages_json);
    }
    if (!parsed_messages || !cJSON_IsArray(parsed_messages)) {
        cJSON_Delete(parsed_messages);
        parsed_messages = cJSON_CreateArray();
        if (!parsed_messages) {
            cJSON_Delete(body);
            return OPRT_MALLOC_FAILED;
        }
        cJSON *msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "role", "user");
        cJSON_AddStringToObject(msg, "content", messages_json ? messages_json : "");
        cJSON_AddItemToArray(parsed_messages, msg);
    }

    if (provider_is_openai()) {
        cJSON *openai_msgs = convert_messages_openai(system_prompt, parsed_messages);
        if (!openai_msgs) {
            cJSON_Delete(parsed_messages);
            cJSON_Delete(body);
            return OPRT_MALLOC_FAILED;
        }
        cJSON_AddItemToObject(body, "messages", openai_msgs);
    } else {
        cJSON_AddStringToObject(body, "system", system_prompt ? system_prompt : "");
        cJSON_AddItemToObject(body, "messages", parsed_messages);
        parsed_messages = NULL;
    }

    cJSON_Delete(parsed_messages);

    char *post_data = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!post_data) {
        return OPRT_MALLOC_FAILED;
    }

    char *raw_resp = calloc(1, MIMI_LLM_STREAM_BUF_SIZE);
    if (!raw_resp) {
        return OPRT_MALLOC_FAILED;
    }
    MIMI_LOGI(TAG, ">>> LLM chat request: provider=%s model=%s", s_provider, s_model);
    llm_log_payload("LLM request", post_data);
    uint16_t    status = 0;
    OPERATE_RET rt     = llm_http_call(post_data, raw_resp, MIMI_LLM_STREAM_BUF_SIZE, &status);
    cJSON_free(post_data);
    MIMI_LOGI(TAG, "<<< LLM chat response: http_status=%u rt=%d", (unsigned)status, rt);
    llm_log_payload("LLM raw response", raw_resp);

    if (rt != OPRT_OK) {
        snprintf(response_buf, buf_size, "HTTP request failed (rt=%d)", rt);
        free(raw_resp);
        return rt;
    }

    if (status != 200) {
        snprintf(response_buf, buf_size, "API error (HTTP %u): %.300s", status, raw_resp);
        free(raw_resp);
        return OPRT_COM_ERROR;
    }

    cJSON *root = cJSON_Parse(raw_resp);
    free(raw_resp);
    if (!root) {
        snprintf(response_buf, buf_size, "Error: parse response failed");
        return OPRT_CR_CJSON_ERR;
    }

    if (provider_is_openai()) {
        extract_text_openai(root, response_buf, buf_size);
    } else {
        extract_text_anthropic(root, response_buf, buf_size);
    }
    cJSON_Delete(root);

    if (response_buf[0] == '\0') {
        snprintf(response_buf, buf_size, "No text in response");
    }

    return OPRT_OK;
}

void llm_response_free(llm_response_t *resp)
{
    if (!resp) {
        return;
    }

    free(resp->text);
    resp->text     = NULL;
    resp->text_len = 0;

    for (int i = 0; i < resp->call_count; i++) {
        free(resp->calls[i].input);
        resp->calls[i].input     = NULL;
        resp->calls[i].input_len = 0;
    }

    resp->call_count = 0;
    resp->tool_use   = false;
}

OPERATE_RET llm_chat_tools(const char *system_prompt, cJSON *messages, const char *tools_json, llm_response_t *resp)
{
    return llm_chat_tools_ex(system_prompt, messages, tools_json, false, resp);
}

OPERATE_RET llm_chat_tools_ex(const char *system_prompt, cJSON *messages, const char *tools_json, bool force_tool,
                              llm_response_t *resp)
{
    if (!resp) {
        return OPRT_INVALID_PARM;
    }

    memset(resp, 0, sizeof(*resp));

    if (s_api_key[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    cJSON *body = cJSON_CreateObject();
    if (!body) {
        return OPRT_MALLOC_FAILED;
    }

    cJSON_AddStringToObject(body, "model", s_model);
    if (provider_is_openai()) {
        cJSON_AddNumberToObject(body, "max_completion_tokens", MIMI_LLM_MAX_TOKENS);
    } else {
        cJSON_AddNumberToObject(body, "max_tokens", MIMI_LLM_MAX_TOKENS);
    }

    if (provider_is_openai()) {
        cJSON *openai_msgs = convert_messages_openai(system_prompt, messages);
        if (!openai_msgs) {
            cJSON_Delete(body);
            return OPRT_MALLOC_FAILED;
        }
        cJSON_AddItemToObject(body, "messages", openai_msgs);
        if (tools_json) {
            cJSON *tools = convert_tools_openai(tools_json);
            if (tools) {
                cJSON_AddItemToObject(body, "tools", tools);
                cJSON_AddStringToObject(body, "tool_choice", force_tool ? "required" : "auto");
            }
        }
    } else {
        cJSON *msg_copy = cJSON_IsArray(messages) ? cJSON_Duplicate(messages, 1) : cJSON_CreateArray();
        if (!msg_copy) {
            cJSON_Delete(body);
            return OPRT_MALLOC_FAILED;
        }
        cJSON_AddStringToObject(body, "system", system_prompt ? system_prompt : "");
        cJSON_AddItemToObject(body, "messages", msg_copy);
        if (tools_json) {
            cJSON *tools = cJSON_Parse(tools_json);
            if (tools) {
                cJSON_AddItemToObject(body, "tools", tools);
            }
        }
    }

    char *post_data = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!post_data) {
        return OPRT_MALLOC_FAILED;
    }

    char *raw_resp = calloc(1, MIMI_LLM_STREAM_BUF_SIZE);
    if (!raw_resp) {
        cJSON_free(post_data);
        return OPRT_MALLOC_FAILED;
    }
    MIMI_LOGI(TAG, ">>> LLM tools request: provider=%s model=%s host=%s", s_provider, s_model,
              llm_api_host() ? llm_api_host() : "(null)");
    llm_log_payload("LLM tools request", post_data);
    uint16_t    status = 0;
    OPERATE_RET rt     = llm_http_call(post_data, raw_resp, MIMI_LLM_STREAM_BUF_SIZE, &status);
    cJSON_free(post_data);
    MIMI_LOGI(TAG, "<<< LLM tools response: http_status=%u rt=%d", (unsigned)status, rt);
    llm_log_payload("LLM tools raw response", raw_resp);

    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "LLM tools HTTP call failed rt=%d", rt);
        free(raw_resp);
        return rt;
    }
    if (status != 200) {
        MIMI_LOGE(TAG, "LLM tools API error HTTP %u, body: %.512s", (unsigned)status, raw_resp);
        free(raw_resp);
        return OPRT_COM_ERROR;
    }

    cJSON *root = cJSON_Parse(raw_resp);
    free(raw_resp);
    if (!root) {
        return OPRT_CR_CJSON_ERR;
    }

    if (provider_is_openai()) {
        cJSON *choices = cJSON_GetObjectItem(root, "choices");
        cJSON *choice0 = cJSON_IsArray(choices) ? cJSON_GetArrayItem(choices, 0) : NULL;
        if (choice0) {
            cJSON *finish = cJSON_GetObjectItem(choice0, "finish_reason");
            if (cJSON_IsString(finish)) {
                resp->tool_use = (strcmp(finish->valuestring, "tool_calls") == 0);
            }

            cJSON *message = cJSON_GetObjectItem(choice0, "message");
            cJSON *content = message ? cJSON_GetObjectItem(message, "content") : NULL;
            if (cJSON_IsString(content) && content->valuestring) {
                resp->text     = strdup(content->valuestring);
                resp->text_len = resp->text ? strlen(resp->text) : 0;
            }

            cJSON *tool_calls = message ? cJSON_GetObjectItem(message, "tool_calls") : NULL;
            if (cJSON_IsArray(tool_calls)) {
                cJSON *tc = NULL;
                cJSON_ArrayForEach(tc, tool_calls)
                {
                    if (resp->call_count >= MIMI_MAX_TOOL_CALLS) {
                        break;
                    }
                    llm_tool_call_t *call = &resp->calls[resp->call_count];
                    cJSON           *id   = cJSON_GetObjectItem(tc, "id");
                    cJSON           *func = cJSON_GetObjectItem(tc, "function");
                    cJSON           *name = func ? cJSON_GetObjectItem(func, "name") : NULL;
                    cJSON           *args = func ? cJSON_GetObjectItem(func, "arguments") : NULL;
                    if (cJSON_IsString(id)) {
                        strncpy(call->id, id->valuestring, sizeof(call->id) - 1);
                    }
                    if (cJSON_IsString(name)) {
                        strncpy(call->name, name->valuestring, sizeof(call->name) - 1);
                    }
                    if (cJSON_IsString(args)) {
                        call->input     = strdup(args->valuestring);
                        call->input_len = call->input ? strlen(call->input) : 0;
                    }
                    resp->call_count++;
                }
                if (resp->call_count > 0) {
                    resp->tool_use = true;
                }
            }
        }
    } else {
        cJSON *stop_reason = cJSON_GetObjectItem(root, "stop_reason");
        if (cJSON_IsString(stop_reason)) {
            resp->tool_use = (strcmp(stop_reason->valuestring, "tool_use") == 0);
        }

        cJSON *content = cJSON_GetObjectItem(root, "content");
        if (cJSON_IsArray(content)) {
            char *text = join_text_blocks(content);
            if (text) {
                resp->text     = text;
                resp->text_len = strlen(text);
            }

            cJSON *block = NULL;
            cJSON_ArrayForEach(block, content)
            {
                if (resp->call_count >= MIMI_MAX_TOOL_CALLS) {
                    break;
                }
                cJSON *btype = cJSON_GetObjectItem(block, "type");
                if (!cJSON_IsString(btype) || strcmp(btype->valuestring, "tool_use") != 0) {
                    continue;
                }
                llm_tool_call_t *call  = &resp->calls[resp->call_count];
                cJSON           *id    = cJSON_GetObjectItem(block, "id");
                cJSON           *name  = cJSON_GetObjectItem(block, "name");
                cJSON           *input = cJSON_GetObjectItem(block, "input");
                if (cJSON_IsString(id)) {
                    strncpy(call->id, id->valuestring, sizeof(call->id) - 1);
                }
                if (cJSON_IsString(name)) {
                    strncpy(call->name, name->valuestring, sizeof(call->name) - 1);
                }
                if (input) {
                    char *input_json = cJSON_PrintUnformatted(input);
                    if (input_json) {
                        call->input = strdup(input_json);
                        cJSON_free(input_json);
                        call->input_len = call->input ? strlen(call->input) : 0;
                    }
                }
                resp->call_count++;
            }
        }
    }

    /* Log parsed LLM response details */
    MIMI_LOGI(TAG, "LLM parsed result: tool_use=%s text_len=%u call_count=%d", resp->tool_use ? "true" : "false",
              (unsigned)resp->text_len, resp->call_count);
    if (resp->text && resp->text_len > 0) {
        if (resp->text_len <= 512) {
            MIMI_LOGI(TAG, "LLM response text: %s", resp->text);
        } else {
            MIMI_LOGI(TAG, "LLM response text (first 512): %.512s ...[truncated]", resp->text);
        }
    }
    for (int log_i = 0; log_i < resp->call_count; log_i++) {
        MIMI_LOGI(TAG, "LLM tool_call[%d]: id=%s name=%s input=%s", log_i, resp->calls[log_i].id,
                  resp->calls[log_i].name, resp->calls[log_i].input ? resp->calls[log_i].input : "(null)");
    }

    cJSON_Delete(root);
    return OPRT_OK;
}
