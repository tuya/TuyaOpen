#include "certs/tls_cert_bundle.h"

#include <string.h>

#include "certs/ca_bundle_mini.h"
#include "iotdns.h"
#include "im_config.h"
#include "tal_api.h"

static const char *TAG = "tls_bundle";

#define IM_TLS_CERT_QUERY_RETRY_COUNT      3
#define IM_TLS_CERT_QUERY_RETRY_BASE_MS    400
#define IM_TLS_CERT_QUERY_RETRY_MAX_MS     1600
#define IM_TLS_CERT_FAIL_CACHE_SLOTS       8
#define IM_TLS_CERT_FAIL_RETRY_INTERVAL_MS (5 * 60 * 1000)
#define IM_TLS_CERT_FAIL_LOG_INTERVAL_MS   (60 * 1000)

typedef struct {
    char        host[96];
    OPERATE_RET last_rt;
    uint32_t    failed_at_ms;
    uint32_t    last_log_ms;
    bool        used;
} im_tls_fail_cache_t;

static im_tls_fail_cache_t s_fail_cache[IM_TLS_CERT_FAIL_CACHE_SLOTS] = {0};
static uint8_t               s_fail_cache_next_slot                       = 0;

static void extract_host(const char *host_or_url, char *host, size_t host_size)
{
    if (!host || host_size == 0) {
        return;
    }
    host[0] = '\0';
    if (!host_or_url || host_or_url[0] == '\0') {
        return;
    }

    const char *begin  = host_or_url;
    const char *scheme = strstr(host_or_url, "://");
    if (scheme) {
        begin = scheme + 3;
    }

    size_t copy = strcspn(begin, "/:");
    if (copy >= host_size) {
        copy = host_size - 1;
    }
    memcpy(host, begin, copy);
    host[copy] = '\0';
}

/* Hosts that use public CAs; use builtin bundle only to avoid iot-dns (h6.iot-dns.com)
 * when device cannot reach Tuya DNS (e.g. firewall / no route). */
static const char *const s_builtin_only_hosts[] = {
    "api.telegram.org",
    "discord.com",
    "gateway.discord.gg",
    "open.feishu.cn",
};
static const size_t s_builtin_only_hosts_count =
    sizeof(s_builtin_only_hosts) / sizeof(s_builtin_only_hosts[0]);

static bool is_builtin_only_host(const char *host)
{
    if (!host || host[0] == '\0') {
        return false;
    }
    for (size_t i = 0; i < s_builtin_only_hosts_count; i++) {
        if (strcmp(host, s_builtin_only_hosts[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool should_retry_iotdns_query(OPERATE_RET rt)
{
    return (rt == OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR || rt == OPRT_RESOURCE_NOT_READY || rt == OPRT_TIMEOUT);
}

static uint32_t cert_query_retry_delay_ms(uint32_t attempt)
{
    uint32_t delay = IM_TLS_CERT_QUERY_RETRY_BASE_MS;
    while (attempt > 0 && delay < IM_TLS_CERT_QUERY_RETRY_MAX_MS) {
        if (delay > (IM_TLS_CERT_QUERY_RETRY_MAX_MS / 2)) {
            delay = IM_TLS_CERT_QUERY_RETRY_MAX_MS;
            break;
        }
        delay <<= 1;
        attempt--;
    }
    if (delay > IM_TLS_CERT_QUERY_RETRY_MAX_MS) {
        delay = IM_TLS_CERT_QUERY_RETRY_MAX_MS;
    }
    return delay;
}

static im_tls_fail_cache_t *fail_cache_find(const char *host)
{
    if (!host || host[0] == '\0') {
        return NULL;
    }
    for (uint32_t i = 0; i < IM_TLS_CERT_FAIL_CACHE_SLOTS; i++) {
        if (s_fail_cache[i].used && strcmp(s_fail_cache[i].host, host) == 0) {
            return &s_fail_cache[i];
        }
    }
    return NULL;
}

static void fail_cache_clear(const char *host)
{
    im_tls_fail_cache_t *slot = fail_cache_find(host);
    if (!slot) {
        return;
    }
    memset(slot, 0, sizeof(*slot));
}

static void fail_cache_save(const char *host, OPERATE_RET rt, uint32_t now_ms)
{
    if (!host || host[0] == '\0') {
        return;
    }

    im_tls_fail_cache_t *slot = fail_cache_find(host);
    if (!slot) {
        slot                   = &s_fail_cache[s_fail_cache_next_slot];
        s_fail_cache_next_slot = (uint8_t)((s_fail_cache_next_slot + 1) % IM_TLS_CERT_FAIL_CACHE_SLOTS);
        memset(slot, 0, sizeof(*slot));
    }

    snprintf(slot->host, sizeof(slot->host), "%s", host);
    slot->last_rt      = rt;
    slot->failed_at_ms = now_ms;
    slot->used         = true;
}

static bool fail_cache_hit(const char *host, OPERATE_RET *cached_rt, uint32_t now_ms)
{
    im_tls_fail_cache_t *slot = fail_cache_find(host);
    if (!slot) {
        return false;
    }

    if ((uint32_t)(now_ms - slot->failed_at_ms) >= IM_TLS_CERT_FAIL_RETRY_INTERVAL_MS) {
        memset(slot, 0, sizeof(*slot));
        return false;
    }

    if (cached_rt) {
        *cached_rt = slot->last_rt;
    }

    if ((slot->last_log_ms == 0) || ((uint32_t)(now_ms - slot->last_log_ms) >= IM_TLS_CERT_FAIL_LOG_INTERVAL_MS)) {
        IM_LOGD(TAG, "skip iotdns host=%s due to fail cache rt=%d age_ms=%u", host, slot->last_rt,
                  (unsigned)(now_ms - slot->failed_at_ms));
        slot->last_log_ms = now_ms;
    }

    return true;
}

static OPERATE_RET im_tls_load_builtin_ca_bundle(uint8_t **cacert, size_t *cacert_len)
{
    if (!cacert || !cacert_len) {
        return OPRT_INVALID_PARM;
    }
    if (g_im_ca_bundle_mini_pem_len == 0) {
        return OPRT_NOT_FOUND;
    }
    if (strstr((const char *)g_im_ca_bundle_mini_pem, "-----BEGIN CERTIFICATE-----") == NULL) {
        return OPRT_INVALID_PARM;
    }

    uint8_t *buf = im_malloc(g_im_ca_bundle_mini_pem_len);
    if (!buf) {
        return OPRT_MALLOC_FAILED;
    }

    memcpy(buf, g_im_ca_bundle_mini_pem, g_im_ca_bundle_mini_pem_len);
    *cacert     = buf;
    *cacert_len = g_im_ca_bundle_mini_pem_len;
    return OPRT_OK;
}

OPERATE_RET im_tls_query_domain_certs(const char *host_or_url, uint8_t **cacert, size_t *cacert_len)
{
    if (!host_or_url || !cacert || !cacert_len) {
        return OPRT_INVALID_PARM;
    }

    *cacert     = NULL;
    *cacert_len = 0;

    char host[96] = {0};
    extract_host(host_or_url, host, sizeof(host));
    uint32_t now_ms = tal_system_get_millisecond();

    /* For well-known public hosts, use builtin CA only to avoid requesting h6.iot-dns.com
     * when the device cannot reach Tuya DNS (avoids Transport/HTTPNetworkError logs). */
    if (is_builtin_only_host(host)) {
        OPERATE_RET builtin_rt = im_tls_load_builtin_ca_bundle(cacert, cacert_len);
        if (builtin_rt == OPRT_OK && *cacert && *cacert_len > 0) {
            fail_cache_clear(host);
            return OPRT_OK;
        }
    }

    OPERATE_RET cached_rt = OPRT_COM_ERROR;
    if (fail_cache_hit(host, &cached_rt, now_ms)) {
        return (cached_rt == OPRT_OK) ? OPRT_COM_ERROR : cached_rt;
    }

    OPERATE_RET rt = OPRT_COM_ERROR;
    for (uint32_t attempt = 0; attempt < IM_TLS_CERT_QUERY_RETRY_COUNT; attempt++) {
        uint8_t *iotdns_cert     = NULL;
        uint16_t iotdns_cert_len = 0;

        rt = tuya_iotdns_query_domain_certs((char *)host_or_url, &iotdns_cert, &iotdns_cert_len);
        if (rt == OPRT_OK && iotdns_cert && iotdns_cert_len > 0) {
            *cacert     = iotdns_cert;
            *cacert_len = iotdns_cert_len;
            fail_cache_clear(host);
            return OPRT_OK;
        }

        if (iotdns_cert) {
            im_free(iotdns_cert);
        }

        if (attempt + 1 >= IM_TLS_CERT_QUERY_RETRY_COUNT || !should_retry_iotdns_query(rt)) {
            break;
        }

        uint32_t delay_ms = cert_query_retry_delay_ms(attempt);
        IM_LOGD(TAG, "iotdns cert query retry %u/%u host=%s rt=%d delay=%u", (unsigned)(attempt + 1),
                  (unsigned)IM_TLS_CERT_QUERY_RETRY_COUNT, host_or_url, rt, (unsigned)delay_ms);
        tal_system_sleep(delay_ms);
    }

    OPERATE_RET builtin_rt = im_tls_load_builtin_ca_bundle(cacert, cacert_len);
    if (builtin_rt == OPRT_OK && *cacert && *cacert_len > 0) {
        fail_cache_clear(host);
        IM_LOGD(TAG, "iotdns cert unavailable host=%s rt=%d, fallback to builtin ca bundle len=%zu", host, rt,
                  *cacert_len);
        return OPRT_OK;
    }

    OPERATE_RET final_rt = (rt == OPRT_OK) ? OPRT_COM_ERROR : rt;
    fail_cache_save(host, final_rt, now_ms);
    IM_LOGD(TAG, "iotdns cert unavailable host=%s rt=%d, builtin_ca_rt=%d", host, rt, builtin_rt);
    return final_rt;
}
