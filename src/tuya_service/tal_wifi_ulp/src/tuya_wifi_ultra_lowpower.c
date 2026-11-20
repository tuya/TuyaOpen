/**
 * @file tuya_wifi_ultra_lowpower.c
 * @brief Ultra Low Power WiFi initialization and event integration for TuyaOpen.
 *
 * Ported from the tuyaos-ai ULP integration. Faithful to the original design:
 *  - Per-network-operation wakelocks via the dev_evt mechanism (__dev_evt_cb),
 *    so each DNS/TCP/MQTT-connect/publish/DP/OTA/reset briefly keeps the device
 *    awake, then drops back to the DTIM baseline.
 *  - Lifecycle locks: DISABLE as a short boot guard; NETCFG (full awake) +
 *    LONG_NETCFG held only while the device is unprovisioned (netcfg), released
 *    on activation / mqtt-connected respectively.
 *
 * TuyaOpen-specific adaptations (documented at each site):
 *  - No fine-grained hooks existed; they were added at tal_network (DNS/TCP),
 *    mqtt_service (connect/publish) and tuya_iot (DP report). TLS/HTTP need no
 *    hook (covered by the DNS/TCP notify below them), matching the original.
 *  - MQTT ping is owned by the vendored coreMQTT library and is not hooked.
 *  - BLE is torn down after provisioning (see the app) rather than kept under a
 *    lock, because TuyaOpen's nimble stack does not participate in the SoC pm
 *    vote - this mirrors the original which also destroys BLE after netcfg.
 *  - A counter-balance tal_wifi_lp_enable() offsets the unpaired
 *    tal_wifi_lp_disable() that netconn_wifi_open() issues for activated devices.
 *
 * @copyright Copyright (c) 2023 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "tal_log.h"
#include "tal_sw_timer.h"
#include "tal_event.h"
#include "tal_event_info.h"
#include "tal_wifi.h"
#include "tuya_iot.h"
#include "dev_evt.h"
#include "lpmgr.h"

/***********************************************************
*************************micro define***********************
***********************************************************/
#define ULP_INIT_DELAY_MS 10000 /* boot guard: keep awake for the first 10s */

/***********************************************************
*************************variable define********************
***********************************************************/
static TIMER_ID lp_enable_timer = NULL;

/***********************************************************
*************************function define********************
***********************************************************/

/**
 * @brief Device network-operation callback: hold a wakelock for the duration of
 *        each operation. Mapping is identical to the tuyaos-ai __dev_evt_cb.
 */
static void __dev_evt_cb(DEV_EVT_E evt, DEV_ACTION_E action, void *ctx)
{
    TY_LP_TYPE type = TY_LP_MQTT_CONN;

    switch (evt) {
    case DEV_EVT_STA_CONNECT:
        type = TY_LP_NETCONN;
        break;
    case DEV_EVT_TCP_CONNECT:
        type = TY_LP_TCP_CONN;
        break;
    case DEV_EVT_TLS_CONNECT:
        type = TY_LP_TLSCONN;
        break;
    case DEV_EVT_HTTP_CONNECT:
        type = TY_LP_HTTP_CONN;
        break;
    case DEV_EVT_MQTT_CONNECT:
        type = TY_LP_MQTT_CONN;
        break;
    case DEV_EVT_MQTT_PUBLISH:
        type = TY_LP_MQTT_PUBLISH;
        break;
    case DEV_EVT_MQTT_PING:
        type = TY_LP_MQTT_PING;
        break;
    case DEV_EVT_DNS_LOOKUP:
        type = TY_LP_DNS;
        break;
    case DEV_EVT_BLE_STACK:
        type = TY_LP_BLE;
        break;
    case DEV_EVT_DP_PROCESS:
        type = TY_LP_DP;
        break;
    case DEV_EVT_OTA:
        type = TY_LP_OTA;
        break;
    case DEV_EVT_RESET:
        type = TY_LP_DISABLE;
        break;
    default:
        return;
    }

    if (ACTION_BEFORE == action) {
        lpmgr_register(type);
    } else if (ACTION_AFTER == action) {
        lpmgr_unregister(type);
    }
}

/**
 * @brief Boot guard timer: release the DISABLE lock after the init delay.
 */
static void lp_enable_time_cb(TIMER_ID timer_id, void *timer_arg)
{
    lpmgr_unregister(TY_LP_DISABLE);
}

/**
 * @brief Activation success (got token). Mirror of the original devos ACTIVATED
 *        transition: release the full-awake netcfg lock. LONG_NETCFG stays until
 *        mqtt-connected.
 */
static OPERATE_RET wifi_ulp_activate_cb(void *data)
{
    PR_DEBUG("wifi ulp activated, release NETCFG");
    lpmgr_unregister(TY_LP_NETCFG);
    return OPRT_OK;
}

/**
 * @brief MQTT connected: fully online -> release the remaining netcfg/boot
 *        locks so the device settles to the DTIM baseline.
 */
static OPERATE_RET wifi_ulp_mqtt_connected_cb(void *data)
{
    PR_NOTICE("wifi ulp mqtt connected, release lifecycle locks -> low power");
    lpmgr_unregister(TY_LP_LONG_NETCFG);
    lpmgr_unregister(TY_LP_NETCFG);
    lpmgr_unregister(TY_LP_DISABLE);

    /* netconn_wifi_open() issues one unpaired tal_wifi_lp_disable() for
     * already-activated devices to keep WiFi awake during (re)connect, leaving
     * s_tal_wifi.lp_disable_cnt permanently offset by 1. Balance it here so the
     * power manager's enable actually reaches the driver (tkl_wifi_set_lp_mode). */
    tal_wifi_lp_enable();
    return OPRT_OK;
}

/**
 * @brief MQTT disconnected: re-arm a keep-awake lock so the reconnect sequence
 *        is not starved by aggressive power save; released again on reconnect.
 */
static OPERATE_RET wifi_ulp_mqtt_disconnected_cb(void *data)
{
    PR_NOTICE("wifi ulp mqtt disconnected, re-arm LONG_NETCFG for reconnect");
    lpmgr_register(TY_LP_LONG_NETCFG);
    return OPRT_OK;
}

/**
 * @brief Initialize Ultra Low Power WiFi mode.
 */
OPERATE_RET tuya_wifi_ulp_init(void)
{
    lpmgr_init();

    /* Route every instrumented network operation into the power manager. */
    tuya_dev_evt_set_cb(__dev_evt_cb);

    /* Boot guard: keep awake for the first 10s (matches the original). */
    lpmgr_register(TY_LP_DISABLE);
    if (NULL == lp_enable_timer) {
        tal_sw_timer_create(lp_enable_time_cb, NULL, &lp_enable_timer);
    }
    tal_sw_timer_start(lp_enable_timer, ULP_INIT_DELAY_MS, TAL_TIMER_ONCE);

    /* Unprovisioned device: keep awake through the whole netcfg. Mirrors the
     * original devos-state UNREGISTERED handling (NETCFG full awake until
     * activation, LONG_NETCFG until mqtt). Already-activated devices skip this
     * and rely on the per-operation locks during reconnect. */
    tuya_iot_client_t *client = tuya_iot_client_get();
    if (NULL != client && !tuya_iot_activated(client)) {
        lpmgr_register(TY_LP_NETCFG);
        lpmgr_register(TY_LP_LONG_NETCFG);
    }

    tal_event_subscribe(EVENT_LINK_ACTIVATE, "wifi_ulp", wifi_ulp_activate_cb, SUBSCRIBE_TYPE_NORMAL);
    tal_event_subscribe(EVENT_MQTT_CONNECTED, "wifi_ulp", wifi_ulp_mqtt_connected_cb, SUBSCRIBE_TYPE_NORMAL);
    tal_event_subscribe(EVENT_MQTT_DISCONNECTED, "wifi_ulp", wifi_ulp_mqtt_disconnected_cb, SUBSCRIBE_TYPE_NORMAL);

    PR_NOTICE("ULP: tuya_wifi_ulp_init done (dev_evt hooks active)");
    return OPRT_OK;
}
