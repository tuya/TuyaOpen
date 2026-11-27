/**
 * @file wifi_backup_fsm.c
 * @brief Backup AP list storage, KV persistence, switch/fallback FSMs, and lifecycle.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 */

#include "wifi_backup_internal.h"

#include "tal_event_info.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "cJSON.h"

#include "netmgr.h"
#include "netconn_wifi.h"
#include "tuya_iot.h"

/***********************************************************
***********************variable define**********************
***********************************************************/
wifi_backup_mgr_t g_wifi_backup_mgr;
int (*g_wifi_backup_conf_attr)(tuya_wfpt_attr_t *attr) = NULL;
static bool s_enable = true;

/***********************************************************
***********************function define**********************
***********************************************************/
static void        __fsm_task(void *data);
static void        __offline_tick(TIMER_ID timer_id, void *arg);
static void        __offline_scan_task(void *data);
static void        __switch_fsm_run(wifi_backup_switch_fsm_t *fsm);
static void        __connect_fsm_run(wifi_backup_connect_fsm_t *fsm);
static void        __state_notify(tuya_wfpt_state_t state);

void wifi_backup_lock(void)
{
    if (NULL != g_wifi_backup_mgr.lock) {
        tal_mutex_lock(g_wifi_backup_mgr.lock);
    }
}

void wifi_backup_unlock(void)
{
    if (NULL != g_wifi_backup_mgr.lock) {
        tal_mutex_unlock(g_wifi_backup_mgr.lock);
    }
}

void wifi_backup_str_copy(char *dst, uint32_t dst_size, const char *src)
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
bool wifi_backup_cur_ap_get(char *ssid, uint32_t ssid_size, char *passwd, uint32_t passwd_size)
{
    netconn_wifi_info_t info;

    memset(&info, 0, sizeof(info));
    if (OPRT_OK != netmgr_conn_get(NETCONN_WIFI, NETCONN_CMD_SSID_PSWD, &info)) {
        return false;
    }
    if ('\0' == info.ssid[0]) {
        return false;
    }

    wifi_backup_str_copy(ssid, ssid_size, info.ssid);
    wifi_backup_str_copy(passwd, passwd_size, info.pswd);

    return true;
}

/**
 * @brief Remember the AP to roll back to, the target is only persisted on success.
 */
static void __orig_capture(void)
{
    if (false == wifi_backup_cur_ap_get(g_wifi_backup_mgr.orig_ssid, sizeof(g_wifi_backup_mgr.orig_ssid), g_wifi_backup_mgr.orig_passwd, sizeof(g_wifi_backup_mgr.orig_passwd))) {
        g_wifi_backup_mgr.orig_ssid[0]   = '\0';
        g_wifi_backup_mgr.orig_passwd[0] = '\0';
    }
}

bool wifi_backup_is_same_ap_as_current(const char *ssid, const char *passwd)
{
    char cur_ssid[WIFI_SSID_LEN + 1];
    char cur_passwd[WIFI_PASSWD_LEN + 1];

    if (false == wifi_backup_cur_ap_get(cur_ssid, sizeof(cur_ssid), cur_passwd, sizeof(cur_passwd))) {
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
void wifi_backup_ap_hash(wifi_backup_ap_t *ap)
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


/***********************************************************
*********************backup list ops************************
***********************************************************/
wifi_backup_ap_t *wifi_backup_list_find(const uint8_t *hash)
{
    uint8_t i;

    for (i = 0; i < g_wifi_backup_mgr.count; i++) {
        if (0 == memcmp(g_wifi_backup_mgr.list[i].hash, hash, WIFI_BACKUP_HASH_LEN)) {
            return &g_wifi_backup_mgr.list[i];
        }
    }

    return NULL;
}

void wifi_backup_list_remove_at(uint8_t index)
{
    if (index >= g_wifi_backup_mgr.count) {
        return;
    }

    if (index + 1 < g_wifi_backup_mgr.count) {
        memmove(&g_wifi_backup_mgr.list[index], &g_wifi_backup_mgr.list[index + 1], (g_wifi_backup_mgr.count - index - 1) * sizeof(wifi_backup_ap_t));
    }
    g_wifi_backup_mgr.count--;
    memset(&g_wifi_backup_mgr.list[g_wifi_backup_mgr.count], 0, sizeof(wifi_backup_ap_t));
}

/**
 * @brief Add an AP, @p front places it at the head so it is the last to be evicted.
 */
void wifi_backup_list_push(const char *ssid, const char *passwd, bool front)
{
    wifi_backup_ap_t ap;

    if (NULL == ssid || '\0' == ssid[0]) {
        return;
    }

    memset(&ap, 0, sizeof(ap));
    wifi_backup_str_copy(ap.ssid, sizeof(ap.ssid), ssid);
    wifi_backup_str_copy(ap.passwd, sizeof(ap.passwd), passwd);
    wifi_backup_ap_hash(&ap);

    if (NULL != wifi_backup_list_find(ap.hash)) {
        PR_DEBUG("ap %s is already in the backup list", ap.ssid);
        return;
    }

    if (g_wifi_backup_mgr.count >= TUYA_WIFI_BACKUP_AP_NUM) {
        wifi_backup_list_remove_at(TUYA_WIFI_BACKUP_AP_NUM - 1); //!< drop the oldest
    }

    if (front) {
        memmove(&g_wifi_backup_mgr.list[1], &g_wifi_backup_mgr.list[0], g_wifi_backup_mgr.count * sizeof(wifi_backup_ap_t));
        g_wifi_backup_mgr.list[0] = ap;
    } else {
        g_wifi_backup_mgr.list[g_wifi_backup_mgr.count] = ap;
    }
    g_wifi_backup_mgr.count++;
}

/**
 * @brief Keep only the APs whose hash is in @p hashes, preserving the order.
 */
void wifi_backup_list_keep(const uint8_t (*hashes)[WIFI_BACKUP_HASH_LEN], uint8_t num)
{
    uint8_t i;
    uint8_t j;
    uint8_t kept = 0;

    for (i = 0; i < g_wifi_backup_mgr.count; i++) {
        bool match = false;

        for (j = 0; j < num; j++) {
            if (0 == memcmp(g_wifi_backup_mgr.list[i].hash, hashes[j], WIFI_BACKUP_HASH_LEN)) {
                match = true;
                break;
            }
        }

        if (match) {
            if (kept != i) {
                g_wifi_backup_mgr.list[kept] = g_wifi_backup_mgr.list[i];
            }
            kept++;
        }
    }

    memset(&g_wifi_backup_mgr.list[kept], 0, (g_wifi_backup_mgr.count - kept) * sizeof(wifi_backup_ap_t));
    g_wifi_backup_mgr.count = kept;
}

void wifi_backup_list_clear(void)
{
    memset(g_wifi_backup_mgr.list, 0, sizeof(g_wifi_backup_mgr.list));
    g_wifi_backup_mgr.count = 0;
}

/***********************************************************
**********************flash persistence*********************
***********************************************************/
OPERATE_RET wifi_backup_kv_save(void)
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

    for (i = 0; i < g_wifi_backup_mgr.count; i++) {
        cJSON *item = cJSON_CreateObject();

        if (NULL == item) {
            cJSON_Delete(root);
            return OPRT_MALLOC_FAILED;
        }
        cJSON_AddStringToObject(item, "ssid", g_wifi_backup_mgr.list[i].ssid);
        cJSON_AddStringToObject(item, "passwd", g_wifi_backup_mgr.list[i].passwd);
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
    for (i = 0; i < size && g_wifi_backup_mgr.count < TUYA_WIFI_BACKUP_AP_NUM; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        cJSON *ssid = cJSON_GetObjectItem(item, "ssid");
        cJSON *pswd = cJSON_GetObjectItem(item, "passwd");

        if (cJSON_IsString(ssid) && cJSON_IsString(pswd)) {
            wifi_backup_list_push(ssid->valuestring, pswd->valuestring, false);
        }
    }

    cJSON_Delete(root);

    PR_DEBUG("wifi backup list restored, %d ap", g_wifi_backup_mgr.count);
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
    case TUYA_WFPT_SWICTH_SUCCESS:
    case TUYA_WFPT_SWICTH_FALIED:
        netconn_wifi_station_suspend(false);
        break;
    default:
        break;
    }

    if (NULL != g_wifi_backup_mgr.state_cb) {
        g_wifi_backup_mgr.state_cb(state);
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
    char orig_ssid[WIFI_SSID_LEN + 1];
    char orig_passwd[WIFI_PASSWD_LEN + 1];

    wifi_backup_lock();
    wifi_backup_str_copy(orig_ssid, sizeof(orig_ssid), g_wifi_backup_mgr.orig_ssid);
    wifi_backup_str_copy(orig_passwd, sizeof(orig_passwd), g_wifi_backup_mgr.orig_passwd);
    wifi_backup_unlock();

    if ('\0' == orig_ssid[0]) {
        return;
    }

    PR_DEBUG("wifi backup gave up, back to %s", orig_ssid);
    netconn_wifi_station_set(orig_ssid, orig_passwd, false);
}

/**
 * @brief Commit a successful switch: @p ap becomes the current network and the
 *        previous one takes its place in the backup list.
 */
static void __switch_commit(const wifi_backup_ap_t *ap)
{
    wifi_backup_ap_t  target = *ap;
    wifi_backup_ap_t *found  = wifi_backup_list_find(target.hash);

    if (NULL != found) {
        wifi_backup_list_remove_at((uint8_t)(found - g_wifi_backup_mgr.list));
    }
    wifi_backup_list_push(g_wifi_backup_mgr.orig_ssid, g_wifi_backup_mgr.orig_passwd, true);
    wifi_backup_kv_save();

    /* persist the new network so the next boot connects to it directly */
    netconn_wifi_station_info_save(target.ssid, target.passwd);
}

static void __switch_fsm_run(wifi_backup_switch_fsm_t *fsm)
{
    uint32_t now = (uint32_t)tal_system_get_millisecond();

    wifi_backup_lock();
    if (WIFI_BACKUP_STATUS_SWITCH != g_wifi_backup_mgr.status) {
        wifi_backup_unlock();
        return;
    }

    switch (fsm->state) {
    case WIFI_BACKUP_FSM_STATE_INIT: {
        bool recover = (fsm->recover != 0);
        char ssid[WIFI_SSID_LEN + 1];
        char passwd[WIFI_PASSWD_LEN + 1];

        if (recover) {
            wifi_backup_str_copy(ssid, sizeof(ssid), g_wifi_backup_mgr.orig_ssid);
            wifi_backup_str_copy(passwd, sizeof(passwd), g_wifi_backup_mgr.orig_passwd);
        } else {
            wifi_backup_str_copy(ssid, sizeof(ssid), fsm->ap.ssid);
            wifi_backup_str_copy(passwd, sizeof(passwd), fsm->ap.passwd);
        }
        fsm->timeout = now + WIFI_BACKUP_SWITCH_TIMEOUT_MS;
        fsm->state   = WIFI_BACKUP_FSM_STATE_CHECK;
        wifi_backup_unlock();

        if (recover) {
            PR_DEBUG("wifi backup switch rollback to %s", ssid);
            __state_notify(TUYA_WFPT_SWICTH_ORIGINAL);
            if ('\0' != ssid[0]) {
                netconn_wifi_station_set(ssid, passwd, false);
            }
        } else {
            tuya_iot_client_t *client = tuya_iot_client_get();

            PR_DEBUG("wifi backup switch to %s", ssid);
            if (NULL != client) {
                tuya_iot_reconnect(client);
            }
            netconn_wifi_station_set(ssid, passwd, false);
        }
        break;
    }

    case WIFI_BACKUP_FSM_STATE_CHECK: {
        if ((int32_t)(now - fsm->timeout) > 0) {
            PR_DEBUG("wifi backup switch timeout, recover %d", fsm->recover);
            if (fsm->recover) {
                fsm->state = WIFI_BACKUP_FSM_STATE_STOP;
            } else {
                fsm->recover = true;
                fsm->state   = WIFI_BACKUP_FSM_STATE_INIT;
            }
            wifi_backup_unlock();
            break;
        }

        if (false == __link_up() || false == __cloud_connected()) {
            wifi_backup_unlock();
            break;
        }

        tuya_wifi_backup_trans_t trans = fsm->trans;
        char                     tid[WIFI_BACKUP_TID_LEN + 1];
        char                     devid[WIFI_BACKUP_DEVID_LEN + 1];
        int                      res_code = fsm->recover ? 2 : 1;

        wifi_backup_str_copy(tid, sizeof(tid), fsm->tid);
        wifi_backup_str_copy(devid, sizeof(devid), fsm->devid);
        if (false == fsm->recover) {
            __switch_commit(&fsm->ap);
        }
        g_wifi_backup_mgr.status = WIFI_BACKUP_STATUS_IDLE;
        wifi_backup_unlock();

        wifi_backup_resp_simple(trans, "wifiSwitchNew", tid, devid, res_code);
        tal_workq_stop_delayed(g_wifi_backup_mgr.fsm_work);
        __state_notify(TUYA_WFPT_SWICTH_SUCCESS);
        break;
    }

    case WIFI_BACKUP_FSM_STATE_STOP: {
        tuya_wifi_backup_trans_t trans = fsm->trans;
        char                     tid[WIFI_BACKUP_TID_LEN + 1];
        char                     devid[WIFI_BACKUP_DEVID_LEN + 1];

        wifi_backup_str_copy(tid, sizeof(tid), fsm->tid);
        wifi_backup_str_copy(devid, sizeof(devid), fsm->devid);
        g_wifi_backup_mgr.status = WIFI_BACKUP_STATUS_IDLE;
        wifi_backup_unlock();

        if (TUYA_WIFI_BACKUP_TRANS_BLE == trans) {
            wifi_backup_resp_simple(trans, "wifiSwitchNew", tid, devid, 3);
        }
        tal_workq_stop_delayed(g_wifi_backup_mgr.fsm_work);
        __state_notify(TUYA_WFPT_SWICTH_FALIED);
        __switch_give_up();
        break;
    }

    default:
        wifi_backup_unlock();
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
    wifi_backup_connect_fsm_t *connect = &g_wifi_backup_mgr.connect_fsm;
    char                       cur_ssid[WIFI_SSID_LEN + 1];
    char                       cur_passwd[WIFI_PASSWD_LEN + 1];
    bool                       has_cur;
    uint8_t                    i;
    uint8_t                    j;
    uint8_t                    k;

    memset(connect, 0, sizeof(*connect));
    has_cur = wifi_backup_cur_ap_get(cur_ssid, sizeof(cur_ssid), cur_passwd, sizeof(cur_passwd));

    for (i = 0; i < g_wifi_backup_mgr.count; i++) {
        for (j = 0; j < ap_num; j++) {
            if (0 != strcmp((char *)ap_list[j].ssid, g_wifi_backup_mgr.list[i].ssid)) {
                continue;
            }
            connect->ap[connect->count]      = g_wifi_backup_mgr.list[i];
            connect->ap[connect->count].flag = WIFI_BACKUP_AP_EXIST_FLAG;
            connect->ap[connect->count].rssi = ap_list[j].rssi;
            if (has_cur && 0 == strcmp(connect->ap[connect->count].ssid, cur_ssid)) {
                connect->ap[connect->count].flag |= WIFI_BACKUP_AP_PRIOR_FLAG;
            }
            connect->count++;
            break;
        }
    }

    /* selection sort: the current AP first, the strongest RSSI afterwards */
    for (i = 0; i < connect->count; i++) {
        wifi_backup_ap_t tmp;

        k = i;
        if (connect->ap[i].flag & WIFI_BACKUP_AP_PRIOR_FLAG) {
            continue;
        }
        for (j = i + 1; j < connect->count; j++) {
            if (connect->ap[j].flag & WIFI_BACKUP_AP_PRIOR_FLAG) {
                k = j;
                break;
            }
        }
        for (j = i + 1; k == i && j < connect->count; j++) {
            if (connect->ap[j].rssi > connect->ap[k].rssi) {
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
        PR_DEBUG("wifi backup candidate %s, rssi %d", connect->ap[i].ssid, (int)connect->ap[i].rssi);
    }

    return (connect->count > 0) ? OPRT_OK : OPRT_NOT_FOUND;
}

static void __connect_fsm_run(wifi_backup_connect_fsm_t *fsm)
{
    uint32_t now = (uint32_t)tal_system_get_millisecond();

    wifi_backup_lock();
    if (WIFI_BACKUP_STATUS_CONNECT != g_wifi_backup_mgr.status) {
        wifi_backup_unlock();
        return;
    }

    switch (fsm->state) {
    case WIFI_BACKUP_FSM_STATE_INIT: {
        wifi_backup_ap_t ap = fsm->ap[fsm->index];

        fsm->timeout = now + WIFI_BACKUP_CONNECT_TIMEOUT_MS;
        fsm->state   = WIFI_BACKUP_FSM_STATE_CHECK;
        wifi_backup_unlock();

        PR_DEBUG("wifi backup connect %s", ap.ssid);
        netconn_wifi_station_set(ap.ssid, ap.passwd, false);
        break;
    }

    case WIFI_BACKUP_FSM_STATE_CHECK: {
        if ((int32_t)(now - fsm->timeout) > 0) {
            PR_DEBUG("wifi backup connect timeout for %s", fsm->ap[fsm->index].ssid);
            fsm->state = WIFI_BACKUP_FSM_STATE_STOP;
            wifi_backup_unlock();
            break;
        }

        if (false == __link_up()) {
            wifi_backup_unlock();
            break;
        }

        __switch_commit(&fsm->ap[fsm->index]);
        g_wifi_backup_mgr.status = WIFI_BACKUP_STATUS_IDLE;
        fsm->state   = WIFI_BACKUP_FSM_STATE_DONE;
        wifi_backup_unlock();

        tal_workq_stop_delayed(g_wifi_backup_mgr.fsm_work);
        __state_notify(TUYA_WFPT_SWICTH_SUCCESS);
        break;
    }

    case WIFI_BACKUP_FSM_STATE_STOP: {
        bool exhausted = false;

        fsm->state = WIFI_BACKUP_FSM_STATE_INIT;
        if (++fsm->index >= fsm->count) {
            g_wifi_backup_mgr.status = WIFI_BACKUP_STATUS_IDLE;
            memset(fsm, 0, sizeof(*fsm));
            exhausted = true;
        }
        wifi_backup_unlock();

        if (exhausted) {
            tal_workq_stop_delayed(g_wifi_backup_mgr.fsm_work);
            __state_notify(TUYA_WFPT_SWICTH_FALIED);
            __switch_give_up();
        }
        break;
    }

    default:
        wifi_backup_unlock();
        break;
    }
}

static void __fsm_task(void *data)
{
    uint8_t status;

    (void)data;

    wifi_backup_lock();
    status = g_wifi_backup_mgr.status;
    wifi_backup_unlock();

    if (WIFI_BACKUP_STATUS_SWITCH == status) {
        __switch_fsm_run(&g_wifi_backup_mgr.switch_fsm);
    } else if (WIFI_BACKUP_STATUS_CONNECT == status) {
        __connect_fsm_run(&g_wifi_backup_mgr.connect_fsm);
    } else {
        PR_DEBUG("wifi backup invalid status: %d", status);
        tal_workq_stop_delayed(g_wifi_backup_mgr.fsm_work);
    }
}

/***********************************************************
*******************fallback reconnect***********************
***********************************************************/
static void __offline_scan_task(void *data)
{
    char    ssids[TUYA_WIFI_BACKUP_AP_NUM][WIFI_SSID_LEN + 1];
    uint8_t ssid_cnt = 0;
    AP_IF_S found[TUYA_WIFI_BACKUP_AP_NUM];
    uint8_t found_cnt       = 0;
    bool    use_all_scan    = false;
    uint8_t i;

    (void)data;

    wifi_backup_lock();
    for (i = 0; i < g_wifi_backup_mgr.count && ssid_cnt < TUYA_WIFI_BACKUP_AP_NUM; i++) {
        if ('\0' != g_wifi_backup_mgr.list[i].ssid[0]) {
            wifi_backup_str_copy(ssids[ssid_cnt], sizeof(ssids[ssid_cnt]), g_wifi_backup_mgr.list[i].ssid);
            ssid_cnt++;
        }
    }
    wifi_backup_unlock();

    if (0 == ssid_cnt) {
        return;
    }

    /* Directed scan per backup SSID (matches TuyaOS): finds hidden SSIDs and
     * avoids allocating a large array for unrelated nearby APs. */
    memset(found, 0, sizeof(found));
    for (i = 0; i < ssid_cnt; i++) {
        AP_IF_S    *ap = NULL;
        OPERATE_RET rt = tal_wifi_assign_ap_scan((int8_t *)ssids[i], &ap);

        if (OPRT_NOT_SUPPORTED == rt) {
            if (NULL != ap) {
                tal_wifi_release_ap(ap);
            }
            use_all_scan = true;
            break;
        }
        if (OPRT_OK == rt && NULL != ap) {
            if ('\0' != ap->ssid[0] && found_cnt < TUYA_WIFI_BACKUP_AP_NUM) {
                memcpy(&found[found_cnt++], ap, sizeof(AP_IF_S));
            }
            tal_wifi_release_ap(ap);
        }
    }

    if (use_all_scan) {
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
        return;
    }

    if (0 == found_cnt) {
        PR_DEBUG("wifi backup directed scan found nothing");
        return;
    }

    if (OPRT_OK == tuya_wifi_backup_ap_check(found, found_cnt)) {
        PR_NOTICE("wifi backup reconnect started");
    }
}

static void __offline_tick(TIMER_ID timer_id, void *arg)
{
    bool    trigger = false;
    uint8_t count   = 0;

    g_wifi_backup_mgr.local_num = __link_up() ? 0 : g_wifi_backup_mgr.local_num + 1;
    g_wifi_backup_mgr.cloud_num = __cloud_connected() ? 0 : g_wifi_backup_mgr.cloud_num + 1;

    if (0 == g_wifi_backup_mgr.local_num && 0 == g_wifi_backup_mgr.cloud_num) {
        tal_sw_timer_stop(g_wifi_backup_mgr.offline_timer);
        return;
    }
    if (WIFI_BACKUP_STATUS_IDLE != g_wifi_backup_mgr.status) {
        return;
    }

    /* the ap currently configured is not a fallback, it is the one that just
     * failed, so it is left out of the count */
    count = tuya_wifi_backup_count_get();
    if (0 == count) {
        if (g_wifi_backup_mgr.local_num && 0 == g_wifi_backup_mgr.local_num % WIFI_BACKUP_LOCAL_RECONNECT_TIME) {
            PR_DEBUG("offline for %us, but %d ap in the list is the current one", (unsigned int)g_wifi_backup_mgr.local_num,
                     g_wifi_backup_mgr.count);
        }
        return;
    }

    if (g_wifi_backup_mgr.local_num && 0 == g_wifi_backup_mgr.local_num % WIFI_BACKUP_LOCAL_RECONNECT_TIME) {
        trigger = true;
    }
    if (g_wifi_backup_mgr.cloud_num && 0 == g_wifi_backup_mgr.cloud_num % WIFI_BACKUP_CLOUD_RECONNECT_TIME) {
        trigger = true;
    }
    if (false == trigger) {
        return;
    }

    PR_NOTICE("offline for %us (cloud %us), try %d backup network(s)", (unsigned int)g_wifi_backup_mgr.local_num,
              (unsigned int)g_wifi_backup_mgr.cloud_num, count);
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

    wifi_backup_lock();
    if (WIFI_BACKUP_STATUS_IDLE != g_wifi_backup_mgr.status) {
        wifi_backup_unlock();
        return OPRT_COM_ERROR;
    }

    rt = __connect_select(ap_list, ap_num);
    if (OPRT_OK != rt) {
        wifi_backup_unlock();
        PR_DEBUG("no backup ap is in range");
        return rt;
    }

    __orig_capture();
    g_wifi_backup_mgr.status = WIFI_BACKUP_STATUS_CONNECT;
    wifi_backup_unlock();

    rt = tal_workq_start_delayed(g_wifi_backup_mgr.fsm_work, WIFI_BACKUP_CONNECT_INTERVAL_MS, LOOP_CYCLE);
    if (OPRT_OK != rt) {
        wifi_backup_lock();
        g_wifi_backup_mgr.status = WIFI_BACKUP_STATUS_IDLE;
        wifi_backup_unlock();
        return rt;
    }

    __state_notify(TUYA_WFPT_SWICTH_START);

    return OPRT_OK;
}

OPERATE_RET tuya_wifi_backup_is_switching(void)
{
    uint8_t status;

    wifi_backup_lock();
    status = g_wifi_backup_mgr.status;
    wifi_backup_unlock();

    return (WIFI_BACKUP_STATUS_IDLE == status) ? OPRT_COM_ERROR : OPRT_OK;
}

uint8_t tuya_wifi_backup_count_get(void)
{
    uint8_t i;
    uint8_t count = 0;

    wifi_backup_lock();
    for (i = 0; i < g_wifi_backup_mgr.count; i++) {
        if (false == wifi_backup_is_same_ap_as_current(g_wifi_backup_mgr.list[i].ssid, g_wifi_backup_mgr.list[i].passwd)) {
            count++;
        }
    }
    wifi_backup_unlock();

    return count;
}

OPERATE_RET tuya_wifi_backup_list_get(void (*cb)(const char *ssid, const char *passwd))
{
    char             ssid[WIFI_SSID_LEN + 1];
    char             passwd[WIFI_PASSWD_LEN + 1];
    wifi_backup_ap_t snapshot[TUYA_WIFI_BACKUP_AP_NUM];
    uint8_t          count = 0;
    uint8_t          i;

    if (NULL == cb) {
        return OPRT_INVALID_PARM;
    }

    if (wifi_backup_cur_ap_get(ssid, sizeof(ssid), passwd, sizeof(passwd))) {
        cb(ssid, passwd);
    }

    wifi_backup_lock();
    count = g_wifi_backup_mgr.count;
    if (count > TUYA_WIFI_BACKUP_AP_NUM) {
        count = TUYA_WIFI_BACKUP_AP_NUM;
    }
    memcpy(snapshot, g_wifi_backup_mgr.list, count * sizeof(wifi_backup_ap_t));
    wifi_backup_unlock();

    for (i = 0; i < count; i++) {
        if (wifi_backup_is_same_ap_as_current(snapshot[i].ssid, snapshot[i].passwd)) {
            continue;
        }
        cb(snapshot[i].ssid, snapshot[i].passwd);
    }

    return OPRT_OK;
}

OPERATE_RET tuya_wifi_backup_current_ap_add(void)
{
    char        ssid[WIFI_SSID_LEN + 1];
    char        passwd[WIFI_PASSWD_LEN + 1];
    OPERATE_RET rt;

    if (false == wifi_backup_cur_ap_get(ssid, sizeof(ssid), passwd, sizeof(passwd))) {
        PR_DEBUG("no current wifi to add");
        return OPRT_INVALID_PARM;
    }

    wifi_backup_lock();
    wifi_backup_list_push(ssid, passwd, true);
    rt = wifi_backup_kv_save();
    wifi_backup_unlock();

    return rt;
}

OPERATE_RET tuya_wifi_backup_current_ap_in_list(void)
{
    uint8_t i;

    wifi_backup_lock();
    for (i = 0; i < g_wifi_backup_mgr.count; i++) {
        if (wifi_backup_is_same_ap_as_current(g_wifi_backup_mgr.list[i].ssid, g_wifi_backup_mgr.list[i].passwd)) {
            wifi_backup_unlock();
            return OPRT_OK;
        }
    }
    wifi_backup_unlock();

    return OPRT_NOT_FOUND;
}

OPERATE_RET tuya_wifi_backup_test_add(const char *ssid, const char *passwd)
{
    OPERATE_RET rt;

    if (NULL == ssid || '\0' == ssid[0]) {
        return OPRT_INVALID_PARM;
    }

    wifi_backup_lock();
    wifi_backup_list_push(ssid, (NULL == passwd) ? "" : passwd, true);
    rt = wifi_backup_kv_save();
    wifi_backup_unlock();

    return rt;
}

OPERATE_RET tuya_wifi_backup_clear(void)
{
    OPERATE_RET rt;

    wifi_backup_lock();
    wifi_backup_list_clear();
    rt = tal_kv_del(WIFI_BACKUP_KV_KEY);
    wifi_backup_unlock();

    return rt;
}

OPERATE_RET tuya_wifi_backup_switch(const char *ssid, const char *passwd)
{
    if (NULL == ssid || '\0' == ssid[0]) {
        return OPRT_INVALID_PARM;
    }

    return wifi_backup_switch_start("00000000", "", ssid, passwd, TUYA_WIFI_BACKUP_TRANS_CLOUD);
}

OPERATE_RET tuya_wifi_backup_conf_attr(int (*conf_attr)(tuya_wfpt_attr_t *attr))
{
    g_wifi_backup_conf_attr = conf_attr;

    return OPRT_OK;
}

void tuya_wifi_backup_state_cb_set(tuya_wfpt_state_cb_t *cb)
{
    g_wifi_backup_mgr.state_cb = cb;
}

void tuya_wifi_backup_disable(void)
{
    s_enable = false;
}

bool tuya_wifi_backup_is_enable(void)
{
    return s_enable;
}


OPERATE_RET wifi_backup_switch_start(const char *tid, const char *devid, const char *ssid, const char *passwd,
                                  tuya_wifi_backup_trans_t trans)
{
    wifi_backup_switch_fsm_t *fsm = &g_wifi_backup_mgr.switch_fsm;
    OPERATE_RET               rt;

    wifi_backup_lock();
    if (WIFI_BACKUP_STATUS_IDLE != g_wifi_backup_mgr.status) {
        wifi_backup_unlock();
        PR_DEBUG("wifi backup is busy");
        return OPRT_COM_ERROR;
    }

    memset(fsm, 0, sizeof(*fsm));
    fsm->trans = trans;
    wifi_backup_str_copy(fsm->tid, sizeof(fsm->tid), tid);
    wifi_backup_str_copy(fsm->devid, sizeof(fsm->devid), devid);
    wifi_backup_str_copy(fsm->ap.ssid, sizeof(fsm->ap.ssid), ssid);
    wifi_backup_str_copy(fsm->ap.passwd, sizeof(fsm->ap.passwd), passwd);
    wifi_backup_ap_hash(&fsm->ap);

    __orig_capture();
    g_wifi_backup_mgr.status = WIFI_BACKUP_STATUS_SWITCH;
    wifi_backup_unlock();

    rt = tal_workq_start_delayed(g_wifi_backup_mgr.fsm_work, WIFI_BACKUP_SWITCH_INTERVAL_MS, LOOP_CYCLE);
    if (OPRT_OK != rt) {
        wifi_backup_lock();
        g_wifi_backup_mgr.status = WIFI_BACKUP_STATUS_IDLE;
        wifi_backup_unlock();
        return rt;
    }

    PR_DEBUG("wifi backup switch to %s", ssid);
    __state_notify(TUYA_WFPT_SWICTH_START);
    __switch_fsm_run(fsm);

    return OPRT_OK;
}


/***********************************************************
***********************life cycle***************************
***********************************************************/
static OPERATE_RET __reset_event_cb(void *data)
{
    bool was_active = false;

    (void)data;
    PR_DEBUG("wifi backup list cleared by reset");

    /* the fsm would push the old network back once the list is gone; stop it first */
    wifi_backup_lock();
    if (WIFI_BACKUP_STATUS_IDLE != g_wifi_backup_mgr.status) {
        g_wifi_backup_mgr.status = WIFI_BACKUP_STATUS_IDLE;
        was_active   = true;
    }
    wifi_backup_list_clear();
    g_wifi_backup_mgr.orig_ssid[0]   = '\0';
    g_wifi_backup_mgr.orig_passwd[0] = '\0';
    memset(g_wifi_backup_mgr.ecdh_secret, 0, sizeof(g_wifi_backup_mgr.ecdh_secret));
    g_wifi_backup_mgr.ecdh_ready = false;
    g_wifi_backup_mgr.local_num  = 0;
    g_wifi_backup_mgr.cloud_num  = 0;
    wifi_backup_unlock();

    if (was_active) {
        tal_workq_stop_delayed(g_wifi_backup_mgr.fsm_work);
        netconn_wifi_station_suspend(false);
    }
    tal_sw_timer_stop(g_wifi_backup_mgr.offline_timer);

    return tal_kv_del(WIFI_BACKUP_KV_KEY);
}

static OPERATE_RET __link_status_cb(void *data)
{
    netmgr_status_e status = *(netmgr_status_e *)data;

    if ((NETMGR_LINK_UP == status || NETMGR_LINK_UP_SWITH == status) && __cloud_connected()) {
        tal_sw_timer_stop(g_wifi_backup_mgr.offline_timer);
        return OPRT_OK;
    }

    g_wifi_backup_mgr.local_num = 0;
    tal_sw_timer_start(g_wifi_backup_mgr.offline_timer, WIFI_BACKUP_OFFLINE_TICK_MS, TAL_TIMER_CYCLE);

    return OPRT_OK;
}

static OPERATE_RET __mqtt_event_cb(void *data)
{
    if (__cloud_connected()) {
        g_wifi_backup_mgr.cloud_num = 0;
        return OPRT_OK;
    }

    g_wifi_backup_mgr.cloud_num = 0;
    tal_sw_timer_start(g_wifi_backup_mgr.offline_timer, WIFI_BACKUP_OFFLINE_TICK_MS, TAL_TIMER_CYCLE);

    return OPRT_OK;
}


OPERATE_RET tuya_wifi_backup_init(void)
{
    OPERATE_RET rt;

    if (g_wifi_backup_mgr.inited) {
        return OPRT_OK;
    }
    if (false == s_enable) {
        PR_DEBUG("wifi backup is disabled");
        return OPRT_OK;
    }

    memset(&g_wifi_backup_mgr, 0, sizeof(g_wifi_backup_mgr));
    rt = tal_mutex_create_init(&g_wifi_backup_mgr.lock);
    if (OPRT_OK != rt) {
        return rt;
    }
    g_wifi_backup_mgr.inited = true;

    __kv_load();

    rt = tal_workq_init_delayed(WORKQ_HIGHTPRI, __fsm_task, &g_wifi_backup_mgr, &g_wifi_backup_mgr.fsm_work);
    if (OPRT_OK != rt) {
        tal_mutex_release(g_wifi_backup_mgr.lock);
        g_wifi_backup_mgr.lock   = NULL;
        g_wifi_backup_mgr.inited = false;
        return rt;
    }
    rt = tal_sw_timer_create(__offline_tick, NULL, &g_wifi_backup_mgr.offline_timer);
    if (OPRT_OK != rt) {
        tal_workq_cancel_delayed(g_wifi_backup_mgr.fsm_work);
        g_wifi_backup_mgr.fsm_work = NULL;
        tal_mutex_release(g_wifi_backup_mgr.lock);
        g_wifi_backup_mgr.lock   = NULL;
        g_wifi_backup_mgr.inited = false;
        return rt;
    }

    tal_event_subscribe(EVENT_RESET, "wifi_bak", __reset_event_cb, SUBSCRIBE_TYPE_NORMAL);
    tal_event_subscribe(EVENT_LINK_STATUS_CHG, "wifi_bak", __link_status_cb, SUBSCRIBE_TYPE_NORMAL);
    tal_event_subscribe(EVENT_MQTT_DISCONNECTED, "wifi_bak", __mqtt_event_cb, SUBSCRIBE_TYPE_NORMAL);
    tal_event_subscribe(EVENT_MQTT_CONNECTED, "wifi_bak", __mqtt_event_cb, SUBSCRIBE_TYPE_NORMAL);

    wifi_backup_proto_register();

    /* A device that boots without a reachable network never sees a link status
     * change nor an mqtt disconnect, so neither callback below would ever arm
     * the offline tick. Arm it here as well, otherwise the backup list is never
     * tried on a device that comes up offline. */
    if (false == __link_up() || false == __cloud_connected()) {
        tal_sw_timer_start(g_wifi_backup_mgr.offline_timer, WIFI_BACKUP_OFFLINE_TICK_MS, TAL_TIMER_CYCLE);
    }

    PR_DEBUG("wifi backup initialized, %d ap", g_wifi_backup_mgr.count);

    return OPRT_OK;
}
