/**
 * @file wifi_backup_proto.c
 * @brief Backup network APP protocol handlers and ECDH/AES/HMAC crypto.
 *
 * Serves "wifiInfo", "wifiList", "wifiSecure", "wifiSettingBackup",
 * "wifiSettingNew", "wifiSwitchBackup", and "wifiSwitchNew" over cloud MQTT
 * and BLE transparent channel 0x000F.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "wifi_backup_internal.h"

#include "tal_log.h"
#include "tal_memory.h"
#include "cJSON.h"
#include "mix_method.h"
#include "uni_random.h"
#include "mbedtls/ecdh.h"

#include "tuya_iot.h"
#include "mqtt_service.h"
#include "tuya_mqtt_dispatch.h"

/***********************************************************
***********************variable define**********************
***********************************************************/
static tuya_wifi_backup_sender_t s_sender[2] = {NULL, NULL};

static OPERATE_RET __proto_process(cJSON *root, tuya_wifi_backup_trans_t trans);

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

void wifi_backup_resp_simple(tuya_wifi_backup_trans_t trans, const char *req_type, const char *tid, const char *devid,
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
    wifi_backup_lock();
    if (NULL != arr) {
        for (i = 0; i < g_wifi_backup_mgr.count; i++) {
            cJSON *item = cJSON_CreateString(g_wifi_backup_mgr.list[i].ssid);
            if (NULL != item) {
                cJSON_AddItemToArray(arr, item);
            }
        }
    }
    wifi_backup_unlock();

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
        wifi_backup_list_keep((const uint8_t(*)[WIFI_BACKUP_HASH_LEN])hashes, num);
    } else {
        wifi_backup_list_clear();
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
    if (wifi_backup_cur_ap_get(ssid, sizeof(ssid), passwd, sizeof(passwd))) {
        wifi_backup_str_copy(cur.ssid, sizeof(cur.ssid), ssid);
        wifi_backup_str_copy(cur.passwd, sizeof(cur.passwd), passwd);
    }
    wifi_backup_ap_hash(&cur);
    tuya_base64_encode(cur.hash, hash_b64, WIFI_BACKUP_HASH_LEN);

    if (NULL != g_wifi_backup_conf_attr) {
        g_wifi_backup_conf_attr(&attr);
    }

    PR_DEBUG("wifi info: ssid %s, signal %d, network %d, flags %u", cur.ssid, (int)attr.rssi, (int)attr.network,
             (unsigned int)attr.flags);

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
    wifi_backup_lock();
    for (i = 0; NULL != arr && i < g_wifi_backup_mgr.count; i++) {
        cJSON *item = NULL;
        char   hash_b64[WIFI_BACKUP_B64_LEN(WIFI_BACKUP_HASH_LEN)];

        /* the APP already knows the current network from "wifiInfo" */
        if (wifi_backup_is_same_ap_as_current(g_wifi_backup_mgr.list[i].ssid, g_wifi_backup_mgr.list[i].passwd)) {
            continue;
        }

        tuya_base64_encode(g_wifi_backup_mgr.list[i].hash, hash_b64, WIFI_BACKUP_HASH_LEN);
        item = cJSON_CreateObject();
        if (NULL == item) {
            continue;
        }
        cJSON_AddStringToObject(item, "ssid", g_wifi_backup_mgr.list[i].ssid);
        cJSON_AddStringToObject(item, "hash", hash_b64);
        cJSON_AddItemToArray(arr, item);
    }
    wifi_backup_unlock();
    __resp_free(trans, resp);

    return OPRT_OK;
}

static int __ecdh_rng(void *ctx, unsigned char *out, size_t len)
{
    (void)ctx;

    uni_random_bytes(out, len);

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

    uint8_t shared[WIFI_BACKUP_HASH_LEN];
    memset(shared, 0, sizeof(shared));
    memset(pub, 0, sizeof(pub));
    mbedtls_ecdh_init(&ctx);
    ret = mbedtls_ecdh_setup(&ctx, MBEDTLS_ECP_DP_SECP256R1);
    if (0 == ret) {
        ret = mbedtls_ecdh_make_public(&ctx, &olen, pub, sizeof(pub), __ecdh_rng, NULL);
    }
    if (0 == ret) {
        ret = mbedtls_ecdh_read_public(&ctx, peer, sizeof(peer));
    }
    if (0 == ret) {
        ret = mbedtls_ecdh_calc_secret(&ctx, &olen, shared, sizeof(shared), __ecdh_rng, NULL);
    }
    mbedtls_ecdh_free(&ctx);
    wifi_backup_lock();
    if (0 != ret) {
        memset(g_wifi_backup_mgr.ecdh_secret, 0, sizeof(g_wifi_backup_mgr.ecdh_secret));
        g_wifi_backup_mgr.ecdh_ready = false;
        wifi_backup_unlock();
        PR_ERR("ecdh failed: -0x%04X", -ret);
        return OPRT_COM_ERROR;
    }
    memcpy(g_wifi_backup_mgr.ecdh_secret, shared, sizeof(g_wifi_backup_mgr.ecdh_secret));
    g_wifi_backup_mgr.ecdh_ready = true;
    wifi_backup_unlock();
    memset(shared, 0, sizeof(shared));

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
    wifi_backup_lock();
    if (false == cJSON_IsArray(backup_list)) {
        /* no list at all means the APP wants every backup removed */
        PR_DEBUG("backup list delete all");
        wifi_backup_list_clear();
    } else {
        __apply_backup_list(backup_list);
    }
    wifi_backup_kv_save();
    wifi_backup_unlock();

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
    uint8_t            ecdh_secret[WIFI_BACKUP_HASH_LEN];
    uint8_t            secret[WIFI_BACKUP_HASH_LEN];
    uint8_t            digest[WIFI_BACKUP_HASH_LEN];
    uint8_t            hmac[WIFI_BACKUP_HASH_LEN];
    uint8_t            iv[16];
    uint8_t           *cipher = NULL;
    uint8_t           *plain  = NULL;
    uint32_t           cipher_buf_len;
    int                dec_len;
    uint32_t           plain_len = 0;
    int                actual;
    tuya_iot_client_t *client    = NULL;
    const char        *local_key = NULL;
    OPERATE_RET        rt;

    wifi_backup_lock();
    if (false == g_wifi_backup_mgr.ecdh_ready) {
        wifi_backup_unlock();
        return OPRT_AUTHENTICATION_NOT_FOUND;
    }
    memcpy(ecdh_secret, g_wifi_backup_mgr.ecdh_secret, sizeof(ecdh_secret));
    wifi_backup_unlock();

    if (WIFI_BACKUP_HASH_LEN != __b64_decode(hmac_b64, hmac, sizeof(hmac))) {
        return OPRT_INVALID_PARM;
    }
    if (NULL == iv_raw || strlen(iv_raw) < sizeof(iv)) {
        return OPRT_INVALID_PARM;
    }
    memcpy(iv, iv_raw, sizeof(iv));

    client    = tuya_iot_client_get();
    local_key = (NULL == client) ? "" : tuya_iot_localkey_get(client);

    tal_sha256_mac((const uint8_t *)local_key, strlen(local_key), ecdh_secret, sizeof(ecdh_secret), secret);
    memset(ecdh_secret, 0, sizeof(ecdh_secret));
    tal_sha256_mac(secret, sizeof(secret) / 2, (const uint8_t *)cipher_b64, strlen(cipher_b64), digest);

    if (0 != memcmp(digest, hmac, sizeof(digest))) {
        PR_NOTICE("wifi backup hmac mismatch");
        return OPRT_AUTHENTICATION_FAIL;
    }

    cipher_buf_len = (uint32_t)(strlen(cipher_b64) / 4 * 3 + 4);
    cipher         = (uint8_t *)tal_malloc(cipher_buf_len);
    if (NULL == cipher) {
        return OPRT_MALLOC_FAILED;
    }
    memset(cipher, 0, cipher_buf_len);
    dec_len = tuya_base64_decode(cipher_b64, cipher);
    if (dec_len <= 0 || (uint32_t)dec_len > cipher_buf_len || (dec_len % 16) != 0) {
        PR_ERR("wifi backup invalid cipher len: %d", dec_len);
        tal_free(cipher);
        return OPRT_INVALID_PARM;
    }

    rt = tal_aes128_cbc_decode(cipher, (uint32_t)dec_len, secret, iv, &plain, &plain_len);
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
        wifi_backup_resp_simple(trans, "wifiSettingNew", tid->valuestring, devid->valuestring, 1);
        return rt;
    }

    plain = cJSON_Parse(plaintext);
    tal_free(plaintext);
    if (NULL == plain) {
        wifi_backup_resp_simple(trans, "wifiSettingNew", tid->valuestring, devid->valuestring, 1);
        return OPRT_CJSON_PARSE_ERR;
    }

    new_list = cJSON_GetObjectItem(plain, "newList");
    if (false == cJSON_IsArray(new_list)) {
        cJSON_Delete(plain);
        wifi_backup_resp_simple(trans, "wifiSettingNew", tid->valuestring, devid->valuestring, 1);
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
            wifi_backup_resp_simple(trans, "wifiSettingNew", tid->valuestring, devid->valuestring, 1);
            return OPRT_MSG_OUT_OF_LIMIT;
        }
    }

    wifi_backup_lock();
    __apply_backup_list(cJSON_GetObjectItem(plain, "backupList"));

    for (i = 0; i < size; i++) {
        cJSON *item = cJSON_GetArrayItem(new_list, i);
        cJSON *ssid = cJSON_GetObjectItem(item, "ssid");
        cJSON *pswd = cJSON_GetObjectItem(item, "passwd");

        /* the current network is not a backup of itself */
        if (wifi_backup_is_same_ap_as_current(ssid->valuestring, pswd->valuestring)) {
            continue;
        }
        wifi_backup_list_push(ssid->valuestring, pswd->valuestring, true);
    }
    wifi_backup_kv_save();
    wifi_backup_unlock();

    cJSON_Delete(plain);
    __resp_ssid_list(trans, "wifiSettingNew", tid->valuestring, devid->valuestring);

    return OPRT_OK;
}


static OPERATE_RET __on_wifi_switch_backup(cJSON *root, tuya_wifi_backup_trans_t trans)
{
    cJSON            *data  = NULL;
    cJSON            *tid   = NULL;
    cJSON            *devid = NULL;
    cJSON            *hash  = NULL;
    uint8_t           hash_value[WIFI_BACKUP_HASH_LEN];
    wifi_backup_ap_t *ap = NULL;
    wifi_backup_ap_t  target;
    bool              same_ap = false;
    OPERATE_RET       rt;

    if (false == __req_head(root, &data, &tid, &devid)) {
        return OPRT_CJSON_GET_ERR;
    }

    hash = cJSON_GetObjectItem(data, "hash");
    if (!cJSON_IsString(hash) ||
        WIFI_BACKUP_HASH_LEN != __b64_decode(hash->valuestring, hash_value, sizeof(hash_value))) {
        wifi_backup_resp_simple(trans, "wifiSwitchNew", tid->valuestring, devid->valuestring, 3);
        return OPRT_CJSON_GET_ERR;
    }

    wifi_backup_lock();
    ap = wifi_backup_list_find(hash_value);
    if (NULL == ap) {
        wifi_backup_unlock();
        PR_DEBUG("wifi switch: ap not found in the backup list");
        wifi_backup_resp_simple(trans, "wifiSwitchNew", tid->valuestring, devid->valuestring, 3);
        return OPRT_NOT_FOUND;
    }

    if (WIFI_BACKUP_STATUS_IDLE != g_wifi_backup_mgr.status) {
        wifi_backup_unlock();
        PR_DEBUG("wifi switch is already ongoing");
        wifi_backup_resp_simple(trans, "wifiSwitchNew", tid->valuestring, devid->valuestring, 3);
        return OPRT_COM_ERROR;
    }

    target = *ap;
    if (wifi_backup_is_same_ap_as_current(target.ssid, target.passwd)) {
        /* nothing to switch to, but the APP should stop showing it as a backup */
        wifi_backup_list_remove_at((uint8_t)(ap - g_wifi_backup_mgr.list));
        wifi_backup_kv_save();
        same_ap = true;
    }
    wifi_backup_unlock();

    if (same_ap) {
        wifi_backup_resp_simple(trans, "wifiSwitchNew", tid->valuestring, devid->valuestring, 1);
        return OPRT_OK;
    }

    rt = wifi_backup_switch_start(tid->valuestring, devid->valuestring, target.ssid, target.passwd, trans);
    if (OPRT_OK != rt) {
        wifi_backup_resp_simple(trans, "wifiSwitchNew", tid->valuestring, devid->valuestring, 3);
        return rt;
    }

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
        wifi_backup_resp_simple(trans, "wifiSwitchNew", tid->valuestring, devid->valuestring, 3);
        return rt;
    }

    plain = cJSON_Parse(plaintext);
    tal_free(plaintext);
    if (NULL == plain) {
        wifi_backup_resp_simple(trans, "wifiSwitchNew", tid->valuestring, devid->valuestring, 3);
        return OPRT_CJSON_PARSE_ERR;
    }

    ssid = cJSON_GetObjectItem(plain, "ssid");
    pswd = cJSON_GetObjectItem(plain, "passwd");
    if (!cJSON_IsString(ssid) || !cJSON_IsString(pswd) || strlen(ssid->valuestring) > WIFI_SSID_LEN ||
        strlen(pswd->valuestring) > WIFI_PASSWD_LEN || wifi_backup_is_same_ap_as_current(ssid->valuestring, pswd->valuestring)) {
        cJSON_Delete(plain);
        wifi_backup_resp_simple(trans, "wifiSwitchNew", tid->valuestring, devid->valuestring, 3);
        return OPRT_INVALID_PARM;
    }

    rt = wifi_backup_switch_start(tid->valuestring, devid->valuestring, ssid->valuestring, pswd->valuestring, trans);
    cJSON_Delete(plain);
    if (OPRT_OK != rt) {
        wifi_backup_resp_simple(trans, "wifiSwitchNew", tid->valuestring, devid->valuestring, 3);
        return rt;
    }

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

    g_wifi_backup_mgr.trans = trans;

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

    if (false == g_wifi_backup_mgr.inited) {
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


void wifi_backup_proto_register(void)
{
    uint32_t i;

    for (i = 0; i < WIFI_BACKUP_CMD_NUM; i++) {
        tuya_mqtt_dispatch_register(PRO_IOT_DA_REQ, s_cmd_table[i].req_type, "wifi backup", __mqtt_dispatch_cb, NULL);
    }
}

