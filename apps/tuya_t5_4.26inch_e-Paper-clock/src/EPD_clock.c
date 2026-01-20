/**
 * @file EPD_clock.c
 * @brief Main application for the 4.26" e-Paper clock example.
 *
 * This file initializes Tuya IoT, network provisioning, button handling, and
 * renders the e-Paper clock UI. It also handles DP-driven settings such as
 * 12/24-hour time format and light/dark theme.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */
#include <stdlib.h>
#include <string.h>

#include "EPD_Test.h"
#include "EPD_4in26.h"

#include "cJSON.h"
#include "netmgr.h"
#include "tal_cli.h"
#include "tal_kv.h"
#include "tal_sw_timer.h"
#include "tal_system.h"

#include "tdl_button_manage.h"
#include "tdd_button_gpio.h"

#include "tuya_authorize.h"
#include "tuya_config.h"
#include "tuya_error_code.h"
#include "tuya_iot.h"
#include "tuya_iot_dp.h"

#include "clock_net.h"
#include "clock_time.h"
#include "clock_types.h"
#include "clock_ui.h"

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
#include "netconn_wired.h"
#endif
#if defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
#include "netconn_cellular.h"
#endif

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.0.0"
#endif

#ifndef EPD_CLOCK_FULL_REFRESH_MINUTES
// Full refresh periodically to reduce ghosting.
#define EPD_CLOCK_FULL_REFRESH_MINUTES 30
#endif

#ifndef EPD_CLOCK_USE_PARTIAL_UPDATE
#define EPD_CLOCK_USE_PARTIAL_UPDATE 0
#endif

#ifndef EPD_CLOCK_UI_DEBOUNCE_MS
#define EPD_CLOCK_UI_DEBOUNCE_MS 800
#endif

#ifndef EPD_CLOCK_LOOP_DELAY_MS
#define EPD_CLOCK_LOOP_DELAY_MS 200
#endif

static tuya_iot_client_t g_tuya_client;
static tuya_iot_license_t g_tuya_license;
static THREAD_HANDLE g_tuya_iot_thread = NULL;
static volatile BOOL_T g_cloud_connected = FALSE;

// DPs (must match Tuya IoT platform DPIDs)
#define DPID_TIME_MODE  18 // enum: 0=24h, 1=12h
#define DPID_NIGHT_MODE 28 // enum: mode_1..mode_5 (we map to 2 themes)

// Local persistent settings
#define KV_CLOCK_TIME_MODE "clock.time_mode"
#define KV_CLOCK_THEME     "clock.theme"

static volatile clock_time_mode_t g_time_mode = CLOCK_TIME_MODE_24H;
static volatile clock_theme_t g_theme = CLOCK_THEME_LIGHT;

static TDL_BUTTON_HANDLE g_netcfg_key_handle = NULL;
static volatile BOOL_T g_netcfg_key_req = FALSE;

static void __clock_settings_load(void)
{
    uint8_t *buf = NULL;
    size_t len = 0;

    if (tal_kv_get(KV_CLOCK_TIME_MODE, &buf, &len) == OPRT_OK && buf && len >= 1) {
        uint8_t v = buf[0];
        if (v <= (uint8_t)CLOCK_TIME_MODE_12H) {
            g_time_mode = (clock_time_mode_t)v;
        }
    }
    if (buf) {
        tal_kv_free(buf);
        buf = NULL;
    }

    if (tal_kv_get(KV_CLOCK_THEME, &buf, &len) == OPRT_OK && buf && len >= 1) {
        uint8_t v = buf[0];
        if (v <= (uint8_t)CLOCK_THEME_LIGHT) {
            g_theme = (clock_theme_t)v;
        }
    }
    if (buf) {
        tal_kv_free(buf);
        buf = NULL;
    }

    PR_NOTICE("Settings: time_mode=%s theme=%s", (g_time_mode == CLOCK_TIME_MODE_12H) ? "12h" : "24h",
              (g_theme == CLOCK_THEME_DARK) ? "dark" : "light");
}

static void __clock_settings_save(void)
{
    int ret = 0;
    uint8_t v = (uint8_t)g_time_mode;

    ret = tal_kv_set(KV_CLOCK_TIME_MODE, &v, 1);
    if (ret != OPRT_OK) {
        PR_ERR("tal_kv_set %s failed: %d", KV_CLOCK_TIME_MODE, ret);
    }

    v = (uint8_t)g_theme;
    ret = tal_kv_set(KV_CLOCK_THEME, &v, 1);
    if (ret != OPRT_OK) {
        PR_ERR("tal_kv_set %s failed: %d", KV_CLOCK_THEME, ret);
    }
}

static void __clock_settings_report(tuya_iot_client_t *client)
{
    int ret = 0;

    if (!client || !tuya_iot_activated(client)) {
        return;
    }
    if (client->activate.devid[0] == '\0') {
        return;
    }

    dp_obj_t dps[2] = {0};

    dps[0].id = DPID_TIME_MODE;
    dps[0].type = PROP_ENUM;
    dps[0].value.dp_enum = (uint32_t)g_time_mode;

    dps[1].id = DPID_NIGHT_MODE;
    dps[1].type = PROP_ENUM;
    // night_mode range is mode_1..mode_5 (0..4). We only use 0/1.
    dps[1].value.dp_enum = (uint32_t)g_theme;

    ret = tuya_iot_dp_obj_report(client, client->activate.devid, dps, 2, 0);
    if (ret != OPRT_OK) {
        PR_ERR("tuya_iot_dp_obj_report failed: %d", ret);
    }
}

static void __clock_enter_pairing_mode_request(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_NOTICE("KEY: (re)enter pairing mode: clear netinfo + reset iot");

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    // Clear stored WiFi credentials regardless of activation status.
    rt = netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_RESET, NULL);
    if (rt != OPRT_OK) {
        PR_ERR("netmgr_conn_set reset failed: %d", rt);
    }
#endif

    // Reset state machine so it re-enters activation flow and starts BLE/AP netcfg again.
    int ret = tuya_iot_reset(&g_tuya_client);
    if (ret != OPRT_OK) {
        PR_ERR("tuya_iot_reset failed: %d", ret);
    }
}

static void __clock_netcfg_key_event_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    (void)name;
    (void)argc;

    if (event == TDL_BUTTON_PRESS_SINGLE_CLICK || event == TDL_BUTTON_LONG_PRESS_START) {
        g_netcfg_key_req = TRUE;
    }
}

static void __clock_netcfg_key_init_once(void)
{
#if defined(EPD_CLOCK_NETCFG_KEY_ENABLE) && (EPD_CLOCK_NETCFG_KEY_ENABLE == 1)
    static BOOL_T inited = FALSE;
    if (inited) {
        return;
    }
    inited = TRUE;

    static char key_name[] = "clock_key";

    BUTTON_GPIO_CFG_T hw = {
        .pin = (TUYA_GPIO_NUM_E)EPD_CLOCK_NETCFG_KEY_PIN,
        .level = (TUYA_GPIO_LEVEL_E)EPD_CLOCK_NETCFG_KEY_ACTIVE_LEVEL,
        .mode = BUTTON_TIMER_SCAN_MODE,
        .pin_type.gpio_pull = (EPD_CLOCK_NETCFG_KEY_ACTIVE_LEVEL == TUYA_GPIO_LEVEL_LOW) ? TUYA_GPIO_PULLUP : TUYA_GPIO_PULLDOWN,
    };

    OPERATE_RET rt = tdd_gpio_button_register(key_name, &hw);
    if (rt != OPRT_OK) {
        PR_ERR("netcfg key register failed: %d (pin=%d)", rt, (int)hw.pin);
        return;
    }

    TDL_BUTTON_CFG_T sw = {
        .long_start_valid_time = (uint16_t)EPD_CLOCK_NETCFG_KEY_LONGPRESS_MS,
        .long_keep_timer = 100,
        .button_debounce_time = 60,
        .button_repeat_valid_count = 0,
        .button_repeat_valid_time = 0,
    };

    rt = tdl_button_create(key_name, &sw, &g_netcfg_key_handle);
    if (rt != OPRT_OK) {
        PR_ERR("netcfg key create failed: %d", rt);
        g_netcfg_key_handle = NULL;
        return;
    }

    // Don't filter the first press after boot.
    (void)tdl_button_set_ready_flag(key_name, TRUE);

    tdl_button_event_register(g_netcfg_key_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, __clock_netcfg_key_event_cb);
    tdl_button_event_register(g_netcfg_key_handle, TDL_BUTTON_LONG_PRESS_START, __clock_netcfg_key_event_cb);
#endif
}

static void cli_clock_reset(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    tal_cli_echo("Factory reset: clear WiFi + activation, then re-pair in Tuya APP...");
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    // Always clear stored WiFi credentials (even when not activated yet).
    netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_RESET, NULL);
#endif
    int ret = tuya_iot_reset(&g_tuya_client);
    if (ret != OPRT_OK) {
        tal_cli_echo("Factory reset request failed.");
    }

    // Ensure a clean reboot so the device re-enters pairing flow immediately.
    tal_system_sleep(200);
    tal_system_reset();
}

static void cli_clock_wifi(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    BOOL_T activated = tuya_iot_activated(&g_tuya_client) ? TRUE : FALSE;
    tal_cli_echo(activated ? "activated: 1" : "activated: 0");

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    netmgr_status_e status = NETMGR_LINK_DOWN;
    (void)netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);

    netconn_wifi_info_t wifi_info = {0};
    (void)netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info);

    NW_IP_S ip = {0};
    (void)netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_IP, &ip);

    char buf[128];
    snprintf(buf, sizeof(buf), "link: %s", NETMGR_STATUS_TO_STR(status));
    tal_cli_echo(buf);
    snprintf(buf, sizeof(buf), "ssid: %s", wifi_info.ssid[0] ? wifi_info.ssid : "-");
    tal_cli_echo(buf);
    snprintf(buf, sizeof(buf), "ip: %s", ip.ip[0] ? ip.ip : "-");
    tal_cli_echo(buf);
#else
    tal_cli_echo("wifi: disabled");
#endif
}

static const cli_cmd_t s_clock_cli_cmd[] = {{
    .name = "clock-reset",
    .help = "Factory reset and enter pairing mode",
    .func = cli_clock_reset,
}, {
    .name = "clock-wifi",
    .help = "Show current WiFi/activation status",
    .func = cli_clock_wifi,
}};

static void clock_cli_init(void)
{
    int ret = 0;

    ret = tal_cli_cmd_register((cli_cmd_t *)&s_clock_cli_cmd, sizeof(s_clock_cli_cmd) / sizeof(s_clock_cli_cmd[0]));
    if (ret != OPRT_OK) {
        PR_ERR("tal_cli_cmd_register failed: %d", ret);
    }
}

static UDOUBLE image_bytes(UWORD width, UWORD height)
{
    return ((width % 8 == 0) ? (width / 8) : (width / 8 + 1)) * height;
}

#if defined(EPD_CLOCK_USE_PARTIAL_UPDATE) && (EPD_CLOCK_USE_PARTIAL_UPDATE == 1)
static void copy_region_1bpp_aligned(const UBYTE *src_full, UBYTE *dst, UWORD x, UWORD y, UWORD w, UWORD h)
{
    if (!src_full || !dst || w == 0 || h == 0) {
        return;
    }

    if ((x % 8) != 0 || (w % 8) != 0) {
        PR_ERR("partial region requires 8px alignment: x=%u w=%u", (unsigned)x, (unsigned)w);
        return;
    }

    const UWORD full_stride = (UWORD)(EPD_4in26_WIDTH / 8);
    const UWORD dst_stride = (UWORD)(w / 8);
    const UWORD x_byte = (UWORD)(x / 8);

    for (UWORD row = 0; row < h; row++) {
        const UWORD src_off = (UWORD)((y + row) * full_stride + x_byte);
        const UWORD dst_off = (UWORD)(row * dst_stride);
        memcpy(dst + dst_off, src_full + src_off, dst_stride);
    }
}
#endif

static void clock_user_event_handler_on(tuya_iot_client_t *client, tuya_event_msg_t *event)
{
    (void)client;

    switch (event->id) {
    case TUYA_EVENT_MQTT_CONNECTED:
        g_cloud_connected = TRUE;
        __clock_settings_report(client);
        break;

    case TUYA_EVENT_MQTT_DISCONNECT:
        g_cloud_connected = FALSE;
        break;

    case TUYA_EVENT_TIMESTAMP_SYNC:
        tal_time_set_posix((TIME_T)event->value.asInteger, 1);
        clock_time_set_source(CLOCK_TIME_SRC_CLOUD);
        break;

    case TUYA_EVENT_DP_RECEIVE_OBJ: {
        int ret = 0;

        dp_obj_recv_t *dpobj = event->value.dpobj;
        if (!dpobj) {
            break;
        }

        BOOL_T changed = FALSE;

        for (uint32_t index = 0; index < dpobj->dpscnt; index++) {
            dp_obj_t *dp = dpobj->dps + index;
            if (dp->type != PROP_ENUM) {
                continue;
            }

            if (dp->id == DPID_TIME_MODE) {
                if (dp->value.dp_enum <= 1) {
                    clock_time_mode_t new_mode = (dp->value.dp_enum == 0) ? CLOCK_TIME_MODE_24H : CLOCK_TIME_MODE_12H;
                    if (g_time_mode != new_mode) {
                        g_time_mode = new_mode;
                        PR_NOTICE("DP time_mode => %s", (new_mode == CLOCK_TIME_MODE_12H) ? "12h" : "24h");
                        changed = TRUE;
                    }
                }
            } else if (dp->id == DPID_NIGHT_MODE) {
                // Map mode_1..mode_5 to 2 themes:
                // - mode_1 (0) => dark (black bg, white text)
                // - other      => light (white bg, black text)
                clock_theme_t new_theme = (dp->value.dp_enum == 0) ? CLOCK_THEME_DARK : CLOCK_THEME_LIGHT;
                if (g_theme != new_theme) {
                    g_theme = new_theme;
                    PR_NOTICE("DP night_mode => %s", (new_theme == CLOCK_THEME_DARK) ? "dark" : "light");
                    changed = TRUE;
                }
            }
        }

        if (changed) {
            __clock_settings_save();
            __clock_settings_report(client);
        }

        // ACK and mirror to cloud/app (keep Tuya panel in sync).
        ret = tuya_iot_dp_obj_report(client, dpobj->devid, dpobj->dps, dpobj->dpscnt, 0);
        if (ret != OPRT_OK) {
            PR_ERR("tuya_iot_dp_obj_report ack failed: %d", ret);
        }
    } break;

    default:
        break;
    }
}

static bool clock_user_network_check(void)
{
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    return status == NETMGR_LINK_DOWN ? false : true;
}

static void tuya_iot_thread(void *arg)
{
    (void)arg;
    int ret = 0;
    OPERATE_RET rt = OPRT_OK;

    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_malloc, .free_fn = tal_free});

    ret = tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });
    if (ret != OPRT_OK) {
        PR_ERR("tal_kv_init failed: %d", ret);
        return;
    }
    __clock_settings_load();

    rt = tal_sw_timer_init();
    if (rt != OPRT_OK) {
        PR_ERR("tal_sw_timer_init failed: %d", rt);
        return;
    }

    rt = tal_workq_init();
    if (rt != OPRT_OK) {
        PR_ERR("tal_workq_init failed: %d", rt);
        return;
    }

    ret = tal_cli_init();
    if (ret != OPRT_OK) {
        PR_ERR("tal_cli_init failed: %d", ret);
        return;
    }
    clock_cli_init();

    rt = tuya_authorize_init();
    if (rt != OPRT_OK) {
        PR_ERR("tuya_authorize_init failed: %d", rt);
        return;
    }

    if (OPRT_OK != tuya_authorize_read(&g_tuya_license)) {
        g_tuya_license.uuid = (char *)TUYA_OPENSDK_UUID;
        g_tuya_license.authkey = (char *)TUYA_OPENSDK_AUTHKEY;
        PR_WARN("OTP license read failed; set TUYA_OPENSDK_UUID/AUTHKEY in examples/tuya_config.h");
    }

    if (strcmp(TUYA_PRODUCT_ID, "pidxxxxxxxxxxxxxxxx") == 0) {
        PR_WARN("TUYA_PRODUCT_ID is placeholder; set it in examples/tuya_config.h");
    }

    ret = tuya_iot_init(&g_tuya_client, &(const tuya_iot_config_t){
                                               .software_ver = PROJECT_VERSION,
                                               .productkey = TUYA_PRODUCT_ID,
                                               .uuid = g_tuya_license.uuid,
                                               .authkey = g_tuya_license.authkey,
                                               .event_handler = clock_user_event_handler_on,
                                               .network_check = clock_user_network_check,
                                           });
    if (ret != OPRT_OK) {
        PR_ERR("tuya_iot_init failed: %d", ret);
        return;
    }

    netmgr_type_e type = 0;
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    type |= NETCONN_WIFI;
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    type |= NETCONN_WIRED;
#endif
#if defined(ENABLE_CELLULAR) && (ENABLE_CELLULAR == 1)
    type |= NETCONN_CELLULAR;
#endif
    rt = netmgr_init(type);
    if (rt != OPRT_OK) {
        PR_ERR("netmgr_init failed: %d", rt);
        return;
    }

#if defined(EPD_CLOCK_NETCFG_KEY_ENABLE) && (EPD_CLOCK_NETCFG_KEY_ENABLE == 1)
    __clock_netcfg_key_init_once();
#endif

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    // Network setup:
    // - Development: connect WiFi directly with SSID/PASSWORD
    // - Production: use Tuya APP netcfg (BLE/AP) to provision
#if defined(EPD_CLOCK_USE_STATIC_WIFI) && (EPD_CLOCK_USE_STATIC_WIFI == 1)
    netconn_wifi_info_t wifi_info = {0};
    strncpy(wifi_info.ssid, EPD_CLOCK_WIFI_SSID, sizeof(wifi_info.ssid) - 1);
    wifi_info.ssid[sizeof(wifi_info.ssid) - 1] = '\0';
    strncpy(wifi_info.pswd, EPD_CLOCK_WIFI_PASSWORD, sizeof(wifi_info.pswd) - 1);
    wifi_info.pswd[sizeof(wifi_info.pswd) - 1] = '\0';
    rt = netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info);
    if (rt != OPRT_OK) {
        PR_ERR("netmgr_conn_set ssid/pswd failed: %d", rt);
        return;
    }

    // Even when WiFi is pre-configured, a token-get path is still required for first-time activation/binding.
    // Enable BLE/AP netcfg so the device can receive the activation token from Tuya APP.
    rt = netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_NETCFG, &(netcfg_args_t){.type = NETCFG_TUYA_BLE | NETCFG_TUYA_WIFI_AP});
    if (rt != OPRT_OK) {
        PR_ERR("netmgr_conn_set netcfg failed: %d", rt);
        return;
    }
#else
    rt = netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_NETCFG, &(netcfg_args_t){.type = NETCFG_TUYA_BLE | NETCFG_TUYA_WIFI_AP});
    if (rt != OPRT_OK) {
        PR_ERR("netmgr_conn_set netcfg failed: %d", rt);
        return;
    }
#endif
#endif

    ret = tuya_iot_start(&g_tuya_client);
    if (ret != OPRT_OK) {
        PR_ERR("tuya_iot_start failed: %d", ret);
        return;
    }

    for (;;) {
        if (g_netcfg_key_req) {
            g_netcfg_key_req = FALSE;
            __clock_enter_pairing_mode_request();
        }

        tuya_iot_yield(&g_tuya_client);
        tal_system_sleep(10);
    }
}

static void tuya_iot_start_once(void)
{
    if (g_tuya_iot_thread) {
        return;
    }

    THREAD_CFG_T cfg = {8192, 4, "tuya_iot"};
    OPERATE_RET ret = tal_thread_create_and_start(&g_tuya_iot_thread, NULL, NULL, tuya_iot_thread, NULL, &cfg);
    if (ret != OPRT_OK) {
        PR_ERR("create tuya_iot thread failed: %d", ret);
        g_tuya_iot_thread = NULL;
    }
}

int EPD_clock(void)
{
    if (DEV_Module_Init() != 0) {
        return -1;
    }

    clock_time_service_init_once();
    tuya_iot_start_once();
    clock_time_sync_start_once();

    EPD_4in26_Init();
    EPD_4in26_Clear();
    DEV_Delay_ms(300);

    if (!clock_ui_validate_layout()) {
        PR_ERR("Clock layout invalid: regions overlap/out-of-bounds.");
        return -1;
    }

    UBYTE *full_img = (UBYTE *)malloc(image_bytes(EPD_4in26_WIDTH, EPD_4in26_HEIGHT));
    if (!full_img) {
        return -1;
    }

    clock_ui_state_t state = {0};
    clock_time_get(&state);
    clock_net_info_get(&state.net);
    state.cloud_connected = (BOOL_T)g_cloud_connected;
    state.time_mode = (clock_time_mode_t)g_time_mode;
    state.theme = (clock_theme_t)g_theme;

    clock_ui_render(full_img, &state);
    EPD_4in26_Display_Base(full_img);

    UBYTE *date_img = NULL;
    UBYTE *time_img = NULL;
    UBYTE *stat_img = NULL;

#if defined(EPD_CLOCK_USE_PARTIAL_UPDATE) && (EPD_CLOCK_USE_PARTIAL_UPDATE == 1)
    date_img = (UBYTE *)malloc(image_bytes(EPD_CLOCK_DATE_W, EPD_CLOCK_DATE_H));
    time_img = (UBYTE *)malloc(image_bytes(EPD_CLOCK_TIME_W, EPD_CLOCK_TIME_H));
    stat_img = (UBYTE *)malloc(image_bytes(EPD_CLOCK_STAT_W, EPD_CLOCK_STAT_H));
    if (!date_img || !time_img || !stat_img) {
        PR_ERR("malloc region buffers failed");
        free(date_img);
        free(time_img);
        free(stat_img);
        date_img = time_img = stat_img = NULL;
    }
#endif

    int last_hour = state.local.tm_hour;
    int last_min = state.local.tm_min;
    int last_day = state.local.tm_mday;
    int last_mon = state.local.tm_mon;
    int last_year = state.local.tm_year;
    BOOL_T last_synced = state.time_synced;
    clock_time_src_t last_src = state.time_src;
    clock_time_mode_t last_time_mode = state.time_mode;
    clock_theme_t last_theme = state.theme;
    netmgr_status_e last_link = state.net.link;
    char last_ip[40] = {0};
    strncpy(last_ip, state.net.ip, sizeof(last_ip) - 1);
    last_ip[sizeof(last_ip) - 1] = '\0';
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    char last_ssid[WIFI_SSID_LEN + 1] = {0};
#else
    char last_ssid[32 + 1] = {0};
#endif
    strncpy(last_ssid, state.net.ssid, sizeof(last_ssid) - 1);
    last_ssid[sizeof(last_ssid) - 1] = '\0';
    BOOL_T last_cloud = state.cloud_connected;

    SYS_TIME_T last_refresh_ms = tal_system_get_millisecond();
    uint32_t full_refresh_min_count = 0;

    for (;;) {
        clock_ui_state_t cur = {0};
        clock_time_get(&cur);
        clock_net_info_get(&cur.net);
        cur.cloud_connected = (BOOL_T)g_cloud_connected;
        cur.time_mode = (clock_time_mode_t)g_time_mode;
        cur.theme = (clock_theme_t)g_theme;

        BOOL_T dirty_time = FALSE;
        BOOL_T dirty_date = FALSE;
        BOOL_T dirty_stat = FALSE;
        BOOL_T dirty_theme = (cur.theme != last_theme) ? TRUE : FALSE;

        // Time region: update on hour/minute change or when sync state changes.
        if (cur.local.tm_hour != last_hour || cur.local.tm_min != last_min || cur.time_synced != last_synced) {
            dirty_time = TRUE;
        }
        if (cur.time_mode != last_time_mode) {
            dirty_time = TRUE;
        }

        // Header/date region: shows either date (when synced) or network state (when not synced).
        if (cur.time_synced != last_synced) {
            dirty_date = TRUE;
        } else if (dirty_theme) {
            dirty_date = TRUE;
        } else if (!cur.time_synced) {
            if (cur.net.link != last_link) {
                dirty_date = TRUE;
            }
        } else {
            if (cur.local.tm_mday != last_day || cur.local.tm_mon != last_mon || cur.local.tm_year != last_year) {
                dirty_date = TRUE;
            }
        }

        // Status region: network + cloud + time source state.
        if (cur.net.link != last_link || strncmp(cur.net.ip, last_ip, sizeof(last_ip)) != 0 ||
            strncmp(cur.net.ssid, last_ssid, sizeof(last_ssid)) != 0 || cur.cloud_connected != last_cloud ||
            cur.time_synced != last_synced || cur.time_src != last_src || cur.time_mode != last_time_mode || dirty_theme) {
            dirty_stat = TRUE;
        }

        BOOL_T dirty_any = (dirty_time || dirty_date || dirty_stat) ? TRUE : FALSE;

        SYS_TIME_T now_ms = tal_system_get_millisecond();
        if (dirty_any && (uint32_t)(now_ms - last_refresh_ms) >= (uint32_t)EPD_CLOCK_UI_DEBOUNCE_MS) {
            clock_ui_render(full_img, &cur);

            // Periodic base refresh to reduce ghosting.
            if (cur.time_synced && cur.local.tm_min != last_min) {
                full_refresh_min_count++;
            }

            BOOL_T force_full = FALSE;
            if (cur.time_synced != last_synced) {
                // Time sync transition: do a full refresh once for stability.
                force_full = TRUE;
            }
            if (dirty_theme) {
                // Theme change affects the whole screen.
                force_full = TRUE;
            }
            if (cur.time_synced && full_refresh_min_count >= EPD_CLOCK_FULL_REFRESH_MINUTES) {
                force_full = TRUE;
            }

            if (force_full) {
                EPD_4in26_Display_Base(full_img);
                if (cur.time_synced && full_refresh_min_count >= EPD_CLOCK_FULL_REFRESH_MINUTES) {
                    full_refresh_min_count = 0;
                }
            } else if (date_img && time_img && stat_img) {
#if defined(EPD_CLOCK_USE_PARTIAL_UPDATE) && (EPD_CLOCK_USE_PARTIAL_UPDATE == 1)
                if (dirty_date) {
                    copy_region_1bpp_aligned(full_img, date_img, EPD_CLOCK_DATE_X, EPD_CLOCK_DATE_Y, EPD_CLOCK_DATE_W, EPD_CLOCK_DATE_H);
                    EPD_4in26_Display_Part(date_img, EPD_CLOCK_DATE_X, EPD_CLOCK_DATE_Y, EPD_CLOCK_DATE_W, EPD_CLOCK_DATE_H);
                }
                if (dirty_time) {
                    copy_region_1bpp_aligned(full_img, time_img, EPD_CLOCK_TIME_X, EPD_CLOCK_TIME_Y, EPD_CLOCK_TIME_W, EPD_CLOCK_TIME_H);
                    EPD_4in26_Display_Part(time_img, EPD_CLOCK_TIME_X, EPD_CLOCK_TIME_Y, EPD_CLOCK_TIME_W, EPD_CLOCK_TIME_H);
                }
                if (dirty_stat) {
                    copy_region_1bpp_aligned(full_img, stat_img, EPD_CLOCK_STAT_X, EPD_CLOCK_STAT_Y, EPD_CLOCK_STAT_W, EPD_CLOCK_STAT_H);
                    EPD_4in26_Display_Part(stat_img, EPD_CLOCK_STAT_X, EPD_CLOCK_STAT_Y, EPD_CLOCK_STAT_W, EPD_CLOCK_STAT_H);
                }
#else
                EPD_4in26_Display_Base(full_img);
#endif
            } else {
                // Fallback: full-screen base update (most stable).
                EPD_4in26_Display_Base(full_img);
            }

            last_refresh_ms = now_ms;

            last_hour = cur.local.tm_hour;
            last_min = cur.local.tm_min;
            last_day = cur.local.tm_mday;
            last_mon = cur.local.tm_mon;
            last_year = cur.local.tm_year;
            last_synced = cur.time_synced;
            last_src = cur.time_src;
            last_time_mode = cur.time_mode;
            last_theme = cur.theme;
            last_link = cur.net.link;
            strncpy(last_ip, cur.net.ip, sizeof(last_ip) - 1);
            last_ip[sizeof(last_ip) - 1] = '\0';
            strncpy(last_ssid, cur.net.ssid, sizeof(last_ssid) - 1);
            last_ssid[sizeof(last_ssid) - 1] = '\0';
            last_cloud = cur.cloud_connected;
        }

        DEV_Delay_ms(EPD_CLOCK_LOOP_DELAY_MS);
    }
}

