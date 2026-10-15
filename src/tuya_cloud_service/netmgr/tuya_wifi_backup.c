/**
 * @file tuya_wifi_backup.c
 * @brief Backup network (backup AP) service for the WiFi connection.
 *
 * Keeps a short list of known good APs, switches to them when the current one
 * is lost, and serves the APP management protocol ("wifiInfo", "wifiList",
 * "wifiSecure", "wifiSettingBackup", "wifiSettingNew", "wifiSwitchBackup",
 * "wifiSwitchNew") over both the cloud and the BLE transparent channel.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_wifi_backup.h"

#include "tal_api.h"
#include "tal_event_info.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tal_sw_timer.h"
#include "tal_workq_service.h"
#include "cJSON.h"
#include "mix_method.h"
#include "mbedtls/ecdh.h"

#include "netmgr.h"
#include "netconn_wifi.h"
#include "tuya_iot.h"
#include "mqtt_service.h"
#include "tuya_mqtt_dispatch.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define WIFI_BACKUP_KV_KEY    "wifi_bak"
#define WIFI_BACKUP_TID_LEN   32
#define WIFI_BACKUP_DEVID_LEN 32

#define WIFI_BACKUP_STATUS_IDLE    0
#define WIFI_BACKUP_STATUS_SWITCH  1
#define WIFI_BACKUP_STATUS_CONNECT 2

#define WIFI_BACKUP_AP_EXIST_FLAG 1
#define WIFI_BACKUP_AP_PRIOR_FLAG 2

#define WIFI_BACKUP_SWITCH_INTERVAL_MS  1000
#define WIFI_BACKUP_CONNECT_INTERVAL_MS 2000
#define WIFI_BACKUP_SWITCH_TIMEOUT_MS   (45 * 1000)
#define WIFI_BACKUP_CONNECT_TIMEOUT_MS  (35 * 1000)

#define WIFI_BACKUP_LOCAL_RECONNECT_TIME 60
#define WIFI_BACKUP_CLOUD_RECONNECT_TIME 300

#define WIFI_BACKUP_OFFLINE_TICK_MS 1000

#define WIFI_BACKUP_B64_LEN(n) ((((n) / 3 + ((n) % 3 != 0)) * 4) + 1)
#define WIFI_BACKUP_HASH_LEN   32
#define WIFI_BACKUP_PUBKEY_LEN 65

#define WIFI_BACKUP_FSM_STATE_INIT  0
#define WIFI_BACKUP_FSM_STATE_CHECK 1
#define WIFI_BACKUP_FSM_STATE_STOP  2
#define WIFI_BACKUP_FSM_STATE_DONE  3

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char    ssid[WIFI_SSID_LEN + 1];
    char    passwd[WIFI_PASSWD_LEN + 1];
    uint8_t hash[WIFI_BACKUP_HASH_LEN];
    int8_t  rssi;
    uint8_t flag;
} wifi_backup_ap_t;

typedef struct {
    uint8_t                  state;
    uint32_t                 timeout;
    uint8_t                  recover;
    tuya_wifi_backup_trans_t trans;
    wifi_backup_ap_t         ap;
    char                     tid[WIFI_BACKUP_TID_LEN + 1];
    char                     devid[WIFI_BACKUP_DEVID_LEN + 1];
} wifi_backup_switch_fsm_t;

typedef struct {
    uint8_t           state;
    uint32_t          timeout;
    uint8_t           index;
    uint8_t           count;
    wifi_backup_ap_t *ap[TUYA_WIFI_BACKUP_AP_NUM];
} wifi_backup_connect_fsm_t;

typedef struct {
    bool                      inited;
    uint8_t                   status;
    uint8_t                   count;
    wifi_backup_ap_t          list[TUYA_WIFI_BACKUP_AP_NUM];
    uint8_t                   ecdh_secret[WIFI_BACKUP_HASH_LEN];
    char                      orig_ssid[WIFI_SSID_LEN + 1];
    char                      orig_passwd[WIFI_PASSWD_LEN + 1];
    wifi_backup_switch_fsm_t  switch_fsm;
    wifi_backup_connect_fsm_t connect_fsm;
    tuya_wfpt_state_cb_t     *state_cb;
    tuya_wifi_backup_trans_t  trans;
    DELAYED_WORK_HANDLE       fsm_work;
    TIMER_ID                  offline_timer;
    uint32_t                  local_num;
    uint32_t                  cloud_num;
} wifi_backup_mgr_t;

/***********************************************************
***********************variable define**********************
***********************************************************/
static wifi_backup_mgr_t         s_mgr;
static tuya_wifi_backup_sender_t s_sender[2]      = {NULL, NULL};
static bool                      s_enable         = true;
static int (*s_conf_attr)(tuya_wfpt_attr_t *attr) = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/
static void        __fsm_task(void *data);
static void        __offline_tick(TIMER_ID timer_id, void *arg);
static void        __offline_scan_task(void *data);
static void        __switch_fsm_run(wifi_backup_switch_fsm_t *fsm);
static void        __connect_fsm_run(wifi_backup_connect_fsm_t *fsm);
static void        __state_notify(tuya_wfpt_state_t state);
static OPERATE_RET __proto_process(cJSON *root, tuya_wifi_backup_trans_t trans);

static void __str_copy(char *dst, uint32_t dst_size, const char *src)
{
    if (NULL == src) {
        src = "";
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

/**
 * @brief Read the AP the device is currently configured with.
 */
static bool __cur_ap_get(char *ssid, uint32_t ssid_size, char *passwd, uint32_t passwd_size)
{
    netconn_wifi_info_t info;

    memset(&info, 0, sizeof(info));
    if (OPRT_OK != netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &info)) {
        return false;
    }
    if ('\0' == info.ssid[0]) {
        return false;
    }

    __str_copy(ssid, ssid_size, info.ssid);
    __str_copy(passwd, passwd_size, info.pswd);

    return true;
}

/**
 * @brief Remember the AP to roll back to, the target is only persisted on success.
 */
static void __orig_capture(void)
{
    if (false == __cur_ap_get(s_mgr.orig_ssid, sizeof(s_mgr.orig_ssid), s_mgr.orig_passwd, sizeof(s_mgr.orig_passwd))) {
        s_mgr.orig_ssid[0]   = '\0';
        s_mgr.orig_passwd[0] = '\0';
    }
}

static bool __is_same_ap_as_current(const char *ssid, const char *passwd)
{
    char cur_ssid[WIFI_SSID_LEN + 1];
    char cur_passwd[WIFI_PASSWD_LEN + 1];

    if (false == __cur_ap_get(cur_ssid, sizeof(cur_ssid), cur_passwd, sizeof(cur_passwd))) {
        return false;
    }

    return (0 == strcmp(ssid, cur_ssid)) && (0 == strcmp(passwd, cur_passwd));
}

static bool __link_up(void)
{
    netmgr_status_e status = NETMGR_LINK_DOWN;

    netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_STATUS, &status);

    return (NETMGR_LINK_UP == status) || (NETMGR_LINK_UP_SWITH == status);
}

static bool __cloud_connected(void)
{
    tuya_iot_client_t *client = tuya_iot_client_get();

    if (NULL == client) {
        return false;
    }

    return tuya_mqtt_connected(&client->mqctx);
}

/**
 * @brief Compute the identity hash of an AP: SHA256(local_key || ssid || passwd).
 */
static void __ap_hash(wifi_backup_ap_t *ap)
{
    TKL_HASH_HANDLE    ctx;
    tuya_iot_client_t *client    = tuya_iot_client_get();
    const char        *local_key = (NULL == client) ? "" : tuya_iot_localkey_get(client);

    tal_sha256_create_init(&ctx);
    tal_sha256_starts_ret(ctx, 0);
    tal_sha256_update_ret(ctx, (const uint8_t *)local_key, strlen(local_key));
    tal_sha256_update_ret(ctx, (const uint8_t *)ap->ssid, strlen(ap->ssid));
    tal_sha256_update_ret(ctx, (const uint8_t *)ap->passwd, strlen(ap->passwd));
    tal_sha256_finish_ret(ctx, ap->hash);
    tal_sha256_free(ctx);
}

/**
 * @brief base64 decode into a fixed size buffer.
 *
 * @return the decoded length, or a negative value when it does not fit
 */
static int __b64_decode(const char *b64, uint8_t *out, uint32_t out_size)
{
    uint8_t tmp[128];
    int     len;

    if (NULL == b64) {
        return OPRT_INVALID_PARM;
    }
    /* decoded data never exceeds the encoded length, so this also bounds tmp */
    if (strlen(b64) >= (sizeof(tmp) / 4) * 3) {
        return OPRT_INVALID_PARM;
    }

    len = tuya_base64_decode(b64, tmp);
    if (len < 0 || (uint32_t)len > out_size) {
        return OPRT_INVALID_PARM;
    }

    memcpy(out, tmp, len);

    return len;
}

/***********************************************************
*********************backup list ops************************
***********************************************************/
static wifi_backup_ap_t *__list_find(const uint8_t *hash)
{
    uint8_t i;

    for (i = 0; i < s_mgr.count; i++) {
        if (0 == memcmp(s_mgr.list[i].hash, hash, WIFI_BACKUP_HASH_LEN)) {
            return &s_mgr.list[i];
        }
    }

    return NULL;
}

static void __list_remove_at(uint8_t index)
{
    if (index >= s_mgr.count) {
        return;
    }

    if (index + 1 < s_mgr.count) {
        memmove(&s_mgr.list[index], &s_mgr.list[index + 1], (s_mgr.count - index - 1) * sizeof(wifi_backup_ap_t));
    }
    s_mgr.count--;
    memset(&s_mgr.list[s_mgr.count], 0, sizeof(wifi_backup_ap_t));
}

/**
 * @brief Add an AP, @p front places it at the head so it is the last to be evicted.
 */
static void __list_push(const char *ssid, const char *passwd, bool front)
{
    wifi_backup_ap_t ap;

    if (NULL == ssid || '\0' == ssid[0]) {
        return;
    }

    memset(&ap, 0, sizeof(ap));
    __str_copy(ap.ssid, sizeof(ap.ssid), ssid);
    __str_copy(ap.passwd, sizeof(ap.passwd), passwd);
    __ap_hash(&ap);

    if (NULL != __list_find(ap.hash)) {
        PR_DEBUG("ap %s is already in the backup list", ap.ssid);
        return;
    }

    if (s_mgr.count >= TUYA_WIFI_BACKUP_AP_NUM) {
        __list_remove_at(TUYA_WIFI_BACKUP_AP_NUM - 1); //!< drop the oldest
    }

    if (front) {
        memmove(&s_mgr.list[1], &s_mgr.list[0], s_mgr.count * sizeof(wifi_backup_ap_t));
        s_mgr.list[0] = ap;
    } else {
        s_mgr.list[s_mgr.count] = ap;
    }
    s_mgr.count++;
}

/**
 * @brief Keep only the APs whose hash is in @p hashes, preserving the order.
 */
static void __list_keep(const uint8_t (*hashes)[WIFI_BACKUP_HASH_LEN], uint8_t num)
{
    uint8_t i;
    uint8_t j;
    uint8_t kept = 0;

    for (i = 0; i < s_mgr.count; i++) {
        bool match = false;

        for (j = 0; j < num; j++) {
            if (0 == memcmp(s_mgr.list[i].hash, hashes[j], WIFI_BACKUP_HASH_LEN)) {
                match = true;
                break;
            }
        }

        if (match) {
            if (kept != i) {
                s_mgr.list[kept] = s_mgr.list[i];
            }
            kept++;
        }
    }

    memset(&s_mgr.list[kept], 0, (s_mgr.count - kept) * sizeof(wifi_backup_ap_t));
    s_mgr.count = kept;
}

static void __list_clear(void)
{
    memset(s_mgr.list, 0, sizeof(s_mgr.list));
    s_mgr.count = 0;
}

/***********************************************************
**********************flash persistence*********************
***********************************************************/
static OPERATE_RET __kv_save(void)
{
    cJSON      *root = NULL;
    cJSON      *arr  = NULL;
    char       *str  = NULL;
    uint8_t     i;
    OPERATE_RET rt;

    root = cJSON_CreateObject();
    if (NULL == root) {
        return OPRT_MALLOC_FAILED;
    }
    arr = cJSON_AddArrayToObject(root, "ap");
    if (NULL == arr) {
        cJSON_Delete(root);
        return OPRT_MALLOC_FAILED;
    }

    for (i = 0; i < s_mgr.count; i++) {
        cJSON *item = cJSON_CreateObject();

        if (NULL == item) {
            cJSON_Delete(root);
            return OPRT_MALLOC_FAILED;
        }
        cJSON_AddStringToObject(item, "ssid", s_mgr.list[i].ssid);
        cJSON_AddStringToObject(item, "passwd", s_mgr.list[i].passwd);
        cJSON_AddItemToArray(arr, item);
    }

    str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (NULL == str) {
        return OPRT_MALLOC_FAILED;
    }

    rt = tal_kv_set(WIFI_BACKUP_KV_KEY, (const uint8_t *)str, strlen(str));
    cJSON_free(str);

    return rt;
}

static void __kv_load(void)
{
    uint8_t *buf  = NULL;
    size_t   len  = 0;
    cJSON   *root = NULL;
    cJSON   *arr  = NULL;
    int      size = 0;
    int      i;

    if (OPRT_OK != tal_kv_get(WIFI_BACKUP_KV_KEY, &buf, &len) || NULL == buf) {
        return;
    }

    root = cJSON_ParseWithLength((const char *)buf, len);
    tal_kv_free(buf);
    if (NULL == root) {
        return;
    }

    arr  = cJSON_GetObjectItem(root, "ap");
    size = cJSON_GetArraySize(arr);
    for (i = 0; i < size && s_mgr.count < TUYA_WIFI_BACKUP_AP_NUM; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        cJSON *ssid = cJSON_GetObjectItem(item, "ssid");
        cJSON *pswd = cJSON_GetObjectItem(item, "passwd");

        if (cJSON_IsString(ssid) && cJSON_IsString(pswd)) {
            __list_push(ssid->valuestring, pswd->valuestring, false);
        }
    }

    cJSON_Delete(root);

    PR_DEBUG("wifi backup list restored, %d ap", s_mgr.count);
}

/***********************************************************
***********************reply channel************************
***********************************************************/
static int __resp(tuya_wifi_backup_trans_t trans, const char *json, uint32_t len)
{
    tuya_iot_client_t *client = NULL;

    if (TUYA_WIFI_BACKUP_TRANS_BLE == trans) {
        if (NULL == s_sender[TUYA_WIFI_BACKUP_TRANS_BLE]) {
            PR_ERR("no ble sender registered");
            return OPRT_NOT_SUPPORTED;
        }
        return s_sender[TUYA_WIFI_BACKUP_TRANS_BLE]((const uint8_t *)json, len);
    }

    client = tuya_iot_client_get();
    if (NULL == client) {
        return OPRT_INVALID_PARM;
    }

    return tuya_mqtt_protocol_data_publish(&client->mqctx, PRO_IOT_DA_RESP, (const uint8_t *)json, (uint16_t)len);
}

static void __resp_free(tuya_wifi_backup_trans_t trans, cJSON *root)
{
    char *out = cJSON_PrintUnformatted(root);

    if (NULL != out) {
        __resp(trans, out, strlen(out));
        cJSON_free(out);
    }
    cJSON_Delete(root);
}

static cJSON *__resp_head(const char *req_type, const char *tid, const char *devid)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = NULL;

    if (NULL == root) {
        return NULL;
    }
    cJSON_AddStringToObject(root, "reqType", req_type);
    data = cJSON_AddObjectToObject(root, "data");
    if (NULL == data) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddStringToObject(data, "devId", devid);
    cJSON_AddStringToObject(data, "tId", tid);

    return root;
}

static void __resp_simple(tuya_wifi_backup_trans_t trans, const char *req_type, const char *tid, const char *devid,
                          int res_code)
{
    cJSON *root = __resp_head(req_type, tid, devid);

    if (NULL == root) {
        return;
    }
    cJSON_AddNumberToObject(cJSON_GetObjectItem(root, "data"), "resCode", res_code);
    __resp_free(trans, root);
}

static void __resp_ssid_list(tuya_wifi_backup_trans_t trans, const char *req_type, const char *tid, const char *devid)
{
    cJSON  *root = __resp_head(req_type, tid, devid);
    cJSON  *data = NULL;
    cJSON  *arr  = NULL;
    uint8_t i;

    if (NULL == root) {
        return;
    }
    data = cJSON_GetObjectItem(root, "data");
    cJSON_AddNumberToObject(data, "resCode", 0);

    arr = cJSON_AddArrayToObject(data, "ssidList");
    if (NULL != arr) {
        for (i = 0; i < s_mgr.count; i++) {
            cJSON *item = cJSON_CreateString(s_mgr.list[i].ssid);
            if (NULL != item) {
                cJSON_AddItemToArray(arr, item);
            }
        }
    }

    __resp_free(trans, root);
}

/**
 * @brief Parse the {"data":{"devId":..,"tId":..}} envelope shared by the plaintext requests.
 *
 * The encrypted requests ("wifiSettingNew", "wifiSwitchNew") carry "data" as the
 * ciphertext and put "devId"/"tId" next to it on the root, so they must not use
 * this helper.
 */
static bool __req_head(cJSON *root, cJSON **data, cJSON **tid, cJSON **devid)
{
    *data = cJSON_GetObjectItem(root, "data");
    if (NULL == *data) {
        return false;
    }
    *tid   = cJSON_GetObjectItem(*data, "tId");
    *devid = cJSON_GetObjectItem(*data, "devId");
    if (!cJSON_IsString(*tid) || !cJSON_IsString(*devid)) {
        return false;
    }

    return true;
}

/**
 * @brief Collect the "hash" of each entry of an APP supplied list.
 *
 * @return the number of hashes decoded, 0 when nothing usable was found
 */
static uint8_t __req_hash_list(cJSON *list, uint8_t (*hashes)[WIFI_BACKUP_HASH_LEN])
{
    int     size = cJSON_GetArraySize(list);
    uint8_t num  = 0;
    int     i;

    if (size > TUYA_WIFI_BACKUP_AP_NUM) {
        size = TUYA_WIFI_BACKUP_AP_NUM;
    }

    for (i = 0; i < size; i++) {
        cJSON *item = cJSON_GetArrayItem(list, i);
        cJSON *hash = cJSON_GetObjectItem(item, "hash");

        if (!cJSON_IsString(hash)) {
            continue;
        }
        if (WIFI_BACKUP_HASH_LEN != __b64_decode(hash->valuestring, hashes[num], WIFI_BACKUP_HASH_LEN)) {
            continue;
        }
        num++;
    }

    return num;
}

/**
 * @brief Keep the backup list in sync with the "backupList" the APP sent.
 */
static void __apply_backup_list(cJSON *backup_list)
{
    uint8_t hashes[TUYA_WIFI_BACKUP_AP_NUM][WIFI_BACKUP_HASH_LEN];
    uint8_t num;

    if (false == cJSON_IsArray(backup_list)) {
        return;
    }

    num = __req_hash_list(backup_list, hashes);
    if (num > 0) {
        __list_keep((const uint8_t(*)[WIFI_BACKUP_HASH_LEN])hashes, num);
    } else {
        __list_clear();
    }
}

/***********************************************************
**********************protocol handlers*********************
***********************************************************/
static OPERATE_RET __on_wifi_info(cJSON *root, tuya_wifi_backup_trans_t trans)
{
    cJSON           *data  = NULL;
    cJSON           *tid   = NULL;
    cJSON           *devid = NULL;
    cJSON           *resp  = NULL;
    cJSON           *rdata = NULL;
    tuya_wfpt_attr_t attr;
    wifi_backup_ap_t cur;
    char             hash_b64[WIFI_BACKUP_B64_LEN(WIFI_BACKUP_HASH_LEN)];
    char             ssid[WIFI_SSID_LEN + 1];
    char             passwd[WIFI_PASSWD_LEN + 1];
    int8_t           rssi = 0;

    if (false == __req_head(root, &data, &tid, &devid)) {
        return OPRT_CJSON_GET_ERR;
    }

    memset(&attr, 0, sizeof(attr));
    attr.network = TUYA_WFPT_WIFI_MODE;
    if (OPRT_OK == tal_wifi_station_get_conn_ap_rssi(&rssi)) {
        attr.rssi = rssi;
    }

    memset(&cur, 0, sizeof(cur));
    if (__cur_ap_get(ssid, sizeof(ssid), passwd, sizeof(passwd))) {
        __str_copy(cur.ssid, sizeof(cur.ssid), ssid);
        __str_copy(cur.passwd, sizeof(cur.passwd), passwd);
    }
    __ap_hash(&cur);
    tuya_base64_encode(cur.hash, hash_b64, WIFI_BACKUP_HASH_LEN);

    if (NULL != s_conf_attr) {
        s_conf_attr(&attr);
    }

    PR_DEBUG("wifi info: ssid %s, signal %d, network %d, flags %d", cur.ssid, attr.rssi, attr.network, attr.flags);

    resp = __resp_head("wifiInfo", tid->valuestring, devid->valuestring);
    if (NULL == resp) {
        return OPRT_MALLOC_FAILED;
    }
    rdata = cJSON_GetObjectItem(resp, "data");
    cJSON_AddStringToObject(rdata, "ssid", cur.ssid);
    cJSON_AddStringToObject(rdata, "hash", hash_b64);
    cJSON_AddNumberToObject(rdata, "signal", attr.rssi);
    cJSON_AddNumberToObject(rdata, "network", attr.network);
    cJSON_AddNumberToObject(rdata, "version", TUYA_WIFI_BACKUP_VERSION);
    cJSON_AddNumberToObject(rdata, "flags", attr.flags);
    __resp_free(trans, resp);

    return OPRT_OK;
}

static OPERATE_RET __on_wifi_list(cJSON *root, tuya_wifi_backup_trans_t trans)
{
    cJSON  *data  = NULL;
    cJSON  *tid   = NULL;
    cJSON  *devid = NULL;
    cJSON  *resp  = NULL;
    cJSON  *rdata = NULL;
    cJSON  *arr   = NULL;
    uint8_t i;

    if (false == __req_head(root, &data, &tid, &devid)) {
        return OPRT_CJSON_GET_ERR;
    }

    resp = __resp_head("wifiList", tid->valuestring, devid->valuestring);
    if (NULL == resp) {
        return OPRT_MALLOC_FAILED;
    }
    rdata = cJSON_GetObjectItem(resp, "data");
    cJSON_AddNumberToObject(rdata, "maxNum", TUYA_WIFI_BACKUP_AP_NUM);

    arr = cJSON_AddArrayToObject(rdata, "backupList");
    for (i = 0; NULL != arr && i < s_mgr.count; i++) {
        cJSON *item = NULL;
        char   hash_b64[WIFI_BACKUP_B64_LEN(WIFI_BACKUP_HASH_LEN)];

        /* the APP already knows the current network from "wifiInfo" */
        if (__is_same_ap_as_current(s_mgr.list[i].ssid, s_mgr.list[i].passwd)) {
            continue;
        }

        tuya_base64_encode(s_mgr.list[i].hash, hash_b64, WIFI_BACKUP_HASH_LEN);
        item = cJSON_CreateObject();
        if (NULL == item) {
            continue;
        }
        cJSON_AddStringToObject(item, "ssid", s_mgr.list[i].ssid);
        cJSON_AddStringToObject(item, "hash", hash_b64);
        cJSON_AddItemToArray(arr, item);
    }
    __resp_free(trans, resp);

    return OPRT_OK;
}

static int __ecdh_rng(void *ctx, unsigned char *out, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        out[i] = (unsigned char)tal_system_get_random(0xFF);
    }

    return 0;
}

static OPERATE_RET __on_wifi_secure(cJSON *root, tuya_wifi_backup_trans_t trans)
{
    cJSON               *data   = NULL;
    cJSON               *tid    = NULL;
    cJSON               *devid  = NULL;
    cJSON               *curve  = NULL;
    cJSON               *pubkey = NULL;
    cJSON               *resp   = NULL;
    cJSON               *rdata  = NULL;
    mbedtls_ecdh_context ctx;
    /* both mbedtls_ecdh_make_public() and mbedtls_ecdh_read_public() speak the
     * TLS point format: one length byte followed by 0x04 || X || Y */
    uint8_t pub[WIFI_BACKUP_PUBKEY_LEN + 1];
    uint8_t peer[WIFI_BACKUP_PUBKEY_LEN + 1];
    char    pub_b64[WIFI_BACKUP_B64_LEN(WIFI_BACKUP_PUBKEY_LEN)];
    size_t  olen = 0;
    int     peer_len;
    int     ret;

    if (false == __req_head(root, &data, &tid, &devid)) {
        return OPRT_CJSON_GET_ERR;
    }

    curve  = cJSON_GetObjectItem(data, "curve");
    pubkey = cJSON_GetObjectItem(data, "pubkey");
    if (!cJSON_IsString(curve) || !cJSON_IsString(pubkey)) {
        return OPRT_CJSON_GET_ERR;
    }
    if (0 != strcmp(curve->valuestring, "secp256r1")) {
        PR_DEBUG("curve %s is not supported", curve->valuestring);
        return OPRT_NOT_SUPPORTED;
    }

    /* the APP sends the bare uncompressed point, prepend its length */
    memset(peer, 0, sizeof(peer));
    peer_len = __b64_decode(pubkey->valuestring, peer + 1, WIFI_BACKUP_PUBKEY_LEN);
    if (WIFI_BACKUP_PUBKEY_LEN != peer_len) {
        PR_ERR("invalid ecdh public key, %d bytes", peer_len);
        return OPRT_INVALID_PARM;
    }
    peer[0] = (uint8_t)peer_len;

    memset(pub, 0, sizeof(pub));
    mbedtls_ecdh_init(&ctx);
    mbedtls_ecdh_setup(&ctx, MBEDTLS_ECP_DP_SECP256R1);
    ret = mbedtls_ecdh_make_public(&ctx, &olen, pub, sizeof(pub), __ecdh_rng, NULL);
    if (0 == ret) {
        ret = mbedtls_ecdh_read_public(&ctx, peer, sizeof(peer));
    }
    if (0 == ret) {
        ret = mbedtls_ecdh_calc_secret(&ctx, &olen, s_mgr.ecdh_secret, sizeof(s_mgr.ecdh_secret), __ecdh_rng, NULL);
    }
    mbedtls_ecdh_free(&ctx);
    if (0 != ret) {
        PR_ERR("ecdh failed: -0x%04X", -ret);
        memset(s_mgr.ecdh_secret, 0, sizeof(s_mgr.ecdh_secret));
        return OPRT_COM_ERROR;
    }

    /* reply with the bare point as well, so both sides stay symmetric */
    tuya_base64_encode(pub + 1, pub_b64, pub[0]);

    resp = __resp_head("wifiSecure", tid->valuestring, devid->valuestring);
    if (NULL == resp) {
        return OPRT_MALLOC_FAILED;
    }
    rdata = cJSON_GetObjectItem(resp, "data");
    cJSON_AddStringToObject(rdata, "curve", curve->valuestring);
    cJSON_AddStringToObject(rdata, "pubkey", pub_b64);
    __resp_free(trans, resp);

    return OPRT_OK;
}

static OPERATE_RET __on_wifi_setting_backup(cJSON *root, tuya_wifi_backup_trans_t trans)
{
    cJSON *data        = NULL;
    cJSON *tid         = NULL;
    cJSON *devid       = NULL;
    cJSON *backup_list = NULL;

    if (false == __req_head(root, &data, &tid, &devid)) {
        return OPRT_CJSON_GET_ERR;
    }

    backup_list = cJSON_GetObjectItem(data, "backupList");
    if (false == cJSON_IsArray(backup_list)) {
        /* no list at all means the APP wants every backup removed */
        PR_DEBUG("backup list delete all");
        __list_clear();
    } else {
        __apply_backup_list(backup_list);
    }

    __kv_save();
    __resp_ssid_list(trans, "wifiSettingBackup", tid->valuestring, devid->valuestring);

    return OPRT_OK;
}

/**
 * @brief Derive the request key and decrypt a base64 payload from the APP.
 *
 * @param[in]  iv_raw    the raw IV, a 16 byte string, not base64 encoded
 * @param[out] plaintext the NUL terminated plaintext, freed with tal_free
 */
static OPERATE_RET __data_decrypt(const char *hmac_b64, const char *cipher_b64, const char *iv_raw, char **plaintext)
{
    uint8_t            secret[WIFI_BACKUP_HASH_LEN];
    uint8_t            digest[WIFI_BACKUP_HASH_LEN];
    uint8_t            hmac[WIFI_BACKUP_HASH_LEN];
    uint8_t            iv[16];
    uint8_t           *cipher = NULL;
    uint8_t           *plain  = NULL;
    uint32_t           cipher_len;
    uint32_t           plain_len = 0;
    int                actual;
    tuya_iot_client_t *client    = NULL;
    const char        *local_key = NULL;
    OPERATE_RET        rt;

    if (0 == s_mgr.ecdh_secret[0]) {
        return OPRT_AUTHENTICATION_NOT_FOUND;
    }

    if (WIFI_BACKUP_HASH_LEN != __b64_decode(hmac_b64, hmac, sizeof(hmac))) {
        return OPRT_INVALID_PARM;
    }
    if (NULL == iv_raw || strlen(iv_raw) < sizeof(iv)) {
        return OPRT_INVALID_PARM;
    }
    memcpy(iv, iv_raw, sizeof(iv));

    client    = tuya_iot_client_get();
    local_key = (NULL == client) ? "" : tuya_iot_localkey_get(client);

    tal_sha256_mac((const uint8_t *)local_key, strlen(local_key), s_mgr.ecdh_secret, sizeof(s_mgr.ecdh_secret), secret);
    tal_sha256_mac(secret, sizeof(secret) / 2, (const uint8_t *)cipher_b64, strlen(cipher_b64), digest);

    if (0 != memcmp(digest, hmac, sizeof(digest))) {
        PR_NOTICE("wifi backup hmac mismatch");
        return OPRT_AUTHENTICATION_FAIL;
    }

    cipher_len = strlen(cipher_b64) / 4 * 3 + 4;
    cipher     = (uint8_t *)tal_malloc(cipher_len);
    if (NULL == cipher) {
        return OPRT_MALLOC_FAILED;
    }
    memset(cipher, 0, cipher_len);
    cipher_len = tuya_base64_decode(cipher_b64, cipher);

    rt = tal_aes128_cbc_decode(cipher, cipher_len, secret, iv, &plain, &plain_len);
    tal_free(cipher);
    if (OPRT_OK != rt) {
        PR_ERR("wifi backup aes decode failed: %d", rt);
        return rt;
    }

    actual = tal_aes_get_actual_length(plain, plain_len);
    if (actual < 0 || (uint32_t)actual > plain_len) {
        PR_ERR("wifi backup padding check failed: %d", actual);
        tal_free(plain);
        return OPRT_INVALID_PARM;
    }

    plain[actual] = '\0';
    *plaintext    = (char *)plain;

    return OPRT_OK;
}

static OPERATE_RET __on_wifi_setting_new(cJSON *root, tuya_wifi_backup_trans_t trans)
{
    cJSON      *data      = NULL;
    cJSON      *tid       = NULL;
    cJSON      *devid     = NULL;
    cJSON      *iv        = NULL;
    cJSON      *hmac      = NULL;
    cJSON      *plain     = NULL;
    cJSON      *new_list  = NULL;
    char       *plaintext = NULL;
    int         size;
    int         i;
    OPERATE_RET rt;

    data  = cJSON_GetObjectItem(root, "data");
    tid   = cJSON_GetObjectItem(root, "tId");
    devid = cJSON_GetObjectItem(root, "devId");
    iv    = cJSON_GetObjectItem(root, "iv");
    hmac  = cJSON_GetObjectItem(root, "hmac");
    if (!cJSON_IsString(tid) || !cJSON_IsString(devid) || !cJSON_IsString(iv) || !cJSON_IsString(hmac) ||
        !cJSON_IsString(data)) {
        return OPRT_CJSON_GET_ERR;
    }

    rt = __data_decrypt(hmac->valuestring, data->valuestring, iv->valuestring, &plaintext);
    if (OPRT_OK != rt) {
        __resp_simple(trans, "wifiSettingNew", tid->valuestring, devid->valuestring, 1);
        return rt;
    }

    plain = cJSON_Parse(plaintext);
    tal_free(plaintext);
    if (NULL == plain) {
        __resp_simple(trans, "wifiSettingNew", tid->valuestring, devid->valuestring, 1);
        return OPRT_CJSON_PARSE_ERR;
    }

    new_list = cJSON_GetObjectItem(plain, "newList");
    if (false == cJSON_IsArray(new_list)) {
        cJSON_Delete(plain);
        __resp_simple(trans, "wifiSettingNew", tid->valuestring, devid->valuestring, 1);
        return OPRT_CJSON_GET_ERR;
    }

    size = cJSON_GetArraySize(new_list);
    for (i = 0; i < size; i++) {
        cJSON *item = cJSON_GetArrayItem(new_list, i);
        cJSON *ssid = cJSON_GetObjectItem(item, "ssid");
        cJSON *pswd = cJSON_GetObjectItem(item, "passwd");

        if (!cJSON_IsString(ssid) || !cJSON_IsString(pswd) || strlen(ssid->valuestring) > WIFI_SSID_LEN ||
            strlen(pswd->valuestring) > WIFI_PASSWD_LEN) {
            PR_DEBUG("wifi setting new: entry out of limit");
            cJSON_Delete(plain);
            __resp_simple(trans, "wifiSettingNew", tid->valuestring, devid->valuestring, 1);
            return OPRT_MSG_OUT_OF_LIMIT;
        }
    }

    __apply_backup_list(cJSON_GetObjectItem(plain, "backupList"));

    for (i = 0; i < size; i++) {
        cJSON *item = cJSON_GetArrayItem(new_list, i);
        cJSON *ssid = cJSON_GetObjectItem(item, "ssid");
        cJSON *pswd = cJSON_GetObjectItem(item, "passwd");

        /* the current network is not a backup of itself */
        if (__is_same_ap_as_current(ssid->valuestring, pswd->valuestring)) {
            continue;
        }
        __list_push(ssid->valuestring, pswd->valuestring, true);
    }

    cJSON_Delete(plain);

    __kv_save();
    __resp_ssid_list(trans, "wifiSettingNew", tid->valuestring, devid->valuestring);

    return OPRT_OK;
}

static void __switch_start(const char *tid, const char *devid, const char *ssid, const char *passwd,
                           tuya_wifi_backup_trans_t trans)
{
    wifi_backup_switch_fsm_t *fsm = &s_mgr.switch_fsm;

    if (WIFI_BACKUP_STATUS_IDLE != s_mgr.status) {
        PR_DEBUG("wifi backup is busy");
        return;
    }

    memset(fsm, 0, sizeof(*fsm));
    fsm->trans = trans;
    __str_copy(fsm->tid, sizeof(fsm->tid), tid);
    __str_copy(fsm->devid, sizeof(fsm->devid), devid);
    __str_copy(fsm->ap.ssid, sizeof(fsm->ap.ssid), ssid);
    __str_copy(fsm->ap.passwd, sizeof(fsm->ap.passwd), passwd);
    __ap_hash(&fsm->ap);

    __orig_capture();

    s_mgr.status = WIFI_BACKUP_STATUS_SWITCH;
    if (OPRT_OK != tal_workq_start_delayed(s_mgr.fsm_work, WIFI_BACKUP_SWITCH_INTERVAL_MS, LOOP_CYCLE)) {
        s_mgr.status = WIFI_BACKUP_STATUS_IDLE;
        return;
    }

    PR_DEBUG("wifi backup switch to %s", fsm->ap.ssid);
    __state_notify(TUYA_WFPT_SWICTH_START);
    __switch_fsm_run(fsm);
}

static OPERATE_RET __on_wifi_switch_backup(cJSON *root, tuya_wifi_backup_trans_t trans)
{
    cJSON            *data  = NULL;
    cJSON            *tid   = NULL;
    cJSON            *devid = NULL;
    cJSON            *hash  = NULL;
    uint8_t           hash_value[WIFI_BACKUP_HASH_LEN];
    wifi_backup_ap_t *ap = NULL;

    if (false == __req_head(root, &data, &tid, &devid)) {
        return OPRT_CJSON_GET_ERR;
    }

    hash = cJSON_GetObjectItem(data, "hash");
    if (!cJSON_IsString(hash) ||
        WIFI_BACKUP_HASH_LEN != __b64_decode(hash->valuestring, hash_value, sizeof(hash_value))) {
        return OPRT_CJSON_GET_ERR;
    }

    ap = __list_find(hash_value);
    if (NULL == ap) {
        PR_DEBUG("wifi switch: ap not found in the backup list");
        return OPRT_NOT_FOUND;
    }

    if (WIFI_BACKUP_STATUS_IDLE != s_mgr.status) {
        PR_DEBUG("wifi switch is already ongoing");
        return OPRT_COM_ERROR;
    }

    if (__is_same_ap_as_current(ap->ssid, ap->passwd)) {
        /* nothing to switch to, but the APP should stop showing it as a backup */
        __resp_simple(trans, "wifiSwitchNew", tid->valuestring, devid->valuestring, 1);
        __list_remove_at((uint8_t)(ap - s_mgr.list));
        __kv_save();
        return OPRT_OK;
    }

    __switch_start(tid->valuestring, devid->valuestring, ap->ssid, ap->passwd, trans);

    return OPRT_OK;
}

static OPERATE_RET __on_wifi_switch_new(cJSON *root, tuya_wifi_backup_trans_t trans)
{
    cJSON      *data      = NULL;
    cJSON      *tid       = NULL;
    cJSON      *devid     = NULL;
    cJSON      *iv        = NULL;
    cJSON      *hmac      = NULL;
    cJSON      *plain     = NULL;
    char       *plaintext = NULL;
    cJSON      *ssid      = NULL;
    cJSON      *pswd      = NULL;
    OPERATE_RET rt;

    data  = cJSON_GetObjectItem(root, "data");
    tid   = cJSON_GetObjectItem(root, "tId");
    devid = cJSON_GetObjectItem(root, "devId");
    iv    = cJSON_GetObjectItem(root, "iv");
    hmac  = cJSON_GetObjectItem(root, "hmac");
    if (!cJSON_IsString(tid) || !cJSON_IsString(devid) || !cJSON_IsString(iv) || !cJSON_IsString(hmac) ||
        !cJSON_IsString(data)) {
        return OPRT_CJSON_GET_ERR;
    }

    rt = __data_decrypt(hmac->valuestring, data->valuestring, iv->valuestring, &plaintext);
    if (OPRT_OK != rt) {
        __resp_simple(trans, "wifiSwitchNew", tid->valuestring, devid->valuestring, 3);
        return rt;
    }

    plain = cJSON_Parse(plaintext);
    tal_free(plaintext);
    if (NULL == plain) {
        __resp_simple(trans, "wifiSwitchNew", tid->valuestring, devid->valuestring, 3);
        return OPRT_CJSON_PARSE_ERR;
    }

    ssid = cJSON_GetObjectItem(plain, "ssid");
    pswd = cJSON_GetObjectItem(plain, "passwd");
    if (!cJSON_IsString(ssid) || !cJSON_IsString(pswd) || strlen(ssid->valuestring) > WIFI_SSID_LEN ||
        strlen(pswd->valuestring) > WIFI_PASSWD_LEN || __is_same_ap_as_current(ssid->valuestring, pswd->valuestring) ||
        WIFI_BACKUP_STATUS_IDLE != s_mgr.status) {
        cJSON_Delete(plain);
        __resp_simple(trans, "wifiSwitchNew", tid->valuestring, devid->valuestring, 3);
        return OPRT_INVALID_PARM;
    }

    __switch_start(tid->valuestring, devid->valuestring, ssid->valuestring, pswd->valuestring, trans);
    cJSON_Delete(plain);

    return OPRT_OK;
}

typedef OPERATE_RET (*wifi_backup_cmd_cb_t)(cJSON *root, tuya_wifi_backup_trans_t trans);

typedef struct {
    const char          *req_type;
    wifi_backup_cmd_cb_t cb;
} wifi_backup_cmd_t;

static const wifi_backup_cmd_t s_cmd_table[] = {
    {"wifiInfo", __on_wifi_info},
    {"wifiList", __on_wifi_list},
    {"wifiSecure", __on_wifi_secure},
    {"wifiSettingBackup", __on_wifi_setting_backup},
    {"wifiSettingNew", __on_wifi_setting_new},
    {"wifiSwitchBackup", __on_wifi_switch_backup},
    {"wifiSwitchNew", __on_wifi_switch_new},
};

#define WIFI_BACKUP_CMD_NUM (sizeof(s_cmd_table) / sizeof(s_cmd_table[0]))

static OPERATE_RET __proto_process(cJSON *root, tuya_wifi_backup_trans_t trans)
{
    cJSON   *req_type = cJSON_GetObjectItem(root, "reqType");
    uint32_t i;

    if (!cJSON_IsString(req_type)) {
        PR_ERR("wifi backup request has no reqType");
        return OPRT_CJSON_GET_ERR;
    }

    s_mgr.trans = trans;

    for (i = 0; i < WIFI_BACKUP_CMD_NUM; i++) {
        if (0 == strcmp(req_type->valuestring, s_cmd_table[i].req_type)) {
            return s_cmd_table[i].cb(root, trans);
        }
    }

    PR_ERR("unknown wifi backup reqType: %s", req_type->valuestring);

    return OPRT_NOT_FOUND;
}

static int __mqtt_dispatch_cb(cJSON *json, void *user_data)
{
    return __proto_process(json, TUYA_WIFI_BACKUP_TRANS_CLOUD);
}

/***********************************************************
*************************state notify***********************
***********************************************************/
static void __state_notify(tuya_wfpt_state_t state)
{
    switch (state) {
    case TUYA_WFPT_SWICTH_START:
        /* the backup service owns the radio until the switch settles */
        netconn_wifi_station_suspend(true);
        break;
    case TUYA_WFPT_SWICTH_ORIGINAL:
    case TUYA_WFPT_SWICTH_SUCCESS:
    case TUYA_WFPT_SWICTH_FALIED:
        netconn_wifi_station_suspend(false);
        break;
    default:
        break;
    }

    if (NULL != s_mgr.state_cb) {
        s_mgr.state_cb(state);
    }
}

/***********************************************************
***********************switch fsm***************************
***********************************************************/
/**
 * @brief Give the station back to netmgr on the network we came from.
 *
 * netmgr retries whatever credential is in the connection info, and a switch
 * only stores the target there, so without this the device would keep hammering
 * an AP that just proved unreachable instead of returning to the one it had.
 */
static void __switch_give_up(void)
{
    if ('\0' == s_mgr.orig_ssid[0]) {
        return;
    }

    PR_DEBUG("wifi backup gave up, back to %s", s_mgr.orig_ssid);
    netconn_wifi_station_set(s_mgr.orig_ssid, s_mgr.orig_passwd, false);
}

/**
 * @brief Commit a successful switch: @p ap becomes the current network and the
 *        previous one takes its place in the backup list.
 */
static void __switch_commit(const wifi_backup_ap_t *ap)
{
    wifi_backup_ap_t *found = __list_find(ap->hash);

    if (NULL != found) {
        __list_remove_at((uint8_t)(found - s_mgr.list));
    }
    __list_push(s_mgr.orig_ssid, s_mgr.orig_passwd, true);
    __kv_save();

    /* persist the new network so the next boot connects to it directly */
    netconn_wifi_station_info_save(ap->ssid, ap->passwd);
}

static void __switch_fsm_run(wifi_backup_switch_fsm_t *fsm)
{
    uint32_t now = (uint32_t)tal_system_get_millisecond();

    switch (fsm->state) {
    case WIFI_BACKUP_FSM_STATE_INIT:
        if (fsm->recover) {
            PR_DEBUG("wifi backup switch rollback to %s", s_mgr.orig_ssid);
            __state_notify(TUYA_WFPT_SWICTH_ORIGINAL);
            if ('\0' != s_mgr.orig_ssid[0]) {
                netconn_wifi_station_set(s_mgr.orig_ssid, s_mgr.orig_passwd, false);
            }
        } else {
            tuya_iot_client_t *client = tuya_iot_client_get();

            PR_DEBUG("wifi backup switch to %s", fsm->ap.ssid);
            if (NULL != client) {
                tuya_iot_reconnect(client);
            }
            netconn_wifi_station_set(fsm->ap.ssid, fsm->ap.passwd, false);
        }
        fsm->timeout = now + WIFI_BACKUP_SWITCH_TIMEOUT_MS;
        fsm->state   = WIFI_BACKUP_FSM_STATE_CHECK;
        break;

    case WIFI_BACKUP_FSM_STATE_CHECK:
        if ((int32_t)(now - fsm->timeout) > 0) {
            PR_DEBUG("wifi backup switch timeout, recover %d", fsm->recover);
            if (fsm->recover) {
                fsm->state = WIFI_BACKUP_FSM_STATE_STOP;
            } else {
                fsm->recover = true;
                fsm->state   = WIFI_BACKUP_FSM_STATE_INIT;
            }
            break;
        }

        if (false == __link_up() || false == __cloud_connected()) {
            break;
        }

        if (false == fsm->recover) {
            __switch_commit(&fsm->ap);
        }
        __resp_simple(fsm->trans, "wifiSwitchNew", fsm->tid, fsm->devid, fsm->recover ? 2 : 1);
        s_mgr.status = WIFI_BACKUP_STATUS_IDLE;
        tal_workq_stop_delayed(s_mgr.fsm_work);
        __state_notify(TUYA_WFPT_SWICTH_SUCCESS);
        break;

    case WIFI_BACKUP_FSM_STATE_STOP:
        s_mgr.status = WIFI_BACKUP_STATUS_IDLE;
        tal_workq_stop_delayed(s_mgr.fsm_work);
        __state_notify(TUYA_WFPT_SWICTH_FALIED);
        __switch_give_up();
        break;

    default:
        break;
    }
}

/***********************************************************
**********************connect fsm***************************
***********************************************************/
/**
 * @brief Order the backup APs found in a scan: current AP first, then by RSSI.
 */
static OPERATE_RET __connect_select(AP_IF_S *ap_list, uint8_t ap_num)
{
    wifi_backup_connect_fsm_t *connect = &s_mgr.connect_fsm;
    char                       cur_ssid[WIFI_SSID_LEN + 1];
    char                       cur_passwd[WIFI_PASSWD_LEN + 1];
    bool                       has_cur;
    uint8_t                    i;
    uint8_t                    j;
    uint8_t                    k;

    memset(connect, 0, sizeof(*connect));
    has_cur = __cur_ap_get(cur_ssid, sizeof(cur_ssid), cur_passwd, sizeof(cur_passwd));

    for (i = 0; i < s_mgr.count; i++) {
        for (j = 0; j < ap_num; j++) {
            if (0 != strcmp((char *)ap_list[j].ssid, s_mgr.list[i].ssid)) {
                continue;
            }
            s_mgr.list[i].flag = WIFI_BACKUP_AP_EXIST_FLAG;
            s_mgr.list[i].rssi = ap_list[j].rssi;
            if (has_cur && 0 == strcmp(s_mgr.list[i].ssid, cur_ssid)) {
                s_mgr.list[i].flag |= WIFI_BACKUP_AP_PRIOR_FLAG;
            }
            connect->ap[connect->count++] = &s_mgr.list[i];
            break;
        }
    }

    /* selection sort: the current AP first, the strongest RSSI afterwards */
    for (i = 0; i < connect->count; i++) {
        wifi_backup_ap_t *tmp = NULL;

        k = i;
        if (connect->ap[i]->flag & WIFI_BACKUP_AP_PRIOR_FLAG) {
            continue;
        }
        for (j = i + 1; j < connect->count; j++) {
            if (connect->ap[j]->flag & WIFI_BACKUP_AP_PRIOR_FLAG) {
                k = j;
                break;
            }
        }
        for (j = i + 1; k == i && j < connect->count; j++) {
            if (connect->ap[j]->rssi > connect->ap[k]->rssi) {
                k = j;
            }
        }
        if (k != i) {
            tmp            = connect->ap[i];
            connect->ap[i] = connect->ap[k];
            connect->ap[k] = tmp;
        }
    }

    for (i = 0; i < connect->count; i++) {
        PR_DEBUG("wifi backup candidate %s, rssi %d", connect->ap[i]->ssid, connect->ap[i]->rssi);
    }

    return (connect->count > 0) ? OPRT_OK : OPRT_NOT_FOUND;
}

static void __connect_fsm_run(wifi_backup_connect_fsm_t *fsm)
{
    uint32_t          now = (uint32_t)tal_system_get_millisecond();
    wifi_backup_ap_t *ap  = NULL;

    switch (fsm->state) {
    case WIFI_BACKUP_FSM_STATE_INIT:
        ap = fsm->ap[fsm->index];
        PR_DEBUG("wifi backup connect %s", ap->ssid);
        netconn_wifi_station_set(ap->ssid, ap->passwd, false);
        fsm->timeout = now + WIFI_BACKUP_CONNECT_TIMEOUT_MS;
        fsm->state   = WIFI_BACKUP_FSM_STATE_CHECK;
        break;

    case WIFI_BACKUP_FSM_STATE_CHECK:
        if ((int32_t)(now - fsm->timeout) > 0) {
            PR_DEBUG("wifi backup connect timeout for %s", fsm->ap[fsm->index]->ssid);
            fsm->state = WIFI_BACKUP_FSM_STATE_STOP;
            break;
        }

        if (false == __link_up()) {
            break;
        }

        __switch_commit(fsm->ap[fsm->index]);
        s_mgr.status = WIFI_BACKUP_STATUS_IDLE;
        tal_workq_stop_delayed(s_mgr.fsm_work);
        __state_notify(TUYA_WFPT_SWICTH_SUCCESS);
        fsm->state = WIFI_BACKUP_FSM_STATE_DONE;
        break;

    case WIFI_BACKUP_FSM_STATE_STOP:
        fsm->state = WIFI_BACKUP_FSM_STATE_INIT;
        if (++fsm->index >= fsm->count) {
            s_mgr.status = WIFI_BACKUP_STATUS_IDLE;
            memset(fsm, 0, sizeof(*fsm));
            tal_workq_stop_delayed(s_mgr.fsm_work);
            __state_notify(TUYA_WFPT_SWICTH_FALIED);
            __switch_give_up();
        }
        break;

    default:
        break;
    }
}

static void __fsm_task(void *data)
{
    if (WIFI_BACKUP_STATUS_SWITCH == s_mgr.status) {
        __switch_fsm_run(&s_mgr.switch_fsm);
    } else if (WIFI_BACKUP_STATUS_CONNECT == s_mgr.status) {
        __connect_fsm_run(&s_mgr.connect_fsm);
    } else {
        PR_DEBUG("wifi backup invalid status: %d", s_mgr.status);
        tal_workq_stop_delayed(s_mgr.fsm_work);
    }
}

/***********************************************************
*******************fallback reconnect***********************
***********************************************************/
static void __offline_scan_task(void *data)
{
    AP_IF_S *ap_list = NULL;
    uint32_t ap_num  = 0;

    if (OPRT_OK != tal_wifi_all_ap_scan(&ap_list, &ap_num) || NULL == ap_list || 0 == ap_num) {
        PR_DEBUG("wifi backup scan found nothing");
        if (NULL != ap_list) {
            tal_wifi_release_ap(ap_list);
        }
        return;
    }

    if (ap_num > 0xFF) {
        ap_num = 0xFF;
    }
    if (OPRT_OK == tuya_wifi_backup_ap_check(ap_list, (uint8_t)ap_num)) {
        PR_NOTICE("wifi backup reconnect started");
    }

    tal_wifi_release_ap(ap_list);
}

static void __offline_tick(TIMER_ID timer_id, void *arg)
{
    bool trigger = false;
    uint8_t count   = 0;

    s_mgr.local_num = __link_up() ? 0 : s_mgr.local_num + 1;
    s_mgr.cloud_num = __cloud_connected() ? 0 : s_mgr.cloud_num + 1;

    if (0 == s_mgr.local_num && 0 == s_mgr.cloud_num) {
        tal_sw_timer_stop(s_mgr.offline_timer);
        return;
    }
    if (WIFI_BACKUP_STATUS_IDLE != s_mgr.status) {
        return;
    }

    /* the ap currently configured is not a fallback, it is the one that just
     * failed, so it is left out of the count */
    count = tuya_wifi_backup_count_get();
    if (0 == count) {
        if (0 == s_mgr.local_num % WIFI_BACKUP_LOCAL_RECONNECT_TIME) {
            PR_DEBUG("offline for %us, but %d ap in the list is the current one", s_mgr.local_num, s_mgr.count);
        }
        return;
    }

    if (s_mgr.local_num && 0 == s_mgr.local_num % WIFI_BACKUP_LOCAL_RECONNECT_TIME) {
        trigger = true;
    }
    if (s_mgr.cloud_num && 0 == (s_mgr.local_num + s_mgr.cloud_num) % WIFI_BACKUP_CLOUD_RECONNECT_TIME) {
        trigger = true;
    }
    if (false == trigger) {
        return;
    }

    PR_NOTICE("offline for %us (cloud %us), try %d backup network(s)", s_mgr.local_num, s_mgr.cloud_num, count);
    tal_workq_schedule(WORKQ_SYSTEM, __offline_scan_task, NULL);
}

/***********************************************************
***********************public api***************************
***********************************************************/
OPERATE_RET tuya_wifi_backup_ap_check(AP_IF_S *ap_list, uint8_t ap_num)
{
    OPERATE_RET rt;

    if (NULL == ap_list || 0 == ap_num) {
        return OPRT_INVALID_PARM;
    }
    if (WIFI_BACKUP_STATUS_IDLE != s_mgr.status) {
        return OPRT_COM_ERROR;
    }

    rt = __connect_select(ap_list, ap_num);
    if (OPRT_OK != rt) {
        PR_DEBUG("no backup ap is in range");
        return rt;
    }

    __orig_capture();
    s_mgr.status = WIFI_BACKUP_STATUS_CONNECT;
    rt           = tal_workq_start_delayed(s_mgr.fsm_work, WIFI_BACKUP_CONNECT_INTERVAL_MS, LOOP_CYCLE);
    if (OPRT_OK != rt) {
        s_mgr.status = WIFI_BACKUP_STATUS_IDLE;
        return rt;
    }

    __state_notify(TUYA_WFPT_SWICTH_START);

    return OPRT_OK;
}

OPERATE_RET tuya_wifi_backup_is_switching(void)
{
    return (WIFI_BACKUP_STATUS_IDLE == s_mgr.status) ? OPRT_COM_ERROR : OPRT_OK;
}

uint8_t tuya_wifi_backup_count_get(void)
{
    uint8_t i;
    uint8_t count = 0;

    for (i = 0; i < s_mgr.count; i++) {
        if (false == __is_same_ap_as_current(s_mgr.list[i].ssid, s_mgr.list[i].passwd)) {
            count++;
        }
    }

    return count;
}

OPERATE_RET tuya_wifi_backup_list_get(void (*cb)(const char *ssid, const char *passwd))
{
    char    ssid[WIFI_SSID_LEN + 1];
    char    passwd[WIFI_PASSWD_LEN + 1];
    uint8_t i;

    if (NULL == cb) {
        return OPRT_INVALID_PARM;
    }

    if (__cur_ap_get(ssid, sizeof(ssid), passwd, sizeof(passwd))) {
        cb(ssid, passwd);
    }

    for (i = 0; i < s_mgr.count; i++) {
        if (__is_same_ap_as_current(s_mgr.list[i].ssid, s_mgr.list[i].passwd)) {
            continue;
        }
        cb(s_mgr.list[i].ssid, s_mgr.list[i].passwd);
    }

    return OPRT_OK;
}

OPERATE_RET tuya_wifi_backup_current_ap_add(void)
{
    char ssid[WIFI_SSID_LEN + 1];
    char passwd[WIFI_PASSWD_LEN + 1];

    if (false == __cur_ap_get(ssid, sizeof(ssid), passwd, sizeof(passwd))) {
        PR_DEBUG("no current wifi to add");
        return OPRT_INVALID_PARM;
    }

    __list_push(ssid, passwd, true);

    return __kv_save();
}

OPERATE_RET tuya_wifi_backup_current_ap_in_list(void)
{
    uint8_t i;

    for (i = 0; i < s_mgr.count; i++) {
        if (__is_same_ap_as_current(s_mgr.list[i].ssid, s_mgr.list[i].passwd)) {
            return OPRT_OK;
        }
    }

    return OPRT_NOT_FOUND;
}

OPERATE_RET tuya_wifi_backup_test_add(const char *ssid, const char *passwd)
{
    if (NULL == ssid || '\0' == ssid[0]) {
        return OPRT_INVALID_PARM;
    }

    __list_push(ssid, (NULL == passwd) ? "" : passwd, true);

    return __kv_save();
}

OPERATE_RET tuya_wifi_backup_clear(void)
{
    __list_clear();

    return tal_kv_del(WIFI_BACKUP_KV_KEY);
}

OPERATE_RET tuya_wifi_backup_switch(const char *ssid, const char *passwd)
{
    if (NULL == ssid || '\0' == ssid[0]) {
        return OPRT_INVALID_PARM;
    }
    if (WIFI_BACKUP_STATUS_IDLE != s_mgr.status) {
        return OPRT_COM_ERROR;
    }

    __switch_start("00000000", "", ssid, passwd, TUYA_WIFI_BACKUP_TRANS_CLOUD);

    return (WIFI_BACKUP_STATUS_IDLE == s_mgr.status) ? OPRT_COM_ERROR : OPRT_OK;
}

OPERATE_RET tuya_wifi_backup_conf_attr(int (*conf_attr)(tuya_wfpt_attr_t *attr))
{
    s_conf_attr = conf_attr;

    return OPRT_OK;
}

void tuya_wifi_backup_state_cb_set(tuya_wfpt_state_cb_t *cb)
{
    s_mgr.state_cb = cb;
}

void tuya_wifi_backup_disable(void)
{
    s_enable = false;
}

bool tuya_wifi_backup_is_enable(void)
{
    return s_enable;
}

void tuya_wifi_backup_sender_set(tuya_wifi_backup_trans_t trans, tuya_wifi_backup_sender_t sender)
{
    if (trans > TUYA_WIFI_BACKUP_TRANS_BLE) {
        return;
    }

    s_sender[trans] = sender;
}

OPERATE_RET tuya_wifi_backup_proto_process(const char *json, uint32_t len, tuya_wifi_backup_trans_t trans)
{
    cJSON      *root = NULL;
    OPERATE_RET rt;

    if (false == s_mgr.inited) {
        return OPRT_RESOURCE_NOT_READY;
    }
    if (NULL == json || 0 == len) {
        return OPRT_INVALID_PARM;
    }

    root = cJSON_ParseWithLength(json, len);
    if (NULL == root) {
        PR_ERR("wifi backup request is not valid json");
        return OPRT_CJSON_PARSE_ERR;
    }

    rt = __proto_process(root, trans);
    cJSON_Delete(root);

    return rt;
}

/***********************************************************
***********************life cycle***************************
***********************************************************/
static OPERATE_RET __reset_event_cb(void *data)
{
    PR_DEBUG("wifi backup list cleared by reset");
    __list_clear();
    memset(s_mgr.ecdh_secret, 0, sizeof(s_mgr.ecdh_secret));

    return tal_kv_del(WIFI_BACKUP_KV_KEY);
}

static OPERATE_RET __link_status_cb(void *data)
{
    netmgr_status_e status = *(netmgr_status_e *)data;

    if ((NETMGR_LINK_UP == status || NETMGR_LINK_UP_SWITH == status) && __cloud_connected()) {
        tal_sw_timer_stop(s_mgr.offline_timer);
        return OPRT_OK;
    }

    s_mgr.local_num = 0;
    tal_sw_timer_start(s_mgr.offline_timer, WIFI_BACKUP_OFFLINE_TICK_MS, TAL_TIMER_CYCLE);

    return OPRT_OK;
}

static OPERATE_RET __mqtt_event_cb(void *data)
{
    if (__cloud_connected()) {
        s_mgr.cloud_num = 0;
        return OPRT_OK;
    }

    s_mgr.cloud_num = 0;
    tal_sw_timer_start(s_mgr.offline_timer, WIFI_BACKUP_OFFLINE_TICK_MS, TAL_TIMER_CYCLE);

    return OPRT_OK;
}

static void __proto_register(void)
{
    uint32_t i;

    for (i = 0; i < WIFI_BACKUP_CMD_NUM; i++) {
        tuya_mqtt_dispatch_register(PRO_IOT_DA_REQ, s_cmd_table[i].req_type, "wifi backup", __mqtt_dispatch_cb, NULL);
    }
}

OPERATE_RET tuya_wifi_backup_init(void)
{
    OPERATE_RET rt;

    if (s_mgr.inited) {
        return OPRT_OK;
    }
    if (false == s_enable) {
        PR_DEBUG("wifi backup is disabled");
        return OPRT_OK;
    }

    memset(&s_mgr, 0, sizeof(s_mgr));
    s_mgr.inited = true;

    __kv_load();

    rt = tal_workq_init_delayed(WORKQ_HIGHTPRI, __fsm_task, &s_mgr, &s_mgr.fsm_work);
    if (OPRT_OK != rt) {
        s_mgr.inited = false;
        return rt;
    }
    rt = tal_sw_timer_create(__offline_tick, NULL, &s_mgr.offline_timer);
    if (OPRT_OK != rt) {
        s_mgr.inited = false;
        return rt;
    }

    tal_event_subscribe(EVENT_RESET, "wifi_bak", __reset_event_cb, SUBSCRIBE_TYPE_NORMAL);
    tal_event_subscribe(EVENT_LINK_STATUS_CHG, "wifi_bak", __link_status_cb, SUBSCRIBE_TYPE_NORMAL);
    tal_event_subscribe(EVENT_MQTT_DISCONNECTED, "wifi_bak", __mqtt_event_cb, SUBSCRIBE_TYPE_NORMAL);
    tal_event_subscribe(EVENT_MQTT_CONNECTED, "wifi_bak", __mqtt_event_cb, SUBSCRIBE_TYPE_NORMAL);

    __proto_register();

    /* A device that boots without a reachable network never sees a link status
     * change nor an mqtt disconnect, so neither callback below would ever arm
     * the offline tick. Arm it here as well, otherwise the backup list is never
     * tried on a device that comes up offline. */
    if (false == __link_up() || false == __cloud_connected()) {
        tal_sw_timer_start(s_mgr.offline_timer, WIFI_BACKUP_OFFLINE_TICK_MS, TAL_TIMER_CYCLE);
    }

    PR_DEBUG("wifi backup initialized, %d ap", s_mgr.count);

    return OPRT_OK;
}
