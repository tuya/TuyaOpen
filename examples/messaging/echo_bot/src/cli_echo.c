/**
 * @file cli_echo.c
 * @brief Minimal CLI for echo_bot: WiFi, channel switch, push_outbound, config_show, restart.
 */

#include "echo_cli.h"
#include "im_api.h"
#include "echo_config.h"
#include "tal_cli.h"
#include "wifi_manager.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool s_inited = false;

static void cli_echof(const char *fmt, ...)
{
    char line[512] = {0};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    tal_cli_echo(line);
}

static void mask_copy(const char *src, char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    if (!src || src[0] == '\0') {
        snprintf(out, out_size, "(empty)");
        return;
    }
    size_t len = strlen(src);
    if (len <= 4)
        snprintf(out, out_size, "****");
    else
        snprintf(out, out_size, "%.4s****", src);
}

typedef OPERATE_RET (*kv_getter_t)(const char *, const char *, char *, size_t);

static void print_cfg(const char *label, const char *ns, const char *key, const char *build_val, bool mask,
                      kv_getter_t getter)
{
    char kv_val[128] = {0};
    const char *val  = "(empty)";
    if (getter(ns, key, kv_val, sizeof(kv_val)) == OPRT_OK && kv_val[0] != '\0')
        val = kv_val;
    else if (build_val && build_val[0] != '\0')
        val = build_val;
    char buf[128] = {0};
    if (mask)
        mask_copy(val, buf, sizeof(buf));
    else
        snprintf(buf, sizeof(buf), "%s", val);
    cli_echof("%-14s: %s", label, buf);
}

static bool valid_channel_mode(const char *mode)
{
    return mode && (strcmp(mode, "telegram") == 0 || strcmp(mode, "discord") == 0 || strcmp(mode, "feishu") == 0);
}

static void cmd_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_echof("help | set_wifi <ssid> <pass> | wifi_status | wifi_scan");
    cli_echof("set_tg_token <token> | set_dc_token <token> | set_dc_channel <id>");
    cli_echof("set_fs_appid <id> | set_fs_appsecret <secret> | set_fs_allow <open_id_csv>");
    cli_echof("set_channel_mode <telegram|discord|feishu> | push_outbound <channel> <chat_id> <content>");
    cli_echof("config_show | restart");
}

static void cmd_set_wifi(int argc, char *argv[])
{
    if (argc < 3) {
        cli_echof("usage: set_wifi <ssid> <password>");
        return;
    }
    cli_echof("set_wifi rt=%d", wifi_manager_set_credentials(argv[1], argv[2]));
}

static void cmd_wifi_status(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_echof("connected: %s ip: %s", wifi_manager_is_connected() ? "yes" : "no", wifi_manager_get_ip());
}

static void scan_cb(uint32_t index, uint32_t total, const char *ssid, uint8_t channel, int rssi,
                    uint8_t security, const char *bssid, void *user_data)
{
    (void)total;
    (void)user_data;
    cli_echof("ap[%u] ssid=%s ch=%u rssi=%d", (unsigned)index, ssid ? ssid : "<hidden>",
              (unsigned)channel, rssi);
}

static void cmd_wifi_scan(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    wifi_manager_set_scan_result_cb(scan_cb, NULL);
    cli_echof("wifi_scan rt=%d", wifi_manager_scan_and_print());
    wifi_manager_set_scan_result_cb(NULL, NULL);
}

static void cmd_set_tg_token(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_tg_token <token>");
        return;
    }
    cli_echof("set_tg_token rt=%d", telegram_set_token(argv[1]));
}

static void cmd_set_dc_token(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_dc_token <token>");
        return;
    }
    cli_echof("set_dc_token rt=%d", discord_set_token(argv[1]));
}

static void cmd_set_dc_channel(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_dc_channel <channel_id>");
        return;
    }
    cli_echof("set_dc_channel rt=%d", discord_set_channel_id(argv[1]));
}

static void cmd_set_channel_mode(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_channel_mode <telegram|discord|feishu>");
        return;
    }
    if (!valid_channel_mode(argv[1])) {
        cli_echof("invalid mode: %s", argv[1]);
        return;
    }
    cli_echof("set_channel_mode rt=%d", im_kv_set_string(IM_NVS_BOT, IM_NVS_KEY_CHANNEL_MODE, argv[1]));
}

static void cmd_set_fs_appid(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_fs_appid <app_id>");
        return;
    }
    cli_echof("set_fs_appid rt=%d", feishu_set_app_id(argv[1]));
}

static void cmd_set_fs_appsecret(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_fs_appsecret <app_secret>");
        return;
    }
    cli_echof("set_fs_appsecret rt=%d", feishu_set_app_secret(argv[1]));
}

static void cmd_set_fs_allow(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_fs_allow <open_id_csv>");
        return;
    }
    cli_echof("set_fs_allow rt=%d", feishu_set_allow_from(argv[1]));
}

static void cmd_push_outbound(int argc, char *argv[])
{
    if (argc < 4) {
        cli_echof("usage: push_outbound <channel> <chat_id> <content>");
        return;
    }
    size_t len = 0;
    for (int i = 3; i < argc; i++) len += strlen(argv[i]) + (i > 3 ? 1 : 0);
    char *content = tal_malloc(len + 1);
    if (!content) {
        cli_echof("push_outbound oom");
        return;
    }
    content[0] = '\0';
    for (int i = 3; i < argc; i++) {
        if (i > 3) strcat(content, " ");
        strcat(content, argv[i]);
    }
    im_msg_t msg = {0};
    strncpy(msg.channel, argv[1], sizeof(msg.channel) - 1);
    msg.channel[sizeof(msg.channel) - 1] = '\0';
    strncpy(msg.chat_id, argv[2], sizeof(msg.chat_id) - 1);
    msg.chat_id[sizeof(msg.chat_id) - 1] = '\0';
    msg.content = content;
    OPERATE_RET rt = message_bus_push_outbound(&msg);
    if (rt != OPRT_OK) {
        cli_echof("push_outbound failed: %d", rt);
        tal_free(content);
    } else {
        cli_echof("push_outbound ok");
    }
}

static void cmd_config_show(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_echof("=== Echo Bot Config ===");
    print_cfg("WiFi SSID", ECHO_NVS_WIFI, ECHO_NVS_KEY_SSID, ECHO_SECRET_WIFI_SSID, false, echo_kv_get_string);
    print_cfg("WiFi Pass", ECHO_NVS_WIFI, ECHO_NVS_KEY_PASS, ECHO_SECRET_WIFI_PASS, true, echo_kv_get_string);
    print_cfg("TG Token", IM_NVS_TG, IM_NVS_KEY_TG_TOKEN, IM_SECRET_TG_TOKEN, true, im_kv_get_string);
    print_cfg("DC Token", IM_NVS_DC, IM_NVS_KEY_DC_TOKEN, IM_SECRET_DC_TOKEN, true, im_kv_get_string);
    print_cfg("DC Channel", IM_NVS_DC, IM_NVS_KEY_DC_CHANNEL_ID, IM_SECRET_DC_CHANNEL_ID, false, im_kv_get_string);
    print_cfg("FS AppID", IM_NVS_FS, IM_NVS_KEY_FS_APP_ID, IM_SECRET_FS_APP_ID, true, im_kv_get_string);
    print_cfg("FS Secret", IM_NVS_FS, IM_NVS_KEY_FS_APP_SECRET, IM_SECRET_FS_APP_SECRET, true, im_kv_get_string);
    print_cfg("FS Allow", IM_NVS_FS, IM_NVS_KEY_FS_ALLOW_FROM, IM_SECRET_FS_ALLOW_FROM, false, im_kv_get_string);
    print_cfg("ChannelMode", IM_NVS_BOT, IM_NVS_KEY_CHANNEL_MODE, IM_SECRET_CHANNEL_MODE, false, im_kv_get_string);
    cli_echof("======================");
}

static void cmd_restart(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_echof("restarting...");
    tal_system_reset();
}

static const cli_cmd_t s_cmds[] = {
    {.name = "help", .help = "List commands", .func = cmd_help},
    {.name = "set_wifi", .help = "Set WiFi SSID and password", .func = cmd_set_wifi},
    {.name = "wifi_status", .help = "Show WiFi status", .func = cmd_wifi_status},
    {.name = "wifi_scan", .help = "Scan WiFi APs", .func = cmd_wifi_scan},
    {.name = "set_tg_token", .help = "Set Telegram bot token", .func = cmd_set_tg_token},
    {.name = "set_dc_token", .help = "Set Discord bot token", .func = cmd_set_dc_token},
    {.name = "set_dc_channel", .help = "Set Discord channel ID", .func = cmd_set_dc_channel},
    {.name = "set_channel_mode", .help = "Set channel: telegram|discord|feishu", .func = cmd_set_channel_mode},
    {.name = "set_fs_appid", .help = "Set Feishu app_id", .func = cmd_set_fs_appid},
    {.name = "set_fs_appsecret", .help = "Set Feishu app_secret", .func = cmd_set_fs_appsecret},
    {.name = "set_fs_allow", .help = "Set Feishu allow_from open_id CSV", .func = cmd_set_fs_allow},
    {.name = "push_outbound", .help = "Push message to outbound bus", .func = cmd_push_outbound},
    {.name = "config_show", .help = "Show config", .func = cmd_config_show},
    {.name = "restart", .help = "Restart device", .func = cmd_restart},
};

OPERATE_RET echo_cli_init(void)
{
    if (s_inited) return OPRT_OK;
    OPERATE_RET rt = tal_cli_init();
    if (rt != OPRT_OK) return rt;
    rt = tal_cli_cmd_register(s_cmds, sizeof(s_cmds) / sizeof(s_cmds[0]));
    if (rt != OPRT_OK) return rt;
    s_inited = true;
    return OPRT_OK;
}
