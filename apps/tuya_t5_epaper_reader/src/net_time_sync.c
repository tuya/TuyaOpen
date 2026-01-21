#include "net_time_sync.h"

#include "http_client_interface.h"
#include "netmgr.h"
#include "tal_api.h"
#include "tal_time_service.h"
#include "tuya_register_center.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
#include "netconn_wired.h"
#endif

#ifndef EXAMPLE_TIME_SERVER_URL
#define EXAMPLE_TIME_SERVER_URL "www.baidu.com"
#endif

#ifndef EXAMPLE_TIME_SERVER_PATH
#define EXAMPLE_TIME_SERVER_PATH "/"
#endif

#ifndef EXAMPLE_TIME_HTTP_TIMEOUT_MS
#define EXAMPLE_TIME_HTTP_TIMEOUT_MS 8000
#endif

#ifndef EXAMPLE_TIMEZONE_OFFSET_HOURS
#define EXAMPLE_TIMEZONE_OFFSET_HOURS 8
#endif

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#ifdef CONFIG_EPAPER_WIFI_SSID
#define EXAMPLE_WIFI_SSID CONFIG_EPAPER_WIFI_SSID
#endif
#ifdef CONFIG_EPAPER_WIFI_PSWD
#define EXAMPLE_WIFI_PSWD CONFIG_EPAPER_WIFI_PSWD
#endif

#ifndef EXAMPLE_WIFI_SSID
#ifdef DEFAULT_WIFI_SSID
#define EXAMPLE_WIFI_SSID DEFAULT_WIFI_SSID
#else
#define EXAMPLE_WIFI_SSID "your-ssid-****"
#endif
#endif

#ifndef EXAMPLE_WIFI_PSWD
#ifdef DEFAULT_WIFI_PSWD
#define EXAMPLE_WIFI_PSWD DEFAULT_WIFI_PSWD
#else
#define EXAMPLE_WIFI_PSWD "your-pswd-****"
#endif
#endif
#endif

static volatile int sg_time_synced = 0;

static int parse_http_date(const char *date_str, struct tm *tm_time)
{
    const char *p = strstr(date_str, "Date:");
    if (p) {
        p += 5;
        while (*p == ' ') {
            p++;
        }
    } else {
        p = date_str;
    }

    char month_str[4] = {0};
    char weekday[4]   = {0};

    int parsed = sscanf(p, "%3s, %d %3s %d %d:%d:%d", weekday, &tm_time->tm_mday, month_str, &tm_time->tm_year,
                        &tm_time->tm_hour, &tm_time->tm_min, &tm_time->tm_sec);

    if (parsed != 7) {
        PR_ERR("Failed to parse date string: %s", date_str);
        return -1;
    }

    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    tm_time->tm_mon      = -1;
    for (int i = 0; i < 12; i++) {
        if (strcmp(month_str, months[i]) == 0) {
            tm_time->tm_mon = i;
            break;
        }
    }

    if (tm_time->tm_mon == -1) {
        PR_ERR("Invalid month: %s", month_str);
        return -1;
    }

    tm_time->tm_year -= 1900;
    tm_time->tm_isdst = 0;

    return 0;
}

static int sync_time_from_http(void)
{
    http_client_response_t http_response = {0};

    http_client_header_t headers[] = {
        {.key = "User-Agent", .value = "Mozilla/5.0"},
        {.key = "Connection", .value = "close"},
    };

    http_client_status_t http_status = http_client_request(
        &(const http_client_request_t){
            .cacert        = NULL,
            .cacert_len    = 0,
            .host          = EXAMPLE_TIME_SERVER_URL,
            .port          = 80,
            .method        = "GET",
            .path          = EXAMPLE_TIME_SERVER_PATH,
            .headers       = headers,
            .headers_count = 2,
            .body          = (const uint8_t *)"",
            .body_length   = 0,
            .timeout_ms    = EXAMPLE_TIME_HTTP_TIMEOUT_MS,
        },
        &http_response);

    if (HTTP_CLIENT_SUCCESS != http_status) {
        PR_ERR("HTTP request failed: %d", http_status);
        http_client_free(&http_response);
        return OPRT_COM_ERROR;
    }

    if (http_response.headers && http_response.headers_length > 0) {
        char *headers_str = (char *)tal_malloc(http_response.headers_length + 1);
        if (headers_str) {
            memcpy(headers_str, http_response.headers, http_response.headers_length);
            headers_str[http_response.headers_length] = '\0';

            char *date_line = strstr(headers_str, "Date:");
            if (!date_line) {
                date_line = strstr(headers_str, "date:");
            }

            if (date_line) {
                char *line_end = strstr(date_line, "\r\n");
                if (line_end) {
                    *line_end = '\0';
                }

                struct tm tm_time = {0};
                if (parse_http_date(date_line, &tm_time) == 0) {
                    time_t server_time_gmt   = mktime(&tm_time);
                    time_t server_time_local = server_time_gmt + (EXAMPLE_TIMEZONE_OFFSET_HOURS * 3600);
                    tal_time_set_posix(server_time_local, 0);
                    sg_time_synced = 1;
                }
            }

            tal_free(headers_str);
        }
    }

    http_client_free(&http_response);
    return OPRT_OK;
}

static OPERATE_RET link_status_callback(void *data)
{
    static netmgr_status_e last_status    = NETMGR_LINK_DOWN;
    static int             sync_attempted = 0;
    netmgr_status_e        status         = (netmgr_status_e)data;

    if (status == last_status) {
        return OPRT_OK;
    }

    last_status = status;

    if (status == NETMGR_LINK_UP && !sync_attempted) {
        sync_attempted = 1;
        tal_system_sleep(2000);
        sync_time_from_http();
    }

    return OPRT_OK;
}

static void example_basic_runtime_init(void)
{
    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key  = "dflfuap134ddlduq",
    });
    tal_sw_timer_init();
    tal_workq_init();
    tuya_tls_init();
    tuya_register_center_init();
}

static void example_set_default_time_if_needed(void)
{
    if (sg_time_synced) {
        return;
    }

    struct tm default_time = {
        .tm_year  = 2025 - 1900,
        .tm_mon   = 11,
        .tm_mday  = 20,
        .tm_hour  = 21,
        .tm_min   = 0,
        .tm_sec   = 0,
        .tm_isdst = 0,
    };
    time_t default_timestamp = mktime(&default_time);
    tal_time_set_posix(default_timestamp, 0);
}

static BOOL_T wifi_cred_is_placeholder(const char *s)
{
    if (!s) {
        return TRUE;
    }
    if (s[0] == '\0') {
        return TRUE;
    }
    if (strstr(s, "****") != NULL) {
        return TRUE;
    }
    return FALSE;
}

OPERATE_RET example_time_sync_on_startup(uint32_t wait_timeout_ms)
{
    sg_time_synced = 0;

    example_basic_runtime_init();
    tal_event_subscribe(EVENT_LINK_STATUS_CHG, "sd_time", link_status_callback, SUBSCRIBE_TYPE_NORMAL);

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
    TUYA_LwIP_Init();
#endif

    netmgr_type_e type = 0;
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    type |= NETCONN_WIFI;
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    type |= NETCONN_WIRED;
#endif
    if (type == 0) {
        return OPRT_NOT_SUPPORTED;
    }

    netmgr_init(type);

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    if (!wifi_cred_is_placeholder(EXAMPLE_WIFI_SSID)) {
        netconn_wifi_info_t wifi_info = {0};
        strncpy(wifi_info.ssid, EXAMPLE_WIFI_SSID, sizeof(wifi_info.ssid) - 1);
        strncpy(wifi_info.pswd, EXAMPLE_WIFI_PSWD, sizeof(wifi_info.pswd) - 1);
        netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info);
    } else {
        PR_WARN("Wi-Fi credentials not set. Configure CONFIG_EPAPER_WIFI_SSID/PSWD to enable network.");
    }
#endif

    netmgr_status_e status = NETMGR_LINK_DOWN;
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    if (netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_STATUS, &status) == OPRT_OK && status == NETMGR_LINK_UP) {
        tal_system_sleep(2000);
        sync_time_from_http();
    }
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    status = NETMGR_LINK_DOWN;
    if (netmgr_conn_get(NETCONN_WIRED, NETCONN_CMD_STATUS, &status) == OPRT_OK && status == NETMGR_LINK_UP) {
        tal_system_sleep(2000);
        sync_time_from_http();
    }
#endif

    if (wait_timeout_ms > 0) {
        uint32_t waited_ms = 0;
        while (!sg_time_synced && waited_ms < wait_timeout_ms) {
            tal_system_sleep(1000);
            waited_ms += 1000;
        }
    }

    example_set_default_time_if_needed();
    return sg_time_synced ? OPRT_OK : OPRT_TIMEOUT;
}
