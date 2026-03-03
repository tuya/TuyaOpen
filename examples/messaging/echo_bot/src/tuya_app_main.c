/**
 * @file tuya_app_main.c
 * @brief Echo bot: WiFi + channel switch (telegram/discord/feishu) + echo reply.
 *        Receives message -> replies with the same content. No LLM/agent.
 */

#include "echo_cli.h"
#include "echo_base.h"
#include "im_api.h"
#include "echo_config.h"
#include "wifi_manager.h"

#include "netmgr.h"
#include "tal_fs.h"
#include "tal_system.h"
#include "tkl_output.h"
#include "tuya_register_center.h"
#include "tuya_tls.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
#include "lwip_init.h"
#endif

static const char *TAG               = "echo_bot";
static THREAD_HANDLE s_outbound_thd = NULL;
static THREAD_HANDLE s_echo_thd     = NULL;

typedef enum {
    ECHO_MODE_TELEGRAM = 0,
    ECHO_MODE_DISCORD,
    ECHO_MODE_FEISHU,
} echo_channel_mode_t;

static bool str_ieq(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static echo_channel_mode_t parse_channel_mode(const char *mode)
{
    if (!mode || mode[0] == '\0') return ECHO_MODE_TELEGRAM;
    if (str_ieq(mode, "telegram")) return ECHO_MODE_TELEGRAM;
    if (str_ieq(mode, "discord")) return ECHO_MODE_DISCORD;
    if (str_ieq(mode, "feishu")) return ECHO_MODE_FEISHU;
    return ECHO_MODE_TELEGRAM;
}

static const char *channel_mode_str(echo_channel_mode_t mode)
{
    switch (mode) {
    case ECHO_MODE_TELEGRAM: return "telegram";
    case ECHO_MODE_DISCORD: return "discord";
    case ECHO_MODE_FEISHU: return "feishu";
    default: return "feishu";
    }
}

static echo_channel_mode_t load_channel_mode(void)
{
    char mode_buf[24] = {0};
    if (IM_SECRET_CHANNEL_MODE[0] != '\0') {
        snprintf(mode_buf, sizeof(mode_buf), "%s", IM_SECRET_CHANNEL_MODE);
    } else {
        snprintf(mode_buf, sizeof(mode_buf), "telegram");
    }
    char kv_mode[24] = {0};
    if (im_kv_get_string(IM_NVS_BOT, IM_NVS_KEY_CHANNEL_MODE, kv_mode, sizeof(kv_mode)) == OPRT_OK &&
        kv_mode[0] != '\0') {
        snprintf(mode_buf, sizeof(mode_buf), "%s", kv_mode);
    }
    if (!(str_ieq(mode_buf, "telegram") || str_ieq(mode_buf, "discord") || str_ieq(mode_buf, "feishu"))) {
        snprintf(mode_buf, sizeof(mode_buf), "telegram");
    }
    return parse_channel_mode(mode_buf);
}

static void outbound_dispatch_task(void *arg)
{
    (void)arg;
    ECHO_LOGI(TAG, "outbound dispatcher started");
    while (1) {
        im_msg_t msg = {0};
        if (message_bus_pop_outbound(&msg, ECHO_WAIT_FOREVER) != OPRT_OK) continue;
        if (strcmp(msg.channel, IM_CHAN_TELEGRAM) == 0) {
            (void)telegram_send_message(msg.chat_id, msg.content ? msg.content : "");
        } else if (strcmp(msg.channel, IM_CHAN_DISCORD) == 0) {
            (void)discord_send_message(msg.chat_id, msg.content ? msg.content : "");
        } else if (strcmp(msg.channel, IM_CHAN_FEISHU) == 0) {
            (void)feishu_send_message(msg.chat_id, msg.content ? msg.content : "");
        } else if (strcmp(msg.channel, "system") == 0) {
            ECHO_LOGI(TAG, "system msg: %s", msg.content ? msg.content : "");
        }
        free(msg.content);
    }
}

#define ECHO_INBOUND_POLL_MS  100

static void echo_loop_task(void *arg)
{
    (void)arg;
    ECHO_LOGI(TAG, "echo loop started");
    while (1) {
        im_msg_t in = {0};
        if (message_bus_pop_inbound(&in, ECHO_INBOUND_POLL_MS) != OPRT_OK) continue;
        im_msg_t out = {0};
        strncpy(out.channel, in.channel, sizeof(out.channel) - 1);
        out.channel[sizeof(out.channel) - 1] = '\0';
        strncpy(out.chat_id, in.chat_id, sizeof(out.chat_id) - 1);
        out.chat_id[sizeof(out.chat_id) - 1] = '\0';
        out.content = in.content ? strdup(in.content) : strdup("");
        if (out.content) {
            if (message_bus_push_outbound(&out) != OPRT_OK) {
                free(out.content);
            }
        }
        tal_free(in.content);
    }
}

static OPERATE_RET start_outbound_dispatcher(void)
{
    if (s_outbound_thd) return OPRT_OK;
    THREAD_CFG_T cfg = {0};
    cfg.stackDepth = IM_OUTBOUND_STACK;
    cfg.priority   = THREAD_PRIO_1;
    cfg.thrdname   = "echo_out";
    return tal_thread_create_and_start(&s_outbound_thd, NULL, NULL, outbound_dispatch_task, NULL, &cfg);
}

static OPERATE_RET start_echo_loop(void)
{
    if (s_echo_thd) return OPRT_OK;
    THREAD_CFG_T cfg = {0};
    cfg.stackDepth = 8 * 1024;
    cfg.priority   = THREAD_PRIO_1;
    cfg.thrdname   = "echo_loop";
    return tal_thread_create_and_start(&s_echo_thd, NULL, NULL, echo_loop_task, NULL, &cfg);
}

static void start_channel_services(echo_channel_mode_t mode)
{
    (void)start_outbound_dispatcher();
    (void)start_echo_loop();

    bool enable_tg = (mode == ECHO_MODE_TELEGRAM);
    bool enable_dc = (mode == ECHO_MODE_DISCORD);
    bool enable_fs = (mode == ECHO_MODE_FEISHU);

    if (enable_tg) {
        OPERATE_RET rt = telegram_bot_start();
        if (rt != OPRT_OK && rt != OPRT_NOT_FOUND)
            ECHO_LOGW(TAG, "telegram_bot_start failed: %d", rt);
    }
    if (enable_dc) {
        OPERATE_RET rt = discord_bot_start();
        if (rt != OPRT_OK && rt != OPRT_NOT_FOUND)
            ECHO_LOGW(TAG, "discord_bot_start failed: %d", rt);
    }
    if (enable_fs) {
        OPERATE_RET rt = feishu_bot_start();
        if (rt != OPRT_OK && rt != OPRT_NOT_FOUND)
            ECHO_LOGW(TAG, "feishu_bot_start failed: %d", rt);
    }
    ECHO_LOGI(TAG, "channel mode=%s", channel_mode_str(mode));
}

static void runtime_init(void)
{
    static bool inited = false;
    if (inited) return;
    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_malloc, .free_fn = tal_free});
    (void)tal_log_init(TAL_LOG_LEVEL_INFO, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    (void)tal_kv_init(&(tal_kv_cfg_t){.seed = "echo_bot_seed", .key = "echo_bot_key"});
    (void)tal_sw_timer_init();
    (void)tal_workq_init();
    (void)tuya_tls_init();
    (void)tuya_register_center_init();
    inited = true;
}

static void network_init(void)
{
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    (void)netmgr_init(NETCONN_WIRED);
#elif defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
    (void)netmgr_init(NETCONN_CELLULAR);
#endif
}

int user_main(void)
{
    runtime_init();
    ECHO_LOGI(TAG, "Echo bot start, heap=%d", tal_system_get_free_heap_size());

    (void)message_bus_init();
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    (void)wifi_manager_init();
#endif
    (void)http_proxy_init();
    (void)telegram_bot_init();
    (void)discord_bot_init();
    (void)feishu_bot_init();

#if OPERATING_SYSTEM != SYSTEM_LINUX
    (void)echo_cli_init();
#endif

#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
    TUYA_LwIP_Init();
#endif
    network_init();

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    if (wifi_manager_start() == OPRT_OK) {
        if (wifi_manager_wait_connected(30000) == OPRT_OK) {
            echo_channel_mode_t mode = load_channel_mode();
            start_channel_services(mode);
        } else {
            ECHO_LOGW(TAG, "WiFi timeout; set_wifi then restart");
        }
    }
#else
    echo_channel_mode_t mode = load_channel_mode();
    start_channel_services(mode);
#endif

    ECHO_LOGI(TAG, "Echo bot ready");
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
static THREAD_HANDLE s_main_thd = NULL;
static void main_thd_entry(void *arg)
{
    (void)arg;
    user_main();
}
void tuya_app_main(void)
{
    THREAD_CFG_T cfg = {0};
    cfg.stackDepth = 1024 * 6;
    cfg.priority   = THREAD_PRIO_1;
    cfg.thrdname   = "echo_main";
    (void)tal_thread_create_and_start(&s_main_thd, NULL, NULL, main_thd_entry, NULL, &cfg);
}
#endif
