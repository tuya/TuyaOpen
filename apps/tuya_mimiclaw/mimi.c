#include "mimi_base.h"

#include "agent/agent_loop.h"
#include "bus/message_bus.h"
#include "cli/serial_cli.h"
#include "channels/discord_bot.h"
#include "channels/feishu_bot.h"
#include "cron/cron_service.h"
#include "gateway/ws_server.h"
#include "heartbeat/heartbeat.h"
#include "llm/llm_proxy.h"
#include "memory/memory_store.h"
#include "memory/session_mgr.h"
#include "mimi_config.h"
#include "proxy/http_proxy.h"
#include "skills/skill_loader.h"
#include "channels/telegram_bot.h"
#include "tools/tool_registry.h"
#include "tuya_register_center.h"
#include "tuya_tls.h"
#include "wifi/wifi_manager.h"

#include "cJSON.h"
#include "netmgr.h"
#include "tal_fs.h"
#include "tkl_output.h"

#include <ctype.h>

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
#include "lwip_init.h"
#endif

static const char   *TAG               = "mimi";
static THREAD_HANDLE s_outbound_thread = NULL;

typedef enum {
    MIMI_CHANNEL_MODE_TELEGRAM = 0,
    MIMI_CHANNEL_MODE_DISCORD,
    MIMI_CHANNEL_MODE_FEISHU,
} mimi_channel_mode_t;

static bool str_ieq(const char *a, const char *b)
{
    if (!a || !b) {
        return false;
    }
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static mimi_channel_mode_t parse_channel_mode(const char *mode)
{
    if (!mode || mode[0] == '\0') {
        return MIMI_CHANNEL_MODE_TELEGRAM;
    }
    if (str_ieq(mode, "telegram")) {
        return MIMI_CHANNEL_MODE_TELEGRAM;
    }
    if (str_ieq(mode, "discord")) {
        return MIMI_CHANNEL_MODE_DISCORD;
    }
    if (str_ieq(mode, "feishu")) {
        return MIMI_CHANNEL_MODE_FEISHU;
    }
    return MIMI_CHANNEL_MODE_TELEGRAM;
}

static const char *channel_mode_str(mimi_channel_mode_t mode)
{
    switch (mode) {
    case MIMI_CHANNEL_MODE_TELEGRAM:
        return "telegram";
    case MIMI_CHANNEL_MODE_DISCORD:
        return "discord";
    case MIMI_CHANNEL_MODE_FEISHU:
        return "feishu";
    default:
        return "telegram";
    }
}

static mimi_channel_mode_t load_channel_mode(void)
{
    char mode_buf[24] = {0};
    if (MIMI_SECRET_CHANNEL_MODE[0] != '\0') {
        snprintf(mode_buf, sizeof(mode_buf), "%s", MIMI_SECRET_CHANNEL_MODE);
    } else {
        snprintf(mode_buf, sizeof(mode_buf), "telegram");
    }

    char kv_mode[24] = {0};
    if (mimi_kv_get_string(MIMI_NVS_BOT, MIMI_NVS_KEY_CHANNEL_MODE, kv_mode, sizeof(kv_mode)) == OPRT_OK &&
        kv_mode[0] != '\0') {
        snprintf(mode_buf, sizeof(mode_buf), "%s", kv_mode);
    }

    if (str_ieq(mode_buf, "auto") || str_ieq(mode_buf, "both")) {
        MIMI_LOGW(TAG, "legacy channel_mode=%s mapped to telegram", mode_buf);
        snprintf(mode_buf, sizeof(mode_buf), "telegram");
    } else if (!(str_ieq(mode_buf, "telegram") || str_ieq(mode_buf, "discord") || str_ieq(mode_buf, "feishu"))) {
        MIMI_LOGW(TAG, "invalid channel_mode=%s, fallback to telegram", mode_buf);
        snprintf(mode_buf, sizeof(mode_buf), "telegram");
    }

    return parse_channel_mode(mode_buf);
}

static void mimi_runtime_init(void)
{
    static bool s_inited = false;
    if (s_inited) {
        return;
    }

    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_malloc, .free_fn = tal_free});
    (void)tal_log_init(TAL_LOG_LEVEL_INFO, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    // LittleFS mount happens inside tal_kv_init(). We must call this before any tal_fs_* APIs.
    (void)tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key  = "dflfuap134ddlduq",
    });

    (void)tal_sw_timer_init();
    (void)tal_workq_init();
    (void)tuya_tls_init();
    (void)tuya_register_center_init();

    s_inited = true;
}

static OPERATE_RET ensure_dir(const char *path)
{
    BOOL_T exists = FALSE;
    if (tal_fs_is_exist(path, &exists) == OPRT_OK && exists) {
        return OPRT_OK;
    }

    OPERATE_RET rt = tal_fs_mkdir(path);
    if (rt != OPRT_OK) {
        MIMI_LOGE(TAG, "mkdir failed: %s rt=%d", path, rt);
        return rt;
    }

    return OPRT_OK;
}

static OPERATE_RET init_storage(void)
{
    OPERATE_RET rt = ensure_dir(MIMI_SPIFFS_BASE);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = ensure_dir(MIMI_SPIFFS_CONFIG_DIR);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = ensure_dir(MIMI_SPIFFS_MEMORY_DIR);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = ensure_dir(MIMI_SPIFFS_SESSION_DIR);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = ensure_dir(MIMI_SPIFFS_SKILLS_DIR);
    if (rt != OPRT_OK) {
        return rt;
    }

    return OPRT_OK;
}

static void outbound_dispatch_task(void *arg)
{
    (void)arg;

    MIMI_LOGI(TAG, "outbound dispatcher started");

    while (1) {
        mimi_msg_t msg = {0};
        if (message_bus_pop_outbound(&msg, MIMI_WAIT_FOREVER) != OPRT_OK) {
            continue;
        }

        if (strcmp(msg.channel, MIMI_CHAN_TELEGRAM) == 0) {
            OPERATE_RET send_rt = telegram_send_message(msg.chat_id, msg.content ? msg.content : "");
            if (send_rt != OPRT_OK) {
                MIMI_LOGE(TAG, "telegram send failed chat=%s rt=%d", msg.chat_id, send_rt);
            }
        } else if (strcmp(msg.channel, MIMI_CHAN_DISCORD) == 0) {
            OPERATE_RET dc_rt = discord_send_message(msg.chat_id, msg.content ? msg.content : "");
            if (dc_rt != OPRT_OK) {
                MIMI_LOGE(TAG, "discord send failed channel=%s rt=%d", msg.chat_id, dc_rt);
            }
        } else if (strcmp(msg.channel, MIMI_CHAN_FEISHU) == 0) {
            OPERATE_RET fs_rt = feishu_send_message(msg.chat_id, msg.content ? msg.content : "");
            if (fs_rt != OPRT_OK) {
                MIMI_LOGE(TAG, "feishu send failed chat=%s rt=%d", msg.chat_id, fs_rt);
            }
        } else if (strcmp(msg.channel, MIMI_CHAN_WEBSOCKET) == 0) {
            OPERATE_RET ws_rt = ws_server_send(msg.chat_id, msg.content ? msg.content : "");
            if (ws_rt != OPRT_OK) {
                MIMI_LOGW(TAG, "websocket send failed chat=%s rt=%d", msg.chat_id, ws_rt);
            }
        } else if (strcmp(msg.channel, MIMI_CHAN_SYSTEM) == 0) {
            MIMI_LOGI(TAG, "system message delivered bytes=%u", (unsigned)strlen(msg.content ? msg.content : ""));
        } else {
            MIMI_LOGW(TAG, "unknown outbound channel: %s", msg.channel);
        }

        free(msg.content);
    }
}

static OPERATE_RET start_outbound_dispatcher(void)
{
    if (s_outbound_thread) {
        return OPRT_OK;
    }

    THREAD_CFG_T cfg = {0};
    cfg.stackDepth   = MIMI_OUTBOUND_STACK;
    cfg.priority     = THREAD_PRIO_1;
    cfg.thrdname     = "mimi_outbound";

    return tal_thread_create_and_start(&s_outbound_thread, NULL, NULL, outbound_dispatch_task, NULL, &cfg);
}

static void mimi_network_init(void)
{
    netmgr_type_e type = 0;
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    type |= NETCONN_WIRED;
#endif
#if defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
    type |= NETCONN_CELLULAR;
#endif

    if (type == 0) {
        MIMI_LOGI(TAG, "skip netmgr init: wifi handled by wifi_manager");
        return;
    }

    OPERATE_RET rt = netmgr_init(type);
    if (rt == OPRT_OK) {
        MIMI_LOGI(TAG, "netmgr initialized (non-wifi), type=0x%x", type);
    } else {
        MIMI_LOGW(TAG, "netmgr_init failed: %d", rt);
    }
}

static void start_online_services(const char *mode)
{
    mimi_channel_mode_t channel_mode = load_channel_mode();
    bool                enable_tg    = (channel_mode == MIMI_CHANNEL_MODE_TELEGRAM);
    bool                enable_dc    = (channel_mode == MIMI_CHANNEL_MODE_DISCORD);
    bool                enable_fs    = (channel_mode == MIMI_CHANNEL_MODE_FEISHU);

    OPERATE_RET rt = start_outbound_dispatcher();
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "start_outbound_dispatcher failed: %d", rt);
    }

    rt = agent_loop_start();
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "agent_loop_start failed: %d", rt);
    }

    rt = cron_service_start();
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "cron_service_start failed: %d", rt);
    }

    rt = heartbeat_start();
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "heartbeat_start failed: %d", rt);
    }

    if (enable_tg) {
        rt = telegram_bot_start();
        if (rt == OPRT_NOT_FOUND) {
            MIMI_LOGW(TAG, "telegram credential missing, telegram service disabled");
        } else if (rt != OPRT_OK) {
            MIMI_LOGW(TAG, "telegram_bot_start failed: %d", rt);
        }
    } else {
        MIMI_LOGI(TAG, "telegram disabled by channel_mode=%s", channel_mode_str(channel_mode));
    }

    if (enable_dc) {
        rt = discord_bot_start();
        if (rt == OPRT_NOT_FOUND) {
            MIMI_LOGW(TAG, "discord credential missing, discord service disabled");
        } else if (rt != OPRT_OK) {
            MIMI_LOGW(TAG, "discord_bot_start failed: %d", rt);
        }
    } else {
        MIMI_LOGI(TAG, "discord disabled by channel_mode=%s", channel_mode_str(channel_mode));
    }

    if (enable_fs) {
        rt = feishu_bot_start();
        if (rt == OPRT_NOT_FOUND) {
            MIMI_LOGW(TAG, "feishu credentials missing, feishu service disabled");
        } else if (rt != OPRT_OK) {
            MIMI_LOGW(TAG, "feishu_bot_start failed: %d", rt);
        }
    } else {
        MIMI_LOGI(TAG, "feishu disabled by channel_mode=%s", channel_mode_str(channel_mode));
    }

    rt = ws_server_start();
    if (rt != OPRT_OK) {
        MIMI_LOGW(TAG, "ws_server_start failed: %d", rt);
    }

    MIMI_LOGI(TAG, "online services started in %s mode, channel_mode=%s", mode ? mode : "unknown",
              channel_mode_str(channel_mode));
}

void mimi_app_main(void)
{
    mimi_runtime_init();

    MIMI_LOGI(TAG, "MimiClaw TuyaOpen app start");
    MIMI_LOGI(TAG, "free heap: %d", tal_system_get_free_heap_size());

    (void)init_storage();
    (void)message_bus_init();
    (void)memory_store_init();
    (void)skill_loader_init();
    (void)session_mgr_init();
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    (void)wifi_manager_init();
#endif
    (void)http_proxy_init();
    (void)telegram_bot_init();
    (void)discord_bot_init();
    (void)feishu_bot_init();
    (void)llm_proxy_init();
    (void)tool_registry_init();
    (void)cron_service_init();
    (void)heartbeat_init();
    (void)agent_loop_init();

#if OPERATING_SYSTEM == SYSTEM_LINUX
    MIMI_LOGI(TAG, "serial CLI disabled on Linux host");
#else
    (void)serial_cli_init();
#endif

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
    TUYA_LwIP_Init();
#endif
    mimi_network_init();

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    OPERATE_RET wifi_rt = wifi_manager_start();
    if (wifi_rt == OPRT_OK) {
        MIMI_LOGI(TAG, "current target ssid: %s", wifi_manager_get_target_ssid());
        MIMI_LOGI(TAG, "waiting for WiFi connection...");

        if (wifi_manager_wait_connected(30000) == OPRT_OK) {
            MIMI_LOGI(TAG, "WiFi connected: %s", wifi_manager_get_ip());
            start_online_services("wifi");
        } else {
            MIMI_LOGW(TAG, "WiFi connection timeout. Check WiFi SSID configuration");
        }
    } else {
        if (wifi_rt == OPRT_NOT_FOUND) {
            MIMI_LOGW(TAG, "No WiFi credentials. Set WiFi SSID configuration");
        } else {
            MIMI_LOGW(TAG, "wifi_manager_start failed: %d", wifi_rt);
        }
    }
#else
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    MIMI_LOGI(TAG, "ENABLE_WIFI disabled, start online services with wired network");
    start_online_services("wired");
#else
    MIMI_LOGW(TAG, "both WiFi and wired are disabled, skip network services");
#endif
#endif

    MIMI_LOGI(TAG, "MimiClaw started");
}

int user_main(void)
{
    mimi_app_main();
    while (1) {
        tal_system_sleep(1000);
    }
    return 0;
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    return user_main();
}
#else
static THREAD_HANDLE s_main_thread = NULL;

static void mimi_main_thread(void *arg)
{
    (void)arg;
    user_main();
}

void tuya_app_main(void)
{
    THREAD_CFG_T cfg = {0};
    cfg.stackDepth   = 1024 * 6;
    cfg.priority     = THREAD_PRIO_1;
    cfg.thrdname     = "mimi_main";
    (void)tal_thread_create_and_start(&s_main_thread, NULL, NULL, mimi_main_thread, NULL, &cfg);
}
#endif
