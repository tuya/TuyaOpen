#include "serial_cli.h"

#include "llm/llm_proxy.h"
#include "memory/memory_store.h"
#include "memory/session_mgr.h"
#include "mimi_config.h"
#include "proxy/http_proxy.h"
#include "channels/discord_bot.h"
#include "channels/feishu_bot.h"
#include "tal_cli.h"
// #include "tal_fs.h"
#include "channels/telegram_bot.h"
#include "tools/tool_web_search.h"
#include "wifi/wifi_manager.h"

#include <stdarg.h>

static const char *TAG          = "cli";
static bool        s_cli_inited = false;

typedef struct {
    const char *name;
    const char *usage;
    const char *summary_1;
    const char *summary_2;
    const char *arg_1;
    const char *arg_2;
    const char *arg_3;
    uint8_t     verbose;
} cli_help_item_t;

static void cli_echof(const char *fmt, ...)
{
    char    line[512] = {0};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    tal_cli_echo(line);
}

static void mask_copy(const char *src, char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return;
    }
    if (!src || src[0] == '\0') {
        snprintf(out, out_size, "(empty)");
        return;
    }

    size_t len = strlen(src);
    if (len <= 4) {
        snprintf(out, out_size, "****");
    } else {
        snprintf(out, out_size, "%.4s****", src);
    }
}

static void print_config_item(const char *label, const char *ns, const char *key, const char *build_val, bool mask)
{
    char        kv_val[128] = {0};
    const char *src         = "not set";
    const char *val         = "(empty)";

    if (mimi_kv_get_string(ns, key, kv_val, sizeof(kv_val)) == OPRT_OK && kv_val[0] != '\0') {
        src = "kv";
        val = kv_val;
    } else if (build_val && build_val[0] != '\0') {
        src = "build";
        val = build_val;
    }

    char masked[64] = {0};
    if (mask) {
        mask_copy(val, masked, sizeof(masked));
        cli_echof("%-14s: %s [%s]", label, masked, src);
    } else {
        cli_echof("%-14s: %s [%s]", label, val, src);
    }
}

static const cli_help_item_t s_cli_help_items[] = {
    {
        .name      = "help",
        .usage     = "[<string>] [-v <0|1>]",
        .summary_1 = "Print the summary of all registered commands if no arguments are given,",
        .summary_2 = "otherwise print summary of given command.",
        .arg_1     = "      <string>  Name of command",
        .arg_2     = "  -v, --verbose=<0|1>  If specified, list console commands with given verbose level",
        .verbose   = 0,
    },
    {
        .name      = "set_wifi",
        .usage     = "<ssid> <password>",
        .summary_1 = "Set WiFi SSID and password",
        .arg_1     = "        <ssid>  WiFi SSID",
        .arg_2     = "    <password>  WiFi password",
        .verbose   = 0,
    },
    {
        .name      = "wifi_status",
        .summary_1 = "Show WiFi connection status",
        .verbose   = 0,
    },
    {
        .name      = "wifi_scan",
        .summary_1 = "Scan and list nearby WiFi APs",
        .verbose   = 0,
    },
    {
        .name      = "set_tg_token",
        .usage     = "<token>",
        .summary_1 = "Set Telegram bot token",
        .arg_1     = "       <token>  Telegram bot token",
        .verbose   = 0,
    },
    {
        .name      = "set_dc_token",
        .usage     = "<token>",
        .summary_1 = "Set Discord bot token",
        .arg_1     = "       <token>  Discord bot token",
        .verbose   = 0,
    },
    {
        .name      = "set_dc_channel",
        .usage     = "<channel_id>",
        .summary_1 = "Set optional default Discord channel ID",
        .arg_1     = "  <channel_id>  Discord channel ID",
        .verbose   = 0,
    },
    {
        .name      = "set_channel_mode",
        .usage     = "<telegram|discord|feishu>",
        .summary_1 = "Set active chat channel mode",
        .arg_1     = "         <mode>  telegram|discord|feishu",
        .verbose   = 0,
    },
    {
        .name      = "set_fs_appid",
        .usage     = "<app_id>",
        .summary_1 = "Set Feishu app_id",
        .arg_1     = "      <app_id>  Feishu app_id",
        .verbose   = 0,
    },
    {
        .name      = "set_fs_appsecret",
        .usage     = "<app_secret>",
        .summary_1 = "Set Feishu app_secret",
        .arg_1     = "  <app_secret>  Feishu app_secret",
        .verbose   = 0,
    },
    {
        .name      = "set_fs_allow",
        .usage     = "<open_id_csv>",
        .summary_1 = "Set Feishu allow_from open_id CSV (empty = allow all)",
        .arg_1     = " <open_id_csv>  Comma-separated sender open_id allowlist",
        .verbose   = 0,
    },
    {
        .name      = "set_api_key",
        .usage     = "<key>",
        .summary_1 = "Set LLM API key",
        .arg_1     = "         <key>  LLM API key",
        .verbose   = 0,
    },
    {
        .name      = "set_api_url",
        .usage     = "<url>",
        .summary_1 = "Set LLM API URL (provider decided by set_model_provider)",
        .arg_1     = "         <url>  API endpoint URL, e.g. https://api.xxx/v1/chat/completions",
        .verbose   = 0,
    },
    {
        .name      = "set_model",
        .usage     = "<model>",
        .summary_1 = "Set LLM model (default: " MIMI_LLM_DEFAULT_MODEL ")",
        .arg_1     = "       <model>  Model identifier",
        .verbose   = 0,
    },
    {
        .name      = "set_model_provider",
        .usage     = "<provider>",
        .summary_1 = "Set LLM model provider (default: " MIMI_LLM_PROVIDER_DEFAULT ")",
        .arg_1     = "    <provider>  Model provider (anthropic|openai)",
        .verbose   = 0,
    },
    {
        .name      = "memory_read",
        .summary_1 = "Read MEMORY.md",
        .verbose   = 0,
    },
    {
        .name      = "memory_write",
        .usage     = "<content>",
        .summary_1 = "Write to MEMORY.md",
        .arg_1     = "     <content>  Content to write",
        .verbose   = 0,
    },
    {
        .name      = "memory_clear",
        .summary_1 = "Clear MEMORY.md",
        .verbose   = 0,
    },
    {
        .name      = "file_list",
        .summary_1 = "List /spiffs entries",
        .verbose   = 0,
    },
    {
        .name      = "file_read",
        .usage     = "<path>",
        .summary_1 = "Read a text file under /spiffs/",
        .arg_1     = "        <path>  File path, e.g. /spiffs/sessions/xxx.jsonl",
        .verbose   = 0,
    },
    {
        .name      = "file_clear",
        .usage     = "<path>",
        .summary_1 = "Delete a file under /spiffs/",
        .arg_1     = "        <path>  File path, e.g. /spiffs/memory/daily_note.md",
        .verbose   = 0,
    },
    {
        .name      = "session_list",
        .summary_1 = "List all sessions",
        .verbose   = 0,
    },
    {
        .name      = "session_clear",
        .usage     = "<chat_id>",
        .summary_1 = "Clear a session only",
        .arg_1     = "     <chat_id>  Chat ID or tg_*.jsonl file name",
        .verbose   = 0,
    },
    {
        .name      = "session_clear_all",
        .summary_1 = "Clear all session files",
        .verbose   = 0,
    },
    {
        .name      = "heap_info",
        .summary_1 = "Show heap memory usage",
        .verbose   = 0,
    },
    {
        .name      = "set_search_key",
        .usage     = "<key>",
        .summary_1 = "Set Brave Search API key for web_search tool",
        .arg_1     = "         <key>  Brave Search API key",
        .verbose   = 0,
    },
    {
        .name      = "set_proxy",
        .usage     = "<host> <port> [type]",
        .summary_1 = "Set proxy (e.g. set_proxy 192.168.1.83 7897 socks5)",
        .arg_1     = "        <host>  Proxy host/IP",
        .arg_2     = "        <port>  Proxy port",
        .arg_3     = "      [type]  http|socks5 (default: http)",
        .verbose   = 0,
    },
    {
        .name      = "clear_proxy",
        .summary_1 = "Remove proxy configuration",
        .verbose   = 0,
    },
    {
        .name      = "config_show",
        .summary_1 = "Show current configuration (build-time + NVS)",
        .verbose   = 0,
    },
    {
        .name      = "config_reset",
        .summary_1 = "Clear all NVS overrides, revert to build-time defaults",
        .verbose   = 0,
    },
    {
        .name      = "restart",
        .summary_1 = "Restart the device",
        .verbose   = 0,
    },
};

static const cli_help_item_t *find_help_item(const char *name)
{
    if (!name || name[0] == '\0') {
        return NULL;
    }

    for (size_t i = 0; i < sizeof(s_cli_help_items) / sizeof(s_cli_help_items[0]); i++) {
        if (strcmp(name, s_cli_help_items[i].name) == 0) {
            return &s_cli_help_items[i];
        }
    }
    return NULL;
}

static void print_help_item(const cli_help_item_t *item)
{
    if (!item) {
        return;
    }

    if (item->usage && item->usage[0] != '\0') {
        cli_echof("%s  %s", item->name, item->usage);
    } else {
        cli_echof("%s", item->name);
    }

    if (item->summary_1 && item->summary_1[0] != '\0') {
        cli_echof("  %s", item->summary_1);
    }
    if (item->summary_2 && item->summary_2[0] != '\0') {
        cli_echof("  %s", item->summary_2);
    }
    if (item->arg_1 && item->arg_1[0] != '\0') {
        cli_echof("%s", item->arg_1);
    }
    if (item->arg_2 && item->arg_2[0] != '\0') {
        cli_echof("%s", item->arg_2);
    }
    if (item->arg_3 && item->arg_3[0] != '\0') {
        cli_echof("%s", item->arg_3);
    }
}

static bool parse_verbose_level(const char *s, int *out)
{
    if (!s || !out) {
        return false;
    }
    if (strcmp(s, "0") == 0) {
        *out = 0;
        return true;
    }
    if (strcmp(s, "1") == 0) {
        *out = 1;
        return true;
    }
    return false;
}

static void cmd_help(int argc, char *argv[])
{
    const char *cmd_name       = NULL;
    int         verbose_filter = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            if (i + 1 >= argc || !parse_verbose_level(argv[i + 1], &verbose_filter)) {
                cli_echof("usage: help [<string>] [-v <0|1>]");
                return;
            }
            i++;
            continue;
        }

        if (strncmp(argv[i], "--verbose=", 10) == 0) {
            if (!parse_verbose_level(argv[i] + 10, &verbose_filter)) {
                cli_echof("usage: help [<string>] [-v <0|1>]");
                return;
            }
            continue;
        }

        if (argv[i][0] == '-') {
            cli_echof("usage: help [<string>] [-v <0|1>]");
            return;
        }

        if (cmd_name != NULL) {
            cli_echof("usage: help [<string>] [-v <0|1>]");
            return;
        }
        cmd_name = argv[i];
    }

    if (cmd_name) {
        const cli_help_item_t *item = find_help_item(cmd_name);
        if (!item) {
            cli_echof("command not found: %s", cmd_name);
            return;
        }
        if (verbose_filter >= 0 && item->verbose != (uint8_t)verbose_filter) {
            cli_echof("command not found: %s", cmd_name);
            return;
        }
        print_help_item(item);
        return;
    }

    bool printed = false;
    for (size_t i = 0; i < sizeof(s_cli_help_items) / sizeof(s_cli_help_items[0]); i++) {
        const cli_help_item_t *item = &s_cli_help_items[i];
        if (verbose_filter >= 0 && item->verbose != (uint8_t)verbose_filter) {
            continue;
        }
        if (printed) {
            cli_echof("");
        }
        print_help_item(item);
        printed = true;
    }

    if (!printed) {
        cli_echof("no commands for verbose level %d", verbose_filter);
    }
}

static void cmd_wifi_set(int argc, char *argv[])
{
    if (argc < 3) {
        cli_echof("usage: set_wifi <ssid> <password>");
        return;
    }

    OPERATE_RET rt = wifi_manager_set_credentials(argv[1], argv[2]);
    cli_echof("set_wifi rt=%d", rt);
}

static void cmd_wifi_set_legacy(int argc, char *argv[])
{
    cmd_wifi_set(argc, argv);
}

static void cmd_wifi_status(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_echof("connected: %s", wifi_manager_is_connected() ? "yes" : "no");
    cli_echof("ip: %s", wifi_manager_get_ip());
}

static void cli_wifi_scan_result_cb(uint32_t index, uint32_t total, const char *ssid, uint8_t channel, int rssi,
                                    uint8_t security, const char *bssid, void *user_data)
{
    (void)total;
    (void)user_data;
    cli_echof("ap[%u] ssid=%s ch=%u rssi=%d sec=%u bssid=%s", (unsigned)index, ssid ? ssid : "<hidden>",
              (unsigned)channel, rssi, (unsigned)security, bssid ? bssid : "-");
}

static void cmd_wifi_scan(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_echof("wifi_scan start");
    wifi_manager_set_scan_result_cb(cli_wifi_scan_result_cb, NULL);
    OPERATE_RET rt = wifi_manager_scan_and_print();
    wifi_manager_set_scan_result_cb(NULL, NULL);
    cli_echof("wifi_scan rt=%d", rt);
}

static void cmd_set_tg_token(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_tg_token <token>");
        return;
    }
    OPERATE_RET rt = telegram_set_token(argv[1]);
    cli_echof("set_tg_token rt=%d", rt);
}

static bool is_valid_channel_mode(const char *mode)
{
    if (!mode || mode[0] == '\0') {
        return false;
    }

    return strcmp(mode, "telegram") == 0 || strcmp(mode, "discord") == 0 || strcmp(mode, "feishu") == 0;
}

static const char *llm_default_api_url_for_provider(void)
{
    const char *provider = MIMI_SECRET_MODEL_PROVIDER;
    if (!provider || provider[0] == '\0') {
        provider = MIMI_LLM_PROVIDER_DEFAULT;
    }

    char provider_kv[32] = {0};
    if (mimi_kv_get_string(MIMI_NVS_LLM, MIMI_NVS_KEY_PROVIDER, provider_kv, sizeof(provider_kv)) == OPRT_OK &&
        provider_kv[0] != '\0') {
        provider = provider_kv;
    }

    if (strcmp(provider, "openai") == 0) {
        return MIMI_OPENAI_API_URL;
    }
    return MIMI_LLM_API_URL;
}

static void cmd_set_dc_token(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_dc_token <token>");
        return;
    }

    OPERATE_RET rt = discord_set_token(argv[1]);
    cli_echof("set_dc_token rt=%d", rt);
}

static void cmd_set_dc_channel(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_dc_channel <channel_id>");
        return;
    }

    OPERATE_RET rt = discord_set_channel_id(argv[1]);
    cli_echof("set_dc_channel rt=%d", rt);
}

static void cmd_set_channel_mode(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_channel_mode <telegram|discord|feishu>");
        return;
    }

    if (!is_valid_channel_mode(argv[1])) {
        cli_echof("invalid mode: %s (use telegram|discord|feishu)", argv[1]);
        return;
    }

    OPERATE_RET rt = mimi_kv_set_string(MIMI_NVS_BOT, MIMI_NVS_KEY_CHANNEL_MODE, argv[1]);
    cli_echof("set_channel_mode rt=%d", rt);
}

static void cmd_set_fs_appid(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_fs_appid <app_id>");
        return;
    }

    OPERATE_RET rt = feishu_set_app_id(argv[1]);
    cli_echof("set_fs_appid rt=%d", rt);
}

static void cmd_set_fs_appsecret(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_fs_appsecret <app_secret>");
        return;
    }

    OPERATE_RET rt = feishu_set_app_secret(argv[1]);
    cli_echof("set_fs_appsecret rt=%d", rt);
}

static void cmd_set_fs_allow(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_fs_allow <open_id_csv>");
        return;
    }

    OPERATE_RET rt = feishu_set_allow_from(argv[1]);
    cli_echof("set_fs_allow rt=%d", rt);
}

static void cmd_set_api_key(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_api_key <api_key>");
        return;
    }
    OPERATE_RET rt = llm_set_api_key(argv[1]);
    cli_echof("set_api_key rt=%d", rt);
}

static void cmd_set_api_url(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_api_url <url>");
        return;
    }
    OPERATE_RET rt = llm_set_api_url(argv[1]);
    cli_echof("set_api_url rt=%d", rt);
}

static void cmd_set_model(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_model <model>");
        return;
    }
    OPERATE_RET rt = llm_set_model(argv[1]);
    cli_echof("set_model rt=%d", rt);
}

static void cmd_set_model_provider(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_model_provider <provider>");
        return;
    }
    OPERATE_RET rt = llm_set_provider(argv[1]);
    cli_echof("set_model_provider rt=%d", rt);
}

static void cmd_set_proxy(int argc, char *argv[])
{
    if (argc < 3) {
        cli_echof("usage: set_proxy <host> <port> [type]");
        return;
    }

    long port = strtol(argv[2], NULL, 10);
    if (port <= 0 || port > 65535) {
        cli_echof("invalid port: %s", argv[2]);
        return;
    }

    const char *type = (argc >= 4) ? argv[3] : "http";
    if (strcmp(type, "http") != 0 && strcmp(type, "socks5") != 0) {
        cli_echof("invalid type: %s (use http or socks5)", type);
        return;
    }

    OPERATE_RET rt = http_proxy_set(argv[1], (uint16_t)port, type);
    cli_echof("set_proxy rt=%d", rt);
}

static void cmd_clear_proxy(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    OPERATE_RET rt = http_proxy_clear();
    cli_echof("clear_proxy rt=%d", rt);
}

static void cmd_set_search_key(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: set_search_key <api_key>");
        return;
    }
    OPERATE_RET rt = tool_web_search_set_key(argv[1]);
    cli_echof("set_search_key rt=%d", rt);
}

static void cmd_memory_read(int argc, char *argv[])
{
    (void)argv;
    if (argc != 1) {
        cli_echof("usage: memory_read");
        return;
    }

    char *buf = tal_malloc(4096);
    if (!buf) {
        cli_echof("memory_read oom");
        return;
    }
    memset(buf, 0, 4096);

    OPERATE_RET rt = memory_read_long_term(buf, 4096);
    if (rt != OPRT_OK || buf[0] == '\0') {
        cli_echof("memory empty rt=%d", rt);
        tal_free(buf);
        return;
    }

    cli_echof("=== MEMORY.md ===");
    cli_echof("%s", buf);
    cli_echof("=================");
    tal_free(buf);
}

static void cmd_file_read(int argc, char *argv[])
{
    if (argc != 2) {
        cli_echof("usage: file_read <path>");
        return;
    }

    const char *path = argv[1];
    if (strncmp(path, MIMI_SPIFFS_BASE "/", sizeof(MIMI_SPIFFS_BASE)) != 0) {
        cli_echof("file_read path must start with /spiffs/");
        return;
    }

    char *buf = tal_malloc(4096);
    if (!buf) {
        cli_echof("file_read oom");
        return;
    }
    memset(buf, 0, 4096);

    TUYA_FILE f = mimi_fopen(path, "r");
    if (!f) {
        cli_echof("file_read open failed: %s", path);
        tal_free(buf);
        return;
    }

    int n = mimi_fread(buf, 4095, f);
    mimi_fclose(f);
    if (n < 0) {
        cli_echof("file_read read failed: %s", path);
        tal_free(buf);
        return;
    }
    buf[n] = '\0';

    int file_size = mimi_fgetsize(path);
    if (buf[0] == '\0') {
        cli_echof("file_read empty: %s", path);
        tal_free(buf);
        return;
    }

    cli_echof("=== %s ===", path);
    cli_echof("%s", buf);
    if (file_size > n && n > 0) {
        cli_echof("[truncated] %d/%d B", n, file_size);
    }
    cli_echof("=================");
    tal_free(buf);
}

static void cmd_memory_write(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: memory_write <content>");
        return;
    }

    char *content = tal_malloc(2048);
    if (!content) {
        cli_echof("memory_write oom");
        return;
    }
    memset(content, 0, 2048);

    size_t off = 0;
    for (int i = 1; i < argc && off < 2048 - 1; i++) {
        int n = snprintf(content + off, 2048 - off, "%s%s", (i == 1) ? "" : " ", argv[i]);
        if (n < 0 || (size_t)n >= 2048 - off) {
            break;
        }
        off += (size_t)n;
    }

    OPERATE_RET rt = memory_write_long_term(content);
    cli_echof("memory_write rt=%d", rt);
    tal_free(content);
}

static void cmd_memory_clear(int argc, char *argv[])
{
    (void)argv;
    if (argc != 1) {
        cli_echof("usage: memory_clear");
        return;
    }

    OPERATE_RET rt = memory_write_long_term("");
    cli_echof("memory_clear rt=%d", rt);
}

typedef struct {
    uint32_t    idx;
    const char *base_dir;
} cli_session_list_ctx_t;

static void cli_session_list_cb(const char *name, void *user_data)
{
    cli_session_list_ctx_t *ctx      = (cli_session_list_ctx_t *)user_data;
    uint32_t                idx      = ctx ? ctx->idx++ : 0;
    const char             *base_dir = (ctx && ctx->base_dir) ? ctx->base_dir : MIMI_SPIFFS_SESSION_DIR;

    char path[192] = {0};
    if (name && name[0] != '\0') {
        snprintf(path, sizeof(path), "%s/%s", base_dir, name);
    }

    int size = path[0] ? mimi_fgetsize(path) : -1;
    if (size >= 0) {
        cli_echof("session[%u]: %s (%d B)", (unsigned)idx, name ? name : "", size);
    } else {
        cli_echof("session[%u]: %s", (unsigned)idx, name ? name : "");
    }
}

static void cli_list_spiffs_entries(void)
{
    uint32_t entry_idx  = 0;
    uint32_t file_count = 0;

    TUYA_DIR root = NULL;
    if (mimi_dir_open(MIMI_SPIFFS_BASE, &root) != OPRT_OK || !root) {
        cli_echof("/spiffs open failed");
        return;
    }

    cli_echof("/spiffs all listing:");
    while (1) {
        TUYA_FILEINFO info = NULL;
        if (mimi_dir_read(root, &info) != OPRT_OK || !info) {
            break;
        }

        const char *name = NULL;
        if (mimi_dir_name(info, &name) != OPRT_OK || !name || name[0] == '\0') {
            continue;
        }
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        char path[192] = {0};
        snprintf(path, sizeof(path), "%s/%s", MIMI_SPIFFS_BASE, name);

        BOOL_T is_regular = FALSE;
        if (mimi_dir_is_regular(info, &is_regular) == OPRT_OK && is_regular) {
            int size = mimi_fgetsize(path);
            cli_echof("/spiffs[%u]: %s (%d B)", (unsigned)entry_idx++, path, size);
            file_count++;
            continue;
        }

        cli_echof("/spiffs[%u]: %s/ (<dir>)", (unsigned)entry_idx++, path);

        TUYA_DIR sub = NULL;
        if (mimi_dir_open(path, &sub) != OPRT_OK || !sub) {
            continue;
        }

        while (1) {
            TUYA_FILEINFO sub_info = NULL;
            if (mimi_dir_read(sub, &sub_info) != OPRT_OK || !sub_info) {
                break;
            }

            const char *sub_name = NULL;
            if (mimi_dir_name(sub_info, &sub_name) != OPRT_OK || !sub_name || sub_name[0] == '\0') {
                continue;
            }
            if (strcmp(sub_name, ".") == 0 || strcmp(sub_name, "..") == 0) {
                continue;
            }

            char sub_path[224] = {0};
            snprintf(sub_path, sizeof(sub_path), "%s/%s", path, sub_name);

            BOOL_T sub_is_regular = FALSE;
            if (mimi_dir_is_regular(sub_info, &sub_is_regular) == OPRT_OK && sub_is_regular) {
                int sub_size = mimi_fgetsize(sub_path);
                cli_echof("/spiffs[%u]: %s (%d B)", (unsigned)entry_idx++, sub_path, sub_size);
                file_count++;
            } else {
                cli_echof("/spiffs[%u]: %s/ (<dir>)", (unsigned)entry_idx++, sub_path);
            }
        }

        mimi_dir_close(sub);
    }

    mimi_dir_close(root);
    cli_echof("file_list done files=%u entries=%u", (unsigned)file_count, (unsigned)entry_idx);
}

static void cmd_session_list(int argc, char *argv[])
{
    (void)argv;
    if (argc != 1) {
        cli_echof("usage: session_list");
        return;
    }

    uint32_t               count = 0;
    cli_session_list_ctx_t ctx   = {
          .idx      = 0,
          .base_dir = MIMI_SPIFFS_SESSION_DIR,
    };
    OPERATE_RET rt = session_list(cli_session_list_cb, &ctx, &count);
    if (rt == OPRT_NOT_FOUND || count == 0) {
        cli_echof("session_list empty");
        return;
    }
    if (rt != OPRT_OK) {
        cli_echof("session_list rt=%d", rt);
        return;
    }

    cli_echof("session_list done count=%u", (unsigned)count);
}

static void cmd_file_list(int argc, char *argv[])
{
    (void)argv;
    if (argc != 1) {
        cli_echof("usage: file_list");
        return;
    }
    cli_list_spiffs_entries();
}

static void cmd_file_clear(int argc, char *argv[])
{
    if (argc != 2) {
        cli_echof("usage: file_clear <path>");
        return;
    }

    const char *path = argv[1];
    if (strncmp(path, MIMI_SPIFFS_BASE "/", sizeof(MIMI_SPIFFS_BASE)) != 0) {
        cli_echof("file_clear path must start with /spiffs/");
        return;
    }

    OPERATE_RET rt = mimi_fs_remove(path);
    cli_echof("file_clear rt=%d", rt);
}

static void cmd_session_clear(int argc, char *argv[])
{
    if (argc < 2) {
        cli_echof("usage: session_clear <chat_id|tg_*.jsonl>");
        return;
    }

    OPERATE_RET rt = session_clear(argv[1]);
    cli_echof("session_clear rt=%d", rt);
}

static void cmd_session_clear_all(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t    removed = 0;
    OPERATE_RET rt      = session_clear_all(&removed);
    cli_echof("session_clear_all rt=%d removed=%u", rt, (unsigned)removed);
}

static void cmd_heap_info(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_echof("free heap: %d", tal_system_get_free_heap_size());
}

static void cmd_config_show(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_echof("=== Current Configuration ===");
    print_config_item("WiFi SSID", MIMI_NVS_WIFI, MIMI_NVS_KEY_SSID, MIMI_SECRET_WIFI_SSID, false);
    print_config_item("WiFi Pass", MIMI_NVS_WIFI, MIMI_NVS_KEY_PASS, MIMI_SECRET_WIFI_PASS, true);
    print_config_item("TG Token", MIMI_NVS_TG, MIMI_NVS_KEY_TG_TOKEN, MIMI_SECRET_TG_TOKEN, true);
    print_config_item("DC Token", MIMI_NVS_DC, MIMI_NVS_KEY_DC_TOKEN, MIMI_SECRET_DC_TOKEN, true);
    print_config_item("DC Channel", MIMI_NVS_DC, MIMI_NVS_KEY_DC_CHANNEL_ID, MIMI_SECRET_DC_CHANNEL_ID, false);
    print_config_item("FS AppID", MIMI_NVS_FS, MIMI_NVS_KEY_FS_APP_ID, MIMI_SECRET_FS_APP_ID, true);
    print_config_item("FS Secret", MIMI_NVS_FS, MIMI_NVS_KEY_FS_APP_SECRET, MIMI_SECRET_FS_APP_SECRET, true);
    print_config_item("FS Allow", MIMI_NVS_FS, MIMI_NVS_KEY_FS_ALLOW_FROM, MIMI_SECRET_FS_ALLOW_FROM, false);
    print_config_item("ChannelMode", MIMI_NVS_BOT, MIMI_NVS_KEY_CHANNEL_MODE, MIMI_SECRET_CHANNEL_MODE, false);
    print_config_item("API Key", MIMI_NVS_LLM, MIMI_NVS_KEY_API_KEY, MIMI_SECRET_API_KEY, true);
    print_config_item("API URL", MIMI_NVS_LLM, MIMI_NVS_KEY_API_URL, llm_default_api_url_for_provider(), false);
    print_config_item("Model", MIMI_NVS_LLM, MIMI_NVS_KEY_MODEL, MIMI_SECRET_MODEL, false);
    print_config_item("Provider", MIMI_NVS_LLM, MIMI_NVS_KEY_PROVIDER, MIMI_SECRET_MODEL_PROVIDER, false);
    print_config_item("Proxy Host", MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_HOST, MIMI_SECRET_PROXY_HOST, false);
    print_config_item("Proxy Port", MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_PORT, MIMI_SECRET_PROXY_PORT, false);
    print_config_item("Proxy Type", MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_TYPE, MIMI_SECRET_PROXY_TYPE, false);
    print_config_item("Search Key", MIMI_NVS_SEARCH, MIMI_NVS_KEY_API_KEY, MIMI_SECRET_SEARCH_KEY, true);
    cli_echof("=============================");
}

static void cmd_config_reset(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    (void)mimi_kv_del(MIMI_NVS_WIFI, MIMI_NVS_KEY_SSID);
    (void)mimi_kv_del(MIMI_NVS_WIFI, MIMI_NVS_KEY_PASS);
    (void)mimi_kv_del(MIMI_NVS_TG, MIMI_NVS_KEY_TG_TOKEN);
    (void)mimi_kv_del(MIMI_NVS_DC, MIMI_NVS_KEY_DC_TOKEN);
    (void)mimi_kv_del(MIMI_NVS_DC, MIMI_NVS_KEY_DC_CHANNEL_ID);
    (void)mimi_kv_del(MIMI_NVS_DC, MIMI_NVS_KEY_DC_LAST_MSG_ID);
    (void)mimi_kv_del(MIMI_NVS_FS, MIMI_NVS_KEY_FS_APP_ID);
    (void)mimi_kv_del(MIMI_NVS_FS, MIMI_NVS_KEY_FS_APP_SECRET);
    (void)mimi_kv_del(MIMI_NVS_FS, MIMI_NVS_KEY_FS_ALLOW_FROM);
    (void)mimi_kv_del(MIMI_NVS_BOT, MIMI_NVS_KEY_CHANNEL_MODE);
    (void)mimi_kv_del(MIMI_NVS_LLM, MIMI_NVS_KEY_API_KEY);
    (void)mimi_kv_del(MIMI_NVS_LLM, MIMI_NVS_KEY_API_URL);
    (void)mimi_kv_del(MIMI_NVS_LLM, MIMI_NVS_KEY_MODEL);
    (void)mimi_kv_del(MIMI_NVS_LLM, MIMI_NVS_KEY_PROVIDER);
    (void)mimi_kv_del(MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_HOST);
    (void)mimi_kv_del(MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_PORT);
    (void)mimi_kv_del(MIMI_NVS_PROXY, MIMI_NVS_KEY_PROXY_TYPE);
    (void)mimi_kv_del(MIMI_NVS_SEARCH, MIMI_NVS_KEY_API_KEY);
    cli_echof("config reset done");
}

static void cmd_restart(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_echof("restarting...");
    tal_system_reset();
}

static const cli_cmd_t s_mimi_cli_cmds[] = {
    {.name = "help", .help = "help [<string>] [-v <0|1>]", .func = cmd_help},
    {.name = "set_wifi", .help = "Set WiFi SSID and password", .func = cmd_wifi_set},
    {.name = "wifi_set", .help = "Set WiFi SSID and password (legacy alias)", .func = cmd_wifi_set_legacy},
    {.name = "wifi_status", .help = "Show WiFi connection status", .func = cmd_wifi_status},
    {.name = "wifi_scan", .help = "Scan and list nearby WiFi APs", .func = cmd_wifi_scan},
    {.name = "set_tg_token", .help = "Set Telegram bot token", .func = cmd_set_tg_token},
    {.name = "set_dc_token", .help = "Set Discord bot token", .func = cmd_set_dc_token},
    {.name = "set_dc_channel", .help = "Set Discord channel ID", .func = cmd_set_dc_channel},
    {.name = "set_channel_mode", .help = "Set channel mode telegram|discord|feishu", .func = cmd_set_channel_mode},
    {.name = "set_fs_appid", .help = "Set Feishu app_id", .func = cmd_set_fs_appid},
    {.name = "set_fs_appsecret", .help = "Set Feishu app_secret", .func = cmd_set_fs_appsecret},
    {.name = "set_fs_allow", .help = "Set Feishu allow_from open_id CSV", .func = cmd_set_fs_allow},
    {.name = "set_api_key", .help = "Set LLM API key", .func = cmd_set_api_key},
    {.name = "set_api_url", .help = "Set LLM API URL", .func = cmd_set_api_url},
    {.name = "set_model", .help = "Set LLM model", .func = cmd_set_model},
    {.name = "set_model_provider", .help = "Set LLM model provider", .func = cmd_set_model_provider},
    {.name = "file_read", .help = "Read a text file from /spiffs", .func = cmd_file_read},
    {.name = "file_list", .help = "List /spiffs entries", .func = cmd_file_list},
    {.name = "file_clear", .help = "Delete a file from /spiffs", .func = cmd_file_clear},
    {.name = "memory_read", .help = "Read MEMORY.md", .func = cmd_memory_read},
    {.name = "memory_write", .help = "Write to MEMORY.md", .func = cmd_memory_write},
    {.name = "memory_clear", .help = "Clear MEMORY.md", .func = cmd_memory_clear},
    {.name = "session_list", .help = "List sessions only", .func = cmd_session_list},
    {.name = "session_clear", .help = "Clear a session only", .func = cmd_session_clear},
    {.name = "session_clear_all", .help = "Clear all sessions", .func = cmd_session_clear_all},
    {.name = "heap_info", .help = "Show heap memory usage", .func = cmd_heap_info},
    {.name = "set_search_key", .help = "Set Brave Search API key", .func = cmd_set_search_key},
    {.name = "set_proxy", .help = "Set proxy host/port/type", .func = cmd_set_proxy},
    {.name = "clear_proxy", .help = "Remove proxy configuration", .func = cmd_clear_proxy},
    {.name = "config_show", .help = "Show current configuration", .func = cmd_config_show},
    {.name = "config_reset", .help = "Clear all NVS overrides", .func = cmd_config_reset},
    {.name = "restart", .help = "Restart the device", .func = cmd_restart},
};

OPERATE_RET serial_cli_init(void)
{
    if (s_cli_inited) {
        return OPRT_OK;
    }

    OPERATE_RET rt = tal_cli_init();
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "tal_cli_init failed: %d", rt);
        return rt;
    }

    rt = tal_cli_cmd_register(s_mimi_cli_cmds, sizeof(s_mimi_cli_cmds) / sizeof(s_mimi_cli_cmds[0]));
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "tal_cli_cmd_register failed: %d", rt);
        return rt;
    }

    s_cli_inited = true;
    MIMI_LOGI(TAG, "serial cli initialized, cmds=%u", (unsigned)(sizeof(s_mimi_cli_cmds) / sizeof(s_mimi_cli_cmds[0])));
    return OPRT_OK;
}
