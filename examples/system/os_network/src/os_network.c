/**
 * @file os_network.c
 * @brief CLI diagnostics for the TAL network API.
 *
 * The example connects Wi-Fi directly through TAL and then exercises
 * tal_net_gethostbyname(), TCP connect/send/recv and UDP send without using
 * Tuya provisioning, the cloud client, HTTP client or TLS.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tal_cli.h"
#include "tal_network.h"
#include "tal_network_register.h"
#include "tkl_output.h"

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "tal_wifi.h"
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define NET_DEFAULT_TIMEOUT_MS 5000U
#define NET_RX_BUFFER_SIZE     1024U

static bool s_network_initialized;

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
static bool s_wifi_initialized;

static void net_wifi_event_(WF_EVENT_E event, void *arg)
{
    (void)arg;
    PR_INFO("wifi event: %d", event);
}
#endif

static void net_echo_(const char *format, ...)
{
    char    line[256];
    va_list args;

    va_start(args, format);
    (void)vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    tal_cli_echo(line);
}

static bool net_parse_u16_(const char *text, uint16_t *value)
{
    uint32_t parsed = 0U;

    if (text == NULL || text[0] == '\0' || value == NULL) {
        return false;
    }
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        uint32_t digit;
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
        digit = (uint32_t)(*cursor - '0');
        if (parsed > (UINT16_MAX - digit) / 10U) {
            return false;
        }
        parsed = parsed * 10U + digit;
    }
    if (parsed == 0U) {
        return false;
    }
    *value = (uint16_t)parsed;
    return true;
}

static int net_send_all_(int fd, const uint8_t *data, size_t size)
{
    size_t sent = 0U;

    while (sent < size) {
        int result = tal_net_send(fd, data + sent, (uint32_t)(size - sent));
        if (result <= 0) {
            return result;
        }
        sent += (size_t)result;
    }
    return (int)sent;
}

static void net_drain_(int fd)
{
    uint8_t scratch[256];
    uint32_t total = 0U;

    while (total < 65536U) {
        int result = tal_net_recv(fd, scratch, sizeof(scratch));
        if (result <= 0) {
            break;
        }
        total += (uint32_t)result;
    }
}

static void net_ensure_card_(void)
{
    if (!s_network_initialized) {
        OPERATE_RET rt = tal_network_card_init();
        if (rt == OPRT_OK) {
            s_network_initialized = true;
        } else {
            net_echo_("network card init failed: %d", rt);
        }
    }
}

static bool net_resolve_(const char *host, TUYA_IP_ADDR_T *addr)
{
    TUYA_IP_ADDR_T parsed = tal_net_str2addr(host);
    if (parsed != 0U && parsed != TY_IPADDR_BROADCAST) {
        *addr = parsed;
        net_echo_("resolve %s -> %s (literal)", host, tal_net_addr2str(parsed));
        return true;
    }

    OPERATE_RET rt = tal_net_gethostbyname(host, addr);
    int errno_value = tal_net_get_errno();
    if (rt != OPRT_OK) {
        net_echo_("DNS %s FAIL ret=%d errno=%d", host, rt, errno_value);
        return false;
    }
    net_echo_("DNS %s PASS addr=%s", host, tal_net_addr2str(*addr));
    return true;
}

static void net_wifi_command_(int argc, char *argv[])
{
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    OPERATE_RET rt;

    if (argc != 3 || argv == NULL) {
        net_echo_("Usage: net_wifi <ssid> <password>");
        return;
    }
    net_ensure_card_();

    if (!s_wifi_initialized) {
        rt = tal_wifi_init(net_wifi_event_);
        if (rt != OPRT_OK) {
            net_echo_("tal_wifi_init FAIL ret=%d", rt);
            return;
        }
        s_wifi_initialized = true;
    }

    rt = tal_wifi_set_work_mode(WWM_STATION);
    if (rt != OPRT_OK) {
        net_echo_("set station mode FAIL ret=%d", rt);
        return;
    }

    net_echo_("wifi connect %s ...", argv[1]);
    rt = tal_wifi_station_connect((int8_t *)argv[1], (int8_t *)argv[2]);
    if (rt != OPRT_OK) {
        net_echo_("wifi connect FAIL ret=%d", rt);
        return;
    }

    NW_IP_S ip = {0};
    rt = tal_wifi_get_ip(WF_STATION, &ip);
    net_echo_("wifi connect PASS ip=%s mask=%s gw=%s ret=%d", ip.ip, ip.mask, ip.gw, rt);
    tal_system_sleep(500U);
#else
    (void)argc;
    (void)argv;
    net_echo_("net_wifi: ENABLE_WIFI is not enabled");
#endif
}

static void net_dns_command_(int argc, char *argv[])
{
    TUYA_IP_ADDR_T addr = 0;

    if (argc != 2 || argv == NULL) {
        net_echo_("Usage: net_dns <host>");
        return;
    }
    net_ensure_card_();
    (void)net_resolve_(argv[1], &addr);
}

static void net_tcp_command_(int argc, char *argv[])
{
    uint16_t port;
    TUYA_IP_ADDR_T addr = 0;

    if (argc != 3 || argv == NULL || !net_parse_u16_(argv[2], &port)) {
        net_echo_("Usage: net_tcp <host> <port>");
        return;
    }
    net_ensure_card_();
    if (!net_resolve_(argv[1], &addr)) {
        return;
    }

    int fd = tal_net_socket_create(PROTOCOL_TCP);
    if (fd < 0) {
        net_echo_("socket create FAIL fd=%d errno=%d", fd, tal_net_get_errno());
        return;
    }
    (void)tal_net_set_timeout(fd, NET_DEFAULT_TIMEOUT_MS, TRANS_SEND);
    (void)tal_net_set_timeout(fd, NET_DEFAULT_TIMEOUT_MS, TRANS_RECV);

    TUYA_ERRNO result = tal_net_connect(fd, addr, port);
    int errno_value = tal_net_get_errno();
    if (result < 0) {
        net_echo_("TCP %s:%u connect FAIL ret=%d errno=%d", argv[1], port, result, errno_value);
    } else {
        net_echo_("TCP %s:%u connect PASS ret=%d", argv[1], port, result);
    }
    (void)tal_net_close(fd);
}

static void net_http_command_(int argc, char *argv[])
{
    uint16_t port = 80U;
    const char *path = "/";
    TUYA_IP_ADDR_T addr = 0;
    uint8_t request[256];
    uint8_t response[NET_RX_BUFFER_SIZE + 1U];

    if (argc < 2 || argc > 4 || argv == NULL) {
        net_echo_("Usage: net_http <host> [port] [path]");
        return;
    }
    if (argc >= 3 && !net_parse_u16_(argv[2], &port)) {
        net_echo_("net_http: invalid port");
        return;
    }
    if (argc >= 4) {
        path = argv[3];
    }
    net_ensure_card_();
    if (!net_resolve_(argv[1], &addr)) {
        return;
    }

    int fd = tal_net_socket_create(PROTOCOL_TCP);
    if (fd < 0) {
        net_echo_("socket create FAIL fd=%d errno=%d", fd, tal_net_get_errno());
        return;
    }
    (void)tal_net_set_timeout(fd, NET_DEFAULT_TIMEOUT_MS, TRANS_SEND);
    (void)tal_net_set_timeout(fd, NET_DEFAULT_TIMEOUT_MS, TRANS_RECV);

    TUYA_ERRNO result = tal_net_connect(fd, addr, port);
    if (result < 0) {
        net_echo_("HTTP connect FAIL ret=%d errno=%d", result, tal_net_get_errno());
        (void)tal_net_close(fd);
        return;
    }

    int written = snprintf((char *)request, sizeof(request), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
                           path, argv[1]);
    if (written <= 0 || (size_t)written >= sizeof(request)) {
        net_echo_("HTTP request build FAIL");
        (void)tal_net_close(fd);
        return;
    }

    int sent = net_send_all_(fd, request, (size_t)written);
    net_echo_("HTTP send ret=%d", sent);
    if (sent > 0) {
        int received = tal_net_recv(fd, response, NET_RX_BUFFER_SIZE);
        if (received > 0) {
            response[received] = '\0';
            net_echo_("HTTP recv PASS len=%d data=%.240s", received, response);
            net_drain_(fd);
        } else {
            net_echo_("HTTP recv FAIL ret=%d errno=%d", received, tal_net_get_errno());
        }
    }
    (void)tal_net_close(fd);
}

static void net_multi_http_command_(int argc, char *argv[])
{
    TUYA_IP_ADDR_T addresses[2] = {0};
    int fds[2] = {-1, -1};
    bool sent_ok[2] = {false, false};
    bool recv_ok[2] = {false, false};
    uint32_t total_start = tal_system_get_millisecond();

    if (argc != 3 || argv == NULL) {
        net_echo_("Usage: net_multi_http <host1> <host2>");
        return;
    }
    net_ensure_card_();
    for (unsigned i = 0U; i < 2U; ++i) {
        uint32_t start = tal_system_get_millisecond();
        bool resolved = net_resolve_(argv[i + 1U], &addresses[i]);
        net_echo_("multi_http dns %s ret=%d elapsed=%ums", argv[i + 1U], resolved ? 0 : -1,
                  (unsigned)(tal_system_get_millisecond() - start));
        if (!resolved) {
            goto cleanup;
        }
    }

    for (unsigned i = 0U; i < 2U; ++i) {
        fds[i] = tal_net_socket_create(PROTOCOL_TCP);
        if (fds[i] < 0) {
            net_echo_("multi_http socket %u create FAIL errno=%d", i, tal_net_get_errno());
            goto cleanup;
        }
        (void)tal_net_set_timeout(fds[i], NET_DEFAULT_TIMEOUT_MS, TRANS_SEND);
        (void)tal_net_set_timeout(fds[i], NET_DEFAULT_TIMEOUT_MS, TRANS_RECV);
    }

    /* Keep both sockets open and connected before sending any request. */
    for (unsigned i = 0U; i < 2U; ++i) {
        uint32_t start = tal_system_get_millisecond();
        TUYA_ERRNO result = tal_net_connect(fds[i], addresses[i], 80U);
        uint32_t elapsed = tal_system_get_millisecond() - start;
        if (result < 0) {
            net_echo_("multi_http connect %s FAIL ret=%d errno=%d elapsed=%ums", argv[i + 1U], result,
                      tal_net_get_errno(), (unsigned)elapsed);
            goto cleanup;
        }
        net_echo_("multi_http connect %s PASS fd=%d elapsed=%ums", argv[i + 1U], fds[i],
                  (unsigned)elapsed);
    }

    for (unsigned i = 0U; i < 2U; ++i) {
        char request[256];
        int length = snprintf(request, sizeof(request),
                              "GET / HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
                              argv[i + 1U]);
        if (length <= 0 || (size_t)length >= sizeof(request)) {
            continue;
        }
        uint32_t start = tal_system_get_millisecond();
        int sent = net_send_all_(fds[i], (const uint8_t *)request, (size_t)length);
        uint32_t elapsed = tal_system_get_millisecond() - start;
        sent_ok[i] = sent == length;
        net_echo_("multi_http send %s ret=%d elapsed=%ums", argv[i + 1U], sent, (unsigned)elapsed);
    }

    for (unsigned i = 0U; i < 2U; ++i) {
        uint8_t response[NET_RX_BUFFER_SIZE + 1U];
        size_t total = 0U;
        bool header_complete = false;
        int last_result = 0;
        uint32_t start = tal_system_get_millisecond();
        while (total < NET_RX_BUFFER_SIZE) {
            last_result = tal_net_recv(fds[i], response + total, (uint32_t)(NET_RX_BUFFER_SIZE - total));
            if (last_result <= 0) {
                break;
            }
            total += (size_t)last_result;
            response[total] = '\0';
            if (strstr((const char *)response, "\r\n\r\n") != NULL) {
                header_complete = true;
                break;
            }
        }
        uint32_t elapsed = tal_system_get_millisecond() - start;
        if (header_complete) {
            recv_ok[i] = strstr((const char *)response, " 200 ") != NULL;
            net_echo_("multi_http recv %s len=%u header=1 status200=%u elapsed=%ums", argv[i + 1U],
                      (unsigned)total, recv_ok[i] ? 1U : 0U, (unsigned)elapsed);
            net_drain_(fds[i]);
        } else {
            net_echo_("multi_http recv %s FAIL len=%u ret=%d errno=%d elapsed=%ums", argv[i + 1U],
                      (unsigned)total, last_result, tal_net_get_errno(), (unsigned)elapsed);
        }
    }

    net_echo_("MULTI_HTTP_RESULT sent=%u/%u recv200=%u/%u total=%ums",
              sent_ok[0] ? 1U : 0U, sent_ok[1] ? 1U : 0U,
              recv_ok[0] ? 1U : 0U, recv_ok[1] ? 1U : 0U,
              (unsigned)(tal_system_get_millisecond() - total_start));

cleanup:
    for (unsigned i = 0U; i < 2U; ++i) {
        if (fds[i] >= 0) {
            (void)tal_net_close(fds[i]);
        }
    }
}

static void net_ap_server_command_(int argc, char *argv[])
{
    uint16_t tcp_port = 6668U;
    uint16_t udp_port = 6667U;
    TUYA_IP_ADDR_T peer_addr = 0;
    uint16_t peer_port = 0;
    uint8_t buffer[128];

    if (argc != 1 && argc != 3) {
        net_echo_("Usage: net_ap_server [tcp_port udp_port]");
        return;
    }
    if (argc == 3 &&
        (!net_parse_u16_(argv[1], &tcp_port) || !net_parse_u16_(argv[2], &udp_port))) {
        net_echo_("net_ap_server: invalid port");
        return;
    }
    net_ensure_card_();

    int tcp_fd = tal_net_socket_create(PROTOCOL_TCP);
    int udp_fd = tal_net_socket_create(PROTOCOL_UDP);
    if (tcp_fd < 0 || udp_fd < 0) {
        net_echo_("AP_SERVER create FAIL tcp=%d udp=%d errno=%d", tcp_fd, udp_fd, tal_net_get_errno());
        goto cleanup;
    }
    if (tal_net_bind(tcp_fd, 0U, tcp_port) != OPRT_OK ||
        tal_net_listen(tcp_fd, 2) != OPRT_OK ||
        tal_net_bind(udp_fd, 0U, udp_port) != OPRT_OK) {
        net_echo_("AP_SERVER listen FAIL tcp=%d udp=%d errno=%d", tcp_port, udp_port,
                  tal_net_get_errno());
        goto cleanup;
    }
    (void)tal_net_set_timeout(tcp_fd, 15000U, TRANS_RECV);
    (void)tal_net_set_timeout(udp_fd, 15000U, TRANS_RECV);
    net_echo_("AP_SERVER_READY tcp=%u udp=%u", tcp_port, udp_port);

    int accepted = tal_net_accept(tcp_fd, &peer_addr, &peer_port);
    if (accepted < 0) {
        net_echo_("AP_SERVER accept FAIL ret=%d errno=%d", accepted, tal_net_get_errno());
        goto cleanup;
    }
    net_echo_("AP_SERVER_ACCEPT fd=%d peer=%s:%u", accepted, tal_net_addr2str(peer_addr), peer_port);

    int received = tal_net_recvfrom(udp_fd, buffer, sizeof(buffer), &peer_addr, &peer_port);
    if (received < 0) {
        net_echo_("AP_SERVER udp recv FAIL ret=%d errno=%d", received, tal_net_get_errno());
    } else {
        net_echo_("AP_SERVER_UDP len=%d peer=%s:%u", received, tal_net_addr2str(peer_addr), peer_port);
    }
    (void)tal_net_close(accepted);

cleanup:
    if (tcp_fd >= 0) {
        (void)tal_net_close(tcp_fd);
    }
    if (udp_fd >= 0) {
        (void)tal_net_close(udp_fd);
    }
}

static void net_udp_command_(int argc, char *argv[])
{
    uint16_t port;
    TUYA_IP_ADDR_T addr = 0;

    if (argc != 4 || argv == NULL || !net_parse_u16_(argv[2], &port)) {
        net_echo_("Usage: net_udp <host> <port> <text>");
        return;
    }
    net_ensure_card_();
    if (!net_resolve_(argv[1], &addr)) {
        return;
    }

    int fd = tal_net_socket_create(PROTOCOL_UDP);
    if (fd < 0) {
        net_echo_("UDP socket create FAIL fd=%d errno=%d", fd, tal_net_get_errno());
        return;
    }
    int sent = tal_net_send_to(fd, argv[3], (uint32_t)strlen(argv[3]), addr, port);
    net_echo_("UDP send %s:%u ret=%d errno=%d", argv[1], port, sent, tal_net_get_errno());
    (void)tal_net_close(fd);
}

static const cli_cmd_t s_commands[] = {
    {.name = "net_wifi", .help = "Connect Wi-Fi: <ssid> <password>", .func = net_wifi_command_},
    {.name = "net_dns", .help = "Resolve host: <host>", .func = net_dns_command_},
    {.name = "net_tcp", .help = "TCP connect: <host> <port>", .func = net_tcp_command_},
    {.name = "net_http", .help = "Raw HTTP GET: <host> [port] [path]", .func = net_http_command_},
    {.name = "net_multi_http", .help = "Two concurrent HTTP sockets: <host1> <host2>", .func = net_multi_http_command_},
    {.name = "net_ap_server", .help = "Dual AP listeners: [tcp_port udp_port]", .func = net_ap_server_command_},
    {.name = "net_udp", .help = "UDP send: <host> <port> <text>", .func = net_udp_command_},
};

void user_main(void)
{
    OPERATE_RET rt;

    tal_time_service_init();
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    tal_sw_timer_init();
    tal_workq_init();

    PR_NOTICE("TAL network diagnostics");
    PR_NOTICE("Commands: net_wifi, net_dns, net_tcp, net_http, net_multi_http, net_ap_server, net_udp");

    rt = tal_cli_init_with_uart(TUYA_UART_NUM_0);
    if (rt != OPRT_OK) {
        PR_ERR("tal_cli_init_with_uart failed: %d", rt);
        return;
    }
    rt = tal_cli_cmd_register(s_commands, sizeof(s_commands) / sizeof(s_commands[0]));
    if (rt != OPRT_OK) {
        PR_ERR("tal_cli_cmd_register failed: %d", rt);
        return;
    }

    while (1) {
        tal_system_sleep(1000U);
    }
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    user_main();
}
#else
static THREAD_HANDLE s_app_thread;

static void os_network_app_thread_(void *arg)
{
    (void)arg;
    user_main();
    tal_thread_delete(s_app_thread);
    s_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T config = {0};
    config.stackDepth = 1024U * 4U;
    config.priority   = THREAD_PRIO_1;
    config.thrdname   = "os_network";
    tal_thread_create_and_start(&s_app_thread, NULL, NULL, os_network_app_thread_, NULL, &config);
}
#endif
