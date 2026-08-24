/**
 * @file netconn_wifi.c
 * @brief Implementation of WiFi network connection management for Tuya IoT devices.
 *
 * This file provides functions and structures to manage WiFi network connections,
 * including connection, disconnection, configuration, and event handling.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 * 2025-07-11   yangjie     Adjust WiFi priority
 *
 */

#include "netconn_wifi.h"
#include "netconn_registry.h"

/* The shared back-off arithmetic, adopted through its PURE layer only
 * (netmgr_retry_interval_ms() / netmgr_retry_advance()). That choice is the whole
 * reason two layers exist: the pure layer touches no state this file does not
 * already own, so the reconnect back-off moves out of here WITHOUT any change to
 * netconn_wifi.h - which matters, because that header is on the global public
 * include path and app code uses its types. conn.stat, conn.count, conn.table,
 * conn.table_size and conn.timer all stay exactly where they were. */
#include "netmgr_retry.h"

#include "tal_api.h"
#include "dev_evt.h"
#include "cJSON.h"
#include "ap_netcfg.h"
#include "tuya_lan.h"

/* Still needed, for exactly one symbol: TAL_NET_PROVIDER_DEFAULT in the static
 * initialiser below. The other data-plane call this file used to make -
 * tal_network_card_get_active_type(), asked to decide a provisioning policy - is
 * gone; see the note in __netconn_activate_token_get(). */
#include "tal_network_register.h"

#ifdef ENABLE_BLUETOOTH
#include "ble_mgr.h"
#include "ble_netcfg.h"
#include "tuya_ble_service.h"
#endif

typedef enum {
    NETCONN_WIFI_MSG_CONNECT,
    NETCONN_WIFI_MSG_DISCONNECT,
    NETCONN_WIFI_MSG_SCAN,
} netmgr_wifi_msg_type_t;

typedef struct {
    int type;
    netmgr_conn_wifi_t *handle;
} netmgr_wifi_msg_t;

netmgr_conn_wifi_t s_netmgr_wifi = {
    .base =
        {
            .pri = 1,
            .type = NETCONN_WIFI,
            .status = NETMGR_LINK_DOWN,
            .card_type = TAL_NET_PROVIDER_DEFAULT,
            .open = netconn_wifi_open,
            .close = netconn_wifi_close,
            .get = netconn_wifi_get,
            .set = netconn_wifi_set,
        },
    .ccode = {"CN"},
    /* conn.table / conn.table_size are deliberately NOT initialised here any
     * more. They used to hold
     *
     *     .table_size = NETCONN_WIFI_CONN_TABLE,   // 16
     *     .table      = {1, 3, 5, 10, 15, 20},     // 6 entries, [6..15] zero
     *
     * i.e. a declared length that did not match the real one, which put the
     * saturation point at attempt 15 instead of attempt 5 and made attempts 6 and
     * up arm the timer with a literal 0. That behaved only because
     * tal_sw_timer_start() keeps the previous interval on a 0 argument.
     * netmgr_retry.h records the whole trace.
     *
     * netconn_wifi_open() seeds both fields from netmgr_retry_table_assoc instead,
     * so the intervals have exactly one definition in the tree and the declared
     * count is the real count. Left zeroed here so that seeding can tell "nobody
     * set a table" from "a product set one". */
    .conn = {0},
};

/**
 * @brief Arm conn.timer for the next reconnect attempt and advance the counter.
 *
 * The eight lines this replaces appeared twice, verbatim, in __netconn_wifi_event()
 * and __netconn_wifi_conn_timer():
 *
 *     tal_sw_timer_start(wifi->conn.timer, wifi->conn.table[wifi->conn.count] * 1000, TAL_TIMER_ONCE);
 *     if (wifi->conn.count < wifi->conn.table_size - 1) {
 *         wifi->conn.count++;
 *     }
 *     wifi->conn.stat = NETCONN_WIFI_CONN_WAIT;
 *
 * netmgr_retry_advance() is that idiom exactly: it indexes with the counter BEFORE
 * bumping it, clamps the index at the last entry, and saturates the counter at
 * count - 1 rather than wrapping - all three of which the original depended on.
 *
 * Two things change, and neither changes the observable sequence:
 *
 *   - the interval is passed to tal_sw_timer_start() EXPLICITLY on every arm. The
 *     original passed 0 from attempt 6 onwards and relied on the timer keeping
 *     its previous interval, which in the steady failure loop was
 *     WIFI_CONN_TIMEOUT_MAX * 1000 rather than table[5] * 1000. Those two
 *     constants are both 20, so the observable back-off is unchanged - and the
 *     back-off table and the connect timeout stop being silently coupled, so
 *     retuning WIFI_CONN_TIMEOUT_MAX no longer moves the back-off;
 *   - `count - 1` cannot underflow on an empty table, because
 *     netmgr_retry_advance() guards it. The original would have computed
 *     `0 - 1` as UINT32_MAX and bumped the counter forever.
 *
 * NETCONN_CMD_RECONN_TABLE is unaffected: it writes conn.table and sets
 * conn.table_size to the caller's own count, and the view built here reads both,
 * so the long ULP back-off keeps its behaviour byte for byte.
 */
static void __netconn_wifi_backoff_wait(netmgr_conn_wifi_t *wifi)
{
    /* A view over the driver's existing fields - no copy, no allocation. The
     * bound NETMGR_RETRY_TABLE_MAX and NETCONN_WIFI_CONN_TABLE are both 16, which
     * is what makes this legal without a header change. */
    netmgr_retry_table_t table = {
        .entry = wifi->conn.table,
        .count = wifi->conn.table_size,
    };
    uint32_t interval_ms = netmgr_retry_advance(&table, &wifi->conn.count);

    /* An empty table answers 0, which means "retry immediately" for this consumer
     * (netmgr_retry_table_t.count spells out why the revalidation consumer reads
     * it the other way). 0 must NOT reach tal_sw_timer_start(), which would treat
     * it as "keep the previous interval" - the very quirk this function exists to
     * stop depending on - so "immediately" is spelled as one millisecond.
     * Unreachable in practice: netconn_wifi_open() always leaves a table. */
    if (0 == interval_ms) {
        interval_ms = 1;
    }

    tal_sw_timer_start(wifi->conn.timer, interval_ms, TAL_TIMER_ONCE);
    wifi->conn.stat = NETCONN_WIFI_CONN_WAIT;
}

/**
 * TRUE once netconn_wifi_open() has brought the WiFi stack up, i.e. once
 * tal_wifi_init() has succeeded. netconn_wifi_close() keys off this so that it
 * is a no-op both on a second call and when open() bailed out before there was
 * anything to tear down - netmgr_deinit() is required to be idempotent and may
 * run on netmgr_init()'s own error paths.
 *
 * It lives here rather than in netmgr_conn_wifi_t because netconn_wifi.h is on
 * the global public include path and M2 does not change it.
 */
static BOOL_T s_wifi_opened = FALSE;

static void __netconn_wifi_connect_process(void *msg)
{
    netmgr_wifi_msg_t *wifi_msg = (netmgr_wifi_msg_t *)msg;
    netmgr_conn_wifi_t *wifi = wifi_msg->handle;

    switch (wifi_msg->type) {
    case NETCONN_WIFI_MSG_CONNECT:
        PR_DEBUG("wifi connnet %s", wifi->conn.wifi_conn_info.ssid);
        tal_wifi_station_disconnect();
        tal_sw_timer_start(wifi->conn.timer, WIFI_CONN_TIMEOUT_MAX * 1000, TAL_TIMER_ONCE);
        wifi->conn.stat = NETCONN_WIFI_CONN_CHECK;
        tal_wifi_set_work_mode(WWM_STATION);
        /* ULP: keep awake while associating; released in __netconn_wifi_event
         * (TY_LP_NETCONN also self-releases via its 3min timeout). */
        tuya_dev_evt_notify(DEV_EVT_STA_CONNECT, ACTION_BEFORE, NULL);
        tal_wifi_station_connect((int8_t *)wifi->conn.wifi_conn_info.ssid, (int8_t *)wifi->conn.wifi_conn_info.pswd);
        break;

    case NETCONN_WIFI_MSG_DISCONNECT:
        tal_sw_timer_stop(wifi->conn.timer);
        wifi->conn.count = 0;
        wifi->conn.stat = NETCONN_WIFI_CONN_STOP;
        tal_wifi_station_disconnect();

        //! wait disconnect event
        tal_sw_timer_start(wifi->conn.timer, 1500, TAL_TIMER_ONCE);
        PR_DEBUG("auto connect stop %d", wifi->conn.stat);
        break;
    }

    tal_free(wifi_msg);
}

OPERATE_RET __netconn_wifi_connect(void)
{
    netmgr_conn_wifi_t *wifi = &s_netmgr_wifi;

    if (NETCONN_WIFI_CONN_LINKUP == wifi->conn.stat) {
        return OPRT_OK;
    }

    netmgr_wifi_msg_t *wifi_msg = tal_malloc(sizeof(netmgr_wifi_msg_t));
    if (NULL == wifi_msg) {
        return OPRT_MALLOC_FAILED;
    }
    wifi_msg->type = NETCONN_WIFI_MSG_CONNECT;
    wifi_msg->handle = wifi;
    return tal_workq_schedule(WORKQ_SYSTEM, __netconn_wifi_connect_process, wifi_msg);
}

OPERATE_RET __netconn_wifi_disconnect(void)
{
    netmgr_conn_wifi_t *wifi = &s_netmgr_wifi;

    if (NETCONN_WIFI_CONN_STOP == wifi->conn.stat || NETCONN_WIFI_CONN_REDAY == wifi->conn.stat) {
        return OPRT_OK;
    }

    netmgr_wifi_msg_t *wifi_msg = tal_malloc(sizeof(netmgr_wifi_msg_t));
    if (NULL == wifi_msg) {
        return OPRT_MALLOC_FAILED;
    }

    wifi_msg->type = NETCONN_WIFI_MSG_DISCONNECT;
    wifi_msg->handle = wifi;

    return tal_workq_schedule(WORKQ_SYSTEM, __netconn_wifi_connect_process, wifi_msg);
}

static void __netconn_wifi_event(WF_EVENT_E event, void *arg)
{
    netmgr_conn_wifi_t *wifi = &s_netmgr_wifi;

    tal_sw_timer_stop(wifi->conn.timer);
    if (event == WFE_CONNECTED) {
        PR_DEBUG("wifi connected in stat %d", wifi->conn.stat);
        wifi->conn.count = 0;
        wifi->conn.stat = NETCONN_WIFI_CONN_LINKUP;
        wifi->base.status = NETMGR_LINK_UP;
    } else {
        //! faild or disconnect auto connect
        if (NETCONN_WIFI_CONN_CHECK == wifi->conn.stat || NETCONN_WIFI_CONN_WAIT == wifi->conn.stat) {
            __netconn_wifi_backoff_wait(wifi);
        } else if (NETCONN_WIFI_CONN_LINKUP == wifi->conn.stat) {
            wifi->conn.stat = NETCONN_WIFI_CONN_REDAY;
            __netconn_wifi_connect();
        }

        wifi->base.status = NETMGR_LINK_DOWN;
    }

    /* ULP: STA connect attempt resolved (up or down) -> release keep-awake */
    tuya_dev_evt_notify(DEV_EVT_STA_CONNECT, ACTION_AFTER, NULL);

    // notify netmgr the wifi connection status changed
    if (wifi->base.event_cb) {
        wifi->base.event_cb(NETCONN_WIFI, wifi->base.status);
    }

    return;
}

static void __netconn_wifi_conn_timer(TIMER_ID timer_id, void *arg)
{
    netmgr_conn_wifi_t *wifi = (netmgr_conn_wifi_t *)arg;

    PR_DEBUG("auto conn timeout cnt %d, stat %d", wifi->conn.count, wifi->conn.stat);
    if (NETCONN_WIFI_CONN_WAIT == wifi->conn.stat) {
        __netconn_wifi_connect();
        wifi->conn.stat = NETCONN_WIFI_CONN_CHECK;
    } else if (NETCONN_WIFI_CONN_STOP == wifi->conn.stat) {
        wifi->conn.stat = NETCONN_WIFI_CONN_REDAY;
    } else if (NETCONN_WIFI_CONN_CHECK == wifi->conn.stat) {
        // The trace line moved above the arm and reads the interval through the
        // same accessor the arm uses, rather than indexing conn.table directly:
        // the original printed table[count] and then armed with table[count], and
        // for count >= 6 both were 0 while the timer actually waited 20 s. So the
        // log disagreed with the behaviour in exactly the case a reader would be
        // reading it.
        netmgr_retry_table_t table = {
            .entry = wifi->conn.table,
            .count = wifi->conn.table_size,
        };

        PR_DEBUG("wifi connect wait %d-%dms", wifi->conn.count, netmgr_retry_interval_ms(&table, wifi->conn.count));
        __netconn_wifi_backoff_wait(wifi);
    }
}

OPERATE_RET __netconn_wifi_info_set(netconn_wifi_info_t *info)
{
    char netinfo[128];
    snprintf(netinfo, sizeof(netinfo), "{\"s\":\"%s\",\"p\":\"%s\"}", info->ssid, info->pswd);

    return tal_kv_set("netinfo", (const uint8_t *)netinfo, strlen(netinfo));
}

OPERATE_RET __netconn_wifi_info_get(netconn_wifi_info_t *info)
{
    OPERATE_RET rt = 0;
    size_t length = 0;
    uint8_t *netinfo = NULL;

    TUYA_CALL_ERR_RETURN(tal_kv_get("netinfo", &netinfo, &length));

    cJSON *json = cJSON_Parse((const char *)netinfo);
    TUYA_CHECK_NULL_GOTO(json, err_exit);
    // PR_DEBUG("netinfo %s", cJSON_PrintUnformatted(json));

    cJSON *s = cJSON_GetObjectItem(json, "s");
    TUYA_CHECK_NULL_GOTO(s, err_exit);

    cJSON *p = cJSON_GetObjectItem(json, "p");
    TUYA_CHECK_NULL_GOTO(p, err_exit);

    strncpy(info->ssid, s->valuestring, sizeof(info->ssid) - 1);
    info->ssid[sizeof(info->ssid) - 1] = '\0';
    strncpy(info->pswd, p->valuestring, sizeof(info->pswd) - 1);
    info->pswd[sizeof(info->pswd) - 1] = '\0';
    cJSON_Delete(json);
    tal_kv_free(netinfo);
    return OPRT_OK;

err_exit:
    if (netinfo) {
        tal_kv_free(netinfo);
        netinfo = NULL;
    }

    if (json) {
        cJSON_Delete(json);
        json = NULL;
    }
    return OPRT_CJSON_PARSE_ERR;
}

static OPERATE_RET __wifi_link_activete_cb(void *data)
{
    OPERATE_RET rt = OPRT_OK;

    PR_DEBUG("__wifi_link_activete_cb");
    // stop all netcfg
    rt = netcfg_stop(NETCFG_STOP_ALL_CFG_MODULE);

    return rt;
}

OPERATE_RET __netconn_wifi_netcfg_finish(int type, netcfg_info_t *info)
{
    netmgr_conn_wifi_t *netmgr_wifi = &s_netmgr_wifi;

    // save wifi info
    // clear first: this is a persistent global, and a shorter new SSID/pwd must
    // not leave a stale longer value behind (e.g. new "Pico" over old "Pico-Test").
    // memset also guarantees the trailing '\0' since the buffers are LEN + 1.
    memset(netmgr_wifi->conn.wifi_conn_info.ssid, 0, sizeof(netmgr_wifi->conn.wifi_conn_info.ssid));
    memset(netmgr_wifi->conn.wifi_conn_info.pswd, 0, sizeof(netmgr_wifi->conn.wifi_conn_info.pswd));
    memcpy(netmgr_wifi->conn.wifi_conn_info.ssid, info->ssid,
           info->s_len > WIFI_SSID_LEN ? WIFI_SSID_LEN : info->s_len);
    memcpy(netmgr_wifi->conn.wifi_conn_info.pswd, info->passwd,
           info->p_len > WIFI_PASSWD_LEN ? WIFI_PASSWD_LEN : info->p_len);
    __netconn_wifi_info_set(&netmgr_wifi->conn.wifi_conn_info);

    // stop all netcfg
    netcfg_stop(NETCFG_STOP_ALL_CFG_MODULE);

    tuya_binding_info_t binding;
    memset(&binding, 0, sizeof(tuya_binding_info_t));
    memcpy(binding.region, info->token, REGION_LEN);
    memcpy(binding.token, info->token + REGION_LEN, TOKEN_LEN);
    memcpy(binding.regist_key, info->token + REGION_LEN + TOKEN_LEN, REGIST_KEY_LEN);

    tal_event_unsubscribe(EVENT_LINK_ACTIVATE, "wifi", __wifi_link_activete_cb);
    tal_event_publish(EVENT_LINK_ACTIVATE, &binding);
    // notify netcfg finished and save
    __netconn_wifi_connect();

    return OPRT_OK;
}

int __netconn_activate_token_get(tuya_iot_config_t *config)
{
    int rt = OPRT_OK;

    netmgr_conn_wifi_t *netmgr_wifi = &s_netmgr_wifi;
    netmgr_wifi->netcfg.uuid = (char *)config->uuid;
    netmgr_wifi->netcfg.pincode = (char *)config->pincode;

    TUYA_CALL_ERR_LOG(
        tal_event_subscribe(EVENT_LINK_ACTIVATE, "wifi", __wifi_link_activete_cb, SUBSCRIBE_TYPE_ONETIME));

    // init netcfg
    netcfg_init();

    /* Whether this link can raise a SoftAP for provisioning is the driver's own
     * declaration, read back from its descriptor - not something inferred from
     * the data plane.
     *
     * What this replaces:
     *
     *     TAL_NETWORK_CARD_TYPE_E active_type = tal_network_card_get_active_type();
     *     if (netcfg.type & TUYA_NETMGR_NETCFG_AP && active_type != TAL_NET_TYPE_AT_MODEM)
     *
     * That was the control plane asking the data plane a control-plane question,
     * and it was also dead: the active provider is only ever written from
     * conn->card_type, every driver sets that to TAL_NET_PROVIDER_DEFAULT, and
     * that expands to TAL_NET_TYPE_POSIX or TAL_NET_TYPE_PLATFORM - never to
     * TAL_NET_TYPE_AT_MODEM. So "do not open AP provisioning while 4G is the
     * active link" was a tautology that never once fired.
     *
     * The capability bit is behaviour-neutral today, because the wifi row sets
     * NETCONN_CAP_NETCFG_AP. Its value is that a module which cannot host an AP
     * now closes this branch by clearing one bit in its descriptor, which is the
     * first time the original intent is actually executable.
     *
     * A NULL descriptor means this build registered no wifi row at all. Nothing
     * should be able to reach this token port in that case, so both branches
     * below treat it as "no provisioning capability" and skip - never as an
     * implicit yes.
     */
    const netconn_desc_t *desc = netconn_registry_find(NETCONN_WIFI);
    netconn_caps_t        caps = (NULL != desc) ? desc->caps : NETCONN_CAP_NONE;
    if (NULL == desc) {
        PR_WARN("wifi has no registry row, skipping netcfg");
    }

    if ((netmgr_wifi->netcfg.type & TUYA_NETMGR_NETCFG_AP) && (caps & NETCONN_CAP_NETCFG_AP)) {
        ap_netcfg_init(&netmgr_wifi->netcfg);
        netcfg_start(NETCFG_TUYA_WIFI_AP, __netconn_wifi_netcfg_finish, NULL);
    }

#ifdef ENABLE_BLUETOOTH
    /* Same question as the AP branch, same source of truth. The #ifdef stays and
     * the two are ANDed, because they answer different things: ENABLE_BLUETOOTH
     * decides whether ble_netcfg_init() exists to link against at all, while
     * NETCONN_CAP_NETCFG_BLE says whether this link wants to be provisioned that
     * way. Behaviour-neutral - netconn_table.c only sets the bit under
     * ENABLE_BLUETOOTH. */
    if ((netmgr_wifi->netcfg.type & TUYA_NETMGR_NETCFG_BLE) && (caps & NETCONN_CAP_NETCFG_BLE)) {
        ble_netcfg_init(&netmgr_wifi->netcfg);
        netcfg_start(NETCFG_TUYA_BLE, __netconn_wifi_netcfg_finish, NULL);
    }
#endif

    return rt;
}

static OPERATE_RET __wifi_reset_event_cb(void *data)
{
    OPERATE_RET rt = OPRT_OK;

    tuya_iot_client_t *client = (tuya_iot_client_t *)data;
    netmgr_conn_wifi_t *netmgr_wifi = &s_netmgr_wifi;

    PR_DEBUG("__wifi_reset_event_cb");
    tal_sw_timer_stop(netmgr_wifi->conn.timer);
    netmgr_wifi->conn.stat = NETCONN_WIFI_CONN_STOP;
    rt = tal_wifi_station_disconnect();
    if (client->is_activated) {
        tal_kv_del("netinfo");
        memset(&netmgr_wifi->conn.wifi_conn_info, 0, sizeof(netmgr_wifi->conn.wifi_conn_info));
    } else {
        // stop all netcfg
        rt = netcfg_stop(NETCFG_STOP_ALL_CFG_MODULE);
    }

    return rt;
}

/**
 * @brief Opens a WiFi network connection.
 *
 * This function is used to open a WiFi network connection using the provided
 * configuration.
 *
 * @param config A pointer to the configuration data for the WiFi network.
 * @return An `OPERATE_RET` value indicating the success or failure of the
 * operation.
 */
OPERATE_RET netconn_wifi_open(void *config)
{
    netmgr_conn_wifi_t *netmgr_wifi = &s_netmgr_wifi;
    OPERATE_RET rt = OPRT_OK;

    /* Seed the reconnect back-off from the one definition of it, so the driver
     * carries no magic numbers of its own - which is what netmgr_retry.h declares
     * netmgr_retry_table_assoc for.
     *
     * Guarded on table_size, so a product-supplied NETCONN_CMD_RECONN_TABLE is
     * never overwritten. The ordering makes that unreachable today - the command
     * arrives through netmgr_conn_set(), which refuses until netmgr_init() has
     * finished, i.e. strictly after this - but a close()/open() cycle must not
     * silently revert a ULP board to the short table either, and this is what
     * stops it. */
    if (0 == netmgr_wifi->conn.table_size && NULL != netmgr_retry_table_assoc.entry) {
        uint32_t n = netmgr_retry_table_assoc.count;

        if (n > NETCONN_WIFI_CONN_TABLE) {
            n = NETCONN_WIFI_CONN_TABLE;
        }
        memcpy(netmgr_wifi->conn.table, netmgr_retry_table_assoc.entry, n * sizeof(uint32_t));
        netmgr_wifi->conn.table_size = n;
        netmgr_wifi->conn.count      = 0;
    }

    // init
    TUYA_CALL_ERR_RETURN(tal_wifi_init(__netconn_wifi_event));

    /* Marked here, not at the end: from this point on __netconn_wifi_event() can
     * fire from a vendor task, and every failure below leaves something for
     * close() to undo. close() guards each individual step, so it is safe to be
     * told about a half-built link. */
    s_wifi_opened = TRUE;

    TUYA_CALL_ERR_RETURN(tal_wifi_set_country_code(netmgr_wifi->ccode));

    // create a connect timer --- it will triggered by netcfg or set ssid&pswd
    TUYA_CALL_ERR_RETURN(tal_sw_timer_create(__netconn_wifi_conn_timer, netmgr_wifi, &netmgr_wifi->conn.timer));
    TUYA_CALL_ERR_RETURN(tal_event_subscribe(EVENT_RESET, "wifi", __wifi_reset_event_cb, SUBSCRIBE_TYPE_NORMAL));

    tuya_iot_client_t *client = tuya_iot_client_get();
    if (client->is_activated) {
        tal_wifi_lp_disable();
        __netconn_wifi_info_get(&netmgr_wifi->conn.wifi_conn_info);
        __netconn_wifi_connect();
    }

    return rt;
}

/**
 * @brief Closes the WiFi network connection.
 *
 * Releases everything netconn_wifi_open() and __netconn_activate_token_get()
 * created. Until M2 this returned OPRT_OK without doing anything, so an
 * init/deinit cycle leaked two event subscriptions and a software timer, and
 * left provisioning running.
 *
 * The order is chosen so that nothing can rearm a resource that has already been
 * released:
 *
 *   1. park the reconnect state machine (conn.stat = STOP). Both the timer
 *      callback and the WiFi event callback branch on this field, and STOP is the
 *      one state in which neither restarts the timer nor calls
 *      __netconn_wifi_connect();
 *   2. unsubscribe the two events. __wifi_reset_event_cb() touches conn.timer
 *      and __wifi_link_activete_cb() calls netcfg_stop(); if either fired during
 *      or after the steps below it would work on state we are dismantling.
 *      EVENT_RESET is SUBSCRIBE_TYPE_NORMAL and never self-removes;
 *      EVENT_LINK_ACTIVATE is ONETIME, so it may already be gone - unsubscribing
 *      an absent entry is harmless;
 *   3. stop provisioning. A netcfg backend that completes calls
 *      __netconn_wifi_netcfg_finish(), which calls __netconn_wifi_connect() and
 *      would put the link straight back up;
 *   4. drop the association. This must come after 1 and 3, or the reconnect path
 *      simply dials again;
 *   5. delete the timer last, because step 4 can raise WFE_DISCONNECTED on a
 *      vendor task and that handler dereferences conn.timer. Stop before delete
 *      so the timer thread cannot be holding it, then NULL the handle - which is
 *      both the idempotency marker for this resource and what keeps a late
 *      tal_sw_timer_stop() from touching freed memory (tal_sw_timer_{stop,start,
 *      delete}() all reject NULL rather than dereferencing it).
 *
 * Two teardowns that are NOT possible here, deliberately:
 *   - tal_wifi.h has no uninit/deinit and no way to unregister the
 *     WIFI_EVENT_CB installed by tal_wifi_init(), so the vendor stack stays up
 *     and a late event can still reach __netconn_wifi_event(). That is why step
 *     1 exists and why every field it touches must survive close();
 *   - netcfg.h has netcfg_init() but no netcfg_deinit(), so the netcfg session
 *     itself is stopped, not freed.
 *
 * Idempotent: a second call, or a call after netconn_wifi_open() failed part way
 * through, returns OPRT_OK having touched nothing.
 *
 * @return OPRT_OK. Individual steps log and continue rather than aborting the
 *         teardown - a resource that cannot be released must not prevent the
 *         next one from being released.
 */
OPERATE_RET netconn_wifi_close(void)
{
    netmgr_conn_wifi_t *netmgr_wifi = &s_netmgr_wifi;

    if (!s_wifi_opened) {
        return OPRT_OK;
    }
    s_wifi_opened = FALSE;

    // 1. park the reconnect state machine
    netmgr_wifi->conn.stat  = NETCONN_WIFI_CONN_STOP;
    netmgr_wifi->conn.count = 0;

    // 2. unsubscribe before releasing anything the callbacks reach
    tal_event_unsubscribe(EVENT_RESET, "wifi", __wifi_reset_event_cb);
    tal_event_unsubscribe(EVENT_LINK_ACTIVATE, "wifi", __wifi_link_activete_cb);

    // 3. stop provisioning; fails harmlessly when netcfg_init() never ran
    netcfg_stop(NETCFG_STOP_ALL_CFG_MODULE);

    // 4. drop the association
    tal_wifi_station_disconnect();

    // 5. timer last - see the note above
    if (NULL != netmgr_wifi->conn.timer) {
        tal_sw_timer_stop(netmgr_wifi->conn.timer);
        tal_sw_timer_delete(netmgr_wifi->conn.timer);
        netmgr_wifi->conn.timer = NULL;
    }

    netmgr_wifi->base.status = NETMGR_LINK_DOWN;

    return OPRT_OK;
}

/**
 * @brief Sets the configuration for the Wi-Fi network connection.
 *
 * This function is used to set the configuration for the Wi-Fi network
 * connection.
 *
 * @param cmd The type of configuration to set. This can be one of the values
 * from the `netmgr_conn_config_type_e` enumeration.
 * @param param A pointer to the configuration parameter. The type of the
 * parameter depends on the value of `cmd`.
 *
 * @return The result of the operation. This can be one of the values from the
 * `OPERATE_RET` enumeration.
 */
OPERATE_RET netconn_wifi_set(netmgr_conn_config_type_e cmd, void *param)
{
    netmgr_conn_wifi_t *netmgr_wifi = &s_netmgr_wifi;
    tuya_iot_client_t *client = tuya_iot_client_get();
    OPERATE_RET rt = OPRT_OK;

    switch (cmd) {
    case NETCONN_CMD_PRI: // set pri will cause status change to reneg the
                          // active connection
        netmgr_wifi->base.pri = *(int *)param;
        netmgr_wifi->base.event_cb(NETCONN_WIFI, netmgr_wifi->base.status);
        break;
    case NETCONN_CMD_IP:
        TUYA_CALL_ERR_RETURN(tal_wifi_set_ip(WF_STATION, (NW_IP_S *)param));
        break;
    case NETCONN_CMD_MAC: // set mac to the station
        TUYA_CALL_ERR_RETURN(tal_wifi_set_mac(WF_STATION, (NW_MAC_S *)param));
        break;
    case NETCONN_CMD_SSID_PSWD: // set ssid&paswd will cause wifi
                                // disconnect&connect
        memcpy(&netmgr_wifi->conn.wifi_conn_info, (netconn_wifi_info_t *)param, sizeof(netconn_wifi_info_t));
        __netconn_wifi_connect();
        break;
    case NETCONN_CMD_COUNTRYCODE:
        memcpy(netmgr_wifi->ccode, (char *)param, strlen((char *)param));
        TUYA_CALL_ERR_RETURN(tal_wifi_set_country_code(netmgr_wifi->ccode));
        break;
    case NETCONN_CMD_NETCFG: {
        netcfg_args_t *netcfg = (netcfg_args_t *)param;
        netmgr_wifi->netcfg.type = netcfg->type;
        if (netmgr_wifi->netcfg.type & NETCFG_TUYA_BLE || netmgr_wifi->netcfg.type & NETCFG_TUYA_WIFI_AP) {
            tuya_iot_token_get_port_register(client, __netconn_activate_token_get);
        }
    } break;
    case NETCONN_CMD_CLOSE:
        __netconn_wifi_disconnect();
        break;

    case NETCONN_CMD_RESET:
        tal_kv_del("netinfo");
        netmgr_wifi->conn.stat = NETCONN_WIFI_CONN_STOP;
        memset(&netmgr_wifi->conn.wifi_conn_info, 0, sizeof(netmgr_wifi->conn.wifi_conn_info));
        tal_wifi_station_disconnect();
        break;

    case NETCONN_CMD_RECONN_TABLE: {
        /* Override the reconnect back-off table (seconds). A longer table keeps
         * the radio off for longer while the network is down - used by ULP. */
        netmgr_reconn_table_t *rc = (netmgr_reconn_table_t *)param;
        if (NULL == rc || NULL == rc->table || 0 == rc->size) {
            return OPRT_INVALID_PARM;
        }
        uint32_t n = (rc->size > NETCONN_WIFI_CONN_TABLE) ? NETCONN_WIFI_CONN_TABLE : rc->size;
        memcpy(netmgr_wifi->conn.table, rc->table, n * sizeof(uint32_t));
        netmgr_wifi->conn.table_size = n;
        netmgr_wifi->conn.count = 0;
        PR_NOTICE("reconn table set, %d entries (last %ds)", n, netmgr_wifi->conn.table[n - 1]);
    } break;

    default:
        return OPRT_NOT_SUPPORTED;
    }

    return OPRT_OK;
}

/**
 * @brief Retrieves the network connection configuration for WiFi.
 *
 * This function is used to get the network connection configuration for WiFi.
 *
 * @param cmd The type of network connection configuration to retrieve.
 * @param param A pointer to the parameter structure for the network connection
 * configuration.
 *
 * @return The result of the operation.
 */
OPERATE_RET netconn_wifi_get(netmgr_conn_config_type_e cmd, void *param)
{
    netmgr_conn_wifi_t *netmgr_wifi = &s_netmgr_wifi;
    OPERATE_RET rt = OPRT_OK;

    switch (cmd) {
    case NETCONN_CMD_PRI:
        *(int *)param = netmgr_wifi->base.pri;
        break;
    case NETCONN_CMD_MAC:
        TUYA_CALL_ERR_RETURN(tal_wifi_get_mac(WF_STATION, (NW_MAC_S *)param));
        break;
    case NETCONN_CMD_SSID_PSWD:
        memcpy((netconn_wifi_info_t *)param, &netmgr_wifi->conn.wifi_conn_info, sizeof(netconn_wifi_info_t));
        break;
    case NETCONN_CMD_COUNTRYCODE:
        memcpy((char *)param, netmgr_wifi->ccode, strlen(netmgr_wifi->ccode));
        break;
    case NETCONN_CMD_IP:
        TUYA_CALL_ERR_RETURN(tal_wifi_get_ip(WF_STATION, (NW_IP_S *)param));
        break;
    case NETCONN_CMD_NETCFG: {
        netcfg_args_t *netcfg = (netcfg_args_t *)param;
        memcpy(netcfg, &netmgr_wifi->netcfg, sizeof(netcfg_args_t));
    } break;
    case NETCONN_CMD_STATUS:
        *(netmgr_status_e *)param = netmgr_wifi->base.status;
        break;
    default:
        return OPRT_NOT_SUPPORTED;
    }

    return OPRT_OK;
}
