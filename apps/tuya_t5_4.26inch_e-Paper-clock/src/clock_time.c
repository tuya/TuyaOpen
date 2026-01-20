#include <string.h>

#include "clock_time.h"

#include "clock_net.h"

#include "tal_system.h"
#include "tal_network.h"
#include "tuya_config.h"

static volatile BOOL_T g_time_inited = FALSE;
static volatile BOOL_T g_fallback_time_inited = FALSE;
static volatile SYS_TIME_T g_fallback_start_ms = 0;

static THREAD_HANDLE g_time_sync_thread = NULL;
static volatile clock_time_src_t g_time_src = CLOCK_TIME_SRC_DEFAULT;

static BOOL_T local_time_get(POSIX_TM_S *out_local)
{
    if (!out_local) {
        return FALSE;
    }

    if (tal_time_get_local_time_custom(0, out_local) != OPRT_OK) {
        return FALSE;
    }

    // If time isn't synced, many platforms return 1970-01-01.
    if ((out_local->tm_year + 1900) < 2020) {
        return FALSE;
    }

    return TRUE;
}

static void fallback_time_get(POSIX_TM_S *out_local)
{
    SYS_TIME_T now_ms = tal_system_get_millisecond();
    if (!g_fallback_time_inited) {
        g_fallback_time_inited = TRUE;
        g_fallback_start_ms = now_ms;
    }

    uint32_t elapsed_sec = (uint32_t)((uint32_t)now_ms - (uint32_t)g_fallback_start_ms) / 1000;
    uint32_t total_sec = 8U * 3600U + elapsed_sec;

    POSIX_TM_S tmp = {0};
    tmp.tm_hour = (int)((total_sec / 3600U) % 24U);
    tmp.tm_min = (int)((total_sec / 60U) % 60U);
    tmp.tm_sec = (int)(total_sec % 60U);
    *out_local = tmp;
}

void clock_time_service_init_once(void)
{
    if (g_time_inited) {
        return;
    }
    g_time_inited = TRUE;

    tal_time_service_init();
    tal_time_set_time_zone_seconds(EPD_CLOCK_TZ_SECONDS);
}

void clock_time_set_source(clock_time_src_t src)
{
    g_time_src = src;
}

clock_time_src_t clock_time_get_source(void)
{
    return g_time_src;
}

static uint32_t u32_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static OPERATE_RET ntp_sync_once(const char *server, TIME_T *out_posix)
{
    if (!server || !server[0] || !out_posix) {
        return OPRT_INVALID_PARM;
    }

    TUYA_IP_ADDR_T addr = 0;
    OPERATE_RET rt = tal_net_gethostbyname(server, &addr);
    if (rt != OPRT_OK || addr == 0) {
        // Fallback: allow passing a dotted IPv4 string.
        addr = tal_net_str2addr(server);
    }
    if (addr == 0) {
        PR_WARN("NTP resolve failed: %s (rt=%d)", server, rt);
        return OPRT_COM_ERROR;
    }

    int fd = tal_net_socket_create(PROTOCOL_UDP);
    if (fd < 0) {
        return OPRT_COM_ERROR;
    }

    tal_net_set_timeout(fd, 2000, TRANS_RECV);

    uint8_t req[48] = {0};
    req[0] = 0x1B; // LI=0, VN=3, Mode=3 (client)

    if (tal_net_send_to(fd, req, sizeof(req), addr, 123) <= 0) {
        tal_net_close(fd);
        PR_WARN("NTP send failed: %s", server);
        return OPRT_COM_ERROR;
    }

    uint8_t resp[48] = {0};
    TUYA_IP_ADDR_T from = 0;
    uint16_t from_port = 0;
    int r = tal_net_recvfrom(fd, resp, sizeof(resp), &from, &from_port);
    tal_net_close(fd);
    if (r < (int)sizeof(resp)) {
        PR_WARN("NTP recv failed: %s (r=%d)", server, r);
        return OPRT_COM_ERROR;
    }

    // Transmit Timestamp starts at byte 40 (seconds since 1900).
    uint32_t sec_1900 = u32_be(&resp[40]);
    const uint32_t NTP_UNIX_EPOCH_DELTA = 2208988800UL;
    if (sec_1900 < NTP_UNIX_EPOCH_DELTA) {
        return OPRT_COM_ERROR;
    }

    *out_posix = (TIME_T)(sec_1900 - NTP_UNIX_EPOCH_DELTA);
    return OPRT_OK;
}

static void time_sync_thread(void *arg)
{
    (void)arg;

#if defined(EPD_CLOCK_ENABLE_NTP_FALLBACK) && (EPD_CLOCK_ENABLE_NTP_FALLBACK == 1)
    PR_INFO("time_sync start (NTP fallback enabled, server=%s)", EPD_CLOCK_NTP_SERVER);
#else
    PR_INFO("time_sync start (NTP fallback disabled)");
#endif

#if defined(EPD_CLOCK_ENABLE_NTP_FALLBACK) && (EPD_CLOCK_ENABLE_NTP_FALLBACK == 1)
    const char *server = EPD_CLOCK_NTP_SERVER;
#else
    const char *server = NULL;
#endif

    SYS_TIME_T last_try_ms = 0;

    THREAD_HANDLE self = g_time_sync_thread;

    for (;;) {
        POSIX_TM_S tmp = {0};
        if (local_time_get(&tmp)) {
            PR_INFO("time_sync done (time already valid)");
            break;
        }

        // Only try when WiFi has an IP.
        clock_net_info_t net = {0};
        clock_net_info_get(&net);

        if (net.link != NETMGR_LINK_DOWN && net.ip[0] != '\0' && server && server[0] != '\0') {
            SYS_TIME_T now_ms = tal_system_get_millisecond();
            if ((uint32_t)(now_ms - last_try_ms) >= 15000U) {
                last_try_ms = now_ms;
                TIME_T posix = 0;
                if (ntp_sync_once(server, &posix) == OPRT_OK) {
                    tal_time_set_posix(posix, 2);
                    g_time_src = CLOCK_TIME_SRC_NTP;
                    PR_INFO("time_sync done (NTP ok)");
                    break;
                }
            }
        }

        tal_system_sleep(1000);
    }

    g_time_sync_thread = NULL;
    if (self) {
        tal_thread_delete(self);
    }
}

void clock_time_sync_start_once(void)
{
    if (g_time_sync_thread) {
        return;
    }

    THREAD_CFG_T cfg = {4096, 4, "time_sync"};
    OPERATE_RET ret = tal_thread_create_and_start(&g_time_sync_thread, NULL, NULL, time_sync_thread, NULL, &cfg);
    if (ret != OPRT_OK) {
        g_time_sync_thread = NULL;
    }
}

void clock_time_get(clock_ui_state_t *state_out)
{
    if (!state_out) {
        return;
    }

    POSIX_TM_S tmp = {0};
    BOOL_T synced = local_time_get(&tmp);
    if (!synced) {
        fallback_time_get(&tmp);
    }

    memset(state_out, 0, sizeof(*state_out));
    state_out->local = tmp;
    state_out->time_synced = synced;
    state_out->time_src = g_time_src;
}
