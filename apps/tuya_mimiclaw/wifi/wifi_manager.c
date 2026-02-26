#include "wifi_manager.h"

#include "mimi_config.h"

static const char           *TAG                  = "wifi";
static wifi_scan_result_cb_t s_scan_result_cb     = NULL;
static void                 *s_scan_result_cb_ctx = NULL;

void wifi_manager_set_scan_result_cb(wifi_scan_result_cb_t cb, void *user_data)
{
    s_scan_result_cb     = cb;
    s_scan_result_cb_ctx = user_data;
}

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)

#include "tal_wifi.h"

#ifndef WIFI_SSID_LEN
#define WIFI_SSID_LEN 32
#endif

#ifndef WIFI_PASSWD_LEN
#define WIFI_PASSWD_LEN 64
#endif

static volatile bool s_connected                        = false;
static bool          s_wifi_inited                      = false;
static char          s_ip_str[40]                       = "0.0.0.0";
static char          s_target_ssid[WIFI_SSID_LEN + 1]   = {0};
static char          s_target_pass[WIFI_PASSWD_LEN + 1] = {0};
static volatile bool s_retry_pending                    = false;
static uint32_t      s_retry_count                      = 0;
static uint64_t      s_next_retry_ms                    = 0;

static void reset_link_state(void)
{
    s_connected = false;
    snprintf(s_ip_str, sizeof(s_ip_str), "0.0.0.0");
}

static void reset_retry_state(void)
{
    s_retry_pending = false;
    s_retry_count   = 0;
    s_next_retry_ms = 0;
}

static uint32_t wifi_retry_delay_ms(uint32_t retry_count)
{
    uint32_t delay = MIMI_WIFI_RETRY_BASE_MS;
    while (retry_count > 0 && delay < MIMI_WIFI_RETRY_MAX_MS) {
        if (delay > (MIMI_WIFI_RETRY_MAX_MS / 2)) {
            delay = MIMI_WIFI_RETRY_MAX_MS;
            break;
        }
        delay <<= 1;
        retry_count--;
    }
    if (delay > MIMI_WIFI_RETRY_MAX_MS) {
        delay = MIMI_WIFI_RETRY_MAX_MS;
    }
    return delay;
}

static void schedule_wifi_retry(const char *reason)
{
    if (s_target_ssid[0] == '\0') {
        return;
    }
    if (s_retry_count >= MIMI_WIFI_MAX_RETRY) {
        s_retry_pending = false;
        MIMI_LOGW(TAG, "wifi retry exhausted (%u), reason=%s", (unsigned)MIMI_WIFI_MAX_RETRY,
                  reason ? reason : "unknown");
        return;
    }

    uint32_t delay_ms = wifi_retry_delay_ms(s_retry_count);
    s_next_retry_ms   = tal_time_get_posix_ms() + delay_ms;
    s_retry_pending   = true;
    MIMI_LOGW(TAG, "schedule wifi retry %u/%u in %u ms, reason=%s", (unsigned)(s_retry_count + 1),
              (unsigned)MIMI_WIFI_MAX_RETRY, (unsigned)delay_ms, reason ? reason : "unknown");
}

static void update_ip_from_wifi(void)
{
    NW_IP_S ip = {0};
    if (tal_wifi_get_ip(WF_STATION, &ip) != OPRT_OK) {
        snprintf(s_ip_str, sizeof(s_ip_str), "0.0.0.0");
        return;
    }

#ifdef nwipstr
    snprintf(s_ip_str, sizeof(s_ip_str), "%s", ip.nwipstr);
#else
    snprintf(s_ip_str, sizeof(s_ip_str), "%s", ip.ip);
#endif
}

static void wifi_event_callback(WF_EVENT_E event, void *arg)
{
    (void)arg;

    switch (event) {
    case WFE_CONNECTED:
        s_connected = true;
        reset_retry_state();
        update_ip_from_wifi();
        MIMI_LOGI(TAG, "wifi connected, ip=%s", s_ip_str);
        break;

    case WFE_CONNECT_FAILED:
        reset_link_state();
        MIMI_LOGW(TAG, "wifi connect failed");
        schedule_wifi_retry("connect_failed");
        break;

    case WFE_DISCONNECTED:
        reset_link_state();
        MIMI_LOGW(TAG, "wifi disconnected");
        schedule_wifi_retry("disconnected");
        break;

    default:
        break;
    }
}

OPERATE_RET wifi_manager_init(void)
{
    reset_link_state();
    if (s_wifi_inited) {
        return OPRT_OK;
    }

    OPERATE_RET rt = tal_wifi_init(wifi_event_callback);
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "tal_wifi_init failed: %d", rt);
        return rt;
    }

    s_wifi_inited = true;
    return OPRT_OK;
}

OPERATE_RET wifi_manager_start(void)
{
    OPERATE_RET rt = wifi_manager_init();
    if (rt != OPRT_OK) {
        return rt;
    }

    char ssid[WIFI_SSID_LEN + 1]   = {0};
    char pass[WIFI_PASSWD_LEN + 1] = {0};

    (void)mimi_kv_get_string(MIMI_NVS_WIFI, MIMI_NVS_KEY_SSID, ssid, sizeof(ssid));
    (void)mimi_kv_get_string(MIMI_NVS_WIFI, MIMI_NVS_KEY_PASS, pass, sizeof(pass));

    if (ssid[0] == '\0' && MIMI_SECRET_WIFI_SSID[0] != '\0') {
        snprintf(ssid, sizeof(ssid), "%s", MIMI_SECRET_WIFI_SSID);
        snprintf(pass, sizeof(pass), "%s", MIMI_SECRET_WIFI_PASS);
    }

    if (ssid[0] == '\0') {
        return OPRT_NOT_FOUND;
    }

    reset_link_state();
    reset_retry_state();

    snprintf(s_target_ssid, sizeof(s_target_ssid), "%s", ssid);
    snprintf(s_target_pass, sizeof(s_target_pass), "%s", pass);

    // Keep the same visible connect print style as official STA example.
    PR_NOTICE("connect wifi ssid: %s", s_target_ssid);

    rt = tal_wifi_set_work_mode(WWM_STATION);
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "tal_wifi_set_work_mode station failed: %d", rt);
        return rt;
    }

    MIMI_LOGI(TAG, "connecting wifi ssid=%s", s_target_ssid);
    rt = tal_wifi_station_connect((int8_t *)ssid, (int8_t *)pass);
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "tal_wifi_station_connect failed: %d", rt);
        schedule_wifi_retry("connect_call_failed");
    }

    return OPRT_OK;
}

OPERATE_RET wifi_manager_wait_connected(uint32_t timeout_ms)
{
    uint64_t start = tal_time_get_posix_ms();
    while (1) {
        if (s_connected) {
            update_ip_from_wifi();
            return OPRT_OK;
        }

        uint64_t now = tal_time_get_posix_ms();
        if (s_retry_pending && now >= s_next_retry_ms) {
            s_retry_pending = false;
            if (s_retry_count < MIMI_WIFI_MAX_RETRY) {
                s_retry_count++;
                MIMI_LOGI(TAG, "retrying wifi connect attempt %u/%u ssid=%s", (unsigned)s_retry_count,
                          (unsigned)MIMI_WIFI_MAX_RETRY, s_target_ssid);
                OPERATE_RET rt = tal_wifi_station_connect((int8_t *)s_target_ssid, (int8_t *)s_target_pass);
                if (rt != OPRT_OK) {
                    MIMI_LOGW(TAG, "retry connect call failed: %d", rt);
                    schedule_wifi_retry("retry_call_failed");
                }
            }
        }

        if (timeout_ms != UINT32_MAX) {
            if (now > start && (now - start) >= timeout_ms) {
                s_connected = false;
                snprintf(s_ip_str, sizeof(s_ip_str), "0.0.0.0");
                return OPRT_TIMEOUT;
            }
        }

        tal_system_sleep(200);
    }
}

bool wifi_manager_is_connected(void)
{
    if (s_connected) {
        update_ip_from_wifi();
    }

    return s_connected;
}

const char *wifi_manager_get_ip(void)
{
    if (wifi_manager_is_connected()) {
        return s_ip_str;
    }

    return "0.0.0.0";
}

const char *wifi_manager_get_target_ssid(void)
{
    return s_target_ssid[0] ? s_target_ssid : "<empty>";
}

OPERATE_RET wifi_manager_set_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password) {
        return OPRT_INVALID_PARM;
    }

    OPERATE_RET rt = mimi_kv_set_string(MIMI_NVS_WIFI, MIMI_NVS_KEY_SSID, ssid);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = mimi_kv_set_string(MIMI_NVS_WIFI, MIMI_NVS_KEY_PASS, password);
    if (rt != OPRT_OK) {
        return rt;
    }

    return OPRT_OK;
}

OPERATE_RET wifi_manager_scan_and_print(void)
{
    OPERATE_RET rt = wifi_manager_init();
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "wifi scan init failed: %d", rt);
        return rt;
    }

    WF_WK_MD_E mode = WWM_POWERDOWN;
    rt              = tal_wifi_get_work_mode(&mode);
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "wifi scan get mode failed: %d", rt);
        return rt;
    }
    if (mode != WWM_STATION && mode != WWM_STATIONAP) {
        rt = tal_wifi_set_work_mode(WWM_STATION);
        if (rt != OPRT_OK) {
            MIMI_LOGW(TAG, "wifi scan set station mode failed: %d", rt);
            return rt;
        }
        tal_system_sleep(200);
    }

    AP_IF_S *ap_list = NULL;
    uint32_t ap_num  = 0;

    for (int attempt = 0; attempt < 2; attempt++) {
        rt = tal_wifi_all_ap_scan(&ap_list, &ap_num);
        if (rt == OPRT_OK && ap_num > 0) {
            break;
        }

        if (ap_list) {
            (void)tal_wifi_release_ap(ap_list);
            ap_list = NULL;
        }
        ap_num = 0;

        if (attempt == 0) {
            tal_system_sleep(300);
        }
    }

    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "wifi scan failed: %d", rt);
        return rt;
    }

    if (ap_num == 0 || !ap_list) {
        MIMI_LOGW(TAG, "wifi scan found 0 ap(s)");
        return OPRT_NOT_FOUND;
    }

    MIMI_LOGI(TAG, "wifi scan found %u ap(s)", (unsigned)ap_num);

    for (uint32_t i = 0; i < ap_num; i++) {
        AP_IF_S *ap                      = &ap_list[i];
        char     ssid[WIFI_SSID_LEN + 1] = {0};
        size_t   ssid_len                = ap->s_len;
        if (ssid_len == 0) {
            ssid_len = strnlen((const char *)ap->ssid, WIFI_SSID_LEN);
        }
        if (ssid_len > WIFI_SSID_LEN) {
            ssid_len = WIFI_SSID_LEN;
        }
        if (ssid_len > 0) {
            memcpy(ssid, ap->ssid, ssid_len);
            ssid[ssid_len] = '\0';
        }

        char bssid[18] = {0};
        snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X", ap->bssid[0], ap->bssid[1], ap->bssid[2],
                 ap->bssid[3], ap->bssid[4], ap->bssid[5]);

        MIMI_LOGI(TAG, "ap[%u] ssid=%s ch=%u rssi=%d sec=%u bssid=%s", (unsigned)i, ssid[0] ? ssid : "<hidden>",
                  (unsigned)ap->channel, (int)ap->rssi, (unsigned)ap->security, bssid);

        if (s_scan_result_cb) {
            s_scan_result_cb(i, ap_num, ssid[0] ? ssid : "<hidden>", ap->channel, (int)ap->rssi, ap->security, bssid,
                             s_scan_result_cb_ctx);
        }
    }

    (void)tal_wifi_release_ap(ap_list);
    return OPRT_OK;
}

#else

OPERATE_RET wifi_manager_init(void)
{
    MIMI_LOGW(TAG, "wifi disabled (ENABLE_WIFI!=1)");
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET wifi_manager_start(void)
{
    return OPRT_NOT_SUPPORTED;
}

OPERATE_RET wifi_manager_wait_connected(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return OPRT_NOT_SUPPORTED;
}

bool wifi_manager_is_connected(void)
{
    return false;
}

const char *wifi_manager_get_ip(void)
{
    return "0.0.0.0";
}

OPERATE_RET wifi_manager_set_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password) {
        return OPRT_INVALID_PARM;
    }
    // still allow persisting creds even if WiFi feature is off
    OPERATE_RET rt = mimi_kv_set_string(MIMI_NVS_WIFI, MIMI_NVS_KEY_SSID, ssid);
    if (rt != OPRT_OK) {
        return rt;
    }
    return mimi_kv_set_string(MIMI_NVS_WIFI, MIMI_NVS_KEY_PASS, password);
}

OPERATE_RET wifi_manager_scan_and_print(void)
{
    MIMI_LOGW(TAG, "wifi scan disabled (ENABLE_WIFI!=1)");
    return OPRT_NOT_SUPPORTED;
}

#endif
