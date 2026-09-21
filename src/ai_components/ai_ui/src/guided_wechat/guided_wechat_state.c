/**
 * @file guided_wechat_state.c
 * @brief Device-state adapter used by the guided WeChat UI.
 */

#include <string.h>
#include <stdio.h>

#include "tal_api.h"
#include "tal_kv.h"
#include "tuya_authorize.h"
#include "tuya_iot.h"
#include "netmgr.h"
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#include "ap_netcfg.h"
#endif

#include "guided_wechat_state.h"

static volatile guided_wechat_pairing_state_t sg_pairing_state = GUIDED_WECHAT_PAIRING_IDLE;

#define GUIDED_WECHAT_REPROVISION_KEY "guided_wechat_reprovision"

static bool __contains_placeholder(const char *value)
{
    return value == NULL || value[0] == '\0' || strstr(value, "xxx") != NULL ||
           strstr(value, "your_") != NULL || strstr(value, "here") != NULL;
}

static bool __license_valid(const char *uuid, const char *authkey)
{
    size_t uuid_len;

    if (__contains_placeholder(uuid) || __contains_placeholder(authkey)) {
        return false;
    }

    uuid_len = strlen(uuid);
    return (uuid_len == 16 || uuid_len == 20) && strlen(authkey) == 32;
}

static void __copy_value(char *out, size_t out_len, const char *value)
{
    if (out == NULL || out_len == 0) {
        return;
    }
    snprintf(out, out_len, "%s", value != NULL && value[0] != '\0' ? value : "-");
}

static void __mask_value(char *out, size_t out_len, const char *value)
{
    size_t len;

    if (out == NULL || out_len == 0) {
        return;
    }
    if (value == NULL || value[0] == '\0') {
        __copy_value(out, out_len, "-");
        return;
    }

    len = strlen(value);
    if (len <= 6) {
        __copy_value(out, out_len, "***");
        return;
    }
    snprintf(out, out_len, "%.3s***%s", value, value + len - 3);
}

/* The storage license (uuid/authkey in KV) never changes at runtime: no code
 * path writes it after boot and the UI offers no authorization reset. Read it
 * once and serve the cached pointers - state_read() runs from the 1 Hz UI
 * refresh timer, so a per-call flash read would be wasted IO. */
static struct {
    bool loaded;
    bool valid;
    const char *uuid;
    const char *authkey;
} sg_storage_license;

static void __storage_license_load(void)
{
    tuya_iot_license_t license = {0};

    if (sg_storage_license.loaded) {
        return;
    }

    sg_storage_license.valid =
        (OPRT_OK == tuya_authorize_read(&license)) && __license_valid(license.uuid, license.authkey);
    /* tuya_authorize_read() hands out pointers to its own persistent buffers,
     * so the cached pointers stay valid for the lifetime of the firmware. */
    sg_storage_license.uuid = license.uuid;
    sg_storage_license.authkey = license.authkey;
    sg_storage_license.loaded = true;
}

void guided_wechat_pairing_request(void)
{
    if (sg_pairing_state == GUIDED_WECHAT_PAIRING_IDLE) {
        sg_pairing_state = GUIDED_WECHAT_PAIRING_STARTING;
    }
}

void guided_wechat_pairing_begin(void)
{
    sg_pairing_state = GUIDED_WECHAT_PAIRING_WAITING_PHONE;
}

void guided_wechat_pairing_data_received(void)
{
    sg_pairing_state = GUIDED_WECHAT_PAIRING_CONNECTING;
}

void guided_wechat_pairing_complete(void)
{
    sg_pairing_state = GUIDED_WECHAT_PAIRING_IDLE;
}

void guided_wechat_pairing_reset(void)
{
    sg_pairing_state = GUIDED_WECHAT_PAIRING_IDLE;
}

void guided_wechat_reprovision_mark(void)
{
    const uint8_t pending = 1;

    if (OPRT_OK != tal_kv_set(GUIDED_WECHAT_REPROVISION_KEY, &pending, sizeof(pending))) {
        PR_WARN("guided re-provision intent save failed");
    }
}

bool guided_wechat_reprovision_pending(void)
{
    uint8_t *value = NULL;
    size_t length = 0;
    bool pending = false;

    if (OPRT_OK == tal_kv_get(GUIDED_WECHAT_REPROVISION_KEY, &value, &length) &&
        value != NULL && length > 0 && value[0] == 1) {
        pending = true;
    }

    if (value != NULL) {
        tal_kv_free(value);
    }
    return pending;
}

void guided_wechat_reprovision_clear(void)
{
    if (OPRT_OK != tal_kv_del(GUIDED_WECHAT_REPROVISION_KEY)) {
        PR_WARN("guided re-provision intent clear failed");
    }
}

void guided_wechat_state_read(guided_wechat_device_state_t *state)
{
    tuya_iot_client_t *client;
    netmgr_status_e link_status = NETMGR_LINK_DOWN;
    const char *uuid = NULL;
    const char *authkey = NULL;

    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->pairing_state = sg_pairing_state;
    __copy_value(state->version, sizeof(state->version), PROJECT_VERSION);
    __copy_value(state->sdk_version, sizeof(state->sdk_version), OPEN_VERSION);

    __storage_license_load();
    if (sg_storage_license.valid) {
        state->auth_source = GUIDED_WECHAT_AUTH_STORAGE;
        uuid = sg_storage_license.uuid;
        authkey = sg_storage_license.authkey;
    }

    client = tuya_iot_client_get();
    if (client != NULL) {
        state->activated = tuya_iot_activated(client);
        state->cloud_online = tuya_iot_is_connected();
        __copy_value(state->version, sizeof(state->version), client->config.software_ver);
        __copy_value(state->product_id, sizeof(state->product_id), client->config.productkey);
        __copy_value(state->device_id, sizeof(state->device_id), tuya_iot_devid_get(client));

        if (state->auth_source == GUIDED_WECHAT_AUTH_NONE &&
            __license_valid(client->config.uuid, client->config.authkey)) {
            state->auth_source = GUIDED_WECHAT_AUTH_COMPILED;
            uuid = client->config.uuid;
            authkey = client->config.authkey;
        }
    }

    __mask_value(state->uuid_masked, sizeof(state->uuid_masked), uuid);
    __mask_value(state->authkey_masked, sizeof(state->authkey_masked), authkey);

    if (OPRT_OK == netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &link_status)) {
        state->link_up = link_status == NETMGR_LINK_UP || link_status == NETMGR_LINK_UP_SWITH;
    }

#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    netconn_wifi_info_t wifi_info = {0};
    /* NETCONN_CMD_SSID_PSWD is the configured/last requested network. Only
     * expose it as the current network while the link is actually up; when
     * the station is down this value would otherwise make the UI report a
     * stale SSID as if the device were still connecting to it. */
    if (state->link_up &&
        OPRT_OK == netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &wifi_info)) {
        snprintf(state->ssid, sizeof(state->ssid), "%s", wifi_info.ssid);
    }

    /* SoftAP provisioning info: only exists while AP netcfg is running. The
     * getter copies from RAM, so the 1 Hz refresh stays cheap; failure just
     * leaves the "-" placeholder until the hotspot comes up. */
    if (OPRT_OK != ap_netcfg_get_hotspot_info(state->ap_ssid, sizeof(state->ap_ssid),
                                              state->ap_ip, sizeof(state->ap_ip))) {
        __copy_value(state->ap_ssid, sizeof(state->ap_ssid), NULL);
        __copy_value(state->ap_ip, sizeof(state->ap_ip), NULL);
    }
#endif
}

bool guided_wechat_state_equal(const guided_wechat_device_state_t *left,
                               const guided_wechat_device_state_t *right)
{
    return left != NULL && right != NULL && memcmp(left, right, sizeof(*left)) == 0;
}
